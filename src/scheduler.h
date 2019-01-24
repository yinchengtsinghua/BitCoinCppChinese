
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2015-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_SCHEDULER_H
#define BITCOIN_SCHEDULER_H

//
//注：
//boost:：thread/boost:：chrono应移植到std:：thread/std:：chrono
//当我们支持C++ 11时。
//
#include <boost/chrono/chrono.hpp>
#include <boost/thread.hpp>
#include <map>

#include <sync.h>

//
//用于应运行的后台任务的简单类
//定期或“过一段时间”一次
//
//用途：
//
//csscheduler*s=新csscheduler（）；
//s->scheduleFromNow（dosomething，11）；//假设a:void dosomething（）
//s->scheduleFromNow（std:：bind（class:：func，this，argument），3）；
//boost:：thread*t=new boost:：thread（std:：bind（cscheduler:：servicequeue，s））；
//
//…然后在程序关闭时，清理运行servicequeue的线程：
//T-中断（）；
//T-＞联接（）；
//删除T；
//delete s；//必须在线程中断/连接后执行。
//

class CScheduler
{
public:
    CScheduler();
    ~CScheduler();

    typedef std::function<void()> Function;

//在时间t/之后调用func
    void schedule(Function f, boost::chrono::system_clock::time_point t=boost::chrono::system_clock::now());

//方便方法：从现在起调用f一次deltaseconds
    void scheduleFromNow(Function f, int64_t deltaMilliSeconds);

//另一个方便方法：调用f about
//每一个德尔塔秒永远，从现在开始。
//更准确地说：每次f完成，它
//被重新安排为稍后运行deltaseconds。如果你
//需要更精确的调度，不要使用这种方法。
    void scheduleEvery(Function f, int64_t deltaMilliSeconds);

//为了使事情尽可能简单，没有不定期的安排。

//“永远”为队列服务。应该在线程中运行，
//并使用boost：：中断线程中断
    void serviceQueue();

//告诉运行servicequeue的任何线程一旦停止
//已完成服务当前正在服务的任何任务（drain=false）
//或者当没有工作要做时（排水=真）
    void stop(bool drain=false);

//返回等待服务的任务数，
//第一个和最后一个任务时间
    size_t getQueueInfo(boost::chrono::system_clock::time_point &first,
                        boost::chrono::system_clock::time_point &last) const;

//如果在ServiceQueue（）中有活动的线程，则返回true。
    bool AreThreadsServicingQueue() const;

private:
    std::multimap<boost::chrono::system_clock::time_point, Function> taskQueue;
    boost::condition_variable newTaskScheduled;
    mutable boost::mutex newTaskMutex;
    int nThreadsServicingQueue;
    bool stopRequested;
    bool stopWhenEmpty;
    bool shouldStop() const { return stopRequested || (stopWhenEmpty && taskQueue.empty()); }
};

/*
 *csscheduler客户端使用的类，该类可以调度多个作业
 *需要连续运行。作业不能在上运行
 *同一线程，但不会执行两个作业
 *同时释放内存获取一致
 *（调度程序将在调用回调之前在内部执行一个获取
 *以及最后的释放）。实际上，这意味着回调
 *b（）将能够观察回调a（）执行的所有效果。
 *在它之前。
 **/

class SingleThreadedSchedulerClient {
private:
    CScheduler *m_pscheduler;

    CCriticalSection m_cs_callbacks_pending;
    std::list<std::function<void ()>> m_callbacks_pending GUARDED_BY(m_cs_callbacks_pending);
    bool m_are_callbacks_running GUARDED_BY(m_cs_callbacks_pending) = false;

    void MaybeScheduleProcessQueue();
    void ProcessQueue();

public:
    explicit SingleThreadedSchedulerClient(CScheduler *pschedulerIn) : m_pscheduler(pschedulerIn) {}

    /*
     *添加要执行的回调。回调是连续执行的
     *内存为释放获取，在回调执行之间保持一致。
     *实际上，这意味着回调可以表现为执行回调。
     *按单线程顺序排列。
     **/

    void AddToProcessQueue(std::function<void ()> func);

//处理调用线程上的所有剩余队列成员，阻塞直到队列为空
//必须在csscheduler没有剩余的处理线程之后调用！
    void EmptyQueue();

    size_t CallbacksPending();
};

#endif
