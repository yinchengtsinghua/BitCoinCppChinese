
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011-2016比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_QT_BITCOIN_H
#define BITCOIN_QT_BITCOIN_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <QApplication>
#include <memory>
#include <vector>

class BitcoinGUI;
class ClientModel;
class NetworkStyle;
class OptionsModel;
class PaymentServer;
class PlatformStyle;
class WalletController;
class WalletModel;

namespace interfaces {
class Handler;
class Node;
} //命名空间接口

/*类封装比特币核心的启动和关闭。
 *允许在与UI线程不同的线程中运行启动和关闭。
 **/

class BitcoinCore: public QObject
{
    Q_OBJECT
public:
    explicit BitcoinCore(interfaces::Node& node);

public Q_SLOTS:
    void initialize();
    void shutdown();

Q_SIGNALS:
    void initializeResult(bool success);
    void shutdownResult();
    void runawayException(const QString &message);

private:
///pass致命异常消息到ui线程
    void handleRunawayException(const std::exception *e);

    interfaces::Node& m_node;
};

/*主比特币应用程序对象*/
class BitcoinApplication: public QApplication
{
    Q_OBJECT
public:
    explicit BitcoinApplication(interfaces::Node& node, int &argc, char **argv);
    ~BitcoinApplication();

#ifdef ENABLE_WALLET
///创建付款服务器
    void createPaymentServer();
#endif
///基于规则的参数交互/设置
    void parameterSetup();
///创建选项模型
    void createOptionsModel(bool resetSettings);
///创建主窗口
    void createWindow(const NetworkStyle *networkStyle);
///创建初始屏幕
    void createSplashScreen(const NetworkStyle *networkStyle);
///basic初始化，在启动初始化/关闭线程之前。成功后返回true。
    bool baseInitialize();

///请求核心初始化
    void requestInitialize();
///请求核心关闭
    void requestShutdown();

///get进程返回值
    int getReturnValue() const { return returnValue; }

///get qmainwindow的窗口标识符（bitcoingui）
    WId getMainWinId() const;

///SETUP平台样式
    void setupPlatformStyle();

public Q_SLOTS:
    void initializeResult(bool success);
    void shutdownResult();
///处理失控异常。显示有问题的消息框并退出程序。
    void handleRunawayException(const QString &message);

Q_SIGNALS:
    void requestedInitialize();
    void requestedShutdown();
    void splashFinished();
    void windowShown(BitcoinGUI* window);

private:
    QThread *coreThread;
    interfaces::Node& m_node;
    OptionsModel *optionsModel;
    ClientModel *clientModel;
    BitcoinGUI *window;
    QTimer *pollShutdownTimer;
#ifdef ENABLE_WALLET
    PaymentServer* paymentServer{nullptr};
    WalletController* m_wallet_controller{nullptr};
#endif
    int returnValue;
    const PlatformStyle *platformStyle;
    std::unique_ptr<QWidget> shutdownWindow;

    void startThread();
};

int GuiMain(int argc, char* argv[]);

#endif //比特币qt
