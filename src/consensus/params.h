
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

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include <uint256.h>
#include <limits>
#include <map>
#include <string>

namespace Consensus {

enum DeploymentPos
{
    DEPLOYMENT_TESTDUMMY,
DEPLOYMENT_CSV, //部署bip68、bip112和bip113。
DEPLOYMENT_SEGWIT, //部署bip141、bip143和bip147。
//注意：还将新部署添加到versionBits.cpp中的versionBitsDeploymentInfo
    MAX_VERSION_BITS_DEPLOYMENTS
};

/*
 *使用BIP9对每个共识规则进行更改。
 **/

struct BIP9Deployment {
    /*位位置，用于选择转换中的特定位。*/
    int bit;
    /*启动Mediantime进行版本位矿工确认。可以是过去的日期*/
    int64_t nStartTime;
    /*部署尝试的超时/过期Mediantime。*/
    int64_t nTimeout;

    /*在未来很长一段时间内保持不变。*/
    static constexpr int64_t NO_TIMEOUT = std::numeric_limits<int64_t>::max();

    /*InstartTime的特殊值，指示部署始终处于活动状态。
     *这对于测试很有用，因为这意味着测试不需要处理激活。
     *进程（至少需要3个bip9间隔）。仅测试特定测试
     *激活期间的行为不能使用此功能。*/

    static constexpr int64_t ALWAYS_ACTIVE = -1;
};

/*
 *影响连锁共识的参数。
 **/

struct Params {
    uint256 hashGenesisBlock;
    int nSubsidyHalvingInterval;
    /*阻止从BIP16强制中排除的哈希*/
    uint256 BIP16Exception;
    /*bip34激活时的块高度和哈希*/
    int BIP34Height;
    uint256 BIP34Hash;
    /*Bip65激活时的块高度*/
    int BIP65Height;
    /*Bip66激活时的块高度*/
    int BIP66Height;
    /*
     *最低区块数，包括矿工确认再定位期间2016年区块总数，
     （npowTargetTimeSpan/npowTargetSpacing）也用于BIP9部署。
     *示例：1916表示95%，1512表示测试链。
     **/

    uint32_t nRuleChangeActivationThreshold;
    uint32_t nMinerConfirmationWindow;
    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];
    /*工作参数证明*/
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;
    int64_t DifficultyAdjustmentInterval() const { return nPowTargetTimespan / nPowTargetSpacing; }
    uint256 nMinimumChainWork;
    uint256 defaultAssumeValid;
};
} //命名空间共识

#endif //比特币共识参数
