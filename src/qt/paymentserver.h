
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

#ifndef BITCOIN_QT_PAYMENTSERVER_H
#define BITCOIN_QT_PAYMENTSERVER_H

//此类处理来自单击的付款请求
//比特币：URI
//
//这有点棘手，因为我们必须处理
//用户在
//启动/初始化，启动屏幕打开时
//但是主窗口（和发送硬币选项卡）不是。
//
//因此，战略是：
//
//创建服务器并注册事件处理程序，
//创建应用程序时。保存任何URI
//在列表中的启动时或启动期间收到。
//
//当启动完成且主窗口
//如图所示，信号被发送到插槽uiready（），其中
//为任何付款发出receiveDuri（）信号
//启动期间发生的请求。
//
//启动后，ReceiveDuri（）像往常一样发生。
//
//这个类还有一个特性：静态
//查找在命令行中传递的URI的方法
//如果一个服务器正在另一个进程中运行，
//将它们发送到服务器。
//

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#ifdef ENABLE_BIP70
#include <qt/paymentrequestplus.h>
#endif
#include <qt/walletmodel.h>

#include <QObject>
#include <QString>

class OptionsModel;

QT_BEGIN_NAMESPACE
class QApplication;
class QByteArray;
class QLocalServer;
class QNetworkAccessManager;
class QNetworkReply;
class QSslError;
class QUrl;
QT_END_NAMESPACE

//bip70最大付款请求大小（字节）（DOS保护）
static const qint64 BIP70_MAX_PAYMENTREQUEST_SIZE = 50000;

class PaymentServer : public QObject
{
    Q_OBJECT

public:
//在命令行上分析URI
//出错时返回false
    static void ipcParseCommandLine(interfaces::Node& node, int argc, char *argv[]);

//如果命令行上有uri，则返回true
//已成功发送到正在运行的
//过程。
//注意：如果给出了付款请求，请选择参数（main/testnet）
//将被调用，因此我们以正确的模式启动。
    static bool ipcSendCommandLine();

//父级应为Qapplication对象
    explicit PaymentServer(QObject* parent, bool startLocalServer = true);
    ~PaymentServer();

//OptionsModel用于获取代理设置和显示单元
    void setOptionsModel(OptionsModel *optionsModel);

#ifdef ENABLE_BIP70
//加载根证书颁发机构。传递nullptr（默认）
//要从-rootcertificates设置中指定的文件读取，
//或者，如果没有设置，使用系统默认的根证书。
//如果你经过一家商店，你不应该把它从x509商店中解放出来：它会是
//在退出或加载另一组CA时释放。
    static void LoadRootCAs(X509_STORE* store = nullptr);

//返回证书存储
    static X509_STORE* getCertStore();

//验证付款请求网络是否与客户端网络匹配
    static bool verifyNetwork(interfaces::Node& node, const payments::PaymentDetails& requestDetails);
//验证付款请求是否过期
    static bool verifyExpired(const payments::PaymentDetails& requestDetails);
//根据BIP70验证付款请求大小是否有效
    static bool verifySize(qint64 requestSize);
//验证付款请求金额是否有效
    static bool verifyAmount(const CAmount& requestAmount);
#endif

Q_SIGNALS:
//收到有效的付款请求时激发
    void receivedPaymentRequest(SendCoinsRecipient);

//在应向用户报告消息时激发
    void message(const QString &title, const QString &message, unsigned int style);

#ifdef ENABLE_BIP70
//收到有效的paymentack时激发
    void receivedPaymentACK(const QString &paymentACKMsg);
#endif

public Q_SLOTS:
//当主窗口的UI准备就绪时发出此信号
//向用户显示付款请求
    void uiReady();

//使用本地文件方案或文件处理传入的uri、uri
    void handleURIOrFile(const QString& s);

#ifdef ENABLE_BIP70
//向商户提交支付信息，收回付款确认：
    void fetchPaymentACK(WalletModel* walletModel, const SendCoinsRecipient& recipient, QByteArray transaction);
#endif

private Q_SLOTS:
    void handleURIConnection();
#ifdef ENABLE_BIP70
    void netRequestFinished(QNetworkReply*);
    void reportSslErrors(QNetworkReply*, const QList<QSslError> &);
    void handlePaymentACK(const QString& paymentACKMsg);
#endif

protected:
//构造函数在父qapplication上将此注册到
//接收qevent:：fileopen和qevent:drop事件
    bool eventFilter(QObject *object, QEvent *event);

private:
bool saveURIs;                      //启动时为真
    QLocalServer* uriServer;
    OptionsModel *optionsModel;

#ifdef ENABLE_BIP70
    static bool readPaymentRequestFromFile(const QString& filename, PaymentRequestPlus& request);
    bool processPaymentRequest(const PaymentRequestPlus& request, SendCoinsRecipient& recipient);
    void fetchRequest(const QUrl& url);

//设置网络
    void initNetManager();
QNetworkAccessManager* netManager;  //用于获取付款请求
#endif
};

#endif //比特币qt支付服务器
