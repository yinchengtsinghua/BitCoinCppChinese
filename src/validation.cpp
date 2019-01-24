
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

#include <validation.h>

#include <arith_uint256.h>
#include <chain.h>
#include <chainparams.h>
#include <checkpoints.h>
#include <checkqueue.h>
#include <consensus/consensus.h>
#include <consensus/merkle.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <cuckoocache.h>
#include <hash.h>
#include <index/txindex.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <policy/rbf.h>
#include <pow.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <random.h>
#include <reverse_iterator.h>
#include <script/script.h>
#include <script/sigcache.h>
#include <script/standard.h>
#include <shutdown.h>
#include <timedata.h>
#include <tinyformat.h>
#include <txdb.h>
#include <txmempool.h>
#include <ui_interface.h>
#include <undo.h>
#include <util/system.h>
#include <util/moneystr.h>
#include <util/strencodings.h>
#include <validationinterface.h>
#include <warnings.h>

#include <future>
#include <sstream>

#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>

#if defined(NDEBUG)
# error "Bitcoin cannot be compiled without assertions."
#endif

#define MICRO 0.000001
#define MILLI 0.001

/*
 ＊全球状态
 **/

namespace {
    struct CBlockIndexWorkComparator
    {
        bool operator()(const CBlockIndex *pa, const CBlockIndex *pb) const {
//首先按总工作量排序，…
            if (pa->nChainWork > pb->nChainWork) return false;
            if (pa->nChainWork < pb->nChainWork) return true;

//…然后在收到的最早时间，…
            if (pa->nSequenceId < pb->nSequenceId) return false;
            if (pa->nSequenceId > pb->nSequenceId) return true;

//使用指针地址作为连接断路器（应仅与块一起发生
//从磁盘加载，因为它们都具有ID 0）。
            if (pa < pb) return false;
            if (pa > pb) return true;

//相同的块。
            return false;
        }
    };
} //非命名空间

enum DisconnectResult
{
DISCONNECT_OK,      //一切都好。
DISCONNECT_UNCLEAN, //回滚，但utxo集与块不一致。
DISCONNECT_FAILED   //还有什么问题。
};

class ConnectTrace;

/*
 *cchainstate存储并提供API，以更新我们的本地知识
 *当前最佳链和标题树。
 *
 *它通常提供对当前块树的访问以及函数
 *提供新的数据，并将其适当验证和纳入
 *必要时的状态。
 *
 *最终，这里的API的目标是作为
 *可消费的libconsument库，因此添加的任何函数只能调用
 *其他类成员函数，共识其他部分的纯函数
 *库，通过验证接口回调，或读/写磁盘
 *函数（最终也将通过回调实现）。
 **/

class CChainState {
private:
    /*
     *具有块有效事务的所有cblockindex项的集合（对于其本身和所有祖先），以及
     *和我们目前的小费一样好或更好。但是，条目可能失败，修剪节点可能
     *缺少块的数据。
     **/

    std::set<CBlockIndex*, CBlockIndexWorkComparator> setBlockIndexCandidates;

    /*
     *每个接收到的块都被分配了一个唯一且递增的标识符，因此我们
     *知道哪一个在叉子的情况下优先。
     **/

    CCriticalSection cs_nBlockSequenceId;
    /*从磁盘加载的块被分配为ID 0，因此从1开始计数器。*/
    int32_t nBlockSequenceId = 1;
    /*递减计数器（用于后续的preciousBlock调用）。*/
    int32_t nBlockReverseSequenceId = -1;
    /*已应用PreciousBlock的最后一个块的链。*/
    arith_uint256 nLastPreciousChainwork = 0;

    /*为了有效地跟踪报头的无效性，我们保留了
      *我们尝试连接的块在这里无效（即
      *设置为阻止\失败\自上次重新启动后有效）。那么我们就可以
      *浏览此集合并检查新标题是否是
      *这一套，防止我们在尝试时不得不走mapblockindex
      *连接坏块失败。
      *
      *虽然这比标记所有下降的物体更复杂
      *从一个无效的块中，我们发现它是无效的
      *无效，这样做需要遍历所有mapblockindex才能找到所有
      ＊后代。因为这个案子应该很少见
      *block_failed_一组中的有效块应该是正常的，工作方式应该和
      *嗯。
      *
      *因为我们已经在启动时按高度顺序运行mapblockindex，所以我们开始
      *提前并将无效块的后代标记为当时的failed_child，
      *而不是把东西放在这个集合中。
      **/

    std::set<CBlockIndex*> m_failed_blocks;

    /*
     *链状态关键部分
     *修改此链状态时必须持有的锁-在ActivateBestChain（）中持有
     **/

    CCriticalSection m_cs_chainstate;

public:
    CChain chainActive;
    BlockMap mapBlockIndex;
    std::multimap<CBlockIndex*, CBlockIndex*> mapBlocksUnlinked;
    CBlockIndex *pindexBestInvalid = nullptr;

    bool LoadBlockIndex(const Consensus::Params& consensus_params, CBlockTreeDB& blocktree) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    bool ActivateBestChain(CValidationState &state, const CChainParams& chainparams, std::shared_ptr<const CBlock> pblock);

    /*
     *如果尚未看到块头，请调用其上的checkblockheader，确保
     *它不会从无效块下降，然后将其添加到mapblockindex。
     **/

    bool AcceptBlockHeader(const CBlockHeader& block, CValidationState& state, const CChainParams& chainparams, CBlockIndex** ppindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    bool AcceptBlock(const std::shared_ptr<const CBlock>& pblock, CValidationState& state, const CChainParams& chainparams, CBlockIndex** ppindex, bool fRequested, const CDiskBlockPos* dbp, bool* fNewBlock) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//在给定视图上阻止（dis）连接：
    DisconnectResult DisconnectBlock(const CBlock& block, const CBlockIndex* pindex, CCoinsViewCache& view);
    bool ConnectBlock(const CBlock& block, CValidationState& state, CBlockIndex* pindex,
                      CCoinsViewCache& view, const CChainParams& chainparams, bool fJustCheck = false) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//在我们的pcoinstip上阻止断开连接：
    bool DisconnectTip(CValidationState& state, const CChainParams& chainparams, DisconnectedBlockTransactions* disconnectpool) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//手动块有效性操作：
    bool PreciousBlock(CValidationState& state, const CChainParams& params, CBlockIndex* pindex) LOCKS_EXCLUDED(cs_main);
    bool InvalidateBlock(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    void ResetBlockFailureFlags(CBlockIndex* pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    bool ReplayBlocks(const CChainParams& params, CCoinsView* view);
    bool RewindBlockIndex(const CChainParams& params);
    bool LoadGenesisBlock(const CChainParams& chainparams);

    void PruneBlockIndexCandidates();

    void UnloadBlockIndex();

private:
    bool ActivateBestChainStep(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexMostWork, const std::shared_ptr<const CBlock>& pblock, bool& fInvalidFound, ConnectTrace& connectTrace) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    bool ConnectTip(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexNew, const std::shared_ptr<const CBlock>& pblock, ConnectTrace& connectTrace, DisconnectedBlockTransactions &disconnectpool) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    CBlockIndex* AddToBlockIndex(const CBlockHeader& block) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    /*为给定的块哈希创建新的块索引项*/
    CBlockIndex* InsertBlockIndex(const uint256& hash) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    /*
     *对块索引的状态做出各种断言。
     *
     *默认情况下，仅在使用regtest链时完全执行；请参见：fcheckblockindex。
     **/

    void CheckBlockIndex(const Consensus::Params& consensusParams);

    void InvalidBlockFound(CBlockIndex *pindex, const CValidationState &state) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    CBlockIndex* FindMostWorkChain() EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    void ReceivedBlockTransactions(const CBlock& block, CBlockIndex* pindexNew, const CDiskBlockPos& pos, const Consensus::Params& consensusParams) EXCLUSIVE_LOCKS_REQUIRED(cs_main);


    bool RollforwardBlock(const CBlockIndex* pindex, CCoinsViewCache& inputs, const CChainParams& params) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
} g_chainstate;

/*
 *互斥保护对验证特定变量的访问，如读取
 *或更改链状态。
 *
 *在更新事务池时，这也可能需要被锁定，例如在
 *接受MORYPOOL。有关详细信息，请参阅ctxmempol:：cs comment。
 *
 *事务池有一个单独的锁，允许从它和
 *同时链状态。
 **/

RecursiveMutex cs_main;

BlockMap& mapBlockIndex = g_chainstate.mapBlockIndex;
CChain& chainActive = g_chainstate.chainActive;
CBlockIndex *pindexBestHeader = nullptr;
Mutex g_best_block_mutex;
std::condition_variable g_best_block_cv;
uint256 g_best_block;
int nScriptCheckThreads = 0;
std::atomic_bool fImporting(false);
std::atomic_bool fReindex(false);
bool fHavePruned = false;
bool fPruneMode = false;
bool fIsBareMultisigStd = DEFAULT_PERMIT_BAREMULTISIG;
bool fRequireStandard = true;
bool fCheckBlockIndex = false;
bool fCheckpointsEnabled = DEFAULT_CHECKPOINTS_ENABLED;
size_t nCoinCacheUsage = 5000 * 300;
uint64_t nPruneTarget = 0;
int64_t nMaxTipAge = DEFAULT_MAX_TIP_AGE;
bool fEnableReplacement = DEFAULT_ENABLE_REPLACEMENT;

uint256 hashAssumeValid;
arith_uint256 nMinimumChainWork;

CFeeRate minRelayTxFee = CFeeRate(DEFAULT_MIN_RELAY_TX_FEE);
CAmount maxTxFee = DEFAULT_TRANSACTION_MAXFEE;

CBlockPolicyEstimator feeEstimator;
CTxMemPool mempool(&feeEstimator);
std::atomic_bool g_is_mempool_loaded{false};

/*我们为CoinBase交易创建的常量：*/
CScript COINBASE_FLAGS;

const std::string strMessageMagic = "Bitcoin Signed Message:\n";

//内部物质
namespace {
    CBlockIndex *&pindexBestInvalid = g_chainstate.pindexBestInvalid;

    /*所有对A->B，其中A（或其祖先之一）错过事务，但B有事务。
     *修剪后的节点可能有B丢失数据的条目。
     **/

    std::multimap<CBlockIndex*, CBlockIndex*>& mapBlocksUnlinked = g_chainstate.mapBlocksUnlinked;

    CCriticalSection cs_LastBlockFile;
    std::vector<CBlockFileInfo> vinfoBlockFile;
    int nLastBlockFile = 0;
    /*全局标志，指示我们应检查是否存在
     *阻止/撤消应删除的文件。启动启动
     *或者如果我们在修剪模式下分配更多的文件空间
     **/

    bool fCheckForPruning = false;

    /*脏块索引项。*/
    std::set<CBlockIndex*> setDirtyBlockIndex;

    /*脏块文件条目。*/
    std::set<int> setDirtyFileInfo;
} //非命名空间

CBlockIndex* FindForkInGlobalIndex(const CChain& chain, const CBlockLocator& locator)
{
    AssertLockHeld(cs_main);

//查找定位器和链的最新通用块-我们希望
//locator.vhave按高度降序排序。
    for (const uint256& hash : locator.vHave) {
        CBlockIndex* pindex = LookupBlockIndex(hash);
        if (pindex) {
            if (chain.Contains(pindex))
                return pindex;
            if (pindex->GetAncestor(chain.Height()) == chain.Tip()) {
                return chain.Tip();
            }
        }
    }
    return chain.Genesis();
}

std::unique_ptr<CCoinsViewDB> pcoinsdbview;
std::unique_ptr<CCoinsViewCache> pcoinsTip;
std::unique_ptr<CBlockTreeDB> pblocktree;

enum class FlushStateMode {
    NONE,
    IF_NEEDED,
    PERIODIC,
    ALWAYS
};

//见文件定义
static bool FlushStateToDisk(const CChainParams& chainParams, CValidationState &state, FlushStateMode mode, int nManualPruneHeight=0);
static void FindFilesToPruneManual(std::set<int>& setFilesToPrune, int nManualPruneHeight);
static void FindFilesToPrune(std::set<int>& setFilesToPrune, uint64_t nPruneAfterHeight);
bool CheckInputs(const CTransaction& tx, CValidationState &state, const CCoinsViewCache &inputs, bool fScriptChecks, unsigned int flags, bool cacheSigStore, bool cacheFullScriptStore, PrecomputedTransactionData& txdata, std::vector<CScriptCheck> *pvChecks = nullptr);
static FILE* OpenUndoFile(const CDiskBlockPos &pos, bool fReadOnly = false);

bool CheckFinalTx(const CTransaction &tx, int flags)
{
    AssertLockHeld(cs_main);

//按照惯例，标志的负值表示
//应使用当前网络强制的共识规则。在
//未来的软分叉方案意味着检查
//将为下一个块强制执行规则并设置
//适当的标志。目前没有软叉子
//已调度，因此未设置任何标志。
    flags = std::max(flags, 0);

//checkFinalTx（）使用chainActive.height（）+1计算
//nlockTime，因为在内部调用isFinalTx（）时
//cblock:：acceptBlock（），块的高度*为*
//评估是使用的。因此，如果我们想知道
//事务可以是*next*块的一部分，我们需要调用
//isFinalTx（）具有一个以上的chainActive.height（）。
    const int nBlockHeight = chainActive.Height() + 1;

//BIP113要求时间锁定事务的nlockTime设置为
//小于包含它们的上一个块的中间时间。
//创建下一个块时，其上一个块将是当前块
//链尖，所以我们用它来计算传递给
//如果设置了LockTime_Middle_Time_Past，则为isFinalTx（）。
    const int64_t nBlockTime = (flags & LOCKTIME_MEDIAN_TIME_PAST)
                             ? chainActive.Tip()->GetMedianTimePast()
                             : GetAdjustedTime();

    return IsFinalTx(tx, nBlockHeight, nBlockTime);
}

bool TestLockPointValidity(const LockPoints* lp)
{
    AssertLockHeld(cs_main);
    assert(lp);
//如果存在相对锁定时间，则将设置MaxinputBlock
//如果没有相对锁定时间，则锁定点不依赖于链。
    if (lp->maxInputBlock) {
//检查chainactive是否是锁定点所在块的扩展
//计算有效。如果不是，则锁定点不再有效
        if (!chainActive.Contains(lp->maxInputBlock)) {
            return false;
        }
    }

//锁定点仍然有效
    return true;
}

bool CheckSequenceLocks(const CTxMemPool& pool, const CTransaction& tx, int flags, LockPoints* lp, bool useExistingLockPoints)
{
    AssertLockHeld(cs_main);
    AssertLockHeld(pool.cs);

    CBlockIndex* tip = chainActive.Tip();
    assert(tip != nullptr);

    CBlockIndex index;
    index.pprev = tip;
//checkSequenceLocks（）使用chainActive.height（）+1计算
//基于高度的锁，因为在
//connectBlock（），块的高度*为*
//评估是使用的。
//因此，如果我们想知道交易是否可以成为
//*下一个*块，我们需要使用一个以上的chainActive.height（）。
    index.nHeight = tip->nHeight + 1;

    std::pair<int, int64_t> lockPair;
    if (useExistingLockPoints) {
        assert(lp);
        lockPair.first = lp->height;
        lockPair.second = lp->time;
    }
    else {
//pcoinstip包含chainActive.tip（）的utxo集
        CCoinsViewMemPool viewMemPool(pcoinsTip.get(), pool);
        std::vector<int> prevheights;
        prevheights.resize(tx.vin.size());
        for (size_t txinIndex = 0; txinIndex < tx.vin.size(); txinIndex++) {
            const CTxIn& txin = tx.vin[txinIndex];
            Coin coin;
            if (!viewMemPool.GetCoin(txin.prevout, coin)) {
                return error("%s: Missing input", __func__);
            }
            if (coin.nHeight == MEMPOOL_HEIGHT) {
//假设所有mempool事务在下一个块中确认
                prevheights[txinIndex] = tip->nHeight + 1;
            } else {
                prevheights[txinIndex] = coin.nHeight;
            }
        }
        lockPair = CalculateSequenceLocks(tx, flags, &prevheights, index);
        if (lp) {
            lp->height = lockPair.first;
            lp->time = lockPair.second;
//同时存储块的哈希值，其最大高度为
//所有具有序列锁定预防功能的块。
//这个散列需要仍然在链上
//使这些锁定点计算有效
//注：无法正确计算最大输出块
//如果任何序列锁定输入依赖于未确认的TxS，
//除非在特殊情况下，相对锁定时间/高度
//为0，相当于没有序列锁。既然我们假设
//输入MEMPOOL TXS的TIP+1高度并测试结果
//CalculateSequenceLocks对Tip+1的锁对。我们知道
//如果存在非零序，EvaluateSequenceLocks将失败
//锁定一个mempool输入，这样我们可以使用
//检查SequenceLocks以指示锁定点的有效性
            int maxInputHeight = 0;
            for (const int height : prevheights) {
//可以忽略mempool输入，因为如果它们具有非零锁，我们将失败
                if (height != tip->nHeight+1) {
                    maxInputHeight = std::max(maxInputHeight, height);
                }
            }
            lp->maxInputBlock = tip->GetAncestor(maxInputHeight);
        }
    }
    return EvaluateSequenceLocks(index, lockPair);
}

//返回应检查给定块的脚本标志
static unsigned int GetBlockScriptFlags(const CBlockIndex* pindex, const Consensus::Params& chainparams);

static void LimitMempoolSize(CTxMemPool& pool, size_t limit, unsigned long age) {
    int expired = pool.Expire(GetTime() - age);
    if (expired != 0) {
        LogPrint(BCLog::MEMPOOL, "Expired %i transactions from the memory pool\n", expired);
    }

    std::vector<COutPoint> vNoSpendsRemaining;
    pool.TrimToSize(limit, &vNoSpendsRemaining);
    for (const COutPoint& removed : vNoSpendsRemaining)
        pcoinsTip->Uncache(removed);
}

/*将cvalidationState转换为人类可读的日志消息*/
std::string FormatStateMessage(const CValidationState &state)
{
    return strprintf("%s%s (code %i)",
        state.GetRejectReason(),
        state.GetDebugMessage().empty() ? "" : ", "+state.GetDebugMessage(),
        state.GetRejectCode());
}

static bool IsCurrentForFeeEstimation() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
    if (IsInitialBlockDownload())
        return false;
    if (chainActive.Tip()->GetBlockTime() < (GetTime() - MAX_FEE_ESTIMATION_TIP_AGE))
        return false;
    if (chainActive.Height() < pindexBestHeader->nHeight - 1)
        return false;
    return true;
}

/*通过重新添加或递归删除REORG后使mempool保持一致
 *断开与mempool的块事务连接，同时删除
 *由于新的
 *尖端/高度。
 *
 *注意：我们假设disconnectpool只包含不包含
 *已在当前链中确认，也未在mempool中确认（否则，
 *在mempool中，此类事务的后代将被删除）。
 *
 *传递faddtommopol=false将跳过尝试重新添加事务的过程，
 *只需根据需要从内存池中删除即可。
 **/


static void UpdateMempoolForReorg(DisconnectedBlockTransactions &disconnectpool, bool fAddToMempool) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
    std::vector<uint256> vHashUpdate;
//disconnectpool的插入顺序索引将条目从
//从最旧到最新，但最旧的条目将是
//断开连接的最新开采区块。
//反向迭代disconnectpool，以便添加事务
//回到mempool，从
//以前在一个街区看到过。
    auto it = disconnectpool.queuedTx.get<insertion_order>().rbegin();
    while (it != disconnectpool.queuedTx.get<insertion_order>().rend()) {
//忽略重新恢复的事务中的验证错误
        CValidationState stateDummy;
        if (!fAddToMempool || (*it)->IsCoinBase() ||
            /*cepttommorypool（mempool，statedummy，*it，nullptr/*pfmissinginputs*/，
                                nullptr/*pltxnreplaced（空指针/*pltxnreplaced）*/, true /* bypass_limits */, 0 /* nAbsurdFee */)) {

//如果事务没有进入内存池，则删除任何
//依赖它的事务（现在将是孤立的）。
            mempool.removeRecursive(**it, MemPoolRemovalReason::REORG);
        } else if (mempool.exists((*it)->GetHash())) {
            vHashUpdate.push_back((*it)->GetHash());
        }
        ++it;
    }
    disconnectpool.queuedTx.clear();
//acceptToMoryPool/addUnchecked all假定新的mempool条目
//不在mempool子项中，这在添加时通常不正确
//以前确认的事务返回到mempool。
//UpdateTransactionFromBlock查找中任何事务的后代
//重新添加并清除MemPool状态的断开连接池。
    mempool.UpdateTransactionsFromBlock(vHashUpdate);

//我们还需要取消任何现在还不成熟的交易
    mempool.removeForReorg(pcoinsTip.get(), chainActive.Tip()->nHeight + 1, STANDARD_LOCKTIME_VERIFY_FLAGS);
//重新限制内存池大小，以防添加任何事务
    LimitMempoolSize(mempool, gArgs.GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000, gArgs.GetArg("-mempoolexpiry", DEFAULT_MEMPOOL_EXPIRY) * 60 * 60);
}

//用于避免mempool污染共识关键路径，如果ccoinsviewmempool
//不知怎么地被破坏了，返回了错误的scriptpubkeys
static bool CheckInputsFromMempoolAndCache(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& view, const CTxMemPool& pool,
                 unsigned int flags, bool cacheSigStore, PrecomputedTransactionData& txdata) EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
    AssertLockHeld(cs_main);

//pool.cs应该已经被锁定了，但是继续在这里重新锁定
//当我们检查视图时，执行mempool不会在
//当我们实际调用检查输入时
    LOCK(pool.cs);

    assert(!tx.IsCoinBase());
    for (const CTxIn& txin : tx.vin) {
        const Coin& coin = view.AccessCoin(txin.prevout);

//在这一点上，我们还没有真正检查硬币是否全部
//可用（或者不应该假设我们有，因为checkinputs有）。
//所以，如果输入在这里不可用，我们就返回失败，
//然后只需要检查可用输入的等价性。
        if (coin.IsSpent()) return false;

        const CTransactionRef& txFrom = pool.get(txin.prevout.hash);
        if (txFrom) {
            assert(txFrom->GetHash() == txin.prevout.hash);
            assert(txFrom->vout.size() > txin.prevout.n);
            assert(txFrom->vout[txin.prevout.n] == coin.out);
        } else {
            const Coin& coinFromDisk = pcoinsTip->AccessCoin(txin.prevout);
            assert(!coinFromDisk.IsSpent());
            assert(coinFromDisk.out == coin.out);
        }
    }

    return CheckInputs(tx, state, view, true, flags, cacheSigStore, true, txdata);
}

static bool AcceptToMemoryPoolWorker(const CChainParams& chainparams, CTxMemPool& pool, CValidationState& state, const CTransactionRef& ptx,
                              bool* pfMissingInputs, int64_t nAcceptTime, std::list<CTransactionRef>* plTxnReplaced,
                              bool bypass_limits, const CAmount& nAbsurdFee, std::vector<COutPoint>& coins_to_uncache, bool test_accept) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    const CTransaction& tx = *ptx;
    const uint256 hash = tx.GetHash();
    AssertLockHeld(cs_main);
LOCK(pool.cs); //mempool“read lock”（通过getmainsignals（）.transactionaddedtomempool（）持有）
    if (pfMissingInputs) {
        *pfMissingInputs = false;
    }

    if (!CheckTransaction(tx, state))
return false; //支票交易记录填写的状态

//CoinBase仅在块中有效，而不是作为松散事务。
    if (tx.IsCoinBase())
        return state.DoS(100, false, REJECT_INVALID, "coinbase");

//而不是处理非标准事务（除非-testnet/-regtest）
    std::string reason;
    if (fRequireStandard && !IsStandardTx(tx, reason))
        return state.DoS(0, false, REJECT_NONSTANDARD, reason);

//不要处理太小的事务。
//具有1个segwit输入和1个p2wphk输出的事务的非见证大小为82字节。
//小于此值的事务不会中继以减少不必要的malloc开销。
    if (::GetSerializeSize(tx, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) < MIN_STANDARD_TX_NONWITNESS_SIZE)
        return state.DoS(0, false, REJECT_NONSTANDARD, "tx-size-small");

//只接受使用可以在下一个挖掘的事务的nlocktime
//阻止；我们不希望我们的内存池中充满无法
//尚未开采。
    if (!CheckFinalTx(tx, STANDARD_LOCKTIME_VERIFY_FLAGS))
        return state.DoS(0, false, REJECT_NONSTANDARD, "non-final");

//它已经在内存池中了吗？
    if (pool.exists(hash)) {
        return state.Invalid(false, REJECT_DUPLICATE, "txn-already-in-mempool");
    }

//检查是否与内存事务冲突
    std::set<uint256> setConflicts;
    for (const CTxIn &txin : tx.vin)
    {
        const CTransaction* ptxConflicting = pool.GetConflictTx(txin.prevout);
        if (ptxConflicting) {
            if (!setConflicts.count(ptxConflicting->GetHash()))
            {
//通过设置允许退出事务替换
//所有输入上的序列>max_bip125_rbf_sequence（sequence_final-2）。
//
//选择序列“final-1”仍允许使用nlocktime
//不可替换交易。所有输入而不仅仅是一个
//是为了多方协议，我们没有
//希望单个参与方能够禁用替换。
//
//选择退出会忽略后代，因为任何人都依赖
//第一次看到的mempool行为应检查所有
//不管怎样，未确认的祖先；否则做是无望的。
//不安全的。
                bool fReplacementOptOut = true;
                if (fEnableReplacement)
                {
                    for (const CTxIn &_txin : ptxConflicting->vin)
                    {
                        if (_txin.nSequence <= MAX_BIP125_RBF_SEQUENCE)
                        {
                            fReplacementOptOut = false;
                            break;
                        }
                    }
                }
                if (fReplacementOptOut) {
                    return state.Invalid(false, REJECT_DUPLICATE, "txn-mempool-conflict");
                }

                setConflicts.insert(ptxConflicting->GetHash());
            }
        }
    }

    {
        CCoinsView dummy;
        CCoinsViewCache view(&dummy);

        LockPoints lp;
        CCoinsViewMemPool viewMemPool(pcoinsTip.get(), pool);
        view.SetBackend(viewMemPool);

//是否存在所有输入？
        for (const CTxIn& txin : tx.vin) {
            if (!pcoinsTip->HaveCoinInCache(txin.prevout)) {
                coins_to_uncache.push_back(txin.prevout);
            }
            if (!view.HaveCoin(txin.prevout)) {
//输入是否丢失，因为我们已经有了TX？
                for (size_t out = 0; out < tx.vout.size(); out++) {
//乐观地说，只需高效地检查缓存的输出
                    if (pcoinsTip->HaveCoinInCache(COutPoint(hash, out))) {
                        return state.Invalid(false, REJECT_DUPLICATE, "txn-already-known");
                    }
                }
//否则假设这可能是一个孤儿德克萨斯州，我们还没有见过他们的父母。
                if (pfMissingInputs) {
                    *pfMissingInputs = true;
                }
return false; //fMissingInputs和！state.is invalid（）用于检测此条件，不要设置state.invalid（）。
            }
        }

//将最佳块放入范围
        view.GetBestBlock();

//我们现在缓存了所有的输入，所以切换回dummy，这样就不需要对mempool保持锁定。
        view.SetBackend(dummy);

//只接受可以在下一个挖掘的bip68序列锁定事务
//阻止；我们不希望我们的内存池中充满无法
//尚未开采。
//必须为此保留pool.cs，除非我们将checkSequenceLocks更改为
//CoinsView缓存而不是创建自己的
        if (!CheckSequenceLocks(pool, tx, STANDARD_LOCKTIME_VERIFY_FLAGS, &lp))
            return state.DoS(0, false, REJECT_NONSTANDARD, "non-BIP68-final");

        CAmount nFees = 0;
        if (!Consensus::CheckTxInputs(tx, state, view, GetSpendHeight(view), nFees)) {
            return error("%s: Consensus::CheckTxInputs: %s, %s", __func__, tx.GetHash().ToString(), FormatStateMessage(state));
        }

//检查输入中的非标准付薪脚本哈希
        if (fRequireStandard && !AreInputsStandard(tx, view))
            return state.Invalid(false, REJECT_NONSTANDARD, "bad-txns-nonstandard-inputs");

//检查P2WSH中的非标准见证
        if (tx.HasWitness() && fRequireStandard && !IsWitnessStandard(tx, view))
            return state.DoS(0, false, REJECT_NONSTANDARD, "bad-witness-nonstandard", true);

        int64_t nSigOpsCost = GetTransactionSigOpCost(tx, view, STANDARD_SCRIPT_VERIFY_FLAGS);

//nmodifiedfees包括优先级转移中的任何费用差额。
        CAmount nModifiedFees = nFees;
        pool.ApplyDelta(hash, nModifiedFees);

//跟踪花费了一个coinbase的交易，我们重新扫描它
//在重组过程中，以确保CoinBase_的成熟度仍然得到满足。
        bool fSpendsCoinbase = false;
        for (const CTxIn &txin : tx.vin) {
            const Coin &coin = view.AccessCoin(txin.prevout);
            if (coin.IsCoinBase()) {
                fSpendsCoinbase = true;
                break;
            }
        }

        CTxMemPoolEntry entry(ptx, nFees, nAcceptTime, chainActive.Height(),
                              fSpendsCoinbase, nSigOpsCost, lp);
        unsigned int nSize = entry.GetTxSize();

//检查该事务没有过多的
//Sigops，让我无法挖掘。因为CoinBase交易
//自身可以包含sigops max_standard_tx_sigops小于
//max_block_sigops；我们仍然认为这是无效的，而不是
//仅仅是非标准交易。
        if (nSigOpsCost > MAX_STANDARD_TX_SIGOPS_COST)
            return state.DoS(0, false, REJECT_NONSTANDARD, "bad-txns-too-many-sigops", false,
                strprintf("%d", nSigOpsCost));

        CAmount mempoolRejectFee = pool.GetMinFee(gArgs.GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000).GetFee(nSize);
        if (!bypass_limits && mempoolRejectFee > 0 && nModifiedFees < mempoolRejectFee) {
            return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "mempool min fee not met", false, strprintf("%d < %d", nModifiedFees, mempoolRejectFee));
        }

//在minrelaytxfee以下不允许任何事务，除非来自断开连接的块
        if (!bypass_limits && nModifiedFees < ::minRelayTxFee.GetFee(nSize)) {
            return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "min relay fee not met", false, strprintf("%d < %d", nModifiedFees, ::minRelayTxFee.GetFee(nSize)));
        }

        if (nAbsurdFee && nFees > nAbsurdFee)
            return state.Invalid(false,
                REJECT_HIGHFEE, "absurdly-high-fee",
                strprintf("%d > %d", nFees, nAbsurdFee));

//在mempool祖先中计算，上限为。
        CTxMemPool::setEntries setAncestors;
        size_t nLimitAncestors = gArgs.GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT);
        size_t nLimitAncestorSize = gArgs.GetArg("-limitancestorsize", DEFAULT_ANCESTOR_SIZE_LIMIT)*1000;
        size_t nLimitDescendants = gArgs.GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT);
        size_t nLimitDescendantSize = gArgs.GetArg("-limitdescendantsize", DEFAULT_DESCENDANT_SIZE_LIMIT)*1000;
        std::string errString;
        if (!pool.CalculateMemPoolAncestors(entry, setAncestors, nLimitAncestors, nLimitAncestorSize, nLimitDescendants, nLimitDescendantSize, errString)) {
            return state.DoS(0, false, REJECT_NONSTANDARD, "too-long-mempool-chain", false, errString);
        }

//花费将由其替换的输出的事务是无效的。现在
//我们拥有所有的祖先，我们可以检测到这一点。
//通过确保setconflicts和set祖先不
//横断。
        for (CTxMemPool::txiter ancestorIt : setAncestors)
        {
            const uint256 &hashAncestor = ancestorIt->GetTx().GetHash();
            if (setConflicts.count(hashAncestor))
            {
                return state.DoS(10, false,
                                 REJECT_INVALID, "bad-txns-spends-conflicting-tx", false,
                                 strprintf("%s spends conflicting transaction %s",
                                           hash.ToString(),
                                           hashAncestor.ToString()));
            }
        }

//检查挖掘此交易是否经济合理
//比它所取代的还要多。
        CAmount nConflictingFees = 0;
        size_t nConflictingSize = 0;
        uint64_t nConflictingCount = 0;
        CTxMemPool::setEntries allConflicting;

//如果我们不持有锁，所有冲突都可能是不完整的；
//后续的removestaged（）和addunchecked（）调用不保证
//Mempool对我们的一致性。
        const bool fReplacementTransaction = setConflicts.size();
        if (fReplacementTransaction)
        {
            CFeeRate newFeeRate(nModifiedFees, nSize);
            std::set<uint256> setConflictsParents;
            const int maxDescendantsToVisit = 100;
            const CTxMemPool::setEntries setIterConflicting = pool.GetIterSet(setConflicts);
            for (const auto& mi : setIterConflicting) {
//不允许更换以降低
//内存池。
//
//我们通常不想接受低
//比他们换的更便宜，因为这会降低
//下一个街区。要求他们永远
//增加也是防止
//DOS通过替换进行攻击。
//
//我们只考虑直接交易的条件
//被取代，而不是他们的间接后代。虽然如此
//在决定是否
//或者不更换，我们确实需要更换以支付更多的费用。
//总体费用也是如此，减轻了大多数情况。
                CFeeRate oldFeeRate(mi->GetModifiedFee(), mi->GetTxSize());
                if (newFeeRate <= oldFeeRate)
                {
                    return state.DoS(0, false,
                            REJECT_INSUFFICIENTFEE, "insufficient fee", false,
                            strprintf("rejecting replacement %s; new feerate %s <= old feerate %s",
                                  hash.ToString(),
                                  newFeeRate.ToString(),
                                  oldFeeRate.ToString()));
                }

                for (const CTxIn &txin : mi->GetTx().vin)
                {
                    setConflictsParents.insert(txin.prevout.hash);
                }

                nConflictingCount += mi->GetCountWithDescendants();
            }
//这可能高估了实际后代的数量。
//但我们只是想保守一点，避免做太多
//工作。
            if (nConflictingCount <= maxDescendantsToVisit) {
//如果替换的数量不多，则计算
//必须收回的交易
                for (CTxMemPool::txiter it : setIterConflicting) {
                    pool.CalculateDescendants(it, allConflicting);
                }
                for (CTxMemPool::txiter it : allConflicting) {
                    nConflictingFees += it->GetModifiedFee();
                    nConflictingSize += it->GetTxSize();
                }
            } else {
                return state.DoS(0, false,
                        REJECT_NONSTANDARD, "too many potential replacements", false,
                        strprintf("rejecting replacement %s; too many potential replacements (%d > %d)\n",
                            hash.ToString(),
                            nConflictingCount,
                            maxDescendantsToVisit));
            }

            for (unsigned int j = 0; j < tx.vin.size(); j++)
            {
//我们不想接受低成本的替代品
//先开采垃圾。理想情况下，我们会跟踪
//先祖在此基础上做出决定，
//但目前需要确认所有新的输入。
                if (!setConflictsParents.count(tx.vin[j].prevout.hash))
                {
//而不是检查utxo设置-潜在的昂贵-
//检查新输入是否指向
//德州，在记忆池里。
                    if (pool.exists(tx.vin[j].prevout.hash)) {
                        return state.DoS(0, false,
                                         REJECT_NONSTANDARD, "replacement-adds-unconfirmed", false,
                                         strprintf("replacement %s adds unconfirmed input, idx %d",
                                                  hash.ToString(), j));
                    }
                }
            }

//替换必须支付比交易更高的费用
//替换-如果我们使用那些冲突的
//交易不会被支付。
            if (nModifiedFees < nConflictingFees)
            {
                return state.DoS(0, false,
                                 REJECT_INSUFFICIENTFEE, "insufficient fee", false,
                                 strprintf("rejecting replacement %s, less fees than conflicting txs; %s < %s",
                                          hash.ToString(), FormatMoney(nModifiedFees), FormatMoney(nConflictingFees)));
            }

//最后，除了支付比冲突更多的费用外，
//新事务必须为自己的带宽付费。
            CAmount nDeltaFees = nModifiedFees - nConflictingFees;
            if (nDeltaFees < ::incrementalRelayFee.GetFee(nSize))
            {
                return state.DoS(0, false,
                        REJECT_INSUFFICIENTFEE, "insufficient fee", false,
                        strprintf("rejecting replacement %s, not enough additional fees to relay; %s < %s",
                              hash.ToString(),
                              FormatMoney(nDeltaFees),
                              FormatMoney(::incrementalRelayFee.GetFee(nSize))));
            }
        }

        constexpr unsigned int scriptVerifyFlags = STANDARD_SCRIPT_VERIFY_FLAGS;

//对照以前的交易记录检查
//最后这样做有助于防止CPU耗尽拒绝服务攻击。
        PrecomputedTransactionData txdata(tx);
        if (!CheckInputs(tx, state, view, true, scriptVerifyFlags, true, false, txdata)) {
//script_verify_cleanstack需要script_verify_见证，所以我们
//需要同时关闭这两个，并与仅关闭CleanStack进行比较
//查看故障是否是由于见证验证造成的。
CValidationState stateDummy; //希望报告的失败来自首次检查输入
            if (!tx.HasWitness() && CheckInputs(tx, stateDummy, view, true, scriptVerifyFlags & ~(SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_CLEANSTACK), true, false, txdata) &&
                !CheckInputs(tx, stateDummy, view, true, scriptVerifyFlags & ~SCRIPT_VERIFY_CLEANSTACK, true, false, txdata)) {
//只有证人失踪，所以交易本身可能会很好。
                state.SetCorruptionPossible();
            }
return false; //由checkinputs填写的状态
        }

//根据当前块提示的脚本验证再次检查
//缓存脚本执行标志的标志。当然，这是，
//如果下一个块的脚本标志与
//前一个，但是因为缓存为我们跟踪脚本标志
//会自动失效，我们会有一些额外的块
//未激活软分叉。
//
//如果标准标志中的错误导致
//当事务实际上无效时作为有效事务传递。为了
//实例strictenc标志错误地允许
//checksig不是要传递的脚本，即使它们无效。
//
//在CreateNewBlock（）中有一个类似的检查以防止创建
//无效的块（使用testblockvalidity），但是允许
//进入内存池的事务可以被利用为DoS攻击。
        unsigned int currentBlockScriptVerifyFlags = GetBlockScriptFlags(chainActive.Tip(), chainparams.GetConsensus());
        if (!CheckInputsFromMempoolAndCache(tx, state, view, pool, currentBlockScriptVerifyFlags, true, txdata)) {
            return error("%s: BUG! PLEASE REPORT THIS! CheckInputs failed against latest-block but not STANDARD flags %s, %s",
                    __func__, hash.ToString(), FormatStateMessage(state));
        }

        if (test_accept) {
//Tx已接受，但未添加
            return true;
        }

//从内存池中删除冲突事务
        for (CTxMemPool::txiter it : allConflicting)
        {
            LogPrint(BCLog::MEMPOOL, "replacing tx %s with %s for %s BTC additional fees, %d delta bytes\n",
                    it->GetTx().GetHash().ToString(),
                    hash.ToString(),
                    FormatMoney(nModifiedFees - nConflictingFees),
                    (int)nSize - (int)nConflictingSize);
            if (plTxnReplaced)
                plTxnReplaced->push_back(it->GetSharedTx());
        }
        pool.RemoveStaged(allConflicting, false, MemPoolRemovalReason::REPLACED);

//只有在以下情况下，此交易才应计入费用估算：
//-这不是BIP 125替换交易（可能不受广泛支持）
//-在绕过典型Mempool费用限制的REORG期间，不会重新添加
//-节点不在后面
//-该事务不依赖于mempool中的任何其他事务
        bool validForFeeEstimation = !fReplacementTransaction && !bypass_limits && IsCurrentForFeeEstimation() && pool.HasNoInputsOf(tx);

//将事务存储在内存中
        pool.addUnchecked(entry, setAncestors, validForFeeEstimation);

//修剪mempool并检查是否修剪了tx
        if (!bypass_limits) {
            LimitMempoolSize(pool, gArgs.GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000, gArgs.GetArg("-mempoolexpiry", DEFAULT_MEMPOOL_EXPIRY) * 60 * 60);
            if (!pool.exists(hash))
                return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "mempool full");
        }
    }

    GetMainSignals().TransactionAddedToMempool(ptx);

    return true;
}

/*（尝试）在指定的接受时间将事务添加到内存池*/
static bool AcceptToMemoryPoolWithTime(const CChainParams& chainparams, CTxMemPool& pool, CValidationState &state, const CTransactionRef &tx,
                        bool* pfMissingInputs, int64_t nAcceptTime, std::list<CTransactionRef>* plTxnReplaced,
                        bool bypass_limits, const CAmount nAbsurdFee, bool test_accept) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    std::vector<COutPoint> coins_to_uncache;
    bool res = AcceptToMemoryPoolWorker(chainparams, pool, state, tx, pfMissingInputs, nAcceptTime, plTxnReplaced, bypass_limits, nAbsurdFee, coins_to_uncache, test_accept);
    if (!res) {
        for (const COutPoint& hashTx : coins_to_uncache)
            pcoinsTip->Uncache(hashTx);
    }
//在我们有（可能）未缓存的条目之后，确保我们的硬币缓存仍然在其大小限制内。
    CValidationState stateDummy;
    FlushStateToDisk(chainparams, stateDummy, FlushStateMode::PERIODIC);
    return res;
}

bool AcceptToMemoryPool(CTxMemPool& pool, CValidationState &state, const CTransactionRef &tx,
                        bool* pfMissingInputs, std::list<CTransactionRef>* plTxnReplaced,
                        bool bypass_limits, const CAmount nAbsurdFee, bool test_accept)
{
    const CChainParams& chainparams = Params();
    return AcceptToMemoryPoolWithTime(chainparams, pool, state, tx, pfMissingInputs, GetTime(), plTxnReplaced, bypass_limits, nAbsurdFee, test_accept);
}

/*
 *返回txout中的事务，如果在一个块中找到它，则将其哈希放在hash block中。
 *如果提供blockindex，则从相应的块中提取事务。
 **/

bool GetTransaction(const uint256& hash, CTransactionRef& txOut, const Consensus::Params& consensusParams, uint256& hashBlock, bool fAllowSlow, CBlockIndex* blockIndex)
{
    CBlockIndex* pindexSlow = blockIndex;

    LOCK(cs_main);

    if (!blockIndex) {
        CTransactionRef ptx = mempool.get(hash);
        if (ptx) {
            txOut = ptx;
            return true;
        }

        if (g_txindex) {
            return g_txindex->FindTx(hash, hashBlock, txOut);
        }

if (fAllowSlow) { //使用Coin数据库查找包含事务的块并扫描它
            const Coin& coin = AccessByTxid(*pcoinsTip, hash);
            if (!coin.IsSpent()) pindexSlow = chainActive[coin.nHeight];
        }
    }

    if (pindexSlow) {
        CBlock block;
        if (ReadBlockFromDisk(block, pindexSlow, consensusParams)) {
            for (const auto& tx : block.vtx) {
                if (tx->GetHash() == hash) {
                    txOut = tx;
                    hashBlock = pindexSlow->GetBlockHash();
                    return true;
                }
            }
        }
    }

    return false;
}






//////////////////////////////////////////////////
//
//cblock和cblockindex
//

static bool WriteBlockToDisk(const CBlock& block, CDiskBlockPos& pos, const CMessageHeader::MessageStartChars& messageStart)
{
//打开要附加的历史文件
    CAutoFile fileout(OpenBlockFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("WriteBlockToDisk: OpenBlockFile failed");

//写入索引头
    unsigned int nSize = GetSerializeSize(block, fileout.GetVersion());
    fileout << messageStart << nSize;

//写入块
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("WriteBlockToDisk: ftell failed");
    pos.nPos = (unsigned int)fileOutPos;
    fileout << block;

    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CDiskBlockPos& pos, const Consensus::Params& consensusParams)
{
    block.SetNull();

//打开要读取的历史文件
    CAutoFile filein(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("ReadBlockFromDisk: OpenBlockFile failed for %s", pos.ToString());

//读块
    try {
        filein >> block;
    }
    catch (const std::exception& e) {
        return error("%s: Deserialize or I/O error - %s at %s", __func__, e.what(), pos.ToString());
    }

//检查收割台
    if (!CheckProofOfWork(block.GetHash(), block.nBits, consensusParams))
        return error("ReadBlockFromDisk: Errors in block header at %s", pos.ToString());

    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex, const Consensus::Params& consensusParams)
{
    CDiskBlockPos blockPos;
    {
        LOCK(cs_main);
        blockPos = pindex->GetBlockPos();
    }

    if (!ReadBlockFromDisk(block, blockPos, consensusParams))
        return false;
    if (block.GetHash() != pindex->GetBlockHash())
        return error("ReadBlockFromDisk(CBlock&, CBlockIndex*): GetHash() doesn't match index for %s at %s",
                pindex->ToString(), pindex->GetBlockPos().ToString());
    return true;
}

bool ReadRawBlockFromDisk(std::vector<uint8_t>& block, const CDiskBlockPos& pos, const CMessageHeader::MessageStartChars& message_start)
{
    CDiskBlockPos hpos = pos;
hpos.nPos -= 8; //向后搜索元头的8个字节
    CAutoFile filein(OpenBlockFile(hpos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        return error("%s: OpenBlockFile failed for %s", __func__, pos.ToString());
    }

    try {
        CMessageHeader::MessageStartChars blk_start;
        unsigned int blk_size;

        filein >> blk_start >> blk_size;

        if (memcmp(blk_start, message_start, CMessageHeader::MESSAGE_START_SIZE)) {
            return error("%s: Block magic mismatch for %s: %s versus expected %s", __func__, pos.ToString(),
                    HexStr(blk_start, blk_start + CMessageHeader::MESSAGE_START_SIZE),
                    HexStr(message_start, message_start + CMessageHeader::MESSAGE_START_SIZE));
        }

        if (blk_size > MAX_SIZE) {
            return error("%s: Block data is larger than maximum deserialization size for %s: %s versus %s", __func__, pos.ToString(),
                    blk_size, MAX_SIZE);
        }

block.resize(blk_size); //记忆归零是有意的。
        filein.read((char*)block.data(), blk_size);
    } catch(const std::exception& e) {
        return error("%s: Read from block file failed: %s for %s", __func__, e.what(), pos.ToString());
    }

    return true;
}

bool ReadRawBlockFromDisk(std::vector<uint8_t>& block, const CBlockIndex* pindex, const CMessageHeader::MessageStartChars& message_start)
{
    CDiskBlockPos block_pos;
    {
        LOCK(cs_main);
        block_pos = pindex->GetBlockPos();
    }

    return ReadRawBlockFromDisk(block, block_pos, message_start);
}

CAmount GetBlockSubsidy(int nHeight, const Consensus::Params& consensusParams)
{
    int halvings = nHeight / consensusParams.nSubsidyHalvingInterval;
//右移位未定义时强制块奖励为零。
    if (halvings >= 64)
        return 0;

    CAmount nSubsidy = 50 * COIN;
//补贴每21万个街区减半，大约每4年减半。
    nSubsidy >>= halvings;
    return nSubsidy;
}

bool IsInitialBlockDownload()
{
//一旦该函数返回了false，它就必须保持false。
    static std::atomic<bool> latchToFalse{false};
//优化：在获取锁之前对锁进行预测试。
    if (latchToFalse.load(std::memory_order_relaxed))
        return false;

    LOCK(cs_main);
    if (latchToFalse.load(std::memory_order_relaxed))
        return false;
    if (fImporting || fReindex)
        return true;
    if (chainActive.Tip() == nullptr)
        return true;
    if (chainActive.Tip()->nChainWork < nMinimumChainWork)
        return true;
    if (chainActive.Tip()->GetBlockTime() < (GetTime() - nMaxTipAge))
        return true;
    LogPrintf("Leaving InitialBlockDownload (latching to false)\n");
    latchToFalse.store(true, std::memory_order_relaxed);
    return false;
}

CBlockIndex *pindexBestForkTip = nullptr, *pindexBestForkBase = nullptr;

static void AlertNotify(const std::string& strMessage)
{
    uiInterface.NotifyAlertChanged();
    std::string strCmd = gArgs.GetArg("-alertnotify", "");
    if (strCmd.empty()) return;

//警报文本应为来自受信任源的纯ASCII，但应为
//为了安全，我们先去掉任何不在safechars中的内容，然后在周围添加单引号
//将整个字符串传递给shell之前：
    std::string singleQuote("'");
    std::string safeStatus = SanitizeString(strMessage);
    safeStatus = singleQuote+safeStatus+singleQuote;
    boost::replace_all(strCmd, "%s", safeStatus);

    std::thread t(runCommand, strCmd);
t.detach(); //线程自由运行
}

static void CheckForkWarningConditions() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
//在通过最初的下载之前，我们不能可靠地警告forks
//（我们假设在完成初始同步之前不会被卡在分叉上）
    if (IsInitialBlockDownload())
        return;

//如果我们最好的货叉不在72个街区内（+/-12小时，如果没有人挖掘它）
//我们的头，放下它
    if (pindexBestForkTip && chainActive.Height() - pindexBestForkTip->nHeight >= 72)
        pindexBestForkTip = nullptr;

    if (pindexBestForkTip || (pindexBestInvalid && pindexBestInvalid->nChainWork > chainActive.Tip()->nChainWork + (GetBlockProof(*chainActive.Tip()) * 6)))
    {
        if (!GetfLargeWorkForkFound() && pindexBestForkBase)
        {
            std::string warning = std::string("'Warning: Large-work fork detected, forking after block ") +
                pindexBestForkBase->phashBlock->ToString() + std::string("'");
            AlertNotify(warning);
        }
        if (pindexBestForkTip && pindexBestForkBase)
        {
            LogPrintf("%s: Warning: Large valid fork found\n  forking the chain at height %d (%s)\n  lasting to height %d (%s).\nChain state database corruption likely.\n", __func__,
                   pindexBestForkBase->nHeight, pindexBestForkBase->phashBlock->ToString(),
                   pindexBestForkTip->nHeight, pindexBestForkTip->phashBlock->ToString());
            SetfLargeWorkForkFound(true);
        }
        else
        {
            LogPrintf("%s: Warning: Found invalid chain at least ~6 blocks longer than our best chain.\nChain state database corruption likely.\n", __func__);
            SetfLargeWorkInvalidChainFound(true);
        }
    }
    else
    {
        SetfLargeWorkForkFound(false);
        SetfLargeWorkInvalidChainFound(false);
    }
}

static void CheckForkWarningConditionsOnNewFork(CBlockIndex* pindexNewForkTip) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
//如果我们在一个足够大的叉子上，设置一个警告标志
    CBlockIndex* pfork = pindexNewForkTip;
    CBlockIndex* plonger = chainActive.Tip();
    while (pfork && pfork != plonger)
    {
        while (plonger && plonger->nHeight > pfork->nHeight)
            plonger = plonger->pprev;
        if (pfork == plonger)
            break;
        pfork = pfork->pprev;
    }

//我们定义了一个条件，在该条件下，我们应该警告用户至少有7个块作为分叉。
//在我们的72个街区内有一个小费（如果没有人挖掘它的话，+/-12小时）
//我们使用7个块，相当随意，因为它只占持续网络的10%以下。
//在分叉上操作的哈希率。
//或者是一条比我们的链长得多的链，它是无效的（请注意，这两者都应该检测到）。
//我们这样定义它是因为它只允许我们存储满足
//7块条件下，从这一点来看，最有可能导致警告叉
    if (pfork && (!pindexBestForkTip || pindexNewForkTip->nHeight > pindexBestForkTip->nHeight) &&
            pindexNewForkTip->nChainWork - pfork->nChainWork > (GetBlockProof(*pfork) * 7) &&
            chainActive.Height() - pindexNewForkTip->nHeight < 72)
    {
        pindexBestForkTip = pindexNewForkTip;
        pindexBestForkBase = pfork;
    }

    CheckForkWarningConditions();
}

void static InvalidChainFound(CBlockIndex* pindexNew) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (!pindexBestInvalid || pindexNew->nChainWork > pindexBestInvalid->nChainWork)
        pindexBestInvalid = pindexNew;

    LogPrintf("%s: invalid block=%s  height=%d  log2_work=%.8g  date=%s\n", __func__,
      pindexNew->GetBlockHash().ToString(), pindexNew->nHeight,
      log(pindexNew->nChainWork.getdouble())/log(2.0), FormatISO8601DateTime(pindexNew->GetBlockTime()));
    CBlockIndex *tip = chainActive.Tip();
    assert (tip);
    LogPrintf("%s:  current best=%s  height=%d  log2_work=%.8g  date=%s\n", __func__,
      tip->GetBlockHash().ToString(), chainActive.Height(), log(tip->nChainWork.getdouble())/log(2.0),
      FormatISO8601DateTime(tip->GetBlockTime()));
    CheckForkWarningConditions();
}

void CChainState::InvalidBlockFound(CBlockIndex *pindex, const CValidationState &state) {
    if (!state.CorruptionPossible()) {
        pindex->nStatus |= BLOCK_FAILED_VALID;
        m_failed_blocks.insert(pindex);
        setDirtyBlockIndex.insert(pindex);
        setBlockIndexCandidates.erase(pindex);
        InvalidChainFound(pindex);
    }
}

void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, CTxUndo &txundo, int nHeight)
{
//标记花费的输入
    if (!tx.IsCoinBase()) {
        txundo.vprevout.reserve(tx.vin.size());
        for (const CTxIn &txin : tx.vin) {
            txundo.vprevout.emplace_back();
            bool is_spent = inputs.SpendCoin(txin.prevout, &txundo.vprevout.back());
            assert(is_spent);
        }
    }
//添加输出
    AddCoins(inputs, tx, nHeight);
}

void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, int nHeight)
{
    CTxUndo txundo;
    UpdateCoins(tx, inputs, txundo, nHeight);
}

bool CScriptCheck::operator()() {
    const CScript &scriptSig = ptxTo->vin[nIn].scriptSig;
    const CScriptWitness *witness = &ptxTo->vin[nIn].scriptWitness;
    return VerifyScript(scriptSig, m_tx_out.scriptPubKey, witness, nFlags, CachingTransactionSignatureChecker(ptxTo, nIn, m_tx_out.nValue, cacheStore, *txdata), &error);
}

int GetSpendHeight(const CCoinsViewCache& inputs)
{
    LOCK(cs_main);
    CBlockIndex* pindexPrev = LookupBlockIndex(inputs.GetBestBlock());
    return pindexPrev->nHeight + 1;
}


static CuckooCache::cache<uint256, SignatureCacheHasher> scriptExecutionCache;
static uint256 scriptExecutionCacheNonce(GetRandHash());

void InitScriptExecutionCache() {
//nmaxachesize是无符号的。如果-maxsigcachesize设置为零，
//设置字节创建最小可能缓存（2个元素）。
    size_t nMaxCacheSize = std::min(std::max((int64_t)0, gArgs.GetArg("-maxsigcachesize", DEFAULT_MAX_SIG_CACHE_SIZE) / 2), MAX_MAX_SIG_CACHE_SIZE) * ((size_t) 1 << 20);
    size_t nElems = scriptExecutionCache.setup_bytes(nMaxCacheSize);
    LogPrintf("Using %zu MiB out of %zu/2 requested for script execution cache, able to store %zu elements\n",
            (nElems*sizeof(uint256)) >>20, (nMaxCacheSize*2)>>20, nElems);
}

/*
 *检查此事务的所有输入是否有效（无重复开销、脚本和签名、金额）
 *这不会修改utxo集。
 *
 *如果pvchecks不是nullptr，则脚本检查将推送到它上面，而不是直接执行。任何
 *显然，不需要的脚本检查（例如由于脚本执行缓存命中）是，
 *未推到pvchecks/run上。
 *
 *将cachesigstore/cachefullscriptstore设置为false将从相应的缓存中删除元素。
 *匹配。这对于检查可能不需要缓存的块很有用
 *再次进入。
 *
 *src/test/txvalidationcache_tests.cpp中的非静态（并重新声明）
 **/

bool CheckInputs(const CTransaction& tx, CValidationState &state, const CCoinsViewCache &inputs, bool fScriptChecks, unsigned int flags, bool cacheSigStore, bool cacheFullScriptStore, PrecomputedTransactionData& txdata, std::vector<CScriptCheck> *pvChecks) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (!tx.IsCoinBase())
    {
        if (pvChecks)
            pvChecks->reserve(tx.vin.size());

//上面的第一个循环完成所有便宜的检查。
//只有当所有输入都通过时，我们才会执行昂贵的ECDSA签名检查。
//有助于防止CPU耗尽攻击。

//在连接下面的块时跳过脚本验证
//假设有效块。假设假设assumevalid块有效，则
//是安全的，因为仍然计算和检查块merkle哈希，
//当然，如果假定的有效块由于错误的scriptsigs而无效
//此优化将允许接受无效的链。
        if (fScriptChecks) {
//首先检查脚本执行是否使用相同的
//旗帜。注意，这假设提供的输入是
//正确（即Tx中的事务散列
//在输入视图中正确地提交到scriptPubKey
//事务处理）。
            uint256 hashCacheEntry;
//我们只使用nonce的前19个字节来避免第二个sha
//四舍五入-给我们19+32+4=55字节（+8+1=64）
            static_assert(55 - sizeof(flags) - 32 >= 128/8, "Want at least 128 bits of nonce for script execution cache");
            CSHA256().Write(scriptExecutionCacheNonce.begin(), 55 - sizeof(flags) - 32).Write(tx.GetWitnessHash().begin(), 32).Write((unsigned char*)&flags, sizeof(flags)).Finalize(hashCacheEntry.begin());
AssertLockHeld(cs_main); //TODO:通过使布谷缓存不需要外部锁来删除此要求
            if (scriptExecutionCache.contains(hashCacheEntry, !cacheFullScriptStore)) {
                return true;
            }

            for (unsigned int i = 0; i < tx.vin.size(); i++) {
                const COutPoint &prevout = tx.vin[i].prevout;
                const Coin& coin = inputs.AccessCoin(prevout);
                assert(!coin.IsSpent());

//我们非常小心地把东西交给CScriptcheck
//被Tx的证人哈希明确承诺。这提供
//检查我们的缓存是否没有引入共识
//通过附加数据失败，例如，硬币
//作为CScriptcheck的一部分被检查过。

//验证签名
                CScriptCheck check(coin.out, tx, i, flags, cacheSigStore, &txdata);
                if (pvChecks) {
                    pvChecks->push_back(CScriptCheck());
                    check.swap(pvChecks->back());
                } else if (!check()) {
                    if (flags & STANDARD_NOT_MANDATORY_VERIFY_FLAGS) {
//检查故障是否由
//非强制性脚本验证检查，例如
//非标准编码或非空虚拟
//参数；如果是，不要触发DoS保护
//避免在升级和
//未升级的节点。
                        CScriptCheck check2(coin.out, tx, i,
                                flags & ~STANDARD_NOT_MANDATORY_VERIFY_FLAGS, cacheSigStore, &txdata);
                        if (check2())
                            return state.Invalid(false, REJECT_NONSTANDARD, strprintf("non-mandatory-script-verify-flag (%s)", ScriptErrorString(check.GetScriptError())));
                    }
//其他标志的失败表示
//在新块中无效，例如p2sh无效。我们禁止
//不遵循协议的节点。那
//说在升级过程中要仔细考虑
//关于正确的行为-我们可能想继续
//即使在软分叉之后也使用未升级的节点进行对等
//出现了绝大多数信号。
                    return state.DoS(100,false, REJECT_INVALID, strprintf("mandatory-script-verify-flag-failed (%s)", ScriptErrorString(check.GetScriptError())));
                }
            }

            if (cacheFullScriptStore && !pvChecks) {
//我们执行了所有提供的脚本，并被告知
//缓存结果。现在就这么做。
                scriptExecutionCache.insert(hashCacheEntry);
            }
        }
    }

    return true;
}

namespace {

bool UndoWriteToDisk(const CBlockUndo& blockundo, CDiskBlockPos& pos, const uint256& hashBlock, const CMessageHeader::MessageStartChars& messageStart)
{
//打开要附加的历史文件
    CAutoFile fileout(OpenUndoFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s: OpenUndoFile failed", __func__);

//写入索引头
    unsigned int nSize = GetSerializeSize(blockundo, fileout.GetVersion());
    fileout << messageStart << nSize;

//写入撤消数据
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("%s: ftell failed", __func__);
    pos.nPos = (unsigned int)fileOutPos;
    fileout << blockundo;

//计算和写入校验和
    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    hasher << hashBlock;
    hasher << blockundo;
    fileout << hasher.GetHash();

    return true;
}

static bool UndoReadFromDisk(CBlockUndo& blockundo, const CBlockIndex *pindex)
{
    CDiskBlockPos pos = pindex->GetUndoPos();
    if (pos.IsNull()) {
        return error("%s: no undo data available", __func__);
    }

//打开要读取的历史文件
    CAutoFile filein(OpenUndoFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("%s: OpenUndoFile failed", __func__);

//读块
    uint256 hashChecksum;
CHashVerifier<CAutoFile> verifier(&filein); //我们需要一个Chashverifier，因为重新初始化可能会丢失数据。
    try {
        verifier << pindex->pprev->GetBlockHash();
        verifier >> blockundo;
        filein >> hashChecksum;
    }
    catch (const std::exception& e) {
        return error("%s: Deserialize or I/O error - %s", __func__, e.what());
    }

//校验校验和
    if (hashChecksum != verifier.GetHash())
        return error("%s: Checksum mismatch", __func__);

    return true;
}

/*用消息中止*/
static bool AbortNode(const std::string& strMessage, const std::string& userMessage="")
{
    SetMiscWarning(strMessage);
    LogPrintf("*** %s\n", strMessage);
    uiInterface.ThreadSafeMessageBox(
        userMessage.empty() ? _("Error: A fatal internal error occurred, see debug.log for details") : userMessage,
        "", CClientUIInterface::MSG_ERROR);
    StartShutdown();
    return false;
}

static bool AbortNode(CValidationState& state, const std::string& strMessage, const std::string& userMessage="")
{
    AbortNode(strMessage, userMessage);
    return state.Error(strMessage);
}

} //命名空间

/*
 *在给定的位置恢复硬币中的utxo
 *@param撤消要恢复的硬币。
 *@param查看要应用更改的硬币视图。
 *@param输出对应于Tx输入的输出点。
 *@返回一个作为int的disconnectresult
 **/

int ApplyTxInUndo(Coin&& undo, CCoinsViewCache& view, const COutPoint& out)
{
    bool fClean = true;

if (view.HaveCoin(out)) fClean = false; //覆盖事务输出

    if (undo.nHeight == 0) {
//缺少撤消元数据（height和coinbase）。旧版本包括这个
//仅在事务的最后一次支出的撤消记录中提供信息'
//输出。这意味着它必须存在于同一Tx的其他输出中。
        const Coin& alternate = AccessByTxid(view, out.hash);
        if (!alternate.IsSpent()) {
            undo.nHeight = alternate.nHeight;
            undo.fCoinBase = alternate.fCoinBase;
        } else {
return DISCONNECT_FAILED; //为没有已知元数据的事务添加输出
        }
    }
//只有当我们知道
//当然，缓存中还没有硬币。正如我们上面所问的那样
//使用HaveCoin，我们不需要猜测。当fclean是假的时，一枚硬币已经存在，并且
//这是一个覆盖。
    view.AddCoin(out, std::move(undo), !fClean);

    return fClean ? DISCONNECT_OK : DISCONNECT_UNCLEAN;
}

/*撤消此块（具有给定索引）对硬币表示的utxo集的影响。
 *当返回failed时，视图将处于不确定状态。*/

DisconnectResult CChainState::DisconnectBlock(const CBlock& block, const CBlockIndex* pindex, CCoinsViewCache& view)
{
    bool fClean = true;

    CBlockUndo blockUndo;
    if (!UndoReadFromDisk(blockUndo, pindex)) {
        error("DisconnectBlock(): failure reading undo data");
        return DISCONNECT_FAILED;
    }

    if (blockUndo.vtxundo.size() + 1 != block.vtx.size()) {
        error("DisconnectBlock(): block and undo data inconsistent");
        return DISCONNECT_FAILED;
    }

//按相反顺序撤消交易记录
    for (int i = block.vtx.size() - 1; i >= 0; i--) {
        const CTransaction &tx = *(block.vtx[i]);
        uint256 hash = tx.GetHash();
        bool is_coinbase = tx.IsCoinBase();

//检查所有输出是否可用，并匹配块本身的输出。
//确切地。
        for (size_t o = 0; o < tx.vout.size(); o++) {
            if (!tx.vout[o].scriptPubKey.IsUnspendable()) {
                COutPoint out(hash, o);
                Coin coin;
                bool is_spent = view.SpendCoin(out, &coin);
                if (!is_spent || tx.vout[o] != coin.out || pindex->nHeight != coin.nHeight || is_coinbase != coin.fCoinBase) {
fClean = false; //事务输出不匹配
                }
            }
        }

//恢复输入
if (i > 0) { //非共基
            CTxUndo &txundo = blockUndo.vtxundo[i-1];
            if (txundo.vprevout.size() != tx.vin.size()) {
                error("DisconnectBlock(): transaction and undo data inconsistent");
                return DISCONNECT_FAILED;
            }
            for (unsigned int j = tx.vin.size(); j-- > 0;) {
                const COutPoint &out = tx.vin[j].prevout;
                int res = ApplyTxInUndo(std::move(txundo.vprevout[j]), view, out);
                if (res == DISCONNECT_FAILED) return DISCONNECT_FAILED;
                fClean = fClean && res != DISCONNECT_UNCLEAN;
            }
//此时，所有txundo.vprevout都应该被移出。
        }
    }

//将最佳块指针移动到Previout块
    view.SetBestBlock(pindex->pprev->GetBlockHash());

    return fClean ? DISCONNECT_OK : DISCONNECT_UNCLEAN;
}

void static FlushBlockFile(bool fFinalize = false)
{
    LOCK(cs_LastBlockFile);

    CDiskBlockPos posOld(nLastBlockFile, 0);
    bool status = true;

    FILE *fileOld = OpenBlockFile(posOld);
    if (fileOld) {
        if (fFinalize)
            status &= TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nSize);
        status &= FileCommit(fileOld);
        fclose(fileOld);
    }

    fileOld = OpenUndoFile(posOld);
    if (fileOld) {
        if (fFinalize)
            status &= TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nUndoSize);
        status &= FileCommit(fileOld);
        fclose(fileOld);
    }

    if (!status) {
        AbortNode("Flushing block file to disk failed. This is likely the result of an I/O error.");
    }
}

static bool FindUndoPos(CValidationState &state, int nFile, CDiskBlockPos &pos, unsigned int nAddSize);

static bool WriteUndoDataForBlock(const CBlockUndo& blockundo, CValidationState& state, CBlockIndex* pindex, const CChainParams& chainparams)
{
//将撤消信息写入磁盘
    if (pindex->GetUndoPos().IsNull()) {
        CDiskBlockPos _pos;
        if (!FindUndoPos(state, pindex->nFile, _pos, ::GetSerializeSize(blockundo, CLIENT_VERSION) + 40))
            return error("ConnectBlock(): FindUndoPos failed");
        if (!UndoWriteToDisk(blockundo, _pos, pindex->pprev->GetBlockHash(), chainparams.MessageStart()))
            return AbortNode(state, "Failed to write undo data");

//更新块索引中的nundopos
        pindex->nUndoPos = _pos.nPos;
        pindex->nStatus |= BLOCK_HAVE_UNDO;
        setDirtyBlockIndex.insert(pindex);
    }

    return true;
}

static CCheckQueue<CScriptCheck> scriptcheckqueue(128);

void ThreadScriptCheck() {
    RenameThread("bitcoin-scriptch");
    scriptcheckqueue.Thread();
}

VersionBitsCache versionbitscache GUARDED_BY(cs_main);

int32_t ComputeBlockVersion(const CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    LOCK(cs_main);
    int32_t nVersion = VERSIONBITS_TOP_BITS;

    for (int i = 0; i < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; i++) {
        ThresholdState state = VersionBitsState(pindexPrev, params, static_cast<Consensus::DeploymentPos>(i), versionbitscache);
        if (state == ThresholdState::LOCKED_IN || state == ThresholdState::STARTED) {
            nVersion |= VersionBitsMask(params, static_cast<Consensus::DeploymentPos>(i));
        }
    }

    return nVersion;
}

/*
 *当网络上出现未知版本位时触发的阈值条件检查程序。
 **/

class WarningBitsConditionChecker : public AbstractThresholdConditionChecker
{
private:
    int bit;

public:
    explicit WarningBitsConditionChecker(int bitIn) : bit(bitIn) {}

    int64_t BeginTime(const Consensus::Params& params) const override { return 0; }
    int64_t EndTime(const Consensus::Params& params) const override { return std::numeric_limits<int64_t>::max(); }
    int Period(const Consensus::Params& params) const override { return params.nMinerConfirmationWindow; }
    int Threshold(const Consensus::Params& params) const override { return params.nRuleChangeActivationThreshold; }

    bool Condition(const CBlockIndex* pindex, const Consensus::Params& params) const override
    {
        return ((pindex->nVersion & VERSIONBITS_TOP_MASK) == VERSIONBITS_TOP_BITS) &&
               ((pindex->nVersion >> bit) & 1) != 0 &&
               ((ComputeBlockVersion(pindex->pprev, params) >> bit) & 1) == 0;
    }
};

static ThresholdConditionCache warningcache[VERSIONBITS_NUM_BITS] GUARDED_BY(cs_main);

//0.13.0与为testnet定义的segwit部署一起装运，但不用于
//主干网。我们不再需要支持禁用Segwit部署
//除测试目的外，由于功能测试的限制
//环境。参见测试/功能/p2p-segwit.py。
static bool IsScriptWitnessEnabled(const Consensus::Params& params)
{
    return params.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout != 0;
}

static unsigned int GetBlockScriptFlags(const CBlockIndex* pindex, const Consensus::Params& consensusparams) EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
    AssertLockHeld(cs_main);

    unsigned int flags = SCRIPT_VERIFY_NONE;

//bip16直到2012年4月1日才开始使用（在mainnet上，和
//追溯应用于testnet）
//但是，只有一个历史块违反了p2sh规则（在两个方面
//mainnet和testnet），因此为了简单起见，始终保留p2sh
//打开，但违反块除外。
if (consensusparams.BIP16Exception.IsNull() || //此链上没有bip16异常
pindex->phashBlock == nullptr || //这是一个新的候选块，例如来自testblockvalidity（）。
*pindex->phashBlock != consensusparams.BIP16Exception) //这个街区不是历史上的例外
    {
        flags |= SCRIPT_VERIFY_P2SH;
    }

//在p2sh生效时强制执行见证规则（以及segwit
//部署已定义）。
    if (flags & SCRIPT_VERIFY_P2SH && IsScriptWitnessEnabled(consensusparams)) {
        flags |= SCRIPT_VERIFY_WITNESS;
    }

//开始执行Dersig（bip66）规则
    if (pindex->nHeight >= consensusparams.BIP66Height) {
        flags |= SCRIPT_VERIFY_DERSIG;
    }

//开始实施checklocktimeverify（bip65）规则
    if (pindex->nHeight >= consensusparams.BIP65Height) {
        flags |= SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
    }

//开始使用versionBits逻辑强制执行bip68（序列锁）和bip112（checkSequenceVerify）。
    if (VersionBitsState(pindex->pprev, consensusparams, Consensus::DEPLOYMENT_CSV, versionbitscache) == ThresholdState::ACTIVE) {
        flags |= SCRIPT_VERIFY_CHECKSEQUENCEVERIFY;
    }

    if (IsNullDummyEnabled(pindex->pprev, consensusparams)) {
        flags |= SCRIPT_VERIFY_NULLDUMMY;
    }

    return flags;
}



static int64_t nTimeCheck = 0;
static int64_t nTimeForks = 0;
static int64_t nTimeVerify = 0;
static int64_t nTimeConnect = 0;
static int64_t nTimeIndex = 0;
static int64_t nTimeCallbacks = 0;
static int64_t nTimeTotal = 0;
static int64_t nBlocksTotal = 0;

/*将此块（具有给定索引）的效果应用于硬币表示的utxo集合。
 *依赖于utxo集的有效性检查也会完成；connectBlock（））
 *如果这些有效性检查失败（除其他原因外），则可能失败。*/

bool CChainState::ConnectBlock(const CBlock& block, CValidationState& state, CBlockIndex* pindex,
                  CCoinsViewCache& view, const CChainParams& chainparams, bool fJustCheck)
{
    AssertLockHeld(cs_main);
    assert(pindex);
    assert(*pindex->phashBlock == block.GetHash());
    int64_t nTimeStart = GetTimeMicros();

//再次检查，以防以前的版本让坏块进入
//注意：我们目前没有（重新）调用ContextualCheckBlock（）或
//这里是contextualCheckBlockHeader（）。这意味着如果我们添加新的
//在这两个职能中执行的共识规则，那么我们
//可能在更新
//软件，我们不会在这里执行规则。完全解决
//在达成共识后从一个软件版本升级到下一个软件版本
//更改可能很棘手，并且是特定于问题的（请参阅rewindblockindex（））
//一种用于BIP 141部署的通用方法）。
//另外，目前规定未来2小时以上禁止阻塞
//在contextualCheckBlockHeader（）中强制执行；我们不希望
//在这里重新执行这条规则（至少直到我们让它不可能
//getAdjustedTime（）返回）。
    if (!CheckBlock(block, state, chainparams.GetConsensus(), !fJustCheck, !fJustCheck)) {
        if (state.CorruptionPossible()) {
//我们不写块到磁盘，如果他们可能
//损坏了，所以这应该是不可能的，除非我们有硬件
//问题。
            return AbortNode(state, "Corrupt block found indicating potential hardware failure; shutting down");
        }
        return error("%s: Consensus::CheckBlock: %s", __func__, FormatStateMessage(state));
    }

//验证视图的当前状态是否与上一个块对应
    uint256 hashPrevBlock = pindex->pprev == nullptr ? uint256() : pindex->pprev->GetBlockHash();
    assert(hashPrevBlock == view.GetBestBlock());

//Genesis块的特殊情况，跳过其事务的连接
//（它的货币基础是不可依赖的）
    if (block.GetHash() == chainparams.GetConsensus().hashGenesisBlock) {
        if (!fJustCheck)
            view.SetBestBlock(pindex->GetBlockHash());
        return true;
    }

    nBlocksTotal++;

    bool fScriptChecks = true;
    if (!hashAssumeValid.IsNull()) {
//我们已经配置了一个块的散列值，该散列值已在外部验证为具有有效的历史记录。
//软件中包含适当的默认值，并不时更新。因为有效性
//相对于一个软件来说，这些默认值很容易被审查是一个客观事实。
//此设置不会强制选择任何特定的链，但通过
//有效缓存部分验证的结果。
        BlockMap::const_iterator  it = mapBlockIndex.find(hashAssumeValid);
        if (it != mapBlockIndex.end()) {
            if (it->second->GetAncestor(pindex->nHeight) == pindex &&
                pindexBestHeader->GetAncestor(pindex->nHeight) == pindex &&
                pindexBestHeader->nChainWork >= nMinimumChainWork) {
//此块是假定已验证链的成员，也是最佳头的祖先。
//等效时间检查阻止哈希电源通过DoS攻击敲诈网络
//通过告诉用户必须手动设置assumevalid来接受无效的块。
//无论设置如何，要求软件更改或隐藏无效块都会使
//很难隐藏需求的含义。这也避免了发布候选文件
//在测试中几乎不需要进行任何签名验证
//人为地将默认的假定已验证块设置回后面。
//针对nminiumchainwork的测试防止在拒绝访问位于的任何链时跳过
//至少和预期的链条一样好。
                fScriptChecks = (GetBlockProofEquivalentTime(*pindexBestHeader, *pindex, *pindexBestHeader, chainparams.GetConsensus()) <= 60 * 60 * 24 * 7 * 2);
            }
        }
    }

    int64_t nTime1 = GetTimeMicros(); nTimeCheck += nTime1 - nTimeStart;
    LogPrint(BCLog::BENCH, "    - Sanity checks: %.2fms [%.2fs (%.2fms/blk)]\n", MILLI * (nTime1 - nTimeStart), nTimeCheck * MICRO, nTimeCheck * MILLI / nBlocksTotal);

//不允许包含“覆盖”旧事务的事务的块，
//除非这些钱已经花光了。
//如果允许这样的覆盖，则根据这些覆盖来创建coinbase和事务
//可以复制以消除花费第一个实例的能力——甚至在
//发送到其他地址。
//更多信息请参见bip30和http://r6.ca/blog/20120206t005236z.html。
//对于内存池事务，此逻辑不是必需的，因为AcceptToMoryPool
//已经完全拒绝以前已知的事务ID。
//此规则最初应用于2012年3月15日0:00 UTC之后时间戳为的所有块。
//现在整个链条已经不可逆转地超过了这个时间，它被应用到除
//两个在违反它的链条。这样可以防止在节点的
//初始块下载。
    bool fEnforceBIP30 = !((pindex->nHeight==91842 && pindex->GetBlockHash() == uint256S("0x00000000000a4d0a398161ffc163c503763b1f4360639393e0e4c8e300e0caec")) ||
                           (pindex->nHeight==91880 && pindex->GetBlockHash() == uint256S("0x00000000000743f190a18c5577a3c2d2a1f610ae9601ac046a38084ccb7cd721")));

//一旦bip34激活，就不可能创建新的重复的coinbase，因此除了启动
//有了两个重复的coinbase对，就不可能创建覆盖的tx。但通过
//激活bip34的时间，在每个现有的对中，复制的coinbase都覆盖了第一个
//在第一个花光之前。因为这些硬币的底座已经埋得很深了，再也不可能创造出更多的硬币了。
//从已知对中递减的重复事务。
//如果我们在比bip34激活的高度更高的已知链上，我们可以保存bip30检查所需的db访问。

//bip34要求高度为x的块（块x）具有其coinbase
//scriptsig以x的cscriptNum开头（指示高度x）。以上
//BIP34激活后不再需要BIP30的逻辑存在缺陷
//如果bip34高度227931之前有一个块X，
//指示高度y，其中y大于x。块的coinbase
//X也将是块Y的一个有效的coinbase，它可以是一个bip30。
//违反。对之前的所有mainnet coinbase的详尽搜索
//指示高度大于块高度的bip34高度
//显示多个事件。发现的3个最低指示高度为
//209921、490897和1983702，因此这3个街区的货币基础
//高度将是第一次违反bip30的机会。

//搜索显示有许多块具有指示高度
//大于1983702，因此我们只需删除优化即可跳过
//BIP30检查高度为1983702或更高的块。在我们到达之前
//再过25年左右，我们应该利用
//未来的共识改变，做一个新的和改进版本的BIP34
//实际上会阻止在
//未来。
    static constexpr int BIP34_IMPLIES_BIP30_LIMIT = 1983702;

//在块209921上不可能创建重复的coinbase。
//因为这仍然在bip34高度之前，所以显式bip30
//检查仍处于活动状态。

//最后一个案例是176684号区块，指示高度为
//490897。不幸的是，这个问题直到大约2周才被发现。
//在490897号街区之前，没有太多机会解决这个问题。
//除了仔细分析和确定它不会是一个
//问题。事实上，490897号区块开采的货币基础与
//块176684，但重要的是要注意，即使它不是或
//在另一个叉子上用复制的硬币做纪念，我们仍然会
//不会遇到BIP30冲突。这是因为176684年的铸币库
//在185956块用于交易
//D4F7FBBF92F4A3014A230B2DC70B8058D02EB36AC06B4A0736D9D60EAA9E8781。这个
//支出交易不能重复，因为它还支出coinbase
//0328DD85C331237F18E781D692C92DE57649529BD5EDF1D01036DAEA32FDE29。这个
//Coinbase的指示高度超过42亿，而且不会
//可复制到该高度，并且当前无法创建
//链子那么长。尽管如此，我们还是希望考虑未来的软叉
//它可以追溯阻止块490897创建副本
//钴基。两个历史上的BIP30违规行为常常令人困惑
//在操作utxo时使用边缘大小写，而不使用
//另一个需要处理的边缘案例。

//在指示高度的bip34高度之前，testnet3没有块
//Bip34之后大约高度486000000，可能
//在到达1983702块并开始不必要的操作之前重置
//BIP30再次检查。
    assert(pindex->pprev);
    CBlockIndex *pindexBIP34height = pindex->pprev->GetAncestor(chainparams.GetConsensus().BIP34Height);
//只有当我们低于bip34激活高度或该高度的块散列不对应时，才能继续执行。
    fEnforceBIP30 = fEnforceBIP30 && (!pindexBIP34height || !(pindexBIP34height->GetBlockHash() == chainparams.GetConsensus().BIP34Hash));

//TODO:一旦我们有了一个
//共识的改变确保了在这些高度上的共同基础不可能
//复制早期的货币基础。
    if (fEnforceBIP30 || pindex->nHeight >= BIP34_IMPLIES_BIP30_LIMIT) {
        for (const auto& tx : block.vtx) {
            for (size_t o = 0; o < tx->vout.size(); o++) {
                if (view.HaveCoin(COutPoint(tx->GetHash(), o))) {
                    return state.DoS(100, error("ConnectBlock(): tried to overwrite transaction"),
                                     REJECT_INVALID, "bad-txns-BIP30");
                }
            }
        }
    }

//开始使用versionBits逻辑强制执行bip68（序列锁）和bip112（checkSequenceVerify）。
    int nLockTimeFlags = 0;
    if (VersionBitsState(pindex->pprev, chainparams.GetConsensus(), Consensus::DEPLOYMENT_CSV, versionbitscache) == ThresholdState::ACTIVE) {
        nLockTimeFlags |= LOCKTIME_VERIFY_SEQUENCE;
    }

//获取此块的脚本标志
    unsigned int flags = GetBlockScriptFlags(pindex, chainparams.GetConsensus());

    int64_t nTime2 = GetTimeMicros(); nTimeForks += nTime2 - nTime1;
    LogPrint(BCLog::BENCH, "    - Fork checks: %.2fms [%.2fs (%.2fms/blk)]\n", MILLI * (nTime2 - nTime1), nTimeForks * MICRO, nTimeForks * MILLI / nBlocksTotal);

    CBlockUndo blockundo;

    CCheckQueueControl<CScriptCheck> control(fScriptChecks && nScriptCheckThreads ? &scriptcheckqueue : nullptr);

    std::vector<int> prevheights;
    CAmount nFees = 0;
    int nInputs = 0;
    int64_t nSigOpsCost = 0;
    blockundo.vtxundo.reserve(block.vtx.size() - 1);
    std::vector<PrecomputedTransactionData> txdata;
txdata.reserve(block.vtx.size()); //要求，以便指向单个预计算传输数据的指针不会失效
    for (unsigned int i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction &tx = *(block.vtx[i]);

        nInputs += tx.vin.size();

        if (!tx.IsCoinBase())
        {
            CAmount txfee = 0;
            if (!Consensus::CheckTxInputs(tx, state, view, pindex->nHeight, txfee)) {
                return error("%s: Consensus::CheckTxInputs: %s, %s", __func__, tx.GetHash().ToString(), FormatStateMessage(state));
            }
            nFees += txfee;
            if (!MoneyRange(nFees)) {
                return state.DoS(100, error("%s: accumulated fee in the block out of range.", __func__),
                                 REJECT_INVALID, "bad-txns-accumulated-fee-outofrange");
            }

//检查交易是否为BIP68最终交易
//BIP68锁定检查（与非锁定时间检查相反）必须
//在connectBlock中，因为它们需要utxo集
            prevheights.resize(tx.vin.size());
            for (size_t j = 0; j < tx.vin.size(); j++) {
                prevheights[j] = view.AccessCoin(tx.vin[j].prevout).nHeight;
            }

            if (!SequenceLocks(tx, nLockTimeFlags, &prevheights, *pindex)) {
                return state.DoS(100, error("%s: contains a non-BIP68-final transaction", __func__),
                                 REJECT_INVALID, "bad-txns-nonfinal");
            }
        }

//GetTransactionSigOpCost统计3种类型的SigoP：
//*传统（始终）
//*p2sh（当p2sh在标志中启用并且不包括coinbase时）
//*见证（当见证在标志中启用并且不包括coinbase时）
        nSigOpsCost += GetTransactionSigOpCost(tx, view, flags);
        if (nSigOpsCost > MAX_BLOCK_SIGOPS_COST)
            return state.DoS(100, error("ConnectBlock(): too many sigops"),
                             REJECT_INVALID, "bad-blk-sigops");

        txdata.emplace_back(tx);
        if (!tx.IsCoinBase())
        {
            std::vector<CScriptCheck> vChecks;
            /*l fcacheResults=fjustcheck；/*如果我们实际上正在连接块，则不缓存结果（尽管如此，仍请参考缓存）*/
            如果（！）检查输入（Tx、状态、视图、fscriptchecks、标志、fcacheResults、fcacheResults、txtdata[i]、nscriptcheckthreads？&vchecks:空指针）
                返回错误（“connectBlock（）：检查%s上的输入失败，错误为%s”，
                    tx.gethash（）.toString（），formatStateMessage（state））；
            控件。添加（vchecks）；
        }

        CTXUNDOUDO撤销虚拟；
        如果（i＞0）{
            blockUndo.vtxundo.push_back（ctxundo（））；
        }
        更新硬币（Tx，视图，i==0？UndoDummy:blockUndo.vtxUndo.back（），pindex->nheight）；
    }
    int64_t ntime3=getTimemicros（）；ntimeConnect+=ntime3-ntime2；
    logprint（bclog:：bench，“-connect%u事务：%.2fs（%.3fs/tx，%.3fs/txin）[%.2fs（%.2fs/blk）]\n“，（unsigned）block.vtx.size（），milli*（ntime3-ntime2），milli*（ntime3-ntime2）/block.vtx.size（），ninput<=1？0:milli*（ntime3-ntime2）/（ninputs-1），ntimeconnect*micro，ntimeconnect*milli/nblockstotal）；

    camount blockreward=nfees+getblocksubmity（pindex->nheight，chainparams.getconsensus（））；
    if（block.vtx[0]->getValueOut（）>blockReward）
        返回状态.dos（100，
                         错误（“connectBlock（）：CoinBase支付太多（实际值为%d，限制值为%d）”，
                               block.vtx[0]->getValueOut（），blockReward），然后
                               拒绝无效，“不良CB金额”）；

    如果（！）WAITE（）
        返回state.dos（100，错误（“%s:checkqueue failed”，“func”），reject_invalid，block validation failed”）；
    int64_t ntime4=getTimemicros（）；ntimeverify+=ntime4-ntime2；
    logprint（bclog:：bench，“-verify%u txins:%.2fs（%.3fs/txin）[%.2fs（%.2fs/blk）]\n”，ninputs-1，milli*（ntime4-ntime2），ninputs<=1？0:milli*（ntime4-ntime2）/（ninputs-1），ntimeverify*micro，ntimeverify*milli/nblockstotal）；

    如果（稍加检查）
        回归真实；

    如果（！）writeUndoDataForBlock（blockUndo、state、pindex、chainParams）
        返回错误；

    如果（！）pindex->isvalid（block_valid_scripts））
        pindex->raisevalidity（阻止有效的脚本）；
        setdirtyblockindex.insert（pindex）；
    }

    断言（pindex->phashblock）；
    //将此块添加到视图的块链中
    view.setBestBlock（pindex->getBlockHash（））；

    int64_t ntime5=getTimemicros（）；ntimeIndex+=ntime5-ntime4；
    logprint（bclog:：bench，“-index-writing:%.2fs[%.2fs（%.2fs/blk）]\n”，milli*（ntime5-ntime4），ntimeindex*micro，ntimeindex*milli/nblockstotal）；

    int64_t ntime6=getTimemicros（）；ntimeCallbacks+=ntime6-ntime5；
    logprint（bclog:：bench，“-callbacks:%.2fs[%.2fs（%.2fs/blk）]\n”，milli*（ntime6-ntime5），ntimecallbacks*micro，ntimecallbacks*milli/nblockstotal）；

    回归真实；
}

/**
 *更新磁盘上的链状态。
 *缓存和索引的刷新取决于调用的模式。
 *如果它们太大，如果从上一次写入到现在已经有一段时间了，
 *或者如果我们处于删减模式并正在删除文件，则始终如此。
 *
 *如果使用flushstateMode:：none，那么flushstateTodisk（…）将不做任何操作。
 *除了检查我们是否需要修剪。
 **/

bool static FlushStateToDisk(const CChainParams& chainparams, CValidationState &state, FlushStateMode mode, int nManualPruneHeight) {
    int64_t nMempoolUsage = mempool.DynamicMemoryUsage();
    LOCK(cs_main);
    static int64_t nLastWrite = 0;
    static int64_t nLastFlush = 0;
    std::set<int> setFilesToPrune;
    bool full_flush_completed = false;
    try {
    {
        bool fFlushForPrune = false;
        bool fDoFullFlush = false;
        LOCK(cs_LastBlockFile);
        if (fPruneMode && (fCheckForPruning || nManualPruneHeight > 0) && !fReindex) {
            if (nManualPruneHeight > 0) {
                FindFilesToPruneManual(setFilesToPrune, nManualPruneHeight);
            } else {
                FindFilesToPrune(setFilesToPrune, chainparams.PruneAfterHeight());
                fCheckForPruning = false;
            }
            if (!setFilesToPrune.empty()) {
                fFlushForPrune = true;
                if (!fHavePruned) {
                    pblocktree->WriteFlag("prunedblockfiles", true);
                    fHavePruned = true;
                }
            }
        }
        int64_t nNow = GetTimeMicros();
//避免在启动后立即写入/刷新。
        if (nLastWrite == 0) {
            nLastWrite = nNow;
        }
        if (nLastFlush == 0) {
            nLastFlush = nNow;
        }
        int64_t nMempoolSizeMax = gArgs.GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000;
        int64_t cacheSize = pcoinsTip->DynamicMemoryUsage();
        int64_t nTotalSpace = nCoinCacheUsage + std::max<int64_t>(nMempoolSizeMax - nMempoolUsage, 0);
//缓存很大，我们在限制的10%和10 mib范围内，但我们现在有时间（不是在块处理的中间）。
        bool fCacheLarge = mode == FlushStateMode::PERIODIC && cacheSize > std::max((9 * nTotalSpace) / 10, nTotalSpace - MAX_BLOCK_COINSDB_USAGE * 1024 * 1024);
//缓存超过了限制，我们现在必须写入。
        bool fCacheCritical = mode == FlushStateMode::IF_NEEDED && cacheSize > nTotalSpace;
//我们把块索引写到磁盘上已经有一段时间了。经常这样做，这样我们就不需要在碰撞后重新加载。
        bool fPeriodicWrite = mode == FlushStateMode::PERIODIC && nNow > nLastWrite + (int64_t)DATABASE_WRITE_INTERVAL * 1000000;
//我们刷新缓存已经很久了。很少这样做，以优化缓存使用。
        bool fPeriodicFlush = mode == FlushStateMode::PERIODIC && nNow > nLastFlush + (int64_t)DATABASE_FLUSH_INTERVAL * 1000000;
//合并导致完全缓存刷新的所有条件。
        fDoFullFlush = (mode == FlushStateMode::ALWAYS) || fCacheLarge || fCacheCritical || fPeriodicFlush || fFlushForPrune;
//将块和块索引写入磁盘。
        if (fDoFullFlush || fPeriodicWrite) {
//依靠nmindiskspace确保我们可以写入块索引
            if (!CheckDiskSpace(0, true))
                return state.Error("out of disk space");
//首先确保所有块和撤消数据都已刷新到磁盘。
            FlushBlockFile();
//然后更新所有块文件信息（可能指块和撤消文件）。
            {
                std::vector<std::pair<int, const CBlockFileInfo*> > vFiles;
                vFiles.reserve(setDirtyFileInfo.size());
                for (std::set<int>::iterator it = setDirtyFileInfo.begin(); it != setDirtyFileInfo.end(); ) {
                    vFiles.push_back(std::make_pair(*it, &vinfoBlockFile[*it]));
                    setDirtyFileInfo.erase(it++);
                }
                std::vector<const CBlockIndex*> vBlocks;
                vBlocks.reserve(setDirtyBlockIndex.size());
                for (std::set<CBlockIndex*>::iterator it = setDirtyBlockIndex.begin(); it != setDirtyBlockIndex.end(); ) {
                    vBlocks.push_back(*it);
                    setDirtyBlockIndex.erase(it++);
                }
                if (!pblocktree->WriteBatchSync(vFiles, nLastBlockFile, vBlocks)) {
                    return AbortNode(state, "Failed to write to block index database");
                }
            }
//最后删除所有修剪过的文件
            if (fFlushForPrune)
                UnlinkPrunedFiles(setFilesToPrune);
            nLastWrite = nNow;
        }
//刷新最佳链相关状态。只有在块/块索引写入也已完成时，才能执行此操作。
        if (fDoFullFlush && !pcoinsTip->GetBestBlock().IsNull()) {
//磁盘上典型的硬币结构大小约为48字节。
//将一个新的推送到数据库中可能会导致它被写入
//两次（一次在日志中，一次在表中）。这已经
//高估，因为大多数人会删除现有的条目或
//改写一个。不过，使用保守的安全系数2。
            if (!CheckDiskSpace(48 * 2 * 2 * pcoinsTip->GetCacheSize()))
                return state.Error("out of disk space");
//刷新链状态（可能指块索引项）。
            if (!pcoinsTip->Flush())
                return AbortNode(state, "Failed to write to coin database");
            nLastFlush = nNow;
            full_flush_completed = true;
        }
    }
    if (full_flush_completed) {
//更新钱包中的最佳块（以便我们可以检测还原的钱包）。
        GetMainSignals().ChainStateFlushed(chainActive.GetLocator());
    }
    } catch (const std::runtime_error& e) {
        return AbortNode(state, std::string("System error while flushing: ") + e.what());
    }
    return true;
}

void FlushStateToDisk() {
    CValidationState state;
    const CChainParams& chainparams = Params();
    if (!FlushStateToDisk(chainparams, state, FlushStateMode::ALWAYS)) {
        LogPrintf("%s: failed to flush state (%s)\n", __func__, FormatStateMessage(state));
    }
}

void PruneAndFlush() {
    CValidationState state;
    fCheckForPruning = true;
    const CChainParams& chainparams = Params();
    if (!FlushStateToDisk(chainparams, state, FlushStateMode::NONE)) {
        LogPrintf("%s: failed to flush state (%s)\n", __func__, FormatStateMessage(state));
    }
}

static void DoWarning(const std::string& strWarning)
{
    static bool fWarned = false;
    SetMiscWarning(strWarning);
    if (!fWarned) {
        AlertNotify(strWarning);
        fWarned = true;
    }
}

/*连接警告消息的私有助手函数。*/
static void AppendWarning(std::string& res, const std::string& warn)
{
    if (!res.empty()) res += ", ";
    res += warn;
}

/*检查警告条件，并对新的链提示集执行一些通知。*/
void static UpdateTip(const CBlockIndex *pindexNew, const CChainParams& chainParams) {
//新最佳区块
    mempool.AddTransactionsUpdated(1);

    {
        LOCK(g_best_block_mutex);
        g_best_block = pindexNew->GetBlockHash();
        g_best_block_cv.notify_all();
    }

    std::string warningMessages;
    if (!IsInitialBlockDownload())
    {
        int nUpgraded = 0;
        const CBlockIndex* pindex = pindexNew;
        for (int bit = 0; bit < VERSIONBITS_NUM_BITS; bit++) {
            WarningBitsConditionChecker checker(bit);
            ThresholdState state = checker.GetStateFor(pindex, chainParams.GetConsensus(), warningcache[bit]);
            if (state == ThresholdState::ACTIVE || state == ThresholdState::LOCKED_IN) {
                const std::string strWarning = strprintf(_("Warning: unknown new rules activated (versionbit %i)"), bit);
                if (state == ThresholdState::ACTIVE) {
                    DoWarning(strWarning);
                } else {
                    AppendWarning(warningMessages, strWarning);
                }
            }
        }
//检查最后100个块的版本，看看是否需要升级：
        for (int i = 0; i < 100 && pindex != nullptr; i++)
        {
            int32_t nExpectedVersion = ComputeBlockVersion(pindex->pprev, chainParams.GetConsensus());
            if (pindex->nVersion > VERSIONBITS_LAST_OLD_BLOCK_VERSION && (pindex->nVersion & ~nExpectedVersion) != 0)
                ++nUpgraded;
            pindex = pindex->pprev;
        }
        if (nUpgraded > 0)
            AppendWarning(warningMessages, strprintf(_("%d of last 100 blocks have unexpected version"), nUpgraded));
        if (nUpgraded > 100/2)
        {
            std::string strWarning = _("Warning: Unknown block versions being mined! It's possible unknown rules are in effect");
//通知getwarnings（），由qt和json-rpc代码调用，以警告用户：
            DoWarning(strWarning);
        }
    }
    /*printf（“%s:new best=%s height=%d version=0x%08X log2_work=%.8g tx=%lu date='%s'progress=%f cache=%.1fmib（%utxo）”，uufunc_uuuuuuux，/*继续*/
      pindexNew->getBlockHash（）.toString（），pindexNew->nheight，pindexNew->nversion，
      日志（pindexnew->nchainwork.getdouble（））/log（2.0），（无符号长）pindexnew->nchaintx，
      格式ISO8601日期时间（pindexNew->getBlockTime（）），
      guessVerificationProgress（chainParams.txData（），pindexNew），pcoinsTip->dynamicMemoryUsage（）*（1.0/（1<<20）），pcoinsTip->getCacheSize（））；
    如果（！）warningmessages.empty（））
        logprintf（“warning='%s'”，warningmessages）；/*续*/

    LogPrintf("\n");

}

/*断开链条主动端。
  *调用后，mempool将处于不一致状态，
  *来自正在添加到disconnectpool的断开连接块的事务。你
  *应通过调用updateMempoolForreorg使mempool再次保持一致。
  *保持CSU MAIN。
  *
  *如果disconnectpool为nullptr，则不会将断开连接的事务添加到
  *disconnectpool（请注意，调用者负责mempool的一致性
  *无论如何。
  **/

bool CChainState::DisconnectTip(CValidationState& state, const CChainParams& chainparams, DisconnectedBlockTransactions *disconnectpool)
{
    CBlockIndex *pindexDelete = chainActive.Tip();
    assert(pindexDelete);
//从磁盘读取块。
    std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>();
    CBlock& block = *pblock;
    if (!ReadBlockFromDisk(block, pindexDelete, chainparams.GetConsensus()))
        return AbortNode(state, "Failed to read block");
//原子地将块应用于链状态。
    int64_t nStart = GetTimeMicros();
    {
        CCoinsViewCache view(pcoinsTip.get());
        assert(view.GetBestBlock() == pindexDelete->GetBlockHash());
        if (DisconnectBlock(block, pindexDelete, view) != DISCONNECT_OK)
            return error("DisconnectTip(): DisconnectBlock %s failed", pindexDelete->GetBlockHash().ToString());
        bool flushed = view.Flush();
        assert(flushed);
    }
    LogPrint(BCLog::BENCH, "- Disconnect block: %.2fms\n", (GetTimeMicros() - nStart) * MILLI);
//如有必要，将链状态写入磁盘。
    if (!FlushStateToDisk(chainparams, state, FlushStateMode::IF_NEEDED))
        return false;

    if (disconnectpool) {
//保存事务以便在reorg结束时重新添加到mempool
        for (auto it = block.vtx.rbegin(); it != block.vtx.rend(); ++it) {
            disconnectpool->addTransaction(*it);
        }
        while (disconnectpool->DynamicMemoryUsage() > MAX_DISCONNECTED_TX_POOL_SIZE * 1000) {
//删除最早的条目，并将其子项从mempool中删除。
            auto it = disconnectpool->queuedTx.get<insertion_order>().begin();
            mempool.removeRecursive(**it, MemPoolRemovalReason::REORG);
            disconnectpool->removeEntry(it);
        }
    }

    chainActive.SetTip(pindexDelete->pprev);

    UpdateTip(pindexDelete->pprev, chainparams);
//让钱包知道交易从1确认到
//0-确认或冲突：
    GetMainSignals().BlockDisconnected(pblock);
    return true;
}

static int64_t nTimeReadFromDisk = 0;
static int64_t nTimeConnectTotal = 0;
static int64_t nTimeFlush = 0;
static int64_t nTimeChainState = 0;
static int64_t nTimePostConnect = 0;

struct PerBlockConnectTrace {
    CBlockIndex* pindex = nullptr;
    std::shared_ptr<const CBlock> pblock;
    std::shared_ptr<std::vector<CTransactionRef>> conflictedTxs;
    PerBlockConnectTrace() : conflictedTxs(std::make_shared<std::vector<CTransactionRef>>()) {}
};
/*
 *用于跟踪其事务作为
 *单个ActivateBestChainStep调用的一部分。
 *
 *此类还跟踪从mempool中删除的事务
 *冲突（每个块），可用于传递所有这些事务
 *通过SyncTransaction。
 *
 *此类假定（并断言）给定事务的冲突事务
 *在关联的blockConnected（）之前，通过mempool回调添加块。
 *这些交易。如果任何交易被标记为冲突，则为
 *假设始终添加关联块。
 *
 *这个类是一次性的，一旦调用getBlocksConnected（），就必须抛出
 *把它拿走，换一个新的。
 **/

class ConnectTrace {
private:
    std::vector<PerBlockConnectTrace> blocksConnected;
    CTxMemPool &pool;
    boost::signals2::scoped_connection m_connNotifyEntryRemoved;

public:
    explicit ConnectTrace(CTxMemPool &_pool) : blocksConnected(1), pool(_pool) {
        m_connNotifyEntryRemoved = pool.NotifyEntryRemoved.connect(std::bind(&ConnectTrace::NotifyEntryRemoved, this, std::placeholders::_1, std::placeholders::_2));
    }

    void BlockConnected(CBlockIndex* pindex, std::shared_ptr<const CBlock> pblock) {
        assert(!blocksConnected.back().pindex);
        assert(pindex);
        assert(pblock);
        blocksConnected.back().pindex = pindex;
        blocksConnected.back().pblock = std::move(pblock);
        blocksConnected.emplace_back();
    }

    std::vector<PerBlockConnectTrace>& GetBlocksConnected() {
//我们总是在名单的最后多留一个街区，因为
//在所有冲突事务
//已填写。因此，最后一个条目应该始终为空
//一个等待下一个块的事务。我们流行
//这里最后一个确保我们返回的列表是健全的条目。
        assert(!blocksConnected.back().pindex);
        assert(blocksConnected.back().conflictedTxs->empty());
        blocksConnected.pop_back();
        return blocksConnected;
    }

    void NotifyEntryRemoved(CTransactionRef txRemoved, MemPoolRemovalReason reason) {
        assert(!blocksConnected.back().pindex);
        if (reason == MemPoolRemovalReason::CONFLICT) {
            blocksConnected.back().conflictedTxs->emplace_back(std::move(txRemoved));
        }
    }
};

/*
 *将新块连接到chainactive。pblock是nullptr或指向cblock的指针
 *对应pindexnew，绕过从磁盘重新加载。
 *
 *如果连接成功，该块将添加到ConnectTrace。
 **/

bool CChainState::ConnectTip(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexNew, const std::shared_ptr<const CBlock>& pblock, ConnectTrace& connectTrace, DisconnectedBlockTransactions &disconnectpool)
{
    assert(pindexNew->pprev == chainActive.Tip());
//从磁盘读取块。
    int64_t nTime1 = GetTimeMicros();
    std::shared_ptr<const CBlock> pthisBlock;
    if (!pblock) {
        std::shared_ptr<CBlock> pblockNew = std::make_shared<CBlock>();
        if (!ReadBlockFromDisk(*pblockNew, pindexNew, chainparams.GetConsensus()))
            return AbortNode(state, "Failed to read block");
        pthisBlock = pblockNew;
    } else {
        pthisBlock = pblock;
    }
    const CBlock& blockConnecting = *pthisBlock;
//原子地将块应用于链状态。
    int64_t nTime2 = GetTimeMicros(); nTimeReadFromDisk += nTime2 - nTime1;
    int64_t nTime3;
    LogPrint(BCLog::BENCH, "  - Load block from disk: %.2fms [%.2fs]\n", (nTime2 - nTime1) * MILLI, nTimeReadFromDisk * MICRO);
    {
        CCoinsViewCache view(pcoinsTip.get());
        bool rv = ConnectBlock(blockConnecting, state, pindexNew, view, chainparams);
        GetMainSignals().BlockChecked(blockConnecting, state);
        if (!rv) {
            if (state.IsInvalid())
                InvalidBlockFound(pindexNew, state);
            return error("%s: ConnectBlock %s failed, %s", __func__, pindexNew->GetBlockHash().ToString(), FormatStateMessage(state));
        }
        nTime3 = GetTimeMicros(); nTimeConnectTotal += nTime3 - nTime2;
        LogPrint(BCLog::BENCH, "  - Connect total: %.2fms [%.2fs (%.2fms/blk)]\n", (nTime3 - nTime2) * MILLI, nTimeConnectTotal * MICRO, nTimeConnectTotal * MILLI / nBlocksTotal);
        bool flushed = view.Flush();
        assert(flushed);
    }
    int64_t nTime4 = GetTimeMicros(); nTimeFlush += nTime4 - nTime3;
    LogPrint(BCLog::BENCH, "  - Flush: %.2fms [%.2fs (%.2fms/blk)]\n", (nTime4 - nTime3) * MILLI, nTimeFlush * MICRO, nTimeFlush * MILLI / nBlocksTotal);
//如有必要，将链状态写入磁盘。
    if (!FlushStateToDisk(chainparams, state, FlushStateMode::IF_NEEDED))
        return false;
    int64_t nTime5 = GetTimeMicros(); nTimeChainState += nTime5 - nTime4;
    LogPrint(BCLog::BENCH, "  - Writing chainstate: %.2fms [%.2fs (%.2fms/blk)]\n", (nTime5 - nTime4) * MILLI, nTimeChainState * MICRO, nTimeChainState * MILLI / nBlocksTotal);
//从内存池中删除冲突事务。
    mempool.removeForBlock(blockConnecting.vtx, pindexNew->nHeight);
    disconnectpool.removeForBlock(blockConnecting.vtx);
//更新chainactive和相关变量。
    chainActive.SetTip(pindexNew);
    UpdateTip(pindexNew, chainparams);

    int64_t nTime6 = GetTimeMicros(); nTimePostConnect += nTime6 - nTime5; nTimeTotal += nTime6 - nTime1;
    LogPrint(BCLog::BENCH, "  - Connect postprocess: %.2fms [%.2fs (%.2fms/blk)]\n", (nTime6 - nTime5) * MILLI, nTimePostConnect * MICRO, nTimePostConnect * MILLI / nBlocksTotal);
    LogPrint(BCLog::BENCH, "- Connect block: %.2fms [%.2fs (%.2fms/blk)]\n", (nTime6 - nTime1) * MILLI, nTimeTotal * MICRO, nTimeTotal * MILLI / nBlocksTotal);

    connectTrace.BlockConnected(pindexNew, std::move(pthisBlock));
    return true;
}

/*
 *把链子的顶端放回最里面的活，这不是
 *已知无效（但还不确定是否有效）。
 **/

CBlockIndex* CChainState::FindMostWorkChain() {
    do {
        CBlockIndex *pindexNew = nullptr;

//找到最佳候选标题。
        {
            std::set<CBlockIndex*, CBlockIndexWorkComparator>::reverse_iterator it = setBlockIndexCandidates.rbegin();
            if (it == setBlockIndexCandidates.rend())
                return nullptr;
            pindexNew = *it;
        }

//检查当前活动链和候选链之间路径上的所有块是否有效。
//直到活动链是一个优化，因为我们知道其中的所有块都已经有效。
        CBlockIndex *pindexTest = pindexNew;
        bool fInvalidAncestor = false;
        while (pindexTest && !chainActive.Contains(pindexTest)) {
            assert(pindexTest->HaveTxsDownloaded() || pindexTest->nHeight == 0);

//修剪后的节点可能在setBlockIndexCandidates中具有项
//哪些块文件已被删除。将那些作为候选对象删除
//对于大多数的工作链，如果我们遇到他们，我们就不能转换
//到一个链，除非我们有所有非活动链父块。
            bool fFailedChain = pindexTest->nStatus & BLOCK_FAILED_MASK;
            bool fMissingData = !(pindexTest->nStatus & BLOCK_HAVE_DATA);
            if (fFailedChain || fMissingData) {
//候选链不可用（无效或缺少数据）
                if (fFailedChain && (pindexBestInvalid == nullptr || pindexNew->nChainWork > pindexBestInvalid->nChainWork))
                    pindexBestInvalid = pindexNew;
                CBlockIndex *pindexFailed = pindexNew;
//从装置上拆下整个链条。
                while (pindexTest != pindexFailed) {
                    if (fFailedChain) {
                        pindexFailed->nStatus |= BLOCK_FAILED_CHILD;
                    } else if (fMissingData) {
//如果缺少数据，则将其添加回mapblocksunlinked，
//这样，如果块将来到达，我们可以尝试添加
//再次设置blockindexcandients。
                        mapBlocksUnlinked.insert(std::make_pair(pindexFailed->pprev, pindexFailed));
                    }
                    setBlockIndexCandidates.erase(pindexFailed);
                    pindexFailed = pindexFailed->pprev;
                }
                setBlockIndexCandidates.erase(pindexTest);
                fInvalidAncestor = true;
                break;
            }
            pindexTest = pindexTest->pprev;
        }
        if (!fInvalidAncestor)
            return pindexNew;
    } while(true);
}

/*删除setblockindexcandidates中所有比当前提示更差的条目。*/
void CChainState::PruneBlockIndexCandidates() {
//注意，我们不能删除当前块本身，因为我们可能需要稍后返回到它，以防
//重组到更好的区块失败。
    std::set<CBlockIndex*, CBlockIndexWorkComparator>::iterator it = setBlockIndexCandidates.begin();
    while (it != setBlockIndexCandidates.end() && setBlockIndexCandidates.value_comp()(*it, chainActive.Tip())) {
        setBlockIndexCandidates.erase(it++);
    }
//无论是当前的提示，还是我们正在努力的后续提示，都将保留在setblockindex候选项中。
    assert(!setBlockIndexCandidates.empty());
}

/*
 *尝试在使PindexMosNetwork成为活动块方面取得一些进展。
 *pblock是nullptr或指向对应于pindexmostwork的cblock的指针。
 **/

bool CChainState::ActivateBestChainStep(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexMostWork, const std::shared_ptr<const CBlock>& pblock, bool& fInvalidFound, ConnectTrace& connectTrace)
{
    AssertLockHeld(cs_main);

    const CBlockIndex *pindexOldTip = chainActive.Tip();
    const CBlockIndex *pindexFork = chainActive.FindFork(pindexMostWork);

//断开不再位于最佳链中的活动块。
    bool fBlocksDisconnected = false;
    DisconnectedBlockTransactions disconnectpool;
    while (chainActive.Tip() && chainActive.Tip() != pindexFork) {
        if (!DisconnectTip(state, chainparams, &disconnectpool)) {
//这可能是一个致命的错误，但是保持内存池的一致性，
//以防万一。仅在这种情况下从mempool中移除。
            UpdateMempoolForReorg(disconnectpool, false);
            return false;
        }
        fBlocksDisconnected = true;
    }

//生成要连接的新块的列表。
    std::vector<CBlockIndex*> vpindexToConnect;
    bool fContinue = true;
    int nHeight = pindexFork ? pindexFork->nHeight : -1;
    while (fContinue && nHeight != pindexMostWork->nHeight) {
//不要重复整个潜在改进列表以获得最佳提示，因为我们可能只需要
//一路上过几个街区。
        int nTargetHeight = std::min(nHeight + 32, pindexMostWork->nHeight);
        vpindexToConnect.clear();
        vpindexToConnect.reserve(nTargetHeight - nHeight);
        CBlockIndex *pindexIter = pindexMostWork->GetAncestor(nTargetHeight);
        while (pindexIter && pindexIter->nHeight != nHeight) {
            vpindexToConnect.push_back(pindexIter);
            pindexIter = pindexIter->pprev;
        }
        nHeight = nTargetHeight;

//连接新块。
        for (CBlockIndex *pindexConnect : reverse_iterate(vpindexToConnect)) {
            if (!ConnectTip(state, chainparams, pindexConnect, pindexConnect == pindexMostWork ? pblock : std::shared_ptr<const CBlock>(), connectTrace, disconnectpool)) {
                if (state.IsInvalid()) {
//阻止违反了共识规则。
                    if (!state.CorruptionPossible()) {
                        InvalidChainFound(vpindexToConnect.front());
                    }
                    state = CValidationState();
                    fInvalidFound = true;
                    fContinue = false;
                    break;
                } else {
//出现系统错误（磁盘空间、数据库错误…）。
//使mempool与当前提示保持一致，以防万一
//任何观察者都会在关机前使用它。
                    UpdateMempoolForReorg(disconnectpool, false);
                    return false;
                }
            } else {
                PruneBlockIndexCandidates();
                if (!pindexOldTip || chainActive.Tip()->nChainWork > pindexOldTip->nChainWork) {
//我们的处境比以前好多了。暂时返回以释放锁。
                    fContinue = false;
                    break;
                }
            }
        }
    }

    if (fBlocksDisconnected) {
//如果断开了任何块的连接，则disconnectpool可以是非空的。添加
//任何断开连接的事务都返回到mempool。
        UpdateMempoolForReorg(disconnectpool, true);
    }
    mempool.check(pcoinsTip.get());

//新的最佳链的回调/通知。
    if (fInvalidFound)
        CheckForkWarningConditionsOnNewFork(vpindexToConnect.back());
    else
        CheckForkWarningConditions();

    return true;
}

static void NotifyHeaderTip() LOCKS_EXCLUDED(cs_main) {
    bool fNotify = false;
    bool fInitialBlockDownload = false;
    static CBlockIndex* pindexHeaderOld = nullptr;
    CBlockIndex* pindexHeader = nullptr;
    {
        LOCK(cs_main);
        pindexHeader = pindexBestHeader;

        if (pindexHeader != pindexHeaderOld) {
            fNotify = true;
            fInitialBlockDownload = IsInitialBlockDownload();
            pindexHeaderOld = pindexHeader;
        }
    }
//发送块提示更改通知而不使用cs_-main
    if (fNotify) {
        uiInterface.NotifyHeaderTip(fInitialBlockDownload, pindexHeader);
    }
}

/*
 *通过多个步骤激活最佳链。结果要么是失败
 *或激活的最佳链。pblock是nullptr或指向块的指针
 *已加载（以避免再次从磁盘加载）。
 *
 *ActivateBestChain分为多个步骤（请参见ActivateBestChainStep），以便
 *我们避免长时间持有cs_Main；此长度
 *调用可能在重新索引或大量重新索引期间很长时间。
 **/

bool CChainState::ActivateBestChain(CValidationState &state, const CChainParams& chainparams, std::shared_ptr<const CBlock> pblock) {
//注意，虽然我们经常从processnewblock调用此函数，但这是
//远远没有保证。p2p/rpc中的内容通常最终会调用
//US处于进程中间newblock-不要假定已设置pblock
//明智的表现或正确性！
    AssertLockNotHeld(cs_main);

//ABC在计算内部状态方面保持了相当高的成本
//因为这个函数周期性地释放cs_main，这样它就不会锁定其他线程太长时间。
//在大型连接期间-并允许回调队列排出
//我们使用m_cs_chainstate强制互斥，这样一次只能有一个调用方执行此函数
    LOCK(m_cs_chainstate);

    CBlockIndex *pindexMostWork = nullptr;
    CBlockIndex *pindexNewTip = nullptr;
    int nStopAtHeight = gArgs.GetArg("-stopatheight", DEFAULT_STOPATHEIGHT);
    do {
        boost::this_thread::interruption_point();

        if (GetMainSignals().CallbacksPending() > 10) {
//阻塞，直到验证队列排出。这在很大程度上应该
//在正常操作中不会发生，但可能发生在
//重新索引，如果我们跑得太远，就会导致内存膨胀。
//注意，如果validationInterface回调最后调用
//activatebestchain这可能导致死锁！我们应该
//将来可能会对此进行调试锁定测试。
            SyncWithValidationInterfaceQueue();
        }

        {
            LOCK(cs_main);
            CBlockIndex* starting_tip = chainActive.Tip();
            bool blocks_connected = false;
            do {
//在取得进展之前，我们绝对不可能解锁CS-MAIN。
//（由于硬件问题、磁盘空间不足等原因导致的关机除外）。
ConnectTrace connectTrace(mempool); //在cs_main解锁前销毁

                if (pindexMostWork == nullptr) {
                    pindexMostWork = FindMostWorkChain();
                }

//不管我们有什么事要做。
                if (pindexMostWork == nullptr || pindexMostWork == chainActive.Tip()) {
                    break;
                }

                bool fInvalidFound = false;
                std::shared_ptr<const CBlock> nullBlockPtr;
                if (!ActivateBestChainStep(state, chainparams, pindexMostWork, pblock && pblock->GetHash() == pindexMostWork->GetBlockHash() ? pblock : nullBlockPtr, fInvalidFound, connectTrace))
                    return false;
                blocks_connected = true;

                if (fInvalidFound) {
//擦除缓存，我们现在可能需要另一个分支。
                    pindexMostWork = nullptr;
                }
                pindexNewTip = chainActive.Tip();

                for (const PerBlockConnectTrace& trace : connectTrace.GetBlocksConnected()) {
                    assert(trace.pblock && trace.pindex);
                    GetMainSignals().BlockConnected(trace.pblock, trace.pindex, trace.conflictedTxs);
                }
            } while (!chainActive.Tip() || (starting_tip && CBlockIndexWorkComparator()(chainActive.Tip(), starting_tip)));
            if (!blocks_connected) return true;

            const CBlockIndex* pindexFork = chainActive.FindFork(starting_tip);
            bool fInitialDownload = IsInitialBlockDownload();

//通知外部侦听器有关新提示的信息。
//在保持cs_main的同时排队，以确保按块连接的顺序调用updatedBlockTip
            if (pindexFork != pindexNewTip) {
//通知validationInterface订阅服务器
                GetMainSignals().UpdatedBlockTip(pindexNewTip, pindexFork, fInitialDownload);

//如果连接了新的块提示，则始终通知用户界面
                uiInterface.NotifyBlockTip(fInitialDownload, pindexNewTip);
            }
        }
//当我们到达这一点时，我们切换到一个新的提示（存储在pindexnewtip中）。

        if (nStopAtHeight && pindexNewTip && pindexNewTip->nHeight >= nStopAtHeight) StartShutdown();

//只有在给activatebestchainstep一次运行的机会之后，我们才检查关闭，以便
//在加载链提示（）期间，在连接Genesis块之前切勿关闭。在此之前
//在关闭期间导致了assert（）失败，例如utxo db刷新检查
//最好的块散列是非空的。
        if (ShutdownRequested())
            break;
    } while (pindexNewTip != pindexMostWork);
    CheckBlockIndex(chainparams.GetConsensus());

//在中继之后，定期将更改写入磁盘。
    if (!FlushStateToDisk(chainparams, state, FlushStateMode::PERIODIC)) {
        return false;
    }

    return true;
}

bool ActivateBestChain(CValidationState &state, const CChainParams& chainparams, std::shared_ptr<const CBlock> pblock) {
    return g_chainstate.ActivateBestChain(state, chainparams, std::move(pblock));
}

bool CChainState::PreciousBlock(CValidationState& state, const CChainParams& params, CBlockIndex *pindex)
{
    {
        LOCK(cs_main);
        if (pindex->nChainWork < chainActive.Tip()->nChainWork) {
//没什么事，这个方块不在尖端。
            return true;
        }
        if (chainActive.Tip()->nChainWork > nLastPreciousChainwork) {
//上次调用后链已被扩展，请重置计数器。
            nBlockReverseSequenceId = -1;
        }
        nLastPreciousChainwork = chainActive.Tip()->nChainWork;
        setBlockIndexCandidates.erase(pindex);
        pindex->nSequenceId = nBlockReverseSequenceId;
        if (nBlockReverseSequenceId > std::numeric_limits<int32_t>::min()) {
//如果有人真的想，我们不能一直减少柜台的数量
//用同一套提示拨打PreciousBlock 2**31-1次…
            nBlockReverseSequenceId--;
        }
        if (pindex->IsValid(BLOCK_VALID_TRANSACTIONS) && pindex->HaveTxsDownloaded()) {
            setBlockIndexCandidates.insert(pindex);
            PruneBlockIndexCandidates();
        }
    }

    return ActivateBestChain(state, params, std::shared_ptr<const CBlock>());
}
bool PreciousBlock(CValidationState& state, const CChainParams& params, CBlockIndex *pindex) {
    return g_chainstate.PreciousBlock(state, params, pindex);
}

bool CChainState::InvalidateBlock(CValidationState& state, const CChainParams& chainparams, CBlockIndex *pindex)
{
    AssertLockHeld(cs_main);

//我们首先向后断开连接，然后将块标记为无效。
//这可以防止修剪后的节点可能无法使块失效。
//因为没有小费候选人，所以不能开始
//是否没有符合“有数据且不是无效的”的块
//status“包含在setblockindexcandidates中的标准”）。

    bool pindex_was_in_chain = false;
    CBlockIndex *invalid_walk_tip = chainActive.Tip();

    DisconnectedBlockTransactions disconnectpool;
    while (chainActive.Contains(pindex)) {
        pindex_was_in_chain = true;
//activatebestchain考虑chainactive中已经存在的块
//已经无条件有效，所以强制断开它。
        if (!DisconnectTip(state, chainparams, &disconnectpool)) {
//试图让记忆库保持一致可能是没有希望的。
//这里如果disconnecttip失败，我们可以尝试。
            UpdateMempoolForReorg(disconnectpool, false);
            return false;
        }
    }

//现在，将刚刚断开的块标记为后代无效
//（注意，这可能不是所有后代）。
    while (pindex_was_in_chain && invalid_walk_tip != pindex) {
        invalid_walk_tip->nStatus |= BLOCK_FAILED_CHILD;
        setDirtyBlockIndex.insert(invalid_walk_tip);
        setBlockIndexCandidates.erase(invalid_walk_tip);
        invalid_walk_tip = invalid_walk_tip->pprev;
    }

//将块本身标记为无效。
    pindex->nStatus |= BLOCK_FAILED_VALID;
    setDirtyBlockIndex.insert(pindex);
    setBlockIndexCandidates.erase(pindex);
    m_failed_blocks.insert(pindex);

//disconnecttip将向disconnectpool添加事务；请尝试添加这些事务
//回到记忆池。
    UpdateMempoolForReorg(disconnectpool, true);

//由此产生的新最佳提示可能不再存在于setblockindexcandidate中，因此
//再加一次。
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->HaveTxsDownloaded() && !setBlockIndexCandidates.value_comp()(it->second, chainActive.Tip())) {
            setBlockIndexCandidates.insert(it->second);
        }
        it++;
    }

    InvalidChainFound(pindex);

//只有在修改活动链时才通知新的块提示。
    if (pindex_was_in_chain) {
        uiInterface.NotifyBlockTip(IsInitialBlockDownload(), pindex->pprev);
    }
    return true;
}

bool InvalidateBlock(CValidationState& state, const CChainParams& chainparams, CBlockIndex *pindex) {
    return g_chainstate.InvalidateBlock(state, chainparams, pindex);
}

void CChainState::ResetBlockFailureFlags(CBlockIndex *pindex) {
    AssertLockHeld(cs_main);

    int nHeight = pindex->nHeight;

//从该块及其所有子块中删除invalidity标志。
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (!it->second->IsValid() && it->second->GetAncestor(nHeight) == pindex) {
            it->second->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(it->second);
            if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->HaveTxsDownloaded() && setBlockIndexCandidates.value_comp()(chainActive.Tip(), it->second)) {
                setBlockIndexCandidates.insert(it->second);
            }
            if (it->second == pindexBestInvalid) {
//如果块标记指向其中一个，则重置无效块标记。
                pindexBestInvalid = nullptr;
            }
            m_failed_blocks.erase(it->second);
        }
        it++;
    }

//也从所有祖先中删除无效标志。
    while (pindex != nullptr) {
        if (pindex->nStatus & BLOCK_FAILED_MASK) {
            pindex->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(pindex);
            m_failed_blocks.erase(pindex);
        }
        pindex = pindex->pprev;
    }
}

void ResetBlockFailureFlags(CBlockIndex *pindex) {
    return g_chainstate.ResetBlockFailureFlags(pindex);
}

CBlockIndex* CChainState::AddToBlockIndex(const CBlockHeader& block)
{
    AssertLockHeld(cs_main);

//检查是否重复
    uint256 hash = block.GetHash();
    BlockMap::iterator it = mapBlockIndex.find(hash);
    if (it != mapBlockIndex.end())
        return it->second;

//构造新的块索引对象
    CBlockIndex* pindexNew = new CBlockIndex(block);
//我们只在完整数据可用时将序列ID分配给块，
//为了避免矿工扣留块，但广播头，以获得
//竞争优势。
    pindexNew->nSequenceId = 0;
    BlockMap::iterator mi = mapBlockIndex.insert(std::make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);
    BlockMap::iterator miPrev = mapBlockIndex.find(block.hashPrevBlock);
    if (miPrev != mapBlockIndex.end())
    {
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
        pindexNew->BuildSkip();
    }
    pindexNew->nTimeMax = (pindexNew->pprev ? std::max(pindexNew->pprev->nTimeMax, pindexNew->nTime) : pindexNew->nTime);
    pindexNew->nChainWork = (pindexNew->pprev ? pindexNew->pprev->nChainWork : 0) + GetBlockProof(*pindexNew);
    pindexNew->RaiseValidity(BLOCK_VALID_TREE);
    if (pindexBestHeader == nullptr || pindexBestHeader->nChainWork < pindexNew->nChainWork)
        pindexBestHeader = pindexNew;

    setDirtyBlockIndex.insert(pindexNew);

    return pindexNew;
}

/*将一个块标记为已接收并检查其数据（最多为块有效事务）。*/
void CChainState::ReceivedBlockTransactions(const CBlock& block, CBlockIndex* pindexNew, const CDiskBlockPos& pos, const Consensus::Params& consensusParams)
{
    pindexNew->nTx = block.vtx.size();
    pindexNew->nChainTx = 0;
    pindexNew->nFile = pos.nFile;
    pindexNew->nDataPos = pos.nPos;
    pindexNew->nUndoPos = 0;
    pindexNew->nStatus |= BLOCK_HAVE_DATA;
    if (IsWitnessEnabled(pindexNew->pprev, consensusParams)) {
        pindexNew->nStatus |= BLOCK_OPT_WITNESS;
    }
    pindexNew->RaiseValidity(BLOCK_VALID_TRANSACTIONS);
    setDirtyBlockIndex.insert(pindexNew);

    if (pindexNew->pprev == nullptr || pindexNew->pprev->HaveTxsDownloaded()) {
//如果pindexnew是genesis块，或者所有父类都是块有效的事务。
        std::deque<CBlockIndex*> queue;
        queue.push_back(pindexNew);

//递归地处理现在可以连接的任何子代块。
        while (!queue.empty()) {
            CBlockIndex *pindex = queue.front();
            queue.pop_front();
            pindex->nChainTx = (pindex->pprev ? pindex->pprev->nChainTx : 0) + pindex->nTx;
            {
                LOCK(cs_nBlockSequenceId);
                pindex->nSequenceId = nBlockSequenceId++;
            }
            if (chainActive.Tip() == nullptr || !setBlockIndexCandidates.value_comp()(pindex, chainActive.Tip())) {
                setBlockIndexCandidates.insert(pindex);
            }
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = mapBlocksUnlinked.equal_range(pindex);
            while (range.first != range.second) {
                std::multimap<CBlockIndex*, CBlockIndex*>::iterator it = range.first;
                queue.push_back(it->second);
                range.first++;
                mapBlocksUnlinked.erase(it);
            }
        }
    } else {
        if (pindexNew->pprev && pindexNew->pprev->IsValid(BLOCK_VALID_TREE)) {
            mapBlocksUnlinked.insert(std::make_pair(pindexNew->pprev, pindexNew));
        }
    }
}

static bool FindBlockPos(CDiskBlockPos &pos, unsigned int nAddSize, unsigned int nHeight, uint64_t nTime, bool fKnown = false)
{
    LOCK(cs_LastBlockFile);

    unsigned int nFile = fKnown ? pos.nFile : nLastBlockFile;
    if (vinfoBlockFile.size() <= nFile) {
        vinfoBlockFile.resize(nFile + 1);
    }

    if (!fKnown) {
        while (vinfoBlockFile[nFile].nSize + nAddSize >= MAX_BLOCKFILE_SIZE) {
            nFile++;
            if (vinfoBlockFile.size() <= nFile) {
                vinfoBlockFile.resize(nFile + 1);
            }
        }
        pos.nFile = nFile;
        pos.nPos = vinfoBlockFile[nFile].nSize;
    }

    if ((int)nFile != nLastBlockFile) {
        if (!fKnown) {
            LogPrintf("Leaving block file %i: %s\n", nLastBlockFile, vinfoBlockFile[nLastBlockFile].ToString());
        }
        FlushBlockFile(!fKnown);
        nLastBlockFile = nFile;
    }

    vinfoBlockFile[nFile].AddBlock(nHeight, nTime);
    if (fKnown)
        vinfoBlockFile[nFile].nSize = std::max(pos.nPos + nAddSize, vinfoBlockFile[nFile].nSize);
    else
        vinfoBlockFile[nFile].nSize += nAddSize;

    if (!fKnown) {
        unsigned int nOldChunks = (pos.nPos + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        unsigned int nNewChunks = (vinfoBlockFile[nFile].nSize + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        if (nNewChunks > nOldChunks) {
            if (fPruneMode)
                fCheckForPruning = true;
            if (CheckDiskSpace(nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos, true)) {
                FILE *file = OpenBlockFile(pos);
                if (file) {
                    LogPrintf("Pre-allocating up to position 0x%x in blk%05u.dat\n", nNewChunks * BLOCKFILE_CHUNK_SIZE, pos.nFile);
                    AllocateFileRange(file, pos.nPos, nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos);
                    fclose(file);
                }
            }
            else
                return error("out of disk space");
        }
    }

    setDirtyFileInfo.insert(nFile);
    return true;
}

static bool FindUndoPos(CValidationState &state, int nFile, CDiskBlockPos &pos, unsigned int nAddSize)
{
    pos.nFile = nFile;

    LOCK(cs_LastBlockFile);

    unsigned int nNewSize;
    pos.nPos = vinfoBlockFile[nFile].nUndoSize;
    nNewSize = vinfoBlockFile[nFile].nUndoSize += nAddSize;
    setDirtyFileInfo.insert(nFile);

    unsigned int nOldChunks = (pos.nPos + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    unsigned int nNewChunks = (nNewSize + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    if (nNewChunks > nOldChunks) {
        if (fPruneMode)
            fCheckForPruning = true;
        if (CheckDiskSpace(nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos, true)) {
            FILE *file = OpenUndoFile(pos);
            if (file) {
                LogPrintf("Pre-allocating up to position 0x%x in rev%05u.dat\n", nNewChunks * UNDOFILE_CHUNK_SIZE, pos.nFile);
                AllocateFileRange(file, pos.nPos, nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos);
                fclose(file);
            }
        }
        else
            return state.Error("out of disk space");
    }

    return true;
}

static bool CheckBlockHeader(const CBlockHeader& block, CValidationState& state, const Consensus::Params& consensusParams, bool fCheckPOW = true)
{
//检查工作证明与索赔金额是否相符
    if (fCheckPOW && !CheckProofOfWork(block.GetHash(), block.nBits, consensusParams))
        return state.DoS(50, false, REJECT_INVALID, "high-hash", false, "proof of work failed");

    return true;
}

bool CheckBlock(const CBlock& block, CValidationState& state, const Consensus::Params& consensusParams, bool fCheckPOW, bool fCheckMerkleRoot)
{
//这些是独立于上下文的检查。

    if (block.fChecked)
        return true;

//检查收割台是否有效（尤其是POW）。这主要是
//与acceptBlockHeader中的调用冗余。
    if (!CheckBlockHeader(block, state, consensusParams, fCheckPOW))
        return false;

//检查梅克尔根。
    if (fCheckMerkleRoot) {
        bool mutated;
        uint256 hashMerkleRoot2 = BlockMerkleRoot(block, &mutated);
        if (block.hashMerkleRoot != hashMerkleRoot2)
            return state.DoS(100, false, REJECT_INVALID, "bad-txnmrklroot", true, "hashMerkleRoot mismatch");

//检查Merkle Tree延展性（CVE-2012-2459）：重复序列
//不影响块的merkle根的块中的事务，
//但仍然无效。
        if (mutated)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-duplicate", true, "duplicate transaction");
    }

//所有潜在的腐败验证必须在我们进行任何
//事务验证，否则我们可能会将头标记为无效
//因为我们收到了错误的交易。
//请注意，见证延展性是在上下文检查块中检查的，因此不是
//可在此处执行使用见证数据的检查。

//尺寸限制
    if (block.vtx.empty() || block.vtx.size() * WITNESS_SCALE_FACTOR > MAX_BLOCK_WEIGHT || ::GetSerializeSize(block, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * WITNESS_SCALE_FACTOR > MAX_BLOCK_WEIGHT)
        return state.DoS(100, false, REJECT_INVALID, "bad-blk-length", false, "size limits failed");

//第一个事务必须是CoinBase，其余事务不能是
    if (block.vtx.empty() || !block.vtx[0]->IsCoinBase())
        return state.DoS(100, false, REJECT_INVALID, "bad-cb-missing", false, "first tx is not coinbase");
    for (unsigned int i = 1; i < block.vtx.size(); i++)
        if (block.vtx[i]->IsCoinBase())
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-multiple", false, "more than one coinbase");

//检查交易记录
    for (const auto& tx : block.vtx)
        if (!CheckTransaction(*tx, state, true))
            return state.Invalid(false, state.GetRejectCode(), state.GetRejectReason(),
                                 strprintf("Transaction check failed (tx hash %s) %s", tx->GetHash().ToString(), state.GetDebugMessage()));

    unsigned int nSigOps = 0;
    for (const auto& tx : block.vtx)
    {
        nSigOps += GetLegacySigOpCount(*tx);
    }
    if (nSigOps * WITNESS_SCALE_FACTOR > MAX_BLOCK_SIGOPS_COST)
        return state.DoS(100, false, REJECT_INVALID, "bad-blk-sigops", false, "out-of-bounds SigOpCount");

    if (fCheckPOW && fCheckMerkleRoot)
        block.fChecked = true;

    return true;
}

bool IsWitnessEnabled(const CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    LOCK(cs_main);
    return (VersionBitsState(pindexPrev, params, Consensus::DEPLOYMENT_SEGWIT, versionbitscache) == ThresholdState::ACTIVE);
}

bool IsNullDummyEnabled(const CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    LOCK(cs_main);
    return (VersionBitsState(pindexPrev, params, Consensus::DEPLOYMENT_SEGWIT, versionbitscache) == ThresholdState::ACTIVE);
}

//计算块的coinbase事务的哪个vout作为见证
//承诺发生，或者如果找不到-1。
static int GetWitnessCommitmentIndex(const CBlock& block)
{
    int commitpos = -1;
    if (!block.vtx.empty()) {
        for (size_t o = 0; o < block.vtx[0]->vout.size(); o++) {
            if (block.vtx[0]->vout[o].scriptPubKey.size() >= 38 && block.vtx[0]->vout[o].scriptPubKey[0] == OP_RETURN && block.vtx[0]->vout[o].scriptPubKey[1] == 0x24 && block.vtx[0]->vout[o].scriptPubKey[2] == 0xaa && block.vtx[0]->vout[o].scriptPubKey[3] == 0x21 && block.vtx[0]->vout[o].scriptPubKey[4] == 0xa9 && block.vtx[0]->vout[o].scriptPubKey[5] == 0xed) {
                commitpos = o;
            }
        }
    }
    return commitpos;
}

void UpdateUncommittedBlockStructures(CBlock& block, const CBlockIndex* pindexPrev, const Consensus::Params& consensusParams)
{
    int commitpos = GetWitnessCommitmentIndex(block);
    static const std::vector<unsigned char> nonce(32, 0x00);
    if (commitpos != -1 && IsWitnessEnabled(pindexPrev, consensusParams) && !block.vtx[0]->HasWitness()) {
        CMutableTransaction tx(*block.vtx[0]);
        tx.vin[0].scriptWitness.stack.resize(1);
        tx.vin[0].scriptWitness.stack[0] = nonce;
        block.vtx[0] = MakeTransactionRef(std::move(tx));
    }
}

std::vector<unsigned char> GenerateCoinbaseCommitment(CBlock& block, const CBlockIndex* pindexPrev, const Consensus::Params& consensusParams)
{
    std::vector<unsigned char> commitment;
    int commitpos = GetWitnessCommitmentIndex(block);
    std::vector<unsigned char> ret(32, 0x00);
    if (consensusParams.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout != 0) {
        if (commitpos == -1) {
            uint256 witnessroot = BlockWitnessMerkleRoot(block, nullptr);
            CHash256().Write(witnessroot.begin(), 32).Write(ret.data(), 32).Finalize(witnessroot.begin());
            CTxOut out;
            out.nValue = 0;
            out.scriptPubKey.resize(38);
            out.scriptPubKey[0] = OP_RETURN;
            out.scriptPubKey[1] = 0x24;
            out.scriptPubKey[2] = 0xaa;
            out.scriptPubKey[3] = 0x21;
            out.scriptPubKey[4] = 0xa9;
            out.scriptPubKey[5] = 0xed;
            memcpy(&out.scriptPubKey[6], witnessroot.begin(), 32);
            commitment = std::vector<unsigned char>(out.scriptPubKey.begin(), out.scriptPubKey.end());
            CMutableTransaction tx(*block.vtx[0]);
            tx.vout.push_back(out);
            block.vtx[0] = MakeTransactionRef(std::move(tx));
        }
    }
    UpdateUncommittedBlockStructures(block, pindexPrev, consensusParams);
    return commitment;
}

/*上下文相关的有效性检查。
 *通过“context”，我们只指前面的块头，而不是utxo
 *set；与utxo相关的有效性检查在connectBlock（）中完成。
 *注意：ConnectBlock（）当前未调用此函数，因此我们
 *如果我们改变共识规则，应考虑升级问题。
 *在此功能中强制执行（例如通过添加新的共识规则）。见评论
 *在ConnectBlock（）中。
 *请注意-reindex chainstate跳过此处发生的验证！
 **/

static bool ContextualCheckBlockHeader(const CBlockHeader& block, CValidationState& state, const CChainParams& params, const CBlockIndex* pindexPrev, int64_t nAdjustedTime)
{
    assert(pindexPrev != nullptr);
    const int nHeight = pindexPrev->nHeight + 1;

//检查工作证明
    const Consensus::Params& consensusParams = params.GetConsensus();
    if (block.nBits != GetNextWorkRequired(pindexPrev, &block, consensusParams))
        return state.DoS(100, false, REJECT_INVALID, "bad-diffbits", false, "incorrect proof of work");

//检查检查点
    if (fCheckpointsEnabled) {
//在最后一个检查点之前，不要接受来自主链的任何分支。
//GetLastCheckpoint在我们的
//MAP受体阻滞剂
        CBlockIndex* pcheckpoint = Checkpoints::GetLastCheckpoint(params.Checkpoints());
        if (pcheckpoint && nHeight < pcheckpoint->nHeight)
            return state.DoS(100, error("%s: forked chain older than last checkpoint (height %d)", __func__, nHeight), REJECT_CHECKPOINT, "bad-fork-prior-to-checkpoint");
    }

//对照上一个检查时间戳
    if (block.GetBlockTime() <= pindexPrev->GetMedianTimePast())
        return state.Invalid(false, REJECT_INVALID, "time-too-old", "block's timestamp is too early");

//检查时间戳
    if (block.GetBlockTime() > nAdjustedTime + MAX_FUTURE_BLOCK_TIME)
        return state.Invalid(false, REJECT_INVALID, "time-too-new", "block timestamp too far in the future");

//当网络的95%（在testnet上为75%）升级后，拒绝过时的版本块：
//检查版本2、3和4升级
    if((block.nVersion < 2 && nHeight >= consensusParams.BIP34Height) ||
       (block.nVersion < 3 && nHeight >= consensusParams.BIP66Height) ||
       (block.nVersion < 4 && nHeight >= consensusParams.BIP65Height))
            return state.Invalid(false, REJECT_OBSOLETE, strprintf("bad-version(0x%08x)", block.nVersion),
                                 strprintf("rejected nVersion=0x%08x block", block.nVersion));

    return true;
}

/*注意：ConnectBlock（）当前未调用此函数，因此我们
 *如果我们改变共识规则，应考虑升级问题。
 *在此功能中强制执行（例如通过添加新的共识规则）。见评论
 *在ConnectBlock（）中。
 *请注意-reindex chainstate跳过此处发生的验证！
 **/

static bool ContextualCheckBlock(const CBlock& block, CValidationState& state, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    const int nHeight = pindexPrev == nullptr ? 0 : pindexPrev->nHeight + 1;

//开始使用versionBits逻辑强制执行bip113（中值时间过去）。
    int nLockTimeFlags = 0;
    if (VersionBitsState(pindexPrev, consensusParams, Consensus::DEPLOYMENT_CSV, versionbitscache) == ThresholdState::ACTIVE) {
        assert(pindexPrev != nullptr);
        nLockTimeFlags |= LOCKTIME_MEDIAN_TIME_PAST;
    }

    int64_t nLockTimeCutoff = (nLockTimeFlags & LOCKTIME_MEDIAN_TIME_PAST)
                              ? pindexPrev->GetMedianTimePast()
                              : block.GetBlockTime();

//检查所有交易是否已完成
    for (const auto& tx : block.vtx) {
        if (!IsFinalTx(*tx, nHeight, nLockTimeCutoff)) {
            return state.DoS(10, false, REJECT_INVALID, "bad-txns-nonfinal", false, "non-final transaction");
        }
    }

//强制使用coinbase以序列化块高度开头的规则
    if (nHeight >= consensusParams.BIP34Height)
    {
        CScript expect = CScript() << nHeight;
        if (block.vtx[0]->vin[0].scriptSig.size() < expect.size() ||
            !std::equal(expect.begin(), expect.end(), block.vtx[0]->vin[0].scriptSig.begin())) {
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-height", false, "block height mismatch in coinbase");
        }
    }

//见证承诺的确认。
//*我们计算块的所有事务的见证哈希（即包含见证的哈希），除了
//CoinBase（其中使用0x0000….0000）。
//*CoinBase脚本见证是一个32字节向量的堆栈，包含见证保留值（无约束）。
//*我们构建了一个merkle树，其中所有见证哈希都是叶子（类似于块头中的hashmerkleroot）。
//*必须至少有一个输出，其scriptpubkey是一个36字节的push，其中前4个字节是
//0XAA、0X21、0XA9、0XED，以下32个字节为sha256^2（见证根、见证保留值）。如果有
//多个，使用最后一个。
    bool fHaveWitness = false;
    if (VersionBitsState(pindexPrev, consensusParams, Consensus::DEPLOYMENT_SEGWIT, versionbitscache) == ThresholdState::ACTIVE) {
        int commitpos = GetWitnessCommitmentIndex(block);
        if (commitpos != -1) {
            bool malleated = false;
            uint256 hashWitness = BlockWitnessMerkleRoot(block, &malleated);
//延展性检查被忽略；作为事务树本身
//已经不允许了，不可能在
//见证树。
            if (block.vtx[0]->vin[0].scriptWitness.stack.size() != 1 || block.vtx[0]->vin[0].scriptWitness.stack[0].size() != 32) {
                return state.DoS(100, false, REJECT_INVALID, "bad-witness-nonce-size", true, strprintf("%s : invalid witness reserved value size", __func__));
            }
            CHash256().Write(hashWitness.begin(), 32).Write(&block.vtx[0]->vin[0].scriptWitness.stack[0][0], 32).Finalize(hashWitness.begin());
            if (memcmp(hashWitness.begin(), &block.vtx[0]->vout[commitpos].scriptPubKey[6], 32)) {
                return state.DoS(100, false, REJECT_INVALID, "bad-witness-merkle-match", true, strprintf("%s : witness merkle commitment mismatch", __func__));
            }
            fHaveWitness = true;
        }
    }

//不允许在不提交给见证数据的块中使用见证数据，否则将为垃圾邮件留出空间。
    if (!fHaveWitness) {
      for (const auto& tx : block.vtx) {
            if (tx->HasWitness()) {
                return state.DoS(100, false, REJECT_INVALID, "unexpected-witness", true, strprintf("%s : unexpected witness data found", __func__));
            }
        }
    }

//Coinbase见证保留值和承诺验证后，
//我们可以检查块重是否通过（在检查
//Coinbase见证，重量也有可能
//大的，通过填充Coinbase目击证人，这不会改变
//块哈希，因此我们无法将块永久标记为
//失败了）。
    if (GetBlockWeight(block) > MAX_BLOCK_WEIGHT) {
        return state.DoS(100, false, REJECT_INVALID, "bad-blk-weight", false, strprintf("%s : weight limit failed", __func__));
    }

    return true;
}

bool CChainState::AcceptBlockHeader(const CBlockHeader& block, CValidationState& state, const CChainParams& chainparams, CBlockIndex** ppindex)
{
    AssertLockHeld(cs_main);
//检查是否重复
    uint256 hash = block.GetHash();
    BlockMap::iterator miSelf = mapBlockIndex.find(hash);
    CBlockIndex *pindex = nullptr;
    if (hash != chainparams.GetConsensus().hashGenesisBlock) {
        if (miSelf != mapBlockIndex.end()) {
//块头已已知。
            pindex = miSelf->second;
            if (ppindex)
                *ppindex = pindex;
            if (pindex->nStatus & BLOCK_FAILED_MASK)
                return state.Invalid(error("%s: block %s is marked invalid", __func__, hash.ToString()), 0, "duplicate");
            return true;
        }

        if (!CheckBlockHeader(block, state, chainparams.GetConsensus()))
            return error("%s: Consensus::CheckBlockHeader: %s, %s", __func__, hash.ToString(), FormatStateMessage(state));

//获取上一个块索引
        CBlockIndex* pindexPrev = nullptr;
        BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
        if (mi == mapBlockIndex.end())
            return state.DoS(10, error("%s: prev block not found", __func__), 0, "prev-blk-not-found");
        pindexPrev = (*mi).second;
        if (pindexPrev->nStatus & BLOCK_FAILED_MASK)
            return state.DoS(100, error("%s: prev block invalid", __func__), REJECT_INVALID, "bad-prevblk");
        if (!ContextualCheckBlockHeader(block, state, chainparams, pindexPrev, GetAdjustedTime()))
            return error("%s: Consensus::ContextualCheckBlockHeader: %s, %s", __func__, hash.ToString(), FormatStateMessage(state));

        /*确定此块是否从已找到的任何块下降
         *无效（M_块失败），然后标记pindexprev和介于
         *他们失败了。例如：
         *
         *d3
         */
         ＊B2-C2
         */\
         *A D2-E2-F2
         **
         *b1-c1-d1-e1
         *
         *在我们试图从e1到f2重新组合的情况下，只发现
         *c2无效，我们将d2、e2和f2标记为block_failed_child
         *但不是d3（当时我们的任何候选集合中都没有）。
         *
         *在任何情况下，重新启动时，d3也将标记为block_failed_child。
         *在LoadBlockIndex中。
         **/

        if (!pindexPrev->IsValid(BLOCK_VALID_SCRIPTS)) {
//上面的意思不是“无效”：它检查前一个块
//尚未验证以阻止有效的脚本。这是一场表演
//优化，在向尖端添加新块的常见情况下，
//我们不需要遍历失败的块列表。
            for (const CBlockIndex* failedit : m_failed_blocks) {
                if (pindexPrev->GetAncestor(failedit->nHeight) == failedit) {
                    assert(failedit->nStatus & BLOCK_FAILED_VALID);
                    CBlockIndex* invalid_walk = pindexPrev;
                    while (invalid_walk != failedit) {
                        invalid_walk->nStatus |= BLOCK_FAILED_CHILD;
                        setDirtyBlockIndex.insert(invalid_walk);
                        invalid_walk = invalid_walk->pprev;
                    }
                    return state.DoS(100, error("%s: prev block invalid", __func__), REJECT_INVALID, "bad-prevblk");
                }
            }
        }
    }
    if (pindex == nullptr)
        pindex = AddToBlockIndex(block);

    if (ppindex)
        *ppindex = pindex;

    CheckBlockIndex(chainparams.GetConsensus());

    return true;
}

//AcceptBlockHeader的公开包装
bool ProcessNewBlockHeaders(const std::vector<CBlockHeader>& headers, CValidationState& state, const CChainParams& chainparams, const CBlockIndex** ppindex, CBlockHeader *first_invalid)
{
    if (first_invalid != nullptr) first_invalid->SetNull();
    {
        LOCK(cs_main);
        for (const CBlockHeader& header : headers) {
CBlockIndex *pindex = nullptr; //使用temp pindex而不是ppindex来避免常量转换
            if (!g_chainstate.AcceptBlockHeader(header, state, chainparams, &pindex)) {
                if (first_invalid) *first_invalid = header;
                return false;
            }
            if (ppindex) {
                *ppindex = pindex;
            }
        }
    }
    NotifyHeaderTip();
    return true;
}

/*将块存储在磁盘上。如果dbp为非nullptr，则已知该文件已驻留在磁盘上。*/
static CDiskBlockPos SaveBlockToDisk(const CBlock& block, int nHeight, const CChainParams& chainparams, const CDiskBlockPos* dbp) {
    unsigned int nBlockSize = ::GetSerializeSize(block, CLIENT_VERSION);
    CDiskBlockPos blockPos;
    if (dbp != nullptr)
        blockPos = *dbp;
    if (!FindBlockPos(blockPos, nBlockSize+8, nHeight, block.GetBlockTime(), dbp != nullptr)) {
        error("%s: FindBlockPos failed", __func__);
        return CDiskBlockPos();
    }
    if (dbp == nullptr) {
        if (!WriteBlockToDisk(block, blockPos, chainparams.MessageStart())) {
            AbortNode("Failed to write block");
            return CDiskBlockPos();
        }
    }
    return blockPos;
}

/*将块存储在磁盘上。如果dbp为非nullptr，则已知该文件已驻留在磁盘上。*/
bool CChainState::AcceptBlock(const std::shared_ptr<const CBlock>& pblock, CValidationState& state, const CChainParams& chainparams, CBlockIndex** ppindex, bool fRequested, const CDiskBlockPos* dbp, bool* fNewBlock)
{
    const CBlock& block = *pblock;

    if (fNewBlock) *fNewBlock = false;
    AssertLockHeld(cs_main);

    CBlockIndex *pindexDummy = nullptr;
    CBlockIndex *&pindex = ppindex ? *ppindex : pindexDummy;

    if (!AcceptBlockHeader(block, state, chainparams, &pindex))
        return false;

//尝试处理我们没有的所有请求块，但仅限于
//处理一个未请求的块，如果它是新的并且有足够的工作
//提前付小费，前面不算太多街区。
    bool fAlreadyHave = pindex->nStatus & BLOCK_HAVE_DATA;
    bool fHasMoreOrSameWork = (chainActive.Tip() ? pindex->nChainWork >= chainActive.Tip()->nChainWork : true);
//太无序的块不必要地限制了
//修剪，因为修剪不会删除包含
//高度太接近尖端的块。应用此测试
//不管是否启用了修剪；通常应该可以安全地
//不处理未请求的块。
    bool fTooFarAhead = (pindex->nHeight > int(chainActive.Height() + MIN_BLOCKS_TO_KEEP));

//TODO:通过删除frequeuested将此函数与块下载逻辑分离
//这需要一些新的链数据结构来有效地查找
//区块是在一个链中，导致最佳提示的候选人，尽管不是
//本身就是这样一个候选人。

//TODO:更好地处理重复的返回值和错误条件
//和未请求的块。
    if (fAlreadyHave) return true;
if (!fRequested) {  //如果我们没有要求：
if (pindex->nTx != 0) return true;    //这是以前处理过的块，已修剪
if (!fHasMoreOrSameWork) return true; //不要处理较少的工作链
if (fTooFarAhead) return true;        //块高度太高

//防止低工作链的DoS攻击。
//如果我们的小费落在后面，同伴可以试着把我们送到
//在假链子上放低工作区，我们永远不会
//请求；不要处理这些。
        if (pindex->nChainWork < nMinimumChainWork) return true;
    }

    if (!CheckBlock(block, state, chainparams.GetConsensus()) ||
        !ContextualCheckBlock(block, state, chainparams.GetConsensus(), pindex->pprev)) {
        if (state.IsInvalid() && !state.CorruptionPossible()) {
            pindex->nStatus |= BLOCK_FAILED_VALID;
            setDirtyBlockIndex.insert(pindex);
        }
        return error("%s: %s", __func__, FormatStateMessage(state));
    }

//标题有效/有工作，Merkle树和Segwit Merkle树良好…立即中继
//（但是如果它没有建立在我们最好的技巧之上，那么让sendmessages循环将其中继）
    if (!IsInitialBlockDownload() && chainActive.Tip() == pindex->pprev)
        GetMainSignals().NewPoWValidBlock(pindex, pblock);

//将块写入历史文件
    if (fNewBlock) *fNewBlock = true;
    try {
        CDiskBlockPos blockPos = SaveBlockToDisk(block, pindex->nHeight, chainparams, dbp);
        if (blockPos.IsNull()) {
            state.Error(strprintf("%s: Failed to find position to write new block to disk", __func__));
            return false;
        }
        ReceivedBlockTransactions(block, pindex, blockPos, chainparams.GetConsensus());
    } catch (const std::runtime_error& e) {
        return AbortNode(state, std::string("System error: ") + e.what());
    }

    FlushStateToDisk(chainparams, state, FlushStateMode::NONE);

    CheckBlockIndex(chainparams.GetConsensus());

    return true;
}

bool ProcessNewBlock(const CChainParams& chainparams, const std::shared_ptr<const CBlock> pblock, bool fForceProcessing, bool *fNewBlock)
{
    AssertLockNotHeld(cs_main);

    {
        CBlockIndex *pindex = nullptr;
        if (fNewBlock) *fNewBlock = false;
        CValidationState state;

//checkblock（）不支持多线程块验证，因为cblock:：fchecked可能导致数据争用。
//因此，下面的关键部分也必须包含checkblock（）调用。
        LOCK(cs_main);

//确保checkblock（）在调用acceptblock之前通过，如
//皮带和吊带。
        bool ret = CheckBlock(*pblock, state, chainparams.GetConsensus());
        if (ret) {
//存储到磁盘
            ret = g_chainstate.AcceptBlock(pblock, state, chainparams, &pindex, fForceProcessing, nullptr, fNewBlock);
        }
        if (!ret) {
            GetMainSignals().BlockChecked(*pblock, state);
            return error("%s: AcceptBlock FAILED (%s)", __func__, FormatStateMessage(state));
        }
    }

    NotifyHeaderTip();

CValidationState state; //只用于报告错误，而不是无效性-忽略它
    if (!g_chainstate.ActivateBestChain(state, chainparams, pblock))
        return error("%s: ActivateBestChain failed (%s)", __func__, FormatStateMessage(state));

    return true;
}

bool TestBlockValidity(CValidationState& state, const CChainParams& chainparams, const CBlock& block, CBlockIndex* pindexPrev, bool fCheckPOW, bool fCheckMerkleRoot)
{
    AssertLockHeld(cs_main);
    assert(pindexPrev && pindexPrev == chainActive.Tip());
    CCoinsViewCache viewNew(pcoinsTip.get());
    uint256 block_hash(block.GetHash());
    CBlockIndex indexDummy(block);
    indexDummy.pprev = pindexPrev;
    indexDummy.nHeight = pindexPrev->nHeight + 1;
    indexDummy.phashBlock = &block_hash;

//注意：checkblockheader由checkblock调用
    if (!ContextualCheckBlockHeader(block, state, chainparams, pindexPrev, GetAdjustedTime()))
        return error("%s: Consensus::ContextualCheckBlockHeader: %s", __func__, FormatStateMessage(state));
    if (!CheckBlock(block, state, chainparams.GetConsensus(), fCheckPOW, fCheckMerkleRoot))
        return error("%s: Consensus::CheckBlock: %s", __func__, FormatStateMessage(state));
    if (!ContextualCheckBlock(block, state, chainparams.GetConsensus(), pindexPrev))
        return error("%s: Consensus::ContextualCheckBlock: %s", __func__, FormatStateMessage(state));
    if (!g_chainstate.ConnectBlock(block, state, &indexDummy, viewNew, chainparams, true))
        return false;
    assert(state.IsValid());

    return true;
}

/*
 *块修剪代码
 **/


/*计算块和撤消文件当前使用的磁盘空间量*/
uint64_t CalculateCurrentUsage()
{
    LOCK(cs_LastBlockFile);

    uint64_t retval = 0;
    for (const CBlockFileInfo &file : vinfoBlockFile) {
        retval += file.nSize + file.nUndoSize;
    }
    return retval;
}

/*修剪块文件（修改相关数据库条目*/
void PruneOneBlockFile(const int fileNumber)
{
    LOCK(cs_LastBlockFile);

    for (const auto& entry : mapBlockIndex) {
        CBlockIndex* pindex = entry.second;
        if (pindex->nFile == fileNumber) {
            pindex->nStatus &= ~BLOCK_HAVE_DATA;
            pindex->nStatus &= ~BLOCK_HAVE_UNDO;
            pindex->nFile = 0;
            pindex->nDataPos = 0;
            pindex->nUndoPos = 0;
            setDirtyBlockIndex.insert(pindex);

//从mapblocksunlinked删除——我们删除的任何块
//再次下载，以考虑其链，其中
//它将被视为
//mapblocksunlinked或setblockindexcandients。
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = mapBlocksUnlinked.equal_range(pindex->pprev);
            while (range.first != range.second) {
                std::multimap<CBlockIndex *, CBlockIndex *>::iterator _it = range.first;
                range.first++;
                if (_it->second == pindex) {
                    mapBlocksUnlinked.erase(_it);
                }
            }
        }
    }

    vinfoBlockFile[fileNumber].SetNull();
    setDirtyFileInfo.insert(fileNumber);
}


void UnlinkPrunedFiles(const std::set<int>& setFilesToPrune)
{
    for (std::set<int>::iterator it = setFilesToPrune.begin(); it != setFilesToPrune.end(); ++it) {
        CDiskBlockPos pos(*it, 0);
        fs::remove(GetBlockPosFilename(pos, "blk"));
        fs::remove(GetBlockPosFilename(pos, "rev"));
        LogPrintf("Prune: %s deleted blk/rev (%05u)\n", __func__, *it);
    }
}

/*根据用户使用rpc命令pruneblockchain指定的高度，计算要删除的块/rev文件*/
static void FindFilesToPruneManual(std::set<int>& setFilesToPrune, int nManualPruneHeight)
{
    assert(fPruneMode && nManualPruneHeight > 0);

    LOCK2(cs_main, cs_LastBlockFile);
    if (chainActive.Tip() == nullptr)
        return;

//要修剪的最后一个块是（用户指定的高度，最小块与尖端保持距离）中的较小值。
    unsigned int nLastBlockWeCanPrune = std::min((unsigned)nManualPruneHeight, chainActive.Tip()->nHeight - MIN_BLOCKS_TO_KEEP);
    int count=0;
    for (int fileNumber = 0; fileNumber < nLastBlockFile; fileNumber++) {
        if (vinfoBlockFile[fileNumber].nSize == 0 || vinfoBlockFile[fileNumber].nHeightLast > nLastBlockWeCanPrune)
            continue;
        PruneOneBlockFile(fileNumber);
        setFilesToPrune.insert(fileNumber);
        count++;
    }
    LogPrintf("Prune (Manual): prune_height=%d removed %d blk/rev pairs\n", nLastBlockWeCanPrune, count);
}

/*从pruneblockchain的rpc代码调用此函数*/
void PruneBlockFilesManual(int nManualPruneHeight)
{
    CValidationState state;
    const CChainParams& chainparams = Params();
    if (!FlushStateToDisk(chainparams, state, FlushStateMode::NONE, nManualPruneHeight)) {
        LogPrintf("%s: failed to flush state (%s)\n", __func__, FormatStateMessage(state));
    }
}

/*
 *修剪块并撤消文件（黑色？？？？DAT和撤销？？？？.dat）以便使用的磁盘空间小于用户定义的目标。
 *用户在命令行或配置文件中设置目标（MB）。这将在启动时运行，无论何时
 *在块或撤消文件中分配空间，保持在目标下方。改回未运行需要重新索引
 （在这种情况下，意味着必须重新下载区块链。）
 *
 *在设置全局fcheckforpruning标志后，从flushstatetodisk调用修剪函数。
 *在锁定步骤中删除block和undo文件（删除blk00003.dat时，rev00003.dat也会删除）。
 *在最长链至少达到一定长度之前，不能进行修剪（Mainnet为100000，TestNet为1000，RegTest为1000）。
 *修剪永远不会删除与活动链尖端之间的定义距离（当前为288）内的块。
 *通过取消设置have_data来更新块索引，并撤消存储在已删除文件中的任何块。
 *数据库标志记录至少删除了一些块文件的事实。
 *
 *@param[out]setfilestoprune将返回一组可以取消链接的文件索引。
 **/

static void FindFilesToPrune(std::set<int>& setFilesToPrune, uint64_t nPruneAfterHeight)
{
    LOCK2(cs_main, cs_LastBlockFile);
    if (chainActive.Tip() == nullptr || nPruneTarget == 0) {
        return;
    }
    if ((uint64_t)chainActive.Tip()->nHeight <= nPruneAfterHeight) {
        return;
    }

    unsigned int nLastBlockWeCanPrune = chainActive.Tip()->nHeight - MIN_BLOCKS_TO_KEEP;
    uint64_t nCurrentUsage = CalculateCurrentUsage();
//在为文件分配新空间之前，我们不检查是否修剪
//所以我们应该在我们的目标下留一个缓冲区来考虑另一个分配
//在下次修剪之前。
    uint64_t nBuffer = BLOCKFILE_CHUNK_SIZE + UNDOFILE_CHUNK_SIZE;
    uint64_t nBytesToPrune;
    int count=0;

    if (nCurrentUsage + nBuffer >= nPruneTarget) {
//在prune事件中，chainState数据库被刷新。
//避免过多的删减事件否定了高dbcache的好处
//价值观，我们不应该修剪得太快。
//因此，在IBD中进行修剪时，请稍微增加一点缓冲区，以避免过早地重新修剪。
        if (IsInitialBlockDownload()) {
//因为这只在IBD期间相关，所以我们使用固定的10%
            nBuffer += nPruneTarget / 10;
        }

        for (int fileNumber = 0; fileNumber < nLastBlockFile; fileNumber++) {
            nBytesToPrune = vinfoBlockFile[fileNumber].nSize + vinfoBlockFile[fileNumber].nUndoSize;

            if (vinfoBlockFile[fileNumber].nSize == 0)
                continue;

if (nCurrentUsage + nBuffer < nPruneTarget)  //我们低于目标了吗？
                break;

//不要删减可能在主链顶端保留的分块内有块的文件，而是继续扫描
            if (vinfoBlockFile[fileNumber].nHeightLast > nLastBlockWeCanPrune)
                continue;

            PruneOneBlockFile(fileNumber);
//将要删除的文件排队
            setFilesToPrune.insert(fileNumber);
            nCurrentUsage -= nBytesToPrune;
            count++;
        }
    }

    LogPrint(BCLog::PRUNE, "Prune: target=%dMiB actual=%dMiB diff=%dMiB max_prune_height=%d removed %d blk/rev pairs\n",
           nPruneTarget/1024/1024, nCurrentUsage/1024/1024,
           ((int64_t)nPruneTarget - (int64_t)nCurrentUsage)/1024/1024,
           nLastBlockWeCanPrune, count);
}

bool CheckDiskSpace(uint64_t nAdditionalBytes, bool blocks_dir)
{
    uint64_t nFreeBytesAvailable = fs::space(blocks_dir ? GetBlocksDir() : GetDataDir()).available;

//检查nmindiskspace字节（当前为50MB）
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes)
        return AbortNode("Disk space is low!", _("Error: Disk space is low!"));

    return true;
}

static FILE* OpenDiskFile(const CDiskBlockPos &pos, const char *prefix, bool fReadOnly)
{
    if (pos.IsNull())
        return nullptr;
    fs::path path = GetBlockPosFilename(pos, prefix);
    fs::create_directories(path.parent_path());
    FILE* file = fsbridge::fopen(path, fReadOnly ? "rb": "rb+");
    if (!file && !fReadOnly)
        file = fsbridge::fopen(path, "wb+");
    if (!file) {
        LogPrintf("Unable to open file %s\n", path.string());
        return nullptr;
    }
    if (pos.nPos) {
        if (fseek(file, pos.nPos, SEEK_SET)) {
            LogPrintf("Unable to seek to position %u of %s\n", pos.nPos, path.string());
            fclose(file);
            return nullptr;
        }
    }
    return file;
}

FILE* OpenBlockFile(const CDiskBlockPos &pos, bool fReadOnly) {
    return OpenDiskFile(pos, "blk", fReadOnly);
}

/*打开撤消文件（rev？？？？？？？DAT）*/
static FILE* OpenUndoFile(const CDiskBlockPos &pos, bool fReadOnly) {
    return OpenDiskFile(pos, "rev", fReadOnly);
}

fs::path GetBlockPosFilename(const CDiskBlockPos &pos, const char *prefix)
{
    return GetBlocksDir() / strprintf("%s%05u.dat", prefix, pos.nFile);
}

CBlockIndex * CChainState::InsertBlockIndex(const uint256& hash)
{
    AssertLockHeld(cs_main);

    if (hash.IsNull())
        return nullptr;

//返回现有
    BlockMap::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

//创造新
    CBlockIndex* pindexNew = new CBlockIndex();
    mi = mapBlockIndex.insert(std::make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

bool CChainState::LoadBlockIndex(const Consensus::Params& consensus_params, CBlockTreeDB& blocktree)
{
    if (!blocktree.LoadBlockIndexGuts(consensus_params, [this](const uint256& hash) EXCLUSIVE_LOCKS_REQUIRED(cs_main) { return this->InsertBlockIndex(hash); }))
        return false;

//计算nchainwork
    std::vector<std::pair<int, CBlockIndex*> > vSortedByHeight;
    vSortedByHeight.reserve(mapBlockIndex.size());
    for (const std::pair<const uint256, CBlockIndex*>& item : mapBlockIndex)
    {
        CBlockIndex* pindex = item.second;
        vSortedByHeight.push_back(std::make_pair(pindex->nHeight, pindex));
    }
    sort(vSortedByHeight.begin(), vSortedByHeight.end());
    for (const std::pair<int, CBlockIndex*>& item : vSortedByHeight)
    {
        CBlockIndex* pindex = item.second;
        pindex->nChainWork = (pindex->pprev ? pindex->pprev->nChainWork : 0) + GetBlockProof(*pindex);
        pindex->nTimeMax = (pindex->pprev ? std::max(pindex->pprev->nTimeMax, pindex->nTime) : pindex->nTime);
//我们可以链接我们在某个时刻收到交易的区块链。
//修剪的节点可能删除了块。
        if (pindex->nTx > 0) {
            if (pindex->pprev) {
                if (pindex->pprev->HaveTxsDownloaded()) {
                    pindex->nChainTx = pindex->pprev->nChainTx + pindex->nTx;
                } else {
                    pindex->nChainTx = 0;
                    mapBlocksUnlinked.insert(std::make_pair(pindex->pprev, pindex));
                }
            } else {
                pindex->nChainTx = pindex->nTx;
            }
        }
        if (!(pindex->nStatus & BLOCK_FAILED_MASK) && pindex->pprev && (pindex->pprev->nStatus & BLOCK_FAILED_MASK)) {
            pindex->nStatus |= BLOCK_FAILED_CHILD;
            setDirtyBlockIndex.insert(pindex);
        }
        if (pindex->IsValid(BLOCK_VALID_TRANSACTIONS) && (pindex->HaveTxsDownloaded() || pindex->pprev == nullptr))
            setBlockIndexCandidates.insert(pindex);
        if (pindex->nStatus & BLOCK_FAILED_MASK && (!pindexBestInvalid || pindex->nChainWork > pindexBestInvalid->nChainWork))
            pindexBestInvalid = pindex;
        if (pindex->pprev)
            pindex->BuildSkip();
        if (pindex->IsValid(BLOCK_VALID_TREE) && (pindexBestHeader == nullptr || CBlockIndexWorkComparator()(pindexBestHeader, pindex)))
            pindexBestHeader = pindex;
    }

    return true;
}

bool static LoadBlockIndexDB(const CChainParams& chainparams) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (!g_chainstate.LoadBlockIndex(chainparams.GetConsensus(), *pblocktree))
        return false;

//加载块文件信息
    pblocktree->ReadLastBlockFile(nLastBlockFile);
    vinfoBlockFile.resize(nLastBlockFile + 1);
    LogPrintf("%s: last block file = %i\n", __func__, nLastBlockFile);
    for (int nFile = 0; nFile <= nLastBlockFile; nFile++) {
        pblocktree->ReadBlockFileInfo(nFile, vinfoBlockFile[nFile]);
    }
    LogPrintf("%s: last block file info: %s\n", __func__, vinfoBlockFile[nLastBlockFile].ToString());
    for (int nFile = nLastBlockFile + 1; true; nFile++) {
        CBlockFileInfo info;
        if (pblocktree->ReadBlockFileInfo(nFile, info)) {
            vinfoBlockFile.push_back(info);
        } else {
            break;
        }
    }

//检查是否存在BLK文件
    LogPrintf("Checking all blk files are present...\n");
    std::set<int> setBlkDataFiles;
    for (const std::pair<const uint256, CBlockIndex*>& item : mapBlockIndex)
    {
        CBlockIndex* pindex = item.second;
        if (pindex->nStatus & BLOCK_HAVE_DATA) {
            setBlkDataFiles.insert(pindex->nFile);
        }
    }
    for (std::set<int>::iterator it = setBlkDataFiles.begin(); it != setBlkDataFiles.end(); it++)
    {
        CDiskBlockPos pos(*it, 0);
        if (CAutoFile(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION).IsNull()) {
            return false;
        }
    }

//检查是否修剪过块和撤消文件
    pblocktree->ReadFlag("prunedblockfiles", fHavePruned);
    if (fHavePruned)
        LogPrintf("LoadBlockIndexDB(): Block files have previously been pruned\n");

//检查是否需要继续重新索引
    bool fReindexing = false;
    pblocktree->ReadReindexing(fReindexing);
    if(fReindexing) fReindex = true;

    return true;
}

bool LoadChainTip(const CChainParams& chainparams)
{
    AssertLockHeld(cs_main);

    if (chainActive.Tip() && chainActive.Tip()->GetBlockHash() == pcoinsTip->GetBestBlock()) return true;

    if (pcoinsTip->GetBestBlock().IsNull() && mapBlockIndex.size() == 1) {
//如果我们刚刚添加了Genesis块，现在连接它，所以
//我们返回时总是有一个chainActive.tip（）。
        LogPrintf("%s: Connecting genesis block...\n", __func__);
        CValidationState state;
        if (!ActivateBestChain(state, chainparams)) {
            LogPrintf("%s: failed to activate chain (%s)\n", __func__, FormatStateMessage(state));
            return false;
        }
    }

//将指针加载到最佳链的末尾
    CBlockIndex* pindex = LookupBlockIndex(pcoinsTip->GetBestBlock());
    if (!pindex) {
        return false;
    }
    chainActive.SetTip(pindex);

    g_chainstate.PruneBlockIndexCandidates();

    LogPrintf("Loaded best chain: hashBestChain=%s height=%d date=%s progress=%f\n",
        chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(),
        FormatISO8601DateTime(chainActive.Tip()->GetBlockTime()),
        GuessVerificationProgress(chainparams.TxData(), chainActive.Tip()));
    return true;
}

CVerifyDB::CVerifyDB()
{
    uiInterface.ShowProgress(_("Verifying blocks..."), 0, false);
}

CVerifyDB::~CVerifyDB()
{
    uiInterface.ShowProgress("", 100, false);
}

bool CVerifyDB::VerifyDB(const CChainParams& chainparams, CCoinsView *coinsview, int nCheckLevel, int nCheckDepth)
{
    LOCK(cs_main);
    if (chainActive.Tip() == nullptr || chainActive.Tip()->pprev == nullptr)
        return true;

//验证最佳链中的块
    if (nCheckDepth <= 0 || nCheckDepth > chainActive.Height())
        nCheckDepth = chainActive.Height();
    nCheckLevel = std::max(0, std::min(4, nCheckLevel));
    LogPrintf("Verifying last %i blocks at level %i\n", nCheckDepth, nCheckLevel);
    CCoinsViewCache coins(coinsview);
    CBlockIndex* pindex;
    CBlockIndex* pindexFailure = nullptr;
    int nGoodTransactions = 0;
    CValidationState state;
    int reportDone = 0;
    /*printf（“[0%%]…”）；/*续*/
    对于（pindex=chainactive.tip（）；pindex&&pindex->pprev；pindex=pindex->pprev）
        boost:：this_thread:：interrupt_point（）；
        const int percentagedone=std:：max（1，std:：min（99，（int）（（double）（chainactive.height（）-pindex->nheight））/（double）ncheckdepth*（nchecklevel>=4？50）（100）））；
        如果（reportdone<percentagedone/10）
            //每10%步骤报告一次
            logprintf（“[%d%%]…”，PercentageDone）；/*继续*/

            reportDone = percentageDone/10;
        }
        uiInterface.ShowProgress(_("Verifying blocks..."), percentageDone, false);
        if (pindex->nHeight <= chainActive.Height()-nCheckDepth)
            break;
        if (fPruneMode && !(pindex->nStatus & BLOCK_HAVE_DATA)) {
//如果修剪，只返回到我们有数据的地方。
            LogPrintf("VerifyDB(): block verification stopping at height %d (pruning, no data)\n", pindex->nHeight);
            break;
        }
        CBlock block;
//检查级别0:从磁盘读取
        if (!ReadBlockFromDisk(block, pindex, chainparams.GetConsensus()))
            return error("VerifyDB(): *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
//检查级别1：验证块有效性
        if (nCheckLevel >= 1 && !CheckBlock(block, state, chainparams.GetConsensus()))
            return error("%s: *** found bad block at %d, hash=%s (%s)\n", __func__,
                         pindex->nHeight, pindex->GetBlockHash().ToString(), FormatStateMessage(state));
//检查级别2:验证撤消有效性
        if (nCheckLevel >= 2 && pindex) {
            CBlockUndo undo;
            if (!pindex->GetUndoPos().IsNull()) {
                if (!UndoReadFromDisk(undo, pindex)) {
                    return error("VerifyDB(): *** found bad undo data at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
                }
            }
        }
//检查级别3：仅在断开提示块的内存时检查不一致性
        if (nCheckLevel >= 3 && (coins.DynamicMemoryUsage() + pcoinsTip->DynamicMemoryUsage()) <= nCoinCacheUsage) {
            assert(coins.GetBestBlock() == pindex->GetBlockHash());
            DisconnectResult res = g_chainstate.DisconnectBlock(block, pindex, coins);
            if (res == DISCONNECT_FAILED) {
                return error("VerifyDB(): *** irrecoverable inconsistency in block data at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            }
            if (res == DISCONNECT_UNCLEAN) {
                nGoodTransactions = 0;
                pindexFailure = pindex;
            } else {
                nGoodTransactions += block.vtx.size();
            }
        }
        if (ShutdownRequested())
            return true;
    }
    if (pindexFailure)
        return error("VerifyDB(): *** coin database inconsistencies found (last %i blocks, %i good transactions before that)\n", chainActive.Height() - pindexFailure->nHeight + 1, nGoodTransactions);

//在检查级别大于等于4的情况下移动pindex时存储块计数
    int block_count = chainActive.Height() - pindex->nHeight;

//检查4级：尝试重新连接块
    if (nCheckLevel >= 4) {
        while (pindex != chainActive.Tip()) {
            boost::this_thread::interruption_point();
            const int percentageDone = std::max(1, std::min(99, 100 - (int)(((double)(chainActive.Height() - pindex->nHeight)) / (double)nCheckDepth * 50)));
            if (reportDone < percentageDone/10) {
//每10%步骤报告一次
                /*printf（“[%d%%]…”，percentagedone）；/*继续*/
                reportDone=完成百分比/10；
            }
            uiinterface.showprogress（（“验证块…”），percentageDone，false）；
            pindex=chainactive.next（pindex）；
            块块；
            如果（！）readblockfromdisk（block，pindex，chainparams.getconsensus（））
                返回错误（“verifydb（）：***readblockfromdisk在%d失败，哈希=%s”，pindex->nheight，pindex->getblockhash（）.toString（））；
            如果（！）连接块（块、状态、pindex、硬币、链参数）
                返回错误（“verifydb（）：***在%d处发现不可连接的块，哈希=%s（%s）”，pindex->nheight，pindex->getblockhash（）.toString（），formatStateMessage（state））；
        }
    }

    logprintf（“[完成]）.\n”）；
    logprintf（“在最后的%i块中没有硬币数据库不一致（%i个事务），block_count，ngoodTransactions）；

    回归真实；
}

/**在utxo缓存上应用块的效果，忽略它可能已经应用。*/

bool CChainState::RollforwardBlock(const CBlockIndex* pindex, CCoinsViewCache& inputs, const CChainParams& params)
{
//TODO:与ConnectBlock合并
    CBlock block;
    if (!ReadBlockFromDisk(block, pindex, params.GetConsensus())) {
        return error("ReplayBlock(): ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
    }

    for (const CTransactionRef& tx : block.vtx) {
        if (!tx->IsCoinBase()) {
            for (const CTxIn &txin : tx->vin) {
                inputs.SpendCoin(txin.prevout);
            }
        }
//pass check=true，因为每添加一个都可能被覆盖。
        AddCoins(inputs, *tx, pindex->nHeight, true);
    }
    return true;
}

bool CChainState::ReplayBlocks(const CChainParams& params, CCoinsView* view)
{
    LOCK(cs_main);

    CCoinsViewCache cache(view);

    std::vector<uint256> hashHeads = view->GetHeadBlocks();
if (hashHeads.empty()) return true; //我们已经处于一致的状态。
    if (hashHeads.size() != 2) return error("ReplayBlocks(): unknown inconsistent state");

    uiInterface.ShowProgress(_("Replaying blocks..."), 0, false);
    LogPrintf("Replaying blocks\n");

const CBlockIndex* pindexOld = nullptr;  //中断冲洗期间的旧尖端。
const CBlockIndex* pindexNew;            //中断冲洗期间的新尖端。
const CBlockIndex* pindexFork = nullptr; //新旧提示共同使用的最新块。

    if (mapBlockIndex.count(hashHeads[0]) == 0) {
        return error("ReplayBlocks(): reorganization to unknown block requested");
    }
    pindexNew = mapBlockIndex[hashHeads[0]];

if (!hashHeads[1].IsNull()) { //旧提示允许为0，表示它是第一次刷新。
        if (mapBlockIndex.count(hashHeads[1]) == 0) {
            return error("ReplayBlocks(): reorganization from unknown block requested");
        }
        pindexOld = mapBlockIndex[hashHeads[1]];
        pindexFork = LastCommonAncestor(pindexOld, pindexNew);
        assert(pindexFork != nullptr);
    }

//沿着旧的分支回滚。
    while (pindexOld != pindexFork) {
if (pindexOld->nHeight > 0) { //切勿断开Genesis模块。
            CBlock block;
            if (!ReadBlockFromDisk(block, pindexOld, params.GetConsensus())) {
                return error("RollbackBlock(): ReadBlockFromDisk() failed at %d, hash=%s", pindexOld->nHeight, pindexOld->GetBlockHash().ToString());
            }
            LogPrintf("Rolling back %s (%i)\n", pindexOld->GetBlockHash().ToString(), pindexOld->nHeight);
            DisconnectResult res = DisconnectBlock(block, pindexOld, cache);
            if (res == DISCONNECT_FAILED) {
                return error("RollbackBlock(): DisconnectBlock failed at %d, hash=%s", pindexOld->nHeight, pindexOld->GetBlockHash().ToString());
            }
//如果返回disconnect-unclean，则表示删除了一个不存在的utxo，或者删除了一个现有的utxo。
//改写。它对应的情况是，要断开的块从未进行过所有操作
//应用于utxo集。但是，由于写一个utxo和删除一个utxo都是等幂运算，
//结果仍然是utxo集的一个版本，该块的效果将被撤消。
        }
        pindexOld = pindexOld->pprev;
    }

//从分叉点向前滚动到新尖端。
    int nForkHeight = pindexFork ? pindexFork->nHeight : 0;
    for (int nHeight = nForkHeight + 1; nHeight <= pindexNew->nHeight; ++nHeight) {
        const CBlockIndex* pindex = pindexNew->GetAncestor(nHeight);
        LogPrintf("Rolling forward %s (%i)\n", pindex->GetBlockHash().ToString(), nHeight);
        uiInterface.ShowProgress(_("Replaying blocks..."), (int) ((nHeight - nForkHeight) * 100.0 / (pindexNew->nHeight - nForkHeight)) , false);
        if (!RollforwardBlock(pindex, cache, params)) return false;
    }

    cache.SetBestBlock(pindexNew->GetBlockHash());
    cache.Flush();
    uiInterface.ShowProgress("", 100, false);
    return true;
}

bool ReplayBlocks(const CChainParams& params, CCoinsView* view) {
    return g_chainstate.ReplayBlocks(params, view);
}

bool CChainState::RewindBlockIndex(const CChainParams& params)
{
    LOCK(cs_main);

//请注意，在-reindex chainstate期间，使用空chainactive调用我们！

    int nHeight = 1;
    while (nHeight <= chainActive.Height()) {
//尽管现在普遍强制执行脚本“验证”证人
//ConnectBlock中的块，我们不需要返回
//从segwit实际激活前重新下载/重新验证块。
        if (IsWitnessEnabled(chainActive[nHeight - 1], params.GetConsensus()) && !(chainActive[nHeight]->nStatus & BLOCK_OPT_WITNESS)) {
            break;
        }
        nHeight++;
    }

//nheight现在是第一个未经充分验证的块的高度，或tipheight+1
    CValidationState state;
    CBlockIndex* pindex = chainActive.Tip();
    while (chainActive.Height() >= nHeight) {
        if (fPruneMode && !(chainActive.Tip()->nStatus & BLOCK_HAVE_DATA)) {
//如果修剪，不要尝试倒带超过have_数据点；
//因为旧的街区无论如何都不能提供，所以
//不需要走得更远，尝试断开tip（）。
//将失败（并需要不必要的重新索引/重新下载
//区块链）。
            break;
        }
        if (!DisconnectTip(state, params, nullptr)) {
            return error("RewindBlockIndex: unable to disconnect block at height %i (%s)", pindex->nHeight, FormatStateMessage(state));
        }
//偶尔将状态刷新到磁盘。
        if (!FlushStateToDisk(params, state, FlushStateMode::PERIODIC)) {
            LogPrintf("RewindBlockIndex: unable to flush state to disk (%s)\n", FormatStateMessage(state));
            return false;
        }
    }

//减少有效性标志并具有数据标志。
//我们在实际断开连接后执行此操作，否则将导致写入数据不足
//在写入链状态之前到磁盘，如果中断，将导致继续失败。
    for (const auto& entry : mapBlockIndex) {
        CBlockIndex* pindexIter = entry.second;

//注意：如果我们遇到一个未充分验证的块，
//在chainactive上，它必须是因为我们是一个修剪节点，并且
//此块或某些后续块没有数据，因此我们无法
//一路倒带。此时链上剩余的块处于活动状态
//不得降低其有效性。
        if (IsWitnessEnabled(pindexIter->pprev, params.GetConsensus()) && !(pindexIter->nStatus & BLOCK_OPT_WITNESS) && !chainActive.Contains(pindexIter)) {
//降低效度
            pindexIter->nStatus = std::min<unsigned int>(pindexIter->nStatus & BLOCK_VALID_MASK, BLOCK_VALID_TREE) | (pindexIter->nStatus & ~BLOCK_VALID_MASK);
//移除具有数据标志。
            pindexIter->nStatus &= ~(BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO);
//移除存储位置。
            pindexIter->nFile = 0;
            pindexIter->nDataPos = 0;
            pindexIter->nUndoPos = 0;
//去除其他各种东西
            pindexIter->nTx = 0;
            pindexIter->nChainTx = 0;
            pindexIter->nSequenceId = 0;
//一定要把它写下来。
            setDirtyBlockIndex.insert(pindexIter);
//更新索引
            setBlockIndexCandidates.erase(pindexIter);
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> ret = mapBlocksUnlinked.equal_range(pindexIter->pprev);
            while (ret.first != ret.second) {
                if (ret.first->second == pindexIter) {
                    mapBlocksUnlinked.erase(ret.first++);
                } else {
                    ++ret.first;
                }
            }
        } else if (pindexIter->IsValid(BLOCK_VALID_TRANSACTIONS) && pindexIter->HaveTxsDownloaded()) {
            setBlockIndexCandidates.insert(pindexIter);
        }
    }

    if (chainActive.Tip() != nullptr) {
//如果有，我们不能根据提示删减块索引候选项
//由于chainactive为空，因此没有提示！
        PruneBlockIndexCandidates();

        CheckBlockIndex(params.GetConsensus());
    }

    return true;
}

bool RewindBlockIndex(const CChainParams& params) {
    if (!g_chainstate.RewindBlockIndex(params)) {
        return false;
    }

    if (chainActive.Tip() != nullptr) {
//FlushStateToDisk可能读取chainActive。保守
//跳过这里，我们要重新建立链状状态的索引，所以
//很快就会被称为“一群人”。
        CValidationState state;
        if (!FlushStateToDisk(params, state, FlushStateMode::ALWAYS)) {
            LogPrintf("RewindBlockIndex: unable to flush state to disk (%s)\n", FormatStateMessage(state));
            return false;
        }
    }

    return true;
}

void CChainState::UnloadBlockIndex() {
    nBlockSequenceId = 1;
    m_failed_blocks.clear();
    setBlockIndexCandidates.clear();
}

//连接完成后不能使用
//对等处理逻辑的
//块索引状态
void UnloadBlockIndex()
{
    LOCK(cs_main);
    chainActive.SetTip(nullptr);
    pindexBestInvalid = nullptr;
    pindexBestHeader = nullptr;
    mempool.clear();
    mapBlocksUnlinked.clear();
    vinfoBlockFile.clear();
    nLastBlockFile = 0;
    setDirtyBlockIndex.clear();
    setDirtyFileInfo.clear();
    versionbitscache.Clear();
    for (int b = 0; b < VERSIONBITS_NUM_BITS; b++) {
        warningcache[b].clear();
    }

    for (const BlockMap::value_type& entry : mapBlockIndex) {
        delete entry.second;
    }
    mapBlockIndex.clear();
    fHavePruned = false;

    g_chainstate.UnloadBlockIndex();
}

bool LoadBlockIndex(const CChainParams& chainparams)
{
//从数据库加载块索引
    bool needs_init = fReindex;
    if (!fReindex) {
        bool ret = LoadBlockIndexDB(chainparams);
        if (!ret) return false;
        needs_init = mapBlockIndex.empty();
    }

    if (needs_init) {
//这里的一切都是为了*新*重新索引/DBS。因此，虽然
//如果关闭，则loadblockindexdb可能已设置freindex
//以前我们不检查freindex和
//相反，只在要设置的loadblockindexdb之前检查它
//需要帮助。

        LogPrintf("Initializing databases...\n");
    }
    return true;
}

bool CChainState::LoadGenesisBlock(const CChainParams& chainparams)
{
    LOCK(cs_main);

//通过检查Genesis来检查我们是否已经初始化
//MAP受体阻滞剂注意，这里不能使用chainactive，因为它是
//设置基于硬币数据库，而不是块索引数据库，这是唯一的
//此时已加载。
    if (mapBlockIndex.count(chainparams.GenesisBlock().GetHash()))
        return true;

    try {
        CBlock &block = const_cast<CBlock&>(chainparams.GenesisBlock());
        CDiskBlockPos blockPos = SaveBlockToDisk(block, 0, chainparams, nullptr);
        if (blockPos.IsNull())
            return error("%s: writing genesis block to disk failed", __func__);
        CBlockIndex *pindex = AddToBlockIndex(block);
        ReceivedBlockTransactions(block, pindex, blockPos, chainparams.GetConsensus());
    } catch (const std::runtime_error& e) {
        return error("%s: failed to write genesis block: %s", __func__, e.what());
    }

    return true;
}

bool LoadGenesisBlock(const CChainParams& chainparams)
{
    return g_chainstate.LoadGenesisBlock(chainparams);
}

bool LoadExternalBlockFile(const CChainParams& chainparams, FILE* fileIn, CDiskBlockPos *dbp)
{
//父级未知的块的磁盘位置映射（仅用于重新索引）
    static std::multimap<uint256, CDiskBlockPos> mapBlocksUnknownParent;
    int64_t nStart = GetTimeMillis();

    int nLoaded = 0;
    try {
//它接收filein并在cBufferedFile析构函数中对其调用fclose（）。
        CBufferedFile blkdat(fileIn, 2*MAX_BLOCK_SERIALIZED_SIZE, MAX_BLOCK_SERIALIZED_SIZE+8, SER_DISK, CLIENT_VERSION);
        uint64_t nRewind = blkdat.GetPos();
        while (!blkdat.eof()) {
            boost::this_thread::interruption_point();

            blkdat.SetPos(nRewind);
nRewind++; //下次再启动一个字节，以防出现故障
blkdat.SetLimit(); //删除以前的限制
            unsigned int nSize = 0;
            try {
//定位页眉
                unsigned char buf[CMessageHeader::MESSAGE_START_SIZE];
                blkdat.FindByte(chainparams.MessageStart()[0]);
                nRewind = blkdat.GetPos()+1;
                blkdat >> buf;
                if (memcmp(buf, chainparams.MessageStart(), CMessageHeader::MESSAGE_START_SIZE))
                    continue;
//读取大小
                blkdat >> nSize;
                if (nSize < 80 || nSize > MAX_BLOCK_SERIALIZED_SIZE)
                    continue;
            } catch (const std::exception&) {
//找不到有效的块头；不要抱怨
                break;
            }
            try {
//读块
                uint64_t nBlockPos = blkdat.GetPos();
                if (dbp)
                    dbp->nPos = nBlockPos;
                blkdat.SetLimit(nBlockPos + nSize);
                blkdat.SetPos(nBlockPos);
                std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>();
                CBlock& block = *pblock;
                blkdat >> block;
                nRewind = blkdat.GetPos();

                uint256 hash = block.GetHash();
                {
                    LOCK(cs_main);
//检测出无序块，并将其存储以备以后使用
                    if (hash != chainparams.GetConsensus().hashGenesisBlock && !LookupBlockIndex(block.hashPrevBlock)) {
                        LogPrint(BCLog::REINDEX, "%s: Out of order block %s, parent %s not known\n", __func__, hash.ToString(),
                                block.hashPrevBlock.ToString());
                        if (dbp)
                            mapBlocksUnknownParent.insert(std::make_pair(block.hashPrevBlock, *dbp));
                        continue;
                    }

//处理以防块未知
                    CBlockIndex* pindex = LookupBlockIndex(hash);
                    if (!pindex || (pindex->nStatus & BLOCK_HAVE_DATA) == 0) {
                      CValidationState state;
                      if (g_chainstate.AcceptBlock(pblock, state, chainparams, nullptr, true, dbp, nullptr)) {
                          nLoaded++;
                      }
                      if (state.IsError()) {
                          break;
                      }
                    } else if (hash != chainparams.GetConsensus().hashGenesisBlock && pindex->nHeight % 1000 == 0) {
                      LogPrint(BCLog::REINDEX, "Block Import: already had block %s at height %d\n", hash.ToString(), pindex->nHeight);
                    }
                }

//激活Genesis块，以便继续正常节点进程
                if (hash == chainparams.GetConsensus().hashGenesisBlock) {
                    CValidationState state;
                    if (!ActivateBestChain(state, chainparams)) {
                        break;
                    }
                }

                NotifyHeaderTip();

//以前的递归进程遇到了此块的后续进程
                std::deque<uint256> queue;
                queue.push_back(hash);
                while (!queue.empty()) {
                    uint256 head = queue.front();
                    queue.pop_front();
                    std::pair<std::multimap<uint256, CDiskBlockPos>::iterator, std::multimap<uint256, CDiskBlockPos>::iterator> range = mapBlocksUnknownParent.equal_range(head);
                    while (range.first != range.second) {
                        std::multimap<uint256, CDiskBlockPos>::iterator it = range.first;
                        std::shared_ptr<CBlock> pblockrecursive = std::make_shared<CBlock>();
                        if (ReadBlockFromDisk(*pblockrecursive, it->second, chainparams.GetConsensus()))
                        {
                            LogPrint(BCLog::REINDEX, "%s: Processing out of order child %s of %s\n", __func__, pblockrecursive->GetHash().ToString(),
                                    head.ToString());
                            LOCK(cs_main);
                            CValidationState dummy;
                            if (g_chainstate.AcceptBlock(pblockrecursive, dummy, chainparams, nullptr, true, &it->second, nullptr))
                            {
                                nLoaded++;
                                queue.push_back(pblockrecursive->GetHash());
                            }
                        }
                        range.first++;
                        mapBlocksUnknownParent.erase(it);
                        NotifyHeaderTip();
                    }
                }
            } catch (const std::exception& e) {
                LogPrintf("%s: Deserialize or I/O error - %s\n", __func__, e.what());
            }
        }
    } catch (const std::runtime_error& e) {
        AbortNode(std::string("System error: ") + e.what());
    }
    if (nLoaded > 0)
        LogPrintf("Loaded %i blocks from external file in %dms\n", nLoaded, GetTimeMillis() - nStart);
    return nLoaded > 0;
}

void CChainState::CheckBlockIndex(const Consensus::Params& consensusParams)
{
    if (!fCheckBlockIndex) {
        return;
    }

    LOCK(cs_main);

//在重新索引期间，我们读取genesis块并在activatebestchain之前调用checkblockindex，
//所以我们在mapblockindex中有Genesis区块，但没有活动链。（一些测试时
//迭代块树要求已初始化chainActive。）
    if (chainActive.Height() < 0) {
        assert(mapBlockIndex.size() <= 1);
        return;
    }

//构建整个块树的前向指向图。
    std::multimap<CBlockIndex*,CBlockIndex*> forward;
    for (const std::pair<const uint256, CBlockIndex*>& entry : mapBlockIndex) {
        forward.insert(std::make_pair(entry.second->pprev, entry.second));
    }

    assert(forward.size() == mapBlockIndex.size());

    std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> rangeGenesis = forward.equal_range(nullptr);
    CBlockIndex *pindex = rangeGenesis.first->second;
    rangeGenesis.first++;
assert(rangeGenesis.first == rangeGenesis.second); //只有一个索引项具有父nullptr。

//使用深度优先搜索迭代整个块树。
//沿着这条路，记住从创世记来的路上是否有障碍物
//正在探索的块，这是第一个具有某些属性的块。
    size_t nNodes = 0;
    int nHeight = 0;
CBlockIndex* pindexFirstInvalid = nullptr; //pindex的最早祖先无效。
CBlockIndex* pindexFirstMissing = nullptr; //没有block的pindex的最早祖先有\u数据。
CBlockIndex* pindexFirstNeverProcessed = nullptr; //ntx==0的pindex的最早祖先。
CBlockIndex* pindexFirstNotTreeValid = nullptr; //pindex的最早祖先，没有block-valid-tree（无论是否有效）。
CBlockIndex* pindexFirstNotTransactionsValid = nullptr; //pindex的最早祖先，它没有块有效的事务（无论是否有效）。
CBlockIndex* pindexFirstNotChainValid = nullptr; //pindex的最早祖先，没有block-valid-chain（无论是否有效）。
CBlockIndex* pindexFirstNotScriptsValid = nullptr; //pindex的最早祖先，它没有块有效的脚本（无论是否有效）。
    while (pindex != nullptr) {
        nNodes++;
        if (pindexFirstInvalid == nullptr && pindex->nStatus & BLOCK_FAILED_VALID) pindexFirstInvalid = pindex;
        if (pindexFirstMissing == nullptr && !(pindex->nStatus & BLOCK_HAVE_DATA)) pindexFirstMissing = pindex;
        if (pindexFirstNeverProcessed == nullptr && pindex->nTx == 0) pindexFirstNeverProcessed = pindex;
        if (pindex->pprev != nullptr && pindexFirstNotTreeValid == nullptr && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_TREE) pindexFirstNotTreeValid = pindex;
        if (pindex->pprev != nullptr && pindexFirstNotTransactionsValid == nullptr && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_TRANSACTIONS) pindexFirstNotTransactionsValid = pindex;
        if (pindex->pprev != nullptr && pindexFirstNotChainValid == nullptr && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_CHAIN) pindexFirstNotChainValid = pindex;
        if (pindex->pprev != nullptr && pindexFirstNotScriptsValid == nullptr && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_SCRIPTS) pindexFirstNotScriptsValid = pindex;

//开始：实际一致性检查。
        if (pindex->pprev == nullptr) {
//Genesis块检查。
assert(pindex->GetBlockHash() == consensusParams.hashGenesisBlock); //Genesis块的哈希必须匹配。
assert(pindex == chainActive.Genesis()); //当前活动链的Genesis块必须是这个块。
        }
if (!pindex->HaveTxsDownloaded()) assert(pindex->nSequenceId <= 0); //对于未链接的块，不能将nsequenceid设置为正数（对于preciousBlock，使用负数）
//对于所有节点，有效的事务相当于NTX>0（无论是否进行了修剪）。
//如果没有进行修剪，have_data只相当于ntx>0（或有效的_事务）。
        if (!fHavePruned) {
//如果我们从未修剪过，那么have-data应该等于ntx>0
            assert(!(pindex->nStatus & BLOCK_HAVE_DATA) == (pindex->nTx == 0));
            assert(pindexFirstMissing == pindexFirstNeverProcessed);
        } else {
//如果我们已经修剪过了，那么我们只能说have_data意味着ntx>0
            if (pindex->nStatus & BLOCK_HAVE_DATA) assert(pindex->nTx > 0);
        }
        if (pindex->nStatus & BLOCK_HAVE_UNDO) assert(pindex->nStatus & BLOCK_HAVE_DATA);
assert(((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_TRANSACTIONS) == (pindex->nTx > 0)); //这是独立修剪。
//所有拥有数据的父级（在某一点上）相当于所有具有有效事务的父级，这相当于havetxsdownloaded（）。
        assert((pindexFirstNeverProcessed == nullptr) == pindex->HaveTxsDownloaded());
        assert((pindexFirstNotTransactionsValid == nullptr) == pindex->HaveTxsDownloaded());
assert(pindex->nHeight == nHeight); //Height必须一致。
assert(pindex->pprev == nullptr || pindex->nChainWork >= pindex->pprev->nChainWork); //对于除Genesis块以外的每个块，链的长度必须大于父块的长度。
assert(nHeight < 2 || (pindex->pskip && (pindex->pskip->nHeight < nHeight))); //pskip指针必须指向除前2个块以外的所有块。
assert(pindexFirstNotTreeValid == nullptr); //所有mapblockindex项必须至少是树有效的
if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_TREE) assert(pindexFirstNotTreeValid == nullptr); //树有效表示所有父级都是树有效的
if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_CHAIN) assert(pindexFirstNotChainValid == nullptr); //链有效意味着所有父级都是链有效的
if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_SCRIPTS) assert(pindexFirstNotScriptsValid == nullptr); //脚本有效意味着所有父级都是有效的脚本
        if (pindexFirstInvalid == nullptr) {
//检查是否存在无效块。
assert((pindex->nStatus & BLOCK_FAILED_MASK) == 0); //不能为没有无效父级的块设置失败的掩码。
        }
        if (!CBlockIndexWorkComparator()(pindex, chainActive.Tip()) && pindexFirstNeverProcessed == nullptr) {
            if (pindexFirstInvalid == nullptr) {
//如果此块的排序至少与当前提示的排序相同，
//是有效的，并且我们有它的父级的所有数据，它必须在
//setblockindex候选项。chainActive.tip（）也必须存在
//即使某些数据已被删减。
                if (pindexFirstMissing == nullptr || pindex == chainActive.Tip()) {
                    assert(setBlockIndexCandidates.count(pindex));
                }
//如果缺少某个父级，则此块可能位于
//setBlockIndexCandients，但由于缺少数据而必须删除。
//在这种情况下，它必须在mapblocksunlinked中——请参见下面的测试。
            }
} else { //如果此块的排序比当前提示更差，或者从未见过某个祖先的块，则它不能位于setblockindexcandidate中。
            assert(setBlockIndexCandidates.count(pindex) == 0);
        }
//检查此块是否在mapblocksunlinked中。
        std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> rangeUnlinked = mapBlocksUnlinked.equal_range(pindex->pprev);
        bool foundInUnlinked = false;
        while (rangeUnlinked.first != rangeUnlinked.second) {
            assert(rangeUnlinked.first->first == pindex->pprev);
            if (rangeUnlinked.first->second == pindex) {
                foundInUnlinked = true;
                break;
            }
            rangeUnlinked.first++;
        }
        if (pindex->pprev && (pindex->nStatus & BLOCK_HAVE_DATA) && pindexFirstNeverProcessed != nullptr && pindexFirstInvalid == nullptr) {
//如果此块具有可用的块数据，则从未接收到某些父级，并且没有无效的父级，则它必须位于mapblocksunlinked中。
            assert(foundInUnlinked);
        }
if (!(pindex->nStatus & BLOCK_HAVE_DATA)) assert(!foundInUnlinked); //如果没有数据，就不能在mapblocksunlinked中
if (pindexFirstMissing == nullptr) assert(!foundInUnlinked); //我们没有丢失任何父级的数据--不能在mapblocksunlinked中。
        if (pindex->pprev && (pindex->nStatus & BLOCK_HAVE_DATA) && pindexFirstNeverProcessed == nullptr && pindexFirstMissing != nullptr) {
//我们有这个块的数据，在某个时刻接收到所有父级的数据，但是我们目前缺少某些父级的数据。
assert(fHavePruned); //我们一定修剪过了。
//如果出现以下情况，此块可能已进入mapblocksunlinked：
//-它有一个后代，在某种程度上，它的工作比
//小费，以及
//-我们试着换了那个后代，但是失踪了。
//在chainactive和
//小费。
//因此，如果这个块本身比chainActive.tip（）更好，而且它不在
//setBlockIndexCandients，则它必须在MapBlockSunLinked中。
            if (!CBlockIndexWorkComparator()(pindex, chainActive.Tip()) && setBlockIndexCandidates.count(pindex) == 0) {
                if (pindexFirstInvalid == nullptr) {
                    assert(foundInUnlinked);
                }
            }
        }
//断言（pindex->getBlockHash（）==pindex->getBlockHeader（）.getHash（））；//可能太慢
//结束：实际一致性检查。

//尝试下降到第一个子节点。
        std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> range = forward.equal_range(pindex);
        if (range.first != range.second) {
//找到子节点。
            pindex = range.first->second;
            nHeight++;
            continue;
        }
//这是一个叶节点。
//向上移动，直到到达我们还没有访问过最后一个孩子的节点。
        while (pindex) {
//我们要么搬到Pindex的父母家，要么搬到Pindex的兄弟姐妹家。
//如果pindex是第一个具有特定属性的变量，请取消设置相应的变量。
            if (pindex == pindexFirstInvalid) pindexFirstInvalid = nullptr;
            if (pindex == pindexFirstMissing) pindexFirstMissing = nullptr;
            if (pindex == pindexFirstNeverProcessed) pindexFirstNeverProcessed = nullptr;
            if (pindex == pindexFirstNotTreeValid) pindexFirstNotTreeValid = nullptr;
            if (pindex == pindexFirstNotTransactionsValid) pindexFirstNotTransactionsValid = nullptr;
            if (pindex == pindexFirstNotChainValid) pindexFirstNotChainValid = nullptr;
            if (pindex == pindexFirstNotScriptsValid) pindexFirstNotScriptsValid = nullptr;
//找到我们的父母。
            CBlockIndex* pindexPar = pindex->pprev;
//找到我们刚拜访的孩子。
            std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> rangePar = forward.equal_range(pindexPar);
            while (rangePar.first->second != pindex) {
assert(rangePar.first != rangePar.second); //我们的父节点必须至少有一个作为子节点的节点。
                rangePar.first++;
            }
//继续下一个。
            rangePar.first++;
            if (rangePar.first != rangePar.second) {
//移动到同级。
                pindex = rangePar.first->second;
                break;
            } else {
//再往前走。
                pindex = pindexPar;
                nHeight--;
                continue;
            }
        }
    }

//检查我们是否真的遍历了整个地图。
    assert(nNodes == forward.size());
}

std::string CBlockFileInfo::ToString() const
{
    return strprintf("CBlockFileInfo(blocks=%u, size=%u, heights=%u...%u, time=%s...%s)", nBlocks, nSize, nHeightFirst, nHeightLast, FormatISO8601Date(nTimeFirst), FormatISO8601Date(nTimeLast));
}

CBlockFileInfo* GetBlockFileInfo(size_t n)
{
    LOCK(cs_LastBlockFile);

    return &vinfoBlockFile.at(n);
}

ThresholdState VersionBitsTipState(const Consensus::Params& params, Consensus::DeploymentPos pos)
{
    LOCK(cs_main);
    return VersionBitsState(chainActive.Tip(), params, pos, versionbitscache);
}

BIP9Stats VersionBitsTipStatistics(const Consensus::Params& params, Consensus::DeploymentPos pos)
{
    LOCK(cs_main);
    return VersionBitsStatistics(chainActive.Tip(), params, pos);
}

int VersionBitsTipStateSinceHeight(const Consensus::Params& params, Consensus::DeploymentPos pos)
{
    LOCK(cs_main);
    return VersionBitsStateSinceHeight(chainActive.Tip(), params, pos, versionbitscache);
}

static const uint64_t MEMPOOL_DUMP_VERSION = 1;

bool LoadMempool()
{
    const CChainParams& chainparams = Params();
    int64_t nExpiryTimeout = gArgs.GetArg("-mempoolexpiry", DEFAULT_MEMPOOL_EXPIRY) * 60 * 60;
    FILE* filestr = fsbridge::fopen(GetDataDir() / "mempool.dat", "rb");
    CAutoFile file(filestr, SER_DISK, CLIENT_VERSION);
    if (file.IsNull()) {
        LogPrintf("Failed to open mempool file from disk. Continuing anyway.\n");
        return false;
    }

    int64_t count = 0;
    int64_t expired = 0;
    int64_t failed = 0;
    int64_t already_there = 0;
    int64_t nNow = GetTime();

    try {
        uint64_t version;
        file >> version;
        if (version != MEMPOOL_DUMP_VERSION) {
            return false;
        }
        uint64_t num;
        file >> num;
        while (num--) {
            CTransactionRef tx;
            int64_t nTime;
            int64_t nFeeDelta;
            file >> tx;
            file >> nTime;
            file >> nFeeDelta;

            CAmount amountdelta = nFeeDelta;
            if (amountdelta) {
                mempool.PrioritiseTransaction(tx->GetHash(), amountdelta);
            }
            CValidationState state;
            if (nTime + nExpiryTimeout > nNow) {
                LOCK(cs_main);
                /*epttommorypoolwithtime（chainparams、mempool、state、tx、nullptr/*pfmissinginputs*/，ntime，
                                           nullptr/*pltxnreplaced（空指针/*pltxnreplaced）*/, false /* bypass_limits */, 0 /* nAbsurdFee */,

                                           /*SE/*测试接受*/）；
                if（state.isvalid（））
                    +计数；
                }否则{
                    //mempool可能已经包含事务，例如来自
                    //钱包在我们处理时已加载
                    //mempool事务；将这些事务视为有效的，而不是
                    //失败，但将它们标记为“已经存在”
                    if（mempool.exists（tx->gethash（））
                        +已经在那里了；
                    }否则{
                        +失败；
                    }
                }
            }否则{
                ++过期；
            }
            if（shutdownrequested（））
                返回错误；
        }
        std:：map<uint256，camount>mapdeltas；
        文件>>mapdeltas；

        对于（const auto&i:mapdeltas）
            mempool.优先权转移（i.first，i.second）；
        }
    catch（const std:：exception&e）
        logprintf（“未能反序列化磁盘%s上的mempool数据。仍要继续。\n”，e.what（））；
        返回错误；
    }

    logprintf（“从磁盘导入的mempool事务：%i成功，%i失败，%i过期，%i已经存在\n”，count，failed，expired，already_there）；
    回归真实；
}

bool dumpmempool（）。
{
    int64_t start=getTimemicros（）；

    std:：map<uint256，camount>mapdeltas；
    std:：vector<txmepoolinfo>vinfo；

    静态互斥转储互斥；
    锁定（转储互斥）；

    {
        锁（mempool.cs）；
        用于（const auto&i:mempool.mapdeltas）
            mapdeltas[i.first]=i.second；
        }
        vinfo=mempool.infoall（）；
    }

    int64_t mid=getTimemicros（）；

    尝试{
        文件*filestr=fsbridge:：fopen（getdatadir（）/“mempool.dat.new”，“wb”）；
        如果（！）文件）{
            返回错误；
        }

        cautofile文件（filestr，ser_disk，client_version）；

        uint64_t version=mempool_dump_version；
        文件<<版本；

        文件<（uint64_t）vinfo.size（）；
        用于（const auto&i:vinfo）
            文件<（i.tx）；
            文件<（int64_t）i.ntime；
            文件<（int64_t）i.nfeedelta；
            mapdeltas.erase（i.tx->gethash（））；
        }

        文件<<mapdeltas；
        如果（！）filecommit（file.get（））
            throw std:：runtime_error（“filecommit失败”）；
        文件FFSELL（）；
        renameover（getdatadir（）/“mempool.dat.new”，getdatadir（）/“mempool.dat”）；
        int64_t last=getTimemicros（）；
        logprintf（“转储的内存池：要复制的%gs，要转储的%gs\n”，（中间开始）*micro，（最后中间）*micro）；
    catch（const std:：exception&e）
        logprintf（“无法转储内存池：%s。仍要继续。\n”，e.what（））；
        返回错误；
    }
    回归真实；
}

/ /！猜猜我们在给定的块索引的验证过程中有多远
/ /！如果pindex尚未验证，则需要cs_main（因为nchaintx可能未设置）
双猜测验证进度（const chaintxdata&data，const cblockindex*pindex）
    如果（pindex==nullptr）
        返回0；

    int64_t nnow=时间（nullptr）；

    双ftxtotal；

    if（pindex->nchaintx<=data.ntxcount）
        ftxtotal=data.ntxcount+（now-data.ntime）*data.dtxrate；
    }否则{
        ftxtotal=pindex->nchaintx+（now-pindex->getBlockTime（））*data.dtxrate；
    }

    返回pindex->nchaintx/ftxtotal；
}

类cmaincleanup
{
公众：
    cmainCleanup（）
    ~cmaincleanup（）
        //块头
        blockmap:：iterator it1=mapblockindex.begin（）；
        为了（I1）！=mapblockindex.end（）；it1++）
            删除（*it1）.second；
        mapblockindex.clear（）；
    }
实例清理；
