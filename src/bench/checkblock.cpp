
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2016-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <bench/bench.h>

#include <chainparams.h>
#include <validation.h>
#include <streams.h>
#include <consensus/validation.h>

namespace block_bench {
#include <bench/data/block413567.raw.h>
} //命名空间块\工作台

//这是两个主要的时间下沉，发生在我们完全收到
//一个封锁线，但在我们可以用之前
//紧凑型继电器。

static void DeserializeBlockTest(benchmark::State& state)
{
    CDataStream stream((const char*)block_bench::block413567,
            (const char*)block_bench::block413567 + sizeof(block_bench::block413567),
            SER_NETWORK, PROTOCOL_VERSION);
    char a = '\0';
stream.write(&a, 1); //防止压实

    while (state.KeepRunning()) {
        CBlock block;
        stream >> block;
        bool rewound = stream.Rewind(sizeof(block_bench::block413567));
        assert(rewound);
    }
}

static void DeserializeAndCheckBlockTest(benchmark::State& state)
{
    CDataStream stream((const char*)block_bench::block413567,
            (const char*)block_bench::block413567 + sizeof(block_bench::block413567),
            SER_NETWORK, PROTOCOL_VERSION);
    char a = '\0';
stream.write(&a, 1); //防止压实

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);

    while (state.KeepRunning()) {
CBlock block; //请注意，cblock缓存其选中状态，因此我们需要在此处重新创建它。
        stream >> block;
        bool rewound = stream.Rewind(sizeof(block_bench::block413567));
        assert(rewound);

        CValidationState validationState;
        bool checked = CheckBlock(block, validationState, chainParams->GetConsensus());
        assert(checked);
    }
}

BENCHMARK(DeserializeBlockTest, 130);
BENCHMARK(DeserializeAndCheckBlockTest, 160);
