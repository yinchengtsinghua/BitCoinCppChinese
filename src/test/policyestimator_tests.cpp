
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <policy/policy.h>
#include <policy/fees.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/system.h>

#include <test/test_bitcoin.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(policyestimator_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(BlockPolicyEstimates)
{
    CBlockPolicyEstimator feeEst;
    CTxMemPool mpool(&feeEst);
    LOCK2(cs_main, mpool.cs);
    TestMemPoolEntryHelper entry;
    CAmount basefee(2000);
    CAmount deltaFee(100);
    std::vector<CAmount> feeV;

//填充费用增加的向量
    for (int j = 0; j < 10; j++) {
        feeV.push_back(basefee * (j+1));
    }

//存储已经
//按员工费用添加到mempool
//txshashes[j]由以下任一事务填充：
//费用=基本费用*（j+1）
    std::vector<uint256> txHashes[10];

//创建交易模板
    CScript garbage;
    for (unsigned int i = 0; i < 128; i++)
        garbage.push_back('X');
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].scriptSig = garbage;
    tx.vout.resize(1);
    tx.vout[0].nValue=0LL;
    CFeeRate baseRate(basefee, GetVirtualTransactionSize(CTransaction(tx)));

//创建假块
    std::vector<CTransactionRef> block;
    int blocknum = 0;

//循环通过200个块
//衰退时，每块9952和4笔费用交易
//这使得每个桶的Tx计数约为2.5，远高于0.1阈值。
    while (blocknum < 200) {
for (int j = 0; j < 10; j++) { //每次收费
for (int k = 0; k < 4; k++) { //添加4费用TXS
tx.vin[0].prevout.n = 10000*blocknum+100*j+k; //使事务唯一
                uint256 hash = tx.GetHash();
                mpool.addUnchecked(entry.Fee(feeV[j]).Time(GetTime()).Height(blocknum).FromTx(tx));
                txHashes[j].push_back(hash);
            }
        }
//创建包含更高费用tx的块
        for (int h = 0; h <= blocknum%10; h++) {
//10/10块添加最高费用交易
//9/10块添加第二个最高点，以此类推，直到…
//1/10块添加最低费用交易
            while (txHashes[9-h].size()) {
                CTransactionRef ptx = mpool.get(txHashes[9-h].back());
                if (ptx)
                    block.push_back(ptx);
                txHashes[9-h].pop_back();
            }
        }
        mpool.removeForBlock(block, ++blocknum);
        block.clear();
//在几个TXS之后检查组合桶是否按预期工作
        if (blocknum == 3) {
//此时，我们需要结合3个存储桶来获得足够的数据点。
//所以估计量（1）应该失败，估计量（2）应该返回到附近的某个地方
//9×碱基。估计值（2）%为100100,90=平均97%
            BOOST_CHECK(feeEst.estimateFee(1) == CFeeRate(0));
            BOOST_CHECK(feeEst.estimateFee(2).GetFeePerK() < 9*baseRate.GetFeePerK() + deltaFee);
            BOOST_CHECK(feeEst.estimateFee(2).GetFeePerK() > 9*baseRate.GetFeePerK() - deltaFee);
        }
    }

    std::vector<CAmount> origFeeEst;
//最高频率为10*基本速率，进入所有数据块，
//第二高feerate是9*基本速率，进入9/10块=90%，
//第三高频率为8*基本速率，8/10个数据块=80%，
//因此，EstimateFee（1）将返回10*基本速率，但硬编码以返回失败。
//第二高的feerate有100%的机会被2个区块包括，
//所以估计值（2）应该返回9*基本速率等…
    for (int i = 1; i < 10;i++) {
        origFeeEst.push_back(feeEst.estimateFee(i).GetFeePerK());
if (i > 2) { //费用估计应该单调地减少。
            BOOST_CHECK(origFeeEst[i-1] <= origFeeEst[i-2]);
        }
        int mult = 11-i;
if (i % 2 == 0) { //在等级2，测试逻辑仅适用于偶数目标
            BOOST_CHECK(origFeeEst[i-1] < mult*baseRate.GetFeePerK() + deltaFee);
            BOOST_CHECK(origFeeEst[i-1] > mult*baseRate.GetFeePerK() - deltaFee);
        }
    }
//填写原始估计的其余部分
    for (int i = 10; i <= 48; i++) {
        origFeeEst.push_back(feeEst.estimateFee(i).GetFeePerK());
    }

//再挖50个街区，没有交易发生，估计不会改变
//移动平均线衰减不够，所以每个桶中仍有足够的数据点。
    while (blocknum < 250)
        mpool.removeForBlock(block, ++blocknum);

    BOOST_CHECK(feeEst.estimateFee(1) == CFeeRate(0));
    for (int i = 2; i < 10;i++) {
        BOOST_CHECK(feeEst.estimateFee(i).GetFeePerK() < origFeeEst[i-1] + deltaFee);
        BOOST_CHECK(feeEst.estimateFee(i).GetFeePerK() > origFeeEst[i-1] - deltaFee);
    }


//再开采15个区块，交易频繁且未开采
//估计应该上升
    while (blocknum < 265) {
for (int j = 0; j < 10; j++) { //每次收费倍数
for (int k = 0; k < 4; k++) { //添加4费用TXS
                tx.vin[0].prevout.n = 10000*blocknum+100*j+k;
                uint256 hash = tx.GetHash();
                mpool.addUnchecked(entry.Fee(feeV[j]).Time(GetTime()).Height(blocknum).FromTx(tx));
                txHashes[j].push_back(hash);
            }
        }
        mpool.removeForBlock(block, ++blocknum);
    }

    for (int i = 1; i < 10;i++) {
        BOOST_CHECK(feeEst.estimateFee(i) == CFeeRate(0) || feeEst.estimateFee(i).GetFeePerK() > origFeeEst[i-1] - deltaFee);
    }

//我的所有交易
//估计值仍不应低于原始值
    for (int j = 0; j < 10; j++) {
        while(txHashes[j].size()) {
            CTransactionRef ptx = mpool.get(txHashes[j].back());
            if (ptx)
                block.push_back(ptx);
            txHashes[j].pop_back();
        }
    }
    mpool.removeForBlock(block, 266);
    block.clear();
    BOOST_CHECK(feeEst.estimateFee(1) == CFeeRate(0));
    for (int i = 2; i < 10;i++) {
        BOOST_CHECK(feeEst.estimateFee(i) == CFeeRate(0) || feeEst.estimateFee(i).GetFeePerK() > origFeeEst[i-1] - deltaFee);
    }

//再挖400个街区，每个街区都有开采。
//估计数应低于原始估计数
    while (blocknum < 665) {
for (int j = 0; j < 10; j++) { //每次收费倍数
for (int k = 0; k < 4; k++) { //添加4费用TXS
                tx.vin[0].prevout.n = 10000*blocknum+100*j+k;
                uint256 hash = tx.GetHash();
                mpool.addUnchecked(entry.Fee(feeV[j]).Time(GetTime()).Height(blocknum).FromTx(tx));
                CTransactionRef ptx = mpool.get(hash);
                if (ptx)
                    block.push_back(ptx);

            }
        }
        mpool.removeForBlock(block, ++blocknum);
        block.clear();
    }
    BOOST_CHECK(feeEst.estimateFee(1) == CFeeRate(0));
for (int i = 2; i < 9; i++) { //在9点，最初的估计已经在底部（B/C比例=2）
        BOOST_CHECK(feeEst.estimateFee(i).GetFeePerK() < origFeeEst[i-1] - deltaFee);
    }
}

BOOST_AUTO_TEST_SUITE_END()
