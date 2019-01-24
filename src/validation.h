
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

#ifndef BITCOIN_VALIDATION_H
#define BITCOIN_VALIDATION_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <amount.h>
#include <coins.h>
#include <crypto/common.h> //对于Read 64
#include <fs.h>
#include <protocol.h> //对于cmessageheader:：messagestartchars
#include <policy/feerate.h>
#include <script/script_error.h>
#include <sync.h>
#include <versionbits.h>

#include <algorithm>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <atomic>

class CBlockIndex;
class CBlockTreeDB;
class CChainParams;
class CCoinsViewDB;
class CInv;
class CConnman;
class CScriptCheck;
class CBlockPolicyEstimator;
class CTxMemPool;
class CValidationState;
struct ChainTxData;

struct PrecomputedTransactionData;
struct LockPoints;

/*-whitelistreay的默认值。*/
static const bool DEFAULT_WHITELISTRELAY = true;
/*-whiteListforceRelay的默认值。*/
static const bool DEFAULT_WHITELISTFORCERELAY = true;
/*-minrelaytxfee的默认值，事务的最小中继费*/
static const unsigned int DEFAULT_MIN_RELAY_TX_FEE = 1000;
//！-最大费用默认值
static const CAmount DEFAULT_TRANSACTION_MAXFEE = COIN / 10;
//！不鼓励用户设置高于此金额（以Satoshis为单位）的每千字节费用。
static const CAmount HIGH_TX_FEE_PER_KB = COIN / 100;
//！-如果以高于此金额的费用（在Satoshis）呼叫，MaxTxFee将发出警告。
static const CAmount HIGH_MAX_TX_FEE = 100 * HIGH_TX_FEE_PER_KB;
/*-limitancestorcount的默认值，mempool祖先的最大数目*/
static const unsigned int DEFAULT_ANCESTOR_LIMIT = 25;
/*-limitancestorsize的默认值，Tx的最大千字节数+mempool祖先中的所有内容*/
static const unsigned int DEFAULT_ANCESTOR_SIZE_LIMIT = 101;
/*-limitDescendantCount的默认值，mempool子体的最大数目*/
static const unsigned int DEFAULT_DESCENDANT_LIMIT = 25;
/*-limitDescendantSize的默认值，mempool子体中的最大KB*/
static const unsigned int DEFAULT_DESCENDANT_SIZE_LIMIT = 101;
/*-mempoolexpiry的默认值，mempool事务的过期时间（小时）*/
static const unsigned int DEFAULT_MEMPOOL_EXPIRY = 336;
/*REORG期间要存储以进行处理的事务的最大KB数*/
static const unsigned int MAX_DISCONNECTED_TX_POOL_SIZE = 20000;
/*一个黑色的最大尺寸？？？？？？？.dat文件（从0.8开始）*/
static const unsigned int MAX_BLOCKFILE_SIZE = 0x8000000; //128 MIB
/*BLK的预分配块大小？？？？？？？.dat文件（从0.8开始）*/
static const unsigned int BLOCKFILE_CHUNK_SIZE = 0x1000000; //16 MIB
/*rev的预分配块大小？？？？？？？.dat文件（从0.8开始）*/
static const unsigned int UNDOFILE_CHUNK_SIZE = 0x100000; //1 MIB

/*允许的最大脚本检查线程数*/
static const int MAX_SCRIPTCHECK_THREADS = 16;
/*-par默认值（脚本检查线程数，0=auto）*/
static const int DEFAULT_SCRIPTCHECK_THREADS = 0;
/*在任何给定时间可以从单个对等机请求的块数。*/
static const int MAX_BLOCKS_IN_TRANSIT_PER_PEER = 16;
/*在断开连接之前，对等方必须暂停阻止下载进度的超时（秒）。*/
static const unsigned int BLOCK_STALLING_TIMEOUT = 2;
/*在一个GetHeaders结果中发送的头数。我们的假设是，如果一个对等端发送
 *小于这个数字，我们就到达了它的尖端。更改此值是协议升级。*/

static const unsigned int MAX_HEADERS_RESULTS = 2000;
/*我们愿意为同龄人提供的最大块深度
 *如有要求。对于较旧的块，将发送常规的块响应。*/

static const int MAX_CMPCTBLOCK_DEPTH = 5;
/*我们愿意响应getblocktxn请求的最大块深度。*/
static const int MAX_BLOCKTXN_DEPTH = 10;
/*“块下载窗口”的大小：比当前高度提前多少？
 *较大的窗口允许对等端之间存在较大的下载速度差异，但会增加潜力
 *磁盘上块的无序程度（这使得重新索引和修剪更加困难）。我们可能会
 *希望在某个时间点将其设置为每对等自适应值。*/

static const unsigned int BLOCK_DOWNLOAD_WINDOW = 1024;
/*在将块/块索引写入磁盘之间等待的时间（秒）。*/
static const unsigned int DATABASE_WRITE_INTERVAL = 60 * 60;
/*将链状态刷新到磁盘之间的等待时间（秒）。*/
static const unsigned int DATABASE_FLUSH_INTERVAL = 24 * 60 * 60;
/*拒绝消息的最大长度。*/
static const unsigned int MAX_REJECT_MESSAGE_LENGTH = 111;
/*块下载超时基数，以块间隔的百万分之一（即10分钟）表示。*/
static const int64_t BLOCK_DOWNLOAD_TIMEOUT_BASE = 1000000;
/*每个并行下载对等端的额外块下载超时（即5分钟）*/
static const int64_t BLOCK_DOWNLOAD_TIMEOUT_PER_PEER = 500000;

static const int64_t DEFAULT_MAX_TIP_AGE = 24 * 60 * 60;
/*以秒为单位的小费最长使用期限，我们将被视为最新的费用估算。*/
static const int64_t MAX_FEE_ESTIMATION_TIP_AGE = 3 * 60 * 60;

/*-permitbaremilsig的默认值*/
static const bool DEFAULT_PERMIT_BAREMULTISIG = true;
static const bool DEFAULT_CHECKPOINTS_ENABLED = true;
static const bool DEFAULT_TXINDEX = false;
static const unsigned int DEFAULT_BANSCORE_THRESHOLD = 100;
/*-persistmempool的默认值*/
static const bool DEFAULT_PERSIST_MEMPOOL = true;
/*-mempoolreplacement的默认值*/
static const bool DEFAULT_ENABLE_REPLACEMENT = true;
/*使用费用筛选的默认值*/
static const bool DEFAULT_FEEFILTER = true;

/*用头消息中继块时要通知的头的最大数目*/
static const unsigned int MAX_BLOCKS_TO_ANNOUNCE = 8;

/*DOS得分前未连接头公告的最大数目*/
static const int MAX_UNCONNECTING_HEADERS = 10;

static const bool DEFAULT_PEERBLOOMFILTERS = true;

/*-stopatheight的默认值*/
static const int DEFAULT_STOPATHEIGHT = 0;

struct BlockHasher
{
//它用于调用uint256中的“getcheaphash（）”，该函数后来被移动；该函数
//但是，便宜的哈希函数只调用readle64（），因此最终结果是
//完全相同的
    size_t operator()(const uint256& hash) const { return ReadLE64(hash.begin()); }
};

extern CScript COINBASE_FLAGS;
extern CCriticalSection cs_main;
extern CBlockPolicyEstimator feeEstimator;
extern CTxMemPool mempool;
extern std::atomic_bool g_is_mempool_loaded;
typedef std::unordered_map<uint256, CBlockIndex*, BlockHasher> BlockMap;
extern BlockMap& mapBlockIndex;
extern uint64_t nLastBlockTx;
extern uint64_t nLastBlockWeight;
extern const std::string strMessageMagic;
extern Mutex g_best_block_mutex;
extern std::condition_variable g_best_block_cv;
extern uint256 g_best_block;
extern std::atomic_bool fImporting;
extern std::atomic_bool fReindex;
extern int nScriptCheckThreads;
extern bool fIsBareMultisigStd;
extern bool fRequireStandard;
extern bool fCheckBlockIndex;
extern bool fCheckpointsEnabled;
extern size_t nCoinCacheUsage;
/*小于此值的费率被视为零费用（用于中继、挖掘和事务创建）*/
extern CFeeRate minRelayTxFee;
/*钱包和mempool使用的绝对最高交易费用（在Satoshis）（拒绝sendrawtransaction中的高费用）*/
extern CAmount maxTxFee;
/*如果提示早于此时间（秒），则认为节点处于初始块下载状态。*/
extern int64_t nMaxTipAge;
extern bool fEnableReplacement;

/*块散列，我们假定其祖先拥有有效的脚本而不检查它们。*/
extern uint256 hashAssumeValid;

/*我们假设的最小工作量存在于某个有效的链上。*/
extern arith_uint256 nMinimumChainWork;

/*迄今为止我们看到的最好的头（用于getheaders查询的起始点）。*/
extern CBlockIndex *pindexBestHeader;

/*所需的最小磁盘空间-用于checkdiskspace（）。*/
static const uint64_t nMinDiskSpace = 52428800;

/*修剪相关变量和常量*/
/*如果曾经修剪过任何块文件，则为true。*/
extern bool fHavePruned;
/*如果我们在-prune模式下运行，则为true。*/
extern bool fPruneMode;
/*我们试图保留在下面的块文件的MIB数量。*/
extern uint64_t nPruneTarget;
/*在chainActive.tip（）的min_blocks_to_keep内包含块高度的块文件将不会被修剪。*/
static const unsigned int MIN_BLOCKS_TO_KEEP = 288;
/*信号节点网络限制所需的最小数据块*/
static const unsigned int NODE_NETWORK_LIMITED_MIN_BLOCKS = 288;

static const signed int DEFAULT_CHECKBLOCKS = 6;
static const unsigned int DEFAULT_CHECKLEVEL = 3;

//要求用户为块和撤消文件分配至少550MB（黑色？？？？DAT和Rev？？？？DAT）
//在每个数据块1MB时，288个数据块=288MB。
//撤销数据增加15%=331MB
//对于孤立块率=397MB，增加20%
//我们希望修剪后的低水位至少为397 MB，因为我们修剪
//全块文件块，我们需要高水位标志，触发修剪
//一个128MB的块文件+添加15%的撤消数据=147MB，总计545MB
//将目标设置为大于550MB将使我们有可能尊重目标。
static const uint64_t MIN_DISK_SPACE_FOR_BLOCK_FILES = 550 * 1024 * 1024;

/*
 *处理一个传入块。这只会在最已知的有效值之后返回
 *块被激活。但请注意，它并不保证
 *已检查传递给它的特定块的有效性！
 *
 *如果你想*可能*得到关于pblock是否有效的反馈，你必须
 *安装cvalidationInterface（请参阅validationInterface.h）-这将
 *每当*any*块完成验证时调用它的BlockChecked方法。
 *
 *请注意，我方保证工作证明对PBlock有效，或
 *（也可能）将调用BlockChecked。
 *
 *不能在
 *validationInterface回调。
 *
 *@param[in]pblock我们要处理的块。
 *@param[in]fforceprocessing处理此块，即使未请求；用于非网络块源和白名单对等。
 *@param[out]fnewblock一个布尔值，设置为指示是否通过此调用首次接收到块。
 *@如果state.isvalid（），则返回true
 **/

bool ProcessNewBlock(const CChainParams& chainparams, const std::shared_ptr<const CBlock> pblock, bool fForceProcessing, bool* fNewBlock) LOCKS_EXCLUDED(cs_main);

/*
 *处理传入的块头。
 *
 *不能在
 *validationInterface回调。
 *
 *@param[in]本身阻止块头
 *@param[out]状态如果处理它们时发生任何错误，此状态可能设置为错误状态。
 *@param[in]chain params要连接的链的参数
 *@param[out]ppindex如果设置，指针将被设置为指向给定头的最后一个新块索引对象。
 *@param[out]第一个\无效的第一个未通过验证的头（如果存在）
 **/

bool ProcessNewBlockHeaders(const std::vector<CBlockHeader>& block, CValidationState& state, const CChainParams& chainparams, const CBlockIndex** ppindex = nullptr, CBlockHeader* first_invalid = nullptr) LOCKS_EXCLUDED(cs_main);

/*检查是否有足够的磁盘空间用于传入块*/
bool CheckDiskSpace(uint64_t nAdditionalBytes = 0, bool blocks_dir = false);
/*打开一个块文件（黑色？？？？？？？DAT）*/
FILE* OpenBlockFile(const CDiskBlockPos &pos, bool fReadOnly = false);
/*转换为文件系统路径*/
fs::path GetBlockPosFilename(const CDiskBlockPos &pos, const char *prefix);
/*从外部文件导入块*/
bool LoadExternalBlockFile(const CChainParams& chainparams, FILE* fileIn, CDiskBlockPos *dbp = nullptr);
/*确保在块树中有一个Genesis块，可能将其写入磁盘。*/
bool LoadGenesisBlock(const CChainParams& chainparams);
/*从磁盘加载块树和硬币数据库，
 *如果使用-reindex运行，则初始化状态。*/

bool LoadBlockIndex(const CChainParams& chainparams) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
/*根据数据库信息更新链提示。*/
bool LoadChainTip(const CChainParams& chainparams) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
/*卸载数据库信息*/
void UnloadBlockIndex();
/*运行脚本检查线程的实例*/
void ThreadScriptCheck();
/*检查是否正在进行初始块下载（从磁盘或网络同步）*/
bool IsInitialBlockDownload();
/*检索事务（从内存池或磁盘，如果可能的话）*/
bool GetTransaction(const uint256& hash, CTransactionRef& tx, const Consensus::Params& params, uint256& hashBlock, bool fAllowSlow = false, CBlockIndex* blockIndex = nullptr);
/*
 *找到最著名的木块，使其成为木链的尖端。
 *
 *不可在保持CSU MAIN的情况下调用。不能在
 *validationInterface回调。
 **/

bool ActivateBestChain(CValidationState& state, const CChainParams& chainparams, std::shared_ptr<const CBlock> pblock = std::shared_ptr<const CBlock>());
CAmount GetBlockSubsidy(int nHeight, const Consensus::Params& consensusParams);

/*猜测验证进度（0.0=起源和1.0=当前提示之间的分数）。*/
double GuessVerificationProgress(const ChainTxData& data, const CBlockIndex* pindex);

/*计算块和撤消文件当前使用的磁盘空间量*/
uint64_t CalculateCurrentUsage();

/*
 *将一个块文件标记为修剪。
 **/

void PruneOneBlockFile(const int fileNumber);

/*
 *实际取消链接指定的文件
 **/

void UnlinkPrunedFiles(const std::set<int>& setFilesToPrune);

/*将所有状态、索引和缓冲区刷新到磁盘。*/
void FlushStateToDisk();
/*修剪块文件并将状态刷新到磁盘。*/
void PruneAndFlush();
/*将块文件修剪到给定高度*/
void PruneBlockFilesManual(int nManualPruneHeight);

/*（尝试）将事务添加到内存池
 *pltxnreplaced将被附加到，所有交易将从mempool中替换。*/

bool AcceptToMemoryPool(CTxMemPool& pool, CValidationState &state, const CTransactionRef &tx,
                        bool* pfMissingInputs, std::list<CTransactionRef>* plTxnReplaced,
                        bool bypass_limits, const CAmount nAbsurdFee, bool test_accept=false) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/*将cvalidationState转换为人类可读的日志消息*/
std::string FormatStateMessage(const CValidationState &state);

/*在当前提示处获取给定部署的BIP9状态。*/
ThresholdState VersionBitsTipState(const Consensus::Params& params, Consensus::DeploymentPos pos);

/*获取当前提示下给定部署的bip9状态的数字统计信息。*/
BIP9Stats VersionBitsTipStatistics(const Consensus::Params& params, Consensus::DeploymentPos pos);

/*获取当前提示上的块构建的bip9部署切换到状态的块高度。*/
int VersionBitsTipStateSinceHeight(const Consensus::Params& params, Consensus::DeploymentPos pos);


/*将此事务的效果应用于由视图表示的utxo集*/
void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, int nHeight);

/*事务验证功能*/

/*
 *检查在要创建的下一个块中，事务是否是最终的。
 *
 *使用当前块高度和适当的块时间调用isFinalTx（）。
 *
 *标志定义见共识/共识.h。
 **/

bool CheckFinalTx(const CTransaction &tx, int flags = -1) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/*
 *测试当前链条上的锁定点高度和时间是否仍然有效。
 **/

bool TestLockPointValidity(const LockPoints* lp) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/*
 *检查在要创建的下一个块中，事务是否为BIP 68 final。
 *
 *使用当前活动链顶端的数据模拟调用SequenceLocks（）。
 *可以选择将计算出的高度和时间以及哈希值存储在锁定点中。
 *计算所需的块或跳过计算并使用锁定点
 *通过评估。
 *如果checkSequenceLocks返回false，则锁定点不应视为有效。
 *
 *标志定义见共识/共识.h。
 **/

bool CheckSequenceLocks(const CTxMemPool& pool, const CTransaction& tx, int flags, LockPoints* lp = nullptr, bool useExistingLockPoints = false) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/*
 *表示一个脚本验证的闭包
 *请注意，此存储对支出交易的引用
 **/

class CScriptCheck
{
private:
    CTxOut m_tx_out;
    const CTransaction *ptxTo;
    unsigned int nIn;
    unsigned int nFlags;
    bool cacheStore;
    ScriptError error;
    PrecomputedTransactionData *txdata;

public:
    CScriptCheck(): ptxTo(nullptr), nIn(0), nFlags(0), cacheStore(false), error(SCRIPT_ERR_UNKNOWN_ERROR) {}
    CScriptCheck(const CTxOut& outIn, const CTransaction& txToIn, unsigned int nInIn, unsigned int nFlagsIn, bool cacheIn, PrecomputedTransactionData* txdataIn) :
        m_tx_out(outIn), ptxTo(&txToIn), nIn(nInIn), nFlags(nFlagsIn), cacheStore(cacheIn), error(SCRIPT_ERR_UNKNOWN_ERROR), txdata(txdataIn) { }

    bool operator()();

    void swap(CScriptCheck &check) {
        std::swap(ptxTo, check.ptxTo);
        std::swap(m_tx_out, check.m_tx_out);
        std::swap(nIn, check.nIn);
        std::swap(nFlags, check.nFlags);
        std::swap(cacheStore, check.cacheStore);
        std::swap(error, check.error);
        std::swap(txdata, check.txdata);
    }

    ScriptError GetScriptError() const { return error; }
};

/*初始化脚本执行缓存*/
void InitScriptExecutionCache();


/*块的磁盘访问功能*/
bool ReadBlockFromDisk(CBlock& block, const CDiskBlockPos& pos, const Consensus::Params& consensusParams);
bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex, const Consensus::Params& consensusParams);
bool ReadRawBlockFromDisk(std::vector<uint8_t>& block, const CDiskBlockPos& pos, const CMessageHeader::MessageStartChars& message_start);
bool ReadRawBlockFromDisk(std::vector<uint8_t>& block, const CBlockIndex* pindex, const CMessageHeader::MessageStartChars& message_start);

/*用于验证块和更新块树的函数*/

/*上下文无关的有效性检查*/
bool CheckBlock(const CBlock& block, CValidationState& state, const Consensus::Params& consensusParams, bool fCheckPOW = true, bool fCheckMerkleRoot = true);

/*检查一个块从开始到结束是否完全有效（仅在当前最佳块的顶部有效）*/
bool TestBlockValidity(CValidationState& state, const CChainParams& chainparams, const CBlock& block, CBlockIndex* pindexPrev, bool fCheckPOW = true, bool fCheckMerkleRoot = true) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/*检查区块是否需要见证承诺。*/
bool IsWitnessEnabled(const CBlockIndex* pindexPrev, const Consensus::Params& params);

/*检查NullDummy（BIP 147）是否激活。*/
bool IsNullDummyEnabled(const CBlockIndex* pindexPrev, const Consensus::Params& params);

/*当活动链中存在缺少数据的块时，倒带链状态并将其从块索引中删除。*/
bool RewindBlockIndex(const CChainParams& params);

/*更新未提交的块结构（当前：仅限见证保留值）。这对于提交的块是安全的。*/
void UpdateUncommittedBlockStructures(CBlock& block, const CBlockIndex* pindexPrev, const Consensus::Params& consensusParams);

/*为块生成必要的CoinBase承诺（修改哈希，不要调用挖掘的块）。*/
std::vector<unsigned char> GenerateCoinbaseCommitment(CBlock& block, const CBlockIndex* pindexPrev, const Consensus::Params& consensusParams);

/*verifydb的raii包装：验证块和硬币数据库的一致性*/
class CVerifyDB {
public:
    CVerifyDB();
    ~CVerifyDB();
    bool VerifyDB(const CChainParams& chainparams, CCoinsView *coinsview, int nCheckLevel, int nCheckDepth);
};

/*未完全应用于数据库的重播块。*/
bool ReplayBlocks(const CChainParams& params, CCoinsView* view);

inline CBlockIndex* LookupBlockIndex(const uint256& hash)
{
    AssertLockHeld(cs_main);
    BlockMap::const_iterator it = mapBlockIndex.find(hash);
    return it == mapBlockIndex.end() ? nullptr : it->second;
}

/*找到参数链和定位器之间的最后一个公共块。*/
CBlockIndex* FindForkInGlobalIndex(const CChain& chain, const CBlockLocator& locator) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/*将一个块标记为珍贵并重新组织。
 *
 *不能在
 *validationInterface回调。
 **/

bool PreciousBlock(CValidationState& state, const CChainParams& params, CBlockIndex *pindex) LOCKS_EXCLUDED(cs_main);

/*将块标记为无效。*/
bool InvalidateBlock(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/*从块及其后代中删除无效状态。*/
void ResetBlockFailureFlags(CBlockIndex* pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/*当前连接的块链（受cs_main保护）。*/
extern CChain& chainActive;

/*指向硬币数据库的全局变量（受cs_main保护）*/
extern std::unique_ptr<CCoinsViewDB> pcoinsdbview;

/*指向活动ccoinsview的全局变量（受cs_main保护）*/
extern std::unique_ptr<CCoinsViewCache> pcoinsTip;

/*指向活动块树的全局变量（受cs_main保护）*/
extern std::unique_ptr<CBlockTreeDB> pblocktree;

/*
 *返回花费高度，它比inputs.getBestBlock（）多一个。
 *检查时，getBestBlock（）引用父块。（受CS-MAIN保护）
 *对于mempool检查也是如此。
 **/

int GetSpendHeight(const CCoinsViewCache& inputs);

extern VersionBitsCache versionbitscache;

/*
 *确定新块应使用的转换。
 **/

int32_t ComputeBlockVersion(const CBlockIndex* pindexPrev, const Consensus::Params& params);

/*AcceptToModel可以返回大于或等于此值的拒绝代码
 *对于交易，表示内部状况。他们不能也不应该
 *通过P2P网络发送。
 **/

static const unsigned int REJECT_INTERNAL = 0x100;
/*费用太高。不能由P2P事务触发*/
static const unsigned int REJECT_HIGHFEE = 0x100;

/*获取一个块文件的块文件信息项*/
CBlockFileInfo* GetBlockFileInfo(size_t n);

/*将内存池转储到磁盘。*/
bool DumpMempool();

/*从磁盘加载内存池。*/
bool LoadMempool();

//！检查是否修剪与此索引项关联的块。
inline bool IsBlockPruned(const CBlockIndex* pblockindex)
{
    return (fHavePruned && !(pblockindex->nStatus & BLOCK_HAVE_DATA) && pblockindex->nTx > 0);
}

#endif //比特币验证
