
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

#include <scheduler.h>

#include <random.h>
#include <reverselock.h>

#include <assert.h>
#include <utility>

CScheduler::CScheduler() : nThreadsServicingQueue(0), stopRequested(false), stopWhenEmpty(false)
{
}

CScheduler::~CScheduler()
{
    assert(nThreadsServicingQueue == 0);
}


#if BOOST_VERSION < 105000
static boost::system_time toPosixTime(const boost::chrono::system_clock::time_point& t)
{
//使用“从时间”创建posix时间会损失次秒精度。所以与其把时间点导出到时间点，
//从epoch（0）的posix_时间开始，并添加从那时起经过的毫秒数。
    return boost::posix_time::from_time_t(0) + boost::posix_time::milliseconds(boost::chrono::duration_cast<boost::chrono::milliseconds>(t.time_since_epoch()).count());
}
#endif

void CScheduler::serviceQueue()
{
    boost::unique_lock<boost::mutex> lock(newTaskMutex);
    ++nThreadsServicingQueue;

//newtaskmutex在此循环中被锁定，除非
//当线程正在等待或当用户的函数
//被称为。
    while (!shouldStop()) {
        try {
            if (!shouldStop() && taskQueue.empty()) {
                reverse_lock<boost::unique_lock<boost::mutex> > rlock(lock);
//利用这个机会得到一点点的熵
                RandAddSeedSleep();
            }
            while (!shouldStop() && taskQueue.empty()) {
//等到有事情要做。
                newTaskScheduled.wait(lock);
            }

//等到有新任务，或者等到
//队列中第一个项目的时间：

//等待直到需要1.50或更高版本的Boost；旧版本已超时\u等待：
#if BOOST_VERSION < 105000
            while (!shouldStop() && !taskQueue.empty() &&
                   newTaskScheduled.timed_wait(lock, toPosixTime(taskQueue.begin()->first))) {
//一直等到超时
            }
#else
//一些增强版本有一个冲突的超负荷等待，直到返回空。
//在这里显式地使用一个模板以避免碰到那个重载。
            while (!shouldStop() && !taskQueue.empty()) {
                boost::chrono::system_clock::time_point timeToWaitFor = taskQueue.begin()->first;
                if (newTaskScheduled.wait_until<>(lock, timeToWaitFor) == boost::cv_status::timeout)
break; //超时后退出循环，这意味着我们到达了事件的时间
            }
#endif
//如果有多个线程，则队列可以在等待时清空（另一个线程
//线程可能服务于我们正在等待的任务）。
            if (shouldStop() || taskQueue.empty())
                continue;

            Function f = taskQueue.begin()->second;
            taskQueue.erase(taskQueue.begin());

            {
//在调用f之前解锁，以便它可以重新安排自己或其他任务
//无死锁：
                reverse_lock<boost::unique_lock<boost::mutex> > rlock(lock);
                f();
            }
        } catch (...) {
            --nThreadsServicingQueue;
            throw;
        }
    }
    --nThreadsServicingQueue;
    newTaskScheduled.notify_one();
}

void CScheduler::stop(bool drain)
{
    {
        boost::unique_lock<boost::mutex> lock(newTaskMutex);
        if (drain)
            stopWhenEmpty = true;
        else
            stopRequested = true;
    }
    newTaskScheduled.notify_all();
}

void CScheduler::schedule(CScheduler::Function f, boost::chrono::system_clock::time_point t)
{
    {
        boost::unique_lock<boost::mutex> lock(newTaskMutex);
        taskQueue.insert(std::make_pair(t, f));
    }
    newTaskScheduled.notify_one();
}

void CScheduler::scheduleFromNow(CScheduler::Function f, int64_t deltaMilliSeconds)
{
    schedule(f, boost::chrono::system_clock::now() + boost::chrono::milliseconds(deltaMilliSeconds));
}

static void Repeat(CScheduler* s, CScheduler::Function f, int64_t deltaMilliSeconds)
{
    f();
    s->scheduleFromNow(std::bind(&Repeat, s, f, deltaMilliSeconds), deltaMilliSeconds);
}

void CScheduler::scheduleEvery(CScheduler::Function f, int64_t deltaMilliSeconds)
{
    scheduleFromNow(std::bind(&Repeat, this, f, deltaMilliSeconds), deltaMilliSeconds);
}

size_t CScheduler::getQueueInfo(boost::chrono::system_clock::time_point &first,
                             boost::chrono::system_clock::time_point &last) const
{
    boost::unique_lock<boost::mutex> lock(newTaskMutex);
    size_t result = taskQueue.size();
    if (!taskQueue.empty()) {
        first = taskQueue.begin()->first;
        last = taskQueue.rbegin()->first;
    }
    return result;
}

bool CScheduler::AreThreadsServicingQueue() const {
    boost::unique_lock<boost::mutex> lock(newTaskMutex);
    return nThreadsServicingQueue;
}


void SingleThreadedSchedulerClient::MaybeScheduleProcessQueue() {
    {
        LOCK(m_cs_callbacks_pending);
//尽量避免在这里安排太多的副本，但是如果我们
//意外地同时调度了两个processqueue
//没什么大不了的。
        if (m_are_callbacks_running) return;
        if (m_callbacks_pending.empty()) return;
    }
    m_pscheduler->schedule(std::bind(&SingleThreadedSchedulerClient::ProcessQueue, this));
}

void SingleThreadedSchedulerClient::ProcessQueue() {
    std::function<void ()> callback;
    {
        LOCK(m_cs_callbacks_pending);
        if (m_are_callbacks_running) return;
        if (m_callbacks_pending.empty()) return;
        m_are_callbacks_running = true;

        callback = std::move(m_callbacks_pending.front());
        m_callbacks_pending.pop_front();
    }

//raii fcallbackrunning和调用maybescheduleProcessQueue的设置
//即使callback（）抛出，也要确保两者都安全发生。
    struct RAIICallbacksRunning {
        SingleThreadedSchedulerClient* instance;
        explicit RAIICallbacksRunning(SingleThreadedSchedulerClient* _instance) : instance(_instance) {}
        ~RAIICallbacksRunning() {
            {
                LOCK(instance->m_cs_callbacks_pending);
                instance->m_are_callbacks_running = false;
            }
            instance->MaybeScheduleProcessQueue();
        }
    } raiicallbacksrunning(this);

    callback();
}

void SingleThreadedSchedulerClient::AddToProcessQueue(std::function<void ()> func) {
    assert(m_pscheduler);

    {
        LOCK(m_cs_callbacks_pending);
        m_callbacks_pending.emplace_back(std::move(func));
    }
    MaybeScheduleProcessQueue();
}

void SingleThreadedSchedulerClient::EmptyQueue() {
    assert(!m_pscheduler->AreThreadsServicingQueue());
    bool should_continue = true;
    while (should_continue) {
        ProcessQueue();
        LOCK(m_cs_callbacks_pending);
        should_continue = !m_callbacks_pending.empty();
    }
}

size_t SingleThreadedSchedulerClient::CallbacksPending() {
    LOCK(m_cs_callbacks_pending);
    return m_callbacks_pending.size();
}
