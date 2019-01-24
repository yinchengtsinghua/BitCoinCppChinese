
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/paymentserver.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <chainparams.h>
#include <interfaces/node.h>
#include <policy/policy.h>
#include <key_io.h>
#include <ui_interface.h>
#include <util/system.h>
#include <wallet/wallet.h>

#include <cstdlib>
#include <memory>

#include <openssl/x509_vfy.h>

#include <QApplication>
#include <QByteArray>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileOpenEvent>
#include <QHash>
#include <QList>
#include <QLocalServer>
#include <QLocalSocket>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslError>
#include <QSslSocket>
#include <QStringList>
#include <QTextDocument>
#include <QUrlQuery>

const int BITCOIN_IPC_CONNECT_TIMEOUT = 1000; //毫秒
const QString BITCOIN_IPC_PREFIX("bitcoin:");
#ifdef ENABLE_BIP70
//BIP70支付协议报文
const char* BIP70_MESSAGE_PAYMENTACK = "PaymentACK";
const char* BIP70_MESSAGE_PAYMENTREQUEST = "PaymentRequest";
//BIP71支付协议媒体类型
const char* BIP71_MIMETYPE_PAYMENT = "application/bitcoin-payment";
const char* BIP71_MIMETYPE_PAYMENTACK = "application/bitcoin-paymentack";
const char* BIP71_MIMETYPE_PAYMENTREQUEST = "application/bitcoin-paymentrequest";
#endif

//
//创建唯一的名称：
//测试网/非测试网
//数据目录
//
static QString ipcServerName()
{
    QString name("BitcoinQt");

//附加一个简单的datadir哈希
//注意，getdatadir（true）返回不同的路径
//for-测试网与主网
    QString ddir(GUIUtil::boostPathToQString(GetDataDir(true)));
    name.append(QString::number(qHash(ddir)));

    return name;
}

//
//我们存储之前收到的付款URI和请求
//主GUI窗口已打开并准备好询问用户
//发送付款。

static QList<QString> savedPaymentRequests;

//
//启动时同步发送到服务器。
//如果服务器尚未运行，则继续启动，
//并处理savedpaymentrequest中的项目
//当调用uiready（）时。
//
//警告：ipcsendcommandline（）在init的早期被调用，
//所以不要使用“q_emit message（）”，而是使用“qmessagebox:：”！
//
void PaymentServer::ipcParseCommandLine(interfaces::Node& node, int argc, char* argv[])
{
    for (int i = 1; i < argc; i++)
    {
        QString arg(argv[i]);
        if (arg.startsWith("-"))
            continue;

//如果bitcoin:uri包含付款请求，我们无法检测
//需要获取和分析付款请求的网络。
//这意味着单击包含testnet付款请求的此类URI。
//将启动一个mainnet实例并引发“错误网络”错误。
if (arg.startsWith(BITCOIN_IPC_PREFIX, Qt::CaseInsensitive)) //比特币：URI
        {
            savedPaymentRequests.append(arg);

            SendCoinsRecipient r;
            if (GUIUtil::parseBitcoinURI(arg, &r) && !r.address.isEmpty())
            {
                auto tempChainParams = CreateChainParams(CBaseChainParams::MAIN);

                if (IsValidDestinationString(r.address.toStdString(), *tempChainParams)) {
                    node.selectParams(CBaseChainParams::MAIN);
                } else {
                    tempChainParams = CreateChainParams(CBaseChainParams::TESTNET);
                    if (IsValidDestinationString(r.address.toStdString(), *tempChainParams)) {
                        node.selectParams(CBaseChainParams::TESTNET);
                    }
                }
            }
        }
#ifdef ENABLE_BIP70
else if (QFile::exists(arg)) //文件名
        {
            savedPaymentRequests.append(arg);

            PaymentRequestPlus request;
            if (readPaymentRequestFromFile(arg, request))
            {
                if (request.getDetails().network() == "main")
                {
                    node.selectParams(CBaseChainParams::MAIN);
                }
                else if (request.getDetails().network() == "test")
                {
                    node.selectParams(CBaseChainParams::TESTNET);
                }
            }
        }
        else
        {
//打印到debug.log是我们在这里所能做的最好的工作，
//GUI还没有启动，因此我们无法弹出消息框。
            qWarning() << "PaymentServer::ipcSendCommandLine: Payment request file does not exist: " << arg;
        }
#endif
    }
}

//
//启动时同步发送到服务器。
//如果服务器尚未运行，则继续启动，
//并处理savedpaymentrequest中的项目
//当调用uiready（）时。
//
bool PaymentServer::ipcSendCommandLine()
{
    bool fResult = false;
    for (const QString& r : savedPaymentRequests)
    {
        QLocalSocket* socket = new QLocalSocket();
        socket->connectToServer(ipcServerName(), QIODevice::WriteOnly);
        if (!socket->waitForConnected(BITCOIN_IPC_CONNECT_TIMEOUT))
        {
            delete socket;
            socket = nullptr;
            return false;
        }

        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_4_0);
        out << r;
        out.device()->seek(0);

        socket->write(block);
        socket->flush();
        socket->waitForBytesWritten(BITCOIN_IPC_CONNECT_TIMEOUT);
        socket->disconnectFromServer();

        delete socket;
        socket = nullptr;
        fResult = true;
    }

    return fResult;
}

PaymentServer::PaymentServer(QObject* parent, bool startLocalServer) :
    QObject(parent),
    saveURIs(true),
    uriServer(nullptr),
    optionsModel(nullptr)
#ifdef ENABLE_BIP70
    ,netManager(nullptr)
#endif
{
#ifdef ENABLE_BIP70
//验证我们链接的库的版本是否为
//与我们编译的头的版本兼容。
    GOOGLE_PROTOBUF_VERIFY_VERSION;
#endif

//安装全局事件筛选器以捕获qFileOpenEvents
//在Mac上：点击比特币时发送：链接
//其他操作系统：处理付款请求文件时很有用
    if (parent)
        parent->installEventFilter(this);

    QString name = ipcServerName();

//清理碰撞留下的旧插座：
    QLocalServer::removeServer(name);

    if (startLocalServer)
    {
        uriServer = new QLocalServer(this);

        if (!uriServer->listen(name)) {
//构造函数在init的早期被调用，所以不要在这里使用“q_emit message（）”
            QMessageBox::critical(nullptr, tr("Payment request error"),
                tr("Cannot start bitcoin: click-to-pay handler"));
        }
        else {
            connect(uriServer, &QLocalServer::newConnection, this, &PaymentServer::handleURIConnection);
#ifdef ENABLE_BIP70
            connect(this, &PaymentServer::receivedPaymentACK, this, &PaymentServer::handlePaymentACK);
#endif
        }
    }
}

PaymentServer::~PaymentServer()
{
#ifdef ENABLE_BIP70
    google::protobuf::ShutdownProtobufLibrary();
#endif
}

//
//OSX处理比特币的特定方式：uri和paymentrequest mime类型。
//paymentServerTests.cpp和打开付款请求文件时也使用
//通过“打开URI…”菜单项。
//
bool PaymentServer::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent *fileEvent = static_cast<QFileOpenEvent*>(event);
        if (!fileEvent->file().isEmpty())
            handleURIOrFile(fileEvent->file());
        else if (!fileEvent->url().isEmpty())
            handleURIOrFile(fileEvent->url().toString());

        return true;
    }

    return QObject::eventFilter(object, event);
}

void PaymentServer::uiReady()
{
#ifdef ENABLE_BIP70
    initNetManager();
#endif

    saveURIs = false;
    for (const QString& s : savedPaymentRequests)
    {
        handleURIOrFile(s);
    }
    savedPaymentRequests.clear();
}

void PaymentServer::handleURIOrFile(const QString& s)
{
    if (saveURIs)
    {
        savedPaymentRequests.append(s);
        return;
    }

if (s.startsWith("bitcoin://“，qt：：不区分大小写）
    {
Q_EMIT message(tr("URI handling"), tr("'bitcoin://'不是有效的URI。使用“比特币：”代替。“），
            CClientUIInterface::MSG_ERROR);
    }
else if (s.startsWith(BITCOIN_IPC_PREFIX, Qt::CaseInsensitive)) //比特币：URI
    {
        QUrlQuery uri((QUrl(s)));
if (uri.hasQueryItem("r")) //付款请求URI
        {
#ifdef ENABLE_BIP70
            Q_EMIT message(tr("URI handling"),
                tr("You are using a BIP70 URL which will be unsupported in the future."),
                CClientUIInterface::ICON_WARNING);
            QByteArray temp;
            temp.append(uri.queryItemValue("r"));
            QString decoded = QUrl::fromPercentEncoding(temp);
            QUrl fetchUrl(decoded, QUrl::StrictMode);

            if (fetchUrl.isValid())
            {
                qDebug() << "PaymentServer::handleURIOrFile: fetchRequest(" << fetchUrl << ")";
                fetchRequest(fetchUrl);
            }
            else
            {
                qWarning() << "PaymentServer::handleURIOrFile: Invalid URL: " << fetchUrl;
                Q_EMIT message(tr("URI handling"),
                    tr("Payment request fetch URL is invalid: %1").arg(fetchUrl.toString()),
                    CClientUIInterface::ICON_WARNING);
            }
#else
            Q_EMIT message(tr("URI handling"),
                tr("Cannot process payment request because BIP70 support was not compiled in."),
                CClientUIInterface::ICON_WARNING);
#endif
            return;
        }
else //正常URI
        {
            SendCoinsRecipient recipient;
            if (GUIUtil::parseBitcoinURI(s, &recipient))
            {
                if (!IsValidDestinationString(recipient.address.toStdString())) {
                    Q_EMIT message(tr("URI handling"), tr("Invalid payment address %1").arg(recipient.address),
                        CClientUIInterface::MSG_ERROR);
                }
                else
                    Q_EMIT receivedPaymentRequest(recipient);
            }
            else
                Q_EMIT message(tr("URI handling"),
                    tr("URI cannot be parsed! This can be caused by an invalid Bitcoin address or malformed URI parameters."),
                    CClientUIInterface::ICON_WARNING);

            return;
        }
    }

#ifdef ENABLE_BIP70
if (QFile::exists(s)) //付款请求文件
    {
        PaymentRequestPlus request;
        SendCoinsRecipient recipient;
        if (!readPaymentRequestFromFile(s, request))
        {
            Q_EMIT message(tr("Payment request file handling"),
                tr("Payment request file cannot be read! This can be caused by an invalid payment request file."),
                CClientUIInterface::ICON_WARNING);
        }
        else if (processPaymentRequest(request, recipient))
            Q_EMIT receivedPaymentRequest(recipient);

        return;
    }
#endif
}

void PaymentServer::handleURIConnection()
{
    QLocalSocket *clientConnection = uriServer->nextPendingConnection();

    while (clientConnection->bytesAvailable() < (int)sizeof(quint32))
        clientConnection->waitForReadyRead();

    connect(clientConnection, &QLocalSocket::disconnected, clientConnection, &QLocalSocket::deleteLater);

    QDataStream in(clientConnection);
    in.setVersion(QDataStream::Qt_4_0);
    if (clientConnection->bytesAvailable() < (int)sizeof(quint16)) {
        return;
    }
    QString msg;
    in >> msg;

    handleURIOrFile(msg);
}

void PaymentServer::setOptionsModel(OptionsModel *_optionsModel)
{
    this->optionsModel = _optionsModel;
}

#ifdef ENABLE_BIP70
struct X509StoreDeleter {
      void operator()(X509_STORE* b) {
          X509_STORE_free(b);
      }
};

struct X509Deleter {
      void operator()(X509* b) { X509_free(b); }
};

namespace //Anon命名空间
{
    std::unique_ptr<X509_STORE, X509StoreDeleter> certStore;
}

static void ReportInvalidCertificate(const QSslCertificate& cert)
{
    qDebug() << QString("%1: Payment server found an invalid certificate: ").arg(__func__) << cert.serialNumber() << cert.subjectInfo(QSslCertificate::CommonName) << cert.subjectInfo(QSslCertificate::DistinguishedNameQualifier) << cert.subjectInfo(QSslCertificate::OrganizationalUnitName);
}

//
//加载OpenSSL的根证书颁发机构列表
//
void PaymentServer::LoadRootCAs(X509_STORE* _store)
{
//单元测试主要使用这个来传递假根CA：
    if (_store)
    {
        certStore.reset(_store);
        return;
    }

//正常执行，使用-rootcertificates或系统证书：
    certStore.reset(X509_STORE_new());

//注意：使用“-system-”默认值，这样用户就可以通过-rootcertificates=“”
//并得到“我不喜欢X.509证书，不信任任何人”的行为：
    QString certFile = QString::fromStdString(gArgs.GetArg("-rootcertificates", "-system-"));

//空商店
    if (certFile.isEmpty()) {
        qDebug() << QString("PaymentServer::%1: Payment request authentication via X.509 certificates disabled.").arg(__func__);
        return;
    }

    QList<QSslCertificate> certList;

    if (certFile != "-system-") {
            qDebug() << QString("PaymentServer::%1: Using \"%2\" as trusted root certificate.").arg(__func__).arg(certFile);

        certList = QSslCertificate::fromPath(certFile);
//在获取付款请求时也使用这些证书：
        QSslSocket::setDefaultCaCertificates(certList);
    } else
        certList = QSslSocket::systemCaCertificates();

    int nRootCerts = 0;
    const QDateTime currentTime = QDateTime::currentDateTime();

    for (const QSslCertificate& cert : certList) {
//不记录空证书
        if (cert.isNull())
            continue;

//尚未激活/有效，或证书已过期
        if (currentTime < cert.effectiveDate() || currentTime > cert.expiryDate()) {
            ReportInvalidCertificate(cert);
            continue;
        }

//黑名单证书
        if (cert.isBlacklisted()) {
            ReportInvalidCertificate(cert);
            continue;
        }

        QByteArray certData = cert.toDer();
        const unsigned char *data = (const unsigned char *)certData.data();

        std::unique_ptr<X509, X509Deleter> x509(d2i_X509(0, &data, certData.size()));
        if (x509 && X509_STORE_add_cert(certStore.get(), x509.get()))
        {
//注意：x509_store增加了对x509对象的引用计数，
//我们仍然需要发布我们对它的引用。
            ++nRootCerts;
        }
        else
        {
            ReportInvalidCertificate(cert);
            continue;
        }
    }
    qWarning() << "PaymentServer::LoadRootCAs: Loaded " << nRootCerts << " root certificates";

//另一天的计划：
//获取证书吊销列表，并将其添加到证书存储。
//需要考虑的问题：
//性能（启动线程在后台提取？）
//隐私（通过Tor/Proxy获取，以便不显示IP地址）
//仅仅使用黑名单中编译的会更容易吗？
//或者使用qt的黑名单？
//服务器端缓存的“证书装订”更高效
}

void PaymentServer::initNetManager()
{
    if (!optionsModel)
        return;
    delete netManager;

//netmanager用于获取比特币：uris中给出的paymentrequests。
    netManager = new QNetworkAccessManager(this);

    QNetworkProxy proxy;

//查询活动socks5代理
    if (optionsModel->getProxySettings(proxy)) {
        netManager->setProxy(proxy);

        qDebug() << "PaymentServer::initNetManager: Using SOCKS5 proxy" << proxy.hostName() << ":" << proxy.port();
    }
    else
        qDebug() << "PaymentServer::initNetManager: No active proxy server found.";

    connect(netManager, &QNetworkAccessManager::finished, this, &PaymentServer::netRequestFinished);
    connect(netManager, &QNetworkAccessManager::sslErrors, this, &PaymentServer::reportSslErrors);
}

//
//警告：ipcsendcommandline（）中使用了readPaymentRequestFromFile（）。
//所以不要使用“q_emit message（）”，而是使用“qmessagebox:：”！
//
bool PaymentServer::readPaymentRequestFromFile(const QString& filename, PaymentRequestPlus& request)
{
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << QString("PaymentServer::%1: Failed to open %2").arg(__func__).arg(filename);
        return false;
    }

//BIP70 DOS保护
    if (!verifySize(f.size())) {
        return false;
    }

    QByteArray data = f.readAll();

    return request.parse(data);
}

bool PaymentServer::processPaymentRequest(const PaymentRequestPlus& request, SendCoinsRecipient& recipient)
{
    if (!optionsModel)
        return false;

    if (request.IsInitialized()) {
//付款请求网络是否与客户端网络匹配？
        if (!verifyNetwork(optionsModel->node(), request.getDetails())) {
            Q_EMIT message(tr("Payment request rejected"), tr("Payment request network doesn't match client network."),
                CClientUIInterface::MSG_ERROR);

            return false;
        }

//确保涉及的任何付款请求仍然有效。
//在发送WalletModel:：SendCoins（）中的硬币之前，将重新检查此项。
        if (verifyExpired(request.getDetails())) {
            Q_EMIT message(tr("Payment request rejected"), tr("Payment request expired."),
                CClientUIInterface::MSG_ERROR);

            return false;
        }
    } else {
        Q_EMIT message(tr("Payment request error"), tr("Payment request is not initialized."),
            CClientUIInterface::MSG_ERROR);

        return false;
    }

    recipient.paymentRequest = request;
    recipient.message = GUIUtil::HtmlEscape(request.getDetails().memo());

    request.getMerchant(certStore.get(), recipient.authenticatedMerchant);

    QList<std::pair<CScript, CAmount> > sendingTos = request.getPayTo();
    QStringList addresses;

    for (const std::pair<CScript, CAmount>& sendingTo : sendingTos) {
//提取并检查目标地址
        CTxDestination dest;
        if (ExtractDestination(sendingTo.first, dest)) {
//附加目标地址
            addresses.append(QString::fromStdString(EncodeDestination(dest)));
        }
        else if (!recipient.authenticatedMerchant.isEmpty()) {
//不支持对自定义比特币地址的未经身份验证的付款请求
//（没有很好的方法告诉用户他们在哪里付款
//有机会理解）。
            Q_EMIT message(tr("Payment request rejected"),
                tr("Unverified payment requests to custom payment scripts are unsupported."),
                CClientUIInterface::MSG_ERROR);
            return false;
        }

//比特币金额作为（可选）uint64存储在protobuf消息中（请参见paymentrequest.proto）。
//但是camount被定义为int64，因此我们需要验证金额是否在有效范围内。
//没有发生溢出。
        if (!verifyAmount(sendingTo.second)) {
            Q_EMIT message(tr("Payment request rejected"), tr("Invalid payment request."), CClientUIInterface::MSG_ERROR);
            return false;
        }

//提取并检查金额
        CTxOut txOut(sendingTo.second, sendingTo.first);
        if (IsDust(txOut, optionsModel->node().getDustRelayFee())) {
            Q_EMIT message(tr("Payment request error"), tr("Requested payment amount of %1 is too small (considered dust).")
                .arg(BitcoinUnits::formatWithUnit(optionsModel->getDisplayUnit(), sendingTo.second)),
                CClientUIInterface::MSG_ERROR);

            return false;
        }

        recipient.amount += sendingTo.second;
//另外，在添加额外金额后，还要验证最终金额是否仍在有效范围内。
        if (!verifyAmount(recipient.amount)) {
            Q_EMIT message(tr("Payment request rejected"), tr("Invalid payment request."), CClientUIInterface::MSG_ERROR);
            return false;
        }
    }
//存储地址并对其进行格式化，以便与图形用户界面完美匹配。
    recipient.address = addresses.join("<br />");

    if (!recipient.authenticatedMerchant.isEmpty()) {
        qDebug() << "PaymentServer::processPaymentRequest: Secure payment request from " << recipient.authenticatedMerchant;
    }
    else {
        qDebug() << "PaymentServer::processPaymentRequest: Insecure payment request to " << addresses.join(", ");
    }

    return true;
}

void PaymentServer::fetchRequest(const QUrl& url)
{
    QNetworkRequest netRequest;
    netRequest.setAttribute(QNetworkRequest::User, BIP70_MESSAGE_PAYMENTREQUEST);
    netRequest.setUrl(url);
    netRequest.setRawHeader("User-Agent", CLIENT_NAME.c_str());
    netRequest.setRawHeader("Accept", BIP71_MIMETYPE_PAYMENTREQUEST);
    netManager->get(netRequest);
}

void PaymentServer::fetchPaymentACK(WalletModel* walletModel, const SendCoinsRecipient& recipient, QByteArray transaction)
{
    const payments::PaymentDetails& details = recipient.paymentRequest.getDetails();
    if (!details.has_payment_url())
        return;

    QNetworkRequest netRequest;
    netRequest.setAttribute(QNetworkRequest::User, BIP70_MESSAGE_PAYMENTACK);
    netRequest.setUrl(QString::fromStdString(details.payment_url()));
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, BIP71_MIMETYPE_PAYMENT);
    netRequest.setRawHeader("User-Agent", CLIENT_NAME.c_str());
    netRequest.setRawHeader("Accept", BIP71_MIMETYPE_PAYMENTACK);

    payments::Payment payment;
    payment.set_merchant_data(details.merchant_data());
    payment.add_transactions(transaction.data(), transaction.size());

//创建新的退款地址，或重新使用：
    CPubKey newKey;
    /*（walletmodel->wallet（）.getkefrompool（false/*internal*/，newkey））
        //bip70请求直接对scriptpubkey进行编码，因此我们不受地址限制
        //接收器支持的类型。因此，我们选择地址格式
        //用于更改。尽管实际付款没有变化，但这是一个很接近的匹配：
        //这是我们在隐私问题中使用的输出类型，但不受
        //其他软件支持。
        const outputtype change_type=walletmodel->wallet（）.getDefaultChangeType（）！=outputtype：：更改“自动”？walletModel->wallet（）.getDefaultChangeType（）：walletModel->wallet（）.getDefaultAddressType（）；
        walletmodel->wallet（）。学习相关脚本（newkey，更改_类型）；
        ctxdestination dest=getdestinationforkey（newkey，更改类型）；
        std:：string label=tr（“从%1退款”）.arg（recipient.authenticatedMerchant）.tostdstring（）；
        walletmodel->wallet（）.setaddressbook（dest，label，“退款”）；

        cscript s=获取脚本目标（dest）；
        payments：：output*refunder_to=payment.add_refunder_to（）；
        退款_to->set_script（&s[0]，s.size（））；
    }否则{
        //这不应该发生，因为发送硬币应该
        //打开钱包，重新装满钥匙。
        qwarning（）<“paymentserver:：fetchpaymentack:获取退款密钥时出错，退款未设置”；
    }

    int length=payment.bytesize（）；
    netrequest.setheader（qnetworkrequest:：contentlengthheader，length）；
    qbytearray serdata（长度，“\0”）；
    if（payment.serializeToArray（serdata.data（），length））
        netmanager->post（netrequest，serdata）；
    }
    否则{
        //这也不应该发生。
        qwarning（）<“paymentserver:：fetchpaymentack:序列化付款消息时出错”；
    }
}

void paymentserver:：netrequestfinished（qnetworkreply*reply）
{
    reply->deletelater（）；

    //BIP70 DOS保护
    如果（！）verifySize（reply->size（））
        发出消息（tr（“付款请求被拒绝”），
            tr（“付款请求%1太大（允许%2字节，允许%3字节）。”
                .arg（reply->request（）.url（）.toString（））
                .arg（reply->size（））
                .arg（bip70_max_paymentrequest_size），
            cclientuiinterface:：msg_错误）；
        返回；
    }

    如果（reply->error（）！=QNetworkReply:：NoError）
        qstring msg=tr（“与%1通信时出错：%2”）
            .arg（reply->request（）.url（）.toString（））
            .arg（reply->errorString（））；

        qwarning（）<“paymentserver:：netrequestfinished：”<<msg；
        发出消息（tr（“付款请求错误”），msg，cclientuiinterface:：msg_错误）；
        返回；
    }

    qbytearray data=reply->readall（）；

    qString requestType=reply->request（）.attribute（qNetworkRequest:：user）.toString（）；
    如果（requesttype==bip70_message_paymentrequest）
    {
        PaymentRequestPlus请求；
        发送收件人；
        如果（！）请求分析（数据）
        {
            qwarning（）<“paymentserver:：netrequestfinished:分析付款请求时出错”；
            发出消息（tr（“付款请求错误”），
                tr（“无法分析付款请求！”），
                cclientuiinterface:：msg_错误）；
        }
        else if（processPaymentRequest（请求，收件人））
            Q_发出已接收付款请求（收件人）；

        返回；
    }
    else if（requesttype==bip70_message_paymentack）
    {
        付款：paymentack paymentack；
        如果（！）paymentack.parseFromArray（data.data（），data.size（））
        {
            qstring msg=tr（“来自服务器%1的错误响应”）
                .arg（reply->request（）.url（）.toString（））；

            qwarning（）<“paymentserver:：netrequestfinished：”<<msg；
            发出消息（tr（“付款请求错误”），msg，cclientuiinterface:：msg_错误）；
        }
        其他的
        {
            q_emit receivedpaymentack（guiutil:：htmlescape（paymentack.memo（））；
        }
    }
}

void paymentserver:：reportsslerors（qnetworkreply*reply，const qlist<qsslerorr>&errs）
{
    未使用（回复）；

    qString错误字符串；
    对于（const qssleror&err:errs）
        qwarning（）<“paymentserver:：reportsslerors：”<<err；
        errstring+=err.errorstring（）+“”\n“；
    }
    q_emit message（tr（“网络请求错误”），errstring，cclientuiinterface:：msg_错误）；
}

void paymentserver:：handlepaymentack（const qstring&paymentackmsg）
{
    //目前我们不进一步处理或存储paymentack消息
    q_emit message（tr（“已确认付款”），paymentackmsg，cclientuiinterface:：icon_information_cclientuiinterface:：modal）；
}

bool paymentserver:：verifynetwork（接口：：节点和节点，const payments:：paymentdetails和requestdetails）
{
    bool fverified=requestdetails.network（）==node.getnetwork（）；
    如果（！）F核实）{
        qwarning（）<<qstring（“paymentserver:：%1:付款请求网络\”“%2”“与客户端网络\”“%3”“不匹配”）。
            ARG（α-函数）
            .arg（qString:：FromStdString（requestDetails.network（））
            .arg（qString:：FromStdString（node.getNetwork（））；
    }
    已验证的返回值；
}

bool paymentserver:：verifyexpired（const payments:：paymentdetails和requestdetails）
{
    bool fverified=（requestdetails.hasou expires（）&&（int64_t）requestdetails.expires（）<gettime（））；
    如果（验证）
        const qstring requestExpires=qstring:：fromstdstring（formats8601datetime（（int64_t）requestdetails.expires（））；
        qwarning（）<<qstring（“paymentserver:：%1:付款请求已过期\”“%2”“”）
            ARG（α-函数）
            .arg（请求过期）；
    }
    已验证的返回值；
}

Bool PaymentServer:：VerifySize（qint64请求大小）
{
    bool fverified=（requestsize<=bip70_max_paymentrequest_size）；
    如果（！）F核实）{
        qwarning（）<<qstring（“paymentserver:：%1:付款请求太大（%2字节，允许%3字节）。”）
            ARG（α-函数）
            .arg（请求大小）
            .arg（bip70_max_paymentrequest_size）；
    }
    已验证的返回值；
}

bool paymentserver:：verifyamount（const camount&requestamount）
{
    bool fverified=moneyrange（请求金额）；
    如果（！）F核实）{
        qwarning（）<<qstring（“paymentserver:：%1:付款请求金额超出允许范围（%2，允许0-%3）。”
            ARG（α-函数）
            .arg（请求金额）
            .arg（最大金额）；
    }
    已验证的返回值；
}

x509_store*付款服务器：：getcertstore（）
{
    返回certstore.get（）；
}
第二节
