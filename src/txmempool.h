
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

#ifndef BITCOIN_TXMEMPOOL_H
#define BITCOIN_TXMEMPOOL_H

#include <memory>
#include <set>
#include <map>
#include <vector>
#include <utility>
#include <string>

#include <amount.h>
#include <coins.h>
#include <crypto/siphash.h>
#include <indirectmap.h>
#include <policy/feerate.h>
#include <primitives/transaction.h>
#include <sync.h>
#include <random.h>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/signals2/signal.hpp>

class CBlockIndex;
extern CCriticalSection cs_main;

/*硬币中使用的假高度值表示它们只在内存池中（从0.8开始）*/
static const uint32_t MEMPOOL_HEIGHT = 0x7FFFFFFF;

struct LockPoints
{
//将设置为区块链高度和过去的中间时间
//满足所有相对锁定时间所需的值
//根据我们对区块链历史的看法，该Tx的约束（bip68）
    int height;
    int64_t time;
//只要当前链从最高高度块下降
//包含计算中使用的一个输入，然后缓存
//即使在REORG之后，值仍然有效。
    CBlockIndex* maxInputBlock;

    LockPoints() : height(0), time(0), maxInputBlock(nullptr) { }
};

class CTxMemPool;

/*\类ctxmempoolEntry
 *
 *ctxmempoolEntry还存储有关相应事务的数据。
 *作为依赖于该事务的所有内存池事务的数据
 *（“后代”事务）。
 *
 *当新条目添加到mempool时，我们会更新子体状态。
 （ncountWithDescendants、nsizeWithDescendants和nmodFeesWithDescendants）的
 *新添加事务的所有祖先。
 *
 **/


class CTxMemPoolEntry
{
private:
    const CTransactionRef tx;
const CAmount nFee;             //！<缓存以避免昂贵的父事务查找
const size_t nTxWeight;         //！<…避免重新计算tx权重（也用于gettxsize（））
const size_t nUsageSize;        //！<…和总内存使用量
const int64_t nTime;            //！<进入mempool时的本地时间
const unsigned int entryHeight; //！<进入mempool时的链高度
const bool spendsCoinbase;      //！<跟踪使用CoinBase的交易
const int64_t sigOpCost;        //！<SIGOP总成本
int64_t feeDelta;          //！<用于确定块中挖掘事务的优先级
LockPoints lockPoints;     //！<跟踪Tx最终的高度和时间

//有关此事务的子代的信息
//mempool；如果删除此事务，则必须删除所有这些
//后代也是。
uint64_t nCountWithDescendants;  //！<后代事务数
uint64_t nSizeWithDescendants;   //！<…尺寸
CAmount nModFeesWithDescendants; //！<…总费用（包括我们）

//祖先事务的类似统计
    uint64_t nCountWithAncestors;
    uint64_t nSizeWithAncestors;
    CAmount nModFeesWithAncestors;
    int64_t nSigOpCostWithAncestors;

public:
    CTxMemPoolEntry(const CTransactionRef& _tx, const CAmount& _nFee,
                    int64_t _nTime, unsigned int _entryHeight,
                    bool spendsCoinbase,
                    int64_t nSigOpsCost, LockPoints lp);

    const CTransaction& GetTx() const { return *this->tx; }
    CTransactionRef GetSharedTx() const { return this->tx; }
    const CAmount& GetFee() const { return nFee; }
    size_t GetTxSize() const;
    size_t GetTxWeight() const { return nTxWeight; }
    int64_t GetTime() const { return nTime; }
    unsigned int GetHeight() const { return entryHeight; }
    int64_t GetSigOpCost() const { return sigOpCost; }
    int64_t GetModifiedFee() const { return nFee + feeDelta; }
    size_t DynamicMemoryUsage() const { return nUsageSize; }
    const LockPoints& GetLockPoints() const { return lockPoints; }

//调整后代状态。
    void UpdateDescendantState(int64_t modifySize, CAmount modifyFee, int64_t modifyCount);
//调整祖先状态
    void UpdateAncestorState(int64_t modifySize, CAmount modifyFee, int64_t modifyCount, int64_t modifySigOps);
//更新用于挖掘优先级得分的费用增量，以及
//具有后代的修改费用。
    void UpdateFeeDelta(int64_t feeDelta);
//在REORG之后更新锁定点
    void UpdateLockPoints(const LockPoints& lp);

    uint64_t GetCountWithDescendants() const { return nCountWithDescendants; }
    uint64_t GetSizeWithDescendants() const { return nSizeWithDescendants; }
    CAmount GetModFeesWithDescendants() const { return nModFeesWithDescendants; }

    bool GetSpendsCoinbase() const { return spendsCoinbase; }

    uint64_t GetCountWithAncestors() const { return nCountWithAncestors; }
    uint64_t GetSizeWithAncestors() const { return nSizeWithAncestors; }
    CAmount GetModFeesWithAncestors() const { return nModFeesWithAncestors; }
    int64_t GetSigOpCostWithAncestors() const { return nSigOpCostWithAncestors; }

mutable size_t vTxHashesIdx; //！<mempool的vtxhash中的索引
};

//用于修改ctxmempool:：maptx的帮助程序，它是一个增强多_索引。
struct update_descendant_state
{
    update_descendant_state(int64_t _modifySize, CAmount _modifyFee, int64_t _modifyCount) :
        modifySize(_modifySize), modifyFee(_modifyFee), modifyCount(_modifyCount)
    {}

    void operator() (CTxMemPoolEntry &e)
        { e.UpdateDescendantState(modifySize, modifyFee, modifyCount); }

    private:
        int64_t modifySize;
        CAmount modifyFee;
        int64_t modifyCount;
};

struct update_ancestor_state
{
    update_ancestor_state(int64_t _modifySize, CAmount _modifyFee, int64_t _modifyCount, int64_t _modifySigOpsCost) :
        modifySize(_modifySize), modifyFee(_modifyFee), modifyCount(_modifyCount), modifySigOpsCost(_modifySigOpsCost)
    {}

    void operator() (CTxMemPoolEntry &e)
        { e.UpdateAncestorState(modifySize, modifyFee, modifyCount, modifySigOpsCost); }

    private:
        int64_t modifySize;
        CAmount modifyFee;
        int64_t modifyCount;
        int64_t modifySigOpsCost;
};

struct update_fee_delta
{
    explicit update_fee_delta(int64_t _feeDelta) : feeDelta(_feeDelta) { }

    void operator() (CTxMemPoolEntry &e) { e.UpdateFeeDelta(feeDelta); }

private:
    int64_t feeDelta;
};

struct update_lock_points
{
    explicit update_lock_points(const LockPoints& _lp) : lp(_lp) { }

    void operator() (CTxMemPoolEntry &e) { e.UpdateLockPoints(lp); }

private:
    const LockPoints& lp;
};

//从ctxmempoolEntry或ctransactionRef提取事务哈希
struct mempoolentry_txid
{
    typedef uint256 result_type;
    result_type operator() (const CTxMemPoolEntry &entry) const
    {
        return entry.GetTx().GetHash();
    }

    result_type operator() (const CTransactionRef& tx) const
    {
        return tx->GetHash();
    }
};

/*\类比较xmempooltyDecentsScore
 *
 *按最大值对条目进行排序（条目的Tx的得分/大小，得分/大小以及所有后代）。
 **/

class CompareTxMemPoolEntryByDescendantScore
{
public:
    bool operator()(const CTxMemPoolEntry& a, const CTxMemPoolEntry& b) const
    {
        double a_mod_fee, a_size, b_mod_fee, b_size;

        GetModFeeAndSize(a, a_mod_fee, a_size);
        GetModFeeAndSize(b, b_mod_fee, b_size);

//通过将（a/b>c/d）重写为（a*d>c*b）来避免除法。
        double f1 = a_mod_fee * b_size;
        double f2 = a_size * b_mod_fee;

        if (f1 == f2) {
            return a.GetTime() >= b.GetTime();
        }
        return f1 < f2;
    }

//返回我们用于排序此条目的费用/大小。
    void GetModFeeAndSize(const CTxMemPoolEntry &a, double &mod_fee, double &size) const
    {
//将feerate与后代与事务feerate进行比较，以及
//返回最大值的费用/大小。
        double f1 = (double)a.GetModifiedFee() * a.GetSizeWithDescendants();
        double f2 = (double)a.GetModFeesWithDescendants() * a.GetTxSize();

        if (f2 > f1) {
            mod_fee = a.GetModFeesWithDescendants();
            size = a.GetSizeWithDescendants();
        } else {
            mod_fee = a.GetModifiedFee();
            size = a.GetTxSize();
        }
    }
};

/*\类比较xmempoolEntryScore
 *
 *按条目（费用/大小）的频率降序排序
 *这只用于事务中继，所以我们使用getfee（）。
 *而不是getModifiedFee（），以避免泄漏优先级
 *通过排序顺序显示的信息。
 **/

class CompareTxMemPoolEntryByScore
{
public:
    bool operator()(const CTxMemPoolEntry& a, const CTxMemPoolEntry& b) const
    {
        double f1 = (double)a.GetFee() * b.GetTxSize();
        double f2 = (double)b.GetFee() * a.GetTxSize();
        if (f1 == f2) {
            return b.GetTx().GetHash() < a.GetTx().GetHash();
        }
        return f1 > f2;
    }
};

class CompareTxMemPoolEntryByEntryTime
{
public:
    bool operator()(const CTxMemPoolEntry& a, const CTxMemPoolEntry& b) const
    {
        return a.GetTime() < b.GetTime();
    }
};

/*\类比较xmempooltyByancestorScore
 *
 *按分钟对条目进行排序（条目的Tx的得分/大小，所有祖先的得分/大小）。
 **/

class CompareTxMemPoolEntryByAncestorFee
{
public:
    template<typename T>
    bool operator()(const T& a, const T& b) const
    {
        double a_mod_fee, a_size, b_mod_fee, b_size;

        GetModFeeAndSize(a, a_mod_fee, a_size);
        GetModFeeAndSize(b, b_mod_fee, b_size);

//通过将（a/b>c/d）重写为（a*d>c*b）来避免除法。
        double f1 = a_mod_fee * b_size;
        double f2 = a_size * b_mod_fee;

        if (f1 == f2) {
            return a.GetTx().GetHash() < b.GetTx().GetHash();
        }
        return f1 > f2;
    }

//返回我们用于排序此条目的费用/大小。
    template <typename T>
    void GetModFeeAndSize(const T &a, double &mod_fee, double &size) const
    {
//将feerate与祖先进行比较以确定事务的feerate，以及
//返回最小值的费用/大小。
        double f1 = (double)a.GetModifiedFee() * a.GetSizeWithAncestors();
        double f2 = (double)a.GetModFeesWithAncestors() * a.GetTxSize();

        if (f1 > f2) {
            mod_fee = a.GetModFeesWithAncestors();
            size = a.GetSizeWithAncestors();
        } else {
            mod_fee = a.GetModifiedFee();
            size = a.GetTxSize();
        }
    }
};

//多索引标记名
struct descendant_score {};
struct entry_time {};
struct ancestor_score {};

class CBlockPolicyEstimator;

/*
 *有关mempool事务的信息。
 **/

struct TxMempoolInfo
{
    /*交易本身*/
    CTransactionRef tx;

    /*事务进入内存池的时间。*/
    int64_t nTime;

    /*交易日期。*/
    CFeeRate feeRate;

    /*费用增量。*/
    int64_t nFeeDelta;
};

/*从内存池中删除事务的原因，
 *这将传递给通知信号。
 **/

enum class MemPoolRemovalReason {
UNKNOWN = 0, //！<手动删除或未知原因
EXPIRY,      //！<从mempool过期
SIZELIMIT,   //！<尺寸限制移除
REORG,       //！<为重组而移除
BLOCK,       //！<为块移除
CONFLICT,    //！<因与块内事务冲突而删除
REPLACED,    //！<移除进行更换
};

class SaltedTxidHasher
{
private:
    /*盐*/
    const uint64_t k0, k1;

public:
    SaltedTxidHasher();

    size_t operator()(const uint256& txid) const {
        return SipHashUint256(k0, k1, txid);
    }
};

/*
 *CTxEmpool存储区根据当前最佳链事务有效
 *可能包含在下一个块中。
 *
 *当在网络上看到事务（或由
 *本地节点），但并非所有看到的事务都添加到池中。为了
 *例如，以下新事务将不会添加到mempool中：
 *不符合最低费用要求的交易。
 *—将已存在的事务的输入加倍花费的新事务
 *新交易不满足替换费用的池
 *要求见BIP 125。
 *非标准交易。
 *
 *ctxmempool:：maptx和ctxmempoolEntry簿记：
 *
 *maptx是一个boost：：multi_索引，它按照4个标准对mempool进行排序：
 *-事务哈希
 *-后代feerate[我们使用max（feerate of tx，feerate of tx连同所有后代）]
 *在mempool中的时间
 *祖先feerate[我们使用min（Tx的feerate，Tx的feerate与所有未确认的祖先一起使用）]
 *
 *注：术语“后代”是指依赖于
 *这一个，而“祖先”是指在mempool事务中
 *交易取决于。
 *
 *为了保持正确的排序，我们必须更新交易
 *当新的后代到达时在mempool中。为了促进这一点，我们跟踪
 *maplinks中的in mempool直系父代和直系子代集合。内
 *每个CTxEmpoolEntry，我们跟踪所有后代的大小和费用。
 *
 *通常在将新事务添加到mempool时，mempool中没有。
 *孩子（因为任何这样的孩子都是孤儿）。所以在
 *addunchecked（），我们：
 *-更新一个新条目的setmempoolparents以包含mempool父级中的所有内容
 *-更新新条目的直接父级以将新tx包含为子级
 *-更新事务的所有祖先，以包括新的Tx的大小/费用
 *
 *从内存池中删除事务时，我们必须：
 *-更新mempool中的所有父级，以不跟踪setmempoolchildren中的tx
 *-更新所有祖先，使其在后代状态下不包括Tx的大小/费用。
 *-更新mempool中的所有子级，使其不作为父级包含
 *
 *这些发生在updateforremovefrommempool（）中。（注意，当拆卸
 *事务及其子代，我们必须计算
 *删除前要删除的事务，否则mempool可以
 *处于一种不一致的状态，不可能与
 *一笔交易。）
 *
 *如果是REORG，假设新增加的Tx没有
 *在mempool中，孩子是假的。特别是，mempool在
 *添加新事务时状态不一致，因为
 *来自断开连接块的Tx的后代事务
 *仅查看内存池中的事务就无法访问（链接
 *事务也可能在断开连接的块中，等待添加）。
 *正因为如此，在Mempool中搜索没有多大好处。
 *addunchecked（）中的子级。相反，在交易的特殊情况下
 *从断开连接的块添加时，我们要求调用者清除
 *在mempool中说明，所有
 *通过调用updateTransactionsFromBlock（）在块事务中。注意
 *在调用之前，mempool状态不一致，尤其是
 *地图链接可能不正确（因此功能如下
 *CalculateMempoolancestors（）和CalculateDescendants（）依赖
 *在他们身上行走的mempool一般不安全。
 *
 *计算极限：
 *
 *更新新添加的事务的所有内存池祖先可能会很慢，
 *如果mempool中的祖先数量没有限制。
 *calculateMemPooolancestors（）采用可配置的限制，其设计目的是
 *防止这些计算过于CPU密集。
 *
 **/

class CTxMemPool
{
private:
uint32_t nCheckFrequency GUARDED_BY(cs); //！<n值表示在2^32中，我们检查n次。
unsigned int nTransactionsUpdated; //！<由GetBlockTemplate用于触发CreateNewBlock（）调用
    CBlockPolicyEstimator* minerPolicyEstimator;

uint64_t totalTxSize;      //！<所有Mempool Tx虚拟大小的总和。与序列化的Tx大小不同，因为见证数据已折扣。在BIP 141中定义。
uint64_t cachedInnerUsage; //！<所有映射元素（而不是映射本身）的动态内存使用量之和

    mutable int64_t lastRollingFeeUpdate;
    mutable bool blockSinceLastRollingFeeBump;
mutable double rollingMinimumFeeRate; //！<进入游泳池的最低费用，按指数递减

    void trackPackageRemoved(const CFeeRate& rate) EXCLUSIVE_LOCKS_REQUIRED(cs);

public:

static const int ROLLING_FEE_HALFLIFE = 60 * 60 * 12; //仅用于测试的公共

    typedef boost::multi_index_container<
        CTxMemPoolEntry,
        boost::multi_index::indexed_by<
//用TXID排序
            boost::multi_index::hashed_unique<mempoolentry_txid, SaltedTxidHasher>,
//按费率排序
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<descendant_score>,
                boost::multi_index::identity<CTxMemPoolEntry>,
                CompareTxMemPoolEntryByDescendantScore
            >,
//按进入时间排序
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<entry_time>,
                boost::multi_index::identity<CTxMemPoolEntry>,
                CompareTxMemPoolEntryByEntryTime
            >,
//按与祖先的费率排序
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<ancestor_score>,
                boost::multi_index::identity<CTxMemPoolEntry>,
                CompareTxMemPoolEntryByAncestorFee
            >
        >
    > indexed_transaction_set;

    /*
     *访问“maptx”或其他成员时需要锁定此互斥体。
     *由它保护。
     *
     *票面一致性保证
     *
     *设计保证：
     *
     * 1。同时锁定'cs_main'和'mempool.cs'将提供mempool的视图
     *与当前链尖一致（`chainactive`和
     *`pcoinstip`），并且完全填充。完全填充意味着如果
     *当前活动链缺少
     *以前的活动链，所有丢失的事务将
     *重新添加到mempool中，如果符合大小和
     *一致性约束。
     *
     * 2。锁定“mempool.cs”（不带“cs_main”）将提供mempool的视图
     *与上一次使用'cs_main'后的活动链一致。
     *已锁定，并按上述方式完全填充。这是可以的
     *只需要查询或从mempool中删除事务的代码
     *只锁定'mempool.cs'而不锁定'cs_main'。
     *
     *为提供这些保证，必须同时锁定'cs_main'和
     *`mempool.cs`每当向mempool添加事务时以及每当
     *更换链尖。必须将两个互斥锁都锁定到
     *mempool与新的链端一致，并且完全填充。
     *
     *@par一致性错误
     *
     *上述第二项担保目前尚未执行，但
     *https://github.com/bitcoin/bitcoin/pull/14193将修复此问题。无已知代码
     *比特币目前取决于第二担保，但重要的是
     *修复需要经常轮询的第三方代码
     *没有锁定的mempool'cs_main'并且没有遇到丢失的情况
     *重组期间的交易。
     **/

    mutable RecursiveMutex cs;
    indexed_transaction_set mapTx GUARDED_BY(cs);

    using txiter = indexed_transaction_set::nth_index<0>::type::const_iterator;
std::vector<std::pair<uint256, txiter> > vTxHashes; //！<maptx中的所有tx见证散列/条目，随机顺序

    struct CompareIteratorByHash {
        bool operator()(const txiter &a, const txiter &b) const {
            return a->GetTx().GetHash() < b->GetTx().GetHash();
        }
    };
    typedef std::set<txiter, CompareIteratorByHash> setEntries;

    const setEntries & GetMemPoolParents(txiter entry) const EXCLUSIVE_LOCKS_REQUIRED(cs);
    const setEntries & GetMemPoolChildren(txiter entry) const EXCLUSIVE_LOCKS_REQUIRED(cs);
    uint64_t CalculateDescendantMaximum(txiter entry) const EXCLUSIVE_LOCKS_REQUIRED(cs);
private:
    typedef std::map<txiter, setEntries, CompareIteratorByHash> cacheMap;

    struct TxLinks {
        setEntries parents;
        setEntries children;
    };

    typedef std::map<txiter, TxLinks, CompareIteratorByHash> txlinksMap;
    txlinksMap mapLinks;

    void UpdateParent(txiter entry, txiter parent, bool add);
    void UpdateChild(txiter entry, txiter child, bool add);

    std::vector<indexed_transaction_set::const_iterator> GetSortedDepthAndScore() const EXCLUSIVE_LOCKS_REQUIRED(cs);

public:
    indirectmap<COutPoint, const CTransaction*> mapNextTx GUARDED_BY(cs);
    std::map<uint256, CAmount> mapDeltas;

    /*创建新的CTX模板。
     **/

    explicit CTxMemPool(CBlockPolicyEstimator* estimator = nullptr);

    /*
     *如果已打开健全性检查，请检查确保池
     *一致性（不包含使用相同输入的两个事务，
     *所有输入都在mapnextx数组中）。如果关闭健全性检查，
     *支票不起作用。
     **/

    void check(const CCoinsViewCache *pcoins) const;
    void setSanityCheck(double dFrequency = 1.0) { LOCK(cs); nCheckFrequency = static_cast<uint32_t>(dFrequency * 4294967295.0); }

//addunchecked必须为给定事务的所有祖先更新状态，
//跟踪子事务的大小/计数。第一版
//addunchecked可用于让它调用calculateMemPooolancestors（），以及
//然后调用第二个版本。
//注意，addunchecked只从测试之外的atmp调用
//任何其他呼叫者都可能在Mempool追踪中破坏钱包（由于
//缺少cvalidationInterface:：TransactionalAddedTompool回调）。
    void addUnchecked(const CTxMemPoolEntry& entry, bool validFeeEstimate = true) EXCLUSIVE_LOCKS_REQUIRED(cs, cs_main);
    void addUnchecked(const CTxMemPoolEntry& entry, setEntries& setAncestors, bool validFeeEstimate = true) EXCLUSIVE_LOCKS_REQUIRED(cs, cs_main);

    void removeRecursive(const CTransaction &tx, MemPoolRemovalReason reason = MemPoolRemovalReason::UNKNOWN);
    void removeForReorg(const CCoinsViewCache *pcoins, unsigned int nMemPoolHeight, int flags) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    void removeConflicts(const CTransaction &tx) EXCLUSIVE_LOCKS_REQUIRED(cs);
    void removeForBlock(const std::vector<CTransactionRef>& vtx, unsigned int nBlockHeight);

    void clear();
void _clear() EXCLUSIVE_LOCKS_REQUIRED(cs); //无锁
    bool CompareDepthAndScore(const uint256& hasha, const uint256& hashb);
    void queryHashes(std::vector<uint256>& vtxid);
    bool isSpent(const COutPoint& outpoint) const;
    unsigned int GetTransactionsUpdated() const;
    void AddTransactionsUpdated(unsigned int n);
    /*
     *检查所有这些事务输入都不在mempool中，因此
     *Tx不依赖于块中包含的其他mempool事务。
     **/

    bool HasNoInputsOf(const CTransaction& tx) const;

    /*影响事务的CreateNewBlock优先级*/
    void PrioritiseTransaction(const uint256& hash, const CAmount& nFeeDelta);
    void ApplyDelta(const uint256 hash, CAmount &nFeeDelta) const;
    void ClearPrioritisation(const uint256 hash);

    /*获取池中使用相同prevent的事务*/
    const CTransaction* GetConflictTx(const COutPoint& prevout) const EXCLUSIVE_LOCKS_REQUIRED(cs);

    /*返回给定哈希的迭代器（如果找到）*/
    boost::optional<txiter> GetIter(const uint256& txid) const EXCLUSIVE_LOCKS_REQUIRED(cs);

    /*将一组哈希转换为一组池迭代器以避免重复查找*/
    setEntries GetIterSet(const std::set<uint256>& hashes) const EXCLUSIVE_LOCKS_REQUIRED(cs);

    /*从内存池中删除一组事务。
     *如果事务在此集合中，则所有in mempool子体必须
     *也在集合中，除非此事务因
     *在一个街区。
     *删除块中的Tx时，将UpdateScendants设置为true，因此
     *在mempool中的任何后代都更新了其祖先状态。
     **/

    void RemoveStaged(setEntries &stage, bool updateDescendants, MemPoolRemovalReason reason = MemPoolRemovalReason::UNKNOWN) EXCLUSIVE_LOCKS_REQUIRED(cs);

    /*将断开连接的块中的事务添加回mempool时，
     *新的mempool条目可能在mempool中有子项（通常是
     *否则添加事务时则不是这样）。
     *updateTransactionsFromBlock（）将查找子事务并更新
     *vhashestopdate中每个事务的后代状态（不包括任何
     *vHashToupDate中存在的子交易，这些交易已经入账。
     *）注意：vhashestopdate应该是
     *断开连接的块已被接收回内存池。
     **/

    void UpdateTransactionsFromBlock(const std::vector<uint256>& vHashesToUpdate) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    /*尝试计算所有在mempool中的条目祖先。
     （这些都是计算出来的，包括Tx本身）
     *limitancestorcount=最大祖先数
     *limitancestorsize=祖先的最大大小
     *limitDescendantCount=任何祖先可以拥有的后代的最大数量
     *limitDescendantSize=任何祖先可以拥有的后代的最大大小
     *errstring=如果达到任何限制，则填充错误原因
     *fsearchforparents=是否在mempool家长中搜索Tx的VIN，或
     *从MapLinks查找家长。对于不在mempool中的条目必须为true
     **/

    bool CalculateMemPoolAncestors(const CTxMemPoolEntry& entry, setEntries& setAncestors, uint64_t limitAncestorCount, uint64_t limitAncestorSize, uint64_t limitDescendantCount, uint64_t limitDescendantSize, std::string& errString, bool fSearchForParents = true) const EXCLUSIVE_LOCKS_REQUIRED(cs);

    /*用哈希的所有内存池后代填充setDescendants。
     *假设setDescendants包含所有mempool中任何内容的后代。
     *已经在里面了。*/

    void CalculateDescendants(txiter it, setEntries& setDescendants) const EXCLUSIVE_LOCKS_REQUIRED(cs);

    /*进入mempool的最低费用，这本身可能不够。
      *对于较大规模的交易。
      *incrementalRelayFee策略变量用于绑定时间。
      *收费率一直下降到0。当这个女人
      *否则将是这个的一半，而是设置为0。
      **/

    CFeeRate GetMinFee(size_t sizelimit) const;

    /*从mempool中删除事务，直到其动态大小<=sizelimit。
      *pvnoPendsRemaining（如果设置）将用输出点列表填充。
      *不在Mempool中，不再在Mempool中花费。
      **/

    void TrimToSize(size_t sizelimit, std::vector<COutPoint>* pvNoSpendsRemaining=nullptr);

    /*使内存池中的所有事务（及其依赖项）过期。返回已删除的事务数。*/
    int Expire(int64_t time);

    /*
     *计算给定事务的祖先和后代计数。
     *计数包括交易本身。
     **/

    void GetTransactionAncestry(const uint256& txid, size_t& ancestors, size_t& descendants) const;

    unsigned long size() const
    {
        LOCK(cs);
        return mapTx.size();
    }

    uint64_t GetTotalTxSize() const
    {
        LOCK(cs);
        return totalTxSize;
    }

    bool exists(const uint256& hash) const
    {
        LOCK(cs);
        return (mapTx.count(hash) != 0);
    }

    CTransactionRef get(const uint256& hash) const;
    TxMempoolInfo info(const uint256& hash) const;
    std::vector<TxMempoolInfo> infoAll() const;

    size_t DynamicMemoryUsage() const;

    boost::signals2::signal<void (CTransactionRef)> NotifyEntryAdded;
    boost::signals2::signal<void (CTransactionRef, MemPoolRemovalReason)> NotifyEntryRemoved;

private:
    /*UpdateForDescendants由UpdateTransactionsFromBlock使用以更新
     *已添加到
     *mempool，但可能在mempool中有子事务，例如
     *链条重排。setExclude是
     *必须不考虑的内存池（因为
     *在事务被
     *已更新，因此其状态已反映在父级中。
     *状态）。
     *
     *将使用事务的后代更新cachedDescendants
     *正在更新，以便将来的调用不需要
     *如果在另一个事务链中遇到相同的事务，则再次执行相同的事务。
     **/

    void UpdateForDescendants(txiter updateIt,
            cacheMap &cachedDescendants,
            const std::set<uint256> &setExclude) EXCLUSIVE_LOCKS_REQUIRED(cs);
    /*更新哈希的祖先以将其作为子事务添加/删除。*/
    void UpdateAncestorsOf(bool add, txiter hash, setEntries &setAncestors) EXCLUSIVE_LOCKS_REQUIRED(cs);
    /*设置条目的祖先状态*/
    void UpdateEntryForAncestors(txiter it, const setEntries &setAncestors) EXCLUSIVE_LOCKS_REQUIRED(cs);
    /*对于要删除的每个事务，更新祖先和任何直接子级。
      *如果updateScendants为true，则在mempool-cendants中也更新
      *祖先状态。*/

    void UpdateForRemoveFromMempool(const setEntries &entriesToRemove, bool updateDescendants) EXCLUSIVE_LOCKS_REQUIRED(cs);
    /*在指定的事务和直接子级之间切断链接。*/
    void UpdateChildrenForRemoval(txiter entry) EXCLUSIVE_LOCKS_REQUIRED(cs);

    /*在为给定事务调用removeunchecked之前，
     *必须对整个（依赖）集调用updateForRemoveFromMemPool
     *同时删除的事务数。我们使用每个
     *ctxmempoolEntry的setmempoolParents用于行走
     *给定的事务被删除，因此我们不能删除中间
     *在我们更新了
     ＊去除。
     **/

    void removeUnchecked(txiter entry, MemPoolRemovalReason reason = MemPoolRemovalReason::UNKNOWN) EXCLUSIVE_LOCKS_REQUIRED(cs);
};

/*
 *将mempool中的事务引入视图的ccoinsview。
 *不按内存池事务检查开销。
 *相反，它提供了所有未使用的硬币。
 *基本CCoinsView，或是任何mempool事务的输出！
 *这允许事务替换按预期工作，正如您希望的那样。
 *所有输入“可用”以检查签名，以及
 *依赖关系图直接在AcceptToMoryPool中检查。
 *它还允许您直接在Signrawtransaction中签署双倍费用，
 *只要冲突交易尚未确认。
 **/

class CCoinsViewMemPool : public CCoinsViewBacked
{
protected:
    const CTxMemPool& mempool;

public:
    CCoinsViewMemPool(CCoinsView* baseIn, const CTxMemPool& mempoolIn);
    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
};

/*
 *断开连接的BlockTransactions

 *在REORG期间，需要重新添加以前确认的交易。
 *到mempool，以便新链中未重新确认的任何内容
 *可开采。但是，等到REORG
 *完成并处理当时所有尚未确认的交易，
 *由于我们预计大多数已确认的交易（通常）仍然
 *在新的链中确认，重新接受内存池很昂贵
 （因此最好不要在REORG处理过程中进行）。
 *相反，存储断开连接的事务（按顺序！）当我们走的时候，把任何
 *包含在新链中的块中，然后处理其余的
 *最后仍有未确认的交易。
 **/


//多索引标记名
struct txid_index {};
struct insertion_order {};

struct DisconnectedBlockTransactions {
    typedef boost::multi_index_container<
        CTransactionRef,
        boost::multi_index::indexed_by<
//用TXID排序
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<txid_index>,
                mempoolentry_txid,
                SaltedTxidHasher
            >,
//按区块链中的顺序排序
            boost::multi_index::sequenced<
                boost::multi_index::tag<insertion_order>
            >
        >
    > indexed_disconnected_transactions;

//如果我们以前不清除queuedtx，这几乎肯定是一个逻辑错误。
//破坏，就像我们在断开块的时候添加的那样，然后我们
//需要重新处理剩余事务以确保内存池的一致性。
//现在，断言（）我们已经在销毁时清空了这个对象。
//如果REORG处理代码是
//重新构造，使此假设不再正确（因为
//例如，如果有其他方法，我们在
//REORG，除了排出这个对象）。
    ~DisconnectedBlockTransactions() { assert(queuedTx.empty()); }

    indexed_disconnected_transactions queuedTx;
    uint64_t cachedInnerUsage = 0;

//估计queuedtx的开销为6个指针+一个分配，如
//没有实现Boost：：multi_index_的精确公式。
    size_t DynamicMemoryUsage() const {
        return memusage::MallocUsage(sizeof(CTransactionRef) + 6 * sizeof(void*)) * queuedTx.size() + cachedInnerUsage;
    }

    void addTransaction(const CTransactionRef& tx)
    {
        queuedTx.insert(tx);
        cachedInnerUsage += RecursiveDynamicUsage(tx);
    }

//删除基于txid_索引的条目，并更新内存使用情况。
    void removeForBlock(const std::vector<CTransactionRef>& vtx)
    {
//添加到尖端的块的常见情况下发生短路
        if (queuedTx.empty()) {
            return;
        }
        for (auto const &tx : vtx) {
            auto it = queuedTx.find(tx->GetHash());
            if (it != queuedTx.end()) {
                cachedInnerUsage -= RecursiveDynamicUsage(*it);
                queuedTx.erase(it);
            }
        }
    }

//通过插入顺序索引删除条目，并更新内存使用情况。
    void removeEntry(indexed_disconnected_transactions::index<insertion_order>::type::iterator entry)
    {
        cachedInnerUsage -= RecursiveDynamicUsage(*entry);
        queuedTx.get<insertion_order>().erase(entry);
    }

    void clear()
    {
        cachedInnerUsage = 0;
        queuedTx.clear();
    }
};

#endif //比特币
