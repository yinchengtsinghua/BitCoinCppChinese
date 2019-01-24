
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

#ifndef BITCOIN_CONSENSUS_TX_VERIFY_H
#define BITCOIN_CONSENSUS_TX_VERIFY_H

#include <amount.h>

#include <stdint.h>
#include <vector>

class CBlockIndex;
class CCoinsViewCache;
class CTransaction;
class CValidationState;

/*事务验证功能*/

/*上下文无关的有效性检查*/
bool CheckTransaction(const CTransaction& tx, CValidationState& state, bool fCheckDuplicateInputs=true);

namespace Consensus {
/*
 *检查此交易的所有输入是否有效（无重复支出和金额）
 *这不会修改utxo集。这不会检查脚本和信号。
 *@param[out]txfee设置为交易费用（如果成功）。
 *前提条件：tx.iscoinBase（）为假。
 **/

bool CheckTxInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight, CAmount& txfee);
} //命名空间共识

/*事务验证的辅助功能（理想情况下不应公开）*/

/*
 *使用老式（0.6以前）方法计算ECDSA签名操作
 *@返回此事务的输出在花费时将产生的sigops数
 *@参见ctransaction:：fetchinputs
 **/

unsigned int GetLegacySigOpCount(const CTransaction& tx);

/*
 *计算支付脚本哈希输入中的ECDSA签名操作。
 *
 *@param[in]mapinputs我们正在支出的具有输出的以前事务的映射
 *@返回验证此事务输入所需的最大sigops数
 *@参见ctransaction:：fetchinputs
 **/

unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& mapInputs);

/*
 *计算交易的签名操作总成本。
 *@param[in]tx事务，我们正在为其计算成本
 *@param[in]输入我们正在支出的具有输出的以前事务的映射
 *@param[out]标志脚本验证标志
 *@返回Tx的签名操作总成本
 **/

int64_t GetTransactionSigOpCost(const CTransaction& tx, const CCoinsViewCache& inputs, int flags);

/*
 *检查交易是否为最终交易，是否可以包含在
 *规定的高度和时间。共识至关重要。
 **/

bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime);

/*
 *计算块高度和上一块过去的中间时间
 *在BIP 68中，该交易将被视为最终交易。
 *同时从输入高度向量中删除任何没有
 *与序列锁定输入相对应，因为它们不会影响计算。
 **/

std::pair<int, int64_t> CalculateSequenceLocks(const CTransaction &tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block);

bool EvaluateSequenceLocks(const CBlockIndex& block, std::pair<int, int64_t> lockPair);
/*
 *根据BIP 68序列号检查事务是否为最终事务，是否可以包含在一个块中。
 *共识至关重要。将Tx输入（按顺序）确认的高度列表作为输入。
 **/

bool SequenceLocks(const CTransaction &tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block);

#endif //比特币共识验证
