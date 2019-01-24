
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

#include <consensus/merkle.h>
#include <hash.h>
#include <util/strencodings.h>

/*警告！如果你读这个是因为你在学习密码
       和/或设计一个新的系统，使用梅克尔树，记住
       下面的merkle树算法有一个与
       TxID重复，导致漏洞（CVE-2012-2459）。

       原因是，如果给定时间列表中的哈希数
       是奇数，最后一个在计算下一个级别（即
       在梅克尔树上是不寻常的。这会导致
       导致相同merkle根的事务。例如，这两个
       树：

                    A A A
                  /\/\
                B C B C
               /\/\/\
              D E F D E F F F
             /\/\/\/\/\/\/\/\/\
             1 2 3 4 5 6 1 2 3 4 5 6 6

       对于事务列表[1,2,3,4,5,6]和[1,2,3,4,5,6,5,6]（其中5和
       6重复）导致相同的根散列a（因为
       （f）和（f，f）是c）。

       该漏洞是由于能够使用
       事务列表，具有与相同的merkle根和相同的块哈希
       原件无重复，验证失败。如果
       接收节点继续将该块标记为永久无效
       但是，它将无法接受进一步的未修改（因此可能
       有效）同一块的版本。我们通过检测
       我们将在列表末尾散列两个相同散列的情况
       一起，并将其视为具有无效的块
       梅克尔根假设没有双sha256冲突，这将检测所有
       在不影响merkle的情况下更改事务的已知方法
       根。
**/



uint256 ComputeMerkleRoot(std::vector<uint256> hashes, bool* mutated) {
    bool mutation = false;
    while (hashes.size() > 1) {
        if (mutated) {
            for (size_t pos = 0; pos + 1 < hashes.size(); pos += 2) {
                if (hashes[pos] == hashes[pos + 1]) mutation = true;
            }
        }
        if (hashes.size() & 1) {
            hashes.push_back(hashes.back());
        }
        SHA256D64(hashes[0].begin(), hashes[0].begin(), hashes.size() / 2);
        hashes.resize(hashes.size() / 2);
    }
    if (mutated) *mutated = mutation;
    if (hashes.size() == 0) return uint256();
    return hashes[0];
}


uint256 BlockMerkleRoot(const CBlock& block, bool* mutated)
{
    std::vector<uint256> leaves;
    leaves.resize(block.vtx.size());
    for (size_t s = 0; s < block.vtx.size(); s++) {
        leaves[s] = block.vtx[s]->GetHash();
    }
    return ComputeMerkleRoot(std::move(leaves), mutated);
}

uint256 BlockWitnessMerkleRoot(const CBlock& block, bool* mutated)
{
    std::vector<uint256> leaves;
    leaves.resize(block.vtx.size());
leaves[0].SetNull(); //CoinBase的见证哈希为0。
    for (size_t s = 1; s < block.vtx.size(); s++) {
        leaves[s] = block.vtx[s]->GetWitnessHash();
    }
    return ComputeMerkleRoot(std::move(leaves), mutated);
}

