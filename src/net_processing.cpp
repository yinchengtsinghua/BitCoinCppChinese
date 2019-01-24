
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <net_processing.h>

#include <addrman.h>
#include <arith_uint256.h>
#include <blockencodings.h>
#include <chainparams.h>
#include <consensus/validation.h>
#include <hash.h>
#include <validation.h>
#include <merkleblock.h>
#include <netmessagemaker.h>
#include <netbase.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <random.h>
#include <reverse_iterator.h>
#include <scheduler.h>
#include <tinyformat.h>
#include <txmempool.h>
#include <ui_interface.h>
#include <util/system.h>
#include <util/moneystr.h>
#include <util/strencodings.h>

#include <memory>

#if defined(NDEBUG)
# error "Bitcoin cannot be compiled without assertions."
#endif

/*孤立事务的过期时间（秒）*/
static constexpr int64_t ORPHAN_TX_EXPIRE_TIME = 20 * 60;
/*孤立事务过期时间检查之间的最短时间（秒）*/
static constexpr int64_t ORPHAN_TX_EXPIRE_INTERVAL = 5 * 60;
/*邮件头下载超时（以微秒为单位）
 *超时=基+每个头*（预期的头数）*/

static constexpr int64_t HEADERS_DOWNLOAD_TIMEOUT_BASE = 15 * 60 * 1000000; //15分钟
static constexpr int64_t HEADERS_DOWNLOAD_TIMEOUT_PER_HEADER = 1000; //1MS/Head
/*至少要保护这么多出站对等机，使其免受由于/
 *在收割台链条后面。
 **/

static constexpr int32_t MAX_OUTBOUND_PEERS_TO_PROTECT_FROM_DISCONNECT = 4;
/*出站对等机同步到链工作的超时（未保护），以秒为单位*/
static constexpr int64_t CHAIN_SYNC_TIMEOUT = 20 * 60; //20分钟
/*检查过期提示的频率（秒）*/
static constexpr int64_t STALE_CHECK_INTERVAL = 10 * 60; //10分钟
/*检查额外出站对等机和断开连接的频率（秒）*/
static constexpr int64_t EXTRA_PEER_CHECK_INTERVAL = 45;
/*出站对等机逐出候选必须连接的最短时间（秒）*/
static constexpr int64_t MINIMUM_CONNECT_TIME = 30;
/*sha256（“主地址中继”）[0:8]*/
static constexpr uint64_t RANDOMIZER_ID_ADDRESS_RELAY = 0x3cac0035b5866b90ULL;
///age，在此时间之后，如果被请求作为
///防止指纹。设置为一个月，以秒为单位。
static constexpr int STALE_RELAY_AGE_LIMIT = 30 * 24 * 60 * 60;
///age，在此之后，出于费率的目的，块被视为历史块。
///限位块继电器。设置为一周，以秒为单位。
static constexpr int HISTORICAL_BLOCK_AGE = 7 * 24 * 60 * 60;

struct COrphanTx {
//修改时，请在测试/DOS测试中调整此定义的副本。
    CTransactionRef tx;
    NodeId fromPeer;
    int64_t nTimeExpire;
};
CCriticalSection g_cs_orphans;
std::map<uint256, COrphanTx> mapOrphanTransactions GUARDED_BY(g_cs_orphans);

void EraseOrphansFor(NodeId peer);

/*增加节点的错误行为得分。*/
void Misbehaving(NodeId nodeid, int howmuch, const std::string& message="") EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/*本地地址广播之间的平均延迟（秒）。*/
static constexpr unsigned int AVG_LOCAL_ADDRESS_BROADCAST_INTERVAL = 24 * 60 * 60;
/*对等地址广播之间的平均延迟（秒）。*/
static const unsigned int AVG_ADDRESS_BROADCAST_INTERVAL = 30;
/*涓流式库存传输之间的平均延迟（秒）。
 *块和白名单接收者绕过这一点，出站对等机获得一半的延迟。*/

static const unsigned int INVENTORY_BROADCAST_INTERVAL = 5;
/*每次传输要发送的最大库存项目数。
 *限制低收费交易泛滥的影响。*/

static constexpr unsigned int INVENTORY_BROADCAST_MAX = 7 * INVENTORY_BROADCAST_INTERVAL;
/*feefilter广播之间的平均延迟（秒）。*/
static constexpr unsigned int AVG_FEEFILTER_BROADCAST_INTERVAL = 10 * 60;
/*显著变化后的最大滤波器广播延迟。*/
static constexpr unsigned int MAX_FEEFILTER_CHANGE_DELAY = 5 * 60;

//内部物质
namespace {
    /*fsyncStarted的节点数。*/
    int nSyncStarted GUARDED_BY(cs_main) = 0;

    /*
     *接收块的来源，保存后可以发送拒绝。
     *消息或在之后处理时禁止它们。
     *如果不应将节点设置为
     *如果该块无效，将受到惩罚。
     **/

    std::map<uint256, std::pair<NodeId, bool>> mapBlockSource GUARDED_BY(cs_main);

    /*
     *筛选最近被拒绝的交易
     *接受MORYPOOL。在链端之前，不会对这些进行重新排序
     *更改，此时整个过滤器被重置。
     *
     *如果没有这个过滤器，我们将从我们的每个同行重新请求TXS，
     *大大增加带宽消耗。例如，使用100
     *同龄人，其中一半中继不接受的Tx，可能是50倍。
     *带宽增加。洪水袭击者试图翻滚
     *使用最小大小的60字节筛选，事务可能会设法发送
     *1000/秒，如果我们有快速的同龄人，那么我们选择120000给同龄人
     *两分钟的窗口，向我们发送发票。
     *
     *降低假阳性率相当便宜，因此我们在
     *百万，使用户不太可能对此有问题
     *过滤器。
     *
     *使用的内存：1.3 MB
     **/

    std::unique_ptr<CRollingBloomFilter> recentRejects GUARDED_BY(cs_main);
    uint256 hashRecentRejectsChainTip GUARDED_BY(cs_main);

    /*飞行中的块以及要下载的队列中的块。*/
    struct QueuedBlock {
        uint256 hash;
const CBlockIndex* pindex;                               //！<可选】。
bool fValidatedHeaders;                                  //！<请求时此块是否具有已验证的头。
std::unique_ptr<PartiallyDownloadedBlock> partialBlock;  //！<可选，用于cmpctblock下载
    };
    std::map<uint256, std::pair<NodeId, std::list<QueuedBlock>::iterator> > mapBlocksInFlight GUARDED_BY(cs_main);

    /*已设置为使用压缩块宣布的节点堆栈*/
    std::list<NodeId> lNodesAnnouncingHeaderAndIDs GUARDED_BY(cs_main);

    /*首选块下载对等数。*/
    int nPreferredDownload GUARDED_BY(cs_main) = 0;

    /*正在从中下载块的对等机的数目。*/
    int nPeersWithValidatedDownloads GUARDED_BY(cs_main) = 0;

    /*具有m_链\u同步.m_保护的出站对等数。*/
    int g_outbound_peers_with_protect_from_disconnect GUARDED_BY(cs_main) = 0;

    /*当我们的小费最后一次更新时。*/
    std::atomic<int64_t> g_last_tip_update(0);

    /*中继图*/
    typedef std::map<uint256, CTransactionRef> MapRelay;
    MapRelay mapRelay GUARDED_BY(cs_main);
    /*过期时间已排序的（过期时间，中继映射项）对列表。*/
    std::deque<std::pair<int64_t, MapRelay::iterator>> vRelayExpiration GUARDED_BY(cs_main);

std::atomic<int64_t> nTimeBestReceived(0); //仅用于通知钱包我们上次收到一个街区的时间

    struct IteratorComparator
    {
        template<typename I>
        bool operator()(const I& a, const I& b) const
        {
            return &(*a) < &(*b);
        }
    };
    std::map<COutPoint, std::set<std::map<uint256, COrphanTx>::iterator, IteratorComparator>> mapOrphanTransactionsByPrev GUARDED_BY(g_cs_orphans);

    static size_t vExtraTxnForCompactIt GUARDED_BY(g_cs_orphans) = 0;
    static std::vector<std::pair<uint256, CTransactionRef>> vExtraTxnForCompact GUARDED_BY(g_cs_orphans);
} //命名空间

namespace {
struct CBlockReject {
    unsigned char chRejectCode;
    std::string strRejectReason;
    uint256 hashBlock;
};

/*
 *维护节点的验证特定状态，由cs_main保护，而不是
 *由CNode自己锁。这简化了异步操作，其中
 *传入数据的处理在processMessage调用返回后完成，
 *我们不再持有节点的锁。
 **/

struct CNodeState {
//！对方地址
    const CService address;
//！我们是否建立了完全的联系。
    bool fCurrentlyConnected;
//！累积的错误行为分数。
    int nMisbehavior;
//！是否应该断开和禁止此对等端（除非白名单）。
    bool fShouldBan;
//！此对等方的字符串名称（用于调试/日志记录）。
    const std::string name;
//！要通知此对等方的异步确定的块拒绝列表。
    std::vector<CBlockReject> rejects;
//！我们所知道的最著名的程序块已经公布。
    const CBlockIndex *pindexBestKnownBlock;
//！此对等方公布的最后一个未知块的哈希。
    uint256 hashLastUnknownBlock;
//！最后一个完整的街区我们都有。
    const CBlockIndex *pindexLastCommonBlock;
//！我们发送给同行的最佳邮件头。
    const CBlockIndex *pindexBestHeaderSent;
//！当前连续未连接邮件头公告的长度
    int nUnconnectingHeaders;
//！是否已开始与此对等服务器同步邮件头。
    bool fSyncStarted;
//！何时可能断开对等机以停止邮件头下载
    int64_t nHeadersSyncTimeout;
//！因为当我们停止下载进度时（以微秒计），或者0。
    int64_t nStallingSince;
    std::list<QueuedBlock> vBlocksInFlight;
//！当vblocksinflight中的第一个条目开始下载时。当vblocksinflight为空时，不要在意。
    int64_t nDownloadingSince;
    int nBlocksInFlight;
    int nBlocksInFlightValidHeaders;
//！我们是否认为这是首选的下载对等。
    bool fPreferredDownload;
//！此对等方是否需要invs或headers（如果可能）用于阻止通知。
    bool fPreferHeaders;
//！此对等方是否希望invs或cmpctblock（如果可能）用于块通知。
    bool fPreferHeaderAndIDs;
    /*
      *如果我们请求，该对等方是否会向我们发送cmpctblocks。
      *这不用于门请求逻辑，因为我们只关心fsupportsdesiredcmpctversion，
      *但用作我们发送的压缩块版本（fwantscmpctwitness）的“锁定”标志。
      **/

    bool fProvidesHeaderAndIDs;
//！这个同伴是否能给我们提供证人
    bool fHaveWitness;
//！此对等方是否希望在cmpctblocks/blocktxns中有证人
    bool fWantsCmpctWitness;
    /*
     *如果我们已经向该对等方宣布了节点见证：该对等方是否以cmpctblocks/blocktxns发送见证人，
     *否则：此对等方是否在cmpctblocks/blocktxns中发送非见证人。
     **/

    bool fSupportsDesiredCmpctVersion;

    /*用于强制链同步超时的状态
      *仅对出站、非手动连接有效，具有
      *M_protect==false
      *算法：如果一个对等方最著名的块的工作比我们的提示少，
      *设置一个超时链\同步\未来超时秒数：
      *-如果超时时，他们最著名的块现在比我们的提示有更多的工作
      *设置超时后，重置或清除超时
      （与我们目前的Tip工作进行比较后）
      *-如果在超时时，它们最著名的块的工作仍然少于我们的
      *设置超时时提示已执行，然后发送getheaders消息，
      *并设置一个较短的超时，在将来头响应时间秒。
      *如果在新的超时时间
      *已到达，断开连接。
      **/

    struct ChainSyncTimeoutState {
//！用于检查对等机是否已充分同步的超时。
        int64_t m_timeout;
//！我们在同行链上需要的工作的标题
        const CBlockIndex * m_work_header;
//！达到超时后，发送getheaders后设置为true
        bool m_sent_getheaders;
//！是否保护此对等机免受由于坏链/慢链导致的断开连接的影响
        bool m_protect;
    };

    ChainSyncTimeoutState m_chain_sync;

//！上一次新区块公告时间
    int64_t m_last_block_announcement;

    CNodeState(CAddress addrIn, std::string addrNameIn) : address(addrIn), name(addrNameIn) {
        fCurrentlyConnected = false;
        nMisbehavior = 0;
        fShouldBan = false;
        pindexBestKnownBlock = nullptr;
        hashLastUnknownBlock.SetNull();
        pindexLastCommonBlock = nullptr;
        pindexBestHeaderSent = nullptr;
        nUnconnectingHeaders = 0;
        fSyncStarted = false;
        nHeadersSyncTimeout = 0;
        nStallingSince = 0;
        nDownloadingSince = 0;
        nBlocksInFlight = 0;
        nBlocksInFlightValidHeaders = 0;
        fPreferredDownload = false;
        fPreferHeaders = false;
        fPreferHeaderAndIDs = false;
        fProvidesHeaderAndIDs = false;
        fHaveWitness = false;
        fWantsCmpctWitness = false;
        fSupportsDesiredCmpctVersion = false;
        m_chain_sync = { 0, nullptr, false, false };
        m_last_block_announcement = 0;
    }
};

/*按节点状态维护地图。*/
static std::map<NodeId, CNodeState> mapNodeState GUARDED_BY(cs_main);

static CNodeState *State(NodeId pnode) EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
    std::map<NodeId, CNodeState>::iterator it = mapNodeState.find(pnode);
    if (it == mapNodeState.end())
        return nullptr;
    return &it->second;
}

static void UpdatePreferredDownload(CNode* node, CNodeState* state) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    nPreferredDownload -= state->fPreferredDownload;

//此节点是否应标记为首选下载节点。
    state->fPreferredDownload = (!node->fInbound || node->fWhitelisted) && !node->fOneShot && !node->fClient;

    nPreferredDownload += state->fPreferredDownload;
}

static void PushNodeVersion(CNode *pnode, CConnman* connman, int64_t nTime)
{
    ServiceFlags nLocalNodeServices = pnode->GetLocalServices();
    uint64_t nonce = pnode->GetLocalNonce();
    int nNodeStartingHeight = pnode->GetMyStartingHeight();
    NodeId nodeid = pnode->GetId();
    CAddress addr = pnode->addr;

    CAddress addrYou = (addr.IsRoutable() && !IsProxy(addr) ? addr : CAddress(CService(), addr.nServices));
    CAddress addrMe = CAddress(CService(), nLocalNodeServices);

    connman->PushMessage(pnode, CNetMsgMaker(INIT_PROTO_VERSION).Make(NetMsgType::VERSION, PROTOCOL_VERSION, (uint64_t)nLocalNodeServices, nTime, addrYou, addrMe,
            nonce, strSubVersion, nNodeStartingHeight, ::fRelayTxes));

    if (fLogIPs) {
        LogPrint(BCLog::NET, "send version message: version %d, blocks=%d, us=%s, them=%s, peer=%d\n", PROTOCOL_VERSION, nNodeStartingHeight, addrMe.ToString(), addrYou.ToString(), nodeid);
    } else {
        LogPrint(BCLog::NET, "send version message: version %d, blocks=%d, us=%s, peer=%d\n", PROTOCOL_VERSION, nNodeStartingHeight, addrMe.ToString(), nodeid);
    }
}

//返回一个bool，指示是否请求了此块。
//如果一个块已被/未被/接收并超时或与另一个对等机一起启动，也将使用该块。
static bool MarkBlockAsReceived(const uint256& hash) EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
    std::map<uint256, std::pair<NodeId, std::list<QueuedBlock>::iterator> >::iterator itInFlight = mapBlocksInFlight.find(hash);
    if (itInFlight != mapBlocksInFlight.end()) {
        CNodeState *state = State(itInFlight->second.first);
        assert(state != nullptr);
        state->nBlocksInFlightValidHeaders -= itInFlight->second.second->fValidatedHeaders;
        if (state->nBlocksInFlightValidHeaders == 0 && itInFlight->second.second->fValidatedHeaders) {
//收到队列上最后一个验证的块。
            nPeersWithValidatedDownloads--;
        }
        if (state->vBlocksInFlight.begin() == itInFlight->second.second) {
//收到队列上的第一个块，请更新下一个块的开始下载时间
            state->nDownloadingSince = std::max(state->nDownloadingSince, GetTimeMicros());
        }
        state->vBlocksInFlight.erase(itInFlight->second.second);
        state->nBlocksInFlight--;
        state->nStallingSince = 0;
        mapBlocksInFlight.erase(itInFlight);
        return true;
    }
    return false;
}

//如果块已经从同一个对等机飞行，则返回FALSE，仍设置PIT
//只有在保持同一个CSU主锁的情况下，PIT才有效。
static bool MarkBlockAsInFlight(NodeId nodeid, const uint256& hash, const CBlockIndex* pindex = nullptr, std::list<QueuedBlock>::iterator** pit = nullptr) EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
    CNodeState *state = State(nodeid);
    assert(state != nullptr);

//如果它来自同一个节点，大多数东西都会短路
    std::map<uint256, std::pair<NodeId, std::list<QueuedBlock>::iterator> >::iterator itInFlight = mapBlocksInFlight.find(hash);
    if (itInFlight != mapBlocksInFlight.end() && itInFlight->second.first == nodeid) {
        if (pit) {
            *pit = &itInFlight->second.second;
        }
        return false;
    }

//确保它没有在某个地方列出。
    MarkBlockAsReceived(hash);

    std::list<QueuedBlock>::iterator it = state->vBlocksInFlight.insert(state->vBlocksInFlight.end(),
            {hash, pindex, pindex != nullptr, std::unique_ptr<PartiallyDownloadedBlock>(pit ? new PartiallyDownloadedBlock(&mempool) : nullptr)});
    state->nBlocksInFlight++;
    state->nBlocksInFlightValidHeaders += it->fValidatedHeaders;
    if (state->nBlocksInFlight == 1) {
//我们正在从这个对等端开始一个块下载（批处理）。
        state->nDownloadingSince = GetTimeMicros();
    }
    if (state->nBlocksInFlightValidHeaders == 1 && pindex != nullptr) {
        nPeersWithValidatedDownloads++;
    }
    itInFlight = mapBlocksInFlight.insert(std::make_pair(hash, std::make_pair(nodeid, it))).first;
    if (pit)
        *pit = &itInFlight->second.second;
    return true;
}

/*检查对等端公布的最后一个未知块是否未知。*/
static void ProcessBlockAvailability(NodeId nodeid) EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
    CNodeState *state = State(nodeid);
    assert(state != nullptr);

    if (!state->hashLastUnknownBlock.IsNull()) {
        const CBlockIndex* pindex = LookupBlockIndex(state->hashLastUnknownBlock);
        if (pindex && pindex->nChainWork > 0) {
            if (state->pindexBestKnownBlock == nullptr || pindex->nChainWork >= state->pindexBestKnownBlock->nChainWork) {
                state->pindexBestKnownBlock = pindex;
            }
            state->hashLastUnknownBlock.SetNull();
        }
    }
}

/*更新有关假定对等机具有哪些块的跟踪信息。*/
static void UpdateBlockAvailability(NodeId nodeid, const uint256 &hash) EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
    CNodeState *state = State(nodeid);
    assert(state != nullptr);

    ProcessBlockAvailability(nodeid);

    const CBlockIndex* pindex = LookupBlockIndex(hash);
    if (pindex && pindex->nChainWork > 0) {
//宣布了一个更好的街区。
        if (state->pindexBestKnownBlock == nullptr || pindex->nChainWork >= state->pindexBestKnownBlock->nChainWork) {
            state->pindexBestKnownBlock = pindex;
        }
    } else {
//宣布了一个未知的块；假设最新的块是最好的块。
        state->hashLastUnknownBlock = hash;
    }
}

/*
 *当一个对等机向我们发送一个有效的块时，指示它向我们宣布该块。
 *如果可能，通过将其nodeid添加到
 *将该列表保持在一定大小下
 *必要时拆卸第一个元件。
 **/

static void MaybeSetPeerAsAnnouncingHeaderAndIDs(NodeId nodeid, CConnman* connman) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
    CNodeState* nodestate = State(nodeid);
    if (!nodestate || !nodestate->fSupportsDesiredCmpctVersion) {
//千万不要问那些不能提供证人的同龄人。
        return;
    }
    if (nodestate->fProvidesHeaderAndIDs) {
        for (std::list<NodeId>::iterator it = lNodesAnnouncingHeaderAndIDs.begin(); it != lNodesAnnouncingHeaderAndIDs.end(); it++) {
            if (*it == nodeid) {
                lNodesAnnouncingHeaderAndIDs.erase(it);
                lNodesAnnouncingHeaderAndIDs.push_back(nodeid);
                return;
            }
        }
        connman->ForNode(nodeid, [connman](CNode* pfrom){
            AssertLockHeld(cs_main);
            uint64_t nCMPCTBLOCKVersion = (pfrom->GetLocalServices() & NODE_WITNESS) ? 2 : 1;
            if (lNodesAnnouncingHeaderAndIDs.size() >= 3) {
//根据BIP152，我们只有3个同行宣布
//使用紧凑编码的块。
                connman->ForNode(lNodesAnnouncingHeaderAndIDs.front(), [connman, nCMPCTBLOCKVersion](CNode* pnodeStop){
                    AssertLockHeld(cs_main);
                    /*nman->pushmessage（pnodestop，cnetmsgmaker（pnodestop->getsendversion（））.make（netmsgtype:：sendcmpct，/*fannounceusingcmpctblock=*/false，ncmpctblockversion））；
                    回归真实；
                （}）；
                lnodesannouncingheaderandids.pop_front（）；
            }
            connman->pushmessage（pfrom，cnetmsgmaker（pfrom->getsendVersion（））.make（netmsgtype:：sendcmpct，/*fannounceusingcmpctblock*/true, nCMPCTBLOCKVersion));

            lNodesAnnouncingHeaderAndIDs.push_back(pfrom->GetId());
            return true;
        });
    }
}

static bool TipMayBeStale(const Consensus::Params &consensusParams) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
    if (g_last_tip_update == 0) {
        g_last_tip_update = GetTime();
    }
    return g_last_tip_update < GetTime() - consensusParams.nPowTargetSpacing * 3 && mapBlocksInFlight.empty();
}

static bool CanDirectFetch(const Consensus::Params &consensusParams) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    return chainActive.Tip()->GetBlockTime() > GetAdjustedTime() - consensusParams.nPowTargetSpacing * 20;
}

static bool PeerHasHeader(CNodeState *state, const CBlockIndex *pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (state->pindexBestKnownBlock && pindex == state->pindexBestKnownBlock->GetAncestor(pindex->nHeight))
        return true;
    if (state->pindexBestHeaderSent && pindex == state->pindexBestHeaderSent->GetAncestor(pindex->nHeight))
        return true;
    return false;
}

/*更新pindexlastcommonblock并添加不在运行中缺少的vblock继承者，直到
 *最多计数个条目。*/

static void FindNextBlocksToDownload(NodeId nodeid, unsigned int count, std::vector<const CBlockIndex*>& vBlocks, NodeId& nodeStaller, const Consensus::Params& consensusParams) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (count == 0)
        return;

    vBlocks.reserve(vBlocks.size() + count);
    CNodeState *state = State(nodeid);
    assert(state != nullptr);

//确保pindexbestknownblock是最新的，我们需要它。
    ProcessBlockAvailability(nodeid);

    if (state->pindexBestKnownBlock == nullptr || state->pindexBestKnownBlock->nChainWork < chainActive.Tip()->nChainWork || state->pindexBestKnownBlock->nChainWork < nMinimumChainWork) {
//这个同龄人没有什么有趣的。
        return;
    }

    if (state->pindexLastCommonBlock == nullptr) {
//通过猜测我们最好的技巧的父母是分叉点来快速引导。
//在任何一个方向上猜测错误都不是问题。
        state->pindexLastCommonBlock = chainActive[std::min(state->pindexBestKnownBlock->nHeight, chainActive.Height())];
    }

//如果对等体重新组织，则以前的pindexlastcommonblock可能不是祖先
//它现在的提示。回去解决这个问题。
    state->pindexLastCommonBlock = LastCommonAncestor(state->pindexLastCommonBlock, state->pindexBestKnownBlock);
    if (state->pindexLastCommonBlock == state->pindexBestKnownBlock)
        return;

    std::vector<const CBlockIndex*> vToFetch;
    const CBlockIndex *pindexWalk = state->pindexLastCommonBlock;
//永远不要获取超过我们所知道的最好的块，或超过最后一个块的块下载窗口+1。
//我们与这个对等体有共同的链接块。+1是这样我们就可以检测到失速，也就是说如果我们能够
//如果窗口大于1，下载下一个块。
    int nWindowEnd = state->pindexLastCommonBlock->nHeight + BLOCK_DOWNLOAD_WINDOW;
    int nMaxHeight = std::min<int>(state->pindexBestKnownBlock->nHeight, nWindowEnd + 1);
    NodeId waitingfor = -1;
    while (pindexWalk->nHeight < nMaxHeight) {
//最多可读取128个（如果块数超过所需的块数），Pindexwalk的继任者（朝向
//pindexbestknownblock）插入vtofetch。我们取128，因为cblockindex:：getAncestor可能同样昂贵
//循环访问大约100个cblockindex*条目。
        int nToFetch = std::min(nMaxHeight - pindexWalk->nHeight, std::max<int>(count - vBlocks.size(), 128));
        vToFetch.resize(nToFetch);
        pindexWalk = state->pindexBestKnownBlock->GetAncestor(pindexWalk->nHeight + nToFetch);
        vToFetch[nToFetch - 1] = pindexWalk;
        for (unsigned int i = nToFetch - 1; i > 0; i--) {
            vToFetch[i - 1] = vToFetch[i]->pprev;
        }

//在vtofetch（正向）中迭代这些块，并添加
//尚未下载，也未飞向vblock。同时，更新
//只要所有祖先都已下载，或者
//已经是我们的链条的一部分（因此即使修剪了也不需要它）。
        for (const CBlockIndex* pindex : vToFetch) {
            if (!pindex->IsValid(BLOCK_VALID_TREE)) {
//我们认为这个对等点所在的链无效。
                return;
            }
            if (!State(nodeid)->fHaveWitness && IsWitnessEnabled(pindex->pprev, consensusParams)) {
//我们不会从此对等端下载此块或其后代。
                return;
            }
            if (pindex->nStatus & BLOCK_HAVE_DATA || chainActive.Contains(pindex)) {
                if (pindex->HaveTxsDownloaded())
                    state->pindexLastCommonBlock = pindex;
            } else if (mapBlocksInFlight.count(pindex->GetBlockHash()) == 0) {
//块尚未下载，而且尚未飞行。
                if (pindex->nHeight > nWindowEnd) {
//我们到达了窗口的尽头。
                    if (vBlocks.size() == 0 && waitingfor != nodeid) {
//我们无法获取任何内容，但如果下载窗口更大，我们将无法获取。
                        nodeStaller = waitingfor;
                    }
                    return;
                }
                vBlocks.push_back(pindex);
                if (vBlocks.size() == count) {
                    return;
                }
            } else if (waitingfor == -1) {
//这是第一个已经在飞行区的飞机。
                waitingfor = mapBlocksInFlight[pindex->GetBlockHash()].first;
            }
        }
    }
}

} //命名空间

//此函数用于测试过时的尖端收回逻辑，请参见
//丹尼洛夫服务测试.cpp
void UpdateLastBlockAnnounceTime(NodeId node, int64_t time_in_seconds)
{
    LOCK(cs_main);
    CNodeState *state = State(node);
    if (state) state->m_last_block_announcement = time_in_seconds;
}

//对于出站对等机返回true，不包括手动连接、触角和
//短期行为
static bool IsOutboundDisconnectionCandidate(const CNode *node)
{
    return !(node->fInbound || node->m_manual_connection || node->fFeeler || node->fOneShot);
}

void PeerLogicValidation::InitializeNode(CNode *pnode) {
    CAddress addr = pnode->addr;
    std::string addrName = pnode->GetAddrName();
    NodeId nodeid = pnode->GetId();
    {
        LOCK(cs_main);
        mapNodeState.emplace_hint(mapNodeState.end(), std::piecewise_construct, std::forward_as_tuple(nodeid), std::forward_as_tuple(addr, std::move(addrName)));
    }
    if(!pnode->fInbound)
        PushNodeVersion(pnode, connman, GetTime());
}

void PeerLogicValidation::FinalizeNode(NodeId nodeid, bool& fUpdateConnectionTime) {
    fUpdateConnectionTime = false;
    LOCK(cs_main);
    CNodeState *state = State(nodeid);
    assert(state != nullptr);

    if (state->fSyncStarted)
        nSyncStarted--;

    if (state->nMisbehavior == 0 && state->fCurrentlyConnected) {
        fUpdateConnectionTime = true;
    }

    for (const QueuedBlock& entry : state->vBlocksInFlight) {
        mapBlocksInFlight.erase(entry.hash);
    }
    EraseOrphansFor(nodeid);
    nPreferredDownload -= state->fPreferredDownload;
    nPeersWithValidatedDownloads -= (state->nBlocksInFlightValidHeaders != 0);
    assert(nPeersWithValidatedDownloads >= 0);
    g_outbound_peers_with_protect_from_disconnect -= state->m_chain_sync.m_protect;
    assert(g_outbound_peers_with_protect_from_disconnect >= 0);

    mapNodeState.erase(nodeid);

    if (mapNodeState.empty()) {
//删除最后一个对等点后，执行一致性检查。
        assert(mapBlocksInFlight.empty());
        assert(nPreferredDownload == 0);
        assert(nPeersWithValidatedDownloads == 0);
        assert(g_outbound_peers_with_protect_from_disconnect == 0);
    }
    LogPrint(BCLog::NET, "Cleared nodestate for peer=%d\n", nodeid);
}

bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats) {
    LOCK(cs_main);
    CNodeState *state = State(nodeid);
    if (state == nullptr)
        return false;
    stats.nMisbehavior = state->nMisbehavior;
    stats.nSyncHeight = state->pindexBestKnownBlock ? state->pindexBestKnownBlock->nHeight : -1;
    stats.nCommonHeight = state->pindexLastCommonBlock ? state->pindexLastCommonBlock->nHeight : -1;
    for (const QueuedBlock& queue : state->vBlocksInFlight) {
        if (queue.pindex)
            stats.vHeightInFlight.push_back(queue.pindex->nHeight);
    }
    return true;
}

//////////////////////////////////////////////////
//
//映射孤立事务
//

static void AddToCompactExtraTransactions(const CTransactionRef& tx) EXCLUSIVE_LOCKS_REQUIRED(g_cs_orphans)
{
    size_t max_extra_txn = gArgs.GetArg("-blockreconstructionextratxn", DEFAULT_BLOCK_RECONSTRUCTION_EXTRA_TXN);
    if (max_extra_txn <= 0)
        return;
    if (!vExtraTxnForCompact.size())
        vExtraTxnForCompact.resize(max_extra_txn);
    vExtraTxnForCompact[vExtraTxnForCompactIt] = std::make_pair(tx->GetWitnessHash(), tx);
    vExtraTxnForCompactIt = (vExtraTxnForCompactIt + 1) % max_extra_txn;
}

bool AddOrphanTx(const CTransactionRef& tx, NodeId peer) EXCLUSIVE_LOCKS_REQUIRED(g_cs_orphans)
{
    const uint256& hash = tx->GetHash();
    if (mapOrphanTransactions.count(hash))
        return false;

//忽略大交易，以避免
//送大孤儿记忆衰竭攻击。如果一个同伴有合法的
//我们假设有一个缺少父级的大型事务
//在母公司交易后，它将在稍后重新广播。
//已开采或接收。
//100个孤儿，每个孤儿的最大容量为100000字节
//最多10兆字节的孤立文件，比上一个索引多一些（在最坏的情况下）：
    unsigned int sz = GetTransactionWeight(*tx);
    if (sz > MAX_STANDARD_TX_WEIGHT)
    {
        LogPrint(BCLog::MEMPOOL, "ignoring large orphan tx (size: %u, hash: %s)\n", sz, hash.ToString());
        return false;
    }

    auto ret = mapOrphanTransactions.emplace(hash, COrphanTx{tx, peer, GetTime() + ORPHAN_TX_EXPIRE_TIME});
    assert(ret.second);
    for (const CTxIn& txin : tx->vin) {
        mapOrphanTransactionsByPrev[txin.prevout].insert(ret.first);
    }

    AddToCompactExtraTransactions(tx);

    LogPrint(BCLog::MEMPOOL, "stored orphan tx %s (mapsz %u outsz %u)\n", hash.ToString(),
             mapOrphanTransactions.size(), mapOrphanTransactionsByPrev.size());
    return true;
}

int static EraseOrphanTx(uint256 hash) EXCLUSIVE_LOCKS_REQUIRED(g_cs_orphans)
{
    std::map<uint256, COrphanTx>::iterator it = mapOrphanTransactions.find(hash);
    if (it == mapOrphanTransactions.end())
        return 0;
    for (const CTxIn& txin : it->second.tx->vin)
    {
        auto itPrev = mapOrphanTransactionsByPrev.find(txin.prevout);
        if (itPrev == mapOrphanTransactionsByPrev.end())
            continue;
        itPrev->second.erase(it);
        if (itPrev->second.empty())
            mapOrphanTransactionsByPrev.erase(itPrev);
    }
    mapOrphanTransactions.erase(it);
    return 1;
}

void EraseOrphansFor(NodeId peer)
{
    LOCK(g_cs_orphans);
    int nErased = 0;
    std::map<uint256, COrphanTx>::iterator iter = mapOrphanTransactions.begin();
    while (iter != mapOrphanTransactions.end())
    {
std::map<uint256, COrphanTx>::iterator maybeErase = iter++; //递增以避免迭代器无效
        if (maybeErase->second.fromPeer == peer)
        {
            nErased += EraseOrphanTx(maybeErase->second.tx->GetHash());
        }
    }
    if (nErased > 0) LogPrint(BCLog::MEMPOOL, "Erased %d orphan tx from peer=%d\n", nErased, peer);
}


unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans)
{
    LOCK(g_cs_orphans);

    unsigned int nEvicted = 0;
    static int64_t nNextSweep;
    int64_t nNow = GetTime();
    if (nNextSweep <= nNow) {
//清除过期的孤立池项：
        int nErased = 0;
        int64_t nMinExpTime = nNow + ORPHAN_TX_EXPIRE_TIME - ORPHAN_TX_EXPIRE_INTERVAL;
        std::map<uint256, COrphanTx>::iterator iter = mapOrphanTransactions.begin();
        while (iter != mapOrphanTransactions.end())
        {
            std::map<uint256, COrphanTx>::iterator maybeErase = iter++;
            if (maybeErase->second.nTimeExpire <= nNow) {
                nErased += EraseOrphanTx(maybeErase->second.tx->GetHash());
            } else {
                nMinExpTime = std::min(maybeErase->second.nTimeExpire, nMinExpTime);
            }
        }
//在下一个条目过期5分钟后再次扫描，以批处理线性扫描。
        nNextSweep = nMinExpTime + ORPHAN_TX_EXPIRE_INTERVAL;
        if (nErased > 0) LogPrint(BCLog::MEMPOOL, "Erased %d orphan tx due to expiration\n", nErased);
    }
    FastRandomContext rng;
    while (mapOrphanTransactions.size() > nMaxOrphans)
    {
//逐出随机孤儿：
        uint256 randomhash = rng.rand256();
        std::map<uint256, COrphanTx>::iterator it = mapOrphanTransactions.lower_bound(randomhash);
        if (it == mapOrphanTransactions.end())
            it = mapOrphanTransactions.begin();
        EraseOrphanTx(it->first);
        ++nEvicted;
    }
    return nEvicted;
}

/*
 *根据`-banscore`的值，将行为不端的同伴标记为被禁止。
 **/

void Misbehaving(NodeId pnode, int howmuch, const std::string& message) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (howmuch == 0)
        return;

    CNodeState *state = State(pnode);
    if (state == nullptr)
        return;

    state->nMisbehavior += howmuch;
    int banscore = gArgs.GetArg("-banscore", DEFAULT_BANSCORE_THRESHOLD);
    std::string message_prefixed = message.empty() ? "" : (": " + message);
    if (state->nMisbehavior >= banscore && state->nMisbehavior - howmuch < banscore)
    {
        LogPrint(BCLog::NET, "%s: %s peer=%d (%d -> %d) BAN THRESHOLD EXCEEDED%s\n", __func__, state->name, pnode, state->nMisbehavior-howmuch, state->nMisbehavior, message_prefixed);
        state->fShouldBan = true;
    } else
        LogPrint(BCLog::NET, "%s: %s peer=%d (%d -> %d)%s\n", __func__, state->name, pnode, state->nMisbehavior-howmuch, state->nMisbehavior, message_prefixed);
}








//////////////////////////////////////////////////
//
//区块链->下载逻辑通知
//

//为了防止指纹攻击，只在
//活动链，如果它们不超过一个月（时间和
//最好的等效工作证明）比我们所知道的最好的头链
//我们在某种程度上完全验证了它们。
static bool BlockRequestAllowed(const CBlockIndex* pindex, const Consensus::Params& consensusParams) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
    if (chainActive.Contains(pindex)) return true;
    return pindex->IsValid(BLOCK_VALID_SCRIPTS) && (pindexBestHeader != nullptr) &&
        (pindexBestHeader->GetBlockTime() - pindex->GetBlockTime() < STALE_RELAY_AGE_LIMIT) &&
        (GetBlockProofEquivalentTime(*pindexBestHeader, *pindex, *pindexBestHeader, consensusParams) < STALE_RELAY_AGE_LIMIT);
}

PeerLogicValidation::PeerLogicValidation(CConnman* connmanIn, CScheduler &scheduler, bool enable_bip61)
    : connman(connmanIn), m_stale_tip_check_time(0), m_enable_bip61(enable_bip61) {

//初始化启动时无法构造的全局变量。
    recentRejects.reset(new CRollingBloomFilter(120000, 0.000001));

    const Consensus::Params& consensusParams = Params().GetConsensus();
//陈旧的提示检查和对等驱逐在两个不同的计时器上，但是我们
//不希望它们由于调度程序的漂移而失去同步，所以我们
//将它们组合在一个函数中，并以更快的速度调度（对等逐出）
//计时器。
    static_assert(EXTRA_PEER_CHECK_INTERVAL < STALE_CHECK_INTERVAL, "peer eviction timer should be less than stale tip check timer");
    scheduler.scheduleEvery(std::bind(&PeerLogicValidation::CheckForStaleTipAndEvictPeers, this, consensusParams), EXTRA_PEER_CHECK_INTERVAL * 1000);
}

/*
 *基于新连接的孤立txn池项（eraseorphantx）
 *块。还可以保存上次提示更新的时间。
 **/

void PeerLogicValidation::BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindex, const std::vector<CTransactionRef>& vtxConflicted) {
    LOCK(g_cs_orphans);

    std::vector<uint256> vOrphanErase;

    for (const CTransactionRef& ptx : pblock->vtx) {
        const CTransaction& tx = *ptx;

//我们必须逐出哪些孤立池条目？
        for (const auto& txin : tx.vin) {
            auto itByPrev = mapOrphanTransactionsByPrev.find(txin.prevout);
            if (itByPrev == mapOrphanTransactionsByPrev.end()) continue;
            for (auto mi = itByPrev->second.begin(); mi != itByPrev->second.end(); ++mi) {
                const CTransaction& orphanTx = *(*mi)->second.tx;
                const uint256& orphanHash = orphanTx.GetHash();
                vOrphanErase.push_back(orphanHash);
            }
        }
    }

//删除此块包含或排除的孤立事务
    if (vOrphanErase.size()) {
        int nErased = 0;
        for (const uint256& orphanHash : vOrphanErase) {
            nErased += EraseOrphanTx(orphanHash);
        }
        LogPrint(BCLog::MEMPOOL, "Erased %d orphan tx included or conflicted by block\n", nErased);
    }

    g_last_tip_update = GetTime();
}

//以下所有缓存一个最近的块，并受cs_most_recent_块保护
static CCriticalSection cs_most_recent_block;
static std::shared_ptr<const CBlock> most_recent_block GUARDED_BY(cs_most_recent_block);
static std::shared_ptr<const CBlockHeaderAndShortTxIDs> most_recent_compact_block GUARDED_BY(cs_most_recent_block);
static uint256 most_recent_block_hash GUARDED_BY(cs_most_recent_block);
static bool fWitnessesPresentInMostRecentCompactBlock GUARDED_BY(cs_most_recent_block);

/*
 *保持最佳显示块的状态，并快速宣布一个紧凑块
 *兼容的对等机。
 **/

void PeerLogicValidation::NewPoWValidBlock(const CBlockIndex *pindex, const std::shared_ptr<const CBlock>& pblock) {
    std::shared_ptr<const CBlockHeaderAndShortTxIDs> pcmpctblock = std::make_shared<const CBlockHeaderAndShortTxIDs> (*pblock, true);
    const CNetMsgMaker msgMaker(PROTOCOL_VERSION);

    LOCK(cs_main);

    static int nHighestFastAnnounce = 0;
    if (pindex->nHeight <= nHighestFastAnnounce)
        return;
    nHighestFastAnnounce = pindex->nHeight;

    bool fWitnessEnabled = IsWitnessEnabled(pindex->pprev, Params().GetConsensus());
    uint256 hashBlock(pblock->GetHash());

    {
        LOCK(cs_most_recent_block);
        most_recent_block_hash = hashBlock;
        most_recent_block = pblock;
        most_recent_compact_block = pcmpctblock;
        fWitnessesPresentInMostRecentCompactBlock = fWitnessEnabled;
    }

    connman->ForEachNode([this, &pcmpctblock, pindex, &msgMaker, fWitnessEnabled, &hashBlock](CNode* pnode) {
        AssertLockHeld(cs_main);

//TODO:避免在此处重复序列化
        if (pnode->nVersion < INVALID_CB_NO_BAN_VERSION || pnode->fDisconnect)
            return;
        ProcessBlockAvailability(pnode->GetId());
        CNodeState &state = *State(pnode->GetId());
//如果对方有，或者我们已经向他们宣布了上一个块，
//但我们认为他们没有这个，去宣布吧
        if (state.fPreferHeaderAndIDs && (!fWitnessEnabled || state.fWantsCmpctWitness) &&
                !PeerHasHeader(&state, pindex) && PeerHasHeader(&state, pindex->pprev)) {

            LogPrint(BCLog::NET, "%s sending header-and-ids %s to peer=%d\n", "PeerLogicValidation::NewPoWValidBlock",
                    hashBlock.ToString(), pnode->GetId());
            connman->PushMessage(pnode, msgMaker.Make(NetMsgType::CMPCTBLOCK, *pcmpctblock));
            state.pindexBestHeaderSent = pindex;
        }
    });
}

/*
 *更新我们的最佳高度，并公布以前没有的任何块哈希
 *与我们的同行保持连锁关系。
 **/

void PeerLogicValidation::UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) {
    const int nNewHeight = pindexNew->nHeight;
    connman->SetBestHeight(nNewHeight);

    SetServiceFlagsIBDCache(!fInitialDownload);
    if (!fInitialDownload) {
//查找以前不在最佳链中的所有块的哈希。
        std::vector<uint256> vHashes;
        const CBlockIndex *pindexToAnnounce = pindexNew;
        while (pindexToAnnounce != pindexFork) {
            vHashes.push_back(pindexToAnnounce->GetBlockHash());
            pindexToAnnounce = pindexToAnnounce->pprev;
            if (vHashes.size() == MAX_BLOCKS_TO_ANNOUNCE) {
//在大规模重组的情况下限制公告。
//在这种情况下，依赖于对等机的同步机制。
                break;
            }
        }
//中继资源清册，但不要在初始块下载期间中继旧资源清册。
        connman->ForEachNode([nNewHeight, &vHashes](CNode* pnode) {
            if (nNewHeight > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : 0)) {
                for (const uint256& hash : reverse_iterate(vHashes)) {
                    pnode->PushBlockHash(hash);
                }
            }
        });
        connman->WakeMessageHandler();
    }

    nTimeBestReceived = GetTime();
}

/*
 *处理无效的块拒绝和随后的对等禁止，维护
 *对等端宣布压缩块。
 **/

void PeerLogicValidation::BlockChecked(const CBlock& block, const CValidationState& state) {
    LOCK(cs_main);

    const uint256 hash(block.GetHash());
    std::map<uint256, std::pair<NodeId, bool>>::iterator it = mapBlockSource.find(hash);

    int nDoS = 0;
    if (state.IsInvalid(nDoS)) {
//不要使用代码0或内部拒绝代码发送拒绝消息。
        if (it != mapBlockSource.end() && State(it->second.first) && state.GetRejectCode() > 0 && state.GetRejectCode() < REJECT_INTERNAL) {
            CBlockReject reject = {(unsigned char)state.GetRejectCode(), state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), hash};
            State(it->second.first)->rejects.push_back(reject);
            if (nDoS > 0 && it->second.second)
                Misbehaving(it->second.first, nDoS);
        }
    }
//检查：
//1。该块有效
//2。我们没有进行初始块下载
//三。这是目前我们所知道的最好的块。我们还没有更新
//提示还没有，所以我们无法直接在这里检查。相反，我们
//只需检查一下，目前飞行中没有其他障碍物。
    else if (state.IsValid() &&
             !IsInitialBlockDownload() &&
             mapBlocksInFlight.count(hash) == mapBlocksInFlight.size()) {
        if (it != mapBlockSource.end()) {
            MaybeSetPeerAsAnnouncingHeaderAndIDs(it->second.first, connman);
        }
    }
    if (it != mapBlockSource.end())
        mapBlockSource.erase(it);
}

//////////////////////////////////////////////////
//
//信息
//


bool static AlreadyHave(const CInv& inv) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    switch (inv.type)
    {
    case MSG_TX:
    case MSG_WITNESS_TX:
        {
            assert(recentRejects);
            if (chainActive.Tip()->GetBlockHash() != hashRecentRejectsChainTip)
            {
//如果链提示更改了以前拒绝的事务
//可能现在有效，例如，由于发送时间变为有效，
//或者双倍消费。重置拒绝过滤器并给出
//第二次机会。
                hashRecentRejectsChainTip = chainActive.Tip()->GetBlockHash();
                recentRejects->reset();
            }

            {
                LOCK(g_cs_orphans);
                if (mapOrphanTransactions.count(inv.hash)) return true;
            }

            return recentRejects->contains(inv.hash) ||
                   mempool.exists(inv.hash) ||
pcoinsTip->HaveCoinInCache(COutPoint(inv.hash, 0)) || //尽最大努力：仅尝试输出0和1
                   pcoinsTip->HaveCoinInCache(COutPoint(inv.hash, 1));
        }
    case MSG_BLOCK:
    case MSG_WITNESS_BLOCK:
        return LookupBlockIndex(inv.hash) != nullptr;
    }
//不知道是什么，就说我们已经有了
    return true;
}

static void RelayTransaction(const CTransaction& tx, CConnman* connman)
{
    CInv inv(MSG_TX, tx.GetHash());
    connman->ForEachNode([&inv](CNode* pnode)
    {
        pnode->PushInventory(inv);
    });
}

static void RelayAddress(const CAddress& addr, bool fReachable, CConnman* connman)
{
unsigned int nRelayNodes = fReachable ? 2 : 1; //有限中继网络外的地址

//中继到有限数量的其他节点
//使用确定性随机性发送到同一节点24小时
//因此所选节点的addrknowns可以防止重复
    uint64_t hashAddr = addr.GetHash();
    const CSipHasher hasher = connman->GetDeterministicRandomizer(RANDOMIZER_ID_ADDRESS_RELAY).Write(hashAddr << 32).Write((GetTime() + hashAddr) / (24*60*60));
    FastRandomContext insecure_rand;

    std::array<std::pair<uint64_t, CNode*>,2> best{{{0, nullptr}, {0, nullptr}}};
    assert(nRelayNodes <= best.size());

    auto sortfunc = [&best, &hasher, nRelayNodes](CNode* pnode) {
        if (pnode->nVersion >= CADDR_TIME_VERSION) {
            uint64_t hashKey = CSipHasher(hasher).Write(pnode->GetId()).Finalize();
            for (unsigned int i = 0; i < nRelayNodes; i++) {
                 if (hashKey > best[i].first) {
                     std::copy(best.begin() + i, best.begin() + nRelayNodes - 1, best.begin() + i + 1);
                     best[i] = std::make_pair(hashKey, pnode);
                     break;
                 }
            }
        }
    };

    auto pushfunc = [&addr, &best, nRelayNodes, &insecure_rand] {
        for (unsigned int i = 0; i < nRelayNodes && best[i].first != 0; i++) {
            best[i].second->PushAddress(addr, insecure_rand);
        }
    };

    connman->ForEachNodeThen(std::move(sortfunc), std::move(pushfunc));
}

void static ProcessGetBlockData(CNode* pfrom, const CChainParams& chainparams, const CInv& inv, CConnman* connman)
{
    bool send = false;
    std::shared_ptr<const CBlock> a_recent_block;
    std::shared_ptr<const CBlockHeaderAndShortTxIDs> a_recent_compact_block;
    bool fWitnessesPresentInARecentCompactBlock;
    const Consensus::Params& consensusParams = chainparams.GetConsensus();
    {
        LOCK(cs_most_recent_block);
        a_recent_block = most_recent_block;
        a_recent_compact_block = most_recent_compact_block;
        fWitnessesPresentInARecentCompactBlock = fWitnessesPresentInMostRecentCompactBlock;
    }

    bool need_activate_chain = false;
    {
        LOCK(cs_main);
        const CBlockIndex* pindex = LookupBlockIndex(inv.hash);
        if (pindex) {
            if (pindex->HaveTxsDownloaded() && !pindex->IsValid(BLOCK_VALID_SCRIPTS) &&
                    pindex->IsValid(BLOCK_VALID_TREE)) {
//如果我们有块和它的所有父块，但还没有验证它，
//我们可能在连接它的中间（即在CS-MAIN的解锁中）
//在activateBestChain之前，但在acceptBlock之后）。
//在这种情况下，我们需要在检查中继之前运行activatebestchain
//条件如下。
                need_activate_chain = true;
            }
        }
} //在调用activatebestchain之前释放cs_main
    if (need_activate_chain) {
        CValidationState state;
        if (!ActivateBestChain(state, Params(), a_recent_block)) {
            LogPrint(BCLog::NET, "failed to activate chain (%s)\n", FormatStateMessage(state));
        }
    }

    LOCK(cs_main);
    const CBlockIndex* pindex = LookupBlockIndex(inv.hash);
    if (pindex) {
        send = BlockRequestAllowed(pindex, consensusParams);
        if (!send) {
            LogPrint(BCLog::NET, "%s: ignoring request from peer=%i for old block that isn't in the main chain\n", __func__, pfrom->GetId());
        }
    }
    const CNetMsgMaker msgMaker(pfrom->GetSendVersion());
//断开节点连接，以防我们已达到为历史块提供服务的出站限制
//从不断开白名单节点
    if (send && connman->OutboundTargetReached(true) && ( ((pindexBestHeader != nullptr) && (pindexBestHeader->GetBlockTime() - pindex->GetBlockTime() > HISTORICAL_BLOCK_AGE)) || inv.type == MSG_FILTERED_BLOCK) && !pfrom->fWhitelisted)
    {
        LogPrint(BCLog::NET, "historical block serving limit reached, disconnect peer=%d\n", pfrom->GetId());

//断开节点
        pfrom->fDisconnect = true;
        send = false;
    }
//通过从不发送低于节点网络限制阈值的块来避免修剪高度泄漏
    if (send && !pfrom->fWhitelisted && (
            /*pfrom->getlocalservices（）&node_network_limited）==node_network_limited）&&（（pfrom->getlocalservices（）&node_network）！=node_network）&&（chainactive.tip（）->nheight-pindex->nheight>（int）node_network_limited_min_blocks+2/*为可能的比赛添加两块缓冲区扩展*/）
       ）{
        logprint（bclog:：net，“忽略节点下的块请求”_network_limited threshold from peer=%d\n“，pfrom->getid（））；

        //断开节点连接并防止其停止（否则将等待丢失的块）
        pfrom->fdisconnect=true；
        发送=假；
    }
    //修剪后的节点可能已删除块，因此请检查
    //在尝试发送之前它是可用的。
    if（发送和（&（pindex->nstatus&block_have_data））
    {
        std:：shared_ptr<const cblock>pblock；
        if（a_recent_block&&a_recent_block->gethash（）==pindex->getblockhash（））
            pblock=最近的块；
        else if（inv.type==msg_witness_block）
            //快速路径：在这种情况下，可以直接从磁盘为块提供服务，
            //因为网络格式与磁盘上的格式匹配
            std:：vector<uint8_t>block_data；
            如果（！）readrawblockfromdisk（block_data，pindex，chainParams.messageStart（））
                断言（！）无法从磁盘加载块”）。
            }
            connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：block，makespan（block_data））；
            //不要设置pblock，因为我们已经发送了块
        }否则{
            //从磁盘发送块
            std:：shared_ptr<cblock>pblockread=std:：make_shared<cblock>（）；
            如果（！）readblockfromdisk（*pblockread，pindex，consensusparams））。
                断言（！）无法从磁盘加载块”）。
            pblock=pblockread；
        }
        如果（pBULL）{
            if（inv.type==msg_块）
                connman->pushmessage（pfrom，msgmaker.make（serialize_transaction_no_-witness，netmsgtype:：block，*pblock））；
            else if（inv.type==msg_witness_block）
                connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：block，*pblock））；
            else if（inv.type==msg_filtered_块）
            {
                bool sendmerkleblock=false；
                梅克布洛克；
                {
                    锁（pfrom->cs_filter）；
                    if（pfrom->pfilter）
                        sendmerkleblock=真；
                        merkleblock=cmerkleblock（*pblock，*pfrom->pfilter）；
                    }
                }
                如果（sendmerkleblock）
                    connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：merkleblock，merkleblock））；
                    //cmerkleblock只包含哈希，因此也会推送客户机未看到的块中的任何事务
                    //这避免了无意义地需要往返而损害性能
                    //请注意，当前节点无法请求我们未在此处发送的任何单个事务-
                    //它们必须断开连接并重试，或者请求完整的块。
                    //因此，指定的协议规范允许我们在这里提供重复的txn，
                    //但是，我们必须始终至少提供远程对等机需要的内容
                    typedef std:：pair<unsigned int，uint256>pairtype；
                    用于（pairtype&pair:merkleblock.vmatchedtxn）
                        connman->pushmessage（pfrom，msgmaker.make（serialize_transaction_no_-witness，netmsgtype:：tx，*pblock->vtx[对.优先]）；
                }
                //其他
                    /无应答
            }
            else if（inv.type==msg_cmpct_块）
            {
                //如果一个同龄人要旧块，我们几乎可以保证
                //他们没有一个有用的内存池来匹配一个紧凑的块，
                //我们不想为它们构造对象，所以
                //相反，我们用完整的、非紧凑的块来响应。
                bool fpeerwantswitness=state（pfrom->getid（））->fwantscmptwitness；
                int nsendflags=fpeerwantswitness？0:序列化事务没有见证；
                if（candirectfetch（consensusparams）&&pindex->nheight>=chainactive.height（）-max_cmptblock_depth）
                    如果（（fpeerwantswitness）！fWitnessPresenteinaRecentCompactBlock）&&a_Recent_Compact_Block&&a_Recent_Compact_Block->Header.getHash（）==pindex->getBlockHash（））；
                        connman->pushmessage（pfrom，msgmaker.make（nsendflags，netmsgtype:：cmpctblock，*a_recent_compact_block））；
                    }否则{
                        cblockheaderandshorttxids cmpctblock（*pblock，fpeerwantswitness）；
                        connman->pushmessage（pfrom，msgmaker.make（nsendflags，netmsgtype:：cmpctblock，cmpctblock））；
                    }
                }否则{
                    connman->pushmessage（pfrom，msgmaker.make（nsendflags，netmsgtype:：block，*pblock））；
                }
            }
        }

        //触发对等节点发送下一批库存的getBlocks请求
        if（inv.hash==pfrom->hashcontinue）
        {
            //绕过pushinventory，即使冗余，也必须发送，
            //我们要在最后一个街区之后，这样他们就不会
            //先等其他东西。
            std:：vector<cinv>vinv；
            vinv.push_back（cinv（msg_block，chainactive.tip（）->getblockhash（））；
            connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：inv，vinv））；
            pfrom->hashContinue.setnull（）；
        }
    }
}

void static processgetdata（cnode*pfrom，const-cchainparams&chainparams，ccannman*connman，const-std:：atomic<bool>和interruptmsgproc）锁被排除（cs_-main）
{
    断言锁定未保持（cs_主）；

    std:：deque<cinv>：：迭代器it=pfrom->vrecvgetdata.begin（）；
    std:：vector<cinv>vnotfound；
    const cnetmsgmaker msgmaker（pfrom->getsendVersion（））；
    {
        锁（CSKEMAN）；

        尽管如此！=pfrom->vrecvgetdata.end（）&&（it->type==msg tx it->type==msg _witness _tx））
            if（中断msgproc）
                返回；
            //如果发送缓冲区太满而无法响应，则不必担心
            如果（pfrom->fpaussend）
                断裂；

            const cinv&inv=*it；
            资讯科技+；

            //从中继内存发送流
            bool push=错误；
            auto mi=maprelay.find（inv.hash）；
            int nsendflags=（inv.type==msg_tx？序列化事务\无见证：0）；
            如果（MI）！=maprelay.end（））
                connman->pushmessage（pfrom，msgmaker.make（nsendflags，netmsgtype:：tx，*mi->second））；
                推=真；
            else if（pfrom->timelastmempoolreq）
                auto txinfo=mempool.info（inv.hash）；
                //为了保护隐私，当
                //无法对Tx进行Inved以响应mempool请求。
                if（txinfo.tx&&txinfo.ntime<=pfrom->timelastmempoolreq）
                    connman->pushmessage（pfrom，msgmaker.make（nsendflags，netmsgtype:：tx，*txinfo.tx））；
                    推=真；
                }
            }
            如果（！）推）{
                找到Vnotfound.push_back（inv）；
            }
        }
    //释放cs_main

    如果（它）！=pfrom->vrecvgetdata.end（）&&！pfrom->fpaussend）
        const cinv&inv=*it；
        if（inv.type==msg_block inv.type==msg_filtered_block inv.type==msg_cmpct_block inv.type==msg_witness_block）
            资讯科技+；
            处理块数据（pfrom、chainparams、inv、connman）；
        }
    }

    pfrom->vrecvgetdata.erase（pfrom->vrecvgetdata.begin（），it）；

    如果（！）vnotfound.empty（））
        //让同伴知道我们没有找到它要的东西，所以它没有
        //必须永远等待。目前只有SPV客户真正关心
        //关于此消息：当它们递归地遍历
        //相关未确认事务的依赖关系。SPV客户希望
        //这样做是因为他们想知道（存储和重播
        //风险分析）与之相关的交易的依赖性，而不
        //必须下载整个内存池。
        connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：notfound，vnotfound））；
    }
}

静态uint32_t getfetchflags（cnode*pfrom）exclusive_locks_required（cs_main）
    uint32_t nfetchFlags=0；
    if（（pfrom->getlocalservices（）&node_-witness）&&state（pfrom->getid（））->fhavewitness）
        nfetchflags=msg_witness_flag；
    }
    返回nfectflags；
}

inline void static sendblocktransactions（const cblock&block，const blocktransactionsrequest&req，cnode*pfrom，cconnman*connman）
    大宗交易响应（REQ）；
    对于（size_t i=0；i<req.indexes.size（）；i++）
        if（请求索引[i]>=block.vtx.size（））
            锁（CSKEMAN）；
            行为不当（pfrom->getid（），100，strprintf（“对等方%d向我们发送了一个带有超出界限的tx索引的getblocktxn”，pfrom->getid（））；
            返回；
        }
        resp.txn[i]=block.vtx[req.indexes[i]]；
    }
    锁（CSKEMAN）；
    const cnetmsgmaker msgmaker（pfrom->getsendVersion（））；
    int nsendflags=state（pfrom->getid（））->fwantscmpcctwitness？0:序列化事务没有见证；
    connman->pushmessage（pfrom，msgmaker.make（nsendflags，netmsgtype:：blocktxn，resp））；
}

bool static processHeadersMessage（cnode*pfrom，cconnman*connman，const std:：vector<cblockheader>&headers，const cchainparams&chainparams，bool preamine_duplicate_invalid）
{
    const cnetmsgmaker msgmaker（pfrom->getsendVersion（））；
    size_t ncount=headers.size（）；

    如果（ncount==0）
        //没什么有趣的。停止向此对等服务器请求更多的头。
        回归真实；
    }

    bool received_new_header=false；
    const cblockindex*pindexlast=nullptr；
    {
        锁（CSKEMAN）；
        cnodeState*nodestate=state（pfrom->getid（））；

        //如果这看起来像是块通知（ncount<
        //max_blocks_to_announce），使用特殊逻辑处理
        //不连接：
        //-发送getheaders消息以响应尝试连接链。
        //-对等端最多可以在一行中发送未连接的最大标头
        //在给出DOS点之前不要连接
        //-一旦收到有效且确实连接的头消息，
        //unconnectingHeaders被重置回0。
        如果（！）lookupblockindex（headers[0].hashprevblock）&&ncount<max_blocks_to_announced）
            nodestate->unconnectingheaders++；
            connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：getheaders，chainActive.getLocator（pindexBestHeader），uint256（））；
            logprint（bclog:：net，“接收到头%s:缺少前一个块%s，正在将getheaders（%d）发送到末尾（peer=%d，unconnectingheaders=%d）\n”，
                    头[0].GetHash（）.ToString（），
                    头[0].HashPrevBlock.ToString（），
                    PindexBestHeader->nheight，
                    pfrom->getid（），nodestate->unconnectingheaders）；
            //为此对等机设置hashLastUnknownBlock，以便
            //最终获取头-即使来自不同的对等端-
            //我们可以使用此对等机进行下载。
            updateBlockAvailability（pfrom->getID（），headers.back（）.getHash（））；

            if（nodestate->unconnecting headers%max_unconnecting_headers==0）
                行为不当（pfrom->getid（），20）；
            }
            回归真实；
        }

        uint256 hashlastblock；
        用于（const cblockheader&header:headers）
            如果（！）hashLastBlock.isNull（）&&header.hashPrevBlock！=hashLastBlock）
                行为错误（pfrom->getid（），20，“非连续头序列”）；
                返回错误；
            }
            hashLastBlock=header.getHash（）；
        }

        //如果我们没有最后一个标题，他们就会给我们
        //新内容（如果这些头有效）。
        如果（！）lookupblockindex（hashlastblock））
            接收的_new_header=true；
        }
    }

    cvalidationState状态；
    cblockheader-first-invalid-header；
    如果（！）processNewBlockHeaders（headers，state，chainParams，&pindextlast，&first_invalid_header））
        INDOS；
        if（state.isinvalid（ndos））
            锁（CSKEMAN）；
            如果（NDOS＞0）{
                行为错误（pfrom->getid（），ndos，“接收到无效的头”）；
            }否则{
                logprint（bclog:：net，“peer=%d:接收到无效的头文件\n”，pfrom->getid（））；
            }
            if（惩罚_duplicate_invalid&&lookupBlockIndex（first_invalid_header.gethash（））
                //目标：不允许出站对等方使用出站对等方
                //连接槽（如果它们位于不兼容的链上）。
                / /
                //我们要求调用者根据
                //关于对等端和头传递方法（压缩
                //某些情况下允许块无效，
                //在BIP 152下）。
                //在这里，我们试图检测出
                //有效的块头（即它在头
                //已接收，因此存储在mapblockindex中），但知道
                //块无效，并且某个对等方已声明
                //在其活动链上阻塞。
                //在这种情况下断开对等机的连接。
                / /
                //注意：如果无效的头未被我们的
                //mapblockindex，这也可能是
                //断开对等机的连接，因为它们所在的链很可能
                //不兼容。但是，有一种情况
                //不保留：如果头的时间戳大于
                //比当前时间提前2小时。在这种情况下，头部
                //将来可能会生效，我们不想
                //断开一个对等机的连接，仅仅是因为它为我们提供的服务太远了。
                //阻止头，以防止攻击者拆分
                //通过在2小时边界处挖掘块来建立网络。
                / /
                //TODO:更新DOS逻辑（或者，更确切地说，重写
                //验证和网络处理之间的DOS接口），以便
                //接口更干净，因此我们断开
                //对等端的头链不兼容的原因
                //使用我们的（例如block->nversion softforks，mtp违规，
                //etc），而不仅仅是重复的无效案例。
                pfrom->fdisconnect=true；
            }
            返回错误；
        }
    }

    {
        锁（CSKEMAN）；
        cnodeState*nodestate=state（pfrom->getid（））；
        if（nodstate->unconnectingheaders>0）
            logprint（bclog:：net，“peer=%d:重置unconnectingheaders（%d->0）\n”，pfrom->getid（），nodestate->unconnectingheaders）；
        }
        nodestate->unconnectingheaders=0；

        断言（pindextlast）；
        updateBlockAvailability（pfrom->getID（），pindexLast->getBlockHash（））；

        //在此，pindexBestKnownBlock应保证不为空，
        //因为它是在updateBlockAvailability中设置的。一些nullptr检查
        //仍然存在，但是，作为腰带和吊带。

        if（接收到_new_header&&pindexlast->nchainwork>chainactive.tip（）->nchainwork）
            nodestate->m_last_block_announcement=gettime（）；
        }

        if（ncount==max_headers_results）
            //头消息的大小已达到最大值；对等端可能有更多的头。
            //TODO:优化：如果pindextlast是chainactive.tip或pindexbestheader的祖先，请继续
            从那里开始。
            logprint（bclog:：net，“更多getheaders（%d）到端对端的值为%d（startheight:%d）\n”，pindextlast->nheight，pfrom->getid（），pfrom->instartingheight）；
            connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：getheaders，chainactive.getlocator（pindextlast），uint256（））；
        }

        bool fcandirectfetch=candirectfetch（chainparams.getconsensus（））；
        //如果这组头有效，并且以至少为
        //尽可能多的工作，尽可能多的下载。
        if（fcandirectfetch&&pindextlast->isvalid（block_valid_tree）&&chainactive.tip（）->nchainwork<=pindextlast->nchainwork）
            std:：vector<const cblockindex*>vtofetch；
            const cblockindex*pindexwalk=pindexlast；
            //计算所有需要切换到pindexlast的块，直到达到极限。
            同时（Pindexwalk和&！chainActive.contains（pindexwalk）&&vtofetch.size（）<=max_blocks_in_transit_per_peer）
                如果（！）（pindexwalk->nstatus&block_have_data）&&
                        ！mapblocksinflight.count（pindexwalk->getblockhash（））和&
                        （！iswitnessEnabled（pindexwalk->pprev，chainParams.getconsensus（））state（pfrom->getid（））->fhavewitness））
                    //我们没有这个街区，而且还没有起飞。
                    vtofetch.推回（pindexwalk）；
                }
                pindexwalk=pindexwalk->pprev；
            }
            //如果Pindexwalk仍然不在我们的主链上，我们将看到
            //在我们认为即将赶上的时候，进行了非常大的重组
            //主链——这不应该真的发生。保释
            //直接获取并依赖并行下载。
            如果（！）chainactive.contains（pindexwalk））
                logprint（bclog:：net，“大REORG，不会直接提取到%s（%d）”，
                        pindextlast->getBlockHash（）.toString（），
                        Pindextlast->nheight）；
            }否则{
                std:：vector<cinv>vgetdata；
                //尽可能多地从最早到最晚下载。
                for（const cblockindex*pindex:反向迭代（vtofetch））
                    if（nodestate->nblocksinflight>=max_blocks_in_transit_per_peer）
                        //无法从此对等计算机下载更多内容
                        断裂；
                    }
                    uint32_t nfetchflags=getFetchFlags（pfrom）；
                    vgetdata.push_back（cinv（msg_block_nfetchflags，pindex->getblockhash（））；
                    markblockasinflight（pfrom->getid（），pindex->getblockhash（），pindex）；
                    logprint（bclog:：net，“从对等方请求块%s=%d\n”，
                            pindex->getBlockHash（）.toString（），pfrom->getid（））；
                }
                if（vgetdata.size（）>1）
                    logprint（bclog:：net，“通过头直接获取向%s（%d）下载块\n”，
                            pindextlast->getBlockHash（）.toString（），pindextlast->nheight）；
                }
                if（vgetdata.size（）>0）
                    if（nodestate->fsupportsdesiredcmpctversion&&vgetdata.size（）==1&&mapblocksinflight.size（）==1&&pindextlast->pprev->isvalid（block_valid_chain））
                        //在任何情况下，我们都希望使用压缩块而不是常规块进行下载
                        vgetdata[0]=cinv（msg_cmpct_block，vgetdata[0].哈希）；
                    }
                    connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：getdata，vgetdata））；
                }
            }
        }
        //如果我们在IBD，我们希望出站对等机能够为我们提供有用的
        / /链。断开工作不足的链上的对等机。
        如果（isitialBlockDownload（）&&ncount！）=max_headers_results）
            //当ncount<max_headers_results时，我们知道我们没有更多
            //要从此对等机获取的头。
            if（nodestate->pindexbestknownblock&nodestate->pindexbestknownblock->nchainwork<nminiumchainwork）
                //此对等机在其头链上的工作太少，无法提供帮助
                //US SYNC--如果使用出站插槽，则断开连接（除非
                //白名单或addnode）。
                //注意：我们将它们的尖端与nminiumchainwork（而不是
                //chainActive.tip（））因为我们不会开始块下载
                //直到我们有一个headers链
                //nminiumchainwork，即使一个对等机有一条链经过我们的提示，
                //作为一种防DoS措施。
                if（isoutbounddisconnectionandidate（pfrom））
                    logprintf（“正在断开出站对等端%d的连接--头链的工作不足\n”，pfrom->getid（））；
                    pfrom->fdisconnect=true；
                }
            }
        }

        如果（！）pfrom->fdisconnect&isoutbounddisconnectionandidate（pfrom）&&nodestate->pindexbestknownblock！= null pTr）{
            //如果这是出站对等机，请检查是否应保护
            //来自坏/滞后链逻辑。
            如果（g_outbound_peers_with_protect_from_disconnect<max_outbound_peers_to_protect_from_disconnect&&nodestate->pindexBestKnownBlock->nchainWork>=chainActive.tip（）->nchainWork&！nodestate->m_chain_sync.m_protect）_
                logprint（bclog:：net，“保护出站对等机%d不被逐出\n”，pfrom->getid（））；
                nodestate->m_chain_sync.m_protect=true；
                +G_出站_对等机与_保护_不受_断开连接；
            }
        }
    }

    回归真实；
}

bool static processmessage（cnode*pfrom，const std:：string&strcommand，cdatastream&vrecv，int64-ntimereceived，const cchainparams&chainparams，ccannman*connman，const std:：atomical<bool>&interruptmsgproc，bool enable_bip61）
{
    logprint（bclog:：net，“已接收：%s（%u字节）对等方=%d\n”，sanitiestring（strcommand），vrecv.size（），pfrom->getid（））；
    if（gargs.isargset（“-dropmessagestest”）&&getrand（gargs.getarg（“-dropmessagestest”，0））==0）
    {
        logprintf（“dropmessagestest dropping recv message \n”）；
        回归真实；
    }


    如果（！）（pfrom->getlocalservices（）&node_bloom）&&
              （strcommand==netmsgtype:：filterload
               strcommand==netmsgtype:：filteradd）
    {
        if（pfrom->nversion>=no_bloom_version）
            锁（CSKEMAN）；
            行为不当（pfrom->getid（），100）；
            返回错误；
        }否则{
            pfrom->fdisconnect=true；
            返回错误；
        }
    }

    if（strcommand==netmsgtype:：reject）
    {
        if（logAcceptCategory（bclog:：net））
            尝试{
                std:：string strmsg；无符号字符ccode；std:：string strreason；
                vrecv>>受限_字符串（strmsg，cmessageheader:：command_size）>>ccode>>受限_字符串（strreason，max_reject_message_length）；

                标准：ostringstream ss；
                ss<<strmsg<“code”<<itostr（ccode）<<“：”<<strReason；

                if（strmsg==netmsgtype:：block strmsg==netmsgtype:：tx）
                {
                    uTIN 256散列；
                    散列；
                    ss<“：hash”<<hash.toString（）；
                }
                logprint（bclog:：net，“拒绝%s\n”，sanitiestring（s s.str（））；
            catch（const std:：ios_base:：failure&）
                //通过防止拒绝消息触发新的拒绝消息来避免反馈循环。
                logprint（bclog:：net，“收到无法解析的拒绝消息”）；
            }
        }
        回归真实；
    }

    if（strcommand==netmsgtype:：version）
        //每个连接只能发送一条版本消息
        如果（pfrom->nversion！= 0）
        {
            如果（启用_bip61）
                connman->pushmessage（pfrom，cnetmsgmaker（init_proto_version）.make（netmsgtype:：reject，strcommand，reject_duplicate，std:：string（“复制版本消息”））；
            }
            锁（CSKEMAN）；
            行为不当（pfrom->getid（），1）；
            返回错误；
        }

        国际标准时间；
        球童装饰品；
        caddress addrfrom；
        uint64节点=1；
        uint64服务；
        服务标志服务；
        int版本；
        int-nsendversion；
        std：：字符串strsubser；
        std:：string cleansubser；
        int-instartingheight=-1；
        bool frelay=真；

        VRECV>>Nversion>>NserviceInt>>NTIME>>addrme；
        nsendversion=std:：min（nversion，协议_版本）；
        nServices=服务标志（nServiceInt）；
        如果（！）pfrom->finbound）
        {
            connman->setservices（pfrom->addr，services）；
        }
        如果（！）pfrom->finbound&！pfrom->ffeeler&！pfrom->m_手动连接&！HasAllDesireableServiceFlags（服务）
        {
            logprint（bclog:：net，“peer=%d”不提供预期的服务（提供了%08X，应提供%08X）；正在断开连接\n“，pfrom->getid（），services，getDesireableServiceFlags（services））；
            如果（启用_bip61）
                connman->pushmessage（pfrom，cnetmsgmaker（init_proto_version）.make（netmsgtype:：reject，strcommand，reject_nonstandard，
                                   strprintf（“应提供服务%08X”，GetDesireableServiceFlags（Services）））；
            }
            pfrom->fdisconnect=true；
            返回错误；
        }

        if（nversion<min_peer_proto_version）
            //断开与早于此协议版本的对等机的连接
            logprint（bclog:：net，“使用过时版本%i的peer=%d；正在断开连接\n”，pfrom->getid（），n version）；
            如果（启用_bip61）
                connman->pushmessage（pfrom，cnetmsgmaker（init_proto_version）.make（netmsgtype:：reject，strcommand，reject_obsolete，
                                   strprintf（“版本必须是%d或更高”，min_peer_proto_version））；
            }
            pfrom->fdisconnect=true；
            返回错误；
        }

        如果（！）VCREV.EMPTY（）
            VRECV>>添加自>>无；
        如果（！）vrecv.empty（））
            VRECV>>有限的_字符串（strSubfer，最大_Subversion_长度）；
            CleanSubfer=消毒发情（strSubfer）；
        }
        如果（！）vrecv.empty（））
            VRECV>>安装高度；
        }
        如果（！）VCREV.EMPTY（）
            VRECV>>弗莱；
        //如果我们连接到自己，则断开连接
        如果（pfrom->finbound&！connman->checkincomingnonce（无）
        {
            logprintf（“在%s连接到self，断开连接”，pfrom->addr.toString（））；
            pfrom->fdisconnect=true；
            回归真实；
        }

        if（pfrom->finbound&addrme.isroutable（））
        {
            见本地（addrme）；
        }

        //害羞，在我们听到之前不要发送版本
        if（pfrom->finbound）
            pushnodeversion（pfrom、connman、getAdjustedTime（））；

        connman->pushmessage（pfrom，cnetmsgmaker（init_proto_version）.make（netmsgtype:：verack））；

        pfrom->nservices=服务；
        pfrom->setaddrlocal（addrme）；
        {
            锁定（pfrom->cs_subser）；
            pfrom->strSubfer=strSubfer；
            pfrom->cleansubser=cleansubser；
        }
        pfrom->nstartingheight=nstartingheight；

        //将不中继块和Tx以及不作为历史区块链（部分）的节点设置为“客户端”
        pfrom->fclient=！（服务和节点网络）&&！（服务和节点网络有限公司）；

        //将不能服务完整区块链历史的节点设置为“有限节点”
        pfrom->m_limited_node=！（服务和节点网络）&&（服务和节点网络有限公司））；

        {
            锁（pfrom->cs_filter）；
            pfrom->frelaytxes=frelay；//获取第一个filter*消息后设置为true
        }

        //更改版本
        pfrom->setsendversion（nsendversion）；
        pfrom->nversion=nversion；

        if（（服务和节点见证）
        {
            锁（CSKEMAN）；
            状态（pfrom->getid（））->fhavewistence=true；
        }

        //可能将此对等机标记为首选下载对等机。
        {
        锁（CSKEMAN）；
        updatePreferredDownload（pfrom，state（pfrom->getid（））；
        }

        如果（！）pfrom->finbound）
        {
            //公布我们的地址
            如果（flisten&&！isInitialBlockDownload（））
            {
                caddress addr=getlocaladdress（&pfrom->addr，pfrom->getlocalservices（））；
                快速随机上下文不安全；
                if（addr.isroutable（））
                {
                    logprint（bclog:：net，“processMessages:广告地址%s\n”，addr.toString（））；
                    pfrom->pushaddress（地址，不安全地址）；
                否则，如果（ispeeraddrlocalgood（pfrom））
                    地址setip（addrme）；
                    logprint（bclog:：net，“processMessages:广告地址%s\n”，addr.toString（））；
                    pfrom->pushaddress（地址，不安全地址）；
                }
            }

            //获取最近的地址
            if（pfrom->foneshot pfrom->nversion>=caddr connman->getAddressCount（）<1000）
            {
                connman->pushmessage（pfrom，cnetmsgmaker（nsendversion）.make（netmsgtype:：getaddr））；
                pfrom->fgetaddr=true；
            }
            connman->markaddressgood（pfrom->addr）；
        }

        std：：字符串remoteaddr；
        如果（FLUGIPS）
            remoteaddr=“，peeraddr=”+pfrom->addr.toString（）；

        logprint（bclog:：net，“接收版本消息：%s:版本%d，块=%d，us=%s，对等方=%d%s\n”，
                  cleansubfer，pfrom->nversion，清除子容器，
                  pfrom->instartingheight，addrme.toString（），pfrom->getid（），
                  遥控器；

        int64_t ntimeoffset=ntime-gettime（）；
        pfrom->ntimeoffset=ntimeoffset；
        addtimedata（pfrom->addr，ntimeoffset）；

        //如果对等机足够旧，可以使用旧警报系统，则向其发送最终警报。
        if（pfrom->nversion<=70012）
            cdatastream finalalert（parsehex）（parsehex（“60010000000000000000000000000000000000ffff7f000000000000000000000000000000ffff7f000000000000000ffffff7ffffff7ffffff7f1ffffff7f0ffff7f000ffff7ff000ffff7f02f555247454e543A220416C6574206262626579206F6F6D70726F6666D69797365642C27570677261646520726571756972656564063044022069726563630220220220653FBD6410F470F66BE11CAD11CAD114141414141414141414141414141414141414141414141414141414141414e00da94cad0f型ecaae66ecf689bf71b50”），服务器网络，协议版本；
            connman->pushmessage（pfrom，cnetmsgmaker（nsendversion）.make（“alert”，finalalert））；
        }

        //触角连接仅用于验证地址是否联机。
        如果（pfrom->ffeeler）
            断言（pfrom->finbound==false）；
            pfrom->fdisconnect=true；
        }
        回归真实；
    }

    if（pfrom->nversion==0）
        //必须先有版本消息，再有其他消息
        锁（CSKEMAN）；
        行为不当（pfrom->getid（），1）；
        返回错误；
    }

    //此时，传出消息序列化版本无法更改。
    const cnetmsgmaker msgmaker（pfrom->getsendVersion（））；

    if（strcommand==netmsgtype:：verack）
    {
        pfrom->setrecvversion（std:：min（pfrom->nversion.load（），协议_版本））；

        如果（！）pfrom->finbound）
            //将此节点标记为当前连接，因此稍后更新其时间戳。
            锁（CSKEMAN）；
            状态（pfrom->getid（））->fcurrentlyconnected=true；
            logprintf（“新出站对等连接：版本：%d，块=%d，对等连接=%d%s\n”，
                      pfrom->nversion.load（），pfrom->instartingheight，pfrom->getid（），
                      （弗洛吉普斯？）strprintf（“，peeraddr=%s”，pfrom->addr.toString（））：“”）；
        }

        if（pfrom->nversion>=sendHeaders_version）
            //告诉我们的同伴我们更喜欢接收头而不是inv
            //我们也将此消息发送给非节点网络对等端，因为
            //非节点网络对等方可以通知块（如修剪
            / /节点）
            connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：sendHeaders））；
        }
        if（pfrom->nversion>=short_id_blocks_version）
            //告诉我们的同行我们愿意提供版本1或2 cmpctblocks
            //但是，我们不使用
            //cmpctblock消息。
            //我们也将此消息发送给非节点网络对等端，因为
            //他们可能希望向我们请求压缩块
            bool fannounceusingcmpctblock=false；
            uint64_t ncmpctblockversion=2；
            if（pfrom->getlocalservices（）&node_-witness）
                connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：sendcmpct，fannounceusingcmpctblock，ncmpctblockversion））；
            ncmpctblockversion=1；
            connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：sendcmpct，fannounceusingcmpctblock，ncmpctblockversion））；
        }
        pfrom->fsuaccessfullyconnected=true；
        回归真实；
    }

    如果（！）pfrom->fsuaccessfullyconnected）
        //必须有verack消息才能执行其他操作
        锁（CSKEMAN）；
        行为不当（pfrom->getid（），1）；
        返回错误；
    }

    if（strcommand==netmsgtype:：addr）
        std:：vector<caddress>vaddr；
        VRCDV＞VADDR；

        //不需要旧版本的addr，除非种子设定
        if（pfrom->nversion<caddr_time_version&&connman->getAddressCount（）>1000）
            回归真实；
        如果（varddr.size（）>1000）
        {
            锁（CSKEMAN）；
            行为错误（pfrom->getid（），20，strprintf（“message addr size（）=%u”，vaddr.size（））；
            返回错误；
        }

        //存储新地址
        std:：vector<caddress>vaddrok；
        int64_t nnow=getAdjustedTime（）；
        Int64宽度=无-10*60；
        用于（caddress&addr:vaddr）
        {
            if（中断msgproc）
                回归真实；

            //我们只需要存储完整的节点，尽管这可能包括
            //我们无法建立出站连接的对象，在
            //部分原因是我们可以与它们建立触角连接。
            如果（！）可能有有用的地址数据库（地址服务）&！HasAllDesireableServiceFlags（地址服务）
                继续；

            if（addr.ntime<=100000000 addr.ntime>nNow+10*60）
                地址：ntime=nnow-5*24*60*60；
            pfrom->addaddressknown（addr）；
            bool freachable=isreachable（地址）；
            如果（addr.ntime>nsence&！pfrom->fgetaddr&&vaddr.size（）<=10&&addr.isroutable（））
            {
                //中继到有限数量的其他节点
                中继地址（addr，freachable，connman）；
            }
            //不要在我们的网络之外存储地址
            如果（可恢复）
                回传（addr）；
        }
        connman->addnewaddresses（vaddrok，pfrom->addr，2*60*60）；
        if（varddr.size（）<1000）
            pfrom->fgetaddr=false；
        如果（pfrom->foneshot）
            pfrom->fdisconnect=true；
        回归真实；
    }

    if（strcommand==netmsgtype:：sendHeaders）
        锁（CSKEMAN）；
        状态（pfrom->getid（））->fpreferheaders=true；
        回归真实；
    }

    if（strcommand==netmsgtype:：sendcmpct）
        bool fannounceusingcmpctblock=false；
        uint64_t ncmpctblockversion=0；
        VRECV>>使用MPCTBLOCK>>NCMPCTBLOCK版本的风扇盎司；
        if（ncmpctblockversion==1（（pfrom->getlocalservices（）&node_witness）&&ncmpctblockversion==2））
            锁（CSKEMAN）；
            //fprovidesHeaderAndIs用于“锁定”我们发送的压缩块版本（fwantscmpctwitness）
            如果（！）状态（pfrom->getid（））->fprovidesHeaderAndIds）
                状态（pfrom->getid（））->fprovidesHeaderAndIs=true；
                状态（pfrom->getid（））->fwantscmpcctwitness=ncmpctblockversion==2；
            }
            if（state（pfrom->getid（））->fwantscmpcctwitness==（ncmpctblockversion==2））//忽略更高版本的公告
                状态（pfrom->getid（））->fprefereheaderandis=fannounceusingcmpctblock；
            如果（！）状态（pfrom->getid（））->fsupportsdesiredcmpctversion）
                if（pfrom->getlocalservices（）&node_-witness）
                    状态（pfrom->getid（））->fsupportsDesiredCmpctVersion=（ncmpctBlockVersion==2）；
                其他的
                    状态（pfrom->getid（））->fsupportsDesiredCmpctVersion=（ncmpctBlockVersion==1）；
            }
        }
        回归真实；
    }

    if（strcommand==netmsgtype:：inv）
        std:：vector<cinv>vinv；
        VRVV＞VIV；
        if（vinv.size（）>最大投资额）
        {
            锁（CSKEMAN）；
            行为错误（pfrom->getid（），20，strprintf（“message inv size（）=%u”，vinv.size（））；
            返回错误；
        }

        bool fblocksonly=！FelyTrxes；

        //如果WhiteListRelay为true，则允许白名单对等端以仅块模式发送块以外的数据
        if（pfrom->fHitelisted和gargs.getBoolarg（“-WhiteListRelay”，默认为“whiteListRelay”）。
            fBlocksOnly=假；

        锁（CSKEMAN）；

        uint32_t nfetchflags=getFetchFlags（pfrom）；

        用于（Cinv和Inv:Vinv）
        {
            if（中断msgproc）
                回归真实；

            bool falreadyhave=alreadyhave（inv）；
            logprint（bclog:：net，“获取的inv:s%s对等方=%d\n”，inv.toString（），falreadyhave？”有“：”new“，pfrom->getid（））；

            如果（inv.type==msg_tx）
                inv.type=nfetchFlags；
            }

            if（inv.type==msg_block）
                updateBlockAvailability（pfrom->getid（），inv.hash）；
                如果（！）准备好了&！F导入和&！弗莱克茅斯& &！mapblocksinflight.count（inv.hash））
                    //我们以前在这里请求完整的块，但是由于headers公告现在是
                    //在网络上发布的主要方法，如果节点
                    //回到INV，我们可能有一个REORG，我们应该先得到头部，
                    //我们现在只提供getheaders响应。当我们收到邮件头时，我们会
                    //然后请求我们需要的块。
                    connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：getheaders，chainactive.getlocator（pindexbestheader），inv.hash））；
                    logprint（bclog:：net，“getheaders（%d）%s to peer=%d\n”，pindexbestheader->nheight，inv.hash.toString（），pfrom->getid（））；
                }
            }
            其他的
            {
                pfrom->addinventoryknown（inv）；
                如果（FBlocksonly）
                    logprint（bclog:：net，“违反协议发送的事务（%s）inv peer=%d\n”，inv.hash.toString（），pfrom->getid（））；
                否则，（如果）！准备好了&！F导入和&！弗莱克茅斯& &！isitialBlockDownload（））
                    pfrom->askfor（inv）；
                }
            }
        }
        回归真实；
    }

    if（strcommand==netmsgtype:：getdata）
        std:：vector<cinv>vinv；
        VRVV＞VIV；
        if（vinv.size（）>最大投资额）
        {
            锁（CSKEMAN）；
            行为错误（pfrom->getid（），20，strprintf（“message getdata size（）=%u”，vinv.size（））；
            返回错误；
        }

        logprint（bclog:：net，“接收到的getdata（%u invsz）peer=%d\n”，vinv.size（），pfrom->getid（））；

        如果（vinv.size（）>0）
            logprint（bclog:：net，“接收到的getdata为：%s peer=%d\n”，vinv[0].toString（），pfrom->getid（））；
        }

        pfrom->vrecvgetdata.insert（pfrom->vrecvgetdata.end（），vinv.begin（），vinv.end（））；
        processgetdata（pfrom、chainparams、connman、interruptmsgproc）；
        回归真实；
    }

    if（strcommand==netmsgtype:：getBlocks）
        CBlocklocator定位器；
        uint256哈希停止；
        VRECV>>定位器>>hashstop；

        if（locator.vhave.size（）>max_locator_
            logprint（bclog:：net，“getblocks locator size%lld>>%d，disconnect peer=%d\n”，locator.vhave.size（），max_locator_sz，pfrom->getid（））；
            pfrom->fdisconnect=true；
            回归真实；
        }

        //我们可能已经使用
        //压缩块，导致对等机发送getBlocks
        //请求，否则我们将在没有新块的情况下响应该请求。
        //为了避免这种情况，我们只需验证我们是否尽了最大努力
        //已知链。这是超致命的，但我们处理得更好
        //对于getheaders请求，并且没有支持的已知节点
        //压缩块，但仍使用GetBlocks请求块。
        {
            std:：shared_ptr<const cblock>a_recent_block；
            {
                锁（cs_-most_-recent_-block）；
                a_recent_block=最近的_block；
            }
            cvalidationState状态；
            如果（！）activateBestChain（state，params（），a_recent_block））
                logprint（bclog:：net，“未能激活链（%s）\n”，formatStateMessage（state））；
            }
        }

        锁（CSKEMAN）；

        //查找主链中调用方的最后一个块
        const cblockindex*pindex=findWorkingLobalIndex（chainActive，定位器）；

        //发送链的其余部分
        如果（pHead）
            pindex=chainactive.next（pindex）；
        int nlimit=500；
        logprint（bclog:：net，“GetBlocks%d到%s，限制为%d，来自对等方=%d\n”，（pindex？pindex->nheight:-1），hashstop.isNull（）？“end“：hashstop.toString（），nlimit，pfrom->getid（））；
        对于（；pindex；pindex=chainactive.next（pindex））
        {
            if（pindex->getBlockHash（）==hashStop）
            {
                logprint（bclog:：net，“GetBlocks在%d%s停止”，pindex->nheight，pindex->getBlockHash（）.toString（））；
                断裂；
            }
            //如果修剪，除非磁盘上有块，而且可能仍然有块，否则不要反转块
            //对于块中继可能需要的合理时间窗口（1小时）。
            const int npunedblockslikelytohave=min_blocks_to_keep-3600/chainparams.getconsensus（）.npowTargetSpacking；
            如果（fprunemode&&（！（pindex->nstatus&block_have_data）pindex->nheight<=chainactive.tip（）->nheight-npunedblockslikelytohave））
            {
                logprint（bclog:：net，“GetBlocks在%d%s处停止、修剪或太旧的块”，pindex->nheight，pindex->getBlockHash（）.toString（））；
                断裂；
            }
            pfrom->pushinventory（cinv（msg_block，pindex->getblockhash（））；
            if（--nlimit<=0）
            {
                //当请求此块时，我们将发送一个将
                //触发getBlocks的对等方下一批库存。
                logprint（bclog:：net，“getblocks stopping at limit%d%s\n”，pindex->nheight，pindex->getblockhash（）.toString（））；
                pfrom->hashContinue=pindex->getBlockHash（）；
                断裂；
            }
        }
        回归真实；
    }

    if（strcommand==netmsgtype:：getblocktxn）
        BlockTransactions请求请求请求；
        VRCV＞Req；

        std:：shared_ptr<const cblock>recent_block；
        {
            锁（cs_-most_-recent_-block）；
            if（最近的块哈希==req.block哈希）
                最近的块=最近的块；
            //解锁cs_most_recent_块以避免cs_主锁反转
        }
        如果（最近的_块）
            sendblocktransactions（*recent_k，req，pfrom，connman）；
            回归真实；
        }

        锁（CSKEMAN）；

        const cblockindex*pindex=lookupblockindex（req.blockhash）；
        如果（！）pQueY*！（pindex->nstatus&block_have_data））
            logprint（bclog:：net，“对等方%d向我们发送了一个getblocktxn，用于没有的块”，pfrom->getid（））；
            回归真实；
        }

        if（pindex->nheight<chainactive.height（）-最大块x深度）
            //如果请求旧的块（在实践中不应发生，
            //但可能发生在测试中）发送块响应而不是
            //blocktxn响应。发送完整的块响应而不是
            //如果对等机
            //可能恶意发送大量getblocktxn请求来触发
            //昂贵的磁盘读取，因为它需要
            //实际通过网络接收从磁盘读取的所有数据。
            logprint（bclog:：net，“对等方%d向我们发送了一个getblocktxn，用于一个块>%i deep\n”，pfrom->getid（），max_blocktxn_depth）；
            CVN-IV；
            inv.type=state（pfrom->getid（））->fwantscmptwitness？味精见证块：味精块；
            inv.hash=req.blockhash；
            pfrom->vrecvgetdata.push_back（inv）；
            //消息处理循环将再次循环（不暂停），然后我们将响应（不带cs_main）
            回归真实；
        }

        块块；
        bool ret=readblockfromdisk（block，pindex，chainparams.getconsensus（））；
        断言（RET）；

        发送块事务（块、请求、pfrom、connman）；
        回归真实；
    }

    if（strcommand==netmsgtype:：getheaders）
        CBlocklocator定位器；
        uint256哈希停止；
        VRECV>>定位器>>hashstop；

        if（locator.vhave.size（）>max_locator_
            logprint（bclog:：net，“getheaders locator size%lld>>%d，disconnect peer=%d\n”，locator.vhave.size（），max_locator_sz，pfrom->getid（））；
            pfrom->fdisconnect=true；
            回归真实；
        }

        锁（CSKEMAN）；
        如果（isitialBlockDownload（）&&！pfrom->fwhetelisted）
            logprint（bclog:：net，“忽略对等方的getheaders=%d，因为节点处于初始块下载中”，pfrom->getid（））；
            回归真实；
        }

        cnodeState*nodestate=state（pfrom->getid（））；
        常量cblockindex*pindex=nullptr；
        if（locator.isNull（））
        {
            //如果locator为空，则返回hashstop块
            pindex=查找块索引（hashstop）；
            如果（！）pQueD）{
                回归真实；
            }

            如果（！）blockrequestallowed（pindex，chainparams.getconsensus（））
                logprint（bclog:：net，“%s:忽略对不在主链中的旧块头的peer=%i的请求\n”，
                回归真实；
            }
        }
        其他的
        {
            //查找主链中调用方的最后一个块
            pindex=findWorkingLobalIndex（chainActive，定位器）；
            如果（pHead）
                pindex=chainactive.next（pindex）；
        }

        //我们必须使用cblocks，因为cblockheaders在末尾不包含0x00 ntx计数
        std:：vector<cblock>vheaders；
        int nlimit=max_headers_results；
        logprint（bclog:：net，“getheaders%d to%s from peer=%d\n”，（pindex？pindex->nheight:-1），hashstop.isNull（）？“end“：hashstop.toString（），pfrom->getid（））；
        对于（；pindex；pindex=chainactive.next（pindex））
        {
            vheaders.push_back（pindex->getBlockHeader（））；
            if（--nlimit<=0 pindex->getBlockHash（）==hashStop）
                断裂；
        }
        //如果我们发送chainactive.tip（）或
        //如果我们的对等机有chainActive.tip（）（因此我们发送一个空的
        //头消息）。在这两种情况下，更新都是安全的
        //pindexbestheadersent是我们的小费。
        / /
        //重要的是我们只需在这里重置bestheadersent值，
        //而不是max（bestheadersent，newheadersent）。我们可能已经宣布了
        //当前正在连接的提示使用压缩块，其中
        //导致对等端发送了一个头请求，我们对此进行了响应
        //没有新块。通过重置bestheadersent，我们确保
        //将通过标题（或再次压缩块）重新宣布新块
        //在sendmessages逻辑中。
        nodestate->pindexbestheadersent=pindex？pindex:chainactive.tip（）；
        connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：headers，vheaders））；
        回归真实；
    }

    if（strcommand==netmsgtype:：tx）
        //如果
        //我们处于“仅块”模式，对等端不是白名单，或者白名单中继已关闭
        如果（！）FrelayTxes和（！Pfrom->Fwhetelisted！gargs.getboolarg（“-whitelistreay”，默认值为“whitelistreay”））
        {
            logprint（bclog:：net，“违反协议对等方发送的事务=%d\n”，pfrom->getid（））；
            回归真实；
        }

        std:：deque<coutpoint>vWorkQueue；
        std:：vector<uint256>verasequeue；
        ctransactionref ptx；
        VTCRV＞PTX；
        const ctransaction&tx=*ptx；

        cinv inv（msg_tx，tx.gethash（））；
        pfrom->addinventoryknown（inv）；

        Lock2（CS_Main，G_CS_孤儿）；

        bool fMissingInputs=假；
        cvalidationState状态；

        pfrom->setaskfor.erase（inv.hash）；
        mapalreadyaskedfor.erase（inv.hash）；

        std:：list<ctransactionref>lremovedtxn；

        如果（！）已准备好（投资）&&
            acceptToMoryPool（内存池、状态、ptx、&fMissingPuts、&lRemovedTxn、假/*旁路限制*/, 0 /* nAbsurdFee */)) {

            mempool.check(pcoinsTip.get());
            RelayTransaction(tx, connman);
            for (unsigned int i = 0; i < tx.vout.size(); i++) {
                vWorkQueue.emplace_back(inv.hash, i);
            }

            pfrom->nLastTXTime = GetTime();

            LogPrint(BCLog::MEMPOOL, "AcceptToMemoryPool: peer=%d: accepted %s (poolsz %u txn, %u kB)\n",
                pfrom->GetId(),
                tx.GetHash().ToString(),
                mempool.size(), mempool.DynamicMemoryUsage() / 1000);

//递归处理依赖于此事务的所有孤立事务
            std::set<NodeId> setMisbehaving;
            while (!vWorkQueue.empty()) {
                auto itByPrev = mapOrphanTransactionsByPrev.find(vWorkQueue.front());
                vWorkQueue.pop_front();
                if (itByPrev == mapOrphanTransactionsByPrev.end())
                    continue;
                for (auto mi = itByPrev->second.begin();
                     mi != itByPrev->second.end();
                     ++mi)
                {
                    const CTransactionRef& porphanTx = (*mi)->second.tx;
                    const CTransaction& orphanTx = *porphanTx;
                    const uint256& orphanHash = orphanTx.GetHash();
                    NodeId fromPeer = (*mi)->second.fromPeer;
                    bool fMissingInputs2 = false;
//使用虚拟的cvalidationState，这样就不会有人设置节点来基于孤立的DoS。
//解决方法（即，根据legittxx向人们提供一个无效的事务，以便
//禁止任何人转送合法证件）
                    CValidationState stateDummy;


                    if (setMisbehaving.count(fromPeer))
                        continue;
                    /*（acceptToMoryPool（mempool，statedummy，porphantx，&fMissingInputs2，&lremovedtxn，false/*旁路限制*/，0/*nabsurdfee*/）；
                        logprint（bclog:：mempool，“接受的孤立tx%s\n”，孤立hash.toString（））；
                        相关交易（Orphantx，Connman）；
                        for（unsigned int i=0；i<orphantx.vout.size（）；i++）
                            vworkqueue.emplace_back（孤立哈希，i）；
                        }
                        verasequeue.push_back（孤立哈希）；
                    }
                    否则如果（！）fMissingInputs2）
                    {
                        int nDOS＝0；
                        if（statedummy.isinvalid（ndos）&&ndos>0）
                        {
                            //惩罚给我们一个无效的孤立Tx的对等机
                            行为不端（来自同行、国家统计局）；
                            setmissbehaving.insert（来自对等机）；
                            logprint（bclog:：mempool，“无效的孤立tx%s\n”，孤立hash.toString（））；
                        }
                        //有输入但不接受到mempool
                        //可能是非标准或费用不足
                        logprint（bclog:：mempool，“已删除孤立tx%s\n”，孤立hash.toString（））；
                        verasequeue.push_back（孤立哈希）；
                        如果（！）orphantx.haswitness（）&&！statedummy.corruptionpossible（））
                            //不要对见证事务使用拒绝缓存，或者
                            //见证剥离的事务，因为它们可能被破坏。
                            //详情请参见https://github.com/bitcoin/bitcoin/issues/8279。
                            断言（recentrejects）；
                            recentrejects->insert（孤立哈希）；
                        }
                    }
                    mempool.check（pcoinstip.get（））；
                }
            }

            for（const uint256&hash:verasequeue）
                eraseorphantx（哈希）；
        }
        其他if（fMissingInputs）
        {
            bool frejectedparents=false；//可能是因为孤儿的父母都被拒绝了
            用于（const ctxin&txin:tx.vin）
                if（recentrejects->contains（txin.prevout.hash））
                    FrejectedParents=真；
                    断裂；
                }
            }
            如果（！）被抛弃的父母）
                uint32_t nfetchflags=getFetchFlags（pfrom）；
                用于（const ctxin&txin:tx.vin）
                    cinv _inv（msg_tx nfetchflags，txin.prevout.hash）；
                    pfrom->addinventoryknown（_inv）；
                    如果（！）alreadyhave（_inv））pfrom->askfor（_inv）；
                }
                addorphantx（ptx，pfrom->getid（））；

                //DoS预防：不允许mapornTransactions无限增长
                unsigned int nmaxorphantx=（unsigned int）std:：max（（int64_t）0，gargs.getarg（“-maxorphantx”，默认_max_orphan_transactions））；
                unsigned int nevicted=limitorphantxsize（nmaxorphantx）；
                如果（nevicted>0）
                    logprint（bclog:：mempool，“maporphan溢出，已删除%u tx\n”，未捕获）；
                }
            }否则{
                logprint（bclog:：mempool，“不保留被拒绝父级为%s的孤立项”，tx.gethash（）.toString（））；
                //我们将继续拒绝此Tx，因为它已拒绝
                //父母因此避免向其他同龄人再次请求。
                recentrejects->insert（tx.gethash（））；
            }
        }否则{
            如果（！）tx.haswitness（）&！state.corruptionpossible（））
                //不要对见证事务使用拒绝缓存，或者
                //见证剥离的事务，因为它们可能被破坏。
                //详情请参见https://github.com/bitcoin/bitcoin/issues/8279。
                断言（recentrejects）；
                recentrejects->insert（tx.gethash（））；
                if（递归动态用法（*ptx）<100000）
                    添加到CompactExtraTransactions（PTX）；
                }
            else if（tx.haswistence（）&&recurvedynamicusage（*ptx）<100000）
                添加到CompactExtraTransactions（PTX）；
            }

            if（pfrom->fHitelited和gargs.getBoolarg（“-whiteListforceRelay”，默认为“whiteListforceRelay”）；
                //始终中继从白名单对等方接收的事务，甚至
                //如果它们已经在mempool中或已被拒绝
                //到策略，允许节点作为
                //隐藏在它后面的节点。
                / /
                //从不中继将分配非零DOS的事务
                //得分，因为我们希望同龄人在这方面和我们一样
                //病例。
                int nDOS＝0；
                如果（！）状态.isinvalid（ndos）ndos==0）
                    logprintf（“强制从白名单对等端中继tx%s=%d\n”，tx.gethash（）.toString（），pfrom->getid（））；
                    相关传输（Tx，Connman）；
                }否则{
                    logprintf（“不中继无效的事务%s，来自白名单对等方=%d（%s）\n”，tx.gethash（）.toString（），pfrom->getid（），formatStateMessage（state））；
                }
            }
        }

        用于（const ctransactionref&removedtx:lremovedtxn）
            添加到compactExtraTransactions（removedtx）；

        //如果recentrejects检测到一个tx，我们将到达
        //此点和Tx将被忽略。因为我们还没跑
        //通过AcceptToMoryPool的Tx，我们不会计算DoS
        //得分或确定我们认为它无效的确切原因。
        / /
        //这意味着我们不会惩罚任何随后中继剂量的同龄人。
        //tx（即使我们惩罚了给我们的第一个同伴），因为
        //我们必须解释显示假阳性的recentreject。在
        //换句话说，如果我们不确定，就不应该惩罚同龄人。
        //提交了Dosy Tx。
        / /
        //注意recentrejects不仅仅记录dosy或invalid
        //事务，但mempool不接受的任何Tx，可能是
        //由于节点策略（与协商一致）。所以我们不能对
        //仅用于中继接收到的Tx的对等机，
        //不考虑假阳性。

        int nDOS＝0；
        if（state.isinvalid（ndos））。
        {
            logprint（bclog:：mempoolrej，“%s from peer=%d未被接受：%s\n”，tx.gethash（）.toString（），
                pOX-> GETIDED（）
                formatStateMessage（state））；
            if（enable_bip61&&state.getRejectCode（）>0&&state.getRejectCode（）<Reject_Internal）//从不通过p2p发送AcceptToMoryPool的内部代码
                connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：reject，strcommand，（unsigned char）state.getrejectcode（），
                                   state.getRejectReason（）.substr（0，max_reject_message_length，inv.hash））；
            }
            如果（NDOS＞0）{
                行为不当（pfrom->getid（），ndos）；
            }
        }
        回归真实；
    }

    如果（strcommand==netmsgtype:：cmpctblock&！F导入和&！freindex）//忽略导入时收到的块
    {
        cblockheader和shorttxids cmpctblock；
        VRECV>>CMPCTLOCK；

        bool received_new_header=false；

        {
        锁（CSKEMAN）；

        如果（！）lookupBlockIndex（cmpctBlock.header.hashPrevBlock））
            //不连接（或是Genesis），而不是在AcceptBlockHeader中给药，请求更深的Header
            如果（！）isInitialBlockDownload（））
                connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：getheaders，chainActive.getLocator（pindexBestHeader），uint256（））；
            回归真实；
        }

        如果（！）lookupBlockIndex（cmpctBlock.header.gethash（））
            接收的_new_header=true；
        }
        }

        常量cblockindex*pindex=nullptr；
        cvalidationState状态；
        如果（！）processNewBlockHeaders（cmpctBlock.header，state，chainParams，&pindex））
            INDOS；
            if（state.isinvalid（ndos））
                如果（NDOS＞0）{
                    锁（CSKEMAN）；
                    行为错误（pfrom->getid（），ndos，strprintf（“对等方%d通过cmpctblock向我们发送了无效的头文件\n”，pfrom->getid（））；
                }否则{
                    logprint（bclog:：net，“对等方%d通过cmpctblock向我们发送了无效的头文件\n”，pfrom->getid（））；
                }
                回归真实；
            }
        }

        //当我们成功地从cmpctblock解码块的txid时
        //消息我们通常跳转到blocktxn处理代码，
        //dummy（空）blocktxn消息，重新使用中的逻辑
        //完成假定块的处理（不带cs_main）。
        bool fprocessblocktxn=false；
        cdatastream blocktxnmsg（ser_网络，协议版本）；

        //如果我们最终将其视为一个普通的头消息，也可以调用它
        //没有cs_main。
        bool freverttoheaderprocessing=false；

        //为“乐观的”紧凑块重建保留一个cblock（请参见
        /下）
        std:：shared_ptr<cblock>pblock=std:：make_shared<cblock>（）；
        bool fblockrestructed=false；

        {
        Lock2（CS_Main，G_CS_孤儿）；
        //如果acceptBlockHeader返回true，则设置pindex
        断言（PbEnter）；
        updateBlockAvailability（pfrom->getID（），pindex->getBlockHash（））；

        cnodeState*nodestate=state（pfrom->getid（））；

        //如果这是一个新的头，其工作量超过了提示，请更新
        //对等端的最后一个块通知时间
        if（接收到_new_header&&pindex->nchainwork>chainactive.tip（）->nchainwork）
            nodestate->m_last_block_announcement=gettime（）；
        }

        std:：map<uint256，std:：pair<nodeid，std:：list<queuedblock>：：迭代器>>：：迭代器blockinflightit=mapblocksinflight.find（pindex->getblockhash（））；
        bool falreadyinflight=拦截器！=mapblocksinflight.end（）；

        if（pindex->nstatus&block_有_数据）//此处不做任何操作
            回归真实；

        如果（pindex->nchainwork<=chainactive.tip（）->nchainwork//我们知道一些更好的东西
                pHeX-> NTX！=0）//我们在某个时间点使用了此块，但对其进行了修剪
            如果（飞机失事）
                //我们出于某种原因请求了这个块，但是我们的mempool可能是无用的
                //所以我们只是通过普通的getdata获取块
                std:：vector<cinv>vinv（1）；
                vinv[0]=cinv（msg_block getfetchflags（pfrom），cmpctblock.header.gethash（））；
                connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：getdata，vinv））；
            }
            回归真实；
        }

        //如果我们还不接近tip，就放弃，让parallel block fetch发挥它的魔力
        如果（！）飞行准备就绪&！candirectfetch（chainParams.getconsensus（））
            回归真实；

        如果（iswitnessEnabled（pindex->pprev，chainParams.getconsensus（））&！nodestate->fsupportsdesiredcmpctversion）
            //不要费心处理来自v1对等机的压缩块
            //在Segwit激活后。
            回归真实；
        }

        //我们想要保守一点，只是要格外小心DOS
        //压缩块处理中的可能性…
        if（pindex->nheight<=chainactive.height（）+2）
            如果（）！fareadyFlight和nodestate->nblocksinflight<max_blocks_in_transit_per_peer）
                 （fareadyInflight和blockInflight->second.first==pfrom->getid（））_
                std:：list<queuedblock>：：迭代器*queuedblockit=nullptr；
                如果（！）markblockasinflight（pfrom->getid（），pindex->getblockhash（），pindex，&queuedblockit））；
                    如果（！）（*queuedblockit）->部分块）
                        （*queuedblockit）->partialblock.reset（new partiallydownloadedblock（&mempool））；
                    否则{
                        //该块已在使用来自同一对等机的压缩块运行中
                        logprint（bclog:：net，“Peer向我们发送了压缩块，我们已经同步了！\n）；
                        回归真实；
                    }
                }

                partiallyDownloadedBlock&partialBlock=*（*queuedblockit）->partialBlock；
                readstatus status=partialblock.initdata（cmpctblock，vextratxnforcompact）；
                if（status==read_status_invalid）
                    markblockasreceived（pindex->getblockhash（））；//如果是白名单，则重置飞行状态
                    行为错误（pfrom->getid（），100，strprintf（“对等方%d向我们发送了无效的压缩块\n”，pfrom->getid（））；
                    回归真实；
                否则，如果（status==read_status_failed）
                    //重复的txindexes，块现在正在运行，所以只需请求它
                    std:：vector<cinv>vinv（1）；
                    vinv[0]=cinv（msg_block getfetchflags（pfrom），cmpctblock.header.gethash（））；
                    connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：getdata，vinv））；
                    回归真实；
                }

                BlockTransactions请求请求请求；
                对于（size_t i=0；i<cmpctblock.blocktxcount（）；i++）
                    如果（！）分块可用（i）
                        请求索引。向后推（i）；
                }
                if（req.indexes.empty（））
                    //脏黑客跳转到blocktxn代码（todo:将消息处理移动到自己的函数中）
                    大宗交易txn；
                    txn.blockhash=cmpctblock.header.gethash（）；
                    blocktxnmsg<<txn；
                    fprocessblocktxn=真；
                }否则{
                    req.blockhash=pindex->getblockhash（）；
                    connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：getblocktxn，req））；
                }
            }否则{
                //此块已从另一个
                //对等，或者此对等具有太多未完成的块
                //下载自。
                //乐观地尝试重建，因为我们可能
                //可以不往返。
                部分下载块tempblock（&mempool）；
                readstatus status=tempblock.initdata（cmpctblock，vextratxnforcompact）；
                如果（状态）！=读取_状态_OK）
                    //TODO:不要忽略失败
                    回归真实；
                }
                std:：vector<ctransactionref>dummy；
                状态=tempblock.fillblock（*pblock，dummy）；
                if（status==read_status_ok）
                    fBlockRestructed=真；
                }
            }
        }否则{
            如果（飞机失事）
                //我们请求这个块，但它离未来很远，所以我们的
                //mempool可能是无用的-正常请求块
                std:：vector<cinv>vinv（1）；
                vinv[0]=cinv（msg_block getfetchflags（pfrom），cmpctblock.header.gethash（））；
                connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：getdata，vinv））；
                回归真实；
            }否则{
                //如果这是一个announce cmpctblock，我们希望得到与头消息相同的处理
                freverttoheaderprocessing=true；
            }
        }
        } //CS-主

        如果（fprocessblocktxn）
            返回processmessage（pfrom，netmsgtype:：blocktxn，blocktxnmsg，ntimeReceived，chainParams，connman，interruptmsgproc，enable_bip61）；

        如果（freverttoheaderprocessing）
            //从HB压缩块对等端接收的头可以是
            //在完全验证之前中继（参见BIP 152），因此我们不想断开
            //如果报头是针对无效块的，则为对等端。
            //请注意，如果对等机试图在无效链上构建，则
            //将被检测到并禁止对等机。
            返回processHeadersMessage（pfrom，connman，cmpctblock.header，chainParams，/*惩罚重复无效*/false);

        }

        if (fBlockReconstructed) {
//如果我们到了这里，我们可以乐观地重建
//阻止从其他对等机中传出的消息。
            {
                LOCK(cs_main);
                mapBlockSource.emplace(pblock->GetHash(), std::make_pair(pfrom->GetId(), false));
            }
            bool fNewBlock = false;
//将fforceprocessing设置为true意味着我们绕过了
//我们在AcceptBlock中的防DoS保护，可过滤
//可能试图浪费资源的未请求块
//（如磁盘空间）。因为我们只在
//我们已经接近尾声了（通过candirectfetch（）的要求
//上面，结合不请求块的行为，直到
//我们有一个链，至少有nminiumchainwork），我们忽略了
//工作比我们的小费少的小型砌块，治疗是安全的。
//已按要求重建紧凑块。
            /*cessnewblock（chainparams、pblock、/*fforceprocessing=*/true，&fnewblock）；
            如果（fnewblock）
                pfrom->nlastBlockTime=getTime（）；
            }否则{
                锁（CSKEMAN）；
                mapblocksource.erase（pblock->gethash（））；
            }
            lock（cs_main）；//为cblockindex:：isvalid（）保留cs_main
            if（pindex->isvalid（block_valid_transactions））
                //清除此块的下载状态，该块位于
                //从其他对等进程。我们打电话后就这么做了
                //处理newblock，以使具有malleated cmpctblock的公告
                //不能用来干扰块中继。
                markblockasReceived（pblock->gethash（））；
            }
        }
        回归真实；
    }

    如果（strcommand==netmsgtype:：blocktxn&！F导入和&！freindex）//忽略导入时收到的块
    {
        大宗交易响应；
        VCREV＞RESP；

        std:：shared_ptr<cblock>pblock=std:：make_shared<cblock>（）；
        bool fblockread=false；
        {
            锁（CSKEMAN）；

            std:：map<uint256，std:：pair<nodeid，std:：list<queuedblock>：：迭代器>>：：迭代器it=mapblocksinflight.find（resp.blockhash）；
            如果（it==mapblocksinflight.end（）！it->second.second->partialblock
                    它->第二个。第一个！=pfrom->getid（））
                logprint（bclog:：net，“对等方%d向我们发送了我们不期望的块的块事务\n”，pfrom->getid（））；
                回归真实；
            }

            部分下载块&partialblock=*it->second.second->partialblock；
            readstatus status=partialblock.fillblock（*pblock，resp.txn）；
            if（status==read_status_invalid）
                markblockasreceived（resp.blockhash）；//如果是白名单，则重置飞行状态
                行为错误（pfrom->getid（），100，strprintf（“对等方%d向我们发送了无效的压缩块/不匹配的块事务\n”，pfrom->getid（））；
                回归真实；
            否则，如果（status==read_status_failed）
                //可能发生冲突，现在返回到getdata:（）
                std:：vector<cinv>invs；
                invs.push_back（cinv（msg_block_getfetchflags（pfrom），resp.blockhash））；
                connman->pushmessage（pfrom，msgmaker.make（netmsgtype:：getdata，invs））；
            }否则{
                //块正常，或者我们可能收到
                //读取\状态\检查块\失败。
                //请注意，checkblock只能由于以下几个原因之一而失败：
                / / 1。糟糕的工作证明（这里不可能，因为我们已经
                //接受标题）
                / / 2。merkleroot与给定的事务不匹配（已经
                //在FillBlock中捕获，但读取\状态\失败，因此
                //这里不可能）
                / / 3。块在其他方面无效（例如无效的coinbase，
                //块太大，太多旧的sigops等）。
                //因此，如果checkblock失败，3是唯一的可能性。
                //根据BIP 152，除非工作证明
                //无效（我们不需要所有的无状态检查
                /运行。这是在下面处理的，所以只需将其视为
                //尽管块已成功读取，但依赖于
                //在processNewBlock中处理以确保块索引为
                //更新，拒绝消息传出等。
                markblockasreceived（resp.blockhash）；//现在是空指针
                fBlockRead=真；
                //mapblocksource仅用于发送拒绝消息和DOS分数，
                //所以这里和cs_main in processNewblock之间的竞争很好。
                //BIP 152允许对等机在验证后中继压缩块
                //仅限头部；如果块发生旋转，则不应惩罚对等体
                //无效。
                mapblocksource.emplace（resp.blockhash，std:：make_pair（pfrom->getid（），false））；
            }
        //调用processNewBlock时不要按住cs_Main
        如果（FBlockread）
            bool fnewblock=false；
            //因为我们请求了这个块（它在mapblocksinflight中），所以强制处理它，
            //即使它不是新提示的候选项（缺少上一个块、链不够长等）
            //这将绕过AcceptBlock中的一些反DoS逻辑（例如
            //磁盘空间攻击），但由于
            //压缩块处理程序中的保护--请参见相关注释
            //在紧凑块乐观重建处理中。
            processNewBlock（链参数，pblock，/*fforceprocessing*/true, &fNewBlock);

            if (fNewBlock) {
                pfrom->nLastBlockTime = GetTime();
            } else {
                LOCK(cs_main);
                mapBlockSource.erase(pblock->GetHash());
            }
        }
        return true;
    }

if (strCommand == NetMsgType::HEADERS && !fImporting && !fReindex) //忽略导入时收到的邮件头
    {
        std::vector<CBlockHeader> headers;

//绕过常规的cblock反序列化，因为我们不希望冒险反序列化2000个完整块。
        unsigned int nCount = ReadCompactSize(vRecv);
        if (nCount > MAX_HEADERS_RESULTS) {
            LOCK(cs_main);
            Misbehaving(pfrom->GetId(), 20, strprintf("headers message size = %u", nCount));
            return false;
        }
        headers.resize(nCount);
        for (unsigned int n = 0; n < nCount; n++) {
            vRecv >> headers[n];
ReadCompactSize(vRecv); //忽略Tx计数；假设为0。
        }

//通过头消息接收的头应有效，并反映
//对等机所在的链。如果我们收到一个已知无效的头，
//如果对等机正在使用某个出站连接，则断开它的连接
//槽。
        bool should_punish = !pfrom->fInbound && !pfrom->m_manual_connection;
        return ProcessHeadersMessage(pfrom, connman, headers, chainparams, should_punish);
    }

if (strCommand == NetMsgType::BLOCK && !fImporting && !fReindex) //忽略导入时收到的块
    {
        std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>();
        vRecv >> *pblock;

        LogPrint(BCLog::NET, "received block %s peer=%d\n", pblock->GetHash().ToString(), pfrom->GetId());

        bool forceProcessing = false;
        const uint256 hash(pblock->GetHash());
        {
            LOCK(cs_main);
//如果我们显式地请求块，也总是处理，正如我们可能的那样
//需要它，即使它不是一个新的最佳提示的候选人。
            forceProcessing |= MarkBlockAsReceived(hash);
//mapblocksource仅用于发送拒绝消息和DOS分数，
//因此，这里和CS_Main In ProcessNewBlock之间的竞争很好。
            mapBlockSource.emplace(hash, std::make_pair(pfrom->GetId(), true));
        }
        bool fNewBlock = false;
        ProcessNewBlock(chainparams, pblock, forceProcessing, &fNewBlock);
        if (fNewBlock) {
            pfrom->nLastBlockTime = GetTime();
        } else {
            LOCK(cs_main);
            mapBlockSource.erase(pblock->GetHash());
        }
        return true;
    }

    if (strCommand == NetMsgType::GETADDR) {
//引入了入站和出站连接的这种不对称行为
//防止指纹攻击：攻击者可以发送特定的假地址
//发送给用户的addrman，然后通过发送getaddr消息请求它们。
//使位于NAT之后且只能使传出连接忽略的节点
//getaddr消息减轻了攻击。
        if (!pfrom->fInbound) {
            LogPrint(BCLog::NET, "Ignoring \"getaddr\" from outbound connection. peer=%d\n", pfrom->GetId());
            return true;
        }

//每个连接只发送一个getaddr响应以减少资源浪费
//并且不鼓励在INV公告中添加地址标记。
        if (pfrom->fSentAddr) {
            LogPrint(BCLog::NET, "Ignoring repeated \"getaddr\". peer=%d\n", pfrom->GetId());
            return true;
        }
        pfrom->fSentAddr = true;

        pfrom->vAddrToSend.clear();
        std::vector<CAddress> vAddr = connman->GetAddresses();
        FastRandomContext insecure_rand;
        for (const CAddress &addr : vAddr)
            pfrom->PushAddress(addr, insecure_rand);
        return true;
    }

    if (strCommand == NetMsgType::MEMPOOL) {
        if (!(pfrom->GetLocalServices() & NODE_BLOOM) && !pfrom->fWhitelisted)
        {
            LogPrint(BCLog::NET, "mempool request with bloom filters disabled, disconnect peer=%d\n", pfrom->GetId());
            pfrom->fDisconnect = true;
            return true;
        }

        if (connman->OutboundTargetReached(false) && !pfrom->fWhitelisted)
        {
            LogPrint(BCLog::NET, "mempool request with bandwidth limit reached, disconnect peer=%d\n", pfrom->GetId());
            pfrom->fDisconnect = true;
            return true;
        }

        LOCK(pfrom->cs_inventory);
        pfrom->fSendMempool = true;
        return true;
    }

    if (strCommand == NetMsgType::PING) {
        if (pfrom->nVersion > BIP0031_VERSION)
        {
            uint64_t nonce = 0;
            vRecv >> nonce;
//用nonce回送消息。这允许两个有用的功能：
//
//1）远程节点可以快速检查连接是否正常。
//2）远程节点可以测量网络线程的延迟。如果这个节点
//过载，它不会快速响应ping，远程节点可以
//避免给我们发送更多的工作，比如链下载请求。
//
//nonce阻止远程程序在不同ping之间混淆：不
//如果远程节点每秒发送一次ping，并且此节点占用5
//响应时间为秒，远程发送的第5个ping将显示为
//很快就回来。
            connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::PONG, nonce));
        }
        return true;
    }

    if (strCommand == NetMsgType::PONG) {
        int64_t pingUsecEnd = nTimeReceived;
        uint64_t nonce = 0;
        size_t nAvail = vRecv.in_avail();
        bool bPingFinished = false;
        std::string sProblem;

        if (nAvail >= sizeof(nonce)) {
            vRecv >> nonce;

//只有在存在未完成的ping时才处理pong消息（不带nonce的旧ping不应进行pong）
            if (pfrom->nPingNonceSent != 0) {
                if (nonce == pfrom->nPingNonceSent) {
//与收到的乒乓球相匹配，这个乒乓球不再优秀
                    bPingFinished = true;
                    int64_t pingUsecTime = pingUsecEnd - pfrom->nPingUsecStart;
                    if (pingUsecTime > 0) {
//Ping时间测量成功，替换上一个
                        pfrom->nPingUsecTime = pingUsecTime;
                        pfrom->nMinPingUsecTime = std::min(pfrom->nMinPingUsecTime.load(), pingUsecTime);
                    } else {
//这不应该发生
                        sProblem = "Timing mishap";
                    }
                } else {
//当ping重叠时，nonce不匹配是正常的
                    sProblem = "Nonce mismatch";
                    if (nonce == 0) {
//这很可能是另一个实现中的错误；取消此ping
                        bPingFinished = true;
                        sProblem = "Nonce zero";
                    }
                }
            } else {
                sProblem = "Unsolicited pong without ping";
            }
        } else {
//这很可能是另一个实现中的错误；取消此ping
            bPingFinished = true;
            sProblem = "Short payload";
        }

        if (!(sProblem.empty())) {
            LogPrint(BCLog::NET, "pong peer=%d: %s, %x expected, %x received, %u bytes\n",
                pfrom->GetId(),
                sProblem,
                pfrom->nPingNonceSent,
                nonce,
                nAvail);
        }
        if (bPingFinished) {
            pfrom->nPingNonceSent = 0;
        }
        return true;
    }

    if (strCommand == NetMsgType::FILTERLOAD) {
        CBloomFilter filter;
        vRecv >> filter;

        if (!filter.IsWithinSizeConstraints())
        {
//没有理由发送过大的过滤器
            LOCK(cs_main);
            Misbehaving(pfrom->GetId(), 100);
        }
        else
        {
            LOCK(pfrom->cs_filter);
            pfrom->pfilter.reset(new CBloomFilter(filter));
            pfrom->pfilter->UpdateEmptyFull();
            pfrom->fRelayTxes = true;
        }
        return true;
    }

    if (strCommand == NetMsgType::FILTERADD) {
        std::vector<unsigned char> vData;
        vRecv >> vData;

//节点绝不能发送大于520字节的数据项（脚本数据对象的最大大小，
//因此，在filteradd消息中任何匹配对象的最大大小
        bool bad = false;
        if (vData.size() > MAX_SCRIPT_ELEMENT_SIZE) {
            bad = true;
        } else {
            LOCK(pfrom->cs_filter);
            if (pfrom->pfilter) {
                pfrom->pfilter->insert(vData);
            } else {
                bad = true;
            }
        }
        if (bad) {
            LOCK(cs_main);
            Misbehaving(pfrom->GetId(), 100);
        }
        return true;
    }

    if (strCommand == NetMsgType::FILTERCLEAR) {
        LOCK(pfrom->cs_filter);
        if (pfrom->GetLocalServices() & NODE_BLOOM) {
            pfrom->pfilter.reset(new CBloomFilter());
        }
        pfrom->fRelayTxes = true;
        return true;
    }

    if (strCommand == NetMsgType::FEEFILTER) {
        CAmount newFeeFilter = 0;
        vRecv >> newFeeFilter;
        if (MoneyRange(newFeeFilter)) {
            {
                LOCK(pfrom->cs_feeFilter);
                pfrom->minFeeFilter = newFeeFilter;
            }
            LogPrint(BCLog::NET, "received: feefilter of %s from peer=%d\n", CFeeRate(newFeeFilter).ToString(), pfrom->GetId());
        }
        return true;
    }

    if (strCommand == NetMsgType::NOTFOUND) {
//我们不关心未找到的消息，但是记录一个未知的命令
//当我们自己传递信息时，它是不受欢迎的。
        return true;
    }

//忽略用于扩展性的未知命令
    LogPrint(BCLog::NET, "Unknown command \"%s\" from peer=%d\n", SanitizeString(strCommand), pfrom->GetId());
    return true;
}

static bool SendRejectsAndCheckIfBanned(CNode* pnode, CConnman* connman, bool enable_bip61) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
    CNodeState &state = *State(pnode->GetId());

    if (enable_bip61) {
        for (const CBlockReject& reject : state.rejects) {
            connman->PushMessage(pnode, CNetMsgMaker(INIT_PROTO_VERSION).Make(NetMsgType::REJECT, std::string(NetMsgType::BLOCK), reject.chRejectCode, reject.strRejectReason, reject.hashBlock));
        }
    }
    state.rejects.clear();

    if (state.fShouldBan) {
        state.fShouldBan = false;
        if (pnode->fWhitelisted)
            LogPrintf("Warning: not punishing whitelisted peer %s!\n", pnode->addr.ToString());
        else if (pnode->m_manual_connection)
            LogPrintf("Warning: not punishing manually-connected peer %s!\n", pnode->addr.ToString());
        else {
            pnode->fDisconnect = true;
            if (pnode->addr.IsLocal())
                LogPrintf("Warning: not banning local peer %s!\n", pnode->addr.ToString());
            else
            {
                connman->Ban(pnode->addr, BanReasonNodeMisbehaving);
            }
        }
        return true;
    }
    return false;
}

bool PeerLogicValidation::ProcessMessages(CNode* pfrom, std::atomic<bool>& interruptMsgProc)
{
    const CChainParams& chainparams = Params();
//
//报文格式
//（4）消息开始
//（12）命令
//（4）尺寸
//（4）校验和
//（x）数据
//
    bool fMoreWork = false;

    if (!pfrom->vRecvGetData.empty())
        ProcessGetData(pfrom, chainparams, connman, interruptMsgProc);

    if (pfrom->fDisconnect)
        return false;

//这保持了响应的顺序
    if (!pfrom->vRecvGetData.empty()) return true;

//如果发送缓冲区太满而无法响应，则不必费心。
    if (pfrom->fPauseSend)
        return false;

    std::list<CNetMessage> msgs;
    {
        LOCK(pfrom->cs_vProcessMsg);
        if (pfrom->vProcessMsg.empty())
            return false;
//带个信就行了
        msgs.splice(msgs.begin(), pfrom->vProcessMsg, pfrom->vProcessMsg.begin());
        pfrom->nProcessQueueSize -= msgs.front().vRecv.size() + CMessageHeader::HEADER_SIZE;
        pfrom->fPauseRecv = pfrom->nProcessQueueSize > connman->GetReceiveFloodSize();
        fMoreWork = !pfrom->vProcessMsg.empty();
    }
    CNetMessage& msg(msgs.front());

    msg.SetVersion(pfrom->GetRecvVersion());
//扫描消息开始
    if (memcmp(msg.hdr.pchMessageStart, chainparams.MessageStart(), CMessageHeader::MESSAGE_START_SIZE) != 0) {
        LogPrint(BCLog::NET, "PROCESSMESSAGE: INVALID MESSAGESTART %s peer=%d\n", SanitizeString(msg.hdr.GetCommand()), pfrom->GetId());
        pfrom->fDisconnect = true;
        return false;
    }

//读头
    CMessageHeader& hdr = msg.hdr;
    if (!hdr.IsValid(chainparams.MessageStart()))
    {
        LogPrint(BCLog::NET, "PROCESSMESSAGE: ERRORS IN HEADER %s peer=%d\n", SanitizeString(hdr.GetCommand()), pfrom->GetId());
        return fMoreWork;
    }
    std::string strCommand = hdr.GetCommand();

//消息大小
    unsigned int nMessageSize = hdr.nMessageSize;

//校验和
    CDataStream& vRecv = msg.vRecv;
    const uint256& hash = msg.GetMessageHash();
    if (memcmp(hash.begin(), hdr.pchChecksum, CMessageHeader::CHECKSUM_SIZE) != 0)
    {
        LogPrint(BCLog::NET, "%s(%s, %u bytes): CHECKSUM ERROR expected %s was %s\n", __func__,
           SanitizeString(strCommand), nMessageSize,
           HexStr(hash.begin(), hash.begin()+CMessageHeader::CHECKSUM_SIZE),
           HexStr(hdr.pchChecksum, hdr.pchChecksum+CMessageHeader::CHECKSUM_SIZE));
        return fMoreWork;
    }

//进程消息
    bool fRet = false;
    try
    {
        fRet = ProcessMessage(pfrom, strCommand, vRecv, msg.nTime, chainparams, connman, interruptMsgProc, m_enable_bip61);
        if (interruptMsgProc)
            return false;
        if (!pfrom->vRecvGetData.empty())
            fMoreWork = true;
    }
    catch (const std::ios_base::failure& e)
    {
        if (m_enable_bip61) {
            connman->PushMessage(pfrom, CNetMsgMaker(INIT_PROTO_VERSION).Make(NetMsgType::REJECT, strCommand, REJECT_MALFORMED, std::string("error parsing message")));
        }
        if (strstr(e.what(), "end of data"))
        {
//允许在VRECV上出现长度不足消息的异常
            LogPrint(BCLog::NET, "%s(%s, %u bytes): Exception '%s' caught, normally caused by a message being shorter than its stated length\n", __func__, SanitizeString(strCommand), nMessageSize, e.what());
        }
        else if (strstr(e.what(), "size too large"))
        {
//允许来自过长大小的异常
            LogPrint(BCLog::NET, "%s(%s, %u bytes): Exception '%s' caught\n", __func__, SanitizeString(strCommand), nMessageSize, e.what());
        }
        else if (strstr(e.what(), "non-canonical ReadCompactSize()"))
        {
//允许来自非规范编码的异常
            LogPrint(BCLog::NET, "%s(%s, %u bytes): Exception '%s' caught\n", __func__, SanitizeString(strCommand), nMessageSize, e.what());
        }
        else
        {
            PrintExceptionContinue(&e, "ProcessMessages()");
        }
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "ProcessMessages()");
    } catch (...) {
        PrintExceptionContinue(nullptr, "ProcessMessages()");
    }

    if (!fRet) {
        LogPrint(BCLog::NET, "%s(%s, %u bytes) FAILED peer=%d\n", __func__, SanitizeString(strCommand), nMessageSize, pfrom->GetId());
    }

    LOCK(cs_main);
    SendRejectsAndCheckIfBanned(pfrom, connman, m_enable_bip61);

    return fMoreWork;
}

void PeerLogicValidation::ConsiderEviction(CNode *pto, int64_t time_in_seconds)
{
    AssertLockHeld(cs_main);

    CNodeState &state = *State(pto->GetId());
    const CNetMsgMaker msgMaker(pto->GetSendVersion());

    if (!state.m_chain_sync.m_protect && IsOutboundDisconnectionCandidate(pto) && state.fSyncStarted) {
//这是一个出站对等机，如果不连接，则会断开连接。
//通知一个块，其中包含与当前提示相同的工作量
//链同步超时+头响应时间秒（注：如果
//他们的链条比我们的链条有更多的工作，我们应该与之同步，
//除非它是无效的，在这种情况下，我们应该发现
//从其他地方断开）。
        if (state.pindexBestKnownBlock != nullptr && state.pindexBestKnownBlock->nChainWork >= chainActive.Tip()->nChainWork) {
            if (state.m_chain_sync.m_timeout != 0) {
                state.m_chain_sync.m_timeout = 0;
                state.m_chain_sync.m_work_header = nullptr;
                state.m_chain_sync.m_sent_getheaders = false;
            }
        } else if (state.m_chain_sync.m_timeout == 0 || (state.m_chain_sync.m_work_header != nullptr && state.pindexBestKnownBlock != nullptr && state.pindexBestKnownBlock->nChainWork >= state.m_chain_sync.m_work_header->nChainWork)) {
//我们最著名的障碍是在我们的提示后面，我们要么注意到
//这是第一次，或者这个同龄人能够赶上更早的一点。
//在那里我们核对了我们的小费。
//无论哪种方法，都要根据当前提示设置新的超时。
            state.m_chain_sync.m_timeout = time_in_seconds + CHAIN_SYNC_TIMEOUT;
            state.m_chain_sync.m_work_header = chainActive.Tip();
            state.m_chain_sync.m_sent_getheaders = false;
        } else if (state.m_chain_sync.m_timeout > 0 && time_in_seconds > state.m_chain_sync.m_timeout) {
//还没有证据表明我们的同龄人已经同步到一个工作等于这个的链上。
//当我们第一次发现它在后面的时候。发送单个GetHeaders
//给同伴一个更新我们的机会。
            if (state.m_chain_sync.m_sent_getheaders) {
//他们赶不上了！
                LogPrintf("Disconnecting outbound peer %d for old chain, best known block = %s\n", pto->GetId(), state.pindexBestKnownBlock != nullptr ? state.pindexBestKnownBlock->GetBlockHash().ToString() : "<none>");
                pto->fDisconnect = true;
            } else {
                assert(state.m_chain_sync.m_work_header);
                LogPrint(BCLog::NET, "sending getheaders to outbound peer=%d to verify chain work (current best known block:%s, benchmark blockhash: %s)\n", pto->GetId(), state.pindexBestKnownBlock != nullptr ? state.pindexBestKnownBlock->GetBlockHash().ToString() : "<none>", state.m_chain_sync.m_work_header->GetBlockHash().ToString());
                connman->PushMessage(pto, msgMaker.Make(NetMsgType::GETHEADERS, chainActive.GetLocator(state.m_chain_sync.m_work_header->pprev), uint256()));
                state.m_chain_sync.m_sent_getheaders = true;
constexpr int64_t HEADERS_RESPONSE_TIME = 120; //2分钟
//触发超时以允许响应，这可以清除超时
//（如果响应显示对等机已同步），则重置超时（如果
//对等机同步到所需的工作，但不同步到我们的提示），或结果
//断开连接（如果我们进入超时和pindexBestKnownBlock
//进展不够）
                state.m_chain_sync.m_timeout = time_in_seconds + HEADERS_RESPONSE_TIME;
            }
        }
    }
}

void PeerLogicValidation::EvictExtraOutboundPeers(int64_t time_in_seconds)
{
//检查是否有太多的出站对等机
    int extra_peers = connman->GetExtraOutboundCount();
    if (extra_peers > 0) {
//如果我们有比目标更多的出站对等点，请断开一个。
//选择最近公布的出站对等点
//我们是一个新的街区，选择最近的
//连接（更高的节点ID）
        NodeId worst_peer = -1;
        int64_t oldest_block_announcement = std::numeric_limits<int64_t>::max();

        connman->ForEachNode([&](CNode* pnode) {
            AssertLockHeld(cs_main);

//忽略非出站对等点，或已标记为断开连接的节点
            if (!IsOutboundDisconnectionCandidate(pnode) || pnode->fDisconnect) return;
            CNodeState *state = State(pnode->GetId());
if (state == nullptr) return; //不可能，但以防万一
//不要驱逐我们受保护的同行
            if (state->m_chain_sync.m_protect) return;
            if (state->m_last_block_announcement < oldest_block_announcement || (state->m_last_block_announcement == oldest_block_announcement && pnode->GetId() > worst_peer)) {
                worst_peer = pnode->GetId();
                oldest_block_announcement = state->m_last_block_announcement;
            }
        });
        if (worst_peer != -1) {
            bool disconnected = connman->ForNode(worst_peer, [&](CNode *pnode) {
                AssertLockHeld(cs_main);

//仅断开与我们连接的对等机的连接
//我们检查频率的合理分数
//到了新信息到达的时候了。
//另外，不要断开我们试图下载的任何对等机
//阻止。
                CNodeState &state = *State(pnode->GetId());
                if (time_in_seconds - pnode->nTimeConnected > MINIMUM_CONNECT_TIME && state.nBlocksInFlight == 0) {
                    LogPrint(BCLog::NET, "disconnecting extra outbound peer=%d (last block announcement received at time %d)\n", pnode->GetId(), oldest_block_announcement);
                    pnode->fDisconnect = true;
                    return true;
                } else {
                    LogPrint(BCLog::NET, "keeping outbound peer=%d chosen for eviction (connect time: %d, blocks_in_flight: %d)\n", pnode->GetId(), pnode->nTimeConnected, state.nBlocksInFlight);
                    return false;
                }
            });
            if (disconnected) {
//如果我们断开了一个额外的对等点，这意味着我们成功了
//上次连接后至少连接到一个对等机
//检测到一个过时的提示。不要再尝试其他同行，直到
//接下来，我们检测到一个过时的提示，以限制我们在
//来自这些额外连接的网络。
                connman->SetTryNewOutboundPeer(false);
            }
        }
    }
}

void PeerLogicValidation::CheckForStaleTipAndEvictPeers(const Consensus::Params &consensusParams)
{
    LOCK(cs_main);

    if (connman == nullptr) return;

    int64_t time_in_seconds = GetTime();

    EvictExtraOutboundPeers(time_in_seconds);

    if (time_in_seconds > m_stale_tip_check_time) {
//检查我们的小费是否过期，如果是，允许使用额外的
//出站对等体
        if (!fImporting && !fReindex && connman->GetNetworkActive() && connman->GetUseAddrmanOutgoing() && TipMayBeStale(consensusParams)) {
            LogPrintf("Potential stale tip detected, will try using extra outbound peer (last tip update: %d seconds ago)\n", time_in_seconds - g_last_tip_update);
            connman->SetTryNewOutboundPeer(true);
        } else if (connman->GetTryNewOutboundPeer()) {
            connman->SetTryNewOutboundPeer(false);
        }
        m_stale_tip_check_time = time_in_seconds + STALE_CHECK_INTERVAL;
    }
}

namespace {
class CompareInvMempoolOrder
{
    CTxMemPool *mp;
public:
    explicit CompareInvMempoolOrder(CTxMemPool *_mempool)
    {
        mp = _mempool;
    }

    bool operator()(std::set<uint256>::iterator a, std::set<uint256>::iterator b)
    {
        /*因为std：：使堆产生最大堆，所以我们需要
         *祖先最少/以后排序的费用最高。*/

        return mp->CompareDepthAndScore(*b, *a);
    }
};
}

bool PeerLogicValidation::SendMessages(CNode* pto)
{
    const Consensus::Params& consensusParams = Params().GetConsensus();
    {
//在版本握手完成之前不要发送任何内容
        if (!pto->fSuccessfullyConnected || pto->fDisconnect)
            return true;

//如果我们到达这里，将设置传出消息序列化版本，并且无法更改。
        const CNetMsgMaker msgMaker(pto->GetSendVersion());

//
//信息：平
//
        bool pingSend = false;
        if (pto->fPingQueued) {
//用户的RPC Ping请求
            pingSend = true;
        }
        if (pto->nPingNonceSent == 0 && pto->nPingUsecStart + PING_INTERVAL * 1000000 < GetTimeMicros()) {
//Ping作为延迟探测器自动发送&keepalive。
            pingSend = true;
        }
        if (pingSend) {
            uint64_t nonce = 0;
            while (nonce == 0) {
                GetRandBytes((unsigned char*)&nonce, sizeof(nonce));
            }
            pto->fPingQueued = false;
            pto->nPingUsecStart = GetTimeMicros();
            if (pto->nVersion > BIP0031_VERSION) {
                pto->nPingNonceSent = nonce;
                connman->PushMessage(pto, msgMaker.Make(NetMsgType::PING, nonce));
            } else {
//对等机太旧，无法用nonce支持ping命令，pong将永远不会到达。
                pto->nPingNonceSent = 0;
                connman->PushMessage(pto, msgMaker.Make(NetMsgType::PING));
            }
        }

TRY_LOCK(cs_main, lockMain); //为isitialBlockDownload（）和cnodeState（）获取cs_main
        if (!lockMain)
            return true;

        if (SendRejectsAndCheckIfBanned(pto, connman, m_enable_bip61))
            return true;
        CNodeState &state = *State(pto->GetId());

//地址刷新广播
        int64_t nNow = GetTimeMicros();
        if (!IsInitialBlockDownload() && pto->nNextLocalAddrSend < nNow) {
            AdvertiseLocal(pto);
            pto->nNextLocalAddrSend = PoissonNextSend(nNow, AVG_LOCAL_ADDRESS_BROADCAST_INTERVAL);
        }

//
//信息：ADDR
//
        if (pto->nNextAddrSend < nNow) {
            pto->nNextAddrSend = PoissonNextSend(nNow, AVG_ADDRESS_BROADCAST_INTERVAL);
            std::vector<CAddress> vAddr;
            vAddr.reserve(pto->vAddrToSend.size());
            for (const CAddress& addr : pto->vAddrToSend)
            {
                if (!pto->addrKnown.contains(addr.GetKey()))
                {
                    pto->addrKnown.insert(addr.GetKey());
                    vAddr.push_back(addr);
//接收器拒绝大于1000的地址消息
                    if (vAddr.size() >= 1000)
                    {
                        connman->PushMessage(pto, msgMaker.Make(NetMsgType::ADDR, vAddr));
                        vAddr.clear();
                    }
                }
            }
            pto->vAddrToSend.clear();
            if (!vAddr.empty())
                connman->PushMessage(pto, msgMaker.Make(NetMsgType::ADDR, vAddr));
//我们只发送一次大地址消息
            if (pto->vAddrToSend.capacity() > 40)
                pto->vAddrToSend.shrink_to_fit();
        }

//开始块同步
        if (pindexBestHeader == nullptr)
            pindexBestHeader = chainActive.Tip();
bool fFetch = state.fPreferredDownload || (nPreferredDownload == 0 && !pto->fClient && !pto->fOneShot); //如果这是一个好的对等点，或者我们没有好的对等点，那么可以下载这个对等点。
        if (!state.fSyncStarted && !pto->fClient && !fImporting && !fReindex) {
//只有当我们接近今天时，才主动地从一个对等端请求消息头。
            if ((nSyncStarted == 0 && fFetch) || pindexBestHeader->GetBlockTime() > GetAdjustedTime() - 24 * 60 * 60) {
                state.fSyncStarted = true;
                state.nHeadersSyncTimeout = GetTimeMicros() + HEADERS_DOWNLOAD_TIMEOUT_BASE + HEADERS_DOWNLOAD_TIMEOUT_PER_HEADER * (GetAdjustedTime() - pindexBestHeader->GetBlockTime())/(consensusParams.nPowTargetSpacing);
                nSyncStarted++;
                const CBlockIndex *pindexStart = pindexBestHeader;
                /*如果可能，从当前
                   最著名的标题。这确保了我们总是
                   只要对等端返回非空的头列表
                   是最新的。使用非空响应，我们可以初始化
                   同行的已知最佳区块。这是不可能的
                   如果我们要求从PindexBestHeader开始
                   得到了一个空的答复。*/

                if (pindexStart->pprev)
                    pindexStart = pindexStart->pprev;
                LogPrint(BCLog::NET, "initial getheaders (%d) to peer=%d (startheight:%d)\n", pindexStart->nHeight, pto->GetId(), pto->nStartingHeight);
                connman->PushMessage(pto, msgMaker.Make(NetMsgType::GETHEADERS, chainActive.GetLocator(pindexStart), uint256()));
            }
        }

//重新发送尚未进入数据块的钱包交易
//除了重新索引、导入和IBD期间，当旧钱包
//事务变为未确认，并垃圾邮件其他节点。
        if (!fReindex && !fImporting && !IsInitialBlockDownload())
        {
            GetMainSignals().Broadcast(nTimeBestReceived, connman);
        }

//
//尝试通过标题发送阻止通知
//
        {
//如果我们有少于max的\块\要在我们的
//我们正在中继的块散列列表，我们的对等方希望
//标题通知，然后查找第一个标题
//我们的同伴还不知道，但会连接并发送。
//如果没有连接头，或者我们有太多
//块，或者如果对等端不需要头，只需要
//全部添加到inv队列。
            LOCK(pto->cs_inventory);
            std::vector<CBlock> vHeaders;
            bool fRevertToInv = ((!state.fPreferHeaders &&
                                 (!state.fPreferHeaderAndIDs || pto->vBlockHashesToAnnounce.size() > 1)) ||
                                pto->vBlockHashesToAnnounce.size() > MAX_BLOCKS_TO_ANNOUNCE);
const CBlockIndex *pBestIndex = nullptr; //最后一个排队等待传递的头
ProcessBlockAvailability(pto->GetId()); //确保pindexbestknownblock是最新的

            if (!fRevertToInv) {
                bool fFoundStartingHeader = false;
//试着找到我们的同伴没有的第一个标题，并且
//然后将所有邮件头发送到该邮件头之后。如果我们遇到任何
//不在chainactive上的头，放弃。
                for (const uint256 &hash : pto->vBlockHashesToAnnounce) {
                    const CBlockIndex* pindex = LookupBlockIndex(hash);
                    assert(pindex);
                    if (chainActive[pindex->nHeight] != pindex) {
//如果我们重新安排离开这个街区
                        fRevertToInv = true;
                        break;
                    }
                    if (pBestIndex != nullptr && pindex->pprev != pBestIndex) {
//这意味着要公布的块列表不会
//相互连接。
//这不应该真的在
//常规操作（因为REORGS应该带我们去
//有块的链子，不在先前的链子上，
//这应该在之前的检查中发现），但是
//这种情况的发生方式是使用invalidBlock/
//在尖端反复考虑阻塞，使其
//多次添加到vblockhashestoanounce。
//通过恢复来坚决处理这种罕见的情况
//入侵
                        fRevertToInv = true;
                        break;
                    }
                    pBestIndex = pindex;
                    if (fFoundStartingHeader) {
//将此添加到邮件头中
                        vHeaders.push_back(pindex->GetBlockHeader());
                    } else if (PeerHasHeader(&state, pindex)) {
continue; //继续寻找第一个新街区
                    } else if (pindex->pprev == nullptr || PeerHasHeader(&state, pindex->pprev)) {
//对等端没有此头，但它们有前一个头。
//开始发送邮件头。
                        fFoundStartingHeader = true;
                        vHeaders.push_back(pindex->GetBlockHeader());
                    } else {
//对等端没有此头或前一个头--任何内容都不会
//接上，所以救出。
                        fRevertToInv = true;
                        break;
                    }
                }
            }
            if (!fRevertToInv && !vHeaders.empty()) {
                if (vHeaders.size() == 1 && state.fPreferHeaderAndIDs) {
//我们最多只能发送1个块作为头和ID，否则
//可能意味着我们正在进行初始的ISH同步，或者它们很慢。
                    LogPrint(BCLog::NET, "%s sending header-and-ids %s to peer=%d\n", __func__,
                            vHeaders.front().GetHash().ToString(), pto->GetId());

                    int nSendFlags = state.fWantsCmpctWitness ? 0 : SERIALIZE_TRANSACTION_NO_WITNESS;

                    bool fGotBlockFromCache = false;
                    {
                        LOCK(cs_most_recent_block);
                        if (most_recent_block_hash == pBestIndex->GetBlockHash()) {
                            if (state.fWantsCmpctWitness || !fWitnessesPresentInMostRecentCompactBlock)
                                connman->PushMessage(pto, msgMaker.Make(nSendFlags, NetMsgType::CMPCTBLOCK, *most_recent_compact_block));
                            else {
                                CBlockHeaderAndShortTxIDs cmpctblock(*most_recent_block, state.fWantsCmpctWitness);
                                connman->PushMessage(pto, msgMaker.Make(nSendFlags, NetMsgType::CMPCTBLOCK, cmpctblock));
                            }
                            fGotBlockFromCache = true;
                        }
                    }
                    if (!fGotBlockFromCache) {
                        CBlock block;
                        bool ret = ReadBlockFromDisk(block, pBestIndex, consensusParams);
                        assert(ret);
                        CBlockHeaderAndShortTxIDs cmpctblock(block, state.fWantsCmpctWitness);
                        connman->PushMessage(pto, msgMaker.Make(nSendFlags, NetMsgType::CMPCTBLOCK, cmpctblock));
                    }
                    state.pindexBestHeaderSent = pBestIndex;
                } else if (state.fPreferHeaders) {
                    if (vHeaders.size() > 1) {
                        LogPrint(BCLog::NET, "%s: %u headers, range (%s, %s), to peer=%d\n", __func__,
                                vHeaders.size(),
                                vHeaders.front().GetHash().ToString(),
                                vHeaders.back().GetHash().ToString(), pto->GetId());
                    } else {
                        LogPrint(BCLog::NET, "%s: sending header %s to peer=%d\n", __func__,
                                vHeaders.front().GetHash().ToString(), pto->GetId());
                    }
                    connman->PushMessage(pto, msgMaker.Make(NetMsgType::HEADERS, vHeaders));
                    state.pindexBestHeaderSent = pBestIndex;
                } else
                    fRevertToInv = true;
            }
            if (fRevertToInv) {
//如果回到使用inv，只需尝试inv小费。
//vblockhashestoanounce中的最后一个条目是我们的提示
//在过去。
                if (!pto->vBlockHashesToAnnounce.empty()) {
                    const uint256 &hashToAnnounce = pto->vBlockHashesToAnnounce.back();
                    const CBlockIndex* pindex = LookupBlockIndex(hashToAnnounce);
                    assert(pindex);

//如果我们要通知一个不在主链上的块，则发出警告。
//这应该是非常罕见的，可以优化。
//现在就登陆吧。
                    if (chainActive[pindex->nHeight] != pindex) {
                        LogPrint(BCLog::NET, "Announcing block %s not on main chain (tip=%s)\n",
                            hashToAnnounce.ToString(), chainActive.Tip()->GetBlockHash().ToString());
                    }

//如果同伴的链上有这个块，不要把它倒回去。
                    if (!PeerHasHeader(&state, pindex)) {
                        pto->PushInventory(CInv(MSG_BLOCK, hashToAnnounce));
                        LogPrint(BCLog::NET, "%s: sending inv peer=%d hash=%s\n", __func__,
                            pto->GetId(), hashToAnnounce.ToString());
                    }
                }
            }
            pto->vBlockHashesToAnnounce.clear();
        }

//
//消息：库存
//
        std::vector<CInv> vInv;
        {
            LOCK(pto->cs_inventory);
            vInv.reserve(std::max<size_t>(pto->vInventoryBlockToSend.size(), INVENTORY_BROADCAST_MAX));

//添加块
            for (const uint256& hash : pto->vInventoryBlockToSend) {
                vInv.push_back(CInv(MSG_BLOCK, hash));
                if (vInv.size() == MAX_INV_SZ) {
                    connman->PushMessage(pto, msgMaker.Make(NetMsgType::INV, vInv));
                    vInv.clear();
                }
            }
            pto->vInventoryBlockToSend.clear();

//检查是否应定期发送
            bool fSendTrickle = pto->fWhitelisted;
            if (pto->nNextInvSend < nNow) {
                fSendTrickle = true;
                if (pto->fInbound) {
                    pto->nNextInvSend = connman->PoissonNextSendInbound(nNow, INVENTORY_BROADCAST_INTERVAL);
                } else {
//对于出站对等机，使用一半的延迟，因为他们对隐私的关注较少。
                    pto->nNextInvSend = PoissonNextSend(nNow, INVENTORY_BROADCAST_INTERVAL >> 1);
                }
            }

//发送时间，但对等方已请求我们不中继事务。
            if (fSendTrickle) {
                LOCK(pto->cs_filter);
                if (!pto->fRelayTxes) pto->setInventoryTxToSend.clear();
            }

//响应BIP35内存池请求
            if (fSendTrickle && pto->fSendMempool) {
                auto vtxinfo = mempool.infoAll();
                pto->fSendMempool = false;
                CAmount filterrate = 0;
                {
                    LOCK(pto->cs_feeFilter);
                    filterrate = pto->minFeeFilter;
                }

                LOCK(pto->cs_filter);

                for (const auto& txinfo : vtxinfo) {
                    const uint256& hash = txinfo.tx->GetHash();
                    CInv inv(MSG_TX, hash);
                    pto->setInventoryTxToSend.erase(hash);
                    if (filterrate) {
                        if (txinfo.feeRate.GetFeePerK() < filterrate)
                            continue;
                    }
                    if (pto->pfilter) {
                        if (!pto->pfilter->IsRelevantAndUpdate(*txinfo.tx)) continue;
                    }
                    pto->filterInventoryKnown.insert(hash);
                    vInv.push_back(inv);
                    if (vInv.size() == MAX_INV_SZ) {
                        connman->PushMessage(pto, msgMaker.Make(NetMsgType::INV, vInv));
                        vInv.clear();
                    }
                }
                pto->timeLastMempoolReq = GetTime();
            }

//确定要中继的事务
            if (fSendTrickle) {
//生成一个包含所有要发送的候选者的向量
                std::vector<std::set<uint256>::iterator> vInvTx;
                vInvTx.reserve(pto->setInventoryTxToSend.size());
                for (std::set<uint256>::iterator it = pto->setInventoryTxToSend.begin(); it != pto->setInventoryTxToSend.end(); it++) {
                    vInvTx.push_back(it);
                }
                CAmount filterrate = 0;
                {
                    LOCK(pto->cs_feeFilter);
                    filterrate = pto->minFeeFilter;
                }
//拓扑和费用率排序的库存，我们发送的隐私和优先权的原因。
//使用堆的目的是，如果只发送少数项，则并非所有项都需要排序。
                CompareInvMempoolOrder compareInvMempoolOrder(&mempool);
                std::make_heap(vInvTx.begin(), vInvTx.end(), compareInvMempoolOrder);
//没有理由在网络容量的很多时候耗尽资源，
//尤其是因为我们有很多同龄人，有些人的延误会短得多。
                unsigned int nRelayedTransactions = 0;
                LOCK(pto->cs_filter);
                while (!vInvTx.empty() && nRelayedTransactions < INVENTORY_BROADCAST_MAX) {
//从堆中提取顶层元素
                    std::pop_heap(vInvTx.begin(), vInvTx.end(), compareInvMempoolOrder);
                    std::set<uint256>::iterator it = vInvTx.back();
                    vInvTx.pop_back();
                    uint256 hash = *it;
//将其从要发送的集中删除
                    pto->setInventoryTxToSend.erase(it);
//检查是否已不在筛选器中
                    if (pto->filterInventoryKnown.contains(hash)) {
                        continue;
                    }
//不再在记忆池里了？别费心寄了。
                    auto txinfo = mempool.info(hash);
                    if (!txinfo.tx) {
                        continue;
                    }
                    if (filterrate && txinfo.feeRate.GetFeePerK() < filterrate) {
                        continue;
                    }
                    if (pto->pfilter && !pto->pfilter->IsRelevantAndUpdate(*txinfo.tx)) continue;
//发送
                    vInv.push_back(CInv(MSG_TX, hash));
                    nRelayedTransactions++;
                    {
//使旧中继消息过期
                        while (!vRelayExpiration.empty() && vRelayExpiration.front().first < nNow)
                        {
                            mapRelay.erase(vRelayExpiration.front().second);
                            vRelayExpiration.pop_front();
                        }

                        auto ret = mapRelay.insert(std::make_pair(hash, std::move(txinfo.tx)));
                        if (ret.second) {
                            vRelayExpiration.push_back(std::make_pair(nNow + 15 * 60 * 1000000, ret.first));
                        }
                    }
                    if (vInv.size() == MAX_INV_SZ) {
                        connman->PushMessage(pto, msgMaker.Make(NetMsgType::INV, vInv));
                        vInv.clear();
                    }
                    pto->filterInventoryKnown.insert(hash);
                }
            }
        }
        if (!vInv.empty())
            connman->PushMessage(pto, msgMaker.Make(NetMsgType::INV, vInv));

//检测我们是否在拖延
        nNow = GetTimeMicros();
        if (state.nStallingSince && state.nStallingSince < nNow - 1000000 * BLOCK_STALLING_TIMEOUT) {
//仅当块下载窗口无法移动时才会触发暂停。在正常稳定状态下，
//下载窗口应该比要下载的块集大很多，因此断开连接
//只应在初始块下载期间发生。
            LogPrintf("Peer=%d is stalling block download, disconnecting\n", pto->GetId());
            pto->fDisconnect = true;
            return true;
        }
//如果有一个块从该对等机飞行了2+0.5*n倍的块间隔
//（n是从中下载验证块的对等数），由于超时而断开连接。
//我们补偿其他同行，以防止由于我们自己的下游链接而杀死同行。
//饱和。我们只计算经过验证的飞行中的块，这样对等方就不能公布不存在的块散列。
//不合理地增加我们的超时时间。
        if (state.vBlocksInFlight.size() > 0) {
            QueuedBlock &queuedBlock = state.vBlocksInFlight.front();
            int nOtherPeersWithValidatedDownloads = nPeersWithValidatedDownloads - (state.nBlocksInFlightValidHeaders > 0);
            if (nNow > state.nDownloadingSince + consensusParams.nPowTargetSpacing * (BLOCK_DOWNLOAD_TIMEOUT_BASE + BLOCK_DOWNLOAD_TIMEOUT_PER_PEER * nOtherPeersWithValidatedDownloads)) {
                LogPrintf("Timeout downloading block %s from peer=%d, disconnecting\n", queuedBlock.hash.ToString(), pto->GetId());
                pto->fDisconnect = true;
                return true;
            }
        }
//检查邮件头同步超时
        if (state.fSyncStarted && state.nHeadersSyncTimeout < std::numeric_limits<int64_t>::max()) {
//检测这是否是停止的初始头同步对等
            if (pindexBestHeader->GetBlockTime() <= GetAdjustedTime() - 24*60*60) {
                if (nNow > state.nHeadersSyncTimeout && nSyncStarted == 1 && (nPreferredDownload - state.fPreferredDownload >= 1)) {
//断开（非白名单）对等如果它是我们唯一的同步对等，
//我们可以用其他的代替。
//注意：如果我们所有的同行都是入站的，那么我们不会
//断开同步对等机的连接以停止；我们有更大的
//如果我们找不到任何出站对等机，就会出现问题。
                    if (!pto->fWhitelisted) {
                        LogPrintf("Timeout downloading headers from peer=%d, disconnecting\n", pto->GetId());
                        pto->fDisconnect = true;
                        return true;
                    } else {
                        LogPrintf("Timeout downloading headers from whitelisted peer=%d, not disconnecting\n", pto->GetId());
//重置邮件头同步状态，以便
//尝试从另一个对等机下载的机会。
//注意：这还将导致至少一个以上
//要发送到的GetHeaders消息
//这个同伴（最终）。
                        state.fSyncStarted = false;
                        nSyncStarted--;
                        state.nHeadersSyncTimeout = 0;
                    }
                }
            } else {
//在我们抓到一次后，重新设置超时，这样我们就不能触发
//稍后断开。
                state.nHeadersSyncTimeout = std::numeric_limits<int64_t>::max();
            }
        }

//检查出站对等机是否具有合理的链
//getTime（）被这个反DOS逻辑使用，所以我们可以使用mocktime测试它。
        ConsiderEviction(pto, GetTime());

//
//消息：GetData（块）
//
        std::vector<CInv> vGetData;
        if (!pto->fClient && ((fFetch && !pto->m_limited_node) || !IsInitialBlockDownload()) && state.nBlocksInFlight < MAX_BLOCKS_IN_TRANSIT_PER_PEER) {
            std::vector<const CBlockIndex*> vToDownload;
            NodeId staller = -1;
            FindNextBlocksToDownload(pto->GetId(), MAX_BLOCKS_IN_TRANSIT_PER_PEER - state.nBlocksInFlight, vToDownload, staller, consensusParams);
            for (const CBlockIndex *pindex : vToDownload) {
                uint32_t nFetchFlags = GetFetchFlags(pto);
                vGetData.push_back(CInv(MSG_BLOCK | nFetchFlags, pindex->GetBlockHash()));
                MarkBlockAsInFlight(pto->GetId(), pindex->GetBlockHash(), pindex);
                LogPrint(BCLog::NET, "Requesting block %s (%d) peer=%d\n", pindex->GetBlockHash().ToString(),
                    pindex->nHeight, pto->GetId());
            }
            if (state.nBlocksInFlight == 0 && staller != -1) {
                if (State(staller)->nStallingSince == 0) {
                    State(staller)->nStallingSince = nNow;
                    LogPrint(BCLog::NET, "Stall started peer=%d\n", staller);
                }
            }
        }

//
//消息：GetData（非块）
//
        while (!pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow)
        {
            const CInv& inv = (*pto->mapAskFor.begin()).second;
            if (!AlreadyHave(inv))
            {
                LogPrint(BCLog::NET, "Requesting %s peer=%d\n", inv.ToString(), pto->GetId());
                vGetData.push_back(inv);
                if (vGetData.size() >= 1000)
                {
                    connman->PushMessage(pto, msgMaker.Make(NetMsgType::GETDATA, vGetData));
                    vGetData.clear();
                }
            } else {
//如果我们不想问，就不要期待回应。
                pto->setAskFor.erase(inv.hash);
            }
            pto->mapAskFor.erase(pto->mapAskFor.begin());
        }
        if (!vGetData.empty())
            connman->PushMessage(pto, msgMaker.Make(NetMsgType::GETDATA, vGetData));

//
//消息：feefilter
//
//如果有的话，我们不希望白名单中的同龄人过滤TXS-WhiteListforceRelay
        if (pto->nVersion >= FEEFILTER_VERSION && gArgs.GetBoolArg("-feefilter", DEFAULT_FEEFILTER) &&
            !(pto->fWhitelisted && gArgs.GetBoolArg("-whitelistforcerelay", DEFAULT_WHITELISTFORCERELAY))) {
            CAmount currentFilter = mempool.GetMinFee(gArgs.GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000).GetFeePerK();
            int64_t timeNow = GetTimeMicros();
            if (timeNow > pto->nextSendTimeFeeFilter) {
                static CFeeRate default_feerate(DEFAULT_MIN_RELAY_TX_FEE);
                static FeeFilterRounder filterRounder(default_feerate);
                CAmount filterToSend = filterRounder.round(currentFilter);
//我们总是有至少minrelaytxfee的费用筛选
                filterToSend = std::max(filterToSend, ::minRelayTxFee.GetFeePerK());
                if (filterToSend != pto->lastSentFeeFilter) {
                    connman->PushMessage(pto, msgMaker.Make(NetMsgType::FEEFILTER, filterToSend));
                    pto->lastSentFeeFilter = filterToSend;
                }
                pto->nextSendTimeFeeFilter = PoissonNextSend(timeNow, AVG_FEEFILTER_BROADCAST_INTERVAL);
            }
//如果费用过滤器发生了实质性的变化，并且仍然超过了最大的过滤器更改延迟
//在计划的广播之前，然后将广播移动到max_feefilter_change_delay内。
            else if (timeNow + MAX_FEEFILTER_CHANGE_DELAY * 1000000 < pto->nextSendTimeFeeFilter &&
                     (currentFilter < 3 * pto->lastSentFeeFilter / 4 || currentFilter > 4 * pto->lastSentFeeFilter / 3)) {
                pto->nextSendTimeFeeFilter = timeNow + GetRandInt(MAX_FEEFILTER_CHANGE_DELAY) * 1000000;
            }
        }
    }
    return true;
}

class CNetProcessingCleanup
{
public:
    CNetProcessingCleanup() {}
    ~CNetProcessingCleanup() {
//孤立事务
        mapOrphanTransactions.clear();
        mapOrphanTransactionsByPrev.clear();
    }
} instance_of_cnetprocessingcleanup;
