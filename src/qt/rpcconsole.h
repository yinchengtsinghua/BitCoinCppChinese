
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

#ifndef BITCOIN_QT_RPCCONSOLE_H
#define BITCOIN_QT_RPCCONSOLE_H

#include <qt/guiutil.h>
#include <qt/peertablemodel.h>

#include <net.h>

#include <QWidget>
#include <QCompleter>
#include <QThread>

class ClientModel;
class PlatformStyle;
class RPCTimerInterface;
class WalletModel;

namespace interfaces {
    class Node;
}

namespace Ui {
    class RPCConsole;
}

QT_BEGIN_NAMESPACE
class QMenu;
class QItemSelection;
QT_END_NAMESPACE

/*本地比特币RPC控制台。*/
class RPCConsole: public QWidget
{
    Q_OBJECT

public:
    explicit RPCConsole(interfaces::Node& node, const PlatformStyle *platformStyle, QWidget *parent);
    ~RPCConsole();

    static bool RPCParseCommandLine(interfaces::Node* node, std::string &strResult, const std::string &strCommand, bool fExecute, std::string * const pstrFilteredOut = nullptr, const WalletModel* wallet_model = nullptr);
    static bool RPCExecuteCommandLine(interfaces::Node& node, std::string &strResult, const std::string &strCommand, std::string * const pstrFilteredOut = nullptr, const WalletModel* wallet_model = nullptr) {
        return RPCParseCommandLine(&node, strResult, strCommand, true, pstrFilteredOut, wallet_model);
    }

    void setClientModel(ClientModel *model);
    void addWallet(WalletModel * const walletModel);
    void removeWallet(WalletModel* const walletModel);

    enum MessageClass {
        MC_ERROR,
        MC_DEBUG,
        CMD_REQUEST,
        CMD_REPLY,
        CMD_ERROR
    };

    enum TabTypes {
        TAB_INFO = 0,
        TAB_CONSOLE = 1,
        TAB_GRAPH = 2,
        TAB_PEERS = 3
    };

    std::vector<TabTypes> tabs() const { return {TAB_INFO, TAB_CONSOLE, TAB_GRAPH, TAB_PEERS}; }

    TabTypes tabFocus() const;
    QString tabTitle(TabTypes tab_type) const;

protected:
    virtual bool eventFilter(QObject* obj, QEvent *event);
    void keyPressEvent(QKeyEvent *);

private Q_SLOTS:
    void on_lineEdit_returnPressed();
    void on_tabWidget_currentChanged(int index);
    /*从当前datadir打开debug.log*/
    void on_openDebugLogfileButton_clicked();
    /*更改网络流量图的时间范围*/
    void on_sldGraphRange_valueChanged(int value);
    /*更新流量统计信息*/
    void updateTrafficStats(quint64 totalBytesIn, quint64 totalBytesOut);
    void resizeEvent(QResizeEvent *event);
    void showEvent(QShowEvent *event);
    void hideEvent(QHideEvent *event);
    /*在“对等”选项卡上显示自定义上下文菜单*/
    void showPeersTableContextMenu(const QPoint& point);
    /*在“禁止”选项卡上显示自定义上下文菜单*/
    void showBanTableContextMenu(const QPoint& point);
    /*如果不存在禁止，则隐藏禁止表*/
    void showOrHideBanTableIfRequired();
    /*清除所选节点*/
    void clearSelectedNode();

public Q_SLOTS:
    void clear(bool clearHistory = true);
    void fontBigger();
    void fontSmaller();
    void setFontSize(int newSize);
    /*将消息附加到消息小部件*/
    void message(int category, const QString &msg) { message(category, msg, false); }
    void message(int category, const QString &message, bool html);
    /*设置用户界面中显示的连接数*/
    void setNumConnections(int count);
    /*设置用户界面中显示的网络状态*/
    void setNetworkActive(bool networkActive);
    /*设置用户界面中显示的块数和最后一个块日期*/
    void setNumBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, bool headers);
    /*设置用户界面中内存池的大小（事务数和内存使用情况）*/
    void setMempoolSize(long numberOfTxs, size_t dynUsage);
    /*向前或向后追溯历史*/
    void browseHistory(int offset);
    /*将控制台视图滚动到末尾*/
    void scrollToEnd();
    /*处理对等列表中的对等选择*/
    void peerSelected(const QItemSelection &selected, const QItemSelection &deselected);
    /*在更新前处理选择缓存*/
    void peerLayoutAboutToChange();
    /*处理更新的对等信息*/
    void peerLayoutChanged();
    /*在“对等”选项卡上断开选定节点的连接*/
    void disconnectSelectedNode();
    /*禁止对等选项卡上的选定节点*/
    void banSelectedNode(int bantime);
    /*在BANS选项卡上取消对选定节点的绑定*/
    void unbanSelectedNode();
    /*设置哪个选项卡具有焦点（可见）*/
    void setTabFocus(enum TabTypes tabType);

Q_SIGNALS:
//对于rpc命令执行器
    void cmdRequest(const QString &command, const WalletModel* wallet_model);

private:
    void startExecutor();
    void setTrafficGraphRange(int mins);
    /*显示有关所选节点的UI的详细信息*/
    void updateNodeDetail(const CNodeCombinedStats *stats);

    enum ColumnWidths
    {
        ADDRESS_COLUMN_WIDTH = 200,
        SUBVERSION_COLUMN_WIDTH = 150,
        PING_COLUMN_WIDTH = 80,
        BANSUBNET_COLUMN_WIDTH = 200,
        BANTIME_COLUMN_WIDTH = 250

    };

    interfaces::Node& m_node;
    Ui::RPCConsole* const ui;
    ClientModel *clientModel = nullptr;
    QStringList history;
    int historyPtr = 0;
    QString cmdBeforeBrowsing;
    QList<NodeId> cachedNodeids;
    const PlatformStyle* const platformStyle;
    RPCTimerInterface *rpcTimerInterface = nullptr;
    QMenu *peersTableContextMenu = nullptr;
    QMenu *banTableContextMenu = nullptr;
    int consoleFontSize = 0;
    QCompleter *autoCompleter = nullptr;
    QThread thread;
    WalletModel* m_last_wallet_model{nullptr};

    /*使用模型中的最新网络信息更新用户界面。*/
    void updateNetworkState();
};

#endif //比特币
