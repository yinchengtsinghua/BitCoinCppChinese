
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

#ifndef BITCOIN_POLICY_RBF_H
#define BITCOIN_POLICY_RBF_H

#include <txmempool.h>

static const uint32_t MAX_BIP125_RBF_SEQUENCE = 0xfffffffd;

enum class RBFTransactionState {
    UNKNOWN,
    REPLACEABLE_BIP125,
    FINAL
};

//检查此事务上的序列号是否发出信号
//根据BIP 125，选择付费替换
bool SignalsOptInRBF(const CTransaction &tx);

//确定内存池中的事务是否正在向RBF发送选择加入信号
//根据BIP 125
//这涉及到检查事务的序列号，以及
//作为所有mempool祖先的序列号。
RBFTransactionState IsRBFOptIn(const CTransaction &tx, CTxMemPool &pool) EXCLUSIVE_LOCKS_REQUIRED(pool.cs);

#endif //比特币政策
