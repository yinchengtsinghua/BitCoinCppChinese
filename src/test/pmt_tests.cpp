
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2012-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <consensus/merkle.h>
#include <merkleblock.h>
#include <serialize.h>
#include <streams.h>
#include <uint256.h>
#include <arith_uint256.h>
#include <version.h>
#include <test/test_bitcoin.h>

#include <vector>

#include <boost/test/unit_test.hpp>

class CPartialMerkleTreeTester : public CPartialMerkleTree
{
public:
//在其中一个散列中翻转一位-这将中断身份验证
    void Damage() {
        unsigned int n = InsecureRandRange(vHash.size());
        int bit = InsecureRandBits(8);
        *(vHash[n].begin() + (bit>>3)) ^= 1<<(bit&7);
    }
};

BOOST_FIXTURE_TEST_SUITE(pmt_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(pmt_test1)
{
    SeedInsecureRand(false);
    static const unsigned int nTxCounts[] = {1, 4, 7, 17, 56, 100, 127, 256, 312, 513, 1000, 4095};

    for (int i = 0; i < 12; i++) {
        unsigned int nTx = nTxCounts[i];

//使用一些虚拟事务构建块
        CBlock block;
        for (unsigned int j=0; j<nTx; j++) {
            CMutableTransaction tx;
tx.nLockTime = j; //实际事务数据并不重要；只需使nlocktime的唯一性
            block.vtx.push_back(MakeTransactionRef(std::move(tx)));
        }

//计算实际梅克尔根和高度
        uint256 merkleRoot1 = BlockMerkleRoot(block);
        std::vector<uint256> vTxid(nTx, uint256());
        for (unsigned int j=0; j<nTx; j++)
            vTxid[j] = block.vtx[j]->GetHash();
        int nHeight = 1, nTx_ = nTx;
        while (nTx_ > 1) {
            nTx_ = (nTx_+1)/2;
            nHeight++;
        }

//用随机子集检查包含机会1，1/2，1/4，…，1/128
        for (int att = 1; att < 15; att++) {
//构建TxID的随机子集
            std::vector<bool> vMatch(nTx, false);
            std::vector<uint256> vMatchTxid1;
            for (unsigned int j=0; j<nTx; j++) {
                bool fInclude = InsecureRandBits(att / 2) == 0;
                vMatch[j] = fInclude;
                if (fInclude)
                    vMatchTxid1.push_back(vTxid[j]);
            }

//建立部分梅克尔树
            CPartialMerkleTree pmt1(vTxid, vMatch);

//串行化
            CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            ss << pmt1;

//验证CPartialmerkletree的大小保证
            unsigned int n = std::min<unsigned int>(nTx, 1 + vMatchTxid1.size()*nHeight);
            BOOST_CHECK(ss.size() <= 10 + (258*n+7)/8);

//反序列化为测试仪副本
            CPartialMerkleTreeTester pmt2;
            ss >> pmt2;

//从副本中提取merkle根和匹配的txid
            std::vector<uint256> vMatchTxid2;
            std::vector<unsigned int> vIndex;
            uint256 merkleRoot2 = pmt2.ExtractMatches(vMatchTxid2, vIndex);

//检查它是否有与原始的相同的merkle根，以及一个有效的根。
            BOOST_CHECK(merkleRoot1 == merkleRoot2);
            BOOST_CHECK(!merkleRoot2.IsNull());

//检查它是否包含匹配的事务（顺序相同！）
            BOOST_CHECK(vMatchTxid1 == vMatchTxid2);

//检查随机位翻转是否破坏身份验证
            for (int j=0; j<4; j++) {
                CPartialMerkleTreeTester pmt3(pmt2);
                pmt3.Damage();
                std::vector<uint256> vMatchTxid3;
                uint256 merkleRoot3 = pmt3.ExtractMatches(vMatchTxid3, vIndex);
                BOOST_CHECK(merkleRoot3 != merkleRoot1);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(pmt_malleability)
{
    std::vector<uint256> vTxid = {
        ArithToUint256(1), ArithToUint256(2),
        ArithToUint256(3), ArithToUint256(4),
        ArithToUint256(5), ArithToUint256(6),
        ArithToUint256(7), ArithToUint256(8),
        ArithToUint256(9), ArithToUint256(10),
        ArithToUint256(9), ArithToUint256(10),
    };
    std::vector<bool> vMatch = {false, false, false, false, false, false, false, false, false, true, true, false};

    CPartialMerkleTree tree(vTxid, vMatch);
    std::vector<unsigned int> vIndex;
    BOOST_CHECK(tree.ExtractMatches(vTxid, vIndex).IsNull());
}

BOOST_AUTO_TEST_SUITE_END()
