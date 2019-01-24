
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

#ifndef BITCOIN_NET_PROCESSING_H
#define BITCOIN_NET_PROCESSING_H

#include <net.h>
#include <validationinterface.h>
#include <consensus/params.h>
#include <sync.h>

extern CCriticalSection cs_main;

/*-maxorphantx的默认值，内存中保留的最大孤立事务数*/
static const unsigned int DEFAULT_MAX_ORPHAN_TRANSACTIONS = 100;
/*默认的孤立+最近替换的txn数量，用于块重建。*/
static const unsigned int DEFAULT_BLOCK_RECONSTRUCTION_EXTRA_TXN = 100;
/*bip61的默认值（发送拒绝消息）*/
static constexpr bool DEFAULT_ENABLE_BIP61{false};

class PeerLogicValidation final : public CValidationInterface, public NetEventsInterface {
private:
    CConnman* const connman;

public:
    explicit PeerLogicValidation(CConnman* connman, CScheduler &scheduler, bool enable_bip61);

    /*
     *从cvalidationInterface重写。
     **/

    void BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindexConnected, const std::vector<CTransactionRef>& vtxConflicted) override;
    /*
     *从cvalidationInterface重写。
     **/

    void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) override;
    /*
     *从cvalidationInterface重写。
     **/

    void BlockChecked(const CBlock& block, const CValidationState& state) override;
    /*
     *从cvalidationInterface重写。
     **/

    void NewPoWValidBlock(const CBlockIndex *pindex, const std::shared_ptr<const CBlock>& pblock) override;

    /*通过将对等机添加到mapnodestate并推送请求其版本的消息来初始化对等机*/
    void InitializeNode(CNode* pnode) override;
    /*通过更新各种状态并将其从mapnodestate中删除来处理对端的删除*/
    void FinalizeNode(NodeId nodeid, bool& fUpdateConnectionTime) override;
    /*
    *处理从给定节点接收的协议消息
    *
    *@param[in]p来自接收消息的节点。
    *@param[in]用于处理线程的中断中断条件
    **/

    bool ProcessMessages(CNode* pfrom, std::atomic<bool>& interrupt) override;
    /*
    *发送要发送到给定节点的排队协议消息。
    *
    *@param[in]p将消息发送到的节点。
    *@如果还有更多工作要做，返回true
    **/

    bool SendMessages(CNode* pto) override EXCLUSIVE_LOCKS_REQUIRED(pto->cs_sendProcessing);

    /*考虑根据他们落后于我们提示的时间来驱逐出站对等机*/
    void ConsiderEviction(CNode *pto, int64_t time_in_seconds) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    /*逐出额外的出站对等机。如果我们认为我们的提示可能过时，请连接到一个额外的出站*/
    void CheckForStaleTipAndEvictPeers(const Consensus::Params &consensusParams);
    /*如果我们有额外的出站对等点，请尝试断开与最旧的块通知的连接。*/
    void EvictExtraOutboundPeers(int64_t time_in_seconds) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

private:
int64_t m_stale_tip_check_time; //！<下次检查陈旧提示

    /*启用BIP61（发送拒绝消息）*/
    const bool m_enable_bip61;
};

struct CNodeStateStats {
    int nMisbehavior = 0;
    int nSyncHeight = -1;
    int nCommonHeight = -1;
    std::vector<int> vHeightInFlight;
};

/*从节点状态获取统计信息*/
bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats);

#endif //比特币网络处理
