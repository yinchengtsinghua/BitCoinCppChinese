
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011-2015比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_QT_QVALUECOMBOBOX_H
#define BITCOIN_QT_QVALUECOMBOBOX_H

#include <QComboBox>
#include <QVariant>

/*QComboBox，可与QDataWidgetMapper一起使用，从模型中选择顺序值。*/
class QValueComboBox : public QComboBox
{
    Q_OBJECT

    Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged USER true)

public:
    explicit QValueComboBox(QWidget *parent = nullptr);

    QVariant value() const;
    void setValue(const QVariant &value);

    /*指定要用作序数值的模型角色（默认为qt:：userRole）*/
    void setRole(int role);

Q_SIGNALS:
    void valueChanged();

private:
    int role;

private Q_SLOTS:
    void handleSelectionChanged(int idx);
};

#endif //比特币qt_qValueComboBox_h
