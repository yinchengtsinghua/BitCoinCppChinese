
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <util/moneystr.h>

#include <primitives/transaction.h>
#include <tinyformat.h>
#include <util/strencodings.h>

std::string FormatMoney(const CAmount& n)
{
//注意：这里不使用直sprintf，因为我们不希望
//本地化数字格式。
    int64_t n_abs = (n > 0 ? n : -n);
    int64_t quotient = n_abs/COIN;
    int64_t remainder = n_abs%COIN;
    std::string str = strprintf("%d.%08d", quotient, remainder);

//右修剪小数点前的多余零：
    int nTrim = 0;
    for (int i = str.size()-1; (str[i] == '0' && IsDigit(str[i-2])); --i)
        ++nTrim;
    if (nTrim)
        str.erase(str.size()-nTrim, nTrim);

    if (n < 0)
        str.insert((unsigned int)0, 1, '-');
    return str;
}


bool ParseMoney(const std::string& str, CAmount& nRet)
{
    return ParseMoney(str.c_str(), nRet);
}

bool ParseMoney(const char* pszIn, CAmount& nRet)
{
    std::string strWhole;
    int64_t nUnits = 0;
    const char* p = pszIn;
    while (IsSpace(*p))
        p++;
    for (; *p; p++)
    {
        if (*p == '.')
        {
            p++;
            int64_t nMult = COIN / 10;
            while (IsDigit(*p) && (nMult > 0))
            {
                nUnits += nMult * (*p++ - '0');
                nMult /= 10;
            }
            break;
        }
        if (IsSpace(*p))
            break;
        if (!IsDigit(*p))
            return false;
        strWhole.insert(strWhole.end(), *p);
    }
    for (; *p; p++)
        if (!IsSpace(*p))
            return false;
if (strWhole.size() > 10) //防止63位溢出
        return false;
    if (nUnits < 0 || nUnits > COIN)
        return false;
    int64_t nWhole = atoi64(strWhole);
    CAmount nValue = nWhole*COIN + nUnits;

    nRet = nValue;
    return true;
}
