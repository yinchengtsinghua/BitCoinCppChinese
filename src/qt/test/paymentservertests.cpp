
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <qt/test/paymentservertests.h>

#include <qt/optionsmodel.h>
#include <qt/test/paymentrequestdata.h>

#include <amount.h>
#include <chainparams.h>
#include <interfaces/node.h>
#include <random.h>
#include <script/script.h>
#include <script/standard.h>
#include <util/system.h>
#include <util/strencodings.h>

#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#include <QFileOpenEvent>
#include <QTemporaryFile>

X509 *parse_b64der_cert(const char* cert_data)
{
    std::vector<unsigned char> data = DecodeBase64(cert_data);
    assert(data.size() > 0);
    const unsigned char* dptr = data.data();
    X509 *cert = d2i_X509(nullptr, &dptr, data.size());
    assert(cert);
    return cert;
}

//
//测试付款请求处理
//

static SendCoinsRecipient handleRequest(PaymentServer* server, std::vector<unsigned char>& data)
{
    RecipientCatcher sigCatcher;
    QObject::connect(server, &PaymentServer::receivedPaymentRequest,
        &sigCatcher, &RecipientCatcher::getRecipient);

//将数据写入临时文件：
    QTemporaryFile f;
    f.open();
    f.write((const char*)data.data(), data.size());
    f.close();

//创建QObject，从PaymentServer安装事件筛选器
//并向对象发送文件打开事件
    QObject object;
    object.installEventFilter(server);
    QFileOpenEvent event(f.fileName());
//如果发送事件失败，将导致SigCatcher为空，
//无论如何都会导致测试失败。
    QCoreApplication::sendEvent(&object, &event);

    QObject::disconnect(server, &PaymentServer::receivedPaymentRequest,
        &sigCatcher, &RecipientCatcher::getRecipient);

//从SigCatcher返回结果
    return sigCatcher.recipient;
}

void PaymentServerTests::paymentServerTests()
{
    SelectParams(CBaseChainParams::MAIN);
    auto node = interfaces::MakeNode();
    OptionsModel optionsModel(*node);
    PaymentServer* server = new PaymentServer(nullptr, false);
    X509_STORE* caStore = X509_STORE_new();
    X509_STORE_add_cert(caStore, parse_b64der_cert(caCert1_BASE64));
    PaymentServer::LoadRootCAs(caStore);
    server->setOptionsModel(&optionsModel);
    server->uiReady();

    std::vector<unsigned char> data;
    SendCoinsRecipient r;
    QString merchant;

//现在将paymentrequests发送到服务器，并观察它产生的信号

//此付款请求直接针对
//CACERT1证书颁发机构：
    data = DecodeBase64(paymentrequest1_cert1_BASE64);
    r = handleRequest(server, data);
    r.paymentRequest.getMerchant(caStore, merchant);
    QCOMPARE(merchant, QString("testmerchant.org"));

//请求中已签署但已过期的商户证书：
    data = DecodeBase64(paymentrequest2_cert1_BASE64);
    r = handleRequest(server, data);
    r.paymentRequest.getMerchant(caStore, merchant);
    QCOMPARE(merchant, QString(""));

//10长证书链，所有中间体有效：
    data = DecodeBase64(paymentrequest3_cert1_BASE64);
    r = handleRequest(server, data);
    r.paymentRequest.getMerchant(caStore, merchant);
    QCOMPARE(merchant, QString("testmerchant8.org"));

//长证书链，中间有过期证书：
    data = DecodeBase64(paymentrequest4_cert1_BASE64);
    r = handleRequest(server, data);
    r.paymentRequest.getMerchant(caStore, merchant);
    QCOMPARE(merchant, QString(""));

//有效签署，但由不在根CA列表中的CA签署：
    data = DecodeBase64(paymentrequest5_cert1_BASE64);
    r = handleRequest(server, data);
    r.paymentRequest.getMerchant(caStore, merchant);
    QCOMPARE(merchant, QString(""));

//在没有根CA的情况下重试，verifiedmerchant应为空：
    caStore = X509_STORE_new();
    PaymentServer::LoadRootCAs(caStore);
    data = DecodeBase64(paymentrequest1_cert1_BASE64);
    r = handleRequest(server, data);
    r.paymentRequest.getMerchant(caStore, merchant);
    QCOMPARE(merchant, QString(""));

//加载第二个根证书
    caStore = X509_STORE_new();
    X509_STORE_add_cert(caStore, parse_b64der_cert(caCert2_BASE64));
    PaymentServer::LoadRootCAs(caStore);

    QByteArray byteArray;

//对于下面的测试，我们只需要
//paymentRequestData.h已解析+存储在r.paymentRequest中。
//
//这些测试要求我们绕过以下正常的客户端执行流
//下面显示的是能够显式地触发某个条件！
//
//手数（）
//->PaymentServer:：EventFilter（）。
//->付款服务器：：handleuriorfile（）
//->PaymentServer:：readPaymentRequestFromFile（）。
//->PaymentServer:：ProcessPaymentRequest（）。

//包含一个testnet paytoAddress，因此付款请求网络与客户端网络不匹配：
    data = DecodeBase64(paymentrequest1_cert2_BASE64);
    byteArray = QByteArray((const char*)data.data(), data.size());
    r.paymentRequest.parse(byteArray);
//确保请求已初始化，因为网络“main”是默认值，即使对于
//未初始化的付款请求，将在此处测试失败。
    QVERIFY(r.paymentRequest.IsInitialized());
    QCOMPARE(PaymentServer::verifyNetwork(*node, r.paymentRequest.getDetails()), false);

//过期付款请求（过期设置为1=1970-01-01 00:00:01）：
    data = DecodeBase64(paymentrequest2_cert2_BASE64);
    byteArray = QByteArray((const char*)data.data(), data.size());
    r.paymentRequest.parse(byteArray);
//确保请求已初始化
    QVERIFY(r.paymentRequest.IsInitialized());
//比较1<getTime（）==false（视为过期付款请求）
    QCOMPARE(PaymentServer::verifyExpired(r.paymentRequest.getDetails()), true);

//未过期的付款请求（Expires设置为0x7fffffffffffffffffff=max.int64_t）：
//9223372036854775807（uint64）、9223372036854775807（int64_t）和-1（int32_t）
//-1是1969-12-31 23:59:59（对于32位时间值）
    data = DecodeBase64(paymentrequest3_cert2_BASE64);
    byteArray = QByteArray((const char*)data.data(), data.size());
    r.paymentRequest.parse(byteArray);
//确保请求已初始化
    QVERIFY(r.paymentRequest.IsInitialized());
//比较9223372036854775807<getTime（）==false（视为未过期的付款请求）
    QCOMPARE(PaymentServer::verifyExpired(r.paymentRequest.getDetails()), false);

//未过期的付款请求（Expires设置为0x80000000000000>max.int64_t，allowed uint64）：
//9223372036854775808（uint64），-9223372036854775808（int64_t）和0（int32_t）
//0是1970-01-01 00:00:00（对于32位时间值）
    data = DecodeBase64(paymentrequest4_cert2_BASE64);
    byteArray = QByteArray((const char*)data.data(), data.size());
    r.paymentRequest.parse(byteArray);
//确保请求已初始化
    QVERIFY(r.paymentRequest.IsInitialized());
//比较-9223372036854775808<getTime（）==true（视为过期付款请求）
    QCOMPARE(PaymentServer::verifyExpired(r.paymentRequest.getDetails()), true);

//测试BIP70 DOS保护：
    unsigned char randData[BIP70_MAX_PAYMENTREQUEST_SIZE + 1];
    GetRandBytes(randData, sizeof(randData));
//将数据写入临时文件：
    QTemporaryFile tempFile;
    tempFile.open();
    tempFile.write((const char*)randData, sizeof(randData));
    tempFile.close();
//比较50001<=bip70_max_paymentrequest_size==false
    QCOMPARE(PaymentServer::verifySize(tempFile.size()), false);

//金额溢出的付款请求（金额设置为2100001btc）：
    data = DecodeBase64(paymentrequest5_cert2_BASE64);
    byteArray = QByteArray((const char*)data.data(), data.size());
    r.paymentRequest.parse(byteArray);
//确保请求已初始化
    QVERIFY(r.paymentRequest.IsInitialized());
//从请求中提取地址和金额
    QList<std::pair<CScript, CAmount> > sendingTos = r.paymentRequest.getPayTo();
    for (const std::pair<CScript, CAmount>& sendingTo : sendingTos) {
        CTxDestination dest;
        if (ExtractDestination(sendingTo.first, dest))
            QCOMPARE(PaymentServer::verifyAmount(sendingTo.second), false);
    }

    delete server;
}

void RecipientCatcher::getRecipient(const SendCoinsRecipient& r)
{
    recipient = r;
}
