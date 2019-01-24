
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

#ifndef BITCOIN_CONSENSUS_CONSENSUS_H
#define BITCOIN_CONSENSUS_CONSENSUS_H

#include <stdlib.h>
#include <stdint.h>

/*序列化块允许的最大大小（以字节为单位）（仅限缓冲区大小限制）*/
static const unsigned int MAX_BLOCK_SERIALIZED_SIZE = 4000000;
/*块的最大允许重量，见BIP 141（网络规则）*/
static const unsigned int MAX_BLOCK_WEIGHT = 4000000;
/*块中允许的最大签名检查操作数（网络规则）*/
static const int64_t MAX_BLOCK_SIGOPS_COST = 80000;
/*CoinBase事务输出只能在新块的数目达到此数目后使用（网络规则）*/
static const int COINBASE_MATURITY = 100;

static const int WITNESS_SCALE_FACTOR = 4;

static const size_t MIN_TRANSACTION_WEIGHT = WITNESS_SCALE_FACTOR * 60; //60是有效序列化ctransaction大小的下限
static const size_t MIN_SERIALIZABLE_TRANSACTION_WEIGHT = WITNESS_SCALE_FACTOR * 10; //10是序列化ctransaction大小的下限

/*Nsequence和NlockTime锁的标志*/
/*将序列号解释为相对锁定时间约束。*/
static constexpr unsigned int LOCKTIME_VERIFY_SEQUENCE = (1 << 0);
/*对于端点时间戳，使用getMediantimePost（）而不是ntime。*/
static constexpr unsigned int LOCKTIME_MEDIAN_TIME_PAST = (1 << 1);

#endif //比特币共识
