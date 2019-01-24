
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2015-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_CONSENSUS_MERKLE_H
#define BITCOIN_CONSENSUS_MERKLE_H

#include <stdint.h>
#include <vector>

#include <primitives/transaction.h>
#include <primitives/block.h>
#include <uint256.h>

uint256 ComputeMerkleRoot(std::vector<uint256> hashes, bool* mutated = nullptr);

/*
 *计算块中事务的merkle根。
 **如果找到重复的子树，则将mutated设置为true。
 **/

uint256 BlockMerkleRoot(const CBlock& block, bool* mutated = nullptr);

/*
 *计算块中见证事务的merkle根。
 **如果找到重复的子树，则将mutated设置为true。
 **/

uint256 BlockWitnessMerkleRoot(const CBlock& block, bool* mutated = nullptr);

#endif //比特币共识
