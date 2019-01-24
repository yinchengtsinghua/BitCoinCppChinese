
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

#ifndef BITCOIN_SYNC_H
#define BITCOIN_SYNC_H

#include <threadsafety.h>

#include <condition_variable>
#include <thread>
#include <mutex>


/////////////////////////////////////
/// /
//简单定义，不包括调试代码//
/// /
/////////////////////////////////////

/*
递归互斥体；
    std：：递归互斥体；

锁定（互斥）；
    std:：unique_lock<std:：recursive_mutex>criticalBlock（mutex）；

锁2（互斥1，互斥2）；
    std:：unique_lock<std:：recursive_mutex>criticalBlock1（mutex1）；
    std:：unique_lock<std:：recursive_mutex>criticalBlock2（mutex2）；

尝试锁定（mutex，name）；
    std:：unique_lock<std:：recursive_mutex>name（mutex，std:：try_to_lock_t）；

输入_critical_部分（mutex）；//no raii
    互斥锁。

离开_critical_部分（mutex）；//没有raii
    互斥锁。
 **/


////
/// /
//实际实现//
/// /
////

#ifdef DEBUG_LOCKORDER
void EnterCritical(const char* pszName, const char* pszFile, int nLine, void* cs, bool fTry = false);
void LeaveCritical();
std::string LocksHeld();
void AssertLockHeldInternal(const char* pszName, const char* pszFile, int nLine, void* cs) ASSERT_EXCLUSIVE_LOCK(cs);
void AssertLockNotHeldInternal(const char* pszName, const char* pszFile, int nLine, void* cs);
void DeleteLock(void* cs);

/*
 *如果检测到潜在的锁顺序死锁错误，则调用abort（），而不是
 *只记录信息并抛出逻辑错误。默认为true，并且
 *在调试锁定顺序单元测试中设置为假。
 **/

extern bool g_debug_lockorder_abort;
#else
void static inline EnterCritical(const char* pszName, const char* pszFile, int nLine, void* cs, bool fTry = false) {}
void static inline LeaveCritical() {}
void static inline AssertLockHeldInternal(const char* pszName, const char* pszFile, int nLine, void* cs) ASSERT_EXCLUSIVE_LOCK(cs) {}
void static inline AssertLockNotHeldInternal(const char* pszName, const char* pszFile, int nLine, void* cs) {}
void static inline DeleteLock(void* cs) {}
#endif
#define AssertLockHeld(cs) AssertLockHeldInternal(#cs, __FILE__, __LINE__, &cs)
#define AssertLockNotHeld(cs) AssertLockNotHeldInternal(#cs, __FILE__, __LINE__, &cs)

/*
 *添加了-wthread安全锁定注释和锁定顺序的模板混合
 *检查mutex api的一个子集。
 **/

template <typename PARENT>
class LOCKABLE AnnotatedMixin : public PARENT
{
public:
    ~AnnotatedMixin() {
        DeleteLock((void*)this);
    }

    void lock() EXCLUSIVE_LOCK_FUNCTION()
    {
        PARENT::lock();
    }

    void unlock() UNLOCK_FUNCTION()
    {
        PARENT::unlock();
    }

    bool try_lock() EXCLUSIVE_TRYLOCK_FUNCTION(true)
    {
        return PARENT::try_lock();
    }

    using UniqueLock = std::unique_lock<PARENT>;
};

/*
 *wrapped mutex：支持递归锁定，但不等待
 *TODO:我们应该在默认情况下不再使用递归锁。
 **/

using RecursiveMutex = AnnotatedMixin<std::recursive_mutex>;
typedef AnnotatedMixin<std::recursive_mutex> CCriticalSection;

/*包装的互斥体：支持等待，但不支持递归锁定*/
typedef AnnotatedMixin<std::mutex> Mutex;

#ifdef DEBUG_LOCKCONTENTION
void PrintLockContention(const char* pszName, const char* pszFile, int nLine);
#endif

/*围绕std：：mutex的唯一锁样式锁进行包装。*/
template <typename Mutex, typename Base = typename Mutex::UniqueLock>
class SCOPED_LOCKABLE UniqueLock : public Base
{
private:
    void Enter(const char* pszName, const char* pszFile, int nLine)
    {
        EnterCritical(pszName, pszFile, nLine, (void*)(Base::mutex()));
#ifdef DEBUG_LOCKCONTENTION
        if (!Base::try_lock()) {
            PrintLockContention(pszName, pszFile, nLine);
#endif
            Base::lock();
#ifdef DEBUG_LOCKCONTENTION
        }
#endif
    }

    bool TryEnter(const char* pszName, const char* pszFile, int nLine)
    {
        EnterCritical(pszName, pszFile, nLine, (void*)(Base::mutex()), true);
        Base::try_lock();
        if (!Base::owns_lock())
            LeaveCritical();
        return Base::owns_lock();
    }

public:
    UniqueLock(Mutex& mutexIn, const char* pszName, const char* pszFile, int nLine, bool fTry = false) EXCLUSIVE_LOCK_FUNCTION(mutexIn) : Base(mutexIn, std::defer_lock)
    {
        if (fTry)
            TryEnter(pszName, pszFile, nLine);
        else
            Enter(pszName, pszFile, nLine);
    }

    UniqueLock(Mutex* pmutexIn, const char* pszName, const char* pszFile, int nLine, bool fTry = false) EXCLUSIVE_LOCK_FUNCTION(pmutexIn)
    {
        if (!pmutexIn) return;

        *static_cast<Base*>(this) = Base(*pmutexIn, std::defer_lock);
        if (fTry)
            TryEnter(pszName, pszFile, nLine);
        else
            Enter(pszName, pszFile, nLine);
    }

    ~UniqueLock() UNLOCK_FUNCTION()
    {
        if (Base::owns_lock())
            LeaveCritical();
    }

    operator bool()
    {
        return Base::owns_lock();
    }
};

template<typename MutexArg>
using DebugLock = UniqueLock<typename std::remove_reference<typename std::remove_pointer<MutexArg>::type>::type>;

#define PASTE(x, y) x ## y
#define PASTE2(x, y) PASTE(x, y)

#define LOCK(cs) DebugLock<decltype(cs)> PASTE2(criticalblock, __COUNTER__)(cs, #cs, __FILE__, __LINE__)
#define LOCK2(cs1, cs2)                                               \
    DebugLock<decltype(cs1)> criticalblock1(cs1, #cs1, __FILE__, __LINE__); \
    DebugLock<decltype(cs2)> criticalblock2(cs2, #cs2, __FILE__, __LINE__);
#define TRY_LOCK(cs, name) DebugLock<decltype(cs)> name(cs, #cs, __FILE__, __LINE__, true)
#define WAIT_LOCK(cs, name) DebugLock<decltype(cs)> name(cs, #cs, __FILE__, __LINE__)

#define ENTER_CRITICAL_SECTION(cs)                            \
    {                                                         \
        EnterCritical(#cs, __FILE__, __LINE__, (void*)(&cs)); \
        (cs).lock();                                          \
    }

#define LEAVE_CRITICAL_SECTION(cs) \
    {                              \
        (cs).unlock();             \
        LeaveCritical();           \
    }

class CSemaphore
{
private:
    std::condition_variable condition;
    std::mutex mutex;
    int value;

public:
    explicit CSemaphore(int init) : value(init) {}

    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [&]() { return value >= 1; });
        value--;
    }

    bool try_wait()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (value < 1)
            return false;
        value--;
        return true;
    }

    void post()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            value++;
        }
        condition.notify_one();
    }
};

/*RAII型信号锁*/
class CSemaphoreGrant
{
private:
    CSemaphore* sem;
    bool fHaveGrant;

public:
    void Acquire()
    {
        if (fHaveGrant)
            return;
        sem->wait();
        fHaveGrant = true;
    }

    void Release()
    {
        if (!fHaveGrant)
            return;
        sem->post();
        fHaveGrant = false;
    }

    bool TryAcquire()
    {
        if (!fHaveGrant && sem->try_wait())
            fHaveGrant = true;
        return fHaveGrant;
    }

    void MoveTo(CSemaphoreGrant& grant)
    {
        grant.Release();
        grant.sem = sem;
        grant.fHaveGrant = fHaveGrant;
        fHaveGrant = false;
    }

    CSemaphoreGrant() : sem(nullptr), fHaveGrant(false) {}

    explicit CSemaphoreGrant(CSemaphore& sema, bool fTry = false) : sem(&sema), fHaveGrant(false)
    {
        if (fTry)
            TryAcquire();
        else
            Acquire();
    }

    ~CSemaphoreGrant()
    {
        Release();
    }

    operator bool() const
    {
        return fHaveGrant;
    }
};

#endif //比特尼扬同步机
