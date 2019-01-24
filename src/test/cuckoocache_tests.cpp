
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
#include <boost/test/unit_test.hpp>
#include <cuckoocache.h>
#include <script/sigcache.h>
#include <test/test_bitcoin.h>
#include <random.h>
#include <thread>

/*布谷缓存测试套件
 *
 *1）所有测试都应具有确定性结果（使用不安全的RAND）
 *具有确定性种子）
 *2）为便于测试，模板化了一些测试方法。
 *与新版本/比较
 *3）结果应视为回归测试，即行为
 *与预期相比有显著变化。这可以是正常的，取决于
 *更改的性质，但需要更新测试以反映新的
 *预期行为。例如，提高命中率可能会导致一些测试
 *使用Boost_检查关闭失败。
 *
 **/

BOOST_AUTO_TEST_SUITE(cuckoocache_tests);

/*测试是否没有从缓存中读取未插入的值。
 *
 *前200000个不安全的\getrandhash调用中没有重复
 **/

BOOST_AUTO_TEST_CASE(test_cuckoocache_no_fakes)
{
    SeedInsecureRand(true);
    CuckooCache::cache<uint256, SignatureCacheHasher> cc{};
    size_t megabytes = 4;
    cc.setup_bytes(megabytes << 20);
    for (int x = 0; x < 100000; ++x) {
        cc.insert(InsecureRand256());
    }
    for (int x = 0; x < 100000; ++x) {
        BOOST_CHECK(!cc.contains(InsecureRand256(), false));
    }
};

/*此帮助程序返回当条目数为兆字节*时的命中率。
 *插入到兆字节大小的缓存中
 **/

template <typename Cache>
static double test_cache(size_t megabytes, double load)
{
    SeedInsecureRand(true);
    std::vector<uint256> hashes;
    Cache set{};
    size_t bytes = megabytes * (1 << 20);
    set.setup_bytes(bytes);
    uint32_t n_insert = static_cast<uint32_t>(load * (bytes / sizeof(uint256)));
    hashes.resize(n_insert);
    for (uint32_t i = 0; i < n_insert; ++i) {
        uint32_t* ptr = (uint32_t*)hashes[i].begin();
        for (uint8_t j = 0; j < 8; ++j)
            *(ptr++) = InsecureRand32();
    }
    /*我们复制散列，因为
     *布谷缓存可能会覆盖插入的元素，因此测试是
     *“未来证明”。
     **/

    std::vector<uint256> hashes_insert_copy = hashes;
    /*做插入*/
    for (const uint256& h : hashes_insert_copy)
        set.insert(h);
    /*统计命中率*/
    uint32_t count = 0;
    for (const uint256& h : hashes)
        count += set.contains(h, false);
    double hit_rate = ((double)count) / ((double)n_insert);
    return hit_rate;
}

/*给定负载的标准化命中率。
 *
 *语义有点混乱，请看下面
 ＊解释。
 *
 *实例：
 *
 *1）在加载0.5时，我们期望有一个完美的命中率，因此我们乘以
 * 1
 *2）在加载2.0时，我们希望看到一半的条目，所以命中率非常理想。
 *为0.5。因此，如果我们看到0.4的命中率，0.4*2.0=0.8是
 *标准化命中率。
 *
 *这基本上是正确的语义，但根据
 *如何测量负载1.0左右，正如负载1.0之后您的标准化命中率一样。
 *变得非常完美，忽略新鲜感。
 **/

static double normalize_hit_rate(double hits, double load)
{
    return hits * std::max(load, 1.0);
}

/*检查从0.1到1.6的负载的命中率*/
BOOST_AUTO_TEST_CASE(cuckoocache_hit_rate_ok)
{
    /*为该测试选择的任意命中率阈值
     *作为性能的下限。
     **/

    double HitRateThresh = 0.98;
    size_t megabytes = 4;
    for (double load = 0.1; load < 2; load *= 2) {
        double hits = test_cache<CuckooCache::cache<uint256, SignatureCacheHasher>>(megabytes, load);
        BOOST_CHECK(normalize_hit_rate(hits, load) > HitRateThresh);
    }
}


/*此帮助程序检查删除的元素是否优先插入和
 *刷新键的命中率是合理的。*/

template <typename Cache>
static void test_cache_erase(size_t megabytes)
{
    double load = 1;
    SeedInsecureRand(true);
    std::vector<uint256> hashes;
    Cache set{};
    size_t bytes = megabytes * (1 << 20);
    set.setup_bytes(bytes);
    uint32_t n_insert = static_cast<uint32_t>(load * (bytes / sizeof(uint256)));
    hashes.resize(n_insert);
    for (uint32_t i = 0; i < n_insert; ++i) {
        uint32_t* ptr = (uint32_t*)hashes[i].begin();
        for (uint8_t j = 0; j < 8; ++j)
            *(ptr++) = InsecureRand32();
    }
    /*我们复制散列，因为
     *布谷缓存可能会覆盖插入的元素，因此测试是
     *“未来证明”。
     **/

    std::vector<uint256> hashes_insert_copy = hashes;

    /*插入上半部分*/
    for (uint32_t i = 0; i < (n_insert / 2); ++i)
        set.insert(hashes_insert_copy[i]);
    /*删除第一个季度*/
    for (uint32_t i = 0; i < (n_insert / 4); ++i)
        BOOST_CHECK(set.contains(hashes[i], true));
    /*插入下半部分*/
    for (uint32_t i = (n_insert / 2); i < n_insert; ++i)
        set.insert(hashes_insert_copy[i]);

    /*我们标记为已删除但仍存在的元素*/
    size_t count_erased_but_contained = 0;
    /*我们没有删除但较旧的元素*/
    size_t count_stale = 0;
    /*最近插入的元素*/
    size_t count_fresh = 0;

    for (uint32_t i = 0; i < (n_insert / 4); ++i)
        count_erased_but_contained += set.contains(hashes[i], false);
    for (uint32_t i = (n_insert / 4); i < (n_insert / 2); ++i)
        count_stale += set.contains(hashes[i], false);
    for (uint32_t i = (n_insert / 2); i < n_insert; ++i)
        count_fresh += set.contains(hashes[i], false);

    double hit_rate_erased_but_contained = double(count_erased_but_contained) / (double(n_insert) / 4.0);
    double hit_rate_stale = double(count_stale) / (double(n_insert) / 4.0);
    double hit_rate_fresh = double(count_fresh) / (double(n_insert) / 2.0);

//检查我们的命中率是完美的
    BOOST_CHECK_EQUAL(hit_rate_fresh, 1.0);
//检查陈旧元素的命中率是否比
//删除的元素。
    BOOST_CHECK(hit_rate_stale > 2 * hit_rate_erased_but_contained);
}

BOOST_AUTO_TEST_CASE(cuckoocache_erase_ok)
{
    size_t megabytes = 4;
    test_cache_erase<CuckooCache::cache<uint256, SignatureCacheHasher>>(megabytes);
}

template <typename Cache>
static void test_cache_erase_parallel(size_t megabytes)
{
    double load = 1;
    SeedInsecureRand(true);
    std::vector<uint256> hashes;
    Cache set{};
    size_t bytes = megabytes * (1 << 20);
    set.setup_bytes(bytes);
    uint32_t n_insert = static_cast<uint32_t>(load * (bytes / sizeof(uint256)));
    hashes.resize(n_insert);
    for (uint32_t i = 0; i < n_insert; ++i) {
        uint32_t* ptr = (uint32_t*)hashes[i].begin();
        for (uint8_t j = 0; j < 8; ++j)
            *(ptr++) = InsecureRand32();
    }
    /*我们复制散列，因为
     *布谷缓存可能会覆盖插入的元素，因此测试是
     *“未来证明”。
     **/

    std::vector<uint256> hashes_insert_copy = hashes;
    boost::shared_mutex mtx;

    {
        /*抓住锁确保我们释放插入件*/
        boost::unique_lock<boost::shared_mutex> l(mtx);
        /*插入上半部分*/
        for (uint32_t i = 0; i < (n_insert / 2); ++i)
            set.insert(hashes_insert_copy[i]);
    }

    /*旋转3个线程运行包含擦除。
     **/

    std::vector<std::thread> threads;
    /*删除第一个季度*/
    for (uint32_t x = 0; x < 3; ++x)
        /*每个线程用x copy by value放置
        **/

        threads.emplace_back([&, x] {
            boost::shared_lock<boost::shared_mutex> l(mtx);
            size_t ntodo = (n_insert/4)/3;
            size_t start = ntodo*x;
            size_t end = ntodo*(x+1);
            for (uint32_t i = start; i < end; ++i) {
                bool contains = set.contains(hashes[i], true);
                assert(contains);
            }
        });

    /*等待所有线程完成
     **/

    for (std::thread& t : threads)
        t.join();
    /*抓紧锁，确保我们观察到擦除*/
    boost::unique_lock<boost::shared_mutex> l(mtx);
    /*插入下半部分*/
    for (uint32_t i = (n_insert / 2); i < n_insert; ++i)
        set.insert(hashes_insert_copy[i]);

    /*我们标记为已删除但仍存在的元素*/
    size_t count_erased_but_contained = 0;
    /*我们没有删除但较旧的元素*/
    size_t count_stale = 0;
    /*最近插入的元素*/
    size_t count_fresh = 0;

    for (uint32_t i = 0; i < (n_insert / 4); ++i)
        count_erased_but_contained += set.contains(hashes[i], false);
    for (uint32_t i = (n_insert / 4); i < (n_insert / 2); ++i)
        count_stale += set.contains(hashes[i], false);
    for (uint32_t i = (n_insert / 2); i < n_insert; ++i)
        count_fresh += set.contains(hashes[i], false);

    double hit_rate_erased_but_contained = double(count_erased_but_contained) / (double(n_insert) / 4.0);
    double hit_rate_stale = double(count_stale) / (double(n_insert) / 4.0);
    double hit_rate_fresh = double(count_fresh) / (double(n_insert) / 2.0);

//检查我们的命中率是完美的
    BOOST_CHECK_EQUAL(hit_rate_fresh, 1.0);
//检查陈旧元素的命中率是否比
//删除的元素。
    BOOST_CHECK(hit_rate_stale > 2 * hit_rate_erased_but_contained);
}
BOOST_AUTO_TEST_CASE(cuckoocache_erase_parallel_ok)
{
    size_t megabytes = 4;
    test_cache_erase_parallel<CuckooCache::cache<uint256, SignatureCacheHasher>>(megabytes);
}


template <typename Cache>
static void test_cache_generations()
{
//此测试检查网络活动模拟的新点击
//利率永远不会低于99%，而且比
//99.9%小于1%。
    double min_hit_rate = 0.99;
    double tight_hit_rate = 0.999;
    double max_rate_less_than_tight_hit_rate = 0.01;
//因此，满足此规范的缓存显示为命中
//至少紧密命中率*（1-最大命中率比紧密命中率低）
//最小命中率*最大命中率小于紧命中率=0.999*99%+0.99*1%==99.89%
//低方差命中率。

//我们使用确定性值，但是这个测试也传递了许多
//具有非确定性值的迭代，因此它不“过适合”于
//FastRandomContext中的特定熵（true）和
//隐藏物。
    SeedInsecureRand(true);

//block_activity为一块网络活动建模。插入元素为
//添加到缓存。第一个N/4和最后一个N/4将被存储以供以后删除。
//中间的n/2不存储。这模拟了一个使用半
//最近（自上一个块以来）添加的事务的签名
//立即使用另一半。
    struct block_activity {
        std::vector<uint256> reads;
        block_activity(uint32_t n_insert, Cache& c) : reads()
        {
            std::vector<uint256> inserts;
            inserts.resize(n_insert);
            reads.reserve(n_insert / 2);
            for (uint32_t i = 0; i < n_insert; ++i) {
                uint32_t* ptr = (uint32_t*)inserts[i].begin();
                for (uint8_t j = 0; j < 8; ++j)
                    *(ptr++) = InsecureRand32();
            }
            for (uint32_t i = 0; i < n_insert / 4; ++i)
                reads.push_back(inserts[i]);
            for (uint32_t i = n_insert - (n_insert / 4); i < n_insert; ++i)
                reads.push_back(inserts[i]);
            for (const auto& h : inserts)
                c.insert(h);
        }
    };

    const uint32_t BLOCK_SIZE = 1000;
//我们期望窗口大小60在每个时代都能合理地运行。
//存储缓存大小的45%（约472K）。
    const uint32_t WINDOW_SIZE = 60;
    const uint32_t POP_AMOUNT = (BLOCK_SIZE / WINDOW_SIZE) / 2;
    const double load = 10;
    const size_t megabytes = 4;
    const size_t bytes = megabytes * (1 << 20);
    const uint32_t n_insert = static_cast<uint32_t>(load * (bytes / sizeof(uint256)));

    std::vector<block_activity> hashes;
    Cache set{};
    set.setup_bytes(bytes);
    hashes.reserve(n_insert / BLOCK_SIZE);
    std::deque<block_activity> last_few;
    uint32_t out_of_tight_tolerance = 0;
    uint32_t total = n_insert / BLOCK_SIZE;
//我们用最后一个模型来模拟一个滑窗。在每个
//步骤，每个最后一个窗口的“大小块”活动检查缓存
//弹出他们插入的哈希数，并标记这些已删除。
    for (uint32_t i = 0; i < total; ++i) {
        if (last_few.size() == WINDOW_SIZE)
            last_few.pop_front();
        last_few.emplace_back(BLOCK_SIZE, set);
        uint32_t count = 0;
        for (auto& act : last_few)
            for (uint32_t k = 0; k < POP_AMOUNT; ++k) {
                count += set.contains(act.reads.back(), true);
                act.reads.pop_back();
            }
//我们使用最后的.size（）而不是窗口大小
//第一个窗口大小迭代的行为，其中deque不是
//满了。
        double hit = (double(count)) / (last_few.size() * POP_AMOUNT);
//检查命中率是否高于最小命中率
        BOOST_CHECK(hit > min_hit_rate);
//更严格的检查，计算我们低于严格命中率的次数
//（隐含地，大于最小命中率）
        out_of_tight_tolerance += hit < tight_hit_rate;
    }
//检查超差发生的时间是否小于
//最大命中率
    BOOST_CHECK(double(out_of_tight_tolerance) / double(total) < max_rate_less_than_tight_hit_rate);
}
BOOST_AUTO_TEST_CASE(cuckoocache_generations)
{
    test_cache_generations<CuckooCache::cache<uint256, SignatureCacheHasher>>();
}

BOOST_AUTO_TEST_SUITE_END();
