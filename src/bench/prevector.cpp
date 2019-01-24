
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

#include <prevector.h>
#include <serialize.h>
#include <streams.h>
#include <type_traits>

#include <bench/bench.h>

//GCC 4.8缺失了一些C++ 11个类型的特征，
//网址：https://www.gnu.org/software/gcc/gcc-5/changes.html
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 5
#define IS_TRIVIALLY_CONSTRUCTIBLE std::has_trivial_default_constructor
#else
#define IS_TRIVIALLY_CONSTRUCTIBLE std::is_trivially_default_constructible
#endif

struct nontrivial_t {
    int x;
    nontrivial_t() :x(-1) {}
    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {READWRITE(x);}
};
static_assert(!IS_TRIVIALLY_CONSTRUCTIBLE<nontrivial_t>::value,
              "expected nontrivial_t to not be trivially constructible");

typedef unsigned char trivial_t;
static_assert(IS_TRIVIALLY_CONSTRUCTIBLE<trivial_t>::value,
              "expected trivial_t to be trivially constructible");

template <typename T>
static void PrevectorDestructor(benchmark::State& state)
{
    while (state.KeepRunning()) {
        for (auto x = 0; x < 1000; ++x) {
            prevector<28, T> t0;
            prevector<28, T> t1;
            t0.resize(28);
            t1.resize(29);
        }
    }
}

template <typename T>
static void PrevectorClear(benchmark::State& state)
{

    while (state.KeepRunning()) {
        for (auto x = 0; x < 1000; ++x) {
            prevector<28, T> t0;
            prevector<28, T> t1;
            t0.resize(28);
            t0.clear();
            t1.resize(29);
            t1.clear();
        }
    }
}

template <typename T>
static void PrevectorResize(benchmark::State& state)
{
    while (state.KeepRunning()) {
        prevector<28, T> t0;
        prevector<28, T> t1;
        for (auto x = 0; x < 1000; ++x) {
            t0.resize(28);
            t0.resize(0);
            t1.resize(29);
            t1.resize(0);
        }
    }
}

template <typename T>
static void PrevectorDeserialize(benchmark::State& state)
{
    CDataStream s0(SER_NETWORK, 0);
    prevector<28, T> t0;
    t0.resize(28);
    for (auto x = 0; x < 900; ++x) {
        s0 << t0;
    }
    t0.resize(100);
    for (auto x = 0; x < 101; ++x) {
        s0 << t0;
    }
    while (state.KeepRunning()) {
        prevector<28, T> t1;
        for (auto x = 0; x < 1000; ++x) {
            s0 >> t1;
        }
        s0.Init(SER_NETWORK, 0);
    }
}

#define PREVECTOR_TEST(name, nontrivops, trivops)                       \
    static void Prevector ## name ## Nontrivial(benchmark::State& state) { \
        Prevector ## name<nontrivial_t>(state);                         \
    }                                                                   \
    BENCHMARK(Prevector ## name ## Nontrivial, nontrivops);             \
    static void Prevector ## name ## Trivial(benchmark::State& state) { \
        Prevector ## name<trivial_t>(state);                            \
    }                                                                   \
    BENCHMARK(Prevector ## name ## Trivial, trivops);

PREVECTOR_TEST(Clear, 28300, 88600)
PREVECTOR_TEST(Destructor, 28800, 88900)
PREVECTOR_TEST(Resize, 28900, 90300)
PREVECTOR_TEST(Deserialize, 6800, 52000)
