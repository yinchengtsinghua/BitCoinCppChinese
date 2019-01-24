
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

#include <validation.h>
#include <bech32.h>
#include <util/strencodings.h>

#include <vector>
#include <string>


static void Bech32Encode(benchmark::State& state)
{
    std::vector<uint8_t> v = ParseHex("c97f5a67ec381b760aeaf67573bc164845ff39a3bb26a1cee401ac67243b48db");
    std::vector<unsigned char> tmp = {0};
    tmp.reserve(1 + 32 * 8 / 5);
    ConvertBits<8, 5, true>([&](unsigned char c) { tmp.push_back(c); }, v.begin(), v.end());
    while (state.KeepRunning()) {
        bech32::Encode("bc", tmp);
    }
}


static void Bech32Decode(benchmark::State& state)
{
    std::string addr = "bc1qkallence7tjawwvy0dwt4twc62qjgaw8f4vlhyd006d99f09";
    while (state.KeepRunning()) {
        bech32::Decode(addr);
    }
}


BENCHMARK(Bech32Encode, 800 * 1000);
BENCHMARK(Bech32Decode, 800 * 1000);
