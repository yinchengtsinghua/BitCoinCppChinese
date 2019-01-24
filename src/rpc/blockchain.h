
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2017-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_RPC_BLOCKCHAIN_H
#define BITCOIN_RPC_BLOCKCHAIN_H

#include <vector>
#include <stdint.h>
#include <amount.h>

class CBlock;
class CBlockIndex;
class UniValue;

static constexpr int NUM_GETBLOCKSTATS_PERCENTILES = 5;

/*
 *获得给定块索引的净WRT难度。
 *
 *@返回一个浮点数，该浮点数是主网络最小值的倍数
 *难度（4295032833哈希）。
 **/

double GetDifficulty(const CBlockIndex* blockindex);

/*块提示更改时的回调。*/
void RPCNotifyBlockChange(bool ibd, const CBlockIndex *);

/*JSON的块描述*/
UniValue blockToJSON(const CBlock& block, const CBlockIndex* tip, const CBlockIndex* blockindex, bool txDetails = false);

/*向JSON发送MEMPOOL信息*/
UniValue mempoolInfoToJSON();

/*内存池到JSON*/
UniValue mempoolToJSON(bool fVerbose = false);

/*块头到JSON*/
UniValue blockheaderToJSON(const CBlockIndex* tip, const CBlockIndex* blockindex);

/*由getblockstats用于按重量获取不同百分比的feerates*/
void CalculatePercentilesByWeight(CAmount result[NUM_GETBLOCKSTATS_PERCENTILES], std::vector<std::pair<CAmount, int64_t>>& scores, int64_t total_weight);

#endif
