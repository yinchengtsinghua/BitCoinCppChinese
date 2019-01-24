
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

#ifndef BITCOIN_QT_WALLETFRAME_H
#define BITCOIN_QT_WALLETFRAME_H

#include <QFrame>
#include <QMap>

class BitcoinGUI;
class ClientModel;
class PlatformStyle;
class SendCoinsRecipient;
class WalletModel;
class WalletView;

QT_BEGIN_NAMESPACE
class QStackedWidget;
QT_END_NAMESPACE

/*
 *用于嵌入所有与钱包相关的容器
 *控制比特币。这门课的目的是为了将来
 *钱包控制装置的改进，无需进一步改进。
 *修改比特币图形用户界面，从而大大简化合并，同时
 *降低破坏顶级产品的风险。
 **/

class WalletFrame : public QFrame
{
    Q_OBJECT

public:
    explicit WalletFrame(const PlatformStyle *platformStyle, BitcoinGUI *_gui = nullptr);
    ~WalletFrame();

    void setClientModel(ClientModel *clientModel);

    bool addWallet(WalletModel *walletModel);
    bool setCurrentWallet(WalletModel* wallet_model);
    bool removeWallet(WalletModel* wallet_model);
    void removeAllWallets();

    bool handlePaymentRequest(const SendCoinsRecipient& recipient);

    void showOutOfSyncWarning(bool fShow);

Q_SIGNALS:
    /*通知用户已请求有关不同步警告的详细信息*/
    void requestedSyncWarningInfo();

private:
    QStackedWidget *walletStack;
    BitcoinGUI *gui;
    ClientModel *clientModel;
    QMap<WalletModel*, WalletView*> mapWalletViews;

    bool bOutOfSync;

    const PlatformStyle *platformStyle;

public:
    WalletView* currentWalletView() const;
    WalletModel* currentWalletModel() const;

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

    /*加密钱包*/
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
    /*传递信号超过请求的不同步警告信息*/
    void outOfSyncWarningClicked();
};

#endif //比特币qt_钱包框架
