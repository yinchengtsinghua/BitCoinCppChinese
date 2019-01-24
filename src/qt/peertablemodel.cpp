
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

#include <qt/peertablemodel.h>

#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>

#include <interfaces/node.h>
#include <validation.h> //为CSKEMAN
#include <sync.h>

#include <QDebug>
#include <QList>
#include <QTimer>

bool NodeLessThan::operator()(const CNodeCombinedStats &left, const CNodeCombinedStats &right) const
{
    const CNodeStats *pLeft = &(left.nodeStats);
    const CNodeStats *pRight = &(right.nodeStats);

    if (order == Qt::DescendingOrder)
        std::swap(pLeft, pRight);

    switch(column)
    {
    case PeerTableModel::NetNodeId:
        return pLeft->nodeid < pRight->nodeid;
    case PeerTableModel::Address:
        return pLeft->addrName.compare(pRight->addrName) < 0;
    case PeerTableModel::Subversion:
        return pLeft->cleanSubVer.compare(pRight->cleanSubVer) < 0;
    case PeerTableModel::Ping:
        return pLeft->dMinPing < pRight->dMinPing;
    case PeerTableModel::Sent:
        return pLeft->nSendBytes < pRight->nSendBytes;
    case PeerTableModel::Received:
        return pLeft->nRecvBytes < pRight->nRecvBytes;
    }

    return false;
}

//私有实施
class PeerTablePriv
{
public:
    /*对等信息的本地缓存*/
    QList<CNodeCombinedStats> cachedNodeStats;
    /*节点排序依据的列（默认为未排序）*/
    int sortColumn{-1};
    /*按顺序（升序或降序）对节点排序*/
    Qt::SortOrder sortOrder;
    /*按节点ID列出的行索引*/
    std::map<NodeId, int> mapNodeRows;

    /*将Vnodes中的完整对等点列表拉到缓存中*/
    void refreshPeers(interfaces::Node& node)
    {
        {
            cachedNodeStats.clear();

            interfaces::Node::NodesStats nodes_stats;
            node.getNodesStats(nodes_stats);
            cachedNodeStats.reserve(nodes_stats.size());
            for (const auto& node_stats : nodes_stats)
            {
                CNodeCombinedStats stats;
                stats.nodeStats = std::get<0>(node_stats);
                stats.fNodeStateStatsAvailable = std::get<1>(node_stats);
                stats.nodeStateStats = std::get<2>(node_stats);
                cachedNodeStats.append(stats);
            }
        }

        if (sortColumn >= 0)
//排序cachenodestats（使用稳定排序以防止行不必要地跳跃）
            qStableSort(cachedNodeStats.begin(), cachedNodeStats.end(), NodeLessThan(sortColumn, sortOrder));

//建立索引图
        mapNodeRows.clear();
        int row = 0;
        for (const CNodeCombinedStats& stats : cachedNodeStats)
            mapNodeRows.insert(std::pair<NodeId, int>(stats.nodeStats.nodeid, row++));
    }

    int size() const
    {
        return cachedNodeStats.size();
    }

    CNodeCombinedStats *index(int idx)
    {
        if (idx >= 0 && idx < cachedNodeStats.size())
            return &cachedNodeStats[idx];

        return nullptr;
    }
};

PeerTableModel::PeerTableModel(interfaces::Node& node, ClientModel *parent) :
    QAbstractTableModel(parent),
    m_node(node),
    clientModel(parent),
    timer(nullptr)
{
    columns << tr("NodeId") << tr("Node/Service") << tr("Ping") << tr("Sent") << tr("Received") << tr("User Agent");
    priv.reset(new PeerTablePriv());

//设置自动刷新计时器
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &PeerTableModel::refresh);
    timer->setInterval(MODEL_UPDATE_DELAY);

//加载初始数据
    refresh();
}

PeerTableModel::~PeerTableModel()
{
//故意留空
}

void PeerTableModel::startAutoRefresh()
{
    timer->start();
}

void PeerTableModel::stopAutoRefresh()
{
    timer->stop();
}

int PeerTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int PeerTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant PeerTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    CNodeCombinedStats *rec = static_cast<CNodeCombinedStats*>(index.internalPointer());

    if (role == Qt::DisplayRole) {
        switch(index.column())
        {
        case NetNodeId:
            return (qint64)rec->nodeStats.nodeid;
        case Address:
//预端对端地址入站连接的向下箭头符号和出站连接的向上箭头符号
            return QString(rec->nodeStats.fInbound ? "↓ " : "↑ ") + QString::fromStdString(rec->nodeStats.addrName);
        case Subversion:
            return QString::fromStdString(rec->nodeStats.cleanSubVer);
        case Ping:
            return GUIUtil::formatPingTime(rec->nodeStats.dMinPing);
        case Sent:
            return GUIUtil::formatBytes(rec->nodeStats.nSendBytes);
        case Received:
            return GUIUtil::formatBytes(rec->nodeStats.nRecvBytes);
        }
    } else if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
            case Ping:
            case Sent:
            case Received:
                return QVariant(Qt::AlignRight | Qt::AlignVCenter);
            default:
                return QVariant();
        }
    }

    return QVariant();
}

QVariant PeerTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole && section < columns.size())
        {
            return columns[section];
        }
    }
    return QVariant();
}

Qt::ItemFlags PeerTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    return retval;
}

QModelIndex PeerTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    CNodeCombinedStats *data = priv->index(row);

    if (data)
        return createIndex(row, column, data);
    return QModelIndex();
}

const CNodeCombinedStats *PeerTableModel::getNodeStats(int idx)
{
    return priv->index(idx);
}

void PeerTableModel::refresh()
{
    Q_EMIT layoutAboutToBeChanged();
    priv->refreshPeers(m_node);
    Q_EMIT layoutChanged();
}

int PeerTableModel::getRowByNodeId(NodeId nodeid)
{
    std::map<NodeId, int>::iterator it = priv->mapNodeRows.find(nodeid);
    if (it == priv->mapNodeRows.end())
        return -1;

    return it->second;
}

void PeerTableModel::sort(int column, Qt::SortOrder order)
{
    priv->sortColumn = column;
    priv->sortOrder = order;
    refresh();
}
