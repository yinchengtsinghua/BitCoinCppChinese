
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

#ifndef BITCOIN_QT_SPLASHSCREEN_H
#define BITCOIN_QT_SPLASHSCREEN_H

#include <QWidget>

#include <memory>

class NetworkStyle;

namespace interfaces {
class Handler;
class Node;
class Wallet;
};

/*类，其中包含正在运行的客户端的信息。
 *
 *@注意这不是一个qsplashscreen。比特币核心初始化
 *可能需要很长时间，在这种情况下，进度窗口不能
 *移动和最小化对用户来说是令人沮丧的。
 **/

class SplashScreen : public QWidget
{
    Q_OBJECT

public:
    explicit SplashScreen(interfaces::Node& node, Qt::WindowFlags f, const NetworkStyle *networkStyle);
    ~SplashScreen();

protected:
    void paintEvent(QPaintEvent *event);
    void closeEvent(QCloseEvent *event);

public Q_SLOTS:
    /*隐藏“启动屏幕”窗口并计划删除启动屏幕对象*/
    void finish();

    /*显示消息和进度*/
    void showMessage(const QString &message, int alignment, const QColor &color);

protected:
    bool eventFilter(QObject * obj, QEvent * ev);

private:
    /*将核心信号连接到启动屏幕*/
    void subscribeToCoreSignals();
    /*断开核心信号至防溅屏*/
    void unsubscribeFromCoreSignals();
    /*将钱包信号连接到闪屏*/
    void ConnectWallet(std::unique_ptr<interfaces::Wallet> wallet);

    QPixmap pixmap;
    QString curMessage;
    QColor curColor;
    int curAlignment;

    interfaces::Node& m_node;
    std::unique_ptr<interfaces::Handler> m_handler_init_message;
    std::unique_ptr<interfaces::Handler> m_handler_show_progress;
    std::unique_ptr<interfaces::Handler> m_handler_load_wallet;
    std::list<std::unique_ptr<interfaces::Wallet>> m_connected_wallets;
    std::list<std::unique_ptr<interfaces::Handler>> m_connected_wallet_handlers;
};

#endif //比特币夸脱飞溅屏
