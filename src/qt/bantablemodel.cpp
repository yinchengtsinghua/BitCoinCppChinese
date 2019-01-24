
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

#include <qt/bantablemodel.h>

#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>

#include <interfaces/node.h>
#include <sync.h>
#include <util/time.h>

#include <QDebug>
#include <QList>

bool BannedNodeLessThan::operator()(const CCombinedBan& left, const CCombinedBan& right) const
{
    const CCombinedBan* pLeft = &left;
    const CCombinedBan* pRight = &right;

    if (order == Qt::DescendingOrder)
        std::swap(pLeft, pRight);

    switch(column)
    {
    case BanTableModel::Address:
        return pLeft->subnet.ToString().compare(pRight->subnet.ToString()) < 0;
    case BanTableModel::Bantime:
        return pLeft->banEntry.nBanUntil < pRight->banEntry.nBanUntil;
    }

    return false;
}

//私有实施
class BanTablePriv
{
public:
    /*对等信息的本地缓存*/
    QList<CCombinedBan> cachedBanlist;
    /*节点排序依据的列（默认为未排序）*/
    int sortColumn{-1};
    /*按顺序（升序或降序）对节点排序*/
    Qt::SortOrder sortOrder;

    /*将CNode中被禁止节点的完整列表拉到缓存中*/
    void refreshBanlist(interfaces::Node& node)
    {
        banmap_t banMap;
        node.getBanned(banMap);

        cachedBanlist.clear();
        cachedBanlist.reserve(banMap.size());
        for (const auto& entry : banMap)
        {
            CCombinedBan banEntry;
            banEntry.subnet = entry.first;
            banEntry.banEntry = entry.second;
            cachedBanlist.append(banEntry);
        }

        if (sortColumn >= 0)
//排序cachedbanlist（使用稳定排序以防止行不必要地跳跃）
            qStableSort(cachedBanlist.begin(), cachedBanlist.end(), BannedNodeLessThan(sortColumn, sortOrder));
    }

    int size() const
    {
        return cachedBanlist.size();
    }

    CCombinedBan *index(int idx)
    {
        if (idx >= 0 && idx < cachedBanlist.size())
            return &cachedBanlist[idx];

        return nullptr;
    }
};

BanTableModel::BanTableModel(interfaces::Node& node, ClientModel *parent) :
    QAbstractTableModel(parent),
    m_node(node),
    clientModel(parent)
{
    columns << tr("IP/Netmask") << tr("Banned Until");
    priv.reset(new BanTablePriv());

//加载初始数据
    refresh();
}

BanTableModel::~BanTableModel()
{
//故意留空
}

int BanTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int BanTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant BanTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    CCombinedBan *rec = static_cast<CCombinedBan*>(index.internalPointer());

    if (role == Qt::DisplayRole) {
        switch(index.column())
        {
        case Address:
            return QString::fromStdString(rec->subnet.ToString());
        case Bantime:
            QDateTime date = QDateTime::fromMSecsSinceEpoch(0);
            date = date.addSecs(rec->banEntry.nBanUntil);
            return date.toString(Qt::SystemLocaleLongDate);
        }
    }

    return QVariant();
}

QVariant BanTableModel::headerData(int section, Qt::Orientation orientation, int role) const
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

Qt::ItemFlags BanTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    return retval;
}

QModelIndex BanTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    CCombinedBan *data = priv->index(row);

    if (data)
        return createIndex(row, column, data);
    return QModelIndex();
}

void BanTableModel::refresh()
{
    Q_EMIT layoutAboutToBeChanged();
    priv->refreshBanlist(m_node);
    Q_EMIT layoutChanged();
}

void BanTableModel::sort(int column, Qt::SortOrder order)
{
    priv->sortColumn = column;
    priv->sortOrder = order;
    refresh();
}

bool BanTableModel::shouldShow()
{
    return priv->size() > 0;
}
