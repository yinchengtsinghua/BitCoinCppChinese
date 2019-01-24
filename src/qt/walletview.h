
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

#ifndef BITCOIN_QT_WALLETVIEW_H
#define BITCOIN_QT_WALLETVIEW_H

#include <amount.h>

#include <QStackedWidget>

class BitcoinGUI;
class ClientModel;
class OverviewPage;
class PlatformStyle;
class ReceiveCoinsDialog;
class SendCoinsDialog;
class SendCoinsRecipient;
class TransactionView;
class WalletModel;
class AddressBookPage;

QT_BEGIN_NAMESPACE
class QModelIndex;
class QProgressDialog;
QT_END_NAMESPACE

/*
  WalletView类。此类表示单个钱包的视图。
  它被添加以支持多个钱包功能。每个钱包都有自己的walletview实例。
  它与客户机和钱包模型进行通信，为用户提供最新的
  当前核心状态。
**/

class WalletView : public QStackedWidget
{
    Q_OBJECT

public:
    explicit WalletView(const PlatformStyle *platformStyle, QWidget *parent);
    ~WalletView();

    void setBitcoinGUI(BitcoinGUI *gui);
    /*设置客户端模型。
        客户机模型代表了与P2P网络通信的核心部分，并且与钱包无关。
    **/

    void setClientModel(ClientModel *clientModel);
    WalletModel *getWalletModel() { return walletModel; }
    /*设置钱包模型。
        钱包模型代表比特币钱包，提供对交易列表、通讯簿和发送的访问。
        功能。
    **/

    void setWalletModel(WalletModel *walletModel);

    bool handlePaymentRequest(const SendCoinsRecipient& recipient);

    void showOutOfSyncWarning(bool fShow);

private:
    ClientModel *clientModel;
    WalletModel *walletModel;

    OverviewPage *overviewPage;
    QWidget *transactionsPage;
    ReceiveCoinsDialog *receiveCoinsPage;
    SendCoinsDialog *sendCoinsPage;
    AddressBookPage *usedSendingAddressesPage;
    AddressBookPage *usedReceivingAddressesPage;

    TransactionView *transactionView;

    QProgressDialog *progressDialog;
    const PlatformStyle *platformStyle;

public Q_SLOTS:
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

    /*显示新事务的传入事务通知。

        新项是在给定父项下开始和结束（包括开始和结束）之间的项。
    **/

    /*d processnewtransaction（const qmodelindex&parent，int start，int/*end*/）；
    /**加密钱包*/

    void encryptWallet(bool status);
    /*备份钱包*/
    void backupWallet();
    /*更改加密钱包密码*/
    void changePassphrase();
    /*请求密码临时解锁钱包*/
    void unlockWallet();

    /*显示已用的发送地址*/
    void usedSendingAddresses();
    /*显示已用的接收地址*/
    void usedReceivingAddresses();

    /*重新发出加密状态信号*/
    void updateEncryptionStatus();

    /*显示进度对话框，例如用于重新扫描*/
    void showProgress(const QString &title, int nProgress);

    /*用户已请求有关不同步状态的详细信息*/
    void requestedSyncWarningInfo();

Q_SIGNALS:
    /*显示主窗口的信号*/
    void showNormalIfMinimized();
    /*在应向用户报告消息时激发*/
    void message(const QString &title, const QString &message, unsigned int style);
    /*钱包加密状态已更改*/
    void encryptionStatusChanged();
    /*钱包的高清启用状态已更改（仅在启动时可用）*/
    void hdEnabledStatusChanged();
    /*通知出现新事务*/
    void incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address, const QString& label, const QString& walletName);
    /*通知已按下不同步警告图标*/
    void outOfSyncWarningClicked();
};

#endif //比特币qt-钱包视图
