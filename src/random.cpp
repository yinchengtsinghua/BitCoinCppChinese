
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

#include <random.h>

#include <crypto/sha512.h>
#include <support/cleanse.h>
#ifdef WIN32
#include <compat.h> //对于Windows API
#include <wincrypt.h>
#endif
#include <logging.h>  //对于LogPrime（）
#include <sync.h>     //WaiTi锁
#include <util/time.h> //对于GETTIME（）

#include <stdlib.h>
#include <chrono>
#include <thread>

#ifndef WIN32
#include <fcntl.h>
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_GETRANDOM
#include <sys/syscall.h>
#include <linux/random.h>
#endif
#if defined(HAVE_GETENTROPY) || (defined(HAVE_GETENTROPY_RAND) && defined(MAC_OSX))
#include <unistd.h>
#endif
#if defined(HAVE_GETENTROPY_RAND) && defined(MAC_OSX)
#include <sys/random.h>
#endif
#ifdef HAVE_SYSCTL_ARND
#include <util/strencodings.h> //为阿列伦
#include <sys/sysctl.h>
#endif

#include <mutex>

#if defined(__x86_64__) || defined(__amd64__) || defined(__i386__)
#include <cpuid.h>
#endif

#include <openssl/err.h>
#include <openssl/rand.h>

[[noreturn]] static void RandFailure()
{
    LogPrintf("Failed to read randomness, aborting\n");
    std::abort();
}

static inline int64_t GetPerformanceCounter()
{
//读取硬件时间戳计数器（如果可用）。
//有关详细信息，请参阅https://en.wikipedia.org/wiki/time_stamp_counter。
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    return __rdtsc();
#elif !defined(_MSC_VER) && defined(__i386__)
    uint64_t r = 0;
__asm__ volatile ("rdtsc" : "=A"(r)); //将r变量约束到eax:edx对。
    return r;
#elif !defined(_MSC_VER) && (defined(__x86_64__) || defined(__amd64__))
    uint64_t r1 = 0, r2 = 0;
__asm__ volatile ("rdtsc" : "=a"(r1), "=d"(r2)); //约束r1到rax，r2到rdx。
    return (r2 << 32) | r1;
#else
//回到使用C++ 11时钟（通常微秒或纳秒精度）
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
#endif
}


#if defined(__x86_64__) || defined(__amd64__) || defined(__i386__)
static std::atomic<bool> hwrand_initialized{false};
static bool rdrand_supported = false;
static constexpr uint32_t CPUID_F1_ECX_RDRAND = 0x40000000;
static void RDRandInit()
{
    uint32_t eax, ebx, ecx, edx;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) && (ecx & CPUID_F1_ECX_RDRAND)) {
        LogPrintf("Using RdRand as an additional entropy source\n");
        rdrand_supported = true;
    }
    hwrand_initialized.store(true);
}
#else
static void RDRandInit() {}
#endif

static bool GetHWRand(unsigned char* ent32) {
#if defined(__x86_64__) || defined(__amd64__) || defined(__i386__)
    assert(hwrand_initialized.load(std::memory_order_relaxed));
    if (rdrand_supported) {
        uint8_t ok;
//并非所有的汇编程序都支持RDRAND指令，请用十六进制编写它。
#ifdef __i386__
        for (int iter = 0; iter < 4; ++iter) {
            uint32_t r1, r2;
__asm__ volatile (".byte 0x0f, 0xc7, 0xf0;" //RDRAND%EAX
".byte 0x0f, 0xc7, 0xf2;" //RDRAND%EDX
                              "setc %2" :
                              "=a"(r1), "=d"(r2), "=q"(ok) :: "cc");
            if (!ok) return false;
            WriteLE32(ent32 + 8 * iter, r1);
            WriteLE32(ent32 + 8 * iter + 4, r2);
        }
#else
        uint64_t r1, r2, r3, r4;
__asm__ volatile (".byte 0x48, 0x0f, 0xc7, 0xf0, " //RDRAND%RAX
"0x48, 0x0f, 0xc7, 0xf3, " //RDR% %RBX
"0x48, 0x0f, 0xc7, 0xf1, " //RDRAN% %RCX
"0x48, 0x0f, 0xc7, 0xf2; " //RDR% %RDX
                          "setc %4" :
                          "=a"(r1), "=b"(r2), "=c"(r3), "=d"(r4), "=q"(ok) :: "cc");
        if (!ok) return false;
        WriteLE64(ent32, r1);
        WriteLE64(ent32 + 8, r2);
        WriteLE64(ent32 + 16, r3);
        WriteLE64(ent32 + 24, r4);
#endif
        return true;
    }
#endif
    return false;
}

void RandAddSeed()
{
//带CPU性能计数器的种子
    int64_t nCounter = GetPerformanceCounter();
    RAND_add(&nCounter, sizeof(nCounter), 1.5);
    memory_cleanse((void*)&nCounter, sizeof(nCounter));
}

static void RandAddSeedPerfmon()
{
    RandAddSeed();

#ifdef WIN32
//在Linux上不需要这个，OpenSSL自动使用/dev/urandom
//使用整个PerfMon数据集进行种子设定

//这可能需要2秒钟，所以每10分钟才进行一次
    static int64_t nLastPerfmon;
    if (GetTime() < nLastPerfmon + 10 * 60)
        return;
    nLastPerfmon = GetTime();

    std::vector<unsigned char> vData(250000, 0);
    long ret = 0;
    unsigned long nSize = 0;
const size_t nMaxSize = 10000000; //超过10MB的性能数据
    while (true) {
        nSize = vData.size();
        ret = RegQueryValueExA(HKEY_PERFORMANCE_DATA, "Global", nullptr, nullptr, vData.data(), &nSize);
        if (ret != ERROR_MORE_DATA || vData.size() >= nMaxSize)
            break;
vData.resize(std::max((vData.size() * 3) / 2, nMaxSize)); //缓冲区大小按指数增长
    }
    RegCloseKey(HKEY_PERFORMANCE_DATA);
    if (ret == ERROR_SUCCESS) {
        RAND_add(vData.data(), nSize, nSize / 100.0);
        memory_cleanse(vData.data(), nSize);
        LogPrint(BCLog::RAND, "%s: %lu bytes\n", __func__, nSize);
    } else {
static bool warned = false; //只警告一次
        if (!warned) {
            LogPrintf("%s: Warning: RegQueryValueExA(HKEY_PERFORMANCE_DATA) failed with code %i\n", __func__, ret);
            warned = true;
        }
    }
#endif
}

#ifndef WIN32
/*回退：从/dev/urandom获取32字节的系统熵。最多的
 *在Unix ISH平台上获得加密随机性的兼容方式。
 **/

static void GetDevURandom(unsigned char *ent32)
{
    int f = open("/dev/urandom", O_RDONLY);
    if (f == -1) {
        RandFailure();
    }
    int have = 0;
    do {
        ssize_t n = read(f, ent32 + have, NUM_OS_RANDOM_BYTES - have);
        if (n <= 0 || n + have > NUM_OS_RANDOM_BYTES) {
            close(f);
            RandFailure();
        }
        have += n;
    } while (have < NUM_OS_RANDOM_BYTES);
    close(f);
}
#endif

/*获取32字节的系统熵。*/
void GetOSRand(unsigned char *ent32)
{
#if defined(WIN32)
    HCRYPTPROV hProvider;
    int ret = CryptAcquireContextW(&hProvider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    if (!ret) {
        RandFailure();
    }
    ret = CryptGenRandom(hProvider, NUM_OS_RANDOM_BYTES, ent32);
    if (!ret) {
        RandFailure();
    }
    CryptReleaseContext(hProvider, 0);
#elif defined(HAVE_SYS_GETRANDOM)
    /*Linux。从GetRandom（2）手册页：
     *“如果urandom源已初始化，则最多可读取256个字节。
     *将始终返回请求的字节数，而不会返回
     *被信号中断。”
     **/

    int rv = syscall(SYS_getrandom, ent32, NUM_OS_RANDOM_BYTES, 0);
    if (rv != NUM_OS_RANDOM_BYTES) {
        if (rv < 0 && errno == ENOSYS) {
            /*内核<3.17的回退：返回值将为-1和errno
             *如果系统调用不可用，则返回ENOSYS。
             *到/dev/urandom。
             **/

            GetDevURandom(ent32);
        } else {
            RandFailure();
        }
    }
#elif defined(HAVE_GETENTROPY) && defined(__OpenBSD__)
    /*在OpenBSD上，这可以返回多达256字节的熵，将返回
     *请求更多时出错。
     *调用返回的字节数不能小于请求的字节数。
       get熵在这里被明确地限制为openbsd，类似的（但不是
       相同的）功能可以通过glibc在其他平台上存在。
     **/

    if (getentropy(ent32, NUM_OS_RANDOM_BYTES) != 0) {
        RandFailure();
    }
#elif defined(HAVE_GETENTROPY_RAND) && defined(MAC_OSX)
//对于OSX<10.12，我们需要一个回退。
    if (&getentropy != nullptr) {
        if (getentropy(ent32, NUM_OS_RANDOM_BYTES) != 0) {
            RandFailure();
        }
    } else {
        GetDevURandom(ent32);
    }
#elif defined(HAVE_SYSCTL_ARND)
    /*Freebsd和类似产品。电话回电可能会少一些
     *字节数超过请求的字节数，因此需要在循环中读取。
     **/

    static const int name[2] = {CTL_KERN, KERN_ARND};
    int have = 0;
    do {
        size_t len = NUM_OS_RANDOM_BYTES - have;
        if (sysctl(name, ARRAYLEN(name), ent32 + have, &len, nullptr, 0) != 0) {
            RandFailure();
        }
        have += len;
    } while (have < NUM_OS_RANDOM_BYTES);
#else
    /*如果没有实现特定方法，则返回到/dev/urandom
     *获取此操作系统的系统熵。
     **/

    GetDevURandom(ent32);
#endif
}

void GetRandBytes(unsigned char* buf, int num)
{
    if (RAND_bytes(buf, num) != 1) {
        RandFailure();
    }
}

static void AddDataToRng(void* data, size_t len);

void RandAddSeedSleep()
{
    int64_t nPerfCounter1 = GetPerformanceCounter();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    int64_t nPerfCounter2 = GetPerformanceCounter();

//合并并更新状态
    AddDataToRng(&nPerfCounter1, sizeof(nPerfCounter1));
    AddDataToRng(&nPerfCounter2, sizeof(nPerfCounter2));

    memory_cleanse(&nPerfCounter1, sizeof(nPerfCounter1));
    memory_cleanse(&nPerfCounter2, sizeof(nPerfCounter2));
}


static Mutex cs_rng_state;
static unsigned char rng_state[32] = {0};
static uint64_t rng_counter = 0;

static void AddDataToRng(void* data, size_t len) {
    CSHA512 hasher;
    hasher.Write((const unsigned char*)&len, sizeof(len));
    hasher.Write((const unsigned char*)data, len);
    unsigned char buf[64];
    {
        WAIT_LOCK(cs_rng_state, lock);
        hasher.Write(rng_state, sizeof(rng_state));
        hasher.Write((const unsigned char*)&rng_counter, sizeof(rng_counter));
        ++rng_counter;
        hasher.Finalize(buf);
        memcpy(rng_state, buf + 32, 32);
    }
    memory_cleanse(buf, 64);
}

void GetStrongRandBytes(unsigned char* out, int num)
{
    assert(num <= 32);
    CSHA512 hasher;
    unsigned char buf[64];

//第一个来源：OpenSSL的RNG
    RandAddSeedPerfmon();
    GetRandBytes(buf, 32);
    hasher.Write(buf, 32);

//第二个来源：OS RNG
    GetOSRand(buf);
    hasher.Write(buf, 32);

//第三种来源：HW RNG（如有）。
    if (GetHWRand(buf)) {
        hasher.Write(buf, 32);
    }

//合并并更新状态
    {
        WAIT_LOCK(cs_rng_state, lock);
        hasher.Write(rng_state, sizeof(rng_state));
        hasher.Write((const unsigned char*)&rng_counter, sizeof(rng_counter));
        ++rng_counter;
        hasher.Finalize(buf);
        memcpy(rng_state, buf + 32, 32);
    }

//出产量
    memcpy(out, buf, num);
    memory_cleanse(buf, 64);
}

uint64_t GetRand(uint64_t nMax)
{
    if (nMax == 0)
        return 0;

//随机源的范围必须是模的倍数
//给每个可能的输出值一个相等的可能性
    uint64_t nRange = (std::numeric_limits<uint64_t>::max() / nMax) * nMax;
    uint64_t nRand = 0;
    do {
        GetRandBytes((unsigned char*)&nRand, sizeof(nRand));
    } while (nRand >= nRange);
    return (nRand % nMax);
}

int GetRandInt(int nMax)
{
    return GetRand(nMax);
}

uint256 GetRandHash()
{
    uint256 hash;
    GetRandBytes((unsigned char*)&hash, sizeof(hash));
    return hash;
}

void FastRandomContext::RandomSeed()
{
    uint256 seed = GetRandHash();
    rng.SetKey(seed.begin(), 32);
    requires_seed = false;
}

uint256 FastRandomContext::rand256()
{
    if (bytebuf_size < 32) {
        FillByteBuffer();
    }
    uint256 ret;
    memcpy(ret.begin(), bytebuf + 64 - bytebuf_size, 32);
    bytebuf_size -= 32;
    return ret;
}

std::vector<unsigned char> FastRandomContext::randbytes(size_t len)
{
    if (requires_seed) RandomSeed();
    std::vector<unsigned char> ret(len);
    if (len > 0) {
        rng.Output(&ret[0], len);
    }
    return ret;
}

FastRandomContext::FastRandomContext(const uint256& seed) : requires_seed(false), bytebuf_size(0), bitbuf_size(0)
{
    rng.SetKey(seed.begin(), 32);
}

bool Random_SanityCheck()
{
    uint64_t start = GetPerformanceCounter();

    /*这并不衡量随机性的质量，但它确实检验了这一点。
     *osrandom（）在给定最大值的情况下覆盖所有32个字节的输出
     *尝试次数。
     **/

    static const ssize_t MAX_TRIES = 1024;
    uint8_t data[NUM_OS_RANDOM_BYTES];
    /*l overwrited[num_os_random_bytes]=/*跟踪哪些字节至少被覆盖一次*/
    int num_覆盖；
    int尝试＝0；
    /*循环，直到至少覆盖一次所有字节或达到最大尝试次数*/

    do {
        memset(data, 0, NUM_OS_RANDOM_BYTES);
        GetOSRand(data);
        for (int x=0; x < NUM_OS_RANDOM_BYTES; ++x) {
            overwritten[x] |= (data[x] != 0);
        }

        num_overwritten = 0;
        for (int x=0; x < NUM_OS_RANDOM_BYTES; ++x) {
            if (overwritten[x]) {
                num_overwritten += 1;
            }
        }

        tries += 1;
    } while (num_overwritten < NUM_OS_RANDOM_BYTES && tries < MAX_TRIES);
    /*（数字被覆盖！=num_os_random_bytes）返回false；/*如果失败，则在多次尝试后释放*/

    //检查getPerformanceCounter是否至少在getosrand（）调用+1ms睡眠期间增加。
    std：：此线程：：休眠时间（std：：chrono：：毫秒（1））；
    uint64_t stop=getPerformanceCounter（）；
    if（stop==start）返回false；

    //我们调用了GetPerformanceCounter。用它作为熵。
    rand_add（（const unsigned char*）和start，sizeof（start），1）；
    rand_add（（const unsigned char*）和stop，sizeof（stop），1）；

    回归真实；
}

FastRandomContext:：FastRandomContext（Bool fDeterministic）：需要种子（！fdeterministic），bytebuf_大小（0），bitbuf_大小（0）
{
    如果（！）f确定性）
        返回；
    }
    UIT2525种子；
    rng.setkey（seed.begin（），32）；
}

FastRandomContext&FastRandomContext:：Operator=（FastRandomContext&&From）无异常
{
    需要_seed=from.requires_seed；
    RNG＝RONM RNG；
    std:：copy（std:：begin（from.bytebuf），std:：end（from.bytebuf），std:：begin（bytebuf））；
    bytebuf_大小=从.bytebuf_大小；
    bitbuf=从.bitbuf；
    bitbuf_大小=从.bitbuf_大小；
    From.Requires_Seed=真；
    从.bytebuf_size=0；
    从.bitbuf_size=0；
    返回这个；
}

空随机数
{
    RDRANDIN（）；
}
