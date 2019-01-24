
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2016-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_VERSIONBITS_H
#define BITCOIN_VERSIONBITS_H

#include <chain.h>
#include <map>

/*用于新块的块版本（pre-versionBits）*/
static const int32_t VERSIONBITS_LAST_OLD_BLOCK_VERSION = 4;
/*版本位块版本中要设置的位*/
static const int32_t VERSIONBITS_TOP_BITS = 0x20000000UL;
/*什么位掩码决定版本位是否在使用中*/
static const int32_t VERSIONBITS_TOP_MASK = 0xE0000000UL;
/*可用于版本位的总位*/
static const int32_t VERSIONBITS_NUM_BITS = 29;

enum class ThresholdState {
    DEFINED,
    STARTED,
    LOCKED_IN,
    ACTIVE,
    FAILED,
};

//给出高度为句点（）倍数的块的状态的映射。
//但是，映射是由块的父级索引的，因此映射中的所有键
//将是nullptr或一个（height+1）%period（）==0的块。
typedef std::map<const CBlockIndex*, ThresholdState> ThresholdConditionCache;

struct BIP9Stats {
    int period;
    int threshold;
    int elapsed;
    int count;
    bool possible;
};

/*
 *实现bip9样式阈值逻辑并缓存结果的抽象类。
 **/

class AbstractThresholdConditionChecker {
protected:
    virtual bool Condition(const CBlockIndex* pindex, const Consensus::Params& params) const =0;
    virtual int64_t BeginTime(const Consensus::Params& params) const =0;
    virtual int64_t EndTime(const Consensus::Params& params) const =0;
    virtual int Period(const Consensus::Params& params) const =0;
    virtual int Threshold(const Consensus::Params& params) const =0;

public:
    BIP9Stats GetStateStatisticsFor(const CBlockIndex* pindex, const Consensus::Params& params) const;
//请注意，下面的函数以pindexprev作为输入：它们根据块B的父级计算块B的信息。
    ThresholdState GetStateFor(const CBlockIndex* pindexPrev, const Consensus::Params& params, ThresholdConditionCache& cache) const;
    int GetStateSinceHeightFor(const CBlockIndex* pindexPrev, const Consensus::Params& params, ThresholdConditionCache& cache) const;
};

struct VersionBitsCache
{
    ThresholdConditionCache caches[Consensus::MAX_VERSION_BITS_DEPLOYMENTS];

    void Clear();
};

ThresholdState VersionBitsState(const CBlockIndex* pindexPrev, const Consensus::Params& params, Consensus::DeploymentPos pos, VersionBitsCache& cache);
BIP9Stats VersionBitsStatistics(const CBlockIndex* pindexPrev, const Consensus::Params& params, Consensus::DeploymentPos pos);
int VersionBitsStateSinceHeight(const CBlockIndex* pindexPrev, const Consensus::Params& params, Consensus::DeploymentPos pos, VersionBitsCache& cache);
uint32_t VersionBitsMask(const Consensus::Params& params, Consensus::DeploymentPos pos);

#endif //比特币版本比特币
