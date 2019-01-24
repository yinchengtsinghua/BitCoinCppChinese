
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

#ifndef BITCOIN_MERKLEBLOCK_H
#define BITCOIN_MERKLEBLOCK_H

#include <serialize.h>
#include <uint256.h>
#include <primitives/block.h>
#include <bloom.h>

#include <vector>

/*表示部分Merkle树的数据结构。
 *
 *它表示已知块的TxID的子集，其方式是
 *允许在一个
 *认证方式。
 *
 *编码工作如下：我们按深度第一顺序遍历树，
 *为每个被遍历的节点存储一个位，表示该节点是否是
 *至少一个匹配的叶txid（或匹配的txid本身）的父级。在
 *如果我们处于叶级，或者该位为0，则其merkle节点散列为
 *已存储，其子项不作进一步探索。否则，没有哈希
 *已存储，但我们重复使用这两个（或唯一的）子分支。期间
 *解码时，执行相同深度的第一次遍历，消耗位和
 *在编码过程中写入哈希。
 *
 *序列化是固定的，并为
 *编码大小：
 *
 *尺寸<=10+CEIL（32.25*N）
 *
 *其中n表示部分树的叶节点数。n本身
 *受以下限制：
 *
 *n<=总交易量
 *n<=1+匹配的事务*树高
 *
 *序列化格式：
 *-uint32事务总数（4字节）
 *—散列数变量（1-3字节）
 *-uint256[]按深度一阶散列（<=32*n字节）
 *—标志位的字节数可变（1-3字节）
 *-byte[]标志位，每8个字节压缩一次，最低有效位优先（<=2*n-1位）
 *尺寸限制如下。
 **/

class CPartialMerkleTree
{
protected:
    /*块中的事务总数*/
    unsigned int nTransactions;

    /*节点是匹配txid位的父节点*/
    std::vector<bool> vBits;

    /*TxID和内部哈希*/
    std::vector<uint256> vHash;

    /*遇到无效数据时设置的标志*/
    bool fBad;

    /*帮助函数有效计算Merkle树中给定高度的节点数*/
    unsigned int CalcTreeWidth(int height) const {
        return (nTransactions+(1 << height)-1) >> height;
    }

    /*计算merkle树中节点的哈希（在叶级：txid本身）*/
    uint256 CalcHash(int height, unsigned int pos, const std::vector<uint256> &vTxid);

    /*遍历树节点的递归函数，将数据存储为位和哈希*/
    void TraverseAndBuild(int height, unsigned int pos, const std::vector<uint256> &vTxid, const std::vector<bool> &vMatch);

    /*
     *遍历树节点的递归函数，使用由TraverseAndBuild生成的位和散列。
     *返回各自节点的散列值及其相应索引。
     **/

    uint256 TraverseAndExtract(int height, unsigned int pos, unsigned int &nBitsUsed, unsigned int &nHashUsed, std::vector<uint256> &vMatch, std::vector<unsigned int> &vnIndex);

public:

    /*序列化实现*/
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nTransactions);
        READWRITE(vHash);
        std::vector<unsigned char> vBytes;
        if (ser_action.ForRead()) {
            READWRITE(vBytes);
            CPartialMerkleTree &us = *(const_cast<CPartialMerkleTree*>(this));
            us.vBits.resize(vBytes.size() * 8);
            for (unsigned int p = 0; p < us.vBits.size(); p++)
                us.vBits[p] = (vBytes[p / 8] & (1 << (p % 8))) != 0;
            us.fBad = false;
        } else {
            vBytes.resize((vBits.size()+7)/8);
            for (unsigned int p = 0; p < vBits.size(); p++)
                vBytes[p / 8] |= vBits[p] << (p % 8);
            READWRITE(vBytes);
        }
    }

    /*从事务ID列表中构造一个部分merkle树，以及一个选择其中一个子集的掩码。*/
    CPartialMerkleTree(const std::vector<uint256> &vTxid, const std::vector<bool> &vMatch);

    CPartialMerkleTree();

    /*
     *提取此部分merkle树表示的匹配txid
     *及其在部分树中的各自索引。
     *返回merkle根，如果失败则返回0。
     **/

    uint256 ExtractMatches(std::vector<uint256> &vMatch, std::vector<unsigned int> &vnIndex);

    /*获取Merkle证明所指示的交叉引用的事务数
     *本地区块链知识。
     **/

    unsigned int GetNumTransactions() const { return nTransactions; };

};


/*
 *用于中继数据块作为头+向量<merkle branch>
 *到已筛选的节点。
 *
 *注意：类假定给定的cblock至少有*1个事务。如果cblock有0个txs，它将命中断言。
 **/

class CMerkleBlock
{
public:
    /*仅用于单元测试的公共*/
    CBlockHeader header;
    CPartialMerkleTree txn;

    /*
     *仅公开用于单元测试和继电器测试（非中继）。
     *
     *仅在指定Bloom过滤器允许时使用
     *测试与Bloom过滤器匹配的事务。
     **/

    std::vector<std::pair<unsigned int, uint256> > vMatchedTxn;

    /*
     *从CBlock创建，根据筛选器筛选事务
     *请注意，这将在每个事务的筛选器上调用isrelevantandupdate，
     *因此可能会修改过滤器。
     **/

    CMerkleBlock(const CBlock& block, CBloomFilter& filter) : CMerkleBlock(block, &filter, nullptr) { }

//从cblock创建，匹配集合中的txid
    CMerkleBlock(const CBlock& block, const std::set<uint256>& txids) : CMerkleBlock(block, nullptr, &txids) { }

    CMerkleBlock() {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(header);
        READWRITE(txn);
    }

private:
//合并构造函数以合并代码
    CMerkleBlock(const CBlock& block, CBloomFilter* filter, const std::set<uint256>* txids);
};

#endif //比特币
