
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

#include <crypto/sha256.h>
#include <key.h>
#include <random.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <validation.h>

#include <memory>

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

static const int64_t DEFAULT_BENCH_EVALUATIONS = 5;
static const char* DEFAULT_BENCH_FILTER = ".*";
static const char* DEFAULT_BENCH_SCALING = "1.0";
static const char* DEFAULT_BENCH_PRINTER = "console";
static const char* DEFAULT_PLOT_PLOTLYURL = "https://cdn.plot.ly/plot ly最新.min.js”；
static const int64_t DEFAULT_PLOT_WIDTH = 1024;
static const int64_t DEFAULT_PLOT_HEIGHT = 768;

static void SetupBenchArgs()
{
    gArgs.AddArg("-?", "Print this help message and exit", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-list", "List benchmarks without executing them. Can be combined with -scaling and -filter", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-evals=<n>", strprintf("Number of measurement evaluations to perform. (default: %u)", DEFAULT_BENCH_EVALUATIONS), false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-filter=<regex>", strprintf("Regular expression filter to select benchmark by name (default: %s)", DEFAULT_BENCH_FILTER), false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-scaling=<n>", strprintf("Scaling factor for benchmark's runtime (default: %u)", DEFAULT_BENCH_SCALING), false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-printer=(console|plot)", strprintf("Choose printer format. console: print data to console. plot: Print results as HTML graph (default: %s)", DEFAULT_BENCH_PRINTER), false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-plot-plotlyurl=<uri>", strprintf("URL to use for plotly.js (default: %s)", DEFAULT_PLOT_PLOTLYURL), false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-plot-width=<x>", strprintf("Plot width in pixel (default: %u)", DEFAULT_PLOT_WIDTH), false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-plot-height=<x>", strprintf("Plot height in pixel (default: %u)", DEFAULT_PLOT_HEIGHT), false, OptionsCategory::OPTIONS);

//隐藏的
    gArgs.AddArg("-h", "", false, OptionsCategory::HIDDEN);
    gArgs.AddArg("-help", "", false, OptionsCategory::HIDDEN);
}

static fs::path SetDataDir()
{
    fs::path ret = fs::temp_directory_path() / "bench_bitcoin" / fs::unique_path();
    fs::create_directories(ret);
    gArgs.ForceSetArg("-datadir", ret.string());
    return ret;
}

int main(int argc, char** argv)
{
    SetupBenchArgs();
    std::string error;
    if (!gArgs.ParseParameters(argc, argv, error)) {
        fprintf(stderr, "Error parsing command line arguments: %s\n", error.c_str());
        return EXIT_FAILURE;
    }

    if (HelpRequested(gArgs)) {
        std::cout << gArgs.GetHelpMessage();

        return EXIT_SUCCESS;
    }

//分析工作台选项后设置datadir
    const fs::path bench_datadir{SetDataDir()};

    SHA256AutoDetect();
    RandomInit();
    ECC_Start();
    SetupEnvironment();

    int64_t evaluations = gArgs.GetArg("-evals", DEFAULT_BENCH_EVALUATIONS);
    std::string regex_filter = gArgs.GetArg("-filter", DEFAULT_BENCH_FILTER);
    std::string scaling_str = gArgs.GetArg("-scaling", DEFAULT_BENCH_SCALING);
    bool is_list_only = gArgs.GetBoolArg("-list", false);

    double scaling_factor;
    if (!ParseDouble(scaling_str, &scaling_factor)) {
        fprintf(stderr, "Error parsing scaling factor as double: %s\n", scaling_str.c_str());
        return EXIT_FAILURE;
    }

    std::unique_ptr<benchmark::Printer> printer = MakeUnique<benchmark::ConsolePrinter>();
    std::string printer_arg = gArgs.GetArg("-printer", DEFAULT_BENCH_PRINTER);
    if ("plot" == printer_arg) {
        printer.reset(new benchmark::PlotlyPrinter(
            gArgs.GetArg("-plot-plotlyurl", DEFAULT_PLOT_PLOTLYURL),
            gArgs.GetArg("-plot-width", DEFAULT_PLOT_WIDTH),
            gArgs.GetArg("-plot-height", DEFAULT_PLOT_HEIGHT)));
    }

    benchmark::BenchRunner::RunAll(*printer, evaluations, scaling_factor, regex_filter, is_list_only);

    fs::remove_all(bench_datadir);

    ECC_Stop();

    return EXIT_SUCCESS;
}
