
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

#ifndef BITCOIN_BENCH_BENCH_H
#define BITCOIN_BENCH_BENCH_H

#include <functional>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include <chrono>

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>

//简单的微基准测试框架；API主要匹配Google基准测试的一个子集
//框架（见https://github.com/google/benchmark）
//为什么不使用谷歌基准框架呢？因为添加了另一个依赖项
//（使用cmake作为其构建系统，并具有许多我们不需要的功能）不是
//值得的。

/*
 ＊用法：

静态空代码\到\时间（基准：：状态和状态）
{
    …是否需要任何设置…
    while（state.keeprunning（））
       …做你想做的事…
    }
    …需要清理吗…
}

//默认为运行5000次迭代的基准
基准（代码-时间，5000）；

 **/


namespace benchmark {
//如果高分辨率时钟是稳定的，最好是稳定的，否则使用稳定的时钟。
struct best_clock {
    using hi_res_clock = std::chrono::high_resolution_clock;
    using steady_clock = std::chrono::steady_clock;
    using type = std::conditional<hi_res_clock::is_steady, hi_res_clock, steady_clock>::type;
};
using clock = best_clock::type;
using time_point = clock::time_point;
using duration = clock::duration;

class Printer;

class State
{
public:
    std::string m_name;
    uint64_t m_num_iters_left;
    const uint64_t m_num_iters;
    const uint64_t m_num_evals;
    std::vector<double> m_elapsed_results;
    time_point m_start_time;

    bool UpdateTimer(time_point finish_time);

    State(std::string name, uint64_t num_evals, double num_iters, Printer& printer) : m_name(name), m_num_iters_left(0), m_num_iters(num_iters), m_num_evals(num_evals)
    {
    }

    inline bool KeepRunning()
    {
        if (m_num_iters_left--) {
            return true;
        }

        bool result = UpdateTimer(clock::now());
//再次测量，以便不包括UpdateTimer的运行时
        m_start_time = clock::now();
        return result;
    }
};

typedef std::function<void(State&)> BenchFunction;

class BenchRunner
{
    struct Bench {
        BenchFunction func;
        uint64_t num_iters_for_one_second;
    };
    typedef std::map<std::string, Bench> BenchmarkMap;
    static BenchmarkMap& benchmarks();

public:
    BenchRunner(std::string name, BenchFunction func, uint64_t num_iters_for_one_second);

    static void RunAll(Printer& printer, uint64_t num_evals, double scaling, const std::string& filter, bool is_list_only);
};

//输出基准结果的接口。
class Printer
{
public:
    virtual ~Printer() {}
    virtual void header() = 0;
    virtual void result(const State& state) = 0;
    virtual void footer() = 0;
};

//默认打印机到控制台，显示最小值、最大值和中间值。
class ConsolePrinter : public Printer
{
public:
    void header() override;
    void result(const State& state) override;
    void footer() override;
};

//使用plotly.js创建方框图
class PlotlyPrinter : public Printer
{
public:
    PlotlyPrinter(std::string plotly_url, int64_t width, int64_t height);
    void header() override;
    void result(const State& state) override;
    void footer() override;

private:
    std::string m_plotly_url;
    int64_t m_width;
    int64_t m_height;
};
}


//Benchmark（foo，num-iters_表示一秒钟）扩展到：Benchmark：：Benchrunner Benchmark_11foo（“foo”，num-iu迭代）；
//选择一个num iters_表示大约需要1秒钟的时间。目标是所有的基准都应该取大约
//同时，可以使用总时间适合您的系统的比例因子。
#define BENCHMARK(n, num_iters_for_one_second) \
    benchmark::BenchRunner BOOST_PP_CAT(bench_, BOOST_PP_CAT(__LINE__, n))(BOOST_PP_STRINGIZE(n), n, (num_iters_for_one_second));

#endif //比特币长凳
