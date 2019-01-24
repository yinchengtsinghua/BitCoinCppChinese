
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

#include <bench/bench.h>
#include <util/system.h>
#include <validation.h>
#include <checkqueue.h>
#include <prevector.h>
#include <vector>
#include <boost/thread/thread.hpp>
#include <random.h>


static const int MIN_CORES = 2;
static const size_t BATCHES = 101;
static const size_t BATCH_SIZE = 30;
static const int PREVECTOR_SIZE = 28;
static const unsigned int QUEUE_BATCH_SIZE = 128;

//这个基准测试检查队列的工作负载稍微实际一些，
//如果所有检查都包含间接50%时间的预检者
//而且在两个要添加的调用之间还有一些工作要做。
static void CCheckQueueSpeedPrevectorJob(benchmark::State& state)
{
    struct PrevectorJob {
        prevector<PREVECTOR_SIZE, uint8_t> p;
        PrevectorJob(){
        }
        explicit PrevectorJob(FastRandomContext& insecure_rand){
            p.resize(insecure_rand.randrange(PREVECTOR_SIZE*2));
        }
        bool operator()()
        {
            return true;
        }
        void swap(PrevectorJob& x){p.swap(x.p);};
    };
    CCheckQueue<PrevectorJob> queue {QUEUE_BATCH_SIZE};
    boost::thread_group tg;
    for (auto x = 0; x < std::max(MIN_CORES, GetNumCores()); ++x) {
       tg.create_thread([&]{queue.Thread();});
    }
    while (state.KeepRunning()) {
//在这里做不安全的标记，这样每个迭代都是相同的。
        FastRandomContext insecure_rand(true);
        CCheckQueueControl<PrevectorJob> control(&queue);
        std::vector<std::vector<PrevectorJob>> vBatches(BATCHES);
        for (auto& vChecks : vBatches) {
            vChecks.reserve(BATCH_SIZE);
            for (size_t x = 0; x < BATCH_SIZE; ++x)
                vChecks.emplace_back(insecure_rand);
            control.Add(vChecks);
        }
//控制等待RAII完成，但是
//为了清楚起见，这里做得很明确
        control.Wait();
    }
    tg.interrupt_all();
    tg.join_all();
}
BENCHMARK(CCheckQueueSpeedPrevectorJob, 1400);
