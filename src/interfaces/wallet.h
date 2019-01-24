
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_INTERFACES_WALLET_H
#define BITCOIN_INTERFACES_WALLET_H

#include <amount.h>                    //为金钱
#include <pubkey.h>                    //对于ckeyid和cscriptid（CTxDestination实例化中需要的定义）
#include <script/ismine.h>             //对于isminefilter，isminetype
#include <script/standard.h>           //用于CTX测试
#include <support/allocators/secure.h> //对于SecureString
#include <ui_interface.h>              //为了ChangeType

#include <functional>
#include <map>
#include <memory>
#include <stdint.h>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

class CCoinControl;
class CFeeRate;
class CKey;
class CWallet;
enum class FeeReason;
enum class OutputType;
struct CRecipient;

namespace interfaces {

class Handler;
class PendingWalletTx;
struct WalletAddress;
struct WalletBalances;
struct WalletTx;
struct WalletTxOut;
struct WalletTxStatus;

using WalletOrderForm = std::vector<std::pair<std::string, std::string>>;
using WalletValueMap = std::map<std::string, std::string>;

//！用于访问钱包的接口。
class Wallet
{
public:
    virtual ~Wallet() {}

//！加密钱包。
    virtual bool encryptWallet(const SecureString& wallet_passphrase) = 0;

//！返回钱包是否加密。
    virtual bool isCrypted() = 0;

//！锁钱包。
    virtual bool lock() = 0;

//！解锁钱包。
    virtual bool unlock(const SecureString& wallet_passphrase) = 0;

//！返回钱包是否锁定。
    virtual bool isLocked() = 0;

//！更改钱包密码。
    virtual bool changeWalletPassphrase(const SecureString& old_wallet_passphrase,
        const SecureString& new_wallet_passphrase) = 0;

//！中止重新扫描。
    virtual void abortRescan() = 0;

//！备份钱包。
    virtual bool backupWallet(const std::string& filename) = 0;

//！获取钱包名称。
    virtual std::string getWalletName() = 0;

//从池中获取密钥。
    virtual bool getKeyFromPool(bool internal, CPubKey& pub_key) = 0;

//！获取公钥。
    virtual bool getPubKey(const CKeyID& address, CPubKey& pub_key) = 0;

//！获取私钥。
    virtual bool getPrivKey(const CKeyID& address, CKey& key) = 0;

//！返回钱包是否有私钥。
    virtual bool isSpendable(const CTxDestination& dest) = 0;

//！返回钱包是否只有手表钥匙。
    virtual bool haveWatchOnly() = 0;

//！添加或更新地址。
    virtual bool setAddressBook(const CTxDestination& dest, const std::string& name, const std::string& purpose) = 0;

//删除地址。
    virtual bool delAddressBook(const CTxDestination& dest) = 0;

//！查找钱包中的地址，返回是否存在。
    virtual bool getAddress(const CTxDestination& dest,
        std::string* name,
        isminetype* is_mine,
        std::string* purpose) = 0;

//！获取钱包地址列表。
    virtual std::vector<WalletAddress> getAddresses() = 0;

//！将脚本添加到密钥存储区，使软件版本打开钱包
//！数据库可以检测到新地址类型的付款。
    virtual void learnRelatedScripts(const CPubKey& key, OutputType type) = 0;

//！添加DEST数据。
    virtual bool addDestData(const CTxDestination& dest, const std::string& key, const std::string& value) = 0;

//！清除目标数据。
    virtual bool eraseDestData(const CTxDestination& dest, const std::string& key) = 0;

//！获取带前缀的dest值。
    virtual std::vector<std::string> getDestValues(const std::string& prefix) = 0;

//！锁硬币。
    virtual void lockCoin(const COutPoint& output) = 0;

//！解锁硬币。
    virtual void unlockCoin(const COutPoint& output) = 0;

//！返回是否锁定硬币。
    virtual bool isLockedCoin(const COutPoint& output) = 0;

//！列出锁定的硬币。
    virtual void listLockedCoins(std::vector<COutPoint>& outputs) = 0;

//！创建交易记录。
    virtual std::unique_ptr<PendingWalletTx> createTransaction(const std::vector<CRecipient>& recipients,
        const CCoinControl& coin_control,
        bool sign,
        int& change_pos,
        CAmount& fee,
        std::string& fail_reason) = 0;

//！返回是否可以放弃事务。
    virtual bool transactionCanBeAbandoned(const uint256& txid) = 0;

//！放弃交易。
    virtual bool abandonTransaction(const uint256& txid) = 0;

//！返回是否可以缓冲事务。
    virtual bool transactionCanBeBumped(const uint256& txid) = 0;

//！创建通气事务。
    virtual bool createBumpTransaction(const uint256& txid,
        const CCoinControl& coin_control,
        CAmount total_fee,
        std::vector<std::string>& errors,
        CAmount& old_fee,
        CAmount& new_fee,
        CMutableTransaction& mtx) = 0;

//！签署通气事务。
    virtual bool signBumpTransaction(CMutableTransaction& mtx) = 0;

//！提交Bump事务。
    virtual bool commitBumpTransaction(const uint256& txid,
        CMutableTransaction&& mtx,
        std::vector<std::string>& errors,
        uint256& bumped_txid) = 0;

//！获取交易记录。
    virtual CTransactionRef getTx(const uint256& txid) = 0;

//！获取交易信息。
    virtual WalletTx getWalletTx(const uint256& txid) = 0;

//！获取所有钱包交易的列表。
    virtual std::vector<WalletTx> getWalletTxs() = 0;

//！尽可能在不阻塞的情况下获取特定事务的更新状态。
    virtual bool tryGetTxStatus(const uint256& txid,
        WalletTxStatus& tx_status,
        int& num_blocks,
        int64_t& block_time) = 0;

//！获取交易详细信息。
    virtual WalletTx getWalletTxDetails(const uint256& txid,
        WalletTxStatus& tx_status,
        WalletOrderForm& order_form,
        bool& in_mempool,
        int& num_blocks) = 0;

//！获得平衡。
    virtual WalletBalances getBalances() = 0;

//！尽可能在不阻塞的情况下获得平衡。
    virtual bool tryGetBalances(WalletBalances& balances, int& num_blocks) = 0;

//！取得平衡。
    virtual CAmount getBalance() = 0;

//！获得可用余额。
    virtual CAmount getAvailableBalance(const CCoinControl& coin_control) = 0;

//！返回交易输入是否属于钱包。
    virtual isminetype txinIsMine(const CTxIn& txin) = 0;

//！返回交易输出是否属于钱包。
    virtual isminetype txoutIsMine(const CTxOut& txout) = 0;

//！如果交易输入属于钱包，则返回借方金额。
    virtual CAmount getDebit(const CTxIn& txin, isminefilter filter) = 0;

//！如果交易输入属于钱包，则返回贷方金额。
    virtual CAmount getCredit(const CTxOut& txout, isminefilter filter) = 0;

//！返回按钱包地址分组的可用硬币+锁定的硬币。
//！（将零钱与钱包地址放在一起）
    using CoinsList = std::map<CTxDestination, std::vector<std::tuple<COutPoint, WalletTxOut>>>;
    virtual CoinsList listCoins() = 0;

//！返回钱包交易输出信息。
    virtual std::vector<WalletTxOut> getCoins(const std::vector<COutPoint>& outputs) = 0;

//！获得所需费用。
    virtual CAmount getRequiredFee(unsigned int tx_bytes) = 0;

//！获得最低费用。
    virtual CAmount getMinimumFee(unsigned int tx_bytes,
        const CCoinControl& coin_control,
        int* returned_target,
        FeeReason* reason) = 0;

//！获取Tx确认目标。
    virtual unsigned int getConfirmTarget() = 0;

//返回是否启用HD。
    virtual bool hdEnabled() = 0;

//检查是否设置了某个钱包标志。
    virtual bool IsWalletFlagSet(uint64_t flag) = 0;

//获取默认地址类型。
    virtual OutputType getDefaultAddressType() = 0;

//获取默认更改类型。
    virtual OutputType getDefaultChangeType() = 0;

//！为卸载消息注册处理程序。
    using UnloadFn = std::function<void()>;
    virtual std::unique_ptr<Handler> handleUnload(UnloadFn fn) = 0;

//！为显示进度消息注册处理程序。
    using ShowProgressFn = std::function<void(const std::string& title, int progress)>;
    virtual std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) = 0;

//！注册状态更改消息的处理程序。
    using StatusChangedFn = std::function<void()>;
    virtual std::unique_ptr<Handler> handleStatusChanged(StatusChangedFn fn) = 0;

//！地址簿更改消息的注册处理程序。
    using AddressBookChangedFn = std::function<void(const CTxDestination& address,
        const std::string& label,
        bool is_mine,
        const std::string& purpose,
        ChangeType status)>;
    virtual std::unique_ptr<Handler> handleAddressBookChanged(AddressBookChangedFn fn) = 0;

//！事务更改消息的注册处理程序。
    using TransactionChangedFn = std::function<void(const uint256& txid, ChangeType status)>;
    virtual std::unique_ptr<Handler> handleTransactionChanged(TransactionChangedFn fn) = 0;

//！为watchOnly更改的消息注册处理程序。
    using WatchOnlyChangedFn = std::function<void(bool have_watch_only)>;
    virtual std::unique_ptr<Handler> handleWatchOnlyChanged(WatchOnlyChangedFn fn) = 0;
};

//！跟踪由CreateTransaction返回并传递给CommittTransaction的对象。
class PendingWalletTx
{
public:
    virtual ~PendingWalletTx() {}

//！获取交易数据。
    virtual const CTransaction& get() = 0;

//！获取虚拟事务大小。
    virtual int64_t getVirtualSize() = 0;

//！发送待处理事务并提交到Wallet。
    virtual bool commit(WalletValueMap value_map,
        WalletOrderForm order_form,
        std::string& reject_reason) = 0;
};

//！关于一个钱包地址的信息。
struct WalletAddress
{
    CTxDestination dest;
    isminetype is_mine;
    std::string name;
    std::string purpose;

    WalletAddress(CTxDestination dest, isminetype is_mine, std::string name, std::string purpose)
        : dest(std::move(dest)), is_mine(is_mine), name(std::move(name)), purpose(std::move(purpose))
    {
    }
};

//！收集钱包余额。
struct WalletBalances
{
    CAmount balance = 0;
    CAmount unconfirmed_balance = 0;
    CAmount immature_balance = 0;
    bool have_watch_only = false;
    CAmount watch_only_balance = 0;
    CAmount unconfirmed_watch_only_balance = 0;
    CAmount immature_watch_only_balance = 0;

    bool balanceChanged(const WalletBalances& prev) const
    {
        return balance != prev.balance || unconfirmed_balance != prev.unconfirmed_balance ||
               immature_balance != prev.immature_balance || watch_only_balance != prev.watch_only_balance ||
               unconfirmed_watch_only_balance != prev.unconfirmed_watch_only_balance ||
               immature_watch_only_balance != prev.immature_watch_only_balance;
    }
};

//钱包交易信息。
struct WalletTx
{
    CTransactionRef tx;
    std::vector<isminetype> txin_is_mine;
    std::vector<isminetype> txout_is_mine;
    std::vector<CTxDestination> txout_address;
    std::vector<isminetype> txout_address_is_mine;
    CAmount credit;
    CAmount debit;
    CAmount change;
    int64_t time;
    std::map<std::string, std::string> value_map;
    bool is_coinbase;
};

//！已更新交易状态。
struct WalletTxStatus
{
    int block_height;
    int blocks_to_maturity;
    int depth_in_main_chain;
    unsigned int time_received;
    uint32_t lock_time;
    bool is_final;
    bool is_trusted;
    bool is_abandoned;
    bool is_coinbase;
    bool is_in_main_chain;
};

//！钱包交易输出。
struct WalletTxOut
{
    CTxOut txout;
    int64_t time;
    int depth_in_main_chain = -1;
    bool is_spent = false;
};

//！钱包接口的返回实现。此函数定义于
//！dummywallet.cpp，如果未编译wallet组件，则抛出。
std::unique_ptr<Wallet> MakeWallet(const std::shared_ptr<CWallet>& wallet);

} //命名空间接口

#endif //比特币接口钱包
