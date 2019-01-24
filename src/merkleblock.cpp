
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

#include <merkleblock.h>

#include <hash.h>
#include <consensus/consensus.h>
#include <util/strencodings.h>


CMerkleBlock::CMerkleBlock(const CBlock& block, CBloomFilter* filter, const std::set<uint256>* txids)
{
    header = block.GetBlockHeader();

    std::vector<bool> vMatch;
    std::vector<uint256> vHashes;

    vMatch.reserve(block.vtx.size());
    vHashes.reserve(block.vtx.size());

    for (unsigned int i = 0; i < block.vtx.size(); i++)
    {
        const uint256& hash = block.vtx[i]->GetHash();
        if (txids && txids->count(hash)) {
            vMatch.push_back(true);
        } else if (filter && filter->IsRelevantAndUpdate(*block.vtx[i])) {
            vMatch.push_back(true);
            vMatchedTxn.emplace_back(i, hash);
        } else {
            vMatch.push_back(false);
        }
        vHashes.push_back(hash);
    }

    txn = CPartialMerkleTree(vHashes, vMatch);
}

uint256 CPartialMerkleTree::CalcHash(int height, unsigned int pos, const std::vector<uint256> &vTxid) {
//我们不能在梅克尔区有零Tx，我们总是需要CoinBase Tx。
//如果我们没有这个断言，我们可以在索引到vtxid时遇到内存访问冲突。
    assert(vTxid.size() != 0);
    if (height == 0) {
//高度0处的哈希是txids themself
        return vTxid[pos];
    } else {
//计算左哈希
        uint256 left = CalcHash(height-1, pos*2, vTxid), right;
//如果不超过数组结尾，则计算右哈希-否则复制左哈希
        if (pos*2+1 < CalcTreeWidth(height-1))
            right = CalcHash(height-1, pos*2+1, vTxid);
        else
            right = left;
//合并子灰
        return Hash(left.begin(), left.end(), right.begin(), right.end());
    }
}

void CPartialMerkleTree::TraverseAndBuild(int height, unsigned int pos, const std::vector<uint256> &vTxid, const std::vector<bool> &vMatch) {
//确定此节点是否是至少一个匹配的TxID的父节点
    bool fParentOfMatch = false;
    for (unsigned int p = pos << height; p < (pos+1) << height && p < nTransactions; p++)
        fParentOfMatch |= vMatch[p];
//存储为标志位
    vBits.push_back(fParentOfMatch);
    if (height==0 || !fParentOfMatch) {
//如果高度为0，或者下面没有有趣的内容，则存储哈希并停止
        vHash.push_back(CalcHash(height, pos, vTxid));
    } else {
//否则，不要存储任何哈希，而是下放到子树中
        TraverseAndBuild(height-1, pos*2, vTxid, vMatch);
        if (pos*2+1 < CalcTreeWidth(height-1))
            TraverseAndBuild(height-1, pos*2+1, vTxid, vMatch);
    }
}

uint256 CPartialMerkleTree::TraverseAndExtract(int height, unsigned int pos, unsigned int &nBitsUsed, unsigned int &nHashUsed, std::vector<uint256> &vMatch, std::vector<unsigned int> &vnIndex) {
    if (nBitsUsed >= vBits.size()) {
//位数组溢出-失败
        fBad = true;
        return uint256();
    }
    bool fParentOfMatch = vBits[nBitsUsed++];
    if (height==0 || !fParentOfMatch) {
//如果高度为0，或者下面没有有趣的内容，请使用存储的哈希，不要下降
        if (nHashUsed >= vHash.size()) {
//散列数组溢出-失败
            fBad = true;
            return uint256();
        }
        const uint256 &hash = vHash[nHashUsed++];
if (height==0 && fParentOfMatch) { //如果高度为0，我们有一个匹配的txid
            vMatch.push_back(hash);
            vnIndex.push_back(pos);
        }
        return hash;
    } else {
//否则，下放到子树中提取匹配的txid和散列
        uint256 left = TraverseAndExtract(height-1, pos*2, nBitsUsed, nHashUsed, vMatch, vnIndex), right;
        if (pos*2+1 < CalcTreeWidth(height-1)) {
            right = TraverseAndExtract(height-1, pos*2+1, nBitsUsed, nHashUsed, vMatch, vnIndex);
            if (right == left) {
//左右分支不应与事务相同
//它们所包含的哈希值必须是唯一的。
                fBad = true;
            }
        } else {
            right = left;
        }
//在返回前将它们组合起来
        return Hash(left.begin(), left.end(), right.begin(), right.end());
    }
}

CPartialMerkleTree::CPartialMerkleTree(const std::vector<uint256> &vTxid, const std::vector<bool> &vMatch) : nTransactions(vTxid.size()), fBad(false) {
//复位状态
    vBits.clear();
    vHash.clear();

//计算树的高度
    int nHeight = 0;
    while (CalcTreeWidth(nHeight) > 1)
        nHeight++;

//遍历部分树
    TraverseAndBuild(nHeight, 0, vTxid, vMatch);
}

CPartialMerkleTree::CPartialMerkleTree() : nTransactions(0), fBad(true) {}

uint256 CPartialMerkleTree::ExtractMatches(std::vector<uint256> &vMatch, std::vector<unsigned int> &vnIndex) {
    vMatch.clear();
//空的集合不起作用
    if (nTransactions == 0)
        return uint256();
//检查交易数量是否过高
    if (nTransactions > MAX_BLOCK_WEIGHT / MIN_TRANSACTION_WEIGHT)
        return uint256();
//为每个txid提供的哈希数不能超过一个。
    if (vHash.size() > nTransactions)
        return uint256();
//部分树中每个节点必须至少有一个位，每个哈希必须至少有一个节点。
    if (vBits.size() < vHash.size())
        return uint256();
//计算树的高度
    int nHeight = 0;
    while (CalcTreeWidth(nHeight) > 1)
        nHeight++;
//遍历部分树
    unsigned int nBitsUsed = 0, nHashUsed = 0;
    uint256 hashMerkleRoot = TraverseAndExtract(nHeight, 0, nBitsUsed, nHashUsed, vMatch, vnIndex);
//验证树遍历期间没有出现问题
    if (fBad)
        return uint256();
//验证是否使用了所有位（将其序列化为字节序列所导致的填充除外）
    if ((nBitsUsed+7)/8 != (vBits.size()+7)/8)
        return uint256();
//验证是否已使用所有哈希
    if (nHashUsed != vHash.size())
        return uint256();
    return hashMerkleRoot;
}
