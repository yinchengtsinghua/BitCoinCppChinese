
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
#include <util/time.h>
#include <validation.h>

#include <test/test_bitcoin.h>
#include <checkqueue.h>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

#include <unordered_set>
#include <memory>
#include <random.h>

//由于未设置nscriptCheckThreads，因此basictestingSetup不足
//否则。
BOOST_FIXTURE_TEST_SUITE(checkqueue_tests, TestingSetup)

static const unsigned int QUEUE_BATCH_SIZE = 128;

struct FakeCheck {
    bool operator()()
    {
        return true;
    }
    void swap(FakeCheck& x){};
};

struct FakeCheckCheckCompletion {
    static std::atomic<size_t> n_calls;
    bool operator()()
    {
        n_calls.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    void swap(FakeCheckCheckCompletion& x){};
};

struct FailingCheck {
    bool fails;
    FailingCheck(bool _fails) : fails(_fails){};
    FailingCheck() : fails(true){};
    bool operator()()
    {
        return !fails;
    }
    void swap(FailingCheck& x)
    {
        std::swap(fails, x.fails);
    };
};

struct UniqueCheck {
    static std::mutex m;
    static std::unordered_multiset<size_t> results;
    size_t check_id;
    UniqueCheck(size_t check_id_in) : check_id(check_id_in){};
    UniqueCheck() : check_id(0){};
    bool operator()()
    {
        std::lock_guard<std::mutex> l(m);
        results.insert(check_id);
        return true;
    }
    void swap(UniqueCheck& x) { std::swap(x.check_id, check_id); };
};


struct MemoryCheck {
    static std::atomic<size_t> fake_allocated_memory;
    bool b {false};
    bool operator()()
    {
        return true;
    }
    MemoryCheck(){};
    MemoryCheck(const MemoryCheck& x)
    {
//我们必须这样做以确保析构函数调用是成对的
//
//实际上，复制构造函数应该是可删除的，但CCheckQueue中断
//如果由于内部原因被删除，请将其推回。
        fake_allocated_memory.fetch_add(b, std::memory_order_relaxed);
    };
    MemoryCheck(bool b_) : b(b_)
    {
        fake_allocated_memory.fetch_add(b, std::memory_order_relaxed);
    };
    ~MemoryCheck()
    {
        fake_allocated_memory.fetch_sub(b, std::memory_order_relaxed);
    };
    void swap(MemoryCheck& x) { std::swap(b, x.b); };
};

struct FrozenCleanupCheck {
    static std::atomic<uint64_t> nFrozen;
    static std::condition_variable cv;
    static std::mutex m;
//冻结不能是给定队列的默认初始化行为
//交换默认的初始化检查。
    bool should_freeze {false};
    bool operator()()
    {
        return true;
    }
    FrozenCleanupCheck() {}
    ~FrozenCleanupCheck()
    {
        if (should_freeze) {
            std::unique_lock<std::mutex> l(m);
            nFrozen.store(1, std::memory_order_relaxed);
            cv.notify_one();
            cv.wait(l, []{ return nFrozen.load(std::memory_order_relaxed) == 0;});
        }
    }
    void swap(FrozenCleanupCheck& x){std::swap(should_freeze, x.should_freeze);};
};

//静态分配
std::mutex FrozenCleanupCheck::m{};
std::atomic<uint64_t> FrozenCleanupCheck::nFrozen{0};
std::condition_variable FrozenCleanupCheck::cv{};
std::mutex UniqueCheck::m;
std::unordered_multiset<size_t> UniqueCheck::results;
std::atomic<size_t> FakeCheckCheckCompletion::n_calls{0};
std::atomic<size_t> MemoryCheck::fake_allocated_memory{0};

//队列类型
typedef CCheckQueue<FakeCheckCheckCompletion> Correct_Queue;
typedef CCheckQueue<FakeCheck> Standard_Queue;
typedef CCheckQueue<FailingCheck> Failing_Queue;
typedef CCheckQueue<UniqueCheck> Unique_Queue;
typedef CCheckQueue<MemoryCheck> Memory_Queue;
typedef CCheckQueue<FrozenCleanupCheck> FrozenCleanup_Queue;


/*此测试用例检查CCheckQueue是否正常工作
 *推送每个指定尺寸的检查。
 **/

static void Correct_Queue_range(std::vector<size_t> range)
{
    auto small_queue = MakeUnique<Correct_Queue>(QUEUE_BATCH_SIZE);
    boost::thread_group tg;
    for (auto x = 0; x < nScriptCheckThreads; ++x) {
       tg.create_thread([&]{small_queue->Thread();});
    }
//在此进行vcheck以保存在malloc上（此测试可能很慢…）
    std::vector<FakeCheckCheckCompletion> vChecks;
    for (const size_t i : range) {
        size_t total = i;
        FakeCheckCheckCompletion::n_calls = 0;
        CCheckQueueControl<FakeCheckCheckCompletion> control(small_queue.get());
        while (total) {
            vChecks.resize(std::min(total, (size_t) InsecureRandRange(10)));
            total -= vChecks.size();
            control.Add(vChecks);
        }
        BOOST_REQUIRE(control.Wait());
        if (FakeCheckCheckCompletion::n_calls != i) {
            BOOST_REQUIRE_EQUAL(FakeCheckCheckCompletion::n_calls, i);
            BOOST_TEST_MESSAGE("Failure on trial " << i << " expected, got " << FakeCheckCheckCompletion::n_calls);
        }
    }
    tg.interrupt_all();
    tg.join_all();
}

/*测试0检查是否正确
 **/

BOOST_AUTO_TEST_CASE(test_CheckQueue_Correct_Zero)
{
    std::vector<size_t> range;
    range.push_back((size_t)0);
    Correct_Queue_range(range);
}
/*测试1检查是否正确
 **/

BOOST_AUTO_TEST_CASE(test_CheckQueue_Correct_One)
{
    std::vector<size_t> range;
    range.push_back((size_t)1);
    Correct_Queue_range(range);
}
/*测试最大检查是否正确
 **/

BOOST_AUTO_TEST_CASE(test_CheckQueue_Correct_Max)
{
    std::vector<size_t> range;
    range.push_back(100000);
    Correct_Queue_range(range);
}
/*检查随机数是否正确
 **/

BOOST_AUTO_TEST_CASE(test_CheckQueue_Correct_Random)
{
    std::vector<size_t> range;
    range.reserve(100000/1000);
    for (size_t i = 2; i < 100000; i += std::max((size_t)1, (size_t)InsecureRandRange(std::min((size_t)1000, ((size_t)100000) - i))))
        range.push_back(i);
    Correct_Queue_range(range);
}


/*测试是否捕获未通过的检查*/
BOOST_AUTO_TEST_CASE(test_CheckQueue_Catches_Failure)
{
    auto fail_queue = MakeUnique<Failing_Queue>(QUEUE_BATCH_SIZE);

    boost::thread_group tg;
    for (auto x = 0; x < nScriptCheckThreads; ++x) {
       tg.create_thread([&]{fail_queue->Thread();});
    }

    for (size_t i = 0; i < 1001; ++i) {
        CCheckQueueControl<FailingCheck> control(fail_queue.get());
        size_t remaining = i;
        while (remaining) {
            size_t r = InsecureRandRange(10);

            std::vector<FailingCheck> vChecks;
            vChecks.reserve(r);
            for (size_t k = 0; k < r && remaining; k++, remaining--)
                vChecks.emplace_back(remaining == 1);
            control.Add(vChecks);
        }
        bool success = control.Wait();
        if (i > 0) {
            BOOST_REQUIRE(!success);
        } else if (i == 0) {
            BOOST_REQUIRE(success);
        }
    }
    tg.interrupt_all();
    tg.join_all();
}
//测试未通过的块验证是否不会干扰
//将来的块，即清除坏状态。
BOOST_AUTO_TEST_CASE(test_CheckQueue_Recovers_From_Failure)
{
    auto fail_queue = MakeUnique<Failing_Queue>(QUEUE_BATCH_SIZE);
    boost::thread_group tg;
    for (auto x = 0; x < nScriptCheckThreads; ++x) {
       tg.create_thread([&]{fail_queue->Thread();});
    }

    for (auto times = 0; times < 10; ++times) {
        for (const bool end_fails : {true, false}) {
            CCheckQueueControl<FailingCheck> control(fail_queue.get());
            {
                std::vector<FailingCheck> vChecks;
                vChecks.resize(100, false);
                vChecks[99] = end_fails;
                control.Add(vChecks);
            }
            bool r =control.Wait();
            BOOST_REQUIRE(r != end_fails);
        }
    }
    tg.interrupt_all();
    tg.join_all();
}

//测试唯一检查实际上都是单独调用的，而不是
//只需重复调用一个检查。不调用检查的测试
//不止一次
BOOST_AUTO_TEST_CASE(test_CheckQueue_UniqueCheck)
{
    auto queue = MakeUnique<Unique_Queue>(QUEUE_BATCH_SIZE);
    boost::thread_group tg;
    for (auto x = 0; x < nScriptCheckThreads; ++x) {
       tg.create_thread([&]{queue->Thread();});

    }

    size_t COUNT = 100000;
    size_t total = COUNT;
    {
        CCheckQueueControl<UniqueCheck> control(queue.get());
        while (total) {
            size_t r = InsecureRandRange(10);
            std::vector<UniqueCheck> vChecks;
            for (size_t k = 0; k < r && total; k++)
                vChecks.emplace_back(--total);
            control.Add(vChecks);
        }
    }
    bool r = true;
    BOOST_REQUIRE_EQUAL(UniqueCheck::results.size(), COUNT);
    for (size_t i = 0; i < COUNT; ++i)
        r = r && UniqueCheck::results.count(i) == 1;
    BOOST_REQUIRE(r);
    tg.interrupt_all();
    tg.join_all();
}


//测试那些可能大量释放内存的块。
//
//这个测试试图抓住一个病理病例，通过懒散地释放
//支票可能意味着保留未交换的支票，每个支票减少1
//时间可能会使数据挂在一系列块上。
BOOST_AUTO_TEST_CASE(test_CheckQueue_Memory)
{
    auto queue = MakeUnique<Memory_Queue>(QUEUE_BATCH_SIZE);
    boost::thread_group tg;
    for (auto x = 0; x < nScriptCheckThreads; ++x) {
       tg.create_thread([&]{queue->Thread();});
    }
    for (size_t i = 0; i < 1000; ++i) {
        size_t total = i;
        {
            CCheckQueueControl<MemoryCheck> control(queue.get());
            while (total) {
                size_t r = InsecureRandRange(10);
                std::vector<MemoryCheck> vChecks;
                for (size_t k = 0; k < r && total; k++) {
                    total--;
//每次迭代都会在前面、后面和中间留下数据
//捕获任何类型的释放失败
                    vChecks.emplace_back(total == 0 || total == i || total == i/2);
                }
                control.Add(vChecks);
            }
        }
        BOOST_REQUIRE_EQUAL(MemoryCheck::fake_allocated_memory, 0U);
    }
    tg.interrupt_all();
    tg.join_all();
}

//测试在所有检查之前无法进行新的验证
//已经被摧毁了
BOOST_AUTO_TEST_CASE(test_CheckQueue_FrozenCleanup)
{
    auto queue = MakeUnique<FrozenCleanup_Queue>(QUEUE_BATCH_SIZE);
    boost::thread_group tg;
    bool fails = false;
    for (auto x = 0; x < nScriptCheckThreads; ++x) {
        tg.create_thread([&]{queue->Thread();});
    }
    std::thread t0([&]() {
        CCheckQueueControl<FrozenCleanupCheck> control(queue.get());
        std::vector<FrozenCleanupCheck> vChecks(1);
//冻结不能是给定队列的默认初始化行为
//交换默认的初始化检查（否则冻结析构函数
//会被叫两次）。
        vChecks[0].should_freeze = true;
        control.Add(vChecks);
bool waitResult = control.Wait(); //挂在这里
        assert(waitResult);
    });
    {
        std::unique_lock<std::mutex> l(FrozenCleanupCheck::m);
//等待队列完成所有作业并冻结
        FrozenCleanupCheck::cv.wait(l, [](){return FrozenCleanupCheck::nFrozen == 1;});
    }
//尝试多次控制队列
    for (auto x = 0; x < 100 && !fails; ++x) {
        fails = queue->ControlMutex.try_lock();
    }
    {
//解除冻结（我们需要锁定n个虚假唤醒的情况）
        std::unique_lock<std::mutex> l(FrozenCleanupCheck::m);
        FrozenCleanupCheck::nFrozen = 0;
    }
//唤醒冻结的毁灭者
    FrozenCleanupCheck::cv.notify_one();
//等待控制完成
    t0.join();
    tg.interrupt_all();
    tg.join_all();
    BOOST_REQUIRE(!fails);
}


/**/
BOOST_AUTO_TEST_CASE(test_CheckQueueControl_Locks)
{
    auto queue = MakeUnique<Standard_Queue>(QUEUE_BATCH_SIZE);
    {
        boost::thread_group tg;
        std::atomic<int> nThreads {0};
        std::atomic<int> fails {0};
        for (size_t i = 0; i < 3; ++i) {
            tg.create_thread(
                    [&]{
                    CCheckQueueControl<FakeCheck> control(queue.get());
//
                    auto observed = ++nThreads;
                    MilliSleep(10);
                    fails += observed  != nThreads;
                    });
        }
        tg.join_all();
        BOOST_REQUIRE_EQUAL(fails, 0);
    }
    {
        boost::thread_group tg;
        std::mutex m;
        std::condition_variable cv;
        bool has_lock{false};
        bool has_tried{false};
        bool done{false};
        bool done_ack{false};
        {
            std::unique_lock<std::mutex> l(m);
            tg.create_thread([&]{
                    CCheckQueueControl<FakeCheck> control(queue.get());
                    std::unique_lock<std::mutex> ll(m);
                    has_lock = true;
                    cv.notify_one();
                    cv.wait(ll, [&]{return has_tried;});
                    done = true;
                    cv.notify_one();
//
//
                    cv.wait(ll, [&]{return done_ack;});
                    });
//等待线程获取锁
            cv.wait(l, [&](){return has_lock;});
            bool fails = false;
            for (auto x = 0; x < 100 && !fails; ++x) {
                fails = queue->ControlMutex.try_lock();
            }
            has_tried = true;
            cv.notify_one();
            cv.wait(l, [&](){return done;});
//确认完成
            done_ack = true;
            cv.notify_one();
            BOOST_REQUIRE(!fails);
        }
        tg.join_all();
    }
}
BOOST_AUTO_TEST_SUITE_END()

