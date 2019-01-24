
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

#include <qt/transactiontablemodel.h>

#include <qt/addresstablemodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/transactiondesc.h>
#include <qt/transactionrecord.h>
#include <qt/walletmodel.h>

#include <core_io.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <sync.h>
#include <uint256.h>
#include <util/system.h>
#include <validation.h>

#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QIcon>
#include <QList>


//金额列右对齐，包含数字
static int column_alignments[] = {
        /*：AlignLeft qt:：AlignvCenter，/*状态*/
        qt:：AlignLeft qt:：AlignvCenter，/*仅监视*/

        /*：AlignLeft qt:：AlignvCenter，/*日期*/
        qt:：AlignLeft qt:：AlignvCenter，/*类型*/

        /*：AlignLeft qt:：AlignvCenter，/*地址*/
        qt:：AlignRight qt:：AlignvCenter/*数量*/

    };

//Tx型列表排序/二进制搜索的比较运算符
struct TxLessThan
{
    bool operator()(const TransactionRecord &a, const TransactionRecord &b) const
    {
        return a.hash < b.hash;
    }
    bool operator()(const TransactionRecord &a, const uint256 &b) const
    {
        return a.hash < b;
    }
    bool operator()(const uint256 &a, const TransactionRecord &b) const
    {
        return a < b.hash;
    }
};

//私有实施
class TransactionTablePriv
{
public:
    explicit TransactionTablePriv(TransactionTableModel *_parent) :
        parent(_parent)
    {
    }

    TransactionTableModel *parent;

    /*钱包的本地缓存。
     *根据定义，它与cwallet的顺序相同。
     *按sha256排序。
     **/

    QList<TransactionRecord> cachedWallet;

    /*从核心重新查询整个钱包。
     **/

    void refreshWallet(interfaces::Wallet& wallet)
    {
        qDebug() << "TransactionTablePriv::refreshWallet";
        cachedWallet.clear();
        {
            for (const auto& wtx : wallet.getWalletTxs()) {
                if (TransactionRecord::showTransaction()) {
                    cachedWallet.append(TransactionRecord::decomposeTransaction(wtx));
                }
            }
        }
    }

    /*逐步更新钱包模型，以同步钱包模型
       和核心的那个。

       调用已添加、删除或更改的事务。
     **/

    void updateWallet(interfaces::Wallet& wallet, const uint256 &hash, int status, bool showTransaction)
    {
        qDebug() << "TransactionTablePriv::updateWallet: " + QString::fromStdString(hash.ToString()) + " " + QString::number(status);

//在模型中查找此事务的边界
        QList<TransactionRecord>::iterator lower = qLowerBound(
            cachedWallet.begin(), cachedWallet.end(), hash, TxLessThan());
        QList<TransactionRecord>::iterator upper = qUpperBound(
            cachedWallet.begin(), cachedWallet.end(), hash, TxLessThan());
        int lowerIndex = (lower - cachedWallet.begin());
        int upperIndex = (upper - cachedWallet.begin());
        bool inModel = (lower != upper);

        if(status == CT_UPDATED)
        {
            if(showTransaction && !inModel)
                /*tus=ct_new；/*不在模型中，但要显示，视为新的*/
            如果（！）ShowTransaction和InModel）
                status=ct_deleted；/*在模型中，但要隐藏，视为已删除*/

        }

        qDebug() << "    inModel=" + QString::number(inModel) +
                    " Index=" + QString::number(lowerIndex) + "-" + QString::number(upperIndex) +
                    " showTransaction=" + QString::number(showTransaction) + " derivedStatus=" + QString::number(status);

        switch(status)
        {
        case CT_NEW:
            if(inModel)
            {
                qWarning() << "TransactionTablePriv::updateWallet: Warning: Got CT_NEW, but transaction is already in model";
                break;
            }
            if(showTransaction)
            {
//在钱包中查找交易
                interfaces::WalletTx wtx = wallet.getWalletTx(hash);
                if(!wtx.tx)
                {
                    qWarning() << "TransactionTablePriv::updateWallet: Warning: Got CT_NEW, but transaction is not in wallet";
                    break;
                }
//添加--在正确位置插入
                QList<TransactionRecord> toInsert =
                        TransactionRecord::decomposeTransaction(wtx);
                /*！to insert.isEmpty（））/*仅当要插入的内容时*/
                {
                    parent->beginInsertRows（qmodelIndex（），lowerindex，lowerindex+toinsert.size（）-1）；
                    int insert_idx=lowerindex；
                    用于（const transactionrecord&rec:toinsert）
                    {
                        cachedWallet.insert（插入_idx，rec）；
                        插入_idx+=1；
                    }
                    parent->endinsertrows（）；
                }
            }
            断裂；
        删除的案例：
            如果（！）内模）
            {
                qwarning（）<“transactionTablepriv:：updateWallet:warning:got ct_deleted，but transaction is not in model”；
                断裂；
            }
            //已删除--从表中删除整个事务
            parent->beginremoverws（qmodelindex（），lowerindex，upperindex-1）；
            cachedWallet.erase（下，上）；
            parent->endremoves（）；
            断裂；
        案例CT更新：
            //其他更新——不做任何事情，状态更新将处理此问题，并且只计算
            //可见事务。
            对于（int i=lowerindex；i<upperindex；i++）
                transactionrecord*rec=&cachedwallet[i]；
                rec->status.needsupdate=true；
            }
            断裂；
        }
    }

    int siz（）
    {
        返回cachedWallet.size（）；
    }

    交易记录*索引（接口：：Wallet&Wallet，int IDX）
    {
        如果（idx>=0&&idx<cachedWallet.size（））
        {
            transactionrecord*rec=&cachedwallet[idx]；

            //预先获取所需的锁。这样就避免了图形用户界面
            //如果核心持有锁的时间更长，则会卡住-
            //例如，在钱包重新扫描期间。
            / /
            //如果需要状态更新（自上次检查以来出现块），
            //从钱包更新此交易的状态。否则，
            //只需重新使用缓存状态。
            接口：：wallettxstatus wtx；
            int数字块；
            Int64阻塞时间；
            if（wallet.trygettxstatus（rec->hash，wtx，numblocks，block_time）&&rec->statusupdateeneeded（numblocks））；
                rec->updateStatus（wtx，numblocks，block_time）；
            }
            返回记录；
        }
        返回null pTR；
    }

    qString描述（interfaces:：node&node，interfaces:：wallet&wallet，transactionrecord*rec，int unit）
    {
        返回事务描述：：tohtml（节点、钱包、记录、单位）；
    }

    qstring gettxhex（接口：：wallet&wallet，transactionrecord*rec）
    {
        auto tx=wallet.gettx（rec->hash）；
        如果（Tx）{
            std:：string strhex=encodehextx（*tx）；
            返回qstring:：fromstdstring（strhex）；
        }
        返回qString（）；
    }
}；

TransactionTableModel:：TransactionTableModel（const platformStyle*_platformStyle，walletModel*父级）：
        QabstractTableModel（父级）
        墙模型（父）
        priv（新交易表priv（this）），
        fprocessingQueuedTransactions（假），
        平台样式（_PlatformStyle）
{
    列<<qString（）<<qString（）<<tr（“日期”）<<tr（“类型”）<<tr（“标签”）<<bitcoinUnits:：getAmountColumnTitle（walletModel->getOptionsModel（）->getDisplayUnit（））；
    priv->refreshwallet（walletmodel->wallet（））；

    Connect（walletModel->getOptionsModel（），&OptionsModel:：DisplayUnitChanged，this，&TransactionTableModel:：UpdateDisplayUnit）；

    subscriptOCoreginals（）；
}

TransactionTableModel:：~TransactionTableModel（）。
{
    取消订阅coresignals（）；
    删除PRIV；
}

/**将列标题更新为“amount（displayUnit）”，并发出headerDataChanged（）信号，以便表头作出反应。*/

void TransactionTableModel::updateAmountColumnTitle()
{
    columns[Amount] = BitcoinUnits::getAmountColumnTitle(walletModel->getOptionsModel()->getDisplayUnit());
    Q_EMIT headerDataChanged(Qt::Horizontal,Amount,Amount);
}

void TransactionTableModel::updateTransaction(const QString &hash, int status, bool showTransaction)
{
    uint256 updated;
    updated.SetHex(hash.toStdString());

    priv->updateWallet(walletModel->wallet(), updated, status, showTransaction);
}

void TransactionTableModel::updateConfirmations()
{
//自上次投票以来，出现了一些障碍。
//失效状态（确认数量）和（可能）描述
//对于所有行。qt足够智能，只实际请求
//可见行。
    Q_EMIT dataChanged(index(0, Status), index(priv->size()-1, Status));
    Q_EMIT dataChanged(index(0, ToAddress), index(priv->size()-1, ToAddress));
}

int TransactionTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int TransactionTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QString TransactionTableModel::formatTxStatus(const TransactionRecord *wtx) const
{
    QString status;

    switch(wtx->status.status)
    {
    case TransactionStatus::OpenUntilBlock:
        status = tr("Open for %n more block(s)","",wtx->status.open_for);
        break;
    case TransactionStatus::OpenUntilDate:
        status = tr("Open until %1").arg(GUIUtil::dateTimeStr(wtx->status.open_for));
        break;
    case TransactionStatus::Unconfirmed:
        status = tr("Unconfirmed");
        break;
    case TransactionStatus::Abandoned:
        status = tr("Abandoned");
        break;
    case TransactionStatus::Confirming:
        status = tr("Confirming (%1 of %2 recommended confirmations)").arg(wtx->status.depth).arg(TransactionRecord::RecommendedNumConfirmations);
        break;
    case TransactionStatus::Confirmed:
        status = tr("Confirmed (%1 confirmations)").arg(wtx->status.depth);
        break;
    case TransactionStatus::Conflicted:
        status = tr("Conflicted");
        break;
    case TransactionStatus::Immature:
        status = tr("Immature (%1 confirmations, will be available after %2)").arg(wtx->status.depth).arg(wtx->status.depth + wtx->status.matures_in);
        break;
    case TransactionStatus::NotAccepted:
        status = tr("Generated but not accepted");
        break;
    }

    return status;
}

QString TransactionTableModel::formatTxDate(const TransactionRecord *wtx) const
{
    if(wtx->time)
    {
        return GUIUtil::dateTimeStr(wtx->time);
    }
    return QString();
}

/*在通讯簿中查找地址，如果找到，则返回标签（地址）
   否则只需返回（地址）
 **/

QString TransactionTableModel::lookupAddress(const std::string &address, bool tooltip) const
{
    QString label = walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(address));
    QString description;
    if(!label.isEmpty())
    {
        description += label;
    }
    if(label.isEmpty() || tooltip)
    {
        description += QString(" (") + QString::fromStdString(address) + QString(")");
    }
    return description;
}

QString TransactionTableModel::formatTxType(const TransactionRecord *wtx) const
{
    switch(wtx->type)
    {
    case TransactionRecord::RecvWithAddress:
        return tr("Received with");
    case TransactionRecord::RecvFromOther:
        return tr("Received from");
    case TransactionRecord::SendToAddress:
    case TransactionRecord::SendToOther:
        return tr("Sent to");
    case TransactionRecord::SendToSelf:
        return tr("Payment to yourself");
    case TransactionRecord::Generated:
        return tr("Mined");
    default:
        return QString();
    }
}

QVariant TransactionTableModel::txAddressDecoration(const TransactionRecord *wtx) const
{
    switch(wtx->type)
    {
    case TransactionRecord::Generated:
        return QIcon(":/icons/tx_mined");
    case TransactionRecord::RecvWithAddress:
    case TransactionRecord::RecvFromOther:
        return QIcon(":/icons/tx_input");
    case TransactionRecord::SendToAddress:
    case TransactionRecord::SendToOther:
        return QIcon(":/icons/tx_output");
    default:
        return QIcon(":/icons/tx_inout");
    }
}

QString TransactionTableModel::formatTxToAddress(const TransactionRecord *wtx, bool tooltip) const
{
    QString watchAddress;
    if (tooltip) {
//通过添加“（仅监视）”标记涉及仅监视地址的事务。
        watchAddress = wtx->involvesWatchAddress ? QString(" (") + tr("watch-only") + QString(")") : "";
    }

    switch(wtx->type)
    {
    case TransactionRecord::RecvFromOther:
        return QString::fromStdString(wtx->address) + watchAddress;
    case TransactionRecord::RecvWithAddress:
    case TransactionRecord::SendToAddress:
    case TransactionRecord::Generated:
        return lookupAddress(wtx->address, tooltip) + watchAddress;
    case TransactionRecord::SendToOther:
        return QString::fromStdString(wtx->address) + watchAddress;
    case TransactionRecord::SendToSelf:
    default:
        return tr("(n/a)") + watchAddress;
    }
}

QVariant TransactionTableModel::addressColor(const TransactionRecord *wtx) const
{
//以不太明显的颜色显示不带标签的地址
    switch(wtx->type)
    {
    case TransactionRecord::RecvWithAddress:
    case TransactionRecord::SendToAddress:
    case TransactionRecord::Generated:
        {
        QString label = walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(wtx->address));
        if(label.isEmpty())
            return COLOR_BAREADDRESS;
        } break;
    case TransactionRecord::SendToSelf:
        return COLOR_BAREADDRESS;
    default:
        break;
    }
    return QVariant();
}

QString TransactionTableModel::formatTxAmount(const TransactionRecord *wtx, bool showUnconfirmed, BitcoinUnits::SeparatorStyle separators) const
{
    QString str = BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), wtx->credit + wtx->debit, false, separators);
    if(showUnconfirmed)
    {
        if(!wtx->status.countsForBalance)
        {
            str = QString("[") + str + QString("]");
        }
    }
    return QString(str);
}

QVariant TransactionTableModel::txStatusDecoration(const TransactionRecord *wtx) const
{
    switch(wtx->status.status)
    {
    case TransactionStatus::OpenUntilBlock:
    case TransactionStatus::OpenUntilDate:
        return COLOR_TX_STATUS_OPENUNTILDATE;
    case TransactionStatus::Unconfirmed:
        return QIcon(":/icons/transaction_0");
    case TransactionStatus::Abandoned:
        return QIcon(":/icons/transaction_abandoned");
    case TransactionStatus::Confirming:
        switch(wtx->status.depth)
        {
        case 1: return QIcon(":/icons/transaction_1");
        case 2: return QIcon(":/icons/transaction_2");
        case 3: return QIcon(":/icons/transaction_3");
        case 4: return QIcon(":/icons/transaction_4");
        default: return QIcon(":/icons/transaction_5");
        };
    case TransactionStatus::Confirmed:
        return QIcon(":/icons/transaction_confirmed");
    case TransactionStatus::Conflicted:
        return QIcon(":/icons/transaction_conflicted");
    case TransactionStatus::Immature: {
        int total = wtx->status.depth + wtx->status.matures_in;
        int part = (wtx->status.depth * 4 / total) + 1;
        return QIcon(QString(":/icons/transaction_%1").arg(part));
        }
    case TransactionStatus::NotAccepted:
        return QIcon(":/icons/transaction_0");
    default:
        return COLOR_BLACK;
    }
}

QVariant TransactionTableModel::txWatchonlyDecoration(const TransactionRecord *wtx) const
{
    if (wtx->involvesWatchAddress)
        return QIcon(":/icons/eye");
    else
        return QVariant();
}

QString TransactionTableModel::formatTooltip(const TransactionRecord *rec) const
{
    QString tooltip = formatTxStatus(rec) + QString("\n") + formatTxType(rec);
    if(rec->type==TransactionRecord::RecvFromOther || rec->type==TransactionRecord::SendToOther ||
       rec->type==TransactionRecord::SendToAddress || rec->type==TransactionRecord::RecvWithAddress)
    {
        tooltip += QString(" ") + formatTxToAddress(rec, true);
    }
    return tooltip;
}

QVariant TransactionTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();
    TransactionRecord *rec = static_cast<TransactionRecord*>(index.internalPointer());

    switch(role)
    {
    case RawDecorationRole:
        switch(index.column())
        {
        case Status:
            return txStatusDecoration(rec);
        case Watchonly:
            return txWatchonlyDecoration(rec);
        case ToAddress:
            return txAddressDecoration(rec);
        }
        break;
    case Qt::DecorationRole:
    {
        QIcon icon = qvariant_cast<QIcon>(index.data(RawDecorationRole));
        return platformStyle->TextColorIcon(icon);
    }
    case Qt::DisplayRole:
        switch(index.column())
        {
        case Date:
            return formatTxDate(rec);
        case Type:
            return formatTxType(rec);
        case ToAddress:
            return formatTxToAddress(rec, false);
        case Amount:
            return formatTxAmount(rec, true, BitcoinUnits::separatorAlways);
        }
        break;
    case Qt::EditRole:
//编辑角色用于排序，因此返回未格式化的值
        switch(index.column())
        {
        case Status:
            return QString::fromStdString(rec->status.sortKey);
        case Date:
            return rec->time;
        case Type:
            return formatTxType(rec);
        case Watchonly:
            return (rec->involvesWatchAddress ? 1 : 0);
        case ToAddress:
            return formatTxToAddress(rec, true);
        case Amount:
            return qint64(rec->credit + rec->debit);
        }
        break;
    case Qt::ToolTipRole:
        return formatTooltip(rec);
    case Qt::TextAlignmentRole:
        return column_alignments[index.column()];
    case Qt::ForegroundRole:
//对放弃的交易使用“危险”颜色
        if(rec->status.status == TransactionStatus::Abandoned)
        {
            return COLOR_TX_STATUS_DANGER;
        }
//未确认（但不未成熟），因为交易是灰色的
        if(!rec->status.countsForBalance && rec->status.status != TransactionStatus::Immature)
        {
            return COLOR_UNCONFIRMED;
        }
        if(index.column() == Amount && (rec->credit+rec->debit) < 0)
        {
            return COLOR_NEGATIVE;
        }
        if(index.column() == ToAddress)
        {
            return addressColor(rec);
        }
        break;
    case TypeRole:
        return rec->type;
    case DateRole:
        return QDateTime::fromTime_t(static_cast<uint>(rec->time));
    case WatchonlyRole:
        return rec->involvesWatchAddress;
    case WatchonlyDecorationRole:
        return txWatchonlyDecoration(rec);
    case LongDescriptionRole:
        return priv->describe(walletModel->node(), walletModel->wallet(), rec, walletModel->getOptionsModel()->getDisplayUnit());
    case AddressRole:
        return QString::fromStdString(rec->address);
    case LabelRole:
        return walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(rec->address));
    case AmountRole:
        return qint64(rec->credit + rec->debit);
    case TxHashRole:
        return rec->getTxHash();
    case TxHexRole:
        return priv->getTxHex(walletModel->wallet(), rec);
    case TxPlainTextRole:
        {
            QString details;
            QDateTime date = QDateTime::fromTime_t(static_cast<uint>(rec->time));
            QString txLabel = walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(rec->address));

            details.append(date.toString("M/d/yy HH:mm"));
            details.append(" ");
            details.append(formatTxStatus(rec));
            details.append(". ");
            if(!formatTxType(rec).isEmpty()) {
                details.append(formatTxType(rec));
                details.append(" ");
            }
            if(!rec->address.empty()) {
                if(txLabel.isEmpty())
                    details.append(tr("(no label)") + " ");
                else {
                    details.append("(");
                    details.append(txLabel);
                    details.append(") ");
                }
                details.append(QString::fromStdString(rec->address));
                details.append(" ");
            }
            details.append(formatTxAmount(rec, false, BitcoinUnits::separatorNever));
            return details;
        }
    case ConfirmedRole:
        return rec->status.countsForBalance;
    case FormattedAmountRole:
//用于复制/导出，因此不包括分隔符
        return formatTxAmount(rec, false, BitcoinUnits::separatorNever);
    case StatusRole:
        return rec->status.status;
    }
    return QVariant();
}

QVariant TransactionTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole)
        {
            return columns[section];
        }
        else if (role == Qt::TextAlignmentRole)
        {
            return column_alignments[section];
        } else if (role == Qt::ToolTipRole)
        {
            switch(section)
            {
            case Status:
                return tr("Transaction status. Hover over this field to show number of confirmations.");
            case Date:
                return tr("Date and time that the transaction was received.");
            case Type:
                return tr("Type of transaction.");
            case Watchonly:
                return tr("Whether or not a watch-only address is involved in this transaction.");
            case ToAddress:
                return tr("User-defined intent/purpose of the transaction.");
            case Amount:
                return tr("Amount removed from or added to balance.");
            }
        }
    }
    return QVariant();
}

QModelIndex TransactionTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    TransactionRecord *data = priv->index(walletModel->wallet(), row);
    if(data)
    {
        return createIndex(row, column, priv->index(walletModel->wallet(), row));
    }
    return QModelIndex();
}

void TransactionTableModel::updateDisplayUnit()
{
//发出datachanged以使用当前单位更新amount列
    updateAmountColumnTitle();
    Q_EMIT dataChanged(index(0, Amount), index(priv->size()-1, Amount));
}

//排队通知以显示非冻结进度对话框，例如用于重新扫描
struct TransactionNotification
{
public:
    TransactionNotification() {}
    TransactionNotification(uint256 _hash, ChangeType _status, bool _showTransaction):
        hash(_hash), status(_status), showTransaction(_showTransaction) {}

    void invoke(QObject *ttm)
    {
        QString strHash = QString::fromStdString(hash.GetHex());
        qDebug() << "NotifyTransactionChanged: " + strHash + " status= " + QString::number(status);
        QMetaObject::invokeMethod(ttm, "updateTransaction", Qt::QueuedConnection,
                                  Q_ARG(QString, strHash),
                                  Q_ARG(int, status),
                                  Q_ARG(bool, showTransaction));
    }
private:
    uint256 hash;
    ChangeType status;
    bool showTransaction;
};

static bool fQueueNotifications = false;
static std::vector< TransactionNotification > vQueueNotifications;

static void NotifyTransactionChanged(TransactionTableModel *ttm, const uint256 &hash, ChangeType status)
{
//在钱包中查找交易
//确定是否显示事务（在这里确定，以便在GUI线程中不需要重新锁定）
    bool showTransaction = TransactionRecord::showTransaction();

    TransactionNotification notification(hash, status, showTransaction);

    if (fQueueNotifications)
    {
        vQueueNotifications.push_back(notification);
        return;
    }
    notification.invoke(ttm);
}

static void ShowProgress(TransactionTableModel *ttm, const std::string &title, int nProgress)
{
    if (nProgress == 0)
        fQueueNotifications = true;

    if (nProgress == 100)
    {
        fQueueNotifications = false;
if (vQueueNotifications.size() > 10) //防止气球垃圾邮件，最多显示10个气球
            QMetaObject::invokeMethod(ttm, "setProcessingQueuedTransactions", Qt::QueuedConnection, Q_ARG(bool, true));
        for (unsigned int i = 0; i < vQueueNotifications.size(); ++i)
        {
            if (vQueueNotifications.size() - i <= 10)
                QMetaObject::invokeMethod(ttm, "setProcessingQueuedTransactions", Qt::QueuedConnection, Q_ARG(bool, false));

            vQueueNotifications[i].invoke(ttm);
        }
std::vector<TransactionNotification >().swap(vQueueNotifications); //清楚的
    }
}

void TransactionTableModel::subscribeToCoreSignals()
{
//将信号连接到钱包
    m_handler_transaction_changed = walletModel->wallet().handleTransactionChanged(std::bind(NotifyTransactionChanged, this, std::placeholders::_1, std::placeholders::_2));
    m_handler_show_progress = walletModel->wallet().handleShowProgress(std::bind(ShowProgress, this, std::placeholders::_1, std::placeholders::_2));
}

void TransactionTableModel::unsubscribeFromCoreSignals()
{
//断开钱包信号
    m_handler_transaction_changed->disconnect();
    m_handler_show_progress->disconnect();
}
