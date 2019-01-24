
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

#include <qt/recentrequeststablemodel.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <clientversion.h>
#include <streams.h>


RecentRequestsTableModel::RecentRequestsTableModel(WalletModel *parent) :
    QAbstractTableModel(parent), walletModel(parent)
{
//从钱包加载条目
    std::vector<std::string> vReceiveRequests;
    parent->loadReceiveRequests(vReceiveRequests);
    for (const std::string& request : vReceiveRequests)
        addNewRequest(request);

    /*这些列必须与ColumnIndex枚举中的索引匹配*/
    columns << tr("Date") << tr("Label") << tr("Message") << getAmountTitle();

    connect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &RecentRequestsTableModel::updateDisplayUnit);
}

RecentRequestsTableModel::~RecentRequestsTableModel()
{
    /*故意留空*/
}

int RecentRequestsTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return list.length();
}

int RecentRequestsTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return columns.length();
}

QVariant RecentRequestsTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || index.row() >= list.length())
        return QVariant();

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        const RecentRequestEntry *rec = &list[index.row()];
        switch(index.column())
        {
        case Date:
            return GUIUtil::dateTimeStr(rec->date);
        case Label:
            if(rec->recipient.label.isEmpty() && role == Qt::DisplayRole)
            {
                return tr("(no label)");
            }
            else
            {
                return rec->recipient.label;
            }
        case Message:
            if(rec->recipient.message.isEmpty() && role == Qt::DisplayRole)
            {
                return tr("(no message)");
            }
            else
            {
                return rec->recipient.message;
            }
        case Amount:
            if (rec->recipient.amount == 0 && role == Qt::DisplayRole)
                return tr("(no amount requested)");
            else if (role == Qt::EditRole)
                return BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), rec->recipient.amount, false, BitcoinUnits::separatorNever);
            else
                return BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), rec->recipient.amount);
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        if (index.column() == Amount)
            return (int)(Qt::AlignRight|Qt::AlignVCenter);
    }
    return QVariant();
}

bool RecentRequestsTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    return true;
}

QVariant RecentRequestsTableModel::headerData(int section, Qt::Orientation orientation, int role) const
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

/*将列标题更新为“amount（displayUnit）”，并发出headerDataChanged（）信号，以便表头作出反应。*/
void RecentRequestsTableModel::updateAmountColumnTitle()
{
    columns[Amount] = getAmountTitle();
    Q_EMIT headerDataChanged(Qt::Horizontal,Amount,Amount);
}

/*获取包含当前显示单位的Amount列的标题（如果有OptionsModel引用可用）。*/
QString RecentRequestsTableModel::getAmountTitle()
{
    return (this->walletModel->getOptionsModel() != nullptr) ? tr("Requested") + " ("+BitcoinUnits::shortName(this->walletModel->getOptionsModel()->getDisplayUnit()) + ")" : "";
}

QModelIndex RecentRequestsTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return createIndex(row, column);
}

bool RecentRequestsTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent);

    if(count > 0 && row >= 0 && (row+count) <= list.size())
    {
        for (int i = 0; i < count; ++i)
        {
            const RecentRequestEntry* rec = &list[row+i];
            if (!walletModel->saveReceiveRequest(rec->recipient.address.toStdString(), rec->id, ""))
                return false;
        }

        beginRemoveRows(parent, row, row + count - 1);
        list.erase(list.begin() + row, list.begin() + row + count);
        endRemoveRows();
        return true;
    } else {
        return false;
    }
}

Qt::ItemFlags RecentRequestsTableModel::flags(const QModelIndex &index) const
{
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

//从GUI添加请求时调用
void RecentRequestsTableModel::addNewRequest(const SendCoinsRecipient &recipient)
{
    RecentRequestEntry newEntry;
    newEntry.id = ++nReceiveRequestsMaxId;
    newEntry.date = QDateTime::currentDateTime();
    newEntry.recipient = recipient;

    CDataStream ss(SER_DISK, CLIENT_VERSION);
    ss << newEntry;

    if (!walletModel->saveReceiveRequest(recipient.address.toStdString(), newEntry.id, ss.str()))
        return;

    addNewRequest(newEntry);
}

//从钱包加载时从ctor调用
void RecentRequestsTableModel::addNewRequest(const std::string &recipient)
{
    std::vector<char> data(recipient.begin(), recipient.end());
    CDataStream ss(data, SER_DISK, CLIENT_VERSION);

    RecentRequestEntry entry;
    ss >> entry;

if (entry.id == 0) //不应该发生
        return;

    if (entry.id > nReceiveRequestsMaxId)
        nReceiveRequestsMaxId = entry.id;

    addNewRequest(entry);
}

//实际添加到GUI中的表
void RecentRequestsTableModel::addNewRequest(RecentRequestEntry &recipient)
{
    beginInsertRows(QModelIndex(), 0, 0);
    list.prepend(recipient);
    endInsertRows();
}

void RecentRequestsTableModel::sort(int column, Qt::SortOrder order)
{
    qSort(list.begin(), list.end(), RecentRequestEntryLessThan(column, order));
    Q_EMIT dataChanged(index(0, 0, QModelIndex()), index(list.size() - 1, NUMBER_OF_COLUMNS - 1, QModelIndex()));
}

void RecentRequestsTableModel::updateDisplayUnit()
{
    updateAmountColumnTitle();
}

bool RecentRequestEntryLessThan::operator()(RecentRequestEntry &left, RecentRequestEntry &right) const
{
    RecentRequestEntry *pLeft = &left;
    RecentRequestEntry *pRight = &right;
    if (order == Qt::DescendingOrder)
        std::swap(pLeft, pRight);

    switch(column)
    {
    case RecentRequestsTableModel::Date:
        return pLeft->date.toTime_t() < pRight->date.toTime_t();
    case RecentRequestsTableModel::Label:
        return pLeft->recipient.label < pRight->recipient.label;
    case RecentRequestsTableModel::Message:
        return pLeft->recipient.message < pRight->recipient.message;
    case RecentRequestsTableModel::Amount:
        return pLeft->recipient.amount < pRight->recipient.amount;
    default:
        return pLeft->id < pRight->id;
    }
}
