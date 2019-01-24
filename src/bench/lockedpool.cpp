
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

#include <support/lockedpool.h>

#include <iostream>
#include <vector>

#define ASIZE 2048
#define BITER 5000
#define MSIZE 2048

static void BenchLockedPool(benchmark::State& state)
{
    void *synth_base = reinterpret_cast<void*>(0x08000000);
    const size_t synth_size = 1024*1024;
    Arena b(synth_base, synth_size, 16);

    std::vector<void*> addr;
    for (int x=0; x<ASIZE; ++x)
        addr.push_back(nullptr);
    uint32_t s = 0x12345678;
    while (state.KeepRunning()) {
        for (int x=0; x<BITER; ++x) {
            int idx = s & (addr.size()-1);
            if (s & 0x80000000) {
                b.free(addr[idx]);
                addr[idx] = nullptr;
            } else if(!addr[idx]) {
                addr[idx] = b.alloc((s >> 16) & (MSIZE-1));
            }
            bool lsb = s & 1;
            s >>= 1;
            if (lsb)
s ^= 0xf00f00f0; //LFSR周期0xF7FFFE0
        }
    }
    for (void *ptr: addr)
        b.free(ptr);
    addr.clear();
}

BENCHMARK(BenchLockedPool, 1300);
