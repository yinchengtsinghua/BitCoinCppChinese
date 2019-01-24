
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

#include <random.h>
#include <scheduler.h>

#include <test/test_bitcoin.h>

#include <boost/thread.hpp>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(scheduler_tests)

static void microTask(CScheduler& s, boost::mutex& mutex, int& counter, int delta, boost::chrono::system_clock::time_point rescheduleTime)
{
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        counter += delta;
    }
    boost::chrono::system_clock::time_point noTime = boost::chrono::system_clock::time_point::min();
    if (rescheduleTime != noTime) {
        CScheduler::Function f = std::bind(&microTask, std::ref(s), std::ref(mutex), std::ref(counter), -delta + 1, noTime);
        s.schedule(f, rescheduleTime);
    }
}

static void MicroSleep(uint64_t n)
{
#if defined(HAVE_WORKING_BOOST_SLEEP_FOR)
    boost::this_thread::sleep_for(boost::chrono::microseconds(n));
#elif defined(HAVE_WORKING_BOOST_SLEEP)
    boost::this_thread::sleep(boost::posix_time::microseconds(n));
#else
//不应该到这里
    #error missing boost sleep implementation
#endif
}

BOOST_AUTO_TEST_CASE(manythreads)
{
//压力测试：数百微秒的预定任务，
//用10个螺纹进行维修。
//
//所以…十个共享计数器，如果所有任务都执行
//正确地将与完成的任务数相加。
//每个任务都从
//计数器，然后调度另一个任务0-1000
//将来要从中进行减法或加法的微秒数
//计数器-随机数+1，所以最终共享
//计数器应与执行的初始任务数相加。
    CScheduler microTasks;

    boost::mutex counterMutex[10];
    int counter[10] = { 0 };
    /*trandomcontext rng/*fdeterministic*/true
    auto zerotonine=[]（fastRandomContext&rc）->int返回rc.randRange（10）；；//[0，9]
    auto-randomsec=[]（fastrandomcontext&rc）->int return-11+（int）rc.randrange（1012）；/[-11，1000]
    auto randomdata=[]（fastrandomcontext&rc）->int return-1000+（int）rc.randrange（2001）；//[-1000，1000]

    boost:：chrono:：system_clock:：time_point start=boost:：chrono:：system_clock:：now（）；
    boost:：chrono:：system_clock:：time_point now=开始；
    Boost:：Chrono:：System_Clock:：Time_Point First，Last；
    size_t ntasks=microtasks.getqueueinfo（第一个，最后一个）；
    增压检查（ntasks==0）；

    对于（int i=0；i<100；+i）
        boost:：chrono:：system_clock:：time_point t=now+boost:：chrono:：microseconds（randomsec（rng））；
        boost:：chrono:：system_clock:：time_point treschedule=now+boost:：chrono:：microseconds（500+randomsec（rng））；
        int whichcounter=zerotonine（rng）；
        cscheduler:：function f=std:：bind（&microtask，std:：ref（microtasks）），
                                             std:：ref（countermutex[whichcounter]），std:：ref（counter[whichcounter]），
                                             随机三角洲（RNG），排程表；
        微任务.时间表（f，t）；
    }
    ntasks=microtasks.getqueueinfo（first，last）；
    增压检查（ntasks==100）；
    增强检查（第一次<最后一次）；
    增强检查（最近>现在）；

    //一旦创建了这些，它们将开始运行并为队列提供服务
    boost：：线程组微线程；
    对于（int i=0；i<5；i++）
        创建线程（std:：bind（&cscheduler:：servicequeue，&microtasks））；

    微尺度（600）；
    now=boost:：chrono:：system_clock:：now（）；

    //更多线程和更多任务：
    对于（int i=0；i<5；i++）
        创建线程（std:：bind（&cscheduler:：servicequeue，&microtasks））；
    对于（int i=0；i<100；i++）
        boost:：chrono:：system_clock:：time_point t=now+boost:：chrono:：microseconds（randomsec（rng））；
        boost:：chrono:：system_clock:：time_point treschedule=now+boost:：chrono:：microseconds（500+randomsec（rng））；
        int whichcounter=zerotonine（rng）；
        cscheduler:：function f=std:：bind（&microtask，std:：ref（microtasks）），
                                             std:：ref（countermutex[whichcounter]），std:：ref（counter[whichcounter]），
                                             随机三角洲（RNG），排程表；
        微任务.时间表（f，t）；
    }

    //清空任务队列，然后退出线程
    微任务。停止（真）；
    微线程。join_all（）；/…等待所有线程完成

    int countersum=0；
    对于（int i=0；i<10；i++）
        增压检查（计数器[I]！＝0）；
        countersum+=计数器[i]；
    }
    boost_check_equal（countersum，200）；
}

Boost_Auto_Test_案例（单线程调度程序已订购）
{
    CScheduler调度；

    //每个队列都应该按照自身而不是其他队列的顺序排列
    单线程调度客户端队列1（&scheduler）；
    单线程调度客户端队列2（&scheduler）；

    //创建的线程多于队列
    //如果队列只允许一次执行一个任务，则
    //额外的线程实际上应该什么都不做
    //如果他们不这样做，我们就会失去秩序
    boost：：线程组线程；
    对于（int i=0；i<5；+i）
        创建线程（std:：bind（&cscheduler:：servicequeue，&scheduler））；
    }

    //如果singleThreadSchedulerClient阻止
    //在队列级别并行执行此处不需要同步
    int counter1=0；
    int counter2=0；

    //只需对每个队列进行计数-如果执行顺序正确，则
    //回调的运行顺序应与它们排队的顺序完全相同
    对于（int i=0；i<100；+i）
        队列1.addToProcessQueue（[i，&counter1]（）
            布尔期望值=i==counter1++；
            断言（期望）；
        （}）；

        队列2.addToProcessQueue（[i，&counter2]（）
            布尔期望值=i==counter2++；
            断言（期望）；
        （}）；
    }

    /完成
    scheduler.stop（真）；
    threads.join_all（）；

    boost_check_equal（计数器1，100）；
    boost_check_equal（计数器2100）；
}

Boost_Auto_测试套件_end（）
