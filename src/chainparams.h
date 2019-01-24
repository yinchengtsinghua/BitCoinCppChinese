
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

#ifndef BITCOIN_CHAINPARAMS_H
#define BITCOIN_CHAINPARAMS_H

#include <chainparamsbase.h>
#include <consensus/params.h>
#include <primitives/block.h>
#include <protocol.h>

#include <memory>
#include <vector>

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

typedef std::map<int, uint256> MapCheckpoints;

struct CCheckpointData {
    MapCheckpoints mapCheckpoints;
};

/*
 *保存链内交易的各种统计数据。用于估计
 *链同步期间的验证进度。
 *
 *另请参见：cchainparams:：txtdata，guessverificationprogress。
 **/

struct ChainTxData {
int64_t nTime;    //！<unix最后已知事务数的时间戳
int64_t nTxCount; //！<Genesis和该时间戳之间的事务总数
double dTxRate;   //！<该时间戳之后每秒的估计事务数
};

/*
 *cchainParams定义了
 *比特币系统。有三个：人们交易货物的主要网络
 *和服务，公共测试网络，可随时重置，以及
 *仅用于专用网络的回归测试模式。它有
 *确保可以立即找到块的难度最小。
 **/

class CChainParams
{
public:
    enum Base58Type {
        PUBKEY_ADDRESS,
        SCRIPT_ADDRESS,
        SECRET_KEY,
        EXT_PUBLIC_KEY,
        EXT_SECRET_KEY,

        MAX_BASE58_TYPES
    };

    const Consensus::Params& GetConsensus() const { return consensus; }
    const CMessageHeader::MessageStartChars& MessageStart() const { return pchMessageStart; }
    int GetDefaultPort() const { return nDefaultPort; }

    const CBlock& GenesisBlock() const { return genesis; }
    /*-checkmempool和-checkblockindex参数的默认值*/
    bool DefaultConsistencyChecks() const { return fDefaultConsistencyChecks; }
    /*策略：筛选与定义良好的模式不匹配的事务*/
    bool RequireStandard() const { return fRequireStandard; }
    uint64_t PruneAfterHeight() const { return nPruneAfterHeight; }
    /*数据目录所需的最小可用空间（GB）*/
    uint64_t AssumedBlockchainSize() const { return m_assumed_blockchain_size; }
    /*修剪时数据目录所需的最小可用空间（GB）；不包括修剪皮重*/
    uint64_t AssumedChainStateSize() const { return m_assumed_chain_state_size; }
    /*发现块后，让矿工停止工作。在rpc中，在生成ngenproclimit块之前不要返回*/
    bool MineBlocksOnDemand() const { return fMineBlocksOnDemand; }
    /*返回BIP70网络字符串（MAIN、TEST或REGTEST）*/
    std::string NetworkIDString() const { return strNetworkID; }
    /*如果此网络默认启用了回退费用，则返回true*/
    bool IsFallbackFeeEnabled() const { return m_fallback_fee_enabled; }
    /*返回主机名列表以查找DNS种子*/
    const std::vector<std::string>& DNSSeeds() const { return vSeeds; }
    const std::vector<unsigned char>& Base58Prefix(Base58Type type) const { return base58Prefixes[type]; }
    const std::string& Bech32HRP() const { return bech32_hrp; }
    const std::vector<SeedSpec6>& FixedSeeds() const { return vFixedSeeds; }
    const CCheckpointData& Checkpoints() const { return checkpointData; }
    const ChainTxData& TxData() const { return chainTxData; }
protected:
    CChainParams() {}

    Consensus::Params consensus;
    CMessageHeader::MessageStartChars pchMessageStart;
    int nDefaultPort;
    uint64_t nPruneAfterHeight;
    uint64_t m_assumed_blockchain_size;
    uint64_t m_assumed_chain_state_size;
    std::vector<std::string> vSeeds;
    std::vector<unsigned char> base58Prefixes[MAX_BASE58_TYPES];
    std::string bech32_hrp;
    std::string strNetworkID;
    CBlock genesis;
    std::vector<SeedSpec6> vFixedSeeds;
    bool fDefaultConsistencyChecks;
    bool fRequireStandard;
    bool fMineBlocksOnDemand;
    CCheckpointData checkpointData;
    ChainTxData chainTxData;
    bool m_fallback_fee_enabled;
};

/*
 *创建并返回所选链的std:：unique_ptr<cchainparams>。
 *@返回所选链的CChainParams*。
 *@如果不支持链，则抛出一个std:：runtime_错误。
 **/

std::unique_ptr<const CChainParams> CreateChainParams(const std::string& chain);

/*
 *返回当前选择的参数。这在应用后不会改变
 *启动，单元测试除外。
 **/

const CChainParams &Params();

/*
 *将params（）返回的参数设置为给定bip70链名称的参数。
 *@throws std:：runtime在不支持链时出错。
 **/

void SelectParams(const std::string& chain);

#endif //比特币链参数
