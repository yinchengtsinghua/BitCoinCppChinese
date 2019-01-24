
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2012-2017比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <sync.h>
#include <test/test_bitcoin.h>

#include <boost/test/unit_test.hpp>

namespace {
template <typename MutexType>
void TestPotentialDeadLockDetected(MutexType& mutex1, MutexType& mutex2)
{
    {
        LOCK2(mutex1, mutex2);
    }
    bool error_thrown = false;
    try {
        LOCK2(mutex2, mutex1);
    } catch (const std::logic_error& e) {
        BOOST_CHECK_EQUAL(e.what(), "potential deadlock detected");
        error_thrown = true;
    }
    #ifdef DEBUG_LOCKORDER
    BOOST_CHECK(error_thrown);
    #else
    BOOST_CHECK(!error_thrown);
    #endif
}
} //命名空间

BOOST_FIXTURE_TEST_SUITE(sync_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(potential_deadlock_detected)
{
    #ifdef DEBUG_LOCKORDER
    bool prev = g_debug_lockorder_abort;
    g_debug_lockorder_abort = false;
    #endif

    CCriticalSection rmutex1, rmutex2;
    TestPotentialDeadLockDetected(rmutex1, rmutex2);

    Mutex mutex1, mutex2;
    TestPotentialDeadLockDetected(mutex1, mutex2);

    #ifdef DEBUG_LOCKORDER
    g_debug_lockorder_abort = prev;
    #endif
}

BOOST_AUTO_TEST_SUITE_END()
