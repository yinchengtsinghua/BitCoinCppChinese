
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

#ifndef BITCOIN_QT_BITCOINGUI_H
#define BITCOIN_QT_BITCOINGUI_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/optionsdialog.h>

#include <amount.h>

#include <QLabel>
#include <QMainWindow>
#include <QMap>
#include <QPoint>
#include <QSystemTrayIcon>

#ifdef Q_OS_MAC
#include <qt/macos_appnap.h>
#endif

#include <memory>

class ClientModel;
class NetworkStyle;
class Notificator;
class OptionsModel;
class PlatformStyle;
class RPCConsole;
class SendCoinsRecipient;
class UnitDisplayStatusBarControl;
class WalletController;
class WalletFrame;
class WalletModel;
class HelpMessageDialog;
class ModalOverlay;

namespace interfaces {
class Handler;
class Node;
}

QT_BEGIN_NAMESPACE
class QAction;
class QComboBox;
class QMenu;
class QProgressBar;
class QProgressDialog;
QT_END_NAMESPACE

namespace GUIUtil {
class ClickableLabel;
class ClickableProgressBar;
}

/*
  比特币GUI主类。此类表示比特币用户界面的主窗口。它与客户机和
  钱包模型，为用户提供当前核心状态的最新视图。
**/

class BitcoinGUI : public QMainWindow
{
    Q_OBJECT

public:
    static const std::string DEFAULT_UIPLATFORM;

    explicit BitcoinGUI(interfaces::Node& node, const PlatformStyle *platformStyle, const NetworkStyle *networkStyle, QWidget *parent = nullptr);
    ~BitcoinGUI();

    /*设置客户端模型。
        客户机模型代表了与P2P网络通信的核心部分，并且与钱包无关。
    **/

    void setClientModel(ClientModel *clientModel);
#ifdef ENABLE_WALLET
    void setWalletController(WalletController* wallet_controller);
#endif

#ifdef ENABLE_WALLET
    /*设置钱包模型。
        钱包模型代表比特币钱包，提供对交易列表、通讯簿和发送的访问。
        功能。
    **/

    void addWallet(WalletModel* walletModel);
    void removeWallet(WalletModel* walletModel);
    void removeAllWallets();
#endif //允许钱包
    bool enableWallet = false;

    /*获取托盘图标状态。
        有些系统没有“系统托盘”或“通知区域”可用。
    **/

    bool hasTrayIcon() const { return trayIcon; }

protected:
    void changeEvent(QEvent *e);
    void closeEvent(QCloseEvent *event);
    void showEvent(QShowEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    bool eventFilter(QObject *object, QEvent *event);

private:
    interfaces::Node& m_node;
    WalletController* m_wallet_controller{nullptr};
    std::unique_ptr<interfaces::Handler> m_handler_message_box;
    std::unique_ptr<interfaces::Handler> m_handler_question;
    ClientModel* clientModel = nullptr;
    WalletFrame* walletFrame = nullptr;

    UnitDisplayStatusBarControl* unitDisplayControl = nullptr;
    QLabel* labelWalletEncryptionIcon = nullptr;
    QLabel* labelWalletHDStatusIcon = nullptr;
    GUIUtil::ClickableLabel* labelProxyIcon = nullptr;
    GUIUtil::ClickableLabel* connectionsControl = nullptr;
    GUIUtil::ClickableLabel* labelBlocksIcon = nullptr;
    QLabel* progressBarLabel = nullptr;
    GUIUtil::ClickableProgressBar* progressBar = nullptr;
    QProgressDialog* progressDialog = nullptr;

    QMenuBar* appMenuBar = nullptr;
    QToolBar* appToolBar = nullptr;
    QAction* overviewAction = nullptr;
    QAction* historyAction = nullptr;
    QAction* quitAction = nullptr;
    QAction* sendCoinsAction = nullptr;
    QAction* sendCoinsMenuAction = nullptr;
    QAction* usedSendingAddressesAction = nullptr;
    QAction* usedReceivingAddressesAction = nullptr;
    QAction* signMessageAction = nullptr;
    QAction* verifyMessageAction = nullptr;
    QAction* aboutAction = nullptr;
    QAction* receiveCoinsAction = nullptr;
    QAction* receiveCoinsMenuAction = nullptr;
    QAction* optionsAction = nullptr;
    QAction* toggleHideAction = nullptr;
    QAction* encryptWalletAction = nullptr;
    QAction* backupWalletAction = nullptr;
    QAction* changePassphraseAction = nullptr;
    QAction* aboutQtAction = nullptr;
    QAction* openRPCConsoleAction = nullptr;
    QAction* openAction = nullptr;
    QAction* showHelpMessageAction = nullptr;
    QAction* m_wallet_selector_label_action = nullptr;
    QAction* m_wallet_selector_action = nullptr;

    QLabel *m_wallet_selector_label = nullptr;
    QComboBox* m_wallet_selector = nullptr;

    QSystemTrayIcon* trayIcon = nullptr;
    const std::unique_ptr<QMenu> trayIconMenu;
    Notificator* notificator = nullptr;
    RPCConsole* rpcConsole = nullptr;
    HelpMessageDialog* helpMessageDialog = nullptr;
    ModalOverlay* modalOverlay = nullptr;

#ifdef Q_OS_MAC
    CAppNapInhibitor* m_app_nap_inhibitor = nullptr;
#endif

    /*跟踪以前的块数，以检测进度*/
    int prevBlocks = 0;
    int spinnerFrame = 0;

    const PlatformStyle *platformStyle;
    const NetworkStyle* const m_network_style;

    /*创建主UI操作。*/
    void createActions();
    /*创建菜单栏和子菜单。*/
    void createMenuBar();
    /*创建工具栏*/
    void createToolBars();
    /*创建系统托盘图标和通知*/
    void createTrayIcon();
    /*创建系统托盘菜单（或设置停靠菜单）*/
    void createTrayIconMenu();

    /*启用或禁用所有与钱包相关的操作*/
    void setWalletActionsEnabled(bool enabled);

    /*将核心信号连接到GUI客户端*/
    void subscribeToCoreSignals();
    /*从GUI客户端断开核心信号*/
    void unsubscribeFromCoreSignals();

    /*使用模型中的最新网络信息更新用户界面。*/
    void updateNetworkState();

    void updateHeadersSyncProgressLabel();

    /*打开指定选项卡索引上的选项对话框*/
    void openOptionsDialogWithTab(OptionsDialog::Tab tab);

Q_SIGNALS:
    /*输入或拖动到GUI时引发的信号*/
    void receivedURI(const QString &uri);
    /*显示RPC控制台时引发的信号*/
    void consoleShown(RPCConsole* console);

public Q_SLOTS:
    /*设置用户界面中显示的连接数*/
    void setNumConnections(int count);
    /*设置用户界面中显示的网络状态*/
    void setNetworkActive(bool networkActive);
    /*设置用户界面中显示的块数和最后一个块日期*/
    void setNumBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, bool headers);

    /*从核心网络或事务处理代码通知用户事件。
       @param[in]标题消息框/通知标题
       @param[in]消息显示的文本
       @param[in]样式形式和样式定义（图标和使用的按钮-仅用于消息框的按钮）
                            @请参见cclientuiinterface:：messageboxflags
       @param[in]ret指向bool的指针，该bool将被修改为是否单击了OK（仅限模式）
    **/

    void message(const QString &title, const QString &message, unsigned int style, bool *ret = nullptr);

#ifdef ENABLE_WALLET
    void setCurrentWallet(WalletModel* wallet_model);
    void setCurrentWalletBySelectorIndex(int index);
    /*根据当前选定的钱包设置用户界面状态指示器。
    **/

    void updateWalletStatus();

private:
    /*设置加密状态，如用户界面所示。
       @param[in]状态当前加密状态
       @参见walletmodel:：encryptionstatus
    **/

    void setEncryptionStatus(int status);

    /*设置HD启用状态，如用户界面所示。
     @param[in]hd启用当前hd启用状态
     @参见walletmodel:：encryptionstatus
     **/

    void setHDStatus(bool privkeyDisabled, int hdEnabled);

public Q_SLOTS:
    bool handlePaymentRequest(const SendCoinsRecipient& recipient);

    /*显示新事务的传入事务通知。*/
    void incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address, const QString& label, const QString& walletName);
#endif //允许钱包

private:
    /*设置代理启用图标，如用户界面中所示。*/
    void updateProxyIcon();
    void updateWindowTitle();

public Q_SLOTS:
#ifdef ENABLE_WALLET
    /*切换到概述（主页）页*/
    void gotoOverviewPage();
    /*切换到历史记录（事务）页*/
    void gotoHistoryPage();
    /*切换到接收硬币页面*/
    void gotoReceiveCoinsPage();
    /*切换到发送硬币页面*/
    void gotoSendCoinsPage(QString addr = "");

    /*显示签名/验证消息对话框并切换到签名消息选项卡*/
    void gotoSignMessageTab(QString addr = "");
    /*显示签名/验证消息对话框并切换到验证消息选项卡*/
    void gotoVerifyMessageTab(QString addr = "");

    /*显示打开的对话框*/
    void openClicked();
#endif //允许钱包
    /*显示配置对话框*/
    void optionsClicked();
    /*显示关于对话框*/
    void aboutClicked();
    /*显示调试窗口*/
    void showDebugWindow();
    /*显示调试窗口并将焦点设置到控制台*/
    void showDebugWindowActivateConsole();
    /*显示帮助消息对话框*/
    void showHelpMessageClicked();
#ifndef Q_OS_MAC
    /*已单击句柄托盘图标*/
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
#else
    /*已单击处理MacOS Dock图标*/
    void macosDockIconActivated();
#endif

    /*隐藏时显示窗口，最小化时取消最小化，隐藏时上升，或隐藏时显示，并且ftoggleHidden为真*/
    void showNormalIfMinimized() { showNormalIfMinimized(false); }
    void showNormalIfMinimized(bool fToggleHidden);
    /*只需调用showNormalIFMinimized（true）在slot（）宏中使用*/
    void toggleHidden();

    /*由计时器调用以检查是否已设置shutdownrequested（）。*/
    void detectShutdown();

    /*显示进度对话框，例如用于VerifyChain*/
    void showProgress(const QString &title, int nProgress);

    /*当在选项中更改hidetrayicon设置时，隐藏或显示相应的图标。*/
    void setTrayIconVisible(bool);

    void showModalOverlay();
};

class UnitDisplayStatusBarControl : public QLabel
{
    Q_OBJECT

public:
    explicit UnitDisplayStatusBarControl(const PlatformStyle *platformStyle);
    /*让控件知道选项模型（及其信号）*/
    void setOptionsModel(OptionsModel *optionsModel);

protected:
    /*以便它响应左键单击*/
    void mousePressEvent(QMouseEvent *event);

private:
    OptionsModel *optionsModel;
    QMenu* menu;

    /*按鼠标坐标显示带有显示单位选项的上下文菜单*/
    void onDisplayUnitsClicked(const QPoint& point);
    /*创建上下文菜单及其操作，并连接鼠标事件的所有相关信号。*/
    void createContextMenu();

private Q_SLOTS:
    /*当选项“Model”上的显示单位发生更改时，它将刷新状态栏上控件的显示文本。*/
    void updateDisplayUnit(int newUnits);
    /*通知基础选项Model更新其当前显示单元。*/
    void onMenuSelection(QAction* action);
};

#endif //比特币qt
