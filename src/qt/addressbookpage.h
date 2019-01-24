
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

#ifndef BITCOIN_QT_ADDRESSBOOKPAGE_H
#define BITCOIN_QT_ADDRESSBOOKPAGE_H

#include <QDialog>

class AddressBookSortFilterProxyModel;
class AddressTableModel;
class PlatformStyle;

namespace Ui {
    class AddressBookPage;
}

QT_BEGIN_NAMESPACE
class QItemSelection;
class QMenu;
class QModelIndex;
QT_END_NAMESPACE

/*显示发送或接收地址列表的小部件。
  **/

class AddressBookPage : public QDialog
{
    Q_OBJECT

public:
    enum Tabs {
        SendingTab = 0,
        ReceivingTab = 1
    };

    enum Mode {
        /*selection，/**<打开通讯簿选择地址*/
        for editing/**<打开通讯簿进行编辑*/

    };

    explicit AddressBookPage(const PlatformStyle *platformStyle, Mode mode, Tabs tab, QWidget *parent = nullptr);
    ~AddressBookPage();

    void setModel(AddressTableModel *model);
    const QString &getReturnValue() const { return returnValue; }

public Q_SLOTS:
    void done(int retval);

private:
    Ui::AddressBookPage *ui;
    AddressTableModel *model;
    Mode mode;
    Tabs tab;
    QString returnValue;
    AddressBookSortFilterProxyModel *proxyModel;
    QMenu *contextMenu;
QAction *deleteAction; //能够明确地禁用它
    QString newAddressToSelect;

private Q_SLOTS:
    /*删除当前选定的地址项*/
    void on_deleteAddress_clicked();
    /*创建接收硬币的新地址和/或添加新的通讯簿条目*/
    void on_newAddress_clicked();
    /*将当前选定地址项的地址复制到剪贴板*/
    void on_copyAddress_clicked();
    /*将当前选定地址项的标签复制到剪贴板（无按钮）*/
    void onCopyLabelAction();
    /*编辑当前选定的地址条目（无按钮）*/
    void onEditAction();
    /*已单击导出按钮*/
    void on_exportButton_clicked();

    /*根据所选选项卡和所选内容设置按钮状态*/
    void selectionChanged();
    /*为通讯簿条目生成上下文菜单（鼠标右键菜单）*/
    void contextualMenu(const QPoint &point);
    /*新条目/条目已添加到地址表中*/
    /*d selectnewaddress（const qmodelindex&parent，int begin，int/*end*/）；

QY信号：
    空发送硬币（qstring addr）；
}；

endif//比特币\u qt \u地址簿页\u h
