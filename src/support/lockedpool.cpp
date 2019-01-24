
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

#include <support/lockedpool.h>
#include <support/cleanse.h>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#ifdef WIN32
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h> //对于MMAP
#include <sys/resource.h> //对于GeTrimeLime:
#include <limits.h> //页面化
#include <unistd.h> //为西斯康夫
#endif

#include <algorithm>

LockedPoolManager* LockedPoolManager::_instance = nullptr;
std::once_flag LockedPoolManager::init_flag;

/*********************************************************************/
//公用事业
//
/*对齐到2的幂*/
static inline size_t align_up(size_t x, size_t align)
{
    return (x + align - 1) & ~(align - 1);
}

/*********************************************************************/
//实施：竞技场

Arena::Arena(void *base_in, size_t size_in, size_t alignment_in):
    base(static_cast<char*>(base_in)), end(static_cast<char*>(base_in) + size_in), alignment(alignment_in)
{
//从一个覆盖整个竞技场的自由块开始
    auto it = size_to_free_chunk.emplace(size_in, base);
    chunks_free.emplace(base, it);
    chunks_free_end.emplace(base + size_in, it);
}

Arena::~Arena()
{
}

void* Arena::alloc(size_t size)
{
//四舍五入到下一个对齐倍数
    size = align_up(size, alignment);

//不处理零大小的块
    if (size == 0)
        return nullptr;

//选择足够大的空闲块。返回指向不小于键的第一个元素的迭代器。
//这种分配策略是最合适的。根据“动态存储分配：调查与评论”，
//Wilson et. al。1995年，http://www.scs.stanford.edu/14wi-cs140/sched/readings/wilson.pdf，最佳匹配和首次匹配
//政策在实践中似乎很有效。
    auto size_ptr_it = size_to_free_chunk.lower_bound(size);
    if (size_ptr_it == size_to_free_chunk.end())
        return nullptr;

//创建已使用的块，从空闲块的末尾占用其空间
    const size_t size_remaining = size_ptr_it->first - size;
    auto allocated = chunks_used.emplace(size_ptr_it->second + size_remaining, size).first;
    chunks_free_end.erase(size_ptr_it->second + size_ptr_it->first);
    if (size_ptr_it->first == size) {
//整块都用完了
        chunks_free.erase(size_ptr_it->second);
    } else {
//还有一些内存留在块中
        auto it_remaining = size_to_free_chunk.emplace(size_remaining, size_ptr_it->second);
        chunks_free[size_ptr_it->second] = it_remaining;
        chunks_free_end.emplace(size_ptr_it->second + size_remaining, it_remaining);
    }
    size_to_free_chunk.erase(size_ptr_it);

    return reinterpret_cast<void*>(allocated->first);
}

void Arena::free(void *ptr)
{
//释放nullptr指针是可以的。
    if (ptr == nullptr) {
        return;
    }

//从已用映射中删除块
    auto i = chunks_used.find(static_cast<char*>(ptr));
    if (i == chunks_used.end()) {
        throw std::runtime_error("Arena: invalid or double free");
    }
    std::pair<char*, size_t> freed = *i;
    chunks_used.erase(i);

//与前一块释放的合并
    auto prev = chunks_free_end.find(freed.first);
    if (prev != chunks_free_end.end()) {
        freed.first -= prev->second->first;
        freed.second += prev->second->first;
        size_to_free_chunk.erase(prev->second);
        chunks_free_end.erase(prev);
    }

//释放后与块合并释放
    auto next = chunks_free.find(freed.first + freed.second);
    if (next != chunks_free.end()) {
        freed.second += next->second->first;
        size_to_free_chunk.erase(next->second);
        chunks_free.erase(next);
    }

//使用合并的可用块添加/设置空间
    auto it = size_to_free_chunk.emplace(freed.second, freed.first);
    chunks_free[freed.first] = it;
    chunks_free_end[freed.first + freed.second] = it;
}

Arena::Stats Arena::stats() const
{
    Arena::Stats r{ 0, 0, 0, chunks_used.size(), chunks_free.size() };
    for (const auto& chunk: chunks_used)
        r.used += chunk.second;
    for (const auto& chunk: chunks_free)
        r.free += chunk.second->first;
    r.total = r.used + r.free;
    return r;
}

#ifdef ARENA_DEBUG
static void printchunk(char* base, size_t sz, bool used) {
    std::cout <<
        "0x" << std::hex << std::setw(16) << std::setfill('0') << base <<
        " 0x" << std::hex << std::setw(16) << std::setfill('0') << sz <<
        " 0x" << used << std::endl;
}
void Arena::walk() const
{
    for (const auto& chunk: chunks_used)
        printchunk(chunk.first, chunk.second, true);
    std::cout << std::endl;
    for (const auto& chunk: chunks_free)
        printchunk(chunk.first, chunk.second, false);
    std::cout << std::endl;
}
#endif

/*********************************************************************/
//实现：win32lockedpageallocator

#ifdef WIN32
/*专门针对Windows的LockedPageAllocator。
 **/

class Win32LockedPageAllocator: public LockedPageAllocator
{
public:
    Win32LockedPageAllocator();
    void* AllocateLocked(size_t len, bool *lockingSuccess) override;
    void FreeLocked(void* addr, size_t len) override;
    size_t GetLimit() override;
private:
    size_t page_size;
};

Win32LockedPageAllocator::Win32LockedPageAllocator()
{
//确定系统页面大小（字节）
    SYSTEM_INFO sSysInfo;
    GetSystemInfo(&sSysInfo);
    page_size = sSysInfo.dwPageSize;
}
void *Win32LockedPageAllocator::AllocateLocked(size_t len, bool *lockingSuccess)
{
    len = align_up(len, page_size);
    void *addr = VirtualAlloc(nullptr, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (addr) {
//virtualLock用于防止键控材料被交换。注释
//它不提供这一点作为保证，但在实践中，记忆
//已经被virtualLock几乎从未写入页面文件
//除非在极少的情况下，记忆力极低。
        *lockingSuccess = VirtualLock(const_cast<void*>(addr), len) != 0;
    }
    return addr;
}
void Win32LockedPageAllocator::FreeLocked(void* addr, size_t len)
{
    len = align_up(len, page_size);
    memory_cleanse(addr, len);
    VirtualUnlock(const_cast<void*>(addr), len);
}

size_t Win32LockedPageAllocator::GetLimit()
{
//TODO Windows有限制吗，如何获取？
    return std::numeric_limits<size_t>::max();
}
#endif

/*********************************************************************/
//实现：posixLockedPageAllocator

#ifndef WIN32
/*锁定的页面分配器，专门用于那些不尝试
 *特殊雪花。
 **/

class PosixLockedPageAllocator: public LockedPageAllocator
{
public:
    PosixLockedPageAllocator();
    void* AllocateLocked(size_t len, bool *lockingSuccess) override;
    void FreeLocked(void* addr, size_t len) override;
    size_t GetLimit() override;
private:
    size_t page_size;
};

PosixLockedPageAllocator::PosixLockedPageAllocator()
{
//确定系统页面大小（字节）
#if defined(PAGESIZE) //在极限H中定义
    page_size = PAGESIZE;
#else                   //假设一些posix操作系统
    page_size = sysconf(_SC_PAGESIZE);
#endif
}

//有些系统（至少是OS X）还没有定义map-anonymous并定义
//映射已弃用的
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

void *PosixLockedPageAllocator::AllocateLocked(size_t len, bool *lockingSuccess)
{
    void *addr;
    len = align_up(len, page_size);
    addr = mmap(nullptr, len, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) {
        return nullptr;
    }
    if (addr) {
        *lockingSuccess = mlock(addr, len) == 0;
    }
    return addr;
}
void PosixLockedPageAllocator::FreeLocked(void* addr, size_t len)
{
    len = align_up(len, page_size);
    memory_cleanse(addr, len);
    munlock(addr, len);
    munmap(addr, len);
}
size_t PosixLockedPageAllocator::GetLimit()
{
#ifdef RLIMIT_MEMLOCK
    struct rlimit rlim;
    if (getrlimit(RLIMIT_MEMLOCK, &rlim) == 0) {
        if (rlim.rlim_cur != RLIM_INFINITY) {
            return rlim.rlim_cur;
        }
    }
#endif
    return std::numeric_limits<size_t>::max();
}
#endif

/*********************************************************************/
//实现：锁定池

LockedPool::LockedPool(std::unique_ptr<LockedPageAllocator> allocator_in, LockingFailed_Callback lf_cb_in):
    allocator(std::move(allocator_in)), lf_cb(lf_cb_in), cumulative_bytes_locked(0)
{
}

LockedPool::~LockedPool()
{
}
void* LockedPool::alloc(size_t size)
{
    std::lock_guard<std::mutex> lock(mutex);

//不要处理不可能的尺寸
    if (size == 0 || size > ARENA_SIZE)
        return nullptr;

//尝试从每个当前竞技场分配
    for (auto &arena: arenas) {
        void *addr = arena.alloc(size);
        if (addr) {
            return addr;
        }
    }
//如果失败，创建一个新的
    if (new_arena(ARENA_SIZE, ARENA_ALIGN)) {
        return arenas.back().alloc(size);
    }
    return nullptr;
}

void LockedPool::free(void *ptr)
{
    std::lock_guard<std::mutex> lock(mutex);
//我们可以通过保留竞技场地图来做得比线性搜索更好。
//扩展到竞技场，并查找地址。
    for (auto &arena: arenas) {
        if (arena.addressInArena(ptr)) {
            arena.free(ptr);
            return;
        }
    }
    throw std::runtime_error("LockedPool: invalid address not pointing to any arena");
}

LockedPool::Stats LockedPool::stats() const
{
    std::lock_guard<std::mutex> lock(mutex);
    LockedPool::Stats r{0, 0, 0, cumulative_bytes_locked, 0, 0};
    for (const auto &arena: arenas) {
        Arena::Stats i = arena.stats();
        r.used += i.used;
        r.free += i.free;
        r.total += i.total;
        r.chunks_used += i.chunks_used;
        r.chunks_free += i.chunks_free;
    }
    return r;
}

bool LockedPool::new_arena(size_t size, size_t align)
{
    bool locked;
//如果这是第一个竞技场，请特别处理：盖上大尺寸
//按进程限制。这确保第一个竞技场至少
//被锁上。例外情况是，如果进程限制为0:
//在这种情况下，没有内存可以被锁定，所以我们将跳过这个逻辑。
    if (arenas.empty()) {
        size_t limit = allocator->GetLimit();
        if (limit > 0) {
            size = std::min(size, limit);
        }
    }
    void *addr = allocator->AllocateLocked(size, &locked);
    if (!addr) {
        return false;
    }
    if (locked) {
        cumulative_bytes_locked += size;
} else if (lf_cb) { //如果锁定失败，则调用锁定失败回调
if (!lf_cb()) { //如果回调返回false，则释放内存并失败，否则将考虑警告用户并继续。
            allocator->FreeLocked(addr, size);
            return false;
        }
    }
    arenas.emplace_back(allocator.get(), addr, size, align);
    return true;
}

LockedPool::LockedPageArena::LockedPageArena(LockedPageAllocator *allocator_in, void *base_in, size_t size_in, size_t align_in):
    Arena(base_in, size_in, align_in), base(base_in), size(size_in), allocator(allocator_in)
{
}
LockedPool::LockedPageArena::~LockedPageArena()
{
    allocator->FreeLocked(base, size);
}

/*********************************************************************/
//实现：LockedPoolManager
//
LockedPoolManager::LockedPoolManager(std::unique_ptr<LockedPageAllocator> allocator_in):
    LockedPool(std::move(allocator_in), &LockedPoolManager::LockingFailed)
{
}

bool LockedPoolManager::LockingFailed()
{
//托多：记录一些东西，但是怎么做？不包括实用程序h
    return true;
}

void LockedPoolManager::CreateInstance()
{
//使用本地静态实例可以确保对象已初始化
//当它首先被需要的时候，并且在所有使用它的对象之后被去初始化
//一切都结束了。我能想到一个不太可能的情况
//有一个静态的取消初始化顺序/问题，但是签入
//LockedPoolManagerBase的析构函数帮助我们检测是否会发生这种情况。
#ifdef WIN32
    std::unique_ptr<LockedPageAllocator> allocator(new Win32LockedPageAllocator());
#else
    std::unique_ptr<LockedPageAllocator> allocator(new PosixLockedPageAllocator());
#endif
    static LockedPoolManager instance(std::move(allocator));
    LockedPoolManager::_instance = &instance;
}
