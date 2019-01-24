
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

#ifndef BITCOIN_QT_BITCOINUNITS_H
#define BITCOIN_QT_BITCOINUNITS_H

#include <amount.h>

#include <QAbstractListModel>
#include <QString>

//U+2009瘦空间=UTF-8 E2 80 89
#define REAL_THIN_SP_CP 0x2009
#define REAL_THIN_SP_UTF8 "\xE2\x80\x89"
#define REAL_THIN_SP_HTML "&thinsp;"

//U+200A发间=UTF-8 E2 80 8A
#define HAIR_SP_CP 0x200A
#define HAIR_SP_UTF8 "\xE2\x80\x8A"
#define HAIR_SP_HTML "&#8202;"

//U+2006 6-per-em空格=utf-8 e2 80 86
#define SIXPEREM_SP_CP 0x2006
#define SIXPEREM_SP_UTF8 "\xE2\x80\x86"
#define SIXPEREM_SP_HTML "&#8198;"

//U+2007数字空间=UTF-8 E2 80 87
#define FIGURE_SP_CP 0x2007
#define FIGURE_SP_UTF8 "\xE2\x80\x87"
#define FIGURE_SP_HTML "&#8199;"

//qmessagebox似乎有一个bug，它不显示细/发空间。
//正确地。解决方法是以小字体显示空间。如果你
//更改此项，请测试它是否不会导致父跨度开始
//包装。
#define HTML_HACK_SP "<span style='white-space: nowrap; font-size: 6pt'> </span>"

//定义薄变量作为我们首选的薄空间类型
#define THIN_SP_CP   REAL_THIN_SP_CP
#define THIN_SP_UTF8 REAL_THIN_SP_UTF8
#define THIN_SP_HTML HTML_HACK_SP

/*比特币单位定义。封装解析和格式化
   作为下拉选择框的列表模型。
**/

class BitcoinUnits: public QAbstractListModel
{
    Q_OBJECT

public:
    explicit BitcoinUnits(QObject *parent);

    /*比特币单位。
      @备注来源：https://en.bitcoin.it/wiki/units。请只加合理的
     **/

    enum Unit
    {
        BTC,
        mBTC,
        uBTC,
        SAT
    };

    enum SeparatorStyle
    {
        separatorNever,
        separatorStandard,
        separatorAlways
    };

//！@name静态API
//！单位转换和格式设置
///@

//！获取单位列表，用于下拉框
    static QList<Unit> availableUnits();
//！单位ID有效吗？
    static bool valid(int unit);
//！长名
    static QString longName(int unit);
//！短名称
    static QString shortName(int unit);
//！更长的描述
    static QString description(int unit);
//！每台机组的饱和（1e-8）数量
    static qint64 factor(int unit);
//！剩余小数位数
    static int decimals(int unit);
//！格式化为字符串
    static QString format(int unit, const CAmount& amount, bool plussign=false, SeparatorStyle separators=separatorStandard);
//！格式为字符串（带单位）
    static QString formatWithUnit(int unit, const CAmount& amount, bool plussign=false, SeparatorStyle separators=separatorStandard);
//！格式为HTML字符串（带单位）
    static QString formatHtmlWithUnit(int unit, const CAmount& amount, bool plussign=false, SeparatorStyle separators=separatorStandard);
//！将字符串解析为硬币数量
    static bool parse(int unit, const QString &value, CAmount *val_out);
//！获取包含当前显示单位的amount列的标题，条件是OptionsModel引用可用*/
    static QString getAmountColumnTitle(int unit);
///@

//！@name abstractlistmodel实现
//！单位下拉选择框的列表模型。
///@
    enum RoleIndex {
        /*单位标识符*/
        UnitRole = Qt::UserRole
    };
    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
///@

    static QString removeSpaces(QString text)
    {
        text.remove(' ');
        text.remove(QChar(THIN_SP_CP));
#if (THIN_SP_CP != REAL_THIN_SP_CP)
        text.remove(QChar(REAL_THIN_SP_CP));
#endif
        return text;
    }

//！返回最大基本单位数（Satoshis）
    static CAmount maxMoney();

private:
    QList<BitcoinUnits::Unit> unitlist;
};
typedef BitcoinUnits::Unit BitcoinUnit;

#endif //比特币单位
