
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

#ifndef BITCOIN_QT_TRANSACTIONTABLEMODEL_H
#define BITCOIN_QT_TRANSACTIONTABLEMODEL_H

#include <qt/bitcoinunits.h>

#include <QAbstractTableModel>
#include <QStringList>

#include <memory>

namespace interfaces {
class Handler;
}

class PlatformStyle;
class TransactionRecord;
class TransactionTablePriv;
class WalletModel;

/*钱包交易表的用户界面模型。
 **/

class TransactionTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit TransactionTableModel(const PlatformStyle *platformStyle, WalletModel *parent = nullptr);
    ~TransactionTableModel();

    enum ColumnIndex {
        Status = 0,
        Watchonly = 1,
        Date = 2,
        Type = 3,
        ToAddress = 4,
        Amount = 5
    };

    /*从事务行获取特定信息的角色。
        它们独立于列。
    **/

    enum RoleIndex {
        /*交易类型*/
        TypeRole = Qt::UserRole,
        /*创建此交易记录的日期和时间*/
        DateRole,
        /*仅监视布尔值*/
        WatchonlyRole,
        /*只看图标*/
        WatchonlyDecorationRole,
        /*长描述（HTML格式）*/
        LongDescriptionRole,
        /*交易地址*/
        AddressRole,
        /*与交易相关的地址标签*/
        LabelRole,
        /*交易净额*/
        AmountRole,
        /*事务哈希*/
        TxHashRole,
        /*事务数据，十六进制编码*/
        TxHexRole,
        /*纯文本形式的整个事务*/
        TxPlainTextRole,
        /*交易确认了吗？*/
        ConfirmedRole,
        /*格式化金额，未确认时不带括号*/
        FormattedAmountRole,
        /*事务状态（TransactionRecord:：Status）*/
        StatusRole,
        /*未处理图标*/
        RawDecorationRole,
    };

    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex & parent = QModelIndex()) const;
    bool processingQueuedTransactions() const { return fProcessingQueuedTransactions; }

private:
    WalletModel *walletModel;
    std::unique_ptr<interfaces::Handler> m_handler_transaction_changed;
    std::unique_ptr<interfaces::Handler> m_handler_show_progress;
    QStringList columns;
    TransactionTablePriv *priv;
    bool fProcessingQueuedTransactions;
    const PlatformStyle *platformStyle;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

    QString lookupAddress(const std::string &address, bool tooltip) const;
    QVariant addressColor(const TransactionRecord *wtx) const;
    QString formatTxStatus(const TransactionRecord *wtx) const;
    QString formatTxDate(const TransactionRecord *wtx) const;
    QString formatTxType(const TransactionRecord *wtx) const;
    QString formatTxToAddress(const TransactionRecord *wtx, bool tooltip) const;
    QString formatTxAmount(const TransactionRecord *wtx, bool showUnconfirmed=true, BitcoinUnits::SeparatorStyle separators=BitcoinUnits::separatorStandard) const;
    QString formatTooltip(const TransactionRecord *rec) const;
    QVariant txStatusDecoration(const TransactionRecord *wtx) const;
    QVariant txWatchonlyDecoration(const TransactionRecord *wtx) const;
    QVariant txAddressDecoration(const TransactionRecord *wtx) const;

public Q_SLOTS:
    /*新事务或事务更改状态*/
    void updateTransaction(const QString &hash, int status, bool showTransaction);
    void updateConfirmations();
    void updateDisplayUnit();
    /*将列标题更新为“amount（displayUnit）”，并发出headerDataChanged（）信号，以便表头作出反应。*/
    void updateAmountColumnTitle();
    /*需要通过QueuedConnection更新fprocessingQueuedTransactions*/
    void setProcessingQueuedTransactions(bool value) { fProcessingQueuedTransactions = value; }

    friend class TransactionTablePriv;
};

#endif //比特币qt_交易表模型
