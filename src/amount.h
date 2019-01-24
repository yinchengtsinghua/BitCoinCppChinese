
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

#ifndef BITCOIN_AMOUNT_H
#define BITCOIN_AMOUNT_H

#include <stdint.h>

/*Satoshis中的数量（可以是负数）*/
typedef int64_t CAmount;

static const CAmount COIN = 100000000;

/*大于此（在佐藤）的金额无效。
 *
 *请注意，这个常数是*而不是*总货币供应量，单位是比特币。
 *由于各种原因，目前刚好低于21000000 BTC，但
 *相当于健康检查。因为这种健全性检查是由共识批评使用的
 *验证码，max-money常量的确切值是一致的。
 *关键；在异常情况下，如允许的（其他）溢出错误
 *为了创造出稀薄空气中的硬币，修改可能会导致叉子。
 **/

static const CAmount MAX_MONEY = 21000000 * COIN;
inline bool MoneyRange(const CAmount& nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

#endif //比特币金额
