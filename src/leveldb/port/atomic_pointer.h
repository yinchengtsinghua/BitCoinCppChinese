
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011 LevelDB作者。版权所有。
//此源代码的使用受可以
//在许可证文件中找到。有关参与者的名称，请参阅作者文件。

//atomicpointer为无锁指针提供存储。
//AtomicPointer的平台相关实现：
//-如果平台提供了一个便宜的屏障，我们将它与原始指针一起使用。
//-如果存在<atomic>（在更新版本的gcc上，它是），我们使用
//一个基于原子的原子指针。但是我们更喜欢记忆
//基于屏障的版本，因为至少在GCC4.4 32位构建上
//在Linux上，我们遇到了错误的<atomic>实现。
//另外，一些<atomic>实现比内存屏障慢得多。
//基于实现（<atomic>的获取负载约16ns与~1ns）
//基于屏障的获取负载）。
//此代码基于AtomicOps内部构件-*在Google的PerfTools中：
//http://code.google.com/p/google perftools/source/browse/svn%2ftrunk%2fsrc%2fbase

#ifndef PORT_ATOMIC_POINTER_H_
#define PORT_ATOMIC_POINTER_H_

#include <stdint.h>
#ifdef LEVELDB_ATOMIC_PRESENT
#include <atomic>
#endif
#ifdef OS_WIN
#include <windows.h>
#endif
#ifdef OS_MACOSX
#include <libkern/OSAtomic.h>
#endif

#if defined(_M_X64) || defined(__x86_64__)
#define ARCH_CPU_X86_FAMILY 1
#elif defined(_M_IX86) || defined(__i386__) || defined(__i386)
#define ARCH_CPU_X86_FAMILY 1
#elif defined(__ARMEL__)
#define ARCH_CPU_ARM_FAMILY 1
#elif defined(__aarch64__)
#define ARCH_CPU_ARM64_FAMILY 1
#elif defined(__ppc__) || defined(__powerpc__) || defined(__powerpc64__)
#define ARCH_CPU_PPC_FAMILY 1
#elif defined(__mips__)
#define ARCH_CPU_MIPS_FAMILY 1
#endif

namespace leveldb {
namespace port {

//原子指针基于<csttadomic>如果可用
#if defined(LEVELDB_ATOMIC_PRESENT)
class AtomicPointer {
 private:
  std::atomic<void*> rep_;
 public:
  AtomicPointer() { }
  explicit AtomicPointer(void* v) : rep_(v) { }
  inline void* Acquire_Load() const {
    return rep_.load(std::memory_order_acquire);
  }
  inline void Release_Store(void* v) {
    rep_.store(v, std::memory_order_release);
  }
  inline void* NoBarrier_Load() const {
    return rep_.load(std::memory_order_relaxed);
  }
  inline void NoBarrier_Store(void* v) {
    rep_.store(v, std::memory_order_relaxed);
  }
};

#else

//定义memorybarrier（）（如果可用）
//x86上的窗口
#if defined(OS_WIN) && defined(COMPILER_MSVC) && defined(ARCH_CPU_X86_FAMILY)
//windows.h已经提供了一个memorybarrier（void）宏
//http://msdn.microsoft.com/en-us/library/ms684208（v=vs.85）.aspx
#define LEVELDB_HAVE_MEMORY_BARRIER

//MAC操作系统
#elif defined(OS_MACOSX)
inline void MemoryBarrier() {
  OSMemoryBarrier();
}
#define LEVELDB_HAVE_MEMORY_BARRIER

//G86在X86上
#elif defined(ARCH_CPU_X86_FAMILY) && defined(__GNUC__)
inline void MemoryBarrier() {
//请参阅http://gcc.gnu.org/ml/gcc/2003-04/msg01180.html了解有关
//这个成语。另请参见http://en.wikipedia.org/wiki/memory_ordering。
  __asm__ __volatile__("" : : : "memory");
}
#define LEVELDB_HAVE_MEMORY_BARRIER

//阳光工作室
#elif defined(ARCH_CPU_X86_FAMILY) && defined(__SUNPRO_CC)
inline void MemoryBarrier() {
//请参阅http://gcc.gnu.org/ml/gcc/2003-04/msg01180.html了解有关
//这个成语。另请参见http://en.wikipedia.org/wiki/memory_ordering。
  asm volatile("" : : : "memory");
}
#define LEVELDB_HAVE_MEMORY_BARRIER

//ARM Linux
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(__linux__)
typedef void (*LinuxKernelMemoryBarrierFunc)(void);
//LinuxARM内核提供高度优化的设备特定内存
//固定内存地址处的屏障函数，映射到
//用户级流程。
//
//这比使用CPU特定的指令要好，这些指令在单核上
//不必要且非常昂贵的设备（如ARMV7-A“DMB”需要更多
//比皮质上的180毫微秒还多，A8就像NexusOne上的一样）。标杆管理
//表明额外的函数调用成本在
//多核设备。
//
inline void MemoryBarrier() {
  (*(LinuxKernelMemoryBarrierFunc)0xffff0fa0)();
}
#define LEVELDB_HAVE_MEMORY_BARRIER

//ARM64
#elif defined(ARCH_CPU_ARM64_FAMILY)
inline void MemoryBarrier() {
  asm volatile("dmb sy" : : : "memory");
}
#define LEVELDB_HAVE_MEMORY_BARRIER

//聚合氯化铝
#elif defined(ARCH_CPU_PPC_FAMILY) && defined(__GNUC__)
inline void MemoryBarrier() {
//对于一些PowerPC专家来说，有没有更便宜的合适的变体？
//也许通过为获取和释放操作设置单独的屏障。
  asm volatile("sync" : : : "memory");
}
#define LEVELDB_HAVE_MEMORY_BARRIER

//MIPS
#elif defined(ARCH_CPU_MIPS_FAMILY) && defined(__GNUC__)
inline void MemoryBarrier() {
  __asm__ __volatile__("sync" : : : "memory");
}
#define LEVELDB_HAVE_MEMORY_BARRIER

#endif

//使用平台特定的memorybarrier（）生成的AtomicPointer
#if defined(LEVELDB_HAVE_MEMORY_BARRIER)
class AtomicPointer {
 private:
  void* rep_;
 public:
  AtomicPointer() { }
  explicit AtomicPointer(void* p) : rep_(p) {}
  inline void* NoBarrier_Load() const { return rep_; }
  inline void NoBarrier_Store(void* v) { rep_ = v; }
  inline void* Acquire_Load() const {
    void* result = rep_;
    MemoryBarrier();
    return result;
  }
  inline void Release_Store(void* v) {
    MemoryBarrier();
    rep_ = v;
  }
};

//基于SPARC内存屏障的原子指针
#elif defined(__sparcv9) && defined(__GNUC__)
class AtomicPointer {
 private:
  void* rep_;
 public:
  AtomicPointer() { }
  explicit AtomicPointer(void* v) : rep_(v) { }
  inline void* Acquire_Load() const {
    void* val;
    __asm__ __volatile__ (
        "ldx [%[rep_]], %[val] \n\t"
         "membar #LoadLoad|#LoadStore \n\t"
        : [val] "=r" (val)
        : [rep_] "r" (&rep_)
        : "memory");
    return val;
  }
  inline void Release_Store(void* v) {
    __asm__ __volatile__ (
        "membar #LoadStore|#StoreStore \n\t"
        "stx %[v], [%[rep_]] \n\t"
        :
        : [rep_] "r" (&rep_), [v] "r" (v)
        : "memory");
  }
  inline void* NoBarrier_Load() const { return rep_; }
  inline void NoBarrier_Store(void* v) { rep_ = v; }
};

//基于ia64 acq/rel的原子指针
#elif defined(__ia64) && defined(__GNUC__)
class AtomicPointer {
 private:
  void* rep_;
 public:
  AtomicPointer() { }
  explicit AtomicPointer(void* v) : rep_(v) { }
  inline void* Acquire_Load() const {
    void* val    ;
    __asm__ __volatile__ (
        "ld8.acq %[val] = [%[rep_]] \n\t"
        : [val] "=r" (val)
        : [rep_] "r" (&rep_)
        : "memory"
        );
    return val;
  }
  inline void Release_Store(void* v) {
    __asm__ __volatile__ (
        "st8.rel [%[rep_]] = %[v]  \n\t"
        :
        : [rep_] "r" (&rep_), [v] "r" (v)
        : "memory"
        );
  }
  inline void* NoBarrier_Load() const { return rep_; }
  inline void NoBarrier_Store(void* v) { rep_ = v; }
};

//我们既没有memorybarrier（），也没有<atomic>
#else
#error Please implement AtomicPointer for this platform.

#endif
#endif

#undef LEVELDB_HAVE_MEMORY_BARRIER
#undef ARCH_CPU_X86_FAMILY
#undef ARCH_CPU_ARM_FAMILY
#undef ARCH_CPU_ARM64_FAMILY
#undef ARCH_CPU_PPC_FAMILY

}  //命名空间端口
}  //命名空间级别数据库

#endif  //端口\u原子指针\u H\u
