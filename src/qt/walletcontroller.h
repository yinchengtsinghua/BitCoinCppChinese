
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2019比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_QT_WALLETCONTROLLER_H
#define BITCOIN_QT_WALLETCONTROLLER_H

#include <qt/walletmodel.h>
#include <sync.h>

#include <list>
#include <memory>
#include <vector>

#include <QMutex>

class OptionsModel;
class PlatformStyle;

namespace interfaces {
class Handler;
class Node;
} //命名空间接口

/*
 *接口：：节点、WalletModel实例和GUI之间的控制器。
 **/

class WalletController : public QObject
{
    Q_OBJECT

    WalletModel* getOrCreateWallet(std::unique_ptr<interfaces::Wallet> wallet);
    void removeAndDeleteWallet(WalletModel* wallet_model);

public:
    WalletController(interfaces::Node& node, const PlatformStyle* platform_style, OptionsModel* options_model, QObject* parent);
    ~WalletController();

    std::vector<WalletModel*> getWallets() const;

private Q_SLOTS:
    void addWallet(WalletModel* wallet_model);

Q_SIGNALS:
    void walletAdded(WalletModel* wallet_model);
    void walletRemoved(WalletModel* wallet_model);

    void coinsSent(WalletModel* wallet_model, SendCoinsRecipient recipient, QByteArray transaction);

private:
    interfaces::Node& m_node;
    const PlatformStyle* const m_platform_style;
    OptionsModel* const m_options_model;
    mutable QMutex m_mutex;
    std::vector<WalletModel*> m_wallets;
    std::unique_ptr<interfaces::Handler> m_handler_load_wallet;
};

#endif //比特币qt钱包控制器
