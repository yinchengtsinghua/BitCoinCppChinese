
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
#include <txmempool.h>
#include <util/system.h>

#include <test/test_bitcoin.h>

#include <boost/test/unit_test.hpp>
#include <list>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(mempool_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(MempoolRemoveTest)
{
//测试CTxEmpool：：删除功能

    TestMemPoolEntryHelper entry;
//具有三个子级的父级事务，
//还有三个孙子：
    CMutableTransaction txParent;
    txParent.vin.resize(1);
    txParent.vin[0].scriptSig = CScript() << OP_11;
    txParent.vout.resize(3);
    for (int i = 0; i < 3; i++)
    {
        txParent.vout[i].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txParent.vout[i].nValue = 33000LL;
    }
    CMutableTransaction txChild[3];
    for (int i = 0; i < 3; i++)
    {
        txChild[i].vin.resize(1);
        txChild[i].vin[0].scriptSig = CScript() << OP_11;
        txChild[i].vin[0].prevout.hash = txParent.GetHash();
        txChild[i].vin[0].prevout.n = i;
        txChild[i].vout.resize(1);
        txChild[i].vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txChild[i].vout[0].nValue = 11000LL;
    }
    CMutableTransaction txGrandChild[3];
    for (int i = 0; i < 3; i++)
    {
        txGrandChild[i].vin.resize(1);
        txGrandChild[i].vin[0].scriptSig = CScript() << OP_11;
        txGrandChild[i].vin[0].prevout.hash = txChild[i].GetHash();
        txGrandChild[i].vin[0].prevout.n = 0;
        txGrandChild[i].vout.resize(1);
        txGrandChild[i].vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txGrandChild[i].vout[0].nValue = 11000LL;
    }


    CTxMemPool testPool;
    LOCK2(cs_main, testPool.cs);

//池中没有任何内容，移除不应执行任何操作：
    unsigned int poolSize = testPool.size();
    testPool.removeRecursive(CTransaction(txParent));
    BOOST_CHECK_EQUAL(testPool.size(), poolSize);

//只有家长：
    testPool.addUnchecked(entry.FromTx(txParent));
    poolSize = testPool.size();
    testPool.removeRecursive(CTransaction(txParent));
    BOOST_CHECK_EQUAL(testPool.size(), poolSize - 1);

//父母、子女、孙子女：
    testPool.addUnchecked(entry.FromTx(txParent));
    for (int i = 0; i < 3; i++)
    {
        testPool.addUnchecked(entry.FromTx(txChild[i]));
        testPool.addUnchecked(entry.FromTx(txGrandChild[i]));
    }
//删除子项[0]，应删除孙子项[0]：
    poolSize = testPool.size();
    testPool.removeRecursive(CTransaction(txChild[0]));
    BOOST_CHECK_EQUAL(testPool.size(), poolSize - 2);
//…确保孙子和孩子不在：
    poolSize = testPool.size();
    testPool.removeRecursive(CTransaction(txGrandChild[0]));
    BOOST_CHECK_EQUAL(testPool.size(), poolSize);
    poolSize = testPool.size();
    testPool.removeRecursive(CTransaction(txChild[0]));
    BOOST_CHECK_EQUAL(testPool.size(), poolSize);
//移除父级，所有子级/孙级都应：
    poolSize = testPool.size();
    testPool.removeRecursive(CTransaction(txParent));
    BOOST_CHECK_EQUAL(testPool.size(), poolSize - 5);
    BOOST_CHECK_EQUAL(testPool.size(), 0U);

//添加子代和孙子，但不添加父代（模拟父代位于块中）
    for (int i = 0; i < 3; i++)
    {
        testPool.addUnchecked(entry.FromTx(txChild[i]));
        testPool.addUnchecked(entry.FromTx(txGrandChild[i]));
    }
//现在删除父级，就像发生块重新组织时可能发生的情况一样，但父级不能
//放入内存池（可能是因为它不标准）：
    poolSize = testPool.size();
    testPool.removeRecursive(CTransaction(txParent));
    BOOST_CHECK_EQUAL(testPool.size(), poolSize - 6);
    BOOST_CHECK_EQUAL(testPool.size(), 0U);
}

template<typename name>
static void CheckSort(CTxMemPool &pool, std::vector<std::string> &sortedOrder) EXCLUSIVE_LOCKS_REQUIRED(pool.cs)
{
    BOOST_CHECK_EQUAL(pool.size(), sortedOrder.size());
    typename CTxMemPool::indexed_transaction_set::index<name>::type::iterator it = pool.mapTx.get<name>().begin();
    int count=0;
    for (; it != pool.mapTx.get<name>().end(); ++it, ++count) {
        BOOST_CHECK_EQUAL(it->GetTx().GetHash().ToString(), sortedOrder[count]);
    }
}

BOOST_AUTO_TEST_CASE(MempoolIndexingTest)
{
    CTxMemPool pool;
    LOCK2(cs_main, pool.cs);
    TestMemPoolEntryHelper entry;

    /*第三最高费用*/
    CMutableTransaction tx1 = CMutableTransaction();
    tx1.vout.resize(1);
    tx1.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx1.vout[0].nValue = 10 * COIN;
    pool.addUnchecked(entry.Fee(10000LL).FromTx(tx1));

    /*最高费用*/
    CMutableTransaction tx2 = CMutableTransaction();
    tx2.vout.resize(1);
    tx2.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx2.vout[0].nValue = 2 * COIN;
    pool.addUnchecked(entry.Fee(20000LL).FromTx(tx2));

    /*最低费用*/
    CMutableTransaction tx3 = CMutableTransaction();
    tx3.vout.resize(1);
    tx3.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx3.vout[0].nValue = 5 * COIN;
    pool.addUnchecked(entry.Fee(0LL).FromTx(tx3));

    /*第二最高费用*/
    CMutableTransaction tx4 = CMutableTransaction();
    tx4.vout.resize(1);
    tx4.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx4.vout[0].nValue = 6 * COIN;
    pool.addUnchecked(entry.Fee(15000LL).FromTx(tx4));

    /*与TX1相同的费率，但更新*/
    CMutableTransaction tx5 = CMutableTransaction();
    tx5.vout.resize(1);
    tx5.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx5.vout[0].nValue = 11 * COIN;
    entry.nTime = 1;
    pool.addUnchecked(entry.Fee(10000LL).FromTx(tx5));
    BOOST_CHECK_EQUAL(pool.size(), 5U);

    std::vector<std::string> sortedOrder;
    sortedOrder.resize(5);
sortedOrder[0] = tx3.GetHash().ToString(); //零
sortedOrder[1] = tx5.GetHash().ToString(); //一万
sortedOrder[2] = tx1.GetHash().ToString(); //一万
sortedOrder[3] = tx4.GetHash().ToString(); //一万五千
sortedOrder[4] = tx2.GetHash().ToString(); //二万
    CheckSort<descendant_score>(pool, sortedOrder);

    /*低收费但有高收费的孩子*/
    /*TX6->TX7->TX8，TX9->TX10*/
    CMutableTransaction tx6 = CMutableTransaction();
    tx6.vout.resize(1);
    tx6.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx6.vout[0].nValue = 20 * COIN;
    pool.addUnchecked(entry.Fee(0LL).FromTx(tx6));
    BOOST_CHECK_EQUAL(pool.size(), 6U);
//检查此时，tx6是否排序低
    sortedOrder.insert(sortedOrder.begin(), tx6.GetHash().ToString());
    CheckSort<descendant_score>(pool, sortedOrder);

    CTxMemPool::setEntries setAncestors;
    setAncestors.insert(pool.mapTx.find(tx6.GetHash()));
    CMutableTransaction tx7 = CMutableTransaction();
    tx7.vin.resize(1);
    tx7.vin[0].prevout = COutPoint(tx6.GetHash(), 0);
    tx7.vin[0].scriptSig = CScript() << OP_11;
    tx7.vout.resize(2);
    tx7.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx7.vout[0].nValue = 10 * COIN;
    tx7.vout[1].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx7.vout[1].nValue = 1 * COIN;

    CTxMemPool::setEntries setAncestorsCalculated;
    std::string dummy;
    BOOST_CHECK_EQUAL(pool.CalculateMemPoolAncestors(entry.Fee(2000000LL).FromTx(tx7), setAncestorsCalculated, 100, 1000000, 1000, 1000000, dummy), true);
    BOOST_CHECK(setAncestorsCalculated == setAncestors);

    pool.addUnchecked(entry.FromTx(tx7), setAncestors);
    BOOST_CHECK_EQUAL(pool.size(), 7U);

//现在，tx6应该排序得更高（高费用子项）：tx7，tx6，tx2，…
    sortedOrder.erase(sortedOrder.begin());
    sortedOrder.push_back(tx6.GetHash().ToString());
    sortedOrder.push_back(tx7.GetHash().ToString());
    CheckSort<descendant_score>(pool, sortedOrder);

    /*TX7的低费用子项*/
    CMutableTransaction tx8 = CMutableTransaction();
    tx8.vin.resize(1);
    tx8.vin[0].prevout = COutPoint(tx7.GetHash(), 0);
    tx8.vin[0].scriptSig = CScript() << OP_11;
    tx8.vout.resize(1);
    tx8.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx8.vout[0].nValue = 10 * COIN;
    setAncestors.insert(pool.mapTx.find(tx7.GetHash()));
    pool.addUnchecked(entry.Fee(0LL).Time(2).FromTx(tx8), setAncestors);

//现在，tx8应该是低排序，但tx6/tx都是高排序
    sortedOrder.insert(sortedOrder.begin(), tx8.GetHash().ToString());
    CheckSort<descendant_score>(pool, sortedOrder);

    /*TX7的低费用子项*/
    CMutableTransaction tx9 = CMutableTransaction();
    tx9.vin.resize(1);
    tx9.vin[0].prevout = COutPoint(tx7.GetHash(), 1);
    tx9.vin[0].scriptSig = CScript() << OP_11;
    tx9.vout.resize(1);
    tx9.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx9.vout[0].nValue = 1 * COIN;
    pool.addUnchecked(entry.Fee(0LL).Time(3).FromTx(tx9), setAncestors);

//TX9应低排序
    BOOST_CHECK_EQUAL(pool.size(), 9U);
    sortedOrder.insert(sortedOrder.begin(), tx9.GetHash().ToString());
    CheckSort<descendant_score>(pool, sortedOrder);

    std::vector<std::string> snapshotOrder = sortedOrder;

    setAncestors.insert(pool.mapTx.find(tx8.GetHash()));
    setAncestors.insert(pool.mapTx.find(tx9.GetHash()));
    /*tx10依赖于tx8和tx9，并具有高铁*/
    CMutableTransaction tx10 = CMutableTransaction();
    tx10.vin.resize(2);
    tx10.vin[0].prevout = COutPoint(tx8.GetHash(), 0);
    tx10.vin[0].scriptSig = CScript() << OP_11;
    tx10.vin[1].prevout = COutPoint(tx9.GetHash(), 0);
    tx10.vin[1].scriptSig = CScript() << OP_11;
    tx10.vout.resize(1);
    tx10.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx10.vout[0].nValue = 10 * COIN;

    setAncestorsCalculated.clear();
    BOOST_CHECK_EQUAL(pool.CalculateMemPoolAncestors(entry.Fee(200000LL).Time(4).FromTx(tx10), setAncestorsCalculated, 100, 1000000, 1000, 1000000, dummy), true);
    BOOST_CHECK(setAncestorsCalculated == setAncestors);

    pool.addUnchecked(entry.FromTx(tx10), setAncestors);

    /*
     *tx8和tx9现在都应该排序得更高。
     *添加tx10后的最终订单：
     *
     ＊Tx3＝0（1）
     *tx5=10000（1）
     *tx1=10000（1）
     *tx4=15000（1）
     *tx2=20000（1）
     *tx9=200K（2个txs）
     *tx8=200K（2个txs）
     *tx10=200K（1个tx）
     *tx6=2.2米（5 txs）
     *tx7=2.2米（4个txs）
     **/

sortedOrder.erase(sortedOrder.begin(), sortedOrder.begin()+2); //从开始处取出tx9、tx8
    sortedOrder.insert(sortedOrder.begin()+5, tx9.GetHash().ToString());
    sortedOrder.insert(sortedOrder.begin()+6, tx8.GetHash().ToString());
sortedOrder.insert(sortedOrder.begin()+7, tx10.GetHash().ToString()); //tx10就在tx6之前
    CheckSort<descendant_score>(pool, sortedOrder);

//内存池中应该有10个事务
    BOOST_CHECK_EQUAL(pool.size(), 10U);

//现在尝试删除tx10并验证排序顺序是否恢复正常
    pool.removeRecursive(pool.mapTx.find(tx10.GetHash())->GetTx());
    CheckSort<descendant_score>(pool, snapshotOrder);

    pool.removeRecursive(pool.mapTx.find(tx9.GetHash())->GetTx());
    pool.removeRecursive(pool.mapTx.find(tx8.GetHash())->GetTx());
}

BOOST_AUTO_TEST_CASE(MempoolAncestorIndexingTest)
{
    CTxMemPool pool;
    LOCK2(cs_main, pool.cs);
    TestMemPoolEntryHelper entry;

    /*第三最高费用*/
    CMutableTransaction tx1 = CMutableTransaction();
    tx1.vout.resize(1);
    tx1.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx1.vout[0].nValue = 10 * COIN;
    pool.addUnchecked(entry.Fee(10000LL).FromTx(tx1));

    /*最高费用*/
    CMutableTransaction tx2 = CMutableTransaction();
    tx2.vout.resize(1);
    tx2.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx2.vout[0].nValue = 2 * COIN;
    pool.addUnchecked(entry.Fee(20000LL).FromTx(tx2));
    uint64_t tx2Size = GetVirtualTransactionSize(CTransaction(tx2));

    /*最低费用*/
    CMutableTransaction tx3 = CMutableTransaction();
    tx3.vout.resize(1);
    tx3.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx3.vout[0].nValue = 5 * COIN;
    pool.addUnchecked(entry.Fee(0LL).FromTx(tx3));

    /*第二最高费用*/
    CMutableTransaction tx4 = CMutableTransaction();
    tx4.vout.resize(1);
    tx4.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx4.vout[0].nValue = 6 * COIN;
    pool.addUnchecked(entry.Fee(15000LL).FromTx(tx4));

    /*与TX1相同的费率，但更新*/
    CMutableTransaction tx5 = CMutableTransaction();
    tx5.vout.resize(1);
    tx5.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx5.vout[0].nValue = 11 * COIN;
    pool.addUnchecked(entry.Fee(10000LL).FromTx(tx5));
    BOOST_CHECK_EQUAL(pool.size(), 5U);

    std::vector<std::string> sortedOrder;
    sortedOrder.resize(5);
sortedOrder[0] = tx2.GetHash().ToString(); //二万
sortedOrder[1] = tx4.GetHash().ToString(); //一万五千
//TX1和TX5都是10000
//绑定被散列而不是时间戳破坏，因此确定
//哈希首先出现。
    if (tx1.GetHash() < tx5.GetHash()) {
        sortedOrder[2] = tx1.GetHash().ToString();
        sortedOrder[3] = tx5.GetHash().ToString();
    } else {
        sortedOrder[2] = tx5.GetHash().ToString();
        sortedOrder[3] = tx1.GetHash().ToString();
    }
sortedOrder[4] = tx3.GetHash().ToString(); //零

    CheckSort<ancestor_score>(pool, sortedOrder);

    /*低收费家长和高收费孩子*/
    /*TX6（0）->TX7（高）*/
    CMutableTransaction tx6 = CMutableTransaction();
    tx6.vout.resize(1);
    tx6.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx6.vout[0].nValue = 20 * COIN;
    uint64_t tx6Size = GetVirtualTransactionSize(CTransaction(tx6));

    pool.addUnchecked(entry.Fee(0LL).FromTx(tx6));
    BOOST_CHECK_EQUAL(pool.size(), 6U);
//领带被杂碎了
    if (tx3.GetHash() < tx6.GetHash())
        sortedOrder.push_back(tx6.GetHash().ToString());
    else
        sortedOrder.insert(sortedOrder.end()-1,tx6.GetHash().ToString());

    CheckSort<ancestor_score>(pool, sortedOrder);

    CMutableTransaction tx7 = CMutableTransaction();
    tx7.vin.resize(1);
    tx7.vin[0].prevout = COutPoint(tx6.GetHash(), 0);
    tx7.vin[0].scriptSig = CScript() << OP_11;
    tx7.vout.resize(1);
    tx7.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx7.vout[0].nValue = 10 * COIN;
    uint64_t tx7Size = GetVirtualTransactionSize(CTransaction(tx7));

    /*当包含祖先时，将费用设置为略低于tx2的费用*/
    CAmount fee = (20000/tx2Size)*(tx7Size + tx6Size) - 1;

    pool.addUnchecked(entry.Fee(fee).FromTx(tx7));
    BOOST_CHECK_EQUAL(pool.size(), 7U);
    sortedOrder.insert(sortedOrder.begin()+1, tx7.GetHash().ToString());
    CheckSort<ancestor_score>(pool, sortedOrder);

    /*开采完TX6后，TX7应向上移动。*/
    std::vector<CTransactionRef> vtx;
    vtx.push_back(MakeTransactionRef(tx6));
    pool.removeForBlock(vtx, 1);

    sortedOrder.erase(sortedOrder.begin()+1);
//领带被杂碎了
    if (tx3.GetHash() < tx6.GetHash())
        sortedOrder.pop_back();
    else
        sortedOrder.erase(sortedOrder.end()-2);
    sortedOrder.insert(sortedOrder.begin(), tx7.GetHash().ToString());
    CheckSort<ancestor_score>(pool, sortedOrder);

//高收费家长，低收费孩子
//TX7-＞TX8
    CMutableTransaction tx8 = CMutableTransaction();
    tx8.vin.resize(1);
    tx8.vin[0].prevout  = COutPoint(tx7.GetHash(), 0);
    tx8.vin[0].scriptSig = CScript() << OP_11;
    tx8.vout.resize(1);
    tx8.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx8.vout[0].nValue = 10*COIN;

//检查我们是否按min排序（feerate，祖先feerate）：
//设置费用，使祖先feerate高于tx1/5，
//但交易本身的利率较低
    pool.addUnchecked(entry.Fee(5000LL).FromTx(tx8));
    sortedOrder.insert(sortedOrder.end()-1, tx8.GetHash().ToString());
    CheckSort<ancestor_score>(pool, sortedOrder);
}


BOOST_AUTO_TEST_CASE(MempoolSizeLimitTest)
{
    CTxMemPool pool;
    LOCK2(cs_main, pool.cs);
    TestMemPoolEntryHelper entry;

    CMutableTransaction tx1 = CMutableTransaction();
    tx1.vin.resize(1);
    tx1.vin[0].scriptSig = CScript() << OP_1;
    tx1.vout.resize(1);
    tx1.vout[0].scriptPubKey = CScript() << OP_1 << OP_EQUAL;
    tx1.vout[0].nValue = 10 * COIN;
    pool.addUnchecked(entry.Fee(10000LL).FromTx(tx1));

    CMutableTransaction tx2 = CMutableTransaction();
    tx2.vin.resize(1);
    tx2.vin[0].scriptSig = CScript() << OP_2;
    tx2.vout.resize(1);
    tx2.vout[0].scriptPubKey = CScript() << OP_2 << OP_EQUAL;
    tx2.vout[0].nValue = 10 * COIN;
    pool.addUnchecked(entry.Fee(5000LL).FromTx(tx2));

pool.TrimToSize(pool.DynamicMemoryUsage()); //什么都不应该做
    BOOST_CHECK(pool.exists(tx1.GetHash()));
    BOOST_CHECK(pool.exists(tx2.GetHash()));

pool.TrimToSize(pool.DynamicMemoryUsage() * 3 / 4); //应删除较低级别的事务
    BOOST_CHECK(pool.exists(tx1.GetHash()));
    BOOST_CHECK(!pool.exists(tx2.GetHash()));

    pool.addUnchecked(entry.FromTx(tx2));
    CMutableTransaction tx3 = CMutableTransaction();
    tx3.vin.resize(1);
    tx3.vin[0].prevout = COutPoint(tx2.GetHash(), 0);
    tx3.vin[0].scriptSig = CScript() << OP_2;
    tx3.vout.resize(1);
    tx3.vout[0].scriptPubKey = CScript() << OP_3 << OP_EQUAL;
    tx3.vout[0].nValue = 10 * COIN;
    pool.addUnchecked(entry.Fee(20000LL).FromTx(tx3));

pool.TrimToSize(pool.DynamicMemoryUsage() * 3 / 4); //TX3应支付TX2（CPFP）
    BOOST_CHECK(!pool.exists(tx1.GetHash()));
    BOOST_CHECK(pool.exists(tx2.GetHash()));
    BOOST_CHECK(pool.exists(tx3.GetHash()));

pool.TrimToSize(GetVirtualTransactionSize(CTransaction(tx1))); //mempool的内存使用限制在tx1的大小，所以没有适合的
    BOOST_CHECK(!pool.exists(tx1.GetHash()));
    BOOST_CHECK(!pool.exists(tx2.GetHash()));
    BOOST_CHECK(!pool.exists(tx3.GetHash()));

    CFeeRate maxFeeRateRemoved(25000, GetVirtualTransactionSize(CTransaction(tx3)) + GetVirtualTransactionSize(CTransaction(tx2)));
    BOOST_CHECK_EQUAL(pool.GetMinFee(1).GetFeePerK(), maxFeeRateRemoved.GetFeePerK() + 1000);

    CMutableTransaction tx4 = CMutableTransaction();
    tx4.vin.resize(2);
    tx4.vin[0].prevout.SetNull();
    tx4.vin[0].scriptSig = CScript() << OP_4;
    tx4.vin[1].prevout.SetNull();
    tx4.vin[1].scriptSig = CScript() << OP_4;
    tx4.vout.resize(2);
    tx4.vout[0].scriptPubKey = CScript() << OP_4 << OP_EQUAL;
    tx4.vout[0].nValue = 10 * COIN;
    tx4.vout[1].scriptPubKey = CScript() << OP_4 << OP_EQUAL;
    tx4.vout[1].nValue = 10 * COIN;

    CMutableTransaction tx5 = CMutableTransaction();
    tx5.vin.resize(2);
    tx5.vin[0].prevout = COutPoint(tx4.GetHash(), 0);
    tx5.vin[0].scriptSig = CScript() << OP_4;
    tx5.vin[1].prevout.SetNull();
    tx5.vin[1].scriptSig = CScript() << OP_5;
    tx5.vout.resize(2);
    tx5.vout[0].scriptPubKey = CScript() << OP_5 << OP_EQUAL;
    tx5.vout[0].nValue = 10 * COIN;
    tx5.vout[1].scriptPubKey = CScript() << OP_5 << OP_EQUAL;
    tx5.vout[1].nValue = 10 * COIN;

    CMutableTransaction tx6 = CMutableTransaction();
    tx6.vin.resize(2);
    tx6.vin[0].prevout = COutPoint(tx4.GetHash(), 1);
    tx6.vin[0].scriptSig = CScript() << OP_4;
    tx6.vin[1].prevout.SetNull();
    tx6.vin[1].scriptSig = CScript() << OP_6;
    tx6.vout.resize(2);
    tx6.vout[0].scriptPubKey = CScript() << OP_6 << OP_EQUAL;
    tx6.vout[0].nValue = 10 * COIN;
    tx6.vout[1].scriptPubKey = CScript() << OP_6 << OP_EQUAL;
    tx6.vout[1].nValue = 10 * COIN;

    CMutableTransaction tx7 = CMutableTransaction();
    tx7.vin.resize(2);
    tx7.vin[0].prevout = COutPoint(tx5.GetHash(), 0);
    tx7.vin[0].scriptSig = CScript() << OP_5;
    tx7.vin[1].prevout = COutPoint(tx6.GetHash(), 0);
    tx7.vin[1].scriptSig = CScript() << OP_6;
    tx7.vout.resize(2);
    tx7.vout[0].scriptPubKey = CScript() << OP_7 << OP_EQUAL;
    tx7.vout[0].nValue = 10 * COIN;
    tx7.vout[1].scriptPubKey = CScript() << OP_7 << OP_EQUAL;
    tx7.vout[1].nValue = 10 * COIN;

    pool.addUnchecked(entry.Fee(7000LL).FromTx(tx4));
    pool.addUnchecked(entry.Fee(1000LL).FromTx(tx5));
    pool.addUnchecked(entry.Fee(1100LL).FromTx(tx6));
    pool.addUnchecked(entry.Fee(9000LL).FromTx(tx7));

//我们只需要在最大2 txn时去掉这个，因为除了这个，我们还不清楚我们到底在优化什么。
    pool.TrimToSize(pool.DynamicMemoryUsage() - 1);
    BOOST_CHECK(pool.exists(tx4.GetHash()));
    BOOST_CHECK(pool.exists(tx6.GetHash()));
    BOOST_CHECK(!pool.exists(tx7.GetHash()));

    if (!pool.exists(tx5.GetHash()))
        pool.addUnchecked(entry.Fee(1000LL).FromTx(tx5));
    pool.addUnchecked(entry.Fee(9000LL).FromTx(tx7));

pool.TrimToSize(pool.DynamicMemoryUsage() / 2); //应仅通过移除5/7来最大化内存池大小
    BOOST_CHECK(pool.exists(tx4.GetHash()));
    BOOST_CHECK(!pool.exists(tx5.GetHash()));
    BOOST_CHECK(pool.exists(tx6.GetHash()));
    BOOST_CHECK(!pool.exists(tx7.GetHash()));

    pool.addUnchecked(entry.Fee(1000LL).FromTx(tx5));
    pool.addUnchecked(entry.Fee(9000LL).FromTx(tx7));

    std::vector<CTransactionRef> vtx;
    SetMockTime(42);
    SetMockTime(42 + CTxMemPool::ROLLING_FEE_HALFLIFE);
    BOOST_CHECK_EQUAL(pool.GetMinFee(1).GetFeePerK(), maxFeeRateRemoved.GetFeePerK() + 1000);
//…我们应该保持同样的最低收费直到我们得到一个街区
    pool.removeForBlock(vtx, 1);
    SetMockTime(42 + 2*CTxMemPool::ROLLING_FEE_HALFLIFE);
    BOOST_CHECK_EQUAL(pool.GetMinFee(1).GetFeePerK(), llround((maxFeeRateRemoved.GetFeePerK() + 1000)/2.0));
//…那么feerate每半衰期应该下降1/2

    SetMockTime(42 + 2*CTxMemPool::ROLLING_FEE_HALFLIFE + CTxMemPool::ROLLING_FEE_HALFLIFE/2);
    BOOST_CHECK_EQUAL(pool.GetMinFee(pool.DynamicMemoryUsage() * 5 / 2).GetFeePerK(), llround((maxFeeRateRemoved.GetFeePerK() + 1000)/4.0));
//…当mempool小于目标大小的1/2时，半衰期为1/2

    SetMockTime(42 + 2*CTxMemPool::ROLLING_FEE_HALFLIFE + CTxMemPool::ROLLING_FEE_HALFLIFE/2 + CTxMemPool::ROLLING_FEE_HALFLIFE/4);
    BOOST_CHECK_EQUAL(pool.GetMinFee(pool.DynamicMemoryUsage() * 9 / 2).GetFeePerK(), llround((maxFeeRateRemoved.GetFeePerK() + 1000)/8.0));
//…当mempool小于目标大小的1/4时，半衰期为1/4

    SetMockTime(42 + 7*CTxMemPool::ROLLING_FEE_HALFLIFE + CTxMemPool::ROLLING_FEE_HALFLIFE/2 + CTxMemPool::ROLLING_FEE_HALFLIFE/4);
    BOOST_CHECK_EQUAL(pool.GetMinFee(1).GetFeePerK(), 1000);
//…但是feerate绝对不能低于1000

    SetMockTime(42 + 8*CTxMemPool::ROLLING_FEE_HALFLIFE + CTxMemPool::ROLLING_FEE_HALFLIFE/2 + CTxMemPool::ROLLING_FEE_HALFLIFE/4);
    BOOST_CHECK_EQUAL(pool.GetMinFee(1).GetFeePerK(), 0);
//…除非它一直到0（超过1000/2）

    SetMockTime(0);
}

inline CTransactionRef make_tx(std::vector<CAmount>&& output_values, std::vector<CTransactionRef>&& inputs=std::vector<CTransactionRef>(), std::vector<uint32_t>&& input_indices=std::vector<uint32_t>())
{
    CMutableTransaction tx = CMutableTransaction();
    tx.vin.resize(inputs.size());
    tx.vout.resize(output_values.size());
    for (size_t i = 0; i < inputs.size(); ++i) {
        tx.vin[i].prevout.hash = inputs[i]->GetHash();
        tx.vin[i].prevout.n = input_indices.size() > i ? input_indices[i] : 0;
    }
    for (size_t i = 0; i < output_values.size(); ++i) {
        tx.vout[i].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        tx.vout[i].nValue = output_values[i];
    }
    return MakeTransactionRef(tx);
}


BOOST_AUTO_TEST_CASE(MempoolAncestryTests)
{
    size_t ancestors, descendants;

    CTxMemPool pool;
    LOCK2(cs_main, pool.cs);
    TestMemPoolEntryHelper entry;

    /*基本事务*/
//
//[TX1]
//
    /*ansactionRef tx1=生成_tx（/*输出_值*/10*硬币）；
    pool.addunchecked（entry.fee（10000ll）.fromtx（tx1））；

    //祖先/后代应为1/1（自身/自身）
    pool.getTransactionEssential（tx1->getHash（），祖先，后代）；
    boost_check_equal（祖先，1所有）；
    boost_check_equal（后代，1所有）；

    /*子事务*/

//
//[tx1].0<-[tx2]
//
    /*ansactionRef tx2=生成_tx（/*输出_值*/495*美分，5*硬币，/*输入*/tx1）；
    pool.addunchecked（entry.fee（10000ll）.fromtx（tx2））；

    //祖先/后代应为：
    //事务祖先后代
    //==========================
    //tx1 1（tx1）2（tx1,2）
    //tx2 2（tx1,2）2（tx1,2）
    pool.getTransactionEssential（tx1->getHash（），祖先，后代）；
    boost_check_equal（祖先，1所有）；
    boost_check_equal（后代，2ull）；
    pool.getTransactionEssential（tx2->getHash（），祖先，后代）；
    boost_check_equal（祖先，2ull）；
    boost_check_equal（后代，2ull）；

    /*孙子1*/

//
//[tx1].0<-[tx2].0<-[tx3]
//
    /*ansactionRef tx3=生成_tx（/*输出_值*/290*分，200*分，/*输入*/tx2）；
    pool.addunchecked（entry.fee（10000ll）.fromtx（tx3））；

    //祖先/后代应为：
    //事务祖先后代
    //==========================
    //tx1 1（tx1）3（tx1,2,3）
    //tx2 2（tx1,2）3（tx1,2,3）
    //tx3 3（tx1,2,3）3（tx1,2,3）
    pool.getTransactionEssential（tx1->getHash（），祖先，后代）；
    boost_check_equal（祖先，1所有）；
    Boost_check_equal（后代，3ull）；
    pool.getTransactionEssential（tx2->getHash（），祖先，后代）；
    boost_check_equal（祖先，2ull）；
    Boost_check_equal（后代，3ull）；
    pool.getTransactionEssential（tx3->getHash（），祖先，后代）；
    Boost_check_equal（祖先，3ull）；
    Boost_check_equal（后代，3ull）；

    /*大孩子2*/

//
//[tx1].0<-[tx2].0<-[tx3]
//γ
//\--- 1 <-[TX4]
//
    /*ansactionRef tx4=生成_tx（/*输出_值*/290*分，250*分，/*输入*/tx2，/*输入_指数*/1）；
    pool.addunchecked（entry.fee（10000ll）.fromtx（tx4））；

    //祖先/后代应为：
    //事务祖先后代
    //==========================
    //tx1 1（tx1）4（tx1,2,3,4）
    //tx2 2（tx1,2）4（tx1,2,3,4）
    //tx3 3（tx1,2,3）4（tx1,2,3,4）
    //tx4 3（tx1,2,4）4（tx1,2,3,4）
    pool.getTransactionEssential（tx1->getHash（），祖先，后代）；
    boost_check_equal（祖先，1所有）；
    Boost_check_equal（后代，4ull）；
    pool.getTransactionEssential（tx2->getHash（），祖先，后代）；
    boost_check_equal（祖先，2ull）；
    Boost_check_equal（后代，4ull）；
    pool.getTransactionEssential（tx3->getHash（），祖先，后代）；
    Boost_check_equal（祖先，3ull）；
    Boost_check_equal（后代，4ull）；
    pool.getTransactionEssential（tx4->getHash（），祖先，后代）；
    Boost_check_equal（祖先，3ull）；
    Boost_check_equal（后代，4ull）；

    /*制作一个较长的备用分支并将其连接到TX3*/

//
//[ty1].0<-[ty2].0<-[ty3].0<-[ty4].0<-[ty5].0
//γ
//[tx1].0<-[tx2].0<-[tx3].0<-[ty6]----/
//γ
//\--- 1 <-[TX4]
//
    CTransactionRef ty1, ty2, ty3, ty4, ty5;
    CTransactionRef* ty[5] = {&ty1, &ty2, &ty3, &ty4, &ty5};
    CAmount v = 5 * COIN;
    for (uint64_t i = 0; i < 5; i++) {
        CTransactionRef& tyi = *ty[i];
        /*=生成_Tx（/*输出_值*/v，/*输入*/i>0？std:：vector<ctransactionref>*ty[i-1]：std:：vector<ctransactionref>）；
        V＝50＊%；
        pool.addunchecked（entry.fee（10000ll）.fromtx（tyi））；
        pool.getTransactionEssential（tyi->getHash（），祖先，后代）；
        boost_check_equal（祖先，i+1）；
        boost_check_equal（后代，i+1）；
    }
    ctransactionref ty6=生成tx（/*输出值*/ {5 * COIN}, /* inputs */ {tx3, ty5});

    pool.addUnchecked(entry.Fee(10000LL).FromTx(ty6));

//祖先/后代应：
//事务祖先后代
//=======================================
//tx1 1（tx1）5（tx1、2、3、4、ty6）
//tx2 2（tx1,2）5（tx1,2,3,4，ty6）
//tx3 3（tx1、2、3）5（tx1、2、3、4、ty6）
//tx4 3（tx1,2,4）5（tx1,2,3,4，ty6）
//ty1 1（ty1）6（ty1、2、3、4、5、6）
//ty2 2（ty1,2）6（ty1,2,3,4,5,6）
//ty3 3（ty1,2,3）6（ty1,2,3,4,5,6）
//ty4 4（y1234）6（ty1、2、3、4、5、6）
//ty5 5（y12345）6（ty1、2、3、4、5、6）
//ty6 9（tx123、ty123456）6（ty1、2、3、4、5、6）
    pool.GetTransactionAncestry(tx1->GetHash(), ancestors, descendants);
    BOOST_CHECK_EQUAL(ancestors, 1ULL);
    BOOST_CHECK_EQUAL(descendants, 5ULL);
    pool.GetTransactionAncestry(tx2->GetHash(), ancestors, descendants);
    BOOST_CHECK_EQUAL(ancestors, 2ULL);
    BOOST_CHECK_EQUAL(descendants, 5ULL);
    pool.GetTransactionAncestry(tx3->GetHash(), ancestors, descendants);
    BOOST_CHECK_EQUAL(ancestors, 3ULL);
    BOOST_CHECK_EQUAL(descendants, 5ULL);
    pool.GetTransactionAncestry(tx4->GetHash(), ancestors, descendants);
    BOOST_CHECK_EQUAL(ancestors, 3ULL);
    BOOST_CHECK_EQUAL(descendants, 5ULL);
    pool.GetTransactionAncestry(ty1->GetHash(), ancestors, descendants);
    BOOST_CHECK_EQUAL(ancestors, 1ULL);
    BOOST_CHECK_EQUAL(descendants, 6ULL);
    pool.GetTransactionAncestry(ty2->GetHash(), ancestors, descendants);
    BOOST_CHECK_EQUAL(ancestors, 2ULL);
    BOOST_CHECK_EQUAL(descendants, 6ULL);
    pool.GetTransactionAncestry(ty3->GetHash(), ancestors, descendants);
    BOOST_CHECK_EQUAL(ancestors, 3ULL);
    BOOST_CHECK_EQUAL(descendants, 6ULL);
    pool.GetTransactionAncestry(ty4->GetHash(), ancestors, descendants);
    BOOST_CHECK_EQUAL(ancestors, 4ULL);
    BOOST_CHECK_EQUAL(descendants, 6ULL);
    pool.GetTransactionAncestry(ty5->GetHash(), ancestors, descendants);
    BOOST_CHECK_EQUAL(ancestors, 5ULL);
    BOOST_CHECK_EQUAL(descendants, 6ULL);
    pool.GetTransactionAncestry(ty6->GetHash(), ancestors, descendants);
    BOOST_CHECK_EQUAL(ancestors, 9ULL);
    BOOST_CHECK_EQUAL(descendants, 6ULL);
}

BOOST_AUTO_TEST_SUITE_END()
