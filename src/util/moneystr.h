
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

/*
 *货币分析/格式化实用程序。
 **/

#ifndef BITCOIN_UTIL_MONEYSTR_H
#define BITCOIN_UTIL_MONEYSTR_H

#include <amount.h>
#include <attributes.h>

#include <cstdint>
#include <string>

/*不要使用这些函数来表示或分析货币金额
 *json，但使用amountFromValue和valueFromAmount。
 **/

std::string FormatMoney(const CAmount& n);
NODISCARD bool ParseMoney(const std::string& str, CAmount& nRet);
NODISCARD bool ParseMoney(const char* pszIn, CAmount& nRet);

#endif //比特币
