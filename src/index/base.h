
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2017-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_INDEX_BASE_H
#define BITCOIN_INDEX_BASE_H

#include <dbwrapper.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <threadinterrupt.h>
#include <uint256.h>
#include <validationinterface.h>

class CBlockIndex;

/*
 *区块链数据索引的基类。这个工具
 *cvalidationInterface并确保块按顺序索引
 *到它们在活动链中的位置。
 **/

class BaseIndex : public CValidationInterface
{
protected:
    class DB : public CDBWrapper
    {
    public:
        DB(const fs::path& path, size_t n_cache_size,
           bool f_memory = false, bool f_wipe = false, bool f_obfuscate = false);

///read与txindex同步的链的块定位器。
        bool ReadBestBlock(CBlockLocator& locator) const;

///write与txindex同步的链的块定位器。
        bool WriteBestBlock(const CBlockLocator& locator);
    };

private:
///索引是否与主链同步。旗子翻了
///从假到真一次，之后开始处理
///validationinterface通知保持同步。
    std::atomic<bool> m_synced{false};

///索引与之同步的链中的最后一个块。
    std::atomic<const CBlockIndex*> m_best_block_index{nullptr};

    std::thread m_thread_sync;
    CThreadInterrupt m_interrupt;

///将索引与从当前最佳块开始的块索引同步。
///打算在自己的线程m_thread_sync中运行，并且可以
///被M_中断中断。一旦索引同步，M_同步
///flag被设置，blockconnected validationInterface回调接受
///over，同步线程退出。
    void ThreadSync();

///将当前链块定位器写入数据库。
    bool WriteBestBlock(const CBlockIndex* block_index);

protected:
    void BlockConnected(const std::shared_ptr<const CBlock>& block, const CBlockIndex* pindex,
                        const std::vector<CTransactionRef>& txn_conflicted) override;

    void ChainStateFlushed(const CBlockLocator& locator) override;

///从数据库和块索引初始化内部状态。
    virtual bool Init();

///write为新连接的块更新索引项。
    virtual bool WriteBlock(const CBlock& block, const CBlockIndex* pindex) { return true; }

    virtual DB& GetDB() const = 0;

///get要在日志中显示的索引的名称。
    virtual const char* GetName() const = 0;

public:
///destructor中断同步线程（如果正在运行），并阻止它，直到它退出。
    virtual ~BaseIndex();

///阻止当前线程，直到索引被捕获到当前线程为止。
///区块链的状态。这只会在索引已进入时阻止
///sync一次，只需要处理validationinterface中的块。
//排队。如果索引远远落后，则此方法可以
///not阻塞并立即返回false。
    bool BlockUntilSyncedToCurrentChain();

    void Interrupt();

///start初始化同步状态并将实例注册为
///validationinterface使其与区块链更新保持同步。
    void Start();

///阻止实例与区块链更新保持同步。
    void Stop();
};

#endif //比特币指数
