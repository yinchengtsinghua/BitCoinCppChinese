
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

#ifndef BITCOIN_SUPPORT_LOCKEDPOOL_H
#define BITCOIN_SUPPORT_LOCKEDPOOL_H

#include <stdint.h>
#include <list>
#include <map>
#include <mutex>
#include <memory>
#include <unordered_map>

/*
 *与操作系统相关的锁定/固定内存页的分配和释放。
 *抽象基类。
 **/

class LockedPageAllocator
{
public:
    virtual ~LockedPageAllocator() {}
    /*分配和锁定内存页。
     *如果len不是系统页面大小的倍数，则向上取整。
     *如果分配失败，则返回nullptr。
     *
     *如果无法锁定内存页，它仍将
     *返回内存，但LockingSuccess标志将为假。
     *如果分配失败，则未定义LockingSuccess。
     **/

    virtual void* AllocateLocked(size_t len, bool *lockingSuccess) = 0;

    /*解锁并释放内存页。
     *解锁前清除内存。
     **/

    virtual void FreeLocked(void* addr, size_t len) = 0;

    /*获取可能被此锁定的内存总量限制
     *进程，以字节为单位。如果没有限制或限制，返回大小最大值
     *是未知的。如果完全无法锁定内存，则返回0。
     **/

    virtual size_t GetLimit() = 0;
};

/*竞技场通过将内存划分为
 *块。
 **/

class Arena
{
public:
    Arena(void *base, size_t size, size_t alignment);
    virtual ~Arena();

Arena(const Arena& other) = delete; //非构造可复制
Arena& operator=(const Arena&) = delete; //不可复制的

    /*内存统计。*/
    struct Stats
    {
        size_t used;
        size_t free;
        size_t total;
        size_t chunks_used;
        size_t chunks_free;
    };

    /*从这个竞技场分配大小字节。
     *返回指向成功的指针；如果内存已满，则返回0；或者
     *应用程序试图分配0个字节。
     **/

    void* alloc(size_t size);

    /*释放以前分配的内存块。
     *释放零指针无效。
     *出错时引发std:：runtime_错误。
     **/

    void free(void *ptr);

    /*获取竞技场使用统计信息*/
    Stats stats() const;

#ifdef ARENA_DEBUG
    void walk() const;
#endif

    /*返回指针是否指向此竞技场。
     *这将返回base<=ptr<（base+size），因此仅将其用于（包含）
     *区块起始地址。
     **/

    bool addressInArena(void *ptr) const { return ptr >= base && ptr < end; }
private:
    typedef std::multimap<size_t, char*> SizeToChunkSortedMap;
    /*映射以启用O（log（n））最佳匹配分配，因为它按大小排序*/
    SizeToChunkSortedMap size_to_free_chunk;

    typedef std::unordered_map<char*, SizeToChunkSortedMap::const_iterator> ChunkToSizeMap;
    /*从空闲块的开始映射到其大小为\u到\u的节点*/
    ChunkToSizeMap chunks_free;
    /*从自由块的末端映射到其大小为\到\的节点*/
    ChunkToSizeMap chunks_free_end;

    /*从已用块的开头映射到其大小*/
    std::unordered_map<char*, size_t> chunks_used;

    /*竞技场基址*/
    char* base;
    /*竞技场结束地址*/
    char* end;
    /*最小块对齐*/
    size_t alignment;
};

/*用于锁定内存块的池。
 *
 *为了避免敏感密钥数据交换到磁盘，此池中的内存
 *已锁定/固定。
 *
 *竞技场管理一个连续的内存区域。游泳池从一个竞技场开始
 *但如果需要，可以成长为多个竞技场。
 *
 *与普通的C堆不同，管理结构与托管结构是分开的。
 *记忆。这是因为物体的大小和底座本身并不敏感。
 *信息，以保存宝贵的锁定内存。在某些操作系统中
 *可以锁定的内存量很小。
 **/

class LockedPool
{
public:
    /*锁定内存的一个竞技场的大小。这是一种妥协。
     *不要设置得太低，因为管理多个竞技场会增加
     *分配和解除分配开销。设置得太高分配
     *操作系统的锁定内存比严格要求的要多。
     **/

    static const size_t ARENA_SIZE = 256*1024;
    /*块对齐。另一个妥协。设置得太高会浪费
     *内存，设置得太低会促进碎片化。
     **/

    static const size_t ARENA_ALIGN = 16;

    /*分配成功但锁定失败时回调。
     **/

    typedef bool (*LockingFailed_Callback)();

    /*内存统计。*/
    struct Stats
    {
        size_t used;
        size_t free;
        size_t total;
        size_t locked;
        size_t chunks_used;
        size_t chunks_free;
    };

    /*创建新的锁定池。这将取得memoryPageLocker的所有权，
     *只能使用lockedpool（std:：move（…）实例化此实例。
     *
     *第二个参数是锁定新分配的竞技场失败时的可选回调。
     *如果提供此回调并返回false，则分配失败（硬失败），如果
     *它返回的是正确的分配过程，但它可以发出警告。
     **/

    explicit LockedPool(std::unique_ptr<LockedPageAllocator> allocator, LockingFailed_Callback lf_cb_in = nullptr);
    ~LockedPool();

LockedPool(const LockedPool& other) = delete; //非构造可复制
LockedPool& operator=(const LockedPool&) = delete; //不可复制的

    /*从这个竞技场分配大小字节。
     *返回指向成功的指针；如果内存已满，则返回0；或者
     *应用程序试图分配0个字节。
     **/

    void* alloc(size_t size);

    /*释放以前分配的内存块。
     *释放零指针无效。
     *出错时引发std:：runtime_错误。
     **/

    void free(void *ptr);

    /*获取池使用情况统计信息*/
    Stats stats() const;
private:
    std::unique_ptr<LockedPageAllocator> allocator;

    /*从锁定的页面创建竞技场*/
    class LockedPageArena: public Arena
    {
    public:
        LockedPageArena(LockedPageAllocator *alloc_in, void *base_in, size_t size, size_t align);
        ~LockedPageArena();
    private:
        void *base;
        size_t size;
        LockedPageAllocator *allocator;
    };

    bool new_arena(size_t size, size_t align);

    std::list<LockedPageArena> arenas;
    LockingFailed_Callback lf_cb;
    size_t cumulative_bytes_locked;
    /*互斥体保护对该池的数据结构（包括竞技场）的访问。
     **/

    mutable std::mutex mutex;
};

/*
 *singleton类跟踪锁定（即，不可交换）内存，用于
 *std：：分配器模板。
 *
 *STL的一些实现在一些构造函数中分配内存（例如，请参见
 *msvc的vector<t>实现，它在分配器中分配1字节的内存。）
 *由于静态初始值设定项的顺序不可预测，我们必须确保
 *LockedPoolManager实例在使用任何其他基于STL的对象之前存在
 *创建安全分配程序。因此，与其让LockedPoolManager也
 *静态初始化，按需创建。
 **/

class LockedPoolManager : public LockedPool
{
public:
    /*返回当前实例，或创建一次*/
    static LockedPoolManager& Instance()
    {
        std::call_once(LockedPoolManager::init_flag, LockedPoolManager::CreateInstance);
        return *LockedPoolManager::_instance;
    }

private:
    explicit LockedPoolManager(std::unique_ptr<LockedPageAllocator> allocator);

    /*创建一个专门用于操作系统的新的LockedPoolManager*/
    static void CreateInstance();
    /*当锁定失败时调用，在此警告用户*/
    static bool LockingFailed();

    static LockedPoolManager* _instance;
    static std::once_flag init_flag;
};

#endif //比特币支持锁定池
