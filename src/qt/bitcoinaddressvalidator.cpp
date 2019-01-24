
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

#include <qt/bitcoinaddressvalidator.h>

#include <key_io.h>

/*最基本的58个字符是：
     “123456789abcdefghjklmnpqrstuvwxyzabcdefghijkmnopqrstuvwxyz”

  这是：
  -除“0”以外的所有数字
  -除“I”和“O”之外的所有大写字母
  -除“L”外的所有小写字母
**/


BitcoinAddressEntryValidator::BitcoinAddressEntryValidator(QObject *parent) :
    QValidator(parent)
{
}

QValidator::State BitcoinAddressEntryValidator::validate(QString &input, int &pos) const
{
    Q_UNUSED(pos);

//空地址为“中间”输入
    if (input.isEmpty())
        return QValidator::Intermediate;

//修正
    for (int idx = 0; idx < input.size();)
    {
        bool removeChar = false;
        QChar ch = input.at(idx);
//所做的修正是非常保守的，以避免
//用户意外地避免了通常会出现的打字错误
//被检测到，从而发送到错误的地址。
        switch(ch.unicode())
        {
//Qt将这些分类为“其他格式”而不是“分隔符空间”
case 0x200B: //零宽度空间
case 0xFEFF: //零宽不间断空格
            removeChar = true;
            break;
        default:
            break;
        }

//删除空白
        if (ch.isSpace())
            removeChar = true;

//到下一个字符
        if (removeChar)
            input.remove(idx, 1);
        else
            ++idx;
    }

//验证
    QValidator::State state = QValidator::Acceptable;
    for (int idx = 0; idx < input.size(); ++idx)
    {
        int ch = input.at(idx).unicode();

        if (((ch >= '0' && ch<='9') ||
            (ch >= 'a' && ch<='z') ||
            (ch >= 'A' && ch<='Z')) &&
ch != 'I' && ch != 'O') //base58和bech32中的字符无效
        {
//字母数字，不是“禁止”字符
        }
        else
        {
            state = QValidator::Invalid;
        }
    }

    return state;
}

BitcoinAddressCheckValidator::BitcoinAddressCheckValidator(QObject *parent) :
    QValidator(parent)
{
}

QValidator::State BitcoinAddressCheckValidator::validate(QString &input, int &pos) const
{
    Q_UNUSED(pos);
//验证通过的比特币地址
    if (IsValidDestinationString(input.toStdString())) {
        return QValidator::Acceptable;
    }

    return QValidator::Invalid;
}
