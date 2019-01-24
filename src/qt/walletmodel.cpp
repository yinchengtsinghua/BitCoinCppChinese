
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

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/walletmodel.h>

#include <qt/addresstablemodel.h>
#include <qt/guiconstants.h>
#include <qt/optionsmodel.h>
#include <qt/paymentserver.h>
#include <qt/recentrequeststablemodel.h>
#include <qt/sendcoinsdialog.h>
#include <qt/transactiontablemodel.h>

#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <key_io.h>
#include <ui_interface.h>
#include <util/system.h> //对于GETBOOLARG
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>

#include <stdint.h>

#include <QDebug>
#include <QMessageBox>
#include <QSet>
#include <QTimer>


WalletModel::WalletModel(std::unique_ptr<interfaces::Wallet> wallet, interfaces::Node& node, const PlatformStyle *platformStyle, OptionsModel *_optionsModel, QObject *parent) :
    QObject(parent), m_wallet(std::move(wallet)), m_node(node), optionsModel(_optionsModel), addressTableModel(nullptr),
    transactionTableModel(nullptr),
    recentRequestsTableModel(nullptr),
    cachedEncryptionStatus(Unencrypted),
    cachedNumBlocks(0)
{
    fHaveWatchOnly = m_wallet->haveWatchOnly();
    addressTableModel = new AddressTableModel(this);
    transactionTableModel = new TransactionTableModel(platformStyle, this);
    recentRequestsTableModel = new RecentRequestsTableModel(this);

//此计时器将重复启动以更新余额
    pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, &WalletModel::pollBalanceChanged);
    pollTimer->start(MODEL_UPDATE_DELAY);

    subscribeToCoreSignals();
}

WalletModel::~WalletModel()
{
    unsubscribeFromCoreSignals();
}

void WalletModel::updateStatus()
{
    EncryptionStatus newEncryptionStatus = getEncryptionStatus();

    if(cachedEncryptionStatus != newEncryptionStatus) {
        Q_EMIT encryptionStatusChanged();
    }
}

void WalletModel::pollBalanceChanged()
{
//如果无法获取锁，请尝试获取平衡并提前返回。这个
//如果核心是
//将锁保持较长时间-例如，在钱包中
//再扫描。
    interfaces::WalletBalances new_balances;
    int numBlocks = -1;
    if (!m_wallet->tryGetBalances(new_balances, numBlocks)) {
        return;
    }

    if(fForceCheckBalanceChanged || m_node.getNumBlocks() != cachedNumBlocks)
    {
        fForceCheckBalanceChanged = false;

//交易的余额和数量可能已更改
        cachedNumBlocks = m_node.getNumBlocks();

        checkBalanceChanged(new_balances);
        if(transactionTableModel)
            transactionTableModel->updateConfirmations();
    }
}

void WalletModel::checkBalanceChanged(const interfaces::WalletBalances& new_balances)
{
    if(new_balances.balanceChanged(m_cached_balances)) {
        m_cached_balances = new_balances;
        Q_EMIT balanceChanged(new_balances);
    }
}

void WalletModel::updateTransaction()
{
//交易的余额和数量可能已更改
    fForceCheckBalanceChanged = true;
}

void WalletModel::updateAddressBook(const QString &address, const QString &label,
        bool isMine, const QString &purpose, int status)
{
    if(addressTableModel)
        addressTableModel->updateEntry(address, label, isMine, purpose, status);
}

void WalletModel::updateWatchOnlyFlag(bool fHaveWatchonly)
{
    fHaveWatchOnly = fHaveWatchonly;
    Q_EMIT notifyWatchonlyChanged(fHaveWatchonly);
}

bool WalletModel::validateAddress(const QString &address)
{
    return IsValidDestinationString(address.toStdString());
}

WalletModel::SendCoinsReturn WalletModel::prepareTransaction(WalletModelTransaction &transaction, const CCoinControl& coinControl)
{
    CAmount total = 0;
    bool fSubtractFeeFromAmount = false;
    QList<SendCoinsRecipient> recipients = transaction.getRecipients();
    std::vector<CRecipient> vecSend;

    if(recipients.empty())
    {
        return OK;
    }

QSet<QString> setAddress; //用于检测重复项
    int nAddresses = 0;

//预检查输入数据的有效性
    for (const SendCoinsRecipient &rcp : recipients)
    {
        if (rcp.fSubtractFeeFromAmount)
            fSubtractFeeFromAmount = true;

#ifdef ENABLE_BIP70
        if (rcp.paymentRequest.IsInitialized())
{   //付款请求…
            CAmount subtotal = 0;
            const payments::PaymentDetails& details = rcp.paymentRequest.getDetails();
            for (int i = 0; i < details.outputs_size(); i++)
            {
                const payments::Output& out = details.outputs(i);
                if (out.amount() <= 0) continue;
                subtotal += out.amount();
                const unsigned char* scriptStr = (const unsigned char*)out.script().data();
                CScript scriptPubKey(scriptStr, scriptStr+out.script().size());
                CAmount nAmount = out.amount();
                CRecipient recipient = {scriptPubKey, nAmount, rcp.fSubtractFeeFromAmount};
                vecSend.push_back(recipient);
            }
            if (subtotal <= 0)
            {
                return InvalidAmount;
            }
            total += subtotal;
        }
        else
#endif
{   //用户输入的比特币地址/金额：
            if(!validateAddress(rcp.address))
            {
                return InvalidAddress;
            }
            if(rcp.amount <= 0)
            {
                return InvalidAmount;
            }
            setAddress.insert(rcp.address);
            ++nAddresses;

            CScript scriptPubKey = GetScriptForDestination(DecodeDestination(rcp.address.toStdString()));
            CRecipient recipient = {scriptPubKey, rcp.amount, rcp.fSubtractFeeFromAmount};
            vecSend.push_back(recipient);

            total += rcp.amount;
        }
    }
    if(setAddress.size() != nAddresses)
    {
        return DuplicateAddress;
    }

    CAmount nBalance = m_wallet->getAvailableBalance(coinControl);

    if(total > nBalance)
    {
        return AmountExceedsBalance;
    }

    {
        CAmount nFeeRequired = 0;
        int nChangePosRet = -1;
        std::string strFailReason;

        auto& newTx = transaction.getWtx();
        /*tx=m_wallet->createTransaction（vecsend，coincontrol，true/*sign*/，nchangeposret，nfeereequired，strfailreason）；
        交易设置交易费（非对等）；
        如果（fsubtractfefromamount和newtx）
            事务处理。重新分配金额（nchangeposret）；

        如果（！）NEDTX）
        {
            如果（！）fsubtractfeefromomamount&&（total+nfeereequired）>n平衡）
            {
                返回sendcoinsreurn（金额为feeeexceedsbalance）；
            }
            发出消息（tr（“发送硬币”），qstring:：fromstdstring（strfailreason），
                         cclientuiinterface:：msg_错误）；
            退货交易创建失败；
        }

        //拒绝高得离谱的费用。（这永远不会发生，因为
        //钱包的费用上限为maxtxfee。这只是一个
        //皮带和吊带检查）
        if（nfeereequired>m_node.getMaxTxFee（））
            归还荒谬的费用；
    }

    返回sendcoinsreturn（OK）；
}

walletmodel:：sendcoinsreturn walletmodel:：sendcoins（walletmodel transaction&transaction）
{
    qbytearray事务_array；/*存储序列化事务*/


    {
        std::vector<std::pair<std::string, std::string>> vOrderForm;
        for (const SendCoinsRecipient &rcp : transaction.getRecipients())
        {
#ifdef ENABLE_BIP70
            if (rcp.paymentRequest.IsInitialized())
            {
//确保涉及的任何付款请求仍然有效。
                if (PaymentServer::verifyExpired(rcp.paymentRequest.getDetails())) {
                    return PaymentRequestExpired;
                }

//将paymentrequests存储在wallet中的wtx.vorderform中。
                std::string value;
                rcp.paymentRequest.SerializeToString(&value);
                vOrderForm.emplace_back("PaymentRequest", std::move(value));
            }
            else
#endif
if (!rcp.message.isEmpty()) //来自普通比特币的信息：uri（比特币：123…？消息=示例）
                vOrderForm.emplace_back("Message", rcp.message.toStdString());
        }

        auto& newTx = transaction.getWtx();
        std::string rejectReason;
        /*（！newtx->commit（/*mapvalue*/，std:：move（vorderform），rejectReason）
            返回sendcoinsreurn（TransactionCommitFailed，qString:：FromStdString（RejectReason））；

        cdatastream sstx（ser_网络，协议版本）；
        sstx<<newtx->get（）；
        事务_array.append（&（sstx[0]），sstx.size（））；
    }

    //添加地址/更新我们发送到通讯簿的标签，
    //并为每个接收者发出共同信号
    for（const sendcoinsrecipient&rcp:transaction.getRecipients（））
    {
        //有付款请求时不要触摸通讯簿
ifdef启用_bip70
        如果（！）rcp.paymentRequest.isInitialized（））
第二节
        {
            std:：string straddress=rcp.address.tostdstring（）；
            ctxdestination dest=解码目的地（跨地址）；
            std:：string strlabel=rcp.label.tostdstring（）；
            {
                //检查是否有新地址或更新的标签
                std：：字符串名称；
                如果（！）M钱包->获取地址（
                     dest，&name，/*is_mine=*/ nullptr, /* purpose= */ nullptr))

                {
                    m_wallet->setAddressBook(dest, strLabel, "send");
                }
                else if (name != strLabel)
                {
m_wallet->setAddressBook(dest, strLabel, ""); //“意思是不要改变目的
                }
            }
        }
        Q_EMIT coinsSent(this, rcp, transaction_array);
    }

checkBalanceChanged(m_wallet->getBalances()); //立即更新余额，否则在PollBalanceChanceChanged点击之前可能会有一小段明显的延迟。

    return SendCoinsReturn(OK);
}

OptionsModel *WalletModel::getOptionsModel()
{
    return optionsModel;
}

AddressTableModel *WalletModel::getAddressTableModel()
{
    return addressTableModel;
}

TransactionTableModel *WalletModel::getTransactionTableModel()
{
    return transactionTableModel;
}

RecentRequestsTableModel *WalletModel::getRecentRequestsTableModel()
{
    return recentRequestsTableModel;
}

WalletModel::EncryptionStatus WalletModel::getEncryptionStatus() const
{
    if(!m_wallet->isCrypted())
    {
        return Unencrypted;
    }
    else if(m_wallet->isLocked())
    {
        return Locked;
    }
    else
    {
        return Unlocked;
    }
}

bool WalletModel::setWalletEncrypted(bool encrypted, const SecureString &passphrase)
{
    if(encrypted)
    {
//加密
        return m_wallet->encryptWallet(passphrase);
    }
    else
    {
//decrypt--todo；尚不支持
        return false;
    }
}

bool WalletModel::setWalletLocked(bool locked, const SecureString &passPhrase)
{
    if(locked)
    {
//锁
        return m_wallet->lock();
    }
    else
    {
//解锁
        return m_wallet->unlock(passPhrase);
    }
}

bool WalletModel::changePassphrase(const SecureString &oldPass, const SecureString &newPass)
{
m_wallet->lock(); //在更换通行证之前，确保钱包已锁定。
    return m_wallet->changeWalletPassphrase(oldPass, newPass);
}

//核心信号处理程序
static void NotifyUnload(WalletModel* walletModel)
{
    qDebug() << "NotifyUnload";
    QMetaObject::invokeMethod(walletModel, "unload");
}

static void NotifyKeyStoreStatusChanged(WalletModel *walletmodel)
{
    qDebug() << "NotifyKeyStoreStatusChanged";
    QMetaObject::invokeMethod(walletmodel, "updateStatus", Qt::QueuedConnection);
}

static void NotifyAddressBookChanged(WalletModel *walletmodel,
        const CTxDestination &address, const std::string &label, bool isMine,
        const std::string &purpose, ChangeType status)
{
    QString strAddress = QString::fromStdString(EncodeDestination(address));
    QString strLabel = QString::fromStdString(label);
    QString strPurpose = QString::fromStdString(purpose);

    qDebug() << "NotifyAddressBookChanged: " + strAddress + " " + strLabel + " isMine=" + QString::number(isMine) + " purpose=" + strPurpose + " status=" + QString::number(status);
    QMetaObject::invokeMethod(walletmodel, "updateAddressBook", Qt::QueuedConnection,
                              Q_ARG(QString, strAddress),
                              Q_ARG(QString, strLabel),
                              Q_ARG(bool, isMine),
                              Q_ARG(QString, strPurpose),
                              Q_ARG(int, status));
}

static void NotifyTransactionChanged(WalletModel *walletmodel, const uint256 &hash, ChangeType status)
{
    Q_UNUSED(hash);
    Q_UNUSED(status);
    QMetaObject::invokeMethod(walletmodel, "updateTransaction", Qt::QueuedConnection);
}

static void ShowProgress(WalletModel *walletmodel, const std::string &title, int nProgress)
{
//发出“ShowProgress”信号
    QMetaObject::invokeMethod(walletmodel, "showProgress", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(title)),
                              Q_ARG(int, nProgress));
}

static void NotifyWatchonlyChanged(WalletModel *walletmodel, bool fHaveWatchonly)
{
    QMetaObject::invokeMethod(walletmodel, "updateWatchOnlyFlag", Qt::QueuedConnection,
                              Q_ARG(bool, fHaveWatchonly));
}

void WalletModel::subscribeToCoreSignals()
{
//将信号连接到钱包
    m_handler_unload = m_wallet->handleUnload(std::bind(&NotifyUnload, this));
    m_handler_status_changed = m_wallet->handleStatusChanged(std::bind(&NotifyKeyStoreStatusChanged, this));
    m_handler_address_book_changed = m_wallet->handleAddressBookChanged(std::bind(NotifyAddressBookChanged, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
    m_handler_transaction_changed = m_wallet->handleTransactionChanged(std::bind(NotifyTransactionChanged, this, std::placeholders::_1, std::placeholders::_2));
    m_handler_show_progress = m_wallet->handleShowProgress(std::bind(ShowProgress, this, std::placeholders::_1, std::placeholders::_2));
    m_handler_watch_only_changed = m_wallet->handleWatchOnlyChanged(std::bind(NotifyWatchonlyChanged, this, std::placeholders::_1));
}

void WalletModel::unsubscribeFromCoreSignals()
{
//断开钱包信号
    m_handler_unload->disconnect();
    m_handler_status_changed->disconnect();
    m_handler_address_book_changed->disconnect();
    m_handler_transaction_changed->disconnect();
    m_handler_show_progress->disconnect();
    m_handler_watch_only_changed->disconnect();
}

//WalletModel:：UnlockContext实现
WalletModel::UnlockContext WalletModel::requestUnlock()
{
    bool was_locked = getEncryptionStatus() == Locked;
    if(was_locked)
    {
//请求用户界面解锁钱包
        Q_EMIT requireUnlock();
    }
//如果钱包仍被锁定、解锁失败或取消，则将上下文标记为无效
    bool valid = getEncryptionStatus() != Locked;

    return UnlockContext(this, valid, was_locked);
}

WalletModel::UnlockContext::UnlockContext(WalletModel *_wallet, bool _valid, bool _relock):
        wallet(_wallet),
        valid(_valid),
        relock(_relock)
{
}

WalletModel::UnlockContext::~UnlockContext()
{
    if(valid && relock)
    {
        wallet->setWalletLocked(true);
    }
}

void WalletModel::UnlockContext::CopyFrom(const UnlockContext& rhs)
{
//传输上下文；旧对象不再重新锁定钱包
    *this = rhs;
    rhs.relock = false;
}

void WalletModel::loadReceiveRequests(std::vector<std::string>& vReceiveRequests)
{
vReceiveRequests = m_wallet->getDestValues("rr"); //接收请求
}

bool WalletModel::saveReceiveRequest(const std::string &sAddress, const int64_t nId, const std::string &sRequest)
{
    CTxDestination dest = DecodeDestination(sAddress);

    std::stringstream ss;
    ss << nId;
std::string key = "rr" + ss.str(); //destdata中的“rr”prefix=“receive request”

    if (sRequest.empty())
        return m_wallet->eraseDestData(dest, key);
    else
        return m_wallet->addDestData(dest, key, sRequest);
}

bool WalletModel::bumpFee(uint256 hash, uint256& new_hash)
{
    CCoinControl coin_control;
    coin_control.m_signal_bip125_rbf = true;
    std::vector<std::string> errors;
    CAmount old_fee;
    CAmount new_fee;
    CMutableTransaction mtx;
    /*（！m_wallet->createBunbTransaction（哈希、硬币控制、0/*totalFee*/、错误、旧_费、新_费、MTX））
        qmessagebox:：critical（nullptr，tr（“费用波动错误”），tr（“增加交易费用失败”）+“<br/>（”+
            （错误。大小（）？qstring:：fromstdstring（错误[0]）：“”“）+”“）；
         返回错误；
    }

    //允许基于用户的费用验证
    qString questionString=tr（“你想增加费用吗？”）；
    questionString.append（“<br/>”）；
    questionString.append（“<table style=\”text-align:left；“\”>“）；
    questionString.append（“<tr><td>”）；
    questionString.append（tr（“当前费用：”））；
    questionString.append（“<td><td>”）；
    questionString.append（bitcoinUnits:：formatHTMLWithUnit（getOptionsModel（）->getDisplayUnit（），old_fee））；
    questionString.append（“<td><tr><tr><td>”）；
    questionString.append（tr（“increase：”））；
    questionString.append（“<td><td>”）；
    questionString.append（bitcoinUnits:：formatHTMLWithUnit（getOptionsModel（）->getDisplayUnit（），new_fee-old_fee））；
    questionString.append（“<td><tr><tr><td>”）；
    questionString.append（tr（“新费用：”））；
    questionString.append（“<td><td>”）；
    questionString.append（bitcoinUnits:：formatHTMLWithUnit（getOptionsModel（）->getDisplayUnit（），new_fee））；
    questionString.append（“<td><tr><table>”）；
    sendconfirmationdialog confirmationdialog（tr（“confirm fee bump”），questionstring）；
    确认对话框.exec（）；
    qmessagebox:：standardbutton retval=static_cast<qmessagebox:：standardbutton>（confirmationdialog.result（））；

    //如果用户不想增加费用，则取消签名和广播
    如果（雷瓦尔）！=Q消息框：：是）
        返回错误；
    }

    WalletModel:：UnlockContext CTX（requestUnlock（））；
    如果（！）验证（）
    {
        返回错误；
    }

    //签署缓冲事务
    如果（！）m_wallet->signbumptransaction（mtx））
        qmessagebox:：critical（nullptr，tr（“费用波动错误”），tr（“无法签署事务”）；
        返回错误；
    }
    //提交缓冲事务
    如果（！）m_wallet->commitBumpTransaction（hash，std:：move（mtx），errors，new_hash））
        qmessagebox:：critical（nullptr，tr（“费用波动错误”），tr（“无法提交事务”）+“<br/>（”+
            qstring：：fromstdstring（错误[0]）+“）；
         返回错误；
    }
    回归真实；
}

bool walletmodel:：iswalletEnabled（）。
{
   回来！gargs.getboolarg（“-disable wallet”，默认为“禁用钱包”）；
}

bool walletmodel:：privatekeysdisabled（）常量
{
    返回m_wallet->iswalletflagset（wallet_flag_disable_private_keys）；
}

qString WalletModel:：GetWalletName（）常量
{
    返回qstring:：fromstdstring（m_wallet->getwalletname（））；
}

qString WalletModel:：GetDisplayName（）常量
{
    const qstring name=getwalletname（）；
    返回name.isEmpty（）？“[“+tr”（“默认钱包”）+“]”：名称；
}

bool walletmodel:：ismultiwallet（）。
{
    返回m_node.getwallets（）.size（）>1；
}
