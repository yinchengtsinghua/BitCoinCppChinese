
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

#ifndef BITCOIN_CHECKQUEUE_H
#define BITCOIN_CHECKQUEUE_H

#include <sync.h>

#include <algorithm>
#include <vector>

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>

template <typename T>
class CCheckQueueControl;

/*
 *必须执行的验证队列。
  *验证由T型表示，它必须提供
  *operator（），返回bool。
  *
  *假设一个线程（主线程）推送一批验证
  *进入队列，由n-1工作线程处理。什么时候？
  *主服务器完成添加工作，临时加入工作池
  *作为第n个工人，直到完成所有工作。
  **/

template <typename T>
class CCheckQueue
{
private:
//！互斥保护内部状态
    boost::mutex mutex;

//！当不工作时，工作线程在此上阻塞
    boost::condition_variable condWorker;

//！工作结束时，主线程在此上阻塞
    boost::condition_variable condMaster;

//！要处理的元素队列。
//！因为布尔值的顺序无关紧要，所以它被用作后进先出法（堆栈）
    std::vector<T> queue;

//！空闲的工人（包括主控）的数量。
    int nIdle;

//！工人总数（包括船长）。
    int nTotal;

//！临时评估结果。
    bool fAllOk;

    /*
     *尚未完成的验证数。
     *这包括不再排队但仍在
     *工人自己的批次。
     **/

    unsigned int nTodo;

//！一批处理的最大元素数
    unsigned int nBatchSize;

    /*执行大部分验证工作的内部函数。*/
    bool Loop(bool fMaster = false)
    {
        boost::condition_variable& cond = fMaster ? condMaster : condWorker;
        std::vector<T> vChecks;
        vChecks.reserve(nBatchSize);
        unsigned int nNow = 0;
        bool fOk = true;
        do {
            {
                boost::unique_lock<boost::mutex> lock(mutex);
//首先对前一个循环运行进行清理（允许我们在相同的critsect中进行清理）
                if (nNow) {
                    fAllOk &= fOk;
                    nTodo -= nNow;
                    if (nTodo == 0 && !fMaster)
//我们处理了最后一个元素；通知主元素它可以退出并返回结果。
                        condMaster.notify_one();
                } else {
//第一次迭代
                    nTotal++;
                }
//从逻辑上讲，do循环从这里开始
                while (queue.empty()) {
                    if (fMaster && nTodo == 0) {
                        nTotal--;
                        bool fRet = fAllOk;
//稍后重置新工作的状态
                        if (fMaster)
                            fAllOk = true;
//返回当前状态
                        return fRet;
                    }
                    nIdle++;
cond.wait(lock); //等待
                    nIdle--;
                }
//决定现在要处理多少工作单元。
//*不要试图一次完成所有的工作，而是瞄准越来越小的批次，因此
//所有工人几乎同时完成工作。
//*试着解释一下空闲的工作，它会立即开始起到帮助作用。
//*不要批量生产小于1（duh）或大于nbatchsize的产品。
                nNow = std::max(1U, std::min(nBatchSize, (unsigned int)queue.size() / (nTotal + nIdle + 1)));
                vChecks.resize(nNow);
                for (unsigned int i = 0; i < nNow; i++) {
//我们希望mutex上的锁尽可能短，以便从全局交换作业
//排队到本地批处理向量，而不是复制。
                    vChecks[i].swap(queue.back());
                    queue.pop_back();
                }
//检查我们是否需要工作
                fOk = fAllOk;
            }
//执行工作
            for (T& check : vChecks)
                if (fOk)
                    fOk = check();
            vChecks.clear();
        } while (true);
    }

public:
//！互斥，以确保只有一个并发CCheckQueueControl
    boost::mutex ControlMutex;

//！创建新的检查队列
    explicit CCheckQueue(unsigned int nBatchSizeIn) : nIdle(0), nTotal(0), fAllOk(true), nTodo(0), nBatchSize(nBatchSizeIn) {}

//！工作者线程
    void Thread()
    {
        Loop();
    }

//！等待执行完成，并返回所有计算是否成功。
    bool Wait()
    {
        return Loop(true);
    }

//！向队列中添加一批检查
    void Add(std::vector<T>& vChecks)
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        for (T& check : vChecks) {
            queue.push_back(T());
            check.swap(queue.back());
        }
        nTodo += vChecks.size();
        if (vChecks.size() == 1)
            condWorker.notify_one();
        else if (vChecks.size() > 1)
            condWorker.notify_all();
    }

    ~CCheckQueue()
    {
    }

};

/*
 *用于确保传递的
 *队列在继续之前已完成。
 **/

template <typename T>
class CCheckQueueControl
{
private:
    CCheckQueue<T> * const pqueue;
    bool fDone;

public:
    CCheckQueueControl() = delete;
    CCheckQueueControl(const CCheckQueueControl&) = delete;
    CCheckQueueControl& operator=(const CCheckQueueControl&) = delete;
    explicit CCheckQueueControl(CCheckQueue<T> * const pqueueIn) : pqueue(pqueueIn), fDone(false)
    {
//传递的队列应该是未使用的或nullptr
        if (pqueue != nullptr) {
            ENTER_CRITICAL_SECTION(pqueue->ControlMutex);
        }
    }

    bool Wait()
    {
        if (pqueue == nullptr)
            return true;
        bool fRet = pqueue->Wait();
        fDone = true;
        return fRet;
    }

    void Add(std::vector<T>& vChecks)
    {
        if (pqueue != nullptr)
            pqueue->Add(vChecks);
    }

    ~CCheckQueueControl()
    {
        if (!fDone)
            Wait();
        if (pqueue != nullptr) {
            LEAVE_CRITICAL_SECTION(pqueue->ControlMutex);
        }
    }
};

#endif //比特币支票队列
