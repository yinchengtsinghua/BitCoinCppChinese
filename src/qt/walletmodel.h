
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011-2019比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_QT_WALLETMODEL_H
#define BITCOIN_QT_WALLETMODEL_H

#include <amount.h>
#include <key.h>
#include <serialize.h>
#include <script/standard.h>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#ifdef ENABLE_BIP70
#include <qt/paymentrequestplus.h>
#endif
#include <qt/walletmodeltransaction.h>

#include <interfaces/wallet.h>
#include <support/allocators/secure.h>

#include <map>
#include <vector>

#include <QObject>

enum class OutputType;

class AddressTableModel;
class OptionsModel;
class PlatformStyle;
class RecentRequestsTableModel;
class TransactionTableModel;
class WalletModelTransaction;

class CCoinControl;
class CKeyID;
class COutPoint;
class COutput;
class CPubKey;
class uint256;

namespace interfaces {
class Node;
} //命名空间接口

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

class SendCoinsRecipient
{
public:
    explicit SendCoinsRecipient() : amount(0), fSubtractFeeFromAmount(false), nVersion(SendCoinsRecipient::CURRENT_VERSION) { }
    explicit SendCoinsRecipient(const QString &addr, const QString &_label, const CAmount& _amount, const QString &_message):
        address(addr), label(_label), amount(_amount), message(_message), fSubtractFeeFromAmount(false), nVersion(SendCoinsRecipient::CURRENT_VERSION) {}

//如果来自未经验证的付款请求，则用于存储
//地址，例如地址-a<br/>地址-b<br/>地址-c。
//信息：因为我们在使用时不需要在这里处理地址
//付款请求，我们可以滥用它显示地址列表。
//托多：这是一个黑客，应该换成更清洁的解决方案！
    QString address;
    QString label;
    CAmount amount;
//如果来自付款请求，则用于存储备忘
    QString message;

#ifdef ENABLE_BIP70
//如果来自付款请求，paymentRequest.isInitialized（）将为true
    PaymentRequestPlus paymentRequest;
#else
//如果禁用带BIP70的建筑，请将付款请求保留为
//序列化字符串以确保加载/存储是无损的
    std::string sPaymentRequest;
#endif
//如果没有身份验证或签名/证书等无效，则为空。
    QString authenticatedMerchant;

bool fSubtractFeeFromAmount; //仅记忆

    static const int CURRENT_VERSION = 1;
    int nVersion;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        std::string sAddress = address.toStdString();
        std::string sLabel = label.toStdString();
        std::string sMessage = message.toStdString();
#ifdef ENABLE_BIP70
        std::string sPaymentRequest;
        if (!ser_action.ForRead() && paymentRequest.IsInitialized())
            paymentRequest.SerializeToString(&sPaymentRequest);
#endif
        std::string sAuthenticatedMerchant = authenticatedMerchant.toStdString();

        READWRITE(this->nVersion);
        READWRITE(sAddress);
        READWRITE(sLabel);
        READWRITE(amount);
        READWRITE(sMessage);
        READWRITE(sPaymentRequest);
        READWRITE(sAuthenticatedMerchant);

        if (ser_action.ForRead())
        {
            address = QString::fromStdString(sAddress);
            label = QString::fromStdString(sLabel);
            message = QString::fromStdString(sMessage);
#ifdef ENABLE_BIP70
            if (!sPaymentRequest.empty())
                paymentRequest.parse(QByteArray::fromRawData(sPaymentRequest.data(), sPaymentRequest.size()));
#endif
            authenticatedMerchant = QString::fromStdString(sAuthenticatedMerchant);
        }
    }
};

/*从qt视图代码到比特币钱包的接口。*/
class WalletModel : public QObject
{
    Q_OBJECT

public:
    explicit WalletModel(std::unique_ptr<interfaces::Wallet> wallet, interfaces::Node& node, const PlatformStyle *platformStyle, OptionsModel *optionsModel, QObject *parent = nullptr);
    ~WalletModel();

enum StatusCode //sendcoins退回
    {
        OK,
        InvalidAmount,
        InvalidAddress,
        AmountExceedsBalance,
        AmountWithFeeExceedsBalance,
        DuplicateAddress,
TransactionCreationFailed, //钱包仍被锁定时返回错误
        TransactionCommitFailed,
        AbsurdFee,
        PaymentRequestExpired
    };

    enum EncryptionStatus
    {
Unencrypted,  //！钱包->iscrypted（））
Locked,       //wallet->iscrypted（）&&wallet->islocked（））
Unlocked      //钱包->iscrypted（）&&！钱包->已锁定（）
    };

    OptionsModel *getOptionsModel();
    AddressTableModel *getAddressTableModel();
    TransactionTableModel *getTransactionTableModel();
    RecentRequestsTableModel *getRecentRequestsTableModel();

    EncryptionStatus getEncryptionStatus() const;

//检查地址是否有效
    bool validateAddress(const QString &address);

//sendcoins的返回状态记录，包含错误ID+信息
    struct SendCoinsReturn
    {
        SendCoinsReturn(StatusCode _status = OK, QString _reasonCommitFailed = "")
            : status(_status),
              reasonCommitFailed(_reasonCommitFailed)
        {
        }
        StatusCode status;
        QString reasonCommitFailed;
    };

//在发送硬币前准备交易以获取txfee
    SendCoinsReturn prepareTransaction(WalletModelTransaction &transaction, const CCoinControl& coinControl);

//将硬币发送到收件人列表
    SendCoinsReturn sendCoins(WalletModelTransaction &transaction);

//钱包加密
    bool setWalletEncrypted(bool encrypted, const SecureString &passphrase);
//仅在解锁时需要密码短语
    bool setWalletLocked(bool locked, const SecureString &passPhrase=SecureString());
    bool changePassphrase(const SecureString &oldPass, const SecureString &newPass);

//用于解锁钱包的rai对象，由requestUnlock（）返回
    class UnlockContext
    {
    public:
        UnlockContext(WalletModel *wallet, bool valid, bool relock);
        ~UnlockContext();

        bool isValid() const { return valid; }

//复制运算符和构造函数传输上下文
        UnlockContext(const UnlockContext& obj) { CopyFrom(obj); }
        UnlockContext& operator=(const UnlockContext& rhs) { CopyFrom(rhs); return *this; }
    private:
        WalletModel *wallet;
        bool valid;
mutable bool relock; //可变，因为可以通过复制将其设置为false

        void CopyFrom(const UnlockContext& rhs);
    };

    UnlockContext requestUnlock();

    void loadReceiveRequests(std::vector<std::string>& vReceiveRequests);
    bool saveReceiveRequest(const std::string &sAddress, const int64_t nId, const std::string &sRequest);

    bool bumpFee(uint256 hash, uint256& new_hash);

    static bool isWalletEnabled();
    bool privateKeysDisabled() const;

    interfaces::Node& node() const { return m_node; }
    interfaces::Wallet& wallet() const { return *m_wallet; }

    QString getWalletName() const;
    QString getDisplayName() const;

    bool isMultiwallet();

    AddressTableModel* getAddressTableModel() const { return addressTableModel; }
private:
    std::unique_ptr<interfaces::Wallet> m_wallet;
    std::unique_ptr<interfaces::Handler> m_handler_unload;
    std::unique_ptr<interfaces::Handler> m_handler_status_changed;
    std::unique_ptr<interfaces::Handler> m_handler_address_book_changed;
    std::unique_ptr<interfaces::Handler> m_handler_transaction_changed;
    std::unique_ptr<interfaces::Handler> m_handler_show_progress;
    std::unique_ptr<interfaces::Handler> m_handler_watch_only_changed;
    interfaces::Node& m_node;

    bool fHaveWatchOnly;
    bool fForceCheckBalanceChanged{false};

//Wallet具有特定于Wallet选项的选项模型
//（例如，交易费）
    OptionsModel *optionsModel;

    AddressTableModel *addressTableModel;
    TransactionTableModel *transactionTableModel;
    RecentRequestsTableModel *recentRequestsTableModel;

//缓存一些值以检测更改
    interfaces::WalletBalances m_cached_balances;
    EncryptionStatus cachedEncryptionStatus;
    int cachedNumBlocks;

    QTimer *pollTimer;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
    void checkBalanceChanged(const interfaces::WalletBalances& new_balances);

Q_SIGNALS:
//钱包里的余额有变化的信号
    void balanceChanged(const interfaces::WalletBalances& balances);

//钱包加密状态已更改
    void encryptionStatusChanged();

//钱包需要解锁时发出的信号
//此信号发出后，听者将钱包锁上是有效的行为；
//这意味着解锁失败或被取消。
    void requireUnlock();

//在应向用户报告消息时激发
    void message(const QString &title, const QString &message, unsigned int style);

//发送的硬币：从钱包，到接收者，在（系列化）交易中：
    void coinsSent(WalletModel* wallet, SendCoinsRecipient recipient, QByteArray transaction);

//显示进度对话框，例如用于重新扫描
    void showProgress(const QString &title, int nProgress);

//仅监视添加的地址
    void notifyWatchonlyChanged(bool fHaveWatchonly);

//表示钱包即将被取下
    void unload();

public Q_SLOTS:
    /*钱包状态可能已更改*/
    void updateStatus();
    /*新事务或事务更改状态*/
    void updateTransaction();
    /*新建、更新或删除通讯簿条目*/
    void updateAddressBook(const QString &address, const QString &label, bool isMine, const QString &purpose, int status);
    /*只添加监视*/
    void updateWatchOnlyFlag(bool fHaveWatchonly);
    /*当前、不成熟或未确认的平衡可能已发生变化-如果是，则发出“BalanceChanged”*/
    void pollBalanceChanged();
};

#endif //比特币\u qt\u钱包型号\u h
