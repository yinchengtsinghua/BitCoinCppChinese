
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2014-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <attributes.h>
#include <coins.h>
#include <consensus/validation.h>
#include <script/standard.h>
#include <test/test_bitcoin.h>
#include <uint256.h>
#include <undo.h>
#include <util/strencodings.h>
#include <validation.h>

#include <map>
#include <vector>

#include <boost/test/unit_test.hpp>

int ApplyTxInUndo(Coin&& undo, CCoinsViewCache& view, const COutPoint& out);
void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, CTxUndo &txundo, int nHeight);

namespace
{
//！相等检验
bool operator==(const Coin &a, const Coin &b) {
//空硬币总是相等的。
    if (a.IsSpent() && b.IsSpent()) return true;
    return a.fCoinBase == b.fCoinBase &&
           a.nHeight == b.nHeight &&
           a.out == b.out;
}

class CCoinsViewTest : public CCoinsView
{
    uint256 hashBestBlock_;
    std::map<COutPoint, Coin> map_;

public:
    NODISCARD bool GetCoin(const COutPoint& outpoint, Coin& coin) const override
    {
        std::map<COutPoint, Coin>::const_iterator it = map_.find(outpoint);
        if (it == map_.end()) {
            return false;
        }
        coin = it->second;
        if (coin.IsSpent() && InsecureRandBool() == 0) {
//如果输入为空，则随机返回false。
            return false;
        }
        return true;
    }

    uint256 GetBestBlock() const override { return hashBestBlock_; }

    bool BatchWrite(CCoinsMap& mapCoins, const uint256& hashBlock) override
    {
        for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end(); ) {
            if (it->second.flags & CCoinsCacheEntry::DIRTY) {
//在ccoinsviewdb中使用的相同优化是只写脏条目。
                map_[it->first] = it->second.coin;
                if (it->second.coin.IsSpent() && InsecureRandRange(3) == 0) {
//在写入时随机删除空条目。
                    map_.erase(it->first);
                }
            }
            mapCoins.erase(it++);
        }
        if (!hashBlock.IsNull())
            hashBestBlock_ = hashBlock;
        return true;
    }
};

class CCoinsViewCacheTest : public CCoinsViewCache
{
public:
    explicit CCoinsViewCacheTest(CCoinsView* _base) : CCoinsViewCache(_base) {}

    void SelfTest() const
    {
//手动重新计算整个数据的动态使用，并进行比较。
        size_t ret = memusage::DynamicUsage(cacheCoins);
        size_t count = 0;
        for (const auto& entry : cacheCoins) {
            ret += entry.second.coin.DynamicMemoryUsage();
            ++count;
        }
        BOOST_CHECK_EQUAL(GetCacheSize(), count);
        BOOST_CHECK_EQUAL(DynamicMemoryUsage(), ret);
    }

    CCoinsMap& map() const { return cacheCoins; }
    size_t& usage() const { return cachedCoinsUsage; }
};

} //命名空间

BOOST_FIXTURE_TEST_SUITE(coins_tests, BasicTestingSetup)

static const unsigned int NUM_SIMULATION_ITERATIONS = 40000;

//这是一个大型随机插入/移除模拟测试，大小可变。
//CCoinsViewTest顶部的缓存堆栈。
//
//它将随机创建/更新/删除缓存尖端的硬币条目，
//从随机256位散列的有限列表中选择的TxID。偶尔，A
//新提示被添加到缓存堆栈中，或者提示被刷新和删除。
//
//在这个过程中，布尔值被保持，以确保
//操作命中所有分支。
BOOST_AUTO_TEST_CASE(coins_cache_simulation_test)
{
//各种覆盖追踪器。
    bool removed_all_caches = false;
    bool reached_4_caches = false;
    bool added_an_entry = false;
    bool added_an_unspendable_entry = false;
    bool removed_an_entry = false;
    bool updated_an_entry = false;
    bool found_an_entry = false;
    bool missed_an_entry = false;
    bool uncached_an_entry = false;

//一个简单的映射来跟踪我们期望缓存堆栈表示的内容。
    std::map<COutPoint, Coin> result;

//缓存堆栈。
CCoinsViewTest base; //底部的ccoinsview测试。
std::vector<CCoinsViewCacheTest*> stack; //上面有一堆ccoinsviewcaches。
stack.push_back(new CCoinsViewCacheTest(&base)); //从一个缓存开始。

//使用一组有限的随机事务ID，因此我们测试覆盖条目。
    std::vector<uint256> txids;
    txids.resize(NUM_SIMULATION_ITERATIONS / 8);
    for (unsigned int i = 0; i < txids.size(); i++) {
        txids[i] = InsecureRand256();
    }

    for (unsigned int i = 0; i < NUM_SIMULATION_ITERATIONS; i++) {
//随机修改。
        {
uint256 txid = txids[InsecureRandRange(txids.size())]; //我们将在这个迭代中修改txid。
            Coin& coin = result[COutPoint(txid, 0)];

//确定是否在访问之前或之后测试HaveCoin*（或两者）。作为这些功能
//可以通过将事物拉入缓存（所有组合）来影响彼此的行为
//进行了测试。
            bool test_havecoin_before = InsecureRandBits(2) == 0;
            bool test_havecoin_after = InsecureRandBits(2) == 0;

            bool result_havecoin = test_havecoin_before ? stack.back()->HaveCoin(COutPoint(txid, 0)) : false;
            const Coin& entry = (InsecureRandRange(500) == 0) ? AccessByTxid(*stack.back(), txid) : stack.back()->AccessCoin(COutPoint(txid, 0));
            BOOST_CHECK(coin == entry);
            BOOST_CHECK(!test_havecoin_before || result_havecoin == !entry.IsSpent());

            if (test_havecoin_after) {
                bool ret = stack.back()->HaveCoin(COutPoint(txid, 0));
                BOOST_CHECK(ret == !entry.IsSpent());
            }

            if (InsecureRandRange(5) == 0 || coin.IsSpent()) {
                Coin newcoin;
                newcoin.out.nValue = InsecureRand32();
                newcoin.nHeight = 1;
                if (InsecureRandRange(16) == 0 && coin.IsSpent()) {
                    newcoin.out.scriptPubKey.assign(1 + InsecureRandBits(6), OP_RETURN);
                    BOOST_CHECK(newcoin.out.scriptPubKey.IsUnspendable());
                    added_an_unspendable_entry = true;
                } else {
newcoin.out.scriptPubKey.assign(InsecureRandBits(6), 0); //随机大小，这样我们就可以测试内存使用情况
                    (coin.IsSpent() ? added_an_entry : updated_an_entry) = true;
                    coin = newcoin;
                }
                stack.back()->AddCoin(COutPoint(txid, 0), std::move(newcoin), !coin.IsSpent() || InsecureRand32() & 1);
            } else {
                removed_an_entry = true;
                coin.Clear();
                BOOST_CHECK(stack.back()->SpendCoin(COutPoint(txid, 0)));
            }
        }

//每10次迭代一次，从缓存中删除一个随机条目
        if (InsecureRandRange(10) == 0) {
            COutPoint out(txids[InsecureRand32() % txids.size()], 0);
            int cacheid = InsecureRand32() % stack.size();
            stack[cacheid]->Uncache(out);
            uncached_an_entry |= !stack[cacheid]->HaveCoinInCache(out);
        }

//每1000次迭代一次，并在最后验证完整的缓存。
        if (InsecureRandRange(1000) == 1 || i == NUM_SIMULATION_ITERATIONS - 1) {
            for (const auto& entry : result) {
                bool have = stack.back()->HaveCoin(entry.first);
                const Coin& coin = stack.back()->AccessCoin(entry.first);
                BOOST_CHECK(have == !coin.IsSpent());
                BOOST_CHECK(coin == entry.second);
                if (coin.IsSpent()) {
                    missed_an_entry = true;
                } else {
                    BOOST_CHECK(stack.back()->HaveCoinInCache(entry.first));
                    found_an_entry = true;
                }
            }
            for (const CCoinsViewCacheTest *test : stack) {
                test->SelfTest();
            }
        }

        if (InsecureRandRange(100) == 0) {
//每100次迭代，刷新一个中间缓存
            if (stack.size() > 1 && InsecureRandBool() == 0) {
                unsigned int flushIndex = InsecureRandRange(stack.size() - 1);
                BOOST_CHECK(stack[flushIndex]->Flush());
            }
        }
        if (InsecureRandRange(100) == 0) {
//每100次迭代，更改缓存堆栈。
            if (stack.size() > 0 && InsecureRandBool() == 0) {
//删除顶部缓存
                BOOST_CHECK(stack.back()->Flush());
                delete stack.back();
                stack.pop_back();
            }
            if (stack.size() == 0 || (stack.size() < 4 && InsecureRandBool())) {
//添加新的缓存
                CCoinsView* tip = &base;
                if (stack.size() > 0) {
                    tip = stack.back();
                } else {
                    removed_all_caches = true;
                }
                stack.push_back(new CCoinsViewCacheTest(tip));
                if (stack.size() == 4) {
                    reached_4_caches = true;
                }
            }
        }
    }

//清理烟囱。
    while (stack.size() > 0) {
        delete stack.back();
        stack.pop_back();
    }

//核实覆盖范围。
    BOOST_CHECK(removed_all_caches);
    BOOST_CHECK(reached_4_caches);
    BOOST_CHECK(added_an_entry);
    BOOST_CHECK(added_an_unspendable_entry);
    BOOST_CHECK(removed_an_entry);
    BOOST_CHECK(updated_an_entry);
    BOOST_CHECK(found_an_entry);
    BOOST_CHECK(missed_an_entry);
    BOOST_CHECK(uncached_an_entry);
}

//存储下一次测试所需的所有Tx和撤消数据
typedef std::map<COutPoint, std::tuple<CTransaction,CTxUndo,Coin>> UtxoData;
UtxoData utxoData;

UtxoData::iterator FindRandomFrom(const std::set<COutPoint> &utxoSet) {
    assert(utxoSet.size());
    auto utxoSetIt = utxoSet.lower_bound(COutPoint(InsecureRand256(), 0));
    if (utxoSetIt == utxoSet.end()) {
        utxoSetIt = utxoSet.begin();
    }
    auto utxoDataIt = utxoData.find(*utxoSetIt);
    assert(utxoDataIt != utxoData.end());
    return utxoDataIt;
}


//此测试与上一个测试类似
//但重点是测试updateCoins的功能
//创建随机tx并使用updateCoins更新缓存堆栈
//特别是测试花费一个重复的coinbase-tx
//具有预期效果（其他副本在所有缓存级别都被覆盖）
BOOST_AUTO_TEST_CASE(updatecoins_simulation_test)
{
    bool spent_a_duplicate_coinbase = false;
//一个简单的映射来跟踪我们期望缓存堆栈表示的内容。
    std::map<COutPoint, Coin> result;

//缓存堆栈。
CCoinsViewTest base; //底部的ccoinsview测试。
std::vector<CCoinsViewCacheTest*> stack; //上面有一堆ccoinsviewcaches。
stack.push_back(new CCoinsViewCacheTest(&base)); //从一个缓存开始。

//跟踪我们在不同集合中使用的TxID
    std::set<COutPoint> coinbase_coins;
    std::set<COutPoint> disconnected_coins;
    std::set<COutPoint> duplicate_coins;
    std::set<COutPoint> utxoset;

    for (unsigned int i = 0; i < NUM_SIMULATION_ITERATIONS; i++) {
        uint32_t randiter = InsecureRand32();

//19/20 TXS添加新交易
        if (randiter % 20 < 19) {
            CMutableTransaction tx;
            tx.vin.resize(1);
            tx.vout.resize(1);
tx.vout[0].nValue = i; //保持TXS唯一，除非打算复制
tx.vout[0].scriptPubKey.assign(InsecureRand32() & 0x3F, 0); //随机大小，这样我们就可以测试内存使用情况
            unsigned int height = InsecureRand32();
            Coin old_coin;

//2/20次创建新的CoinBase
            if (randiter % 20 < 2 || coinbase_coins.size() < 10) {
//其中1/10的时间创建了一个重复的coinbase
                if (InsecureRandRange(10) == 0 && coinbase_coins.size()) {
                    auto utxod = FindRandomFrom(coinbase_coins);
//重复使用完全相同的coinbase
                    tx = CMutableTransaction{std::get<0>(utxod->second)};
//如果它被复制，就不能重新连接
                    disconnected_coins.erase(utxod->first);

                    duplicate_coins.insert(utxod->first);
                }
                else {
                    coinbase_coins.insert(COutPoint(tx.GetHash(), 0));
                }
                assert(CTransaction(tx).IsCoinBase());
            }

//17/20次重新连接上一个或添加常规Tx
            else {

                COutPoint prevout;
//1/20次重新连接先前断开的TX
                if (randiter % 20 == 2 && disconnected_coins.size()) {
                    auto utxod = FindRandomFrom(disconnected_coins);
                    tx = CMutableTransaction{std::get<0>(utxod->second)};
                    prevout = tx.vin[0].prevout;
                    if (!CTransaction(tx).IsCoinBase() && !utxoset.count(prevout)) {
                        disconnected_coins.erase(utxod->first);
                        continue;
                    }

//如果这个tx已经在utxo中，那么它必须是一个coinbase，并且必须是一个副本
                    if (utxoset.count(utxod->first)) {
                        assert(CTransaction(tx).IsCoinBase());
                        assert(duplicate_coins.count(utxod->first));
                    }
                    disconnected_coins.erase(utxod->first);
                }

//16/20次创建常规Tx
                else {
                    auto utxod = FindRandomFrom(utxoset);
                    prevout = utxod->first;

//构造Tx来花费prevouthash的硬币
                    tx.vin[0].prevout = prevout;
                    assert(!CTransaction(tx).IsCoinBase());
                }
//在这个简单的测试中，硬币只有两种状态，即已用或未用，保存未用状态以恢复
                old_coin = result[prevout];
//更新prevouthash的预期结果，以知道这些硬币已用完
                result[prevout].Clear();

                utxoset.erase(prevout);

//该测试旨在确保重复的coinbase能够正常工作。
//如果发生了这种情况，并且没有恢复先前覆盖的coinbase
                if (duplicate_coins.count(prevout)) {
                    spent_a_duplicate_coinbase = true;
                }

            }
//更新预期结果以了解新的输出硬币
            assert(tx.vout.size() == 1);
            const COutPoint outpoint(tx.GetHash(), 0);
            result[outpoint] = Coin(tx.vout[0], height, CTransaction(tx).IsCoinBase());

//调用顶部缓存上的updateCoins
            CTxUndo undo;
            UpdateCoins(CTransaction(tx), *(stack.back()), undo, height);

//为将来的开销更新utxo集
            utxoset.insert(outpoint);

//跟踪此tx并撤消信息以供以后使用
            utxoData.emplace(outpoint, std::make_tuple(tx,undo,old_coin));
        } else if (utxoset.size()) {
//撤消上一个事务的1/20次
            auto utxod = FindRandomFrom(utxoset);

            CTransaction &tx = std::get<0>(utxod->second);
            CTxUndo &undo = std::get<1>(utxod->second);
            Coin &orig_coin = std::get<2>(utxod->second);

//更新预期结果
//删除新输出
            result[utxod->first].Clear();
//如果不是coinbase，则恢复prevout
            if (!tx.IsCoinBase()) {
                result[tx.vin[0].prevout] = orig_coin;
            }

//断开TX与当前UTXO的连接
//参见断开块中的代码
//移除输出
            BOOST_CHECK(stack.back()->SpendCoin(utxod->first));
//恢复输入
            if (!tx.IsCoinBase()) {
                const COutPoint &out = tx.vin[0].prevout;
                Coin coin = undo.vprevout[0];
                ApplyTxInUndo(std::move(coin), *(stack.back()), out);
            }
//作为重新连接的候选存储
            disconnected_coins.insert(utxod->first);

//更新utxoset
            utxoset.erase(utxod->first);
            if (!tx.IsCoinBase())
                utxoset.insert(tx.vin[0].prevout);
        }

//每1000次迭代一次，并在最后验证完整的缓存。
        if (InsecureRandRange(1000) == 1 || i == NUM_SIMULATION_ITERATIONS - 1) {
            for (const auto& entry : result) {
                bool have = stack.back()->HaveCoin(entry.first);
                const Coin& coin = stack.back()->AccessCoin(entry.first);
                BOOST_CHECK(have == !coin.IsSpent());
                BOOST_CHECK(coin == entry.second);
            }
        }

//每10次迭代一次，从缓存中删除一个随机条目
        if (utxoset.size() > 1 && InsecureRandRange(30) == 0) {
            stack[InsecureRand32() % stack.size()]->Uncache(FindRandomFrom(utxoset)->first);
        }
        if (disconnected_coins.size() > 1 && InsecureRandRange(30) == 0) {
            stack[InsecureRand32() % stack.size()]->Uncache(FindRandomFrom(disconnected_coins)->first);
        }
        if (duplicate_coins.size() > 1 && InsecureRandRange(30) == 0) {
            stack[InsecureRand32() % stack.size()]->Uncache(FindRandomFrom(duplicate_coins)->first);
        }

        if (InsecureRandRange(100) == 0) {
//每100次迭代，刷新一个中间缓存
            if (stack.size() > 1 && InsecureRandBool() == 0) {
                unsigned int flushIndex = InsecureRandRange(stack.size() - 1);
                BOOST_CHECK(stack[flushIndex]->Flush());
            }
        }
        if (InsecureRandRange(100) == 0) {
//每100次迭代，更改缓存堆栈。
            if (stack.size() > 0 && InsecureRandBool() == 0) {
                BOOST_CHECK(stack.back()->Flush());
                delete stack.back();
                stack.pop_back();
            }
            if (stack.size() == 0 || (stack.size() < 4 && InsecureRandBool())) {
                CCoinsView* tip = &base;
                if (stack.size() > 0) {
                    tip = stack.back();
                }
                stack.push_back(new CCoinsViewCacheTest(tip));
            }
        }
    }

//清理烟囱。
    while (stack.size() > 0) {
        delete stack.back();
        stack.pop_back();
    }

//核实覆盖范围。
    BOOST_CHECK(spent_a_duplicate_coinbase);
}

BOOST_AUTO_TEST_CASE(ccoins_serialization)
{
//好例子
    CDataStream ss1(ParseHex("97f23c835800816115944e077fe7c803cfa57f29b36bf87c1d35"), SER_DISK, CLIENT_VERSION);
    Coin cc1;
    ss1 >> cc1;
    BOOST_CHECK_EQUAL(cc1.fCoinBase, false);
    BOOST_CHECK_EQUAL(cc1.nHeight, 203998U);
    BOOST_CHECK_EQUAL(cc1.out.nValue, CAmount{60000000000});
    BOOST_CHECK_EQUAL(HexStr(cc1.out.scriptPubKey), HexStr(GetScriptForDestination(CKeyID(uint160(ParseHex("816115944e077fe7c803cfa57f29b36bf87c1d35"))))));

//好例子
    CDataStream ss2(ParseHex("8ddf77bbd123008c988f1a4a4de2161e0f50aac7f17e7f9555caa4"), SER_DISK, CLIENT_VERSION);
    Coin cc2;
    ss2 >> cc2;
    BOOST_CHECK_EQUAL(cc2.fCoinBase, true);
    BOOST_CHECK_EQUAL(cc2.nHeight, 120891U);
    BOOST_CHECK_EQUAL(cc2.out.nValue, 110397);
    BOOST_CHECK_EQUAL(HexStr(cc2.out.scriptPubKey), HexStr(GetScriptForDestination(CKeyID(uint160(ParseHex("8c988f1a4a4de2161e0f50aac7f17e7f9555caa4"))))));

//最小可能示例
    CDataStream ss3(ParseHex("000006"), SER_DISK, CLIENT_VERSION);
    Coin cc3;
    ss3 >> cc3;
    BOOST_CHECK_EQUAL(cc3.fCoinBase, false);
    BOOST_CHECK_EQUAL(cc3.nHeight, 0U);
    BOOST_CHECK_EQUAL(cc3.out.nValue, 0);
    BOOST_CHECK_EQUAL(cc3.out.scriptPubKey.size(), 0U);

//在流结尾之外结束的ScriptPubKey
    CDataStream ss4(ParseHex("000007"), SER_DISK, CLIENT_VERSION);
    try {
        Coin cc4;
        ss4 >> cc4;
        BOOST_CHECK_MESSAGE(false, "We should have thrown");
    } catch (const std::ios_base::failure&) {
    }

//超过流结尾的非常大的scriptpubkey（3*10^9字节）
    CDataStream tmp(SER_DISK, CLIENT_VERSION);
    uint64_t x = 3000000000ULL;
    tmp << VARINT(x);
    BOOST_CHECK_EQUAL(HexStr(tmp.begin(), tmp.end()), "8a95c0bb00");
    CDataStream ss5(ParseHex("00008a95c0bb00"), SER_DISK, CLIENT_VERSION);
    try {
        Coin cc5;
        ss5 >> cc5;
        BOOST_CHECK_MESSAGE(false, "We should have thrown");
    } catch (const std::ios_base::failure&) {
    }
}

const static COutPoint OUTPOINT;
const static CAmount PRUNED = -1;
const static CAmount ABSENT = -2;
const static CAmount FAIL = -3;
const static CAmount VALUE1 = 100;
const static CAmount VALUE2 = 200;
const static CAmount VALUE3 = 300;
const static char DIRTY = CCoinsCacheEntry::DIRTY;
const static char FRESH = CCoinsCacheEntry::FRESH;
const static char NO_ENTRY = -1;

const static auto FLAGS = {char(0), FRESH, DIRTY, char(DIRTY | FRESH)};
const static auto CLEAN_FLAGS = {char(0), FRESH};
const static auto ABSENT_FLAGS = {NO_ENTRY};

static void SetCoinsValue(CAmount value, Coin& coin)
{
    assert(value != ABSENT);
    coin.Clear();
    assert(coin.IsSpent());
    if (value != PRUNED) {
        coin.out.nValue = value;
        coin.nHeight = 1;
        assert(!coin.IsSpent());
    }
}

static size_t InsertCoinsMapEntry(CCoinsMap& map, CAmount value, char flags)
{
    if (value == ABSENT) {
        assert(flags == NO_ENTRY);
        return 0;
    }
    assert(flags != NO_ENTRY);
    CCoinsCacheEntry entry;
    entry.flags = flags;
    SetCoinsValue(value, entry.coin);
    auto inserted = map.emplace(OUTPOINT, std::move(entry));
    assert(inserted.second);
    return inserted.first->second.coin.DynamicMemoryUsage();
}

void GetCoinsMapEntry(const CCoinsMap& map, CAmount& value, char& flags)
{
    auto it = map.find(OUTPOINT);
    if (it == map.end()) {
        value = ABSENT;
        flags = NO_ENTRY;
    } else {
        if (it->second.coin.IsSpent()) {
            value = PRUNED;
        } else {
            value = it->second.coin.out.nValue;
        }
        flags = it->second.flags;
        assert(flags != NO_ENTRY);
    }
}

void WriteCoinsViewEntry(CCoinsView& view, CAmount value, char flags)
{
    CCoinsMap map;
    InsertCoinsMapEntry(map, value, flags);
    BOOST_CHECK(view.BatchWrite(map, {}));
}

class SingleEntryCacheTest
{
public:
    SingleEntryCacheTest(CAmount base_value, CAmount cache_value, char cache_flags)
    {
        WriteCoinsViewEntry(base, base_value, base_value == ABSENT ? NO_ENTRY : DIRTY);
        cache.usage() += InsertCoinsMapEntry(cache.map(), cache_value, cache_flags);
    }

    CCoinsView root;
    CCoinsViewCacheTest base{&root};
    CCoinsViewCacheTest cache{&base};
};

static void CheckAccessCoin(CAmount base_value, CAmount cache_value, CAmount expected_value, char cache_flags, char expected_flags)
{
    SingleEntryCacheTest test(base_value, cache_value, cache_flags);
    test.cache.AccessCoin(OUTPOINT);
    test.cache.SelfTest();

    CAmount result_value;
    char result_flags;
    GetCoinsMapEntry(test.cache.map(), result_value, result_flags);
    BOOST_CHECK_EQUAL(result_value, expected_value);
    BOOST_CHECK_EQUAL(result_flags, expected_flags);
}

BOOST_AUTO_TEST_CASE(ccoins_access)
{
    /*检查accesscoin行为，从分层的缓存视图请求一个硬币
     *位于基础视图的顶部，并在之后检查缓存中的结果项
     *访问。
     *
     *基本缓存结果缓存结果
     *值值值标志
     **/

    CheckAccessCoin(ABSENT, ABSENT, ABSENT, NO_ENTRY   , NO_ENTRY   );
    CheckAccessCoin(ABSENT, PRUNED, PRUNED, 0          , 0          );
    CheckAccessCoin(ABSENT, PRUNED, PRUNED, FRESH      , FRESH      );
    CheckAccessCoin(ABSENT, PRUNED, PRUNED, DIRTY      , DIRTY      );
    CheckAccessCoin(ABSENT, PRUNED, PRUNED, DIRTY|FRESH, DIRTY|FRESH);
    CheckAccessCoin(ABSENT, VALUE2, VALUE2, 0          , 0          );
    CheckAccessCoin(ABSENT, VALUE2, VALUE2, FRESH      , FRESH      );
    CheckAccessCoin(ABSENT, VALUE2, VALUE2, DIRTY      , DIRTY      );
    CheckAccessCoin(ABSENT, VALUE2, VALUE2, DIRTY|FRESH, DIRTY|FRESH);
    CheckAccessCoin(PRUNED, ABSENT, ABSENT, NO_ENTRY   , NO_ENTRY   );
    CheckAccessCoin(PRUNED, PRUNED, PRUNED, 0          , 0          );
    CheckAccessCoin(PRUNED, PRUNED, PRUNED, FRESH      , FRESH      );
    CheckAccessCoin(PRUNED, PRUNED, PRUNED, DIRTY      , DIRTY      );
    CheckAccessCoin(PRUNED, PRUNED, PRUNED, DIRTY|FRESH, DIRTY|FRESH);
    CheckAccessCoin(PRUNED, VALUE2, VALUE2, 0          , 0          );
    CheckAccessCoin(PRUNED, VALUE2, VALUE2, FRESH      , FRESH      );
    CheckAccessCoin(PRUNED, VALUE2, VALUE2, DIRTY      , DIRTY      );
    CheckAccessCoin(PRUNED, VALUE2, VALUE2, DIRTY|FRESH, DIRTY|FRESH);
    CheckAccessCoin(VALUE1, ABSENT, VALUE1, NO_ENTRY   , 0          );
    CheckAccessCoin(VALUE1, PRUNED, PRUNED, 0          , 0          );
    CheckAccessCoin(VALUE1, PRUNED, PRUNED, FRESH      , FRESH      );
    CheckAccessCoin(VALUE1, PRUNED, PRUNED, DIRTY      , DIRTY      );
    CheckAccessCoin(VALUE1, PRUNED, PRUNED, DIRTY|FRESH, DIRTY|FRESH);
    CheckAccessCoin(VALUE1, VALUE2, VALUE2, 0          , 0          );
    CheckAccessCoin(VALUE1, VALUE2, VALUE2, FRESH      , FRESH      );
    CheckAccessCoin(VALUE1, VALUE2, VALUE2, DIRTY      , DIRTY      );
    CheckAccessCoin(VALUE1, VALUE2, VALUE2, DIRTY|FRESH, DIRTY|FRESH);
}

static void CheckSpendCoins(CAmount base_value, CAmount cache_value, CAmount expected_value, char cache_flags, char expected_flags)
{
    SingleEntryCacheTest test(base_value, cache_value, cache_flags);
    test.cache.SpendCoin(OUTPOINT);
    test.cache.SelfTest();

    CAmount result_value;
    char result_flags;
    GetCoinsMapEntry(test.cache.map(), result_value, result_flags);
    BOOST_CHECK_EQUAL(result_value, expected_value);
    BOOST_CHECK_EQUAL(result_flags, expected_flags);
};

BOOST_AUTO_TEST_CASE(ccoins_spend)
{
    /*检查spendcoin行为，从分层的缓存视图请求硬币
     *基本视图顶部，支出，然后检查
     *修改后缓存中的结果项。
     *
     *基本缓存结果缓存结果
     *值值值标志
     **/

    CheckSpendCoins(ABSENT, ABSENT, ABSENT, NO_ENTRY   , NO_ENTRY   );
    CheckSpendCoins(ABSENT, PRUNED, PRUNED, 0          , DIRTY      );
    CheckSpendCoins(ABSENT, PRUNED, ABSENT, FRESH      , NO_ENTRY   );
    CheckSpendCoins(ABSENT, PRUNED, PRUNED, DIRTY      , DIRTY      );
    CheckSpendCoins(ABSENT, PRUNED, ABSENT, DIRTY|FRESH, NO_ENTRY   );
    CheckSpendCoins(ABSENT, VALUE2, PRUNED, 0          , DIRTY      );
    CheckSpendCoins(ABSENT, VALUE2, ABSENT, FRESH      , NO_ENTRY   );
    CheckSpendCoins(ABSENT, VALUE2, PRUNED, DIRTY      , DIRTY      );
    CheckSpendCoins(ABSENT, VALUE2, ABSENT, DIRTY|FRESH, NO_ENTRY   );
    CheckSpendCoins(PRUNED, ABSENT, ABSENT, NO_ENTRY   , NO_ENTRY   );
    CheckSpendCoins(PRUNED, PRUNED, PRUNED, 0          , DIRTY      );
    CheckSpendCoins(PRUNED, PRUNED, ABSENT, FRESH      , NO_ENTRY   );
    CheckSpendCoins(PRUNED, PRUNED, PRUNED, DIRTY      , DIRTY      );
    CheckSpendCoins(PRUNED, PRUNED, ABSENT, DIRTY|FRESH, NO_ENTRY   );
    CheckSpendCoins(PRUNED, VALUE2, PRUNED, 0          , DIRTY      );
    CheckSpendCoins(PRUNED, VALUE2, ABSENT, FRESH      , NO_ENTRY   );
    CheckSpendCoins(PRUNED, VALUE2, PRUNED, DIRTY      , DIRTY      );
    CheckSpendCoins(PRUNED, VALUE2, ABSENT, DIRTY|FRESH, NO_ENTRY   );
    CheckSpendCoins(VALUE1, ABSENT, PRUNED, NO_ENTRY   , DIRTY      );
    CheckSpendCoins(VALUE1, PRUNED, PRUNED, 0          , DIRTY      );
    CheckSpendCoins(VALUE1, PRUNED, ABSENT, FRESH      , NO_ENTRY   );
    CheckSpendCoins(VALUE1, PRUNED, PRUNED, DIRTY      , DIRTY      );
    CheckSpendCoins(VALUE1, PRUNED, ABSENT, DIRTY|FRESH, NO_ENTRY   );
    CheckSpendCoins(VALUE1, VALUE2, PRUNED, 0          , DIRTY      );
    CheckSpendCoins(VALUE1, VALUE2, ABSENT, FRESH      , NO_ENTRY   );
    CheckSpendCoins(VALUE1, VALUE2, PRUNED, DIRTY      , DIRTY      );
    CheckSpendCoins(VALUE1, VALUE2, ABSENT, DIRTY|FRESH, NO_ENTRY   );
}

static void CheckAddCoinBase(CAmount base_value, CAmount cache_value, CAmount modify_value, CAmount expected_value, char cache_flags, char expected_flags, bool coinbase)
{
    SingleEntryCacheTest test(base_value, cache_value, cache_flags);

    CAmount result_value;
    char result_flags;
    try {
        CTxOut output;
        output.nValue = modify_value;
        test.cache.AddCoin(OUTPOINT, Coin(std::move(output), 1, coinbase), coinbase);
        test.cache.SelfTest();
        GetCoinsMapEntry(test.cache.map(), result_value, result_flags);
    } catch (std::logic_error&) {
        result_value = FAIL;
        result_flags = NO_ENTRY;
    }

    BOOST_CHECK_EQUAL(result_value, expected_value);
    BOOST_CHECK_EQUAL(result_flags, expected_flags);
}

//上面的checkAddCoinBase函数的简单包装
//不同的可能基值，确保每个基值给出相同的结果。
//这个包装可以让硬币加上下面的测试变得更短，重复性更低，
//同时仍在验证CoinsView缓存：：AddCoin实现
//忽略基值。
template <typename... Args>
static void CheckAddCoin(Args&&... args)
{
    for (const CAmount base_value : {ABSENT, PRUNED, VALUE1})
        CheckAddCoinBase(base_value, std::forward<Args>(args)...);
}

BOOST_AUTO_TEST_CASE(ccoins_add)
{
    /*检查addcoin行为，从缓存视图请求新的coin，
     *对硬币进行修改，然后检查结果
     *修改后缓存中的条目。使用验证行为
     *将addcoin potential_overwrite参数设置为false和true。
     *
     *缓存写入结果缓存结果可能覆盖
     *值值值标志
     **/

    CheckAddCoin(ABSENT, VALUE3, VALUE3, NO_ENTRY   , DIRTY|FRESH, false);
    CheckAddCoin(ABSENT, VALUE3, VALUE3, NO_ENTRY   , DIRTY      , true );
    CheckAddCoin(PRUNED, VALUE3, VALUE3, 0          , DIRTY|FRESH, false);
    CheckAddCoin(PRUNED, VALUE3, VALUE3, 0          , DIRTY      , true );
    CheckAddCoin(PRUNED, VALUE3, VALUE3, FRESH      , DIRTY|FRESH, false);
    CheckAddCoin(PRUNED, VALUE3, VALUE3, FRESH      , DIRTY|FRESH, true );
    CheckAddCoin(PRUNED, VALUE3, VALUE3, DIRTY      , DIRTY      , false);
    CheckAddCoin(PRUNED, VALUE3, VALUE3, DIRTY      , DIRTY      , true );
    CheckAddCoin(PRUNED, VALUE3, VALUE3, DIRTY|FRESH, DIRTY|FRESH, false);
    CheckAddCoin(PRUNED, VALUE3, VALUE3, DIRTY|FRESH, DIRTY|FRESH, true );
    CheckAddCoin(VALUE2, VALUE3, FAIL  , 0          , NO_ENTRY   , false);
    CheckAddCoin(VALUE2, VALUE3, VALUE3, 0          , DIRTY      , true );
    CheckAddCoin(VALUE2, VALUE3, FAIL  , FRESH      , NO_ENTRY   , false);
    CheckAddCoin(VALUE2, VALUE3, VALUE3, FRESH      , DIRTY|FRESH, true );
    CheckAddCoin(VALUE2, VALUE3, FAIL  , DIRTY      , NO_ENTRY   , false);
    CheckAddCoin(VALUE2, VALUE3, VALUE3, DIRTY      , DIRTY      , true );
    CheckAddCoin(VALUE2, VALUE3, FAIL  , DIRTY|FRESH, NO_ENTRY   , false);
    CheckAddCoin(VALUE2, VALUE3, VALUE3, DIRTY|FRESH, DIRTY|FRESH, true );
}

void CheckWriteCoins(CAmount parent_value, CAmount child_value, CAmount expected_value, char parent_flags, char child_flags, char expected_flags)
{
    SingleEntryCacheTest test(ABSENT, parent_value, parent_flags);

    CAmount result_value;
    char result_flags;
    try {
        WriteCoinsViewEntry(test.cache, child_value, child_flags);
        test.cache.SelfTest();
        GetCoinsMapEntry(test.cache.map(), result_value, result_flags);
    } catch (std::logic_error&) {
        result_value = FAIL;
        result_flags = NO_ENTRY;
    }

    BOOST_CHECK_EQUAL(result_value, expected_value);
    BOOST_CHECK_EQUAL(result_flags, expected_flags);
}

BOOST_AUTO_TEST_CASE(ccoins_write)
{
    /*检查批写行为，将一个条目从子缓存刷新到
     *父缓存，并检查父缓存中的结果项
     *写完后。
     *
     *父-子结果父-子结果
     *值值值标志标志标志
     **/

    CheckWriteCoins(ABSENT, ABSENT, ABSENT, NO_ENTRY   , NO_ENTRY   , NO_ENTRY   );
    CheckWriteCoins(ABSENT, PRUNED, PRUNED, NO_ENTRY   , DIRTY      , DIRTY      );
    CheckWriteCoins(ABSENT, PRUNED, ABSENT, NO_ENTRY   , DIRTY|FRESH, NO_ENTRY   );
    CheckWriteCoins(ABSENT, VALUE2, VALUE2, NO_ENTRY   , DIRTY      , DIRTY      );
    CheckWriteCoins(ABSENT, VALUE2, VALUE2, NO_ENTRY   , DIRTY|FRESH, DIRTY|FRESH);
    CheckWriteCoins(PRUNED, ABSENT, PRUNED, 0          , NO_ENTRY   , 0          );
    CheckWriteCoins(PRUNED, ABSENT, PRUNED, FRESH      , NO_ENTRY   , FRESH      );
    CheckWriteCoins(PRUNED, ABSENT, PRUNED, DIRTY      , NO_ENTRY   , DIRTY      );
    CheckWriteCoins(PRUNED, ABSENT, PRUNED, DIRTY|FRESH, NO_ENTRY   , DIRTY|FRESH);
    CheckWriteCoins(PRUNED, PRUNED, PRUNED, 0          , DIRTY      , DIRTY      );
    CheckWriteCoins(PRUNED, PRUNED, PRUNED, 0          , DIRTY|FRESH, DIRTY      );
    CheckWriteCoins(PRUNED, PRUNED, ABSENT, FRESH      , DIRTY      , NO_ENTRY   );
    CheckWriteCoins(PRUNED, PRUNED, ABSENT, FRESH      , DIRTY|FRESH, NO_ENTRY   );
    CheckWriteCoins(PRUNED, PRUNED, PRUNED, DIRTY      , DIRTY      , DIRTY      );
    CheckWriteCoins(PRUNED, PRUNED, PRUNED, DIRTY      , DIRTY|FRESH, DIRTY      );
    CheckWriteCoins(PRUNED, PRUNED, ABSENT, DIRTY|FRESH, DIRTY      , NO_ENTRY   );
    CheckWriteCoins(PRUNED, PRUNED, ABSENT, DIRTY|FRESH, DIRTY|FRESH, NO_ENTRY   );
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, 0          , DIRTY      , DIRTY      );
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, 0          , DIRTY|FRESH, DIRTY      );
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, FRESH      , DIRTY      , DIRTY|FRESH);
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, FRESH      , DIRTY|FRESH, DIRTY|FRESH);
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, DIRTY      , DIRTY      , DIRTY      );
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, DIRTY      , DIRTY|FRESH, DIRTY      );
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, DIRTY|FRESH, DIRTY      , DIRTY|FRESH);
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, DIRTY|FRESH, DIRTY|FRESH, DIRTY|FRESH);
    CheckWriteCoins(VALUE1, ABSENT, VALUE1, 0          , NO_ENTRY   , 0          );
    CheckWriteCoins(VALUE1, ABSENT, VALUE1, FRESH      , NO_ENTRY   , FRESH      );
    CheckWriteCoins(VALUE1, ABSENT, VALUE1, DIRTY      , NO_ENTRY   , DIRTY      );
    CheckWriteCoins(VALUE1, ABSENT, VALUE1, DIRTY|FRESH, NO_ENTRY   , DIRTY|FRESH);
    CheckWriteCoins(VALUE1, PRUNED, PRUNED, 0          , DIRTY      , DIRTY      );
    CheckWriteCoins(VALUE1, PRUNED, FAIL  , 0          , DIRTY|FRESH, NO_ENTRY   );
    CheckWriteCoins(VALUE1, PRUNED, ABSENT, FRESH      , DIRTY      , NO_ENTRY   );
    CheckWriteCoins(VALUE1, PRUNED, FAIL  , FRESH      , DIRTY|FRESH, NO_ENTRY   );
    CheckWriteCoins(VALUE1, PRUNED, PRUNED, DIRTY      , DIRTY      , DIRTY      );
    CheckWriteCoins(VALUE1, PRUNED, FAIL  , DIRTY      , DIRTY|FRESH, NO_ENTRY   );
    CheckWriteCoins(VALUE1, PRUNED, ABSENT, DIRTY|FRESH, DIRTY      , NO_ENTRY   );
    CheckWriteCoins(VALUE1, PRUNED, FAIL  , DIRTY|FRESH, DIRTY|FRESH, NO_ENTRY   );
    CheckWriteCoins(VALUE1, VALUE2, VALUE2, 0          , DIRTY      , DIRTY      );
    CheckWriteCoins(VALUE1, VALUE2, FAIL  , 0          , DIRTY|FRESH, NO_ENTRY   );
    CheckWriteCoins(VALUE1, VALUE2, VALUE2, FRESH      , DIRTY      , DIRTY|FRESH);
    CheckWriteCoins(VALUE1, VALUE2, FAIL  , FRESH      , DIRTY|FRESH, NO_ENTRY   );
    CheckWriteCoins(VALUE1, VALUE2, VALUE2, DIRTY      , DIRTY      , DIRTY      );
    CheckWriteCoins(VALUE1, VALUE2, FAIL  , DIRTY      , DIRTY|FRESH, NO_ENTRY   );
    CheckWriteCoins(VALUE1, VALUE2, VALUE2, DIRTY|FRESH, DIRTY      , DIRTY|FRESH);
    CheckWriteCoins(VALUE1, VALUE2, FAIL  , DIRTY|FRESH, DIRTY|FRESH, NO_ENTRY   );

//上面的检查省略了子标志不脏的情况，因为
//它们太重复了（父缓存从未在这些
//病例）。下面的循环覆盖了这些情况，并确保父缓存
//总是保持不变。
    for (const CAmount parent_value : {ABSENT, PRUNED, VALUE1})
        for (const CAmount child_value : {ABSENT, PRUNED, VALUE2})
            for (const char parent_flags : parent_value == ABSENT ? ABSENT_FLAGS : FLAGS)
                for (const char child_flags : child_value == ABSENT ? ABSENT_FLAGS : CLEAN_FLAGS)
                    CheckWriteCoins(parent_value, child_value, parent_value, parent_flags, child_flags, parent_flags);
}

BOOST_AUTO_TEST_SUITE_END()
