
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_RANDOM_H
#define BITCOIN_RANDOM_H

#include <crypto/chacha20.h>
#include <crypto/common.h>
#include <uint256.h>

#include <stdint.h>
#include <limits>

/*使用附加的熵数据设定openssl prng*/
void RandAddSeed();

/*
 *通过openssl prng收集随机数据的函数
 **/

void GetRandBytes(unsigned char* buf, int num);
uint64_t GetRand(uint64_t nMax);
int GetRandInt(int nMax);
uint256 GetRandHash();

/*
 *在getstrongRangBytes的输出中添加一点随机性。
 *这会休眠一毫秒，因此只有在有
 *没有其他工作要做。
 **/

void RandAddSeedSleep();

/*
 *从多个来源收集随机数据的函数，在任何时候都失败
 *在这些来源中，没有提供结果。
 **/

void GetStrongRandBytes(unsigned char* buf, int num);

/*
 *快速随机性源。这是用安全随机数据播种一次的，但是
 *之后是完全确定的和不安全的。
 *此类不是线程安全的。
 **/

class FastRandomContext {
private:
    bool requires_seed;
    ChaCha20 rng;

    unsigned char bytebuf[64];
    int bytebuf_size;

    uint64_t bitbuf;
    int bitbuf_size;

    void RandomSeed();

    void FillByteBuffer()
    {
        if (requires_seed) {
            RandomSeed();
        }
        rng.Output(bytebuf, sizeof(bytebuf));
        bytebuf_size = sizeof(bytebuf);
    }

    void FillBitBuffer()
    {
        bitbuf = rand64();
        bitbuf_size = 64;
    }

public:
    explicit FastRandomContext(bool fDeterministic = false);

    /*用显式种子初始化（仅用于测试）*/
    explicit FastRandomContext(const uint256& seed);

//不允许复制FastRandomContext（移动它，或创建一个新的来重新种子）。
    FastRandomContext(const FastRandomContext&) = delete;
    FastRandomContext(FastRandomContext&&) = delete;
    FastRandomContext& operator=(const FastRandomContext&) = delete;

    /*移动FastRandomContext。如果重新使用原文件，将重新种子。*/
    FastRandomContext& operator=(FastRandomContext&& from) noexcept;

    /*生成一个随机的64位整数。*/
    uint64_t rand64()
    {
        if (bytebuf_size < 8) FillByteBuffer();
        uint64_t ret = ReadLE64(bytebuf + 64 - bytebuf_size);
        bytebuf_size -= 8;
        return ret;
    }

    /*生成随机（位）位整数。*/
    uint64_t randbits(int bits) {
        if (bits == 0) {
            return 0;
        } else if (bits > 32) {
            return rand64() >> (64 - bits);
        } else {
            if (bitbuf_size < bits) FillBitBuffer();
            uint64_t ret = bitbuf & (~(uint64_t)0 >> (64 - bits));
            bitbuf >>= bits;
            bitbuf_size -= bits;
            return ret;
        }
    }

    /*在范围[0..range]内生成一个随机整数。*/
    uint64_t randrange(uint64_t range)
    {
        --range;
        int bits = CountBits(range);
        while (true) {
            uint64_t ret = randbits(bits);
            if (ret <= range) return ret;
        }
    }

    /*生成随机字节。*/
    std::vector<unsigned char> randbytes(size_t len);

    /*生成随机的32位整数。*/
    uint32_t rand32() { return randbits(32); }

    /*生成一个随机的uint256。*/
    uint256 rand256();

    /*生成随机布尔值。*/
    bool randbool() { return randbits(1); }

//与C++ 11一致随机变量生成器概念的兼容性
    typedef uint64_t result_type;
    static constexpr uint64_t min() { return 0; }
    static constexpr uint64_t max() { return std::numeric_limits<uint64_t>::max(); }
    inline uint64_t operator()() { return rand64(); }
};

/*比在FastRandomContext上使用std:：shuffle更有效。
 *
 *这更有效，因为std:：shuffle将以一组
 *当时64位，丢弃最多。
 *
 *这也适用于libstdc++std:：shuffle中可能导致
 *类型：：operator=（类型&&）将在其自身上调用，库的
 *调试模式检测并启动。这是一个已知问题，请参见
 *https://stackoverflow.com/questions/22915325/avointing-self-assignment-in-stdsuffle（https://stackoverflow.com/questions/22915325/avointing-self-assignment-in-stdsuffl
 **/

template<typename I, typename R>
void Shuffle(I first, I last, R&& rng)
{
    while (first != last) {
        size_t j = rng.randrange(last - first);
        if (j) {
            using std::swap;
            swap(*first, *(first + j));
        }
        ++first;
    }
}

/*getosrand返回的随机字节数。
 *更改此常量时，请确保更改所有呼叫站点，并
 *确保所有平台的底层操作系统API都支持这个数字。
 （许多容量限制为256字节）。
 **/

static const int NUM_OS_RANDOM_BYTES = 32;

/*获取32字节的系统熵。不要在应用程序代码中使用此项：使用
 *改为getstrongrandbytes。
 **/

void GetOSRand(unsigned char *ent32);

/*检查操作系统的随机性是否可用，并返回请求的号码
 *字节。
 **/

bool Random_SanityCheck();

/*初始化RNG。*/
void RandomInit();

#endif //比特币
