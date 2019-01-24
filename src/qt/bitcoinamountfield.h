
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

#ifndef BITCOIN_QT_BITCOINAMOUNTFIELD_H
#define BITCOIN_QT_BITCOINAMOUNTFIELD_H

#include <amount.h>

#include <QWidget>

class AmountSpinBox;

QT_BEGIN_NAMESPACE
class QValueComboBox;
QT_END_NAMESPACE

/*用于输入比特币金额的小部件。
  **/

class BitcoinAmountField: public QWidget
{
    Q_OBJECT

//丑陋的黑客：出于某种未知的原因，camount（而不是qint64）不能像预期的那样在这里工作。
//讨论：https://github.com/bitcoin/bitcoin/pull/5117
    Q_PROPERTY(qint64 value READ value WRITE setValue NOTIFY valueChanged USER true)

public:
    explicit BitcoinAmountField(QWidget *parent = nullptr);

    CAmount value(bool *value=nullptr) const;
    void setValue(const CAmount& value);

    /*如果allow empty设置为false，则字段将被设置为最小允许值（如果保留为空）。*/
    void SetAllowEmpty(bool allow);

    /*设置Satoshis中的最小值*/
    void SetMinValue(const CAmount& value);

    /*设置Satoshis中的最大值*/
    void SetMaxValue(const CAmount& value);

    /*在Satoshis设置单步*/
    void setSingleStep(const CAmount& step);

    /*使只读*/
    void setReadOnly(bool fReadOnly);

    /*在UI中将当前值标记为无效。*/
    void setValid(bool valid);
    /*执行输入验证，如果输入的值无效，则将字段标记为无效。*/
    bool validate();

    /*更改用于显示金额的单位。*/
    void setDisplayUnit(int unit);

    /*使字段为空并准备好新输入。*/
    void clear();

    /*启用/禁用。*/
    void setEnabled(bool fEnabled);

    /*在某些情况下，qt默认会弄乱标签链（发布https://bugreports.qt project.org/browse/qtbug-10907）。
        在这些情况下，我们必须手动设置它。
    **/

    QWidget *setupTabChain(QWidget *prev);

Q_SIGNALS:
    void valueChanged();

protected:
    /*截获事件和“，”按键时的焦点*/
    bool eventFilter(QObject *object, QEvent *event);

private:
    AmountSpinBox *amount;
    QValueComboBox *unit;

private Q_SLOTS:
    void unitChanged(int idx);

};

#endif //比特币qt_比特币金额字段
