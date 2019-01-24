
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <bench/bench.h>
#include <blockfilter.h>

static void ConstructGCSFilter(benchmark::State& state)
{
    GCSFilter::ElementSet elements;
    for (int i = 0; i < 10000; ++i) {
        GCSFilter::Element element(32);
        element[0] = static_cast<unsigned char>(i);
        element[1] = static_cast<unsigned char>(i >> 8);
        elements.insert(std::move(element));
    }

    uint64_t siphash_k0 = 0;
    while (state.KeepRunning()) {
        GCSFilter filter({siphash_k0, 0, 20, 1 << 20}, elements);

        siphash_k0++;
    }
}

static void MatchGCSFilter(benchmark::State& state)
{
    GCSFilter::ElementSet elements;
    for (int i = 0; i < 10000; ++i) {
        GCSFilter::Element element(32);
        element[0] = static_cast<unsigned char>(i);
        element[1] = static_cast<unsigned char>(i >> 8);
        elements.insert(std::move(element));
    }
    GCSFilter filter({0, 0, 20, 1 << 20}, elements);

    while (state.KeepRunning()) {
        filter.Match(GCSFilter::Element());
    }
}

BENCHMARK(ConstructGCSFilter, 1000);
BENCHMARK(MatchGCSFilter, 50 * 1000);
