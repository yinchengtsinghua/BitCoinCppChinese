
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

#include <bench/bench.h>
#include <policy/policy.h>
#include <txmempool.h>

#include <list>
#include <vector>

static void AddTx(const CTransactionRef& tx, const CAmount& nFee, CTxMemPool& pool) EXCLUSIVE_LOCKS_REQUIRED(cs_main, pool.cs)
{
    int64_t nTime = 0;
    unsigned int nHeight = 1;
    bool spendsCoinbase = false;
    unsigned int sigOpCost = 4;
    LockPoints lp;
    pool.addUnchecked(CTxMemPoolEntry(
                                         tx, nFee, nTime, nHeight,
                                         spendsCoinbase, sigOpCost, lp));
}

//现在，这只是在一个非常小的
//内存池。需要编写代码以生成更广泛的
//独特的事务处理，实现更有意义的性能度量。
static void MempoolEviction(benchmark::State& state)
{
    CMutableTransaction tx1 = CMutableTransaction();
    tx1.vin.resize(1);
    tx1.vin[0].scriptSig = CScript() << OP_1;
    tx1.vin[0].scriptWitness.stack.push_back({1});
    tx1.vout.resize(1);
    tx1.vout[0].scriptPubKey = CScript() << OP_1 << OP_EQUAL;
    tx1.vout[0].nValue = 10 * COIN;

    CMutableTransaction tx2 = CMutableTransaction();
    tx2.vin.resize(1);
    tx2.vin[0].scriptSig = CScript() << OP_2;
    tx2.vin[0].scriptWitness.stack.push_back({2});
    tx2.vout.resize(1);
    tx2.vout[0].scriptPubKey = CScript() << OP_2 << OP_EQUAL;
    tx2.vout[0].nValue = 10 * COIN;

    CMutableTransaction tx3 = CMutableTransaction();
    tx3.vin.resize(1);
    tx3.vin[0].prevout = COutPoint(tx2.GetHash(), 0);
    tx3.vin[0].scriptSig = CScript() << OP_2;
    tx3.vin[0].scriptWitness.stack.push_back({3});
    tx3.vout.resize(1);
    tx3.vout[0].scriptPubKey = CScript() << OP_3 << OP_EQUAL;
    tx3.vout[0].nValue = 10 * COIN;

    CMutableTransaction tx4 = CMutableTransaction();
    tx4.vin.resize(2);
    tx4.vin[0].prevout.SetNull();
    tx4.vin[0].scriptSig = CScript() << OP_4;
    tx4.vin[0].scriptWitness.stack.push_back({4});
    tx4.vin[1].prevout.SetNull();
    tx4.vin[1].scriptSig = CScript() << OP_4;
    tx4.vin[1].scriptWitness.stack.push_back({4});
    tx4.vout.resize(2);
    tx4.vout[0].scriptPubKey = CScript() << OP_4 << OP_EQUAL;
    tx4.vout[0].nValue = 10 * COIN;
    tx4.vout[1].scriptPubKey = CScript() << OP_4 << OP_EQUAL;
    tx4.vout[1].nValue = 10 * COIN;

    CMutableTransaction tx5 = CMutableTransaction();
    tx5.vin.resize(2);
    tx5.vin[0].prevout = COutPoint(tx4.GetHash(), 0);
    tx5.vin[0].scriptSig = CScript() << OP_4;
    tx5.vin[0].scriptWitness.stack.push_back({4});
    tx5.vin[1].prevout.SetNull();
    tx5.vin[1].scriptSig = CScript() << OP_5;
    tx5.vin[1].scriptWitness.stack.push_back({5});
    tx5.vout.resize(2);
    tx5.vout[0].scriptPubKey = CScript() << OP_5 << OP_EQUAL;
    tx5.vout[0].nValue = 10 * COIN;
    tx5.vout[1].scriptPubKey = CScript() << OP_5 << OP_EQUAL;
    tx5.vout[1].nValue = 10 * COIN;

    CMutableTransaction tx6 = CMutableTransaction();
    tx6.vin.resize(2);
    tx6.vin[0].prevout = COutPoint(tx4.GetHash(), 1);
    tx6.vin[0].scriptSig = CScript() << OP_4;
    tx6.vin[0].scriptWitness.stack.push_back({4});
    tx6.vin[1].prevout.SetNull();
    tx6.vin[1].scriptSig = CScript() << OP_6;
    tx6.vin[1].scriptWitness.stack.push_back({6});
    tx6.vout.resize(2);
    tx6.vout[0].scriptPubKey = CScript() << OP_6 << OP_EQUAL;
    tx6.vout[0].nValue = 10 * COIN;
    tx6.vout[1].scriptPubKey = CScript() << OP_6 << OP_EQUAL;
    tx6.vout[1].nValue = 10 * COIN;

    CMutableTransaction tx7 = CMutableTransaction();
    tx7.vin.resize(2);
    tx7.vin[0].prevout = COutPoint(tx5.GetHash(), 0);
    tx7.vin[0].scriptSig = CScript() << OP_5;
    tx7.vin[0].scriptWitness.stack.push_back({5});
    tx7.vin[1].prevout = COutPoint(tx6.GetHash(), 0);
    tx7.vin[1].scriptSig = CScript() << OP_6;
    tx7.vin[1].scriptWitness.stack.push_back({6});
    tx7.vout.resize(2);
    tx7.vout[0].scriptPubKey = CScript() << OP_7 << OP_EQUAL;
    tx7.vout[0].nValue = 10 * COIN;
    tx7.vout[1].scriptPubKey = CScript() << OP_7 << OP_EQUAL;
    tx7.vout[1].nValue = 10 * COIN;

    CTxMemPool pool;
    LOCK2(cs_main, pool.cs);
//在“热循环”之外创建事务引用
    const CTransactionRef tx1_r{MakeTransactionRef(tx1)};
    const CTransactionRef tx2_r{MakeTransactionRef(tx2)};
    const CTransactionRef tx3_r{MakeTransactionRef(tx3)};
    const CTransactionRef tx4_r{MakeTransactionRef(tx4)};
    const CTransactionRef tx5_r{MakeTransactionRef(tx5)};
    const CTransactionRef tx6_r{MakeTransactionRef(tx6)};
    const CTransactionRef tx7_r{MakeTransactionRef(tx7)};

    while (state.KeepRunning()) {
        AddTx(tx1_r, 10000LL, pool);
        AddTx(tx2_r, 5000LL, pool);
        AddTx(tx3_r, 20000LL, pool);
        AddTx(tx4_r, 7000LL, pool);
        AddTx(tx5_r, 1000LL, pool);
        AddTx(tx6_r, 1100LL, pool);
        AddTx(tx7_r, 9000LL, pool);
        pool.TrimToSize(pool.DynamicMemoryUsage() * 3 / 4);
        pool.TrimToSize(GetVirtualTransactionSize(*tx1_r));
    }
}

BENCHMARK(MempoolEviction, 41000);
