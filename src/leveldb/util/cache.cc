
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "leveldb/cache.h"
#include "port/port.h"
#include "util/hash.h"
#include "util/mutexlock.h"

namespace leveldb {

Cache::~Cache() {
}

namespace {

//LRU缓存实现
//
//缓存条目有一个“in-cache”布尔值，指示缓存是否具有
//条目上的引用。如果没有
//传递给“deleter”的条目是通过erase（），当
//插入具有重复键的元素，或在销毁缓存时插入。
//
//缓存在缓存中保留两个项目的链接列表。中的所有项
//缓存在一个列表或另一个列表中，而不是同时存在。仍然引用的项
//但从缓存中删除的客户端都不在这两个列表中。名单如下：
//-正在使用：包含客户端当前引用的项，不包含
//特殊顺序。（此列表用于固定检查。如果我们
//删除了检查，否则将在此列表中的元素
//保留为断开的单例列表。）
//-lru：包含客户机当前未引用的项目，按lru顺序
//元素通过ref（）和unref（）方法在这些列表之间移动，
//当它们检测到缓存中的元素获取或丢失
//外部参考。

//条目是一个可变长度的堆分配结构。条目
//保存在一个按访问时间排序的循环双链接列表中。
struct LRUHandle {
  void* value;
  void (*deleter)(const Slice&, void* value);
  LRUHandle* next_hash;
  LRUHandle* next;
  LRUHandle* prev;
size_t charge;      //TODO（opt）：只允许uint32？
  size_t key_length;
bool in_cache;      //条目是否在缓存中。
uint32_t refs;      //引用，包括缓存引用（如果存在）。
uint32_t hash;      //key（）的哈希；用于快速切分和比较
char key_data[1];   //键的开头

  Slice key() const {
//为了更便宜的查找，我们允许使用临时句柄对象
//将指向键的指针存储在“value”中。
    if (next == this) {
      return *(reinterpret_cast<Slice*>(value));
    } else {
      return Slice(key_data, key_length);
    }
  }
};

//我们提供了我们自己的简单哈希表，因为它删除了大量
//移植黑客，也比一些内置哈希更快
//一些编译器/运行时组合中的表实现
//我们已经测试过了。例如，ReadRandom的速度比G++快了约5%
//4.4.3的内置哈希表。
class HandleTable {
 public:
  HandleTable() : length_(0), elems_(0), list_(NULL) { Resize(); }
  ~HandleTable() { delete[] list_; }

  LRUHandle* Lookup(const Slice& key, uint32_t hash) {
    return *FindPointer(key, hash);
  }

  LRUHandle* Insert(LRUHandle* h) {
    LRUHandle** ptr = FindPointer(h->key(), h->hash);
    LRUHandle* old = *ptr;
    h->next_hash = (old == NULL ? NULL : old->next_hash);
    *ptr = h;
    if (old == NULL) {
      ++elems_;
      if (elems_ > length_) {
//由于每个缓存条目都相当大，因此我们的目标是
//平均链接列表长度（<=1）。
        Resize();
      }
    }
    return old;
  }

  LRUHandle* Remove(const Slice& key, uint32_t hash) {
    LRUHandle** ptr = FindPointer(key, hash);
    LRUHandle* result = *ptr;
    if (result != NULL) {
      *ptr = result->next_hash;
      --elems_;
    }
    return result;
  }

 private:
//该表由每个存储桶所在的存储桶数组组成。
//散列到存储桶中的缓存项的链接列表。
  uint32_t length_;
  uint32_t elems_;
  LRUHandle** list_;

//返回指向指向缓存项的插槽的指针，
//匹配键/哈希。如果没有此类缓存项，则返回
//指向相应链接列表中尾随插槽的指针。
  LRUHandle** FindPointer(const Slice& key, uint32_t hash) {
    LRUHandle** ptr = &list_[hash & (length_ - 1)];
    while (*ptr != NULL &&
           ((*ptr)->hash != hash || key != (*ptr)->key())) {
      ptr = &(*ptr)->next_hash;
    }
    return ptr;
  }

  void Resize() {
    uint32_t new_length = 4;
    while (new_length < elems_) {
      new_length *= 2;
    }
    LRUHandle** new_list = new LRUHandle*[new_length];
    memset(new_list, 0, sizeof(new_list[0]) * new_length);
    uint32_t count = 0;
    for (uint32_t i = 0; i < length_; i++) {
      LRUHandle* h = list_[i];
      while (h != NULL) {
        LRUHandle* next = h->next_hash;
        uint32_t hash = h->hash;
        LRUHandle** ptr = &new_list[hash & (new_length - 1)];
        h->next_hash = *ptr;
        *ptr = h;
        h = next;
        count++;
      }
    }
    assert(elems_ == count);
    delete[] list_;
    list_ = new_list;
    length_ = new_length;
  }
};

//一个碎片缓存。
class LRUCache {
 public:
  LRUCache();
  ~LRUCache();

//与构造函数分离，以便调用方可以轻松地创建lrucache数组
  void SetCapacity(size_t capacity) { capacity_ = capacity; }

//类似于缓存方法，但带有额外的“hash”参数。
  Cache::Handle* Insert(const Slice& key, uint32_t hash,
                        void* value, size_t charge,
                        void (*deleter)(const Slice& key, void* value));
  Cache::Handle* Lookup(const Slice& key, uint32_t hash);
  void Release(Cache::Handle* handle);
  void Erase(const Slice& key, uint32_t hash);
  void Prune();
  size_t TotalCharge() const {
    MutexLock l(&mutex_);
    return usage_;
  }

 private:
  void LRU_Remove(LRUHandle* e);
  void LRU_Append(LRUHandle*list, LRUHandle* e);
  void Ref(LRUHandle* e);
  void Unref(LRUHandle* e);
  bool FinishErase(LRUHandle* e);

//使用前已初始化。
  size_t capacity_;

//互斥保护以下状态。
  mutable port::Mutex mutex_;
  size_t usage_;

//LRU列表的虚拟头。
//lru.prev是最新的条目，lru.next是最旧的条目。
//条目具有refs==1和in_cache==true。
  LRUHandle lru_;

//使用中的虚拟头列表。
//客户机正在使用条目，并且refs>=2和in_cache==true。
  LRUHandle in_use_;

  HandleTable table_;
};

LRUCache::LRUCache()
    : usage_(0) {
//生成空的循环链接列表。
  lru_.next = &lru_;
  lru_.prev = &lru_;
  in_use_.next = &in_use_;
  in_use_.prev = &in_use_;
}

LRUCache::~LRUCache() {
assert(in_use_.next == &in_use_);  //调用方具有未释放的句柄时出错
  for (LRUHandle* e = lru_.next; e != &lru_; ) {
    LRUHandle* next = e->next;
    assert(e->in_cache);
    e->in_cache = false;
assert(e->refs == 1);  //LRU列表的不变量。
    Unref(e);
    e = next;
  }
}

void LRUCache::Ref(LRUHandle* e) {
if (e->refs == 1 && e->in_cache) {  //如果在lru-list上，移动到in-use-list。
    LRU_Remove(e);
    LRU_Append(&in_use_, e);
  }
  e->refs++;
}

void LRUCache::Unref(LRUHandle* e) {
  assert(e->refs > 0);
  e->refs--;
if (e->refs == 0) { //解除分配。
    assert(!e->in_cache);
    (*e->deleter)(e->key(), e->value);
    free(e);
} else if (e->in_cache && e->refs == 1) {  //不再使用；移动到lru_uu列表。
    LRU_Remove(e);
    LRU_Append(&lru_, e);
  }
}

void LRUCache::LRU_Remove(LRUHandle* e) {
  e->next->prev = e->prev;
  e->prev->next = e->next;
}

void LRUCache::LRU_Append(LRUHandle* list, LRUHandle* e) {
//通过在*列表前插入使“e”成为最新条目
  e->next = list;
  e->prev = list->prev;
  e->prev->next = e;
  e->next->prev = e;
}

Cache::Handle* LRUCache::Lookup(const Slice& key, uint32_t hash) {
  MutexLock l(&mutex_);
  LRUHandle* e = table_.Lookup(key, hash);
  if (e != NULL) {
    Ref(e);
  }
  return reinterpret_cast<Cache::Handle*>(e);
}

void LRUCache::Release(Cache::Handle* handle) {
  MutexLock l(&mutex_);
  Unref(reinterpret_cast<LRUHandle*>(handle));
}

Cache::Handle* LRUCache::Insert(
    const Slice& key, uint32_t hash, void* value, size_t charge,
    void (*deleter)(const Slice& key, void* value)) {
  MutexLock l(&mutex_);

  LRUHandle* e = reinterpret_cast<LRUHandle*>(
      malloc(sizeof(LRUHandle)-1 + key.size()));
  e->value = value;
  e->deleter = deleter;
  e->charge = charge;
  e->key_length = key.size();
  e->hash = hash;
  e->in_cache = false;
e->refs = 1;  //用于返回的句柄。
  memcpy(e->key_data, key.data(), key.size());

  if (capacity_ > 0) {
e->refs++;  //以供缓存参考。
    e->in_cache = true;
    LRU_Append(&in_use_, e);
    usage_ += charge;
    FinishErase(table_.Insert(e));
} //否则不要缓存。（测试使用容量==0关闭缓存。）

  while (usage_ > capacity_ && lru_.next != &lru_) {
    LRUHandle* old = lru_.next;
    assert(old->refs == 1);
    bool erased = FinishErase(table_.Remove(old->key(), old->hash));
if (!erased) {  //在编译ndebug时避免使用未使用的变量
      assert(erased);
    }
  }

  return reinterpret_cast<Cache::Handle*>(e);
}

//如果是！=空，完成从缓存中删除*e；它已被删除
//从哈希表。返回E！= NULL。需要保持互斥。
bool LRUCache::FinishErase(LRUHandle* e) {
  if (e != NULL) {
    assert(e->in_cache);
    LRU_Remove(e);
    e->in_cache = false;
    usage_ -= e->charge;
    Unref(e);
  }
  return e != NULL;
}

void LRUCache::Erase(const Slice& key, uint32_t hash) {
  MutexLock l(&mutex_);
  FinishErase(table_.Remove(key, hash));
}

void LRUCache::Prune() {
  MutexLock l(&mutex_);
  while (lru_.next != &lru_) {
    LRUHandle* e = lru_.next;
    assert(e->refs == 1);
    bool erased = FinishErase(table_.Remove(e->key(), e->hash));
if (!erased) {  //在编译ndebug时避免使用未使用的变量
      assert(erased);
    }
  }
}

static const int kNumShardBits = 4;
static const int kNumShards = 1 << kNumShardBits;

class ShardedLRUCache : public Cache {
 private:
  LRUCache shard_[kNumShards];
  port::Mutex id_mutex_;
  uint64_t last_id_;

  static inline uint32_t HashSlice(const Slice& s) {
    return Hash(s.data(), s.size(), 0);
  }

  static uint32_t Shard(uint32_t hash) {
    return hash >> (32 - kNumShardBits);
  }

 public:
  explicit ShardedLRUCache(size_t capacity)
      : last_id_(0) {
    const size_t per_shard = (capacity + (kNumShards - 1)) / kNumShards;
    for (int s = 0; s < kNumShards; s++) {
      shard_[s].SetCapacity(per_shard);
    }
  }
  virtual ~ShardedLRUCache() { }
  virtual Handle* Insert(const Slice& key, void* value, size_t charge,
                         void (*deleter)(const Slice& key, void* value)) {
    const uint32_t hash = HashSlice(key);
    return shard_[Shard(hash)].Insert(key, hash, value, charge, deleter);
  }
  virtual Handle* Lookup(const Slice& key) {
    const uint32_t hash = HashSlice(key);
    return shard_[Shard(hash)].Lookup(key, hash);
  }
  virtual void Release(Handle* handle) {
    LRUHandle* h = reinterpret_cast<LRUHandle*>(handle);
    shard_[Shard(h->hash)].Release(handle);
  }
  virtual void Erase(const Slice& key) {
    const uint32_t hash = HashSlice(key);
    shard_[Shard(hash)].Erase(key, hash);
  }
  virtual void* Value(Handle* handle) {
    return reinterpret_cast<LRUHandle*>(handle)->value;
  }
  virtual uint64_t NewId() {
    MutexLock l(&id_mutex_);
    return ++(last_id_);
  }
  virtual void Prune() {
    for (int s = 0; s < kNumShards; s++) {
      shard_[s].Prune();
    }
  }
  virtual size_t TotalCharge() const {
    size_t total = 0;
    for (int s = 0; s < kNumShards; s++) {
      total += shard_[s].TotalCharge();
    }
    return total;
  }
};

}  //结束匿名命名空间

Cache* NewLRUCache(size_t capacity) {
  return new ShardedLRUCache(capacity);
}

}  //命名空间级别数据库
