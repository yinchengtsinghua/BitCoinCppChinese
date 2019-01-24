
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

#include <qt/addresstablemodel.h>

#include <qt/guiutil.h>
#include <qt/walletmodel.h>

#include <interfaces/node.h>
#include <key_io.h>
#include <wallet/wallet.h>

#include <QFont>
#include <QDebug>

const QString AddressTableModel::Send = "S";
const QString AddressTableModel::Receive = "R";

struct AddressTableEntry
{
    enum Type {
        Sending,
        Receiving,
        /*den/*qsortfilterproxymodel将过滤掉这些*/
    }；

    类型类型；
    QSPLAN标签；
    Q字符串地址；

    AddressTableEntry（）
    AddressTableEntry（类型类型，const qstring&_标签，const qstring&_地址）：
        类型（_type），标签（_label），地址（_address）
}；

结构AddressTableEntryLessThan
{
    bool operator（）（const-addressTableEntry&A，const-addressTableEntry&B）const
    {
        返回a.address<b.address；
    }
    bool operator（）（const-addressTableEntry&A，const-qString&B）const
    {
        返回a.address<b；
    }
    bool operator（）（const qstring&a，const addressTableEntry&b）const
    {
        返回A<B.地址；
    }
}；

/*根据地址目的确定地址类型*/

static AddressTableEntry::Type translateTransactionType(const QString &strPurpose, bool isMine)
{
    AddressTableEntry::Type addressType = AddressTableEntry::Hidden;
//没有显示“退款”地址，更改地址也根本不在地图地址簿中。
    if (strPurpose == "send")
        addressType = AddressTableEntry::Sending;
    else if (strPurpose == "receive")
        addressType = AddressTableEntry::Receiving;
else if (strPurpose == "unknown" || strPurpose == "") //如果目的不确定，猜猜看
        addressType = (isMine ? AddressTableEntry::Receiving : AddressTableEntry::Sending);
    return addressType;
}

//私有实施
class AddressTablePriv
{
public:
    QList<AddressTableEntry> cachedAddressTable;
    AddressTableModel *parent;

    explicit AddressTablePriv(AddressTableModel *_parent):
        parent(_parent) {}

    void refreshAddressTable(interfaces::Wallet& wallet)
    {
        cachedAddressTable.clear();
        {
            for (const auto& address : wallet.getAddresses())
            {
                AddressTableEntry::Type addressType = translateTransactionType(
                        QString::fromStdString(address.purpose), address.is_mine);
                cachedAddressTable.append(AddressTableEntry(addressType,
                                  QString::fromStdString(address.name),
                                  QString::fromStdString(EncodeDestination(address.dest))));
            }
        }
//qLowerBound（）和qupperbound（）要求我们的cachedDresStable列表按asc顺序排序
//即使地图已经排序，也需要重新排序步骤，因为原始地图
//按二进制地址排序，而不是按base58（）地址排序。
        qSort(cachedAddressTable.begin(), cachedAddressTable.end(), AddressTableEntryLessThan());
    }

    void updateEntry(const QString &address, const QString &label, bool isMine, const QString &purpose, int status)
    {
//在模型中查找地址/标签
        QList<AddressTableEntry>::iterator lower = qLowerBound(
            cachedAddressTable.begin(), cachedAddressTable.end(), address, AddressTableEntryLessThan());
        QList<AddressTableEntry>::iterator upper = qUpperBound(
            cachedAddressTable.begin(), cachedAddressTable.end(), address, AddressTableEntryLessThan());
        int lowerIndex = (lower - cachedAddressTable.begin());
        int upperIndex = (upper - cachedAddressTable.begin());
        bool inModel = (lower != upper);
        AddressTableEntry::Type newEntryType = translateTransactionType(purpose, isMine);

        switch(status)
        {
        case CT_NEW:
            if(inModel)
            {
                qWarning() << "AddressTablePriv::updateEntry: Warning: Got CT_NEW, but entry is already in model";
                break;
            }
            parent->beginInsertRows(QModelIndex(), lowerIndex, lowerIndex);
            cachedAddressTable.insert(lowerIndex, AddressTableEntry(newEntryType, label, address));
            parent->endInsertRows();
            break;
        case CT_UPDATED:
            if(!inModel)
            {
                qWarning() << "AddressTablePriv::updateEntry: Warning: Got CT_UPDATED, but entry is not in model";
                break;
            }
            lower->type = newEntryType;
            lower->label = label;
            parent->emitDataChanged(lowerIndex);
            break;
        case CT_DELETED:
            if(!inModel)
            {
                qWarning() << "AddressTablePriv::updateEntry: Warning: Got CT_DELETED, but entry is not in model";
                break;
            }
            parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex-1);
            cachedAddressTable.erase(lower, upper);
            parent->endRemoveRows();
            break;
        }
    }

    int size()
    {
        return cachedAddressTable.size();
    }

    AddressTableEntry *index(int idx)
    {
        if(idx >= 0 && idx < cachedAddressTable.size())
        {
            return &cachedAddressTable[idx];
        }
        else
        {
            return nullptr;
        }
    }
};

AddressTableModel::AddressTableModel(WalletModel *parent) :
    QAbstractTableModel(parent), walletModel(parent)
{
    columns << tr("Label") << tr("Address");
    priv = new AddressTablePriv(this);
    priv->refreshAddressTable(parent->wallet());
}

AddressTableModel::~AddressTableModel()
{
    delete priv;
}

int AddressTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int AddressTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant AddressTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    AddressTableEntry *rec = static_cast<AddressTableEntry*>(index.internalPointer());

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch(index.column())
        {
        case Label:
            if(rec->label.isEmpty() && role == Qt::DisplayRole)
            {
                return tr("(no label)");
            }
            else
            {
                return rec->label;
            }
        case Address:
            return rec->address;
        }
    }
    else if (role == Qt::FontRole)
    {
        QFont font;
        if(index.column() == Address)
        {
            font = GUIUtil::fixedPitchFont();
        }
        return font;
    }
    else if (role == TypeRole)
    {
        switch(rec->type)
        {
        case AddressTableEntry::Sending:
            return Send;
        case AddressTableEntry::Receiving:
            return Receive;
        default: break;
        }
    }
    return QVariant();
}

bool AddressTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(!index.isValid())
        return false;
    AddressTableEntry *rec = static_cast<AddressTableEntry*>(index.internalPointer());
    std::string strPurpose = (rec->type == AddressTableEntry::Sending ? "send" : "receive");
    editStatus = OK;

    if(role == Qt::EditRole)
    {
        CTxDestination curAddress = DecodeDestination(rec->address.toStdString());
        if(index.column() == Label)
        {
//如果旧标签==新标签，则不执行任何操作
            if(rec->label == value.toString())
            {
                editStatus = NO_CHANGES;
                return false;
            }
            walletModel->wallet().setAddressBook(curAddress, value.toString().toStdString(), strPurpose);
        } else if(index.column() == Address) {
            CTxDestination newAddress = DecodeDestination(value.toString().toStdString());
//拒绝设置无效地址，设置错误状态并返回false
            if(boost::get<CNoDestination>(&newAddress))
            {
                editStatus = INVALID_ADDRESS;
                return false;
            }
//如果旧地址==新地址，则不执行任何操作
            else if(newAddress == curAddress)
            {
                editStatus = NO_CHANGES;
                return false;
            }
//如果尝试，请检查重复地址以防止意外删除地址。
//将现有地址粘贴到另一个地址上（使用不同的标签）
            if (walletModel->wallet().getAddress(
                    /*地址，/*名称=*/nullptr，/*是\u mine=*/nullptr，/*用途=*/nullptr）
            {
                editstatus=重复的地址；
                返回错误；
            }
            //再次检查我们没有覆盖接收地址
            else if（rec->type==addressTableEntry:：sending）
            {
                //删除旧条目
                walletmodel->wallet（）.deladdressbook（curaddress）；
                //添加带有新地址的新条目
                walletmodel->wallet（）.setaddressbook（newaddress，value.toString（）.tostdstring（），strpurpose）；
            }
        }
        回归真实；
    }
    返回错误；
}

qvariant addressTableModel:：HeaderData（int部分，qt:：定向，int角色）const
{
    如果（方向==qt：：水平）
    {
        if（role==qt:：displayRole&&section<columns.size（））
        {
            返回列[节]；
        }
    }
    返回qvariant（）；
}

qt:：itemFlags addressTableModel:：Flags（const-qmodelIndex&index）const
{
    如果（！）index.isvalid（））返回qt:：noitemFlags；

    addressTableEntry*rec=static_cast<addressTableEntry*>（index.internalPointer（））；

    qt:：itemFlags retval=qt:：itemisselectable qt:：itemisEnabled；
    //可以编辑发送地址的地址和标签，
    //并且仅用于接收地址的标签。
    if（rec->type==addressTableEntry:：sending
      （rec->type==addressTableEntry:：receiving&&index.column（）==label））
    {
        retval=qt:：itemieditable；
    }
    返回；
}

qmodelindex addressTableModel:：index（int row，int column，const qmodelindex&parent）const
{
    q_未使用（父级）；
    addressTableEntry*data=priv->index（row）；
    如果（数据）
    {
        返回createindex（row，column，priv->index（row））；
    }
    其他的
    {
        返回qmodelIndex（）；
    }
}

void addressTableModel:：updateEntry（const qstring&address，
        const qstring&label、bool ismine、const qstring&purpose、int status）
{
    //从比特币核心更新通讯簿模型
    priv->updateEntry（地址、标签、ismine、用途、状态）；
}

qstring addressTableModel:：addRow（const qstring&type、const qstring&label、const qstring&address、const outputtype address_type）
{
    std:：string strlabel=label.tostdstring（）；
    std:：string straddress=address.tostdstring（）；

    editStatus=确定；

    if（类型==send）
    {
        如果（！）WalletModel->ValidateAddress（地址）
        {
            editstatus=无效的地址；
            返回qString（）；
        }
        //检查重复地址
        {
            if（walletmodel->wallet（）.getaddress（）。
                    解码目的地（跨地址），/*名称=*/ nullptr, /* is_mine= */ nullptr, /* purpose= */ nullptr))

            {
                editStatus = DUPLICATE_ADDRESS;
                return QString();
            }
        }
    }
    else if(type == Receive)
    {
//生成与给定标签关联的新地址
        CPubKey newKey;
        /*！walletmodel->wallet（）.getkeyfrompool（false/*internal*/，newkey））
        {
            walletmodel:：unlockContext ctx（walletmodel->requestUnlock（））；
            如果（！）验证（）
            {
                //解锁钱包失败或被取消
                editstatus=钱包解锁失败；
                返回qString（）；
            }
            如果（！）WalletModel->Wallet（）.GetKeyFromPool（错误/*内部*/, newKey))

            {
                editStatus = KEY_GENERATION_FAILURE;
                return QString();
            }
        }
        walletModel->wallet().learnRelatedScripts(newKey, address_type);
        strAddress = EncodeDestination(GetDestinationForKey(newKey, address_type));
    }
    else
    {
        return QString();
    }

//添加条目
    walletModel->wallet().setAddressBook(DecodeDestination(strAddress), strLabel,
                           (type == Send ? "send" : "receive"));
    return QString::fromStdString(strAddress);
}

bool AddressTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent);
    AddressTableEntry *rec = priv->index(row);
    if(count != 1 || !rec || rec->type == AddressTableEntry::Receiving)
    {
//一次只能删除一行，不能删除不在模型中的行。
//同时拒绝删除接收地址。
        return false;
    }
    walletModel->wallet().delAddressBook(DecodeDestination(rec->address.toStdString()));
    return true;
}

QString AddressTableModel::labelForAddress(const QString &address) const
{
    std::string name;
    /*（getaddressdata（address，&name，/*purpose=*/nullptr））
        返回qstring:：fromstdstring（name）；
    }
    返回qString（）；
}

qstring addressTableModel:：purposeForAddress（const qstring&address）const
{
    std：：字符串用途；
    if（getAddressData（地址，/*名称=*/ nullptr, &purpose)) {

        return QString::fromStdString(purpose);
    }
    return QString();
}

bool AddressTableModel::getAddressData(const QString &address,
        std::string* name,
        std::string* purpose) const {
    CTxDestination destination = DecodeDestination(address.toStdString());
    /*urn walletmodel->wallet（）.getaddress（目的地，名称，/*是\u mine=*/nullptr，目的地）；
}

int addressTableModel：：查找地址（const qstring&address）const
{
    qmodelIndexList lst=匹配（index（0，address，qmodelIndex（）），
                                qt:：editRole，地址，1，qt:：matchExactly）；
    if（lst.isEmpty（））
    {
        返回- 1；
    }
    其他的
    {
        返回lst.at（0）.row（）；
    }
}

outputtype addressTableModel:：getDefaultAddressType（）const返回walletModel->wallet（）.getDefaultAddressType（）；

void addressTableModel:：EmitDataChanged（int idx）
{
    Q_emit datachanged（index（idx，0，qmodelIndex（）），index（idx，columns.length（）-1，qmodelIndex（））；
}
