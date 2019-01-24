
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

#include <util/system.h>

#include <support/allocators/secure.h>
#include <test/test_bitcoin.h>

#include <memory>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(allocator_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(arena_tests)
{
//用于测试的假内存基址
//不需要实际使用内存。
    void *synth_base = reinterpret_cast<void*>(0x08000000);
    const size_t synth_size = 1024*1024;
    Arena b(synth_base, synth_size, 16);
    void *chunk = b.alloc(1000);
#ifdef ARENA_DEBUG
    b.walk();
#endif
    BOOST_CHECK(chunk != nullptr);
BOOST_CHECK(b.stats().used == 1008); //对齐到16
BOOST_CHECK(b.stats().total == synth_size); //什么都没有消失？
    b.free(chunk);
#ifdef ARENA_DEBUG
    b.walk();
#endif
    BOOST_CHECK(b.stats().used == 0);
    BOOST_CHECK(b.stats().free == synth_size);
try { //双自由测试异常
        b.free(chunk);
        BOOST_CHECK(0);
    } catch(std::runtime_error &)
    {
    }

    void *a0 = b.alloc(128);
    void *a1 = b.alloc(256);
    void *a2 = b.alloc(512);
    BOOST_CHECK(b.stats().used == 896);
    BOOST_CHECK(b.stats().total == synth_size);
#ifdef ARENA_DEBUG
    b.walk();
#endif
    b.free(a0);
#ifdef ARENA_DEBUG
    b.walk();
#endif
    BOOST_CHECK(b.stats().used == 768);
    b.free(a1);
    BOOST_CHECK(b.stats().used == 512);
    void *a3 = b.alloc(128);
#ifdef ARENA_DEBUG
    b.walk();
#endif
    BOOST_CHECK(b.stats().used == 640);
    b.free(a2);
    BOOST_CHECK(b.stats().used == 128);
    b.free(a3);
    BOOST_CHECK(b.stats().used == 0);
    BOOST_CHECK_EQUAL(b.stats().chunks_used, 0U);
    BOOST_CHECK(b.stats().total == synth_size);
    BOOST_CHECK(b.stats().free == synth_size);
    BOOST_CHECK_EQUAL(b.stats().chunks_free, 1U);

    std::vector<void*> addr;
BOOST_CHECK(b.alloc(0) == nullptr); //分配0总是返回nullptr
#ifdef ARENA_DEBUG
    b.walk();
#endif
//清除分配所有内存
    for (int x=0; x<1024; ++x)
        addr.push_back(b.alloc(1024));
    BOOST_CHECK(b.stats().free == 0);
BOOST_CHECK(b.alloc(1024) == nullptr); //内存已满，必须返回nullptr
    BOOST_CHECK(b.alloc(0) == nullptr);
    for (int x=0; x<1024; ++x)
        b.free(addr[x]);
    addr.clear();
    BOOST_CHECK(b.stats().total == synth_size);
    BOOST_CHECK(b.stats().free == synth_size);

//现在换个方向…
    for (int x=0; x<1024; ++x)
        addr.push_back(b.alloc(1024));
    for (int x=0; x<1024; ++x)
        b.free(addr[1023-x]);
    addr.clear();

//现在以较小的不相等块分配，然后随意地取消分配
//不是所有的块都能成功分配，但是释放nullptr是
//允许，所以没问题。
    for (int x=0; x<2048; ++x)
        addr.push_back(b.alloc(x+1));
    for (int x=0; x<2048; ++x)
        b.free(addr[((x*23)%2048)^242]);
    addr.clear();

//完全疯狂：自由和分配交错，
//使用伪随机性生成目标和大小。
    for (int x=0; x<2048; ++x)
        addr.push_back(nullptr);
    uint32_t s = 0x12345678;
    for (int x=0; x<5000; ++x) {
        int idx = s & (addr.size()-1);
        if (s & 0x80000000) {
            b.free(addr[idx]);
            addr[idx] = nullptr;
        } else if(!addr[idx]) {
            addr[idx] = b.alloc((s >> 16) & 2047);
        }
        bool lsb = s & 1;
        s >>= 1;
        if (lsb)
s ^= 0xf00f00f0; //LFSR周期0xF7FFFE0
    }
    for (void *ptr: addr)
        b.free(ptr);
    addr.clear();

    BOOST_CHECK(b.stats().total == synth_size);
    BOOST_CHECK(b.stats().free == synth_size);
}

/*用于测试的模拟锁定页面分配器*/
class TestLockedPageAllocator: public LockedPageAllocator
{
public:
    TestLockedPageAllocator(int count_in, int lockedcount_in): count(count_in), lockedcount(lockedcount_in) {}
    void* AllocateLocked(size_t len, bool *lockingSuccess) override
    {
        *lockingSuccess = false;
        if (count > 0) {
            --count;

            if (lockedcount > 0) {
                --lockedcount;
                *lockingSuccess = true;
            }

return reinterpret_cast<void*>(uint64_t{static_cast<uint64_t>(0x08000000) + (count << 24)}); //假地址，不要使用这个内存
        }
        return nullptr;
    }
    void FreeLocked(void* addr, size_t len) override
    {
    }
    size_t GetLimit() override
    {
        return std::numeric_limits<size_t>::max();
    }
private:
    int count;
    int lockedcount;
};

BOOST_AUTO_TEST_CASE(lockedpool_tests_mock)
{
//测试三个虚拟竞技场，其中一个将成功锁定
    std::unique_ptr<LockedPageAllocator> x = MakeUnique<TestLockedPageAllocator>(3, 1);
    LockedPool pool(std::move(x));
    BOOST_CHECK(pool.stats().total == 0);
    BOOST_CHECK(pool.stats().locked == 0);

//确保在不分配任何内容的情况下拒绝不合理的请求
    void *invalid_toosmall = pool.alloc(0);
    BOOST_CHECK(invalid_toosmall == nullptr);
    BOOST_CHECK(pool.stats().used == 0);
    BOOST_CHECK(pool.stats().free == 0);
    void *invalid_toobig = pool.alloc(LockedPool::ARENA_SIZE+1);
    BOOST_CHECK(invalid_toobig == nullptr);
    BOOST_CHECK(pool.stats().used == 0);
    BOOST_CHECK(pool.stats().free == 0);

    void *a0 = pool.alloc(LockedPool::ARENA_SIZE / 2);
    BOOST_CHECK(a0);
    BOOST_CHECK(pool.stats().locked == LockedPool::ARENA_SIZE);
    void *a1 = pool.alloc(LockedPool::ARENA_SIZE / 2);
    BOOST_CHECK(a1);
    void *a2 = pool.alloc(LockedPool::ARENA_SIZE / 2);
    BOOST_CHECK(a2);
    void *a3 = pool.alloc(LockedPool::ARENA_SIZE / 2);
    BOOST_CHECK(a3);
    void *a4 = pool.alloc(LockedPool::ARENA_SIZE / 2);
    BOOST_CHECK(a4);
    void *a5 = pool.alloc(LockedPool::ARENA_SIZE / 2);
    BOOST_CHECK(a5);
//我们已经通过了三个竞技场，所以这个分配应该失败。
    void *a6 = pool.alloc(16);
    BOOST_CHECK(!a6);

    pool.free(a0);
    pool.free(a2);
    pool.free(a4);
    pool.free(a1);
    pool.free(a3);
    pool.free(a5);
    BOOST_CHECK(pool.stats().total == 3*LockedPool::ARENA_SIZE);
    BOOST_CHECK(pool.stats().locked == LockedPool::ARENA_SIZE);
    BOOST_CHECK(pool.stats().used == 0);
}

//这些测试使用了Live LockedPoolManager对象，也使用了
//通过其他测试，使得条件的可控性有所降低，因此
//测试更容易出错。
BOOST_AUTO_TEST_CASE(lockedpool_tests_live)
{
    LockedPoolManager &pool = LockedPoolManager::Instance();
    LockedPool::Stats initial = pool.stats();

    void *a0 = pool.alloc(16);
    BOOST_CHECK(a0);
//测试读写分配的内存
    *((uint32_t*)a0) = 0x1234;
    BOOST_CHECK(*((uint32_t*)a0) == 0x1234);

    pool.free(a0);
try { //双自由测试异常
        pool.free(a0);
        BOOST_CHECK(0);
    } catch(std::runtime_error &)
    {
    }
//如果为上述测试分配了多个新的竞技场，则会出现问题。
    BOOST_CHECK(pool.stats().total <= (initial.total + LockedPool::ARENA_SIZE));
//使用必须回到开始的位置
    BOOST_CHECK(pool.stats().used == initial.used);
}

BOOST_AUTO_TEST_SUITE_END()
