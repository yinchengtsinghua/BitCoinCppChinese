
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

#include <qt/walletcontroller.h>

#include <interfaces/handler.h>
#include <interfaces/node.h>

#include <algorithm>

#include <QMutexLocker>
#include <QThread>

WalletController::WalletController(interfaces::Node& node, const PlatformStyle* platform_style, OptionsModel* options_model, QObject* parent)
    : QObject(parent)
    , m_node(node)
    , m_platform_style(platform_style)
    , m_options_model(options_model)
{
    m_handler_load_wallet = m_node.handleLoadWallet([this](std::unique_ptr<interfaces::Wallet> wallet) {
        getOrCreateWallet(std::move(wallet));
    });

    for (std::unique_ptr<interfaces::Wallet>& wallet : m_node.getWallets()) {
        getOrCreateWallet(std::move(wallet));
    }
}

//不使用默认析构函数，因为并非所有成员类型定义都是
//在标题中可用，只是向前声明。
WalletController::~WalletController() {}

std::vector<WalletModel*> WalletController::getWallets() const
{
    QMutexLocker locker(&m_mutex);
    return m_wallets;
}

WalletModel* WalletController::getOrCreateWallet(std::unique_ptr<interfaces::Wallet> wallet)
{
    QMutexLocker locker(&m_mutex);

//返回模型实例（如果存在）。
    if (!m_wallets.empty()) {
        std::string name = wallet->getWalletName();
        for (WalletModel* wallet_model : m_wallets) {
            if (wallet_model->wallet().getWalletName() == name) {
                return wallet_model;
            }
        }
    }

//实例化模型并注册它。
    WalletModel* wallet_model = new WalletModel(std::move(wallet), m_node, m_platform_style, m_options_model, nullptr);
    m_wallets.push_back(wallet_model);

    connect(wallet_model, &WalletModel::unload, [this, wallet_model] {
        removeAndDeleteWallet(wallet_model);
    });

//重新发射钱包模型的共同信号。
    connect(wallet_model, &WalletModel::coinsSent, this, &WalletController::coinsSent);

//通知GUI线程上的walletadded信号。
    if (QThread::currentThread() == thread()) {
        addWallet(wallet_model);
    } else {
//处理程序回调在不同的线程中运行，因此修复钱包模型线程关联。
        wallet_model->moveToThread(thread());
        QMetaObject::invokeMethod(this, "addWallet", Qt::QueuedConnection, Q_ARG(WalletModel*, wallet_model));
    }

    return wallet_model;
}

void WalletController::addWallet(WalletModel* wallet_model)
{
//取得钱包模型的所有权并登记。
    wallet_model->setParent(this);
    Q_EMIT walletAdded(wallet_model);
}

void WalletController::removeAndDeleteWallet(WalletModel* wallet_model)
{
//注销钱包模型。
    {
        QMutexLocker locker(&m_mutex);
        m_wallets.erase(std::remove(m_wallets.begin(), m_wallets.end(), wallet_model));
    }
    Q_EMIT walletRemoved(wallet_model);
//因为模型可以保存最后一个
//cwallet共享指针。
    delete wallet_model;
}
