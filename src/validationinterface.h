
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

#ifndef BITCOIN_VALIDATIONINTERFACE_H
#define BITCOIN_VALIDATIONINTERFACE_H

#include <primitives/transaction.h> //ctransaction（参考）
#include <sync.h>

#include <functional>
#include <memory>

extern CCriticalSection cs_main;
class CBlock;
class CBlockIndex;
struct CBlockLocator;
class CBlockIndex;
class CConnman;
class CReserveScript;
class CValidationInterface;
class CValidationState;
class uint256;
class CScheduler;
class CTxMemPool;
enum class MemPoolRemovalReason;

//这些功能发送到一个或所有已注册的钱包

/*注册钱包以接收核心更新*/
void RegisterValidationInterface(CValidationInterface* pwalletIn);
/*从核心注销钱包*/
void UnregisterValidationInterface(CValidationInterface* pwalletIn);
/*从核心注销所有钱包*/
void UnregisterAllValidationInterfaces();
/*
 *将回调函数推送到通知队列，确保
 *调用函数时，将完成以前生成的回调。
 *
 *如果持有任何锁，请小心阻止要调用的func-
 *验证接口客户端可能无法像通常那样取得进展。
 *等待像cs_main这样的东西，所以阻塞直到用cs_main调用func
 *将导致死锁（调试锁定顺序将丢失）。
 **/

void CallFunctionInValidationInterfaceQueue(std::function<void ()> func);
/*
 *这是以下的同义词，它断言某些锁不是
 ＊持有：
 *std:：promise<void>promise；
 *调用函数验证InterfaceQueue（[&Promise]
 *promise.set_value（）；
 *}；
 *promise.get_future（）.wait（）；
 **/

void SyncWithValidationInterfaceQueue() LOCKS_EXCLUDED(cs_main);

/*
 *实现此功能以订阅验证中生成的事件
 *
 *每个cvalidationInterface（）订阅服务器将收到事件回调
 *按验证生成事件的顺序。
 *此外，每个validationInterface（）订阅服务器可能会假定
 *回调在单线程和单线程中有效运行
 *内存一致性。也就是说，对于给定的validationInterface（）。
 *实例化，每个回调都将在下一个回调之前完成
 *调用。这意味着，例如，当一个块连接时，
 *updatedBlockTip（）回调可能取决于在中执行的操作。
 *blockConnected（）回调，不用担心显式
 *同步。不应假定整个
 *validationInterface（）订阅服务器。
 **/

class CValidationInterface {
protected:
    /*
     *受保护的析构函数，因此实例只能由派生类删除。
     *如果不再需要该限制，则应将其公开并虚拟化。
     **/

    ~CValidationInterface() = default;
    /*
     *当区块链提示前进时通知侦听器。
     *
     *当同时连接多个块时，将在最后一个提示上调用updatedBlockTip。
     *但不能在每个中间提示上调用。如果需要后一种行为，
     *改为订阅blockConnected（）。
     *
     *在后台线程上调用。
     **/

    virtual void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) {}
    /*
     *通知侦听器已将事务添加到mempool。
     *
     *在后台线程上调用。
     **/

    virtual void TransactionAddedToMempool(const CTransactionRef &ptxn) {}
    /*
     *通知侦听器某个事务离开mempool。
     *
     *这仅适用于因到期而离开mempool的事务，
     *尺寸限制、REORG（锁定时间/CoinBase成熟度的变化）或
     *替换。这不包括任何包含的交易
     *在BlockConnectedDisconnected中，可在Block->VTX或TxConnected中断开。
     *
     *在后台线程上调用。
     **/

    virtual void TransactionRemovedFromMempool(const CTransactionRef &ptx) {}
    /*
     *通知侦听器正在连接的块。
     *提供从内存池中收回的事务向量。
     *
     *在后台线程上调用。
     **/

    virtual void BlockConnected(const std::shared_ptr<const CBlock> &block, const CBlockIndex *pindex, const std::vector<CTransactionRef> &txnConflicted) {}
    /*
     *通知侦听器某个块正在断开连接
     *
     *在后台线程上调用。
     **/

    virtual void BlockDisconnected(const std::shared_ptr<const CBlock> &block) {}
    /*
     *通知侦听器磁盘上新的活动区块链。
     *
     *在此回调之前，任何更新都不能保证在磁盘上保持不变。
     *（IE客户端需要通过能够
     *了解由于不干净的关机导致某些更新丢失的时间）。
     *
     *当调用此回调时，任何
     *回调保证存在于磁盘上并在重新启动后继续存在，包括
     *关闭不干净。
     *
     *提供描述最佳链的定位器，可能对
     *将当前状态存储在客户机DBS的磁盘上。
     *
     *在后台线程上调用。
     **/

    virtual void ChainStateFlushed(const CBlockLocator &locator) {}
    /*告诉听众广播他们的数据。*/
    virtual void ResendWalletTransactions(int64_t nBestBlockTime, CConnman* connman) {}
    /*
     *通知侦听器块验证结果。
     *如果提供的cvalidationState有效，则提供的块
     *保证是当前最好的块
     *已生成回调（不一定是现在）
     **/

    virtual void BlockChecked(const CBlock&, const CValidationState&) {}
    /*
     *通知侦听器直接基于当前提示构建的块
     *已接收并连接到标题树，但尚未验证*/

    virtual void NewPoWValidBlock(const CBlockIndex *pindex, const std::shared_ptr<const CBlock>& block) {};
    friend void ::RegisterValidationInterface(CValidationInterface*);
    friend void ::UnregisterValidationInterface(CValidationInterface*);
    friend void ::UnregisterAllValidationInterfaces();
};

struct MainSignalsInstance;
class CMainSignals {
private:
    std::unique_ptr<MainSignalsInstance> m_internals;

    friend void ::RegisterValidationInterface(CValidationInterface*);
    friend void ::UnregisterValidationInterface(CValidationInterface*);
    friend void ::UnregisterAllValidationInterfaces();
    friend void ::CallFunctionInValidationInterfaceQueue(std::function<void ()> func);

    void MempoolEntryRemoved(CTransactionRef tx, MemPoolRemovalReason reason);

public:
    /*注册一个CScheduler以给出应在后台运行的回调（只能调用一次）*/
    void RegisterBackgroundSignalScheduler(CScheduler& scheduler);
    /*注销CScheduler以给出应在后台运行的回调-这些回调现在将被删除！*/
    void UnregisterBackgroundSignalScheduler();
    /*调用调用线程上的任何剩余回调*/
    void FlushBackgroundCallbacks();

    size_t CallbacksPending();

    /*注册到mempool以调用transactionremovedFrommempool回调*/
    void RegisterWithMempoolSignals(CTxMemPool& pool);
    /*注销mempool*/
    void UnregisterWithMempoolSignals(CTxMemPool& pool);

    void UpdatedBlockTip(const CBlockIndex *, const CBlockIndex *, bool fInitialDownload);
    void TransactionAddedToMempool(const CTransactionRef &);
    void BlockConnected(const std::shared_ptr<const CBlock> &, const CBlockIndex *pindex, const std::shared_ptr<const std::vector<CTransactionRef>> &);
    void BlockDisconnected(const std::shared_ptr<const CBlock> &);
    void ChainStateFlushed(const CBlockLocator &);
    void Broadcast(int64_t nBestBlockTime, CConnman* connman);
    void BlockChecked(const CBlock&, const CValidationState&);
    void NewPoWValidBlock(const CBlockIndex *, const std::shared_ptr<const CBlock>&);
};

CMainSignals& GetMainSignals();

#endif //比特币验证界面
