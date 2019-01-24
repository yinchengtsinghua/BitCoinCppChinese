
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

#ifndef BITCOIN_QT_PEERTABLEMODEL_H
#define BITCOIN_QT_PEERTABLEMODEL_H

#include <net_processing.h> //对于cnodestats
#include <net.h>

#include <memory>

#include <QAbstractTableModel>
#include <QStringList>

class ClientModel;
class PeerTablePriv;

namespace interfaces {
class Node;
}

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

struct CNodeCombinedStats {
    CNodeStats nodeStats;
    CNodeStateStats nodeStateStats;
    bool fNodeStateStatsAvailable;
};

class NodeLessThan
{
public:
    NodeLessThan(int nColumn, Qt::SortOrder fOrder) :
        column(nColumn), order(fOrder) {}
    bool operator()(const CNodeCombinedStats &left, const CNodeCombinedStats &right) const;

private:
    int column;
    Qt::SortOrder order;
};

/*
   qt模型提供有关连接对等端的信息，类似于
   “GetPeerInfo”RPC调用。由RPC控制台UI使用。
 **/

class PeerTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit PeerTableModel(interfaces::Node& node, ClientModel *parent = nullptr);
    ~PeerTableModel();
    const CNodeCombinedStats *getNodeStats(int idx);
    int getRowByNodeId(NodeId nodeid);
    void startAutoRefresh();
    void stopAutoRefresh();

    enum ColumnIndex {
        NetNodeId = 0,
        Address = 1,
        Ping = 2,
        Sent = 3,
        Received = 4,
        Subversion = 5
    };

    /*@name方法被qabstractTableModel重写
        @*/

    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    void sort(int column, Qt::SortOrder order);
    /**/

public Q_SLOTS:
    void refresh();

private:
    interfaces::Node& m_node;
    ClientModel *clientModel;
    QStringList columns;
    std::unique_ptr<PeerTablePriv> priv;
    QTimer *timer;
};

#endif //比特币Qt-PeerTableModel
