
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

#ifndef BITCOIN_MINER_H
#define BITCOIN_MINER_H

#include <primitives/block.h>
#include <txmempool.h>
#include <validation.h>

#include <stdint.h>
#include <memory>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>

class CBlockIndex;
class CChainParams;
class CScript;

namespace Consensus { struct Params; };

static const bool DEFAULT_PRINTPRIORITY = false;

struct CBlockTemplate
{
    CBlock block;
    std::vector<CAmount> vTxFees;
    std::vector<int64_t> vTxSigOpsCost;
    std::vector<unsigned char> vchCoinbaseCommitment;
};

//用于跟踪祖先更新的容器，如我们所包含的（父级）
//块中的事务
struct CTxMemPoolModifiedEntry {
    explicit CTxMemPoolModifiedEntry(CTxMemPool::txiter entry)
    {
        iter = entry;
        nSizeWithAncestors = entry->GetSizeWithAncestors();
        nModFeesWithAncestors = entry->GetModFeesWithAncestors();
        nSigOpCostWithAncestors = entry->GetSigOpCostWithAncestors();
    }

    int64_t GetModifiedFee() const { return iter->GetModifiedFee(); }
    uint64_t GetSizeWithAncestors() const { return nSizeWithAncestors; }
    CAmount GetModFeesWithAncestors() const { return nModFeesWithAncestors; }
    size_t GetTxSize() const { return iter->GetTxSize(); }
    const CTransaction& GetTx() const { return iter->GetTx(); }

    CTxMemPool::txiter iter;
    uint64_t nSizeWithAncestors;
    CAmount nModFeesWithAncestors;
    int64_t nSigOpCostWithAncestors;
};

/*ctxmempool:：txter对象的比较器。
 *它只是比较ctxmempoolEntry对象的内部内存地址。
 *指出。这意味着它没有意义，只对使用它们有用
 *作为其他索引的键。
 **/

struct CompareCTxMemPoolIter {
    bool operator()(const CTxMemPool::txiter& a, const CTxMemPool::txiter& b) const
    {
        return &(*a) < &(*b);
    }
};

struct modifiedentry_iter {
    typedef CTxMemPool::txiter result_type;
    result_type operator() (const CTxMemPoolModifiedEntry &entry) const
    {
        return entry.iter;
    }
};

//一种基于祖先数量对事务进行排序的比较器。
//这足以按有效的顺序对祖先包进行排序。
//出现在一个块中。
struct CompareTxIterByAncestorCount {
    bool operator()(const CTxMemPool::txiter &a, const CTxMemPool::txiter &b) const
    {
        if (a->GetCountWithAncestors() != b->GetCountWithAncestors())
            return a->GetCountWithAncestors() < b->GetCountWithAncestors();
        return CTxMemPool::CompareIteratorByHash()(a, b);
    }
};

typedef boost::multi_index_container<
    CTxMemPoolModifiedEntry,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<
            modifiedentry_iter,
            CompareCTxMemPoolIter
        >,
//按修改后的上级费率排序
        boost::multi_index::ordered_non_unique<
//重用CTxEmpool相似索引中的相同标记
            boost::multi_index::tag<ancestor_score>,
            boost::multi_index::identity<CTxMemPoolModifiedEntry>,
            CompareTxMemPoolEntryByAncestorFee
        >
    >
> indexed_modified_transaction_set;

typedef indexed_modified_transaction_set::nth_index<0>::type::iterator modtxiter;
typedef indexed_modified_transaction_set::index<ancestor_score>::type::iterator modtxscoreiter;

struct update_for_parent_inclusion
{
    explicit update_for_parent_inclusion(CTxMemPool::txiter it) : iter(it) {}

    void operator() (CTxMemPoolModifiedEntry &e)
    {
        e.nModFeesWithAncestors -= iter->GetFee();
        e.nSizeWithAncestors -= iter->GetTxSize();
        e.nSigOpCostWithAncestors -= iter->GetSigOpCost();
    }

    CTxMemPool::txiter iter;
};

/*生成新块，但没有有效的工作证明*/
class BlockAssembler
{
private:
//构建的块模板
    std::unique_ptr<CBlockTemplate> pblocktemplate;
//总是引用pblocktemplate中的cblock的便利指针
    CBlock* pblock;

//块大小的配置参数
    bool fIncludeWitness;
    unsigned int nBlockMaxWeight;
    CFeeRate blockMinFeeRate;

//块的当前状态信息
    uint64_t nBlockWeight;
    uint64_t nBlockTx;
    uint64_t nBlockSigOpsCost;
    CAmount nFees;
    CTxMemPool::setEntries inBlock;

//块的链上下文
    int nHeight;
    int64_t nLockTimeCutoff;
    const CChainParams& chainparams;

public:
    struct Options {
        Options();
        size_t nBlockMaxWeight;
        CFeeRate blockMinFeeRate;
    };

    explicit BlockAssembler(const CChainParams& params);
    BlockAssembler(const CChainParams& params, const Options& options);

    /*用CoinBase构造一个新的块模板到ScriptPubKeyin*/
    std::unique_ptr<CBlockTemplate> CreateNewBlock(const CScript& scriptPubKeyIn);

private:
//效用函数
    /*清除块的状态并准备组装新块*/
    void resetBlock();
    /*将Tx添加到块*/
    void AddToBlock(CTxMemPool::txiter iter);

//用于将事务添加到块的方法。
    /*添加基于feerate的事务，包括未确认的祖先
      *增加已选择的/未选择的包，加上相应的
      *来自包选择的统计信息（用于记录统计信息）。*/

    void addPackageTxs(int &nPackagesSelected, int &nDescendantsUpdated) EXCLUSIVE_LOCKS_REQUIRED(mempool.cs);

//addPackageTxs（）的帮助程序函数
    /*从给定集合中删除已确认（inblock）项*/
    void onlyUnconfirmed(CTxMemPool::setEntries& testSet);
    /*测试新包是否“适合”块中*/
    bool TestPackage(uint64_t packageSize, int64_t packageSigOpsCost) const;
    /*对包中的每个事务执行检查：
      *锁定时间、过早见证、序列化大小（如有必要）
      *这些检查应该总是成功的，它们就在这里。
      *仅在节点配置不理想时作为额外检查*/

    bool TestPackageTransactions(const CTxMemPool::setEntries& package);
    /*如果已经评估了来自MAPTX的给定事务，则返回true，
      *或者如果事务在MAPTX中的缓存数据不正确。*/

    bool SkipMapTxEntry(CTxMemPool::txiter it, indexed_modified_transaction_set &mapModifiedTx, CTxMemPool::setEntries &failedTx) EXCLUSIVE_LOCKS_REQUIRED(mempool.cs);
    /*按块中显示的有效顺序对包进行排序*/
    void SortForBlock(const CTxMemPool::setEntries& package, std::vector<CTxMemPool::txiter>& sortedEntries);
    /*将给定事务的后代添加到具有祖先的mapmodifiedtx
      *假设给定的事务是inblock，则状态更新。返回数
      *更新的后代。*/

    int UpdatePackagesForAdded(const CTxMemPool::setEntries& alreadyAdded, indexed_modified_transaction_set &mapModifiedTx) EXCLUSIVE_LOCKS_REQUIRED(mempool.cs);
};

/*在块中修改Extrance*/
void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce);
int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev);

#endif //比特科尼矿
