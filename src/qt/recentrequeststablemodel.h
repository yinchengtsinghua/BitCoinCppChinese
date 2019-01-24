
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

#ifndef BITCOIN_QT_RECENTREQUESTSTABLEMODEL_H
#define BITCOIN_QT_RECENTREQUESTSTABLEMODEL_H

#include <qt/walletmodel.h>

#include <QAbstractTableModel>
#include <QStringList>
#include <QDateTime>

class RecentRequestEntry
{
public:
    RecentRequestEntry() : nVersion(RecentRequestEntry::CURRENT_VERSION), id(0) { }

    static const int CURRENT_VERSION = 1;
    int nVersion;
    int64_t id;
    QDateTime date;
    SendCoinsRecipient recipient;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        unsigned int nDate = date.toTime_t();

        READWRITE(this->nVersion);
        READWRITE(id);
        READWRITE(nDate);
        READWRITE(recipient);

        if (ser_action.ForRead())
            date = QDateTime::fromTime_t(nDate);
    }
};

class RecentRequestEntryLessThan
{
public:
    RecentRequestEntryLessThan(int nColumn, Qt::SortOrder fOrder):
        column(nColumn), order(fOrder) {}
    bool operator()(RecentRequestEntry &left, RecentRequestEntry &right) const;

private:
    int column;
    Qt::SortOrder order;
};

/*最近生成的付款请求/比特币列表的模型：uris。
 *钱包型号的一部分。
 **/

class RecentRequestsTableModel: public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit RecentRequestsTableModel(WalletModel *parent);
    ~RecentRequestsTableModel();

    enum ColumnIndex {
        Date = 0,
        Label = 1,
        Message = 2,
        Amount = 3,
        NUMBER_OF_COLUMNS
    };

    /*@name方法被qabstractTableModel重写
        @*/

    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
    Qt::ItemFlags flags(const QModelIndex &index) const;
    /**/

    const RecentRequestEntry &entry(int row) const { return list[row]; }
    void addNewRequest(const SendCoinsRecipient &recipient);
    void addNewRequest(const std::string &recipient);
    void addNewRequest(RecentRequestEntry &recipient);

public Q_SLOTS:
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);
    void updateDisplayUnit();

private:
    WalletModel *walletModel;
    QStringList columns;
    QList<RecentRequestEntry> list;
    int64_t nReceiveRequestsMaxId{0};

    /*将列标题更新为“amount（displayUnit）”，并发出headerDataChanged（）信号，以便表头作出反应。*/
    void updateAmountColumnTitle();
    /*获取包含当前显示单位的Amount列的标题（如果有OptionsModel引用可用）。*/
    QString getAmountTitle();
};

#endif //比特币
