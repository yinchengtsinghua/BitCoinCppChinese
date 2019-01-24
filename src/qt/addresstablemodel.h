
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

#ifndef BITCOIN_QT_ADDRESSTABLEMODEL_H
#define BITCOIN_QT_ADDRESSTABLEMODEL_H

#include <QAbstractTableModel>
#include <QStringList>

enum class OutputType;

class AddressTablePriv;
class WalletModel;

namespace interfaces {
class Wallet;
}

/*
   核心地址簿的qt模型。这允许视图访问和修改通讯簿。
 **/

class AddressTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit AddressTableModel(WalletModel *parent = nullptr);
    ~AddressTableModel();

    enum ColumnIndex {
        /*el=0，/**<用户指定标签*/
        地址=1/**<比特币地址*/

    };

    enum RoleIndex {
        /*erole=qt:：userrole/**<地址类型（发送或接收）*/
    }；

    /**返回编辑/插入操作的状态*/

    enum EditStatus {
        /*/**<一切正常*/
        没有更改，/**<编辑操作期间没有更改*/

        /*alid_地址，/**<不可解析地址*/
        重复的地址，/**<地址簿中已有地址*/

        /*让您解锁失败，/**<钱包无法解锁以创建新的接收地址*/
        key_generation_failure/**<为接收地址生成新的公钥失败*/

    };

    /*tic const qstring send；/**<指定发送地址*/
    static const qstring receive；/**<指定接收地址*/


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

    /*向模型中添加地址。
       返回成功时添加的地址，否则返回空字符串。
     **/

    QString addRow(const QString &type, const QString &label, const QString &address, const OutputType address_type);

    /*在通讯簿中查找地址标签，如果找不到地址，则返回空字符串。*/
    QString labelForAddress(const QString &address) const;

    /*在通讯簿中查找地址的目的，如果找不到地址，则返回空字符串。*/
    QString purposeForAddress(const QString &address) const;

    /*在模型中查找地址的行索引。
       如果找不到，返回-1。
     **/

    int lookupAddress(const QString &address) const;

    EditStatus getEditStatus() const { return editStatus; }

    OutputType GetDefaultAddressType() const;

private:
    WalletModel* const walletModel;
    AddressTablePriv *priv = nullptr;
    QStringList columns;
    EditStatus editStatus = OK;

    /*在给定地址字符串的情况下查找通讯簿数据。*/
    bool getAddressData(const QString &address, std::string* name, std::string* purpose) const;

    /*通知侦听器数据已更改。*/
    void emitDataChanged(int index);

public Q_SLOTS:
    /*从核心更新地址列表。
     **/

    void updateEntry(const QString &address, const QString &label, bool isMine, const QString &purpose, int status);

    friend class AddressTablePriv;
};

#endif //比特币qt_地址表模型
