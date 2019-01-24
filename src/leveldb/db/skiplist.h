
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

#ifndef STORAGE_LEVELDB_DB_SKIPLIST_H_
#define STORAGE_LEVELDB_DB_SKIPLIST_H_

//线程安全性
//--------------
//
//写操作需要外部同步，很可能是互斥体。
//阅读要求保证滑雪运动员不会被摧毁
//在读取过程中。除此之外，读取进度
//没有任何内部锁定或同步。
//
//Invariants：
//
//（1）在skiplist
//摧毁。因为我们
//从不删除任何跳过列表节点。
//
//（2）除了下一个/上一个指针外，节点的内容是
//节点链接到Skiplist后不可变。
//只有insert（）可以修改列表，并且要小心初始化
//一个节点，并使用发布存储在一个或
//更多的列表。
//
//…上一个与下一个指针排序…

#include <assert.h>
#include <stdlib.h>
#include "port/port.h"
#include "util/arena.h"
#include "util/random.h"

namespace leveldb {

class Arena;

template<typename Key, class Comparator>
class SkipList {
 private:
  struct Node;

 public:
//创建一个新的skiplist对象，该对象将使用“cmp”比较键，
//并将使用“*竞技场”分配内存。竞技场中分配的对象
//必须在SkipList对象的生存期内保持分配状态。
  explicit SkipList(Comparator cmp, Arena* arena);

//在列表中插入键。
//要求：列表中当前没有与键比较的内容。
  void Insert(const Key& key);

//返回true iff列表中比较等于key的条目。
  bool Contains(const Key& key) const;

//跳过列表内容的迭代
  class Iterator {
   public:
//在指定列表上初始化迭代器。
//返回的迭代器无效。
    explicit Iterator(const SkipList* list);

//如果迭代器位于有效节点，则返回true。
    bool Valid() const;

//返回当前位置的键。
//要求：有效（）
    const Key& key() const;

//前进到下一个位置。
//要求：有效（）
    void Next();

//前进到上一个位置。
//要求：有效（）
    void Prev();

//使用大于等于target的键前进到第一个条目
    void Seek(const Key& target);

//在列表中的第一个条目处定位。
//迭代器的最终状态为valid（）iff list不为空。
    void SeekToFirst();

//在列表中的最后一个条目处定位。
//迭代器的最终状态为valid（）iff list不为空。
    void SeekToLast();

   private:
    const SkipList* list_;
    Node* node_;
//有意复制
  };

 private:
  enum { kMaxHeight = 12 };

//构造后不变
  Comparator const compare_;
Arena* const arena_;    //用于节点分配的竞技场

  Node* const head_;

//仅由insert（）修改。被读者种族歧视，但陈旧
//值是可以的。
port::AtomicPointer max_height_;   //整个列表的高度

  inline int GetMaxHeight() const {
    return static_cast<int>(
        reinterpret_cast<intptr_t>(max_height_.NoBarrier_Load()));
  }

//只能通过insert（）读取/写入。
  Random rnd_;

  Node* NewNode(const Key& key, int height);
  int RandomHeight();
  bool Equal(const Key& a, const Key& b) const { return (compare_(a, b) == 0); }

//如果键大于“n”中存储的数据，则返回“真”
  bool KeyIsAfterNode(const Key& key, Node* n) const;

//返回键或键之后出现的最早节点。
//如果没有此类节点，则返回空值。
//
//如果prev非空，则用指向上一个的指针填充prev[级别]
//在[0..max_height_uu1]中的每个级别的“级别”节点。
  Node* FindGreaterOrEqual(const Key& key, Node** prev) const;

//返回键<键的最新节点。
//如果没有这样的节点，返回head。
  Node* FindLessThan(const Key& key) const;

//返回列表中的最后一个节点。
//如果列表为空，则返回head。
  Node* FindLast() const;

//不允许复制
  SkipList(const SkipList&);
  void operator=(const SkipList&);
};

//实施细节如下
template<typename Key, class Comparator>
struct SkipList<Key,Comparator>::Node {
  explicit Node(const Key& k) : key(k) { }

  Key const key;

//链接的访问器/转换器。用方法包装，这样我们可以
//必要时添加适当的屏障。
  Node* Next(int n) {
    assert(n >= 0);
//使用“获取加载”，以便我们观察完全初始化的
//返回节点的版本。
    return reinterpret_cast<Node*>(next_[n].Acquire_Load());
  }
  void SetNext(int n, Node* x) {
    assert(n >= 0);
//使用“发布商店”，以便任何阅读此内容的人
//指针观察插入节点的完全初始化版本。
    next_[n].Release_Store(x);
  }

//在一些地方没有可以安全使用的屏障变体。
  Node* NoBarrier_Next(int n) {
    assert(n >= 0);
    return reinterpret_cast<Node*>(next_[n].NoBarrier_Load());
  }
  void NoBarrier_SetNext(int n, Node* x) {
    assert(n >= 0);
    next_[n].NoBarrier_Store(x);
  }

 private:
//长度等于节点高度的数组。下一个“0”是最低级别的链接。
  port::AtomicPointer next_[1];
};

template<typename Key, class Comparator>
typename SkipList<Key,Comparator>::Node*
SkipList<Key,Comparator>::NewNode(const Key& key, int height) {
  char* mem = arena_->AllocateAligned(
      sizeof(Node) + sizeof(port::AtomicPointer) * (height - 1));
  return new (mem) Node(key);
}

template<typename Key, class Comparator>
inline SkipList<Key,Comparator>::Iterator::Iterator(const SkipList* list) {
  list_ = list;
  node_ = NULL;
}

template<typename Key, class Comparator>
inline bool SkipList<Key,Comparator>::Iterator::Valid() const {
  return node_ != NULL;
}

template<typename Key, class Comparator>
inline const Key& SkipList<Key,Comparator>::Iterator::key() const {
  assert(Valid());
  return node_->key;
}

template<typename Key, class Comparator>
inline void SkipList<Key,Comparator>::Iterator::Next() {
  assert(Valid());
  node_ = node_->Next(0);
}

template<typename Key, class Comparator>
inline void SkipList<Key,Comparator>::Iterator::Prev() {
//我们不使用显式的“prev”链接，只搜索
//落在键之前的最后一个节点。
  assert(Valid());
  node_ = list_->FindLessThan(node_->key);
  if (node_ == list_->head_) {
    node_ = NULL;
  }
}

template<typename Key, class Comparator>
inline void SkipList<Key,Comparator>::Iterator::Seek(const Key& target) {
  node_ = list_->FindGreaterOrEqual(target, NULL);
}

template<typename Key, class Comparator>
inline void SkipList<Key,Comparator>::Iterator::SeekToFirst() {
  node_ = list_->head_->Next(0);
}

template<typename Key, class Comparator>
inline void SkipList<Key,Comparator>::Iterator::SeekToLast() {
  node_ = list_->FindLast();
  if (node_ == list_->head_) {
    node_ = NULL;
  }
}

template<typename Key, class Comparator>
int SkipList<Key,Comparator>::RandomHeight() {
//以Kbranching的概率1增加高度
  static const unsigned int kBranching = 4;
  int height = 1;
  while (height < kMaxHeight && ((rnd_.Next() % kBranching) == 0)) {
    height++;
  }
  assert(height > 0);
  assert(height <= kMaxHeight);
  return height;
}

template<typename Key, class Comparator>
bool SkipList<Key,Comparator>::KeyIsAfterNode(const Key& key, Node* n) const {
//空n被视为无穷大
  return (n != NULL) && (compare_(n->key, key) < 0);
}

template<typename Key, class Comparator>
typename SkipList<Key,Comparator>::Node* SkipList<Key,Comparator>::FindGreaterOrEqual(const Key& key, Node** prev)
    const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    Node* next = x->Next(level);
    if (KeyIsAfterNode(key, next)) {
//继续在此列表中搜索
      x = next;
    } else {
      if (prev != NULL) prev[level] = x;
      if (level == 0) {
        return next;
      } else {
//切换到下一个列表
        level--;
      }
    }
  }
}

template<typename Key, class Comparator>
typename SkipList<Key,Comparator>::Node*
SkipList<Key,Comparator>::FindLessThan(const Key& key) const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    assert(x == head_ || compare_(x->key, key) < 0);
    Node* next = x->Next(level);
    if (next == NULL || compare_(next->key, key) >= 0) {
      if (level == 0) {
        return x;
      } else {
//切换到下一个列表
        level--;
      }
    } else {
      x = next;
    }
  }
}

template<typename Key, class Comparator>
typename SkipList<Key,Comparator>::Node* SkipList<Key,Comparator>::FindLast()
    const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    Node* next = x->Next(level);
    if (next == NULL) {
      if (level == 0) {
        return x;
      } else {
//切换到下一个列表
        level--;
      }
    } else {
      x = next;
    }
  }
}

template<typename Key, class Comparator>
SkipList<Key,Comparator>::SkipList(Comparator cmp, Arena* arena)
    : compare_(cmp),
      arena_(arena),
      /*d_uu（newnode（0/*任何键都可以执行*/，kmaxheight）），
      最大高度u（reinterpret_cast<void**（1）），
      RNDUUU（0xDeadBeef）
  对于（int i=0；i<kmaxheight；i++）
    head_u->setnext（i，空）；
  }
}

template<typename key，class comparator>
void skiplist<key，comparator>：：insert（const key&key）
  //todo（opt）：我们可以使用findCreaterRequal（）的无障碍变体
  //这里是因为insert（）是外部同步的。
  节点*上一个[kmaxheight]；
  node*x=findCreaterRequal（key，prev）；

  //我们的数据结构不允许重复插入
  断言（x==null！等于（key，x->key））；

  int height=randomheight（）；
  if（height>getmaxheight（））
    对于（int i=getmaxheight（）；i<height；i++）
      prev[i]=头部
    }
    //fprintf（stderr，“将高度从%d更改为%d\n”，max_height_uu height）；

    //可以在不同步的情况下改变最大高度
    //同时读卡器。同时观察
    //max_height的新值将看到
    //新的级别指针来自head_uu（空），或在
    //下面的循环。在前一种情况下，读者会
    //立即下降到下一个级别，因为毕竟是空排序
    //键。在后一种情况下，读卡器将使用新节点。
    最大_height_u.nobarrier_store（reinterpret_cast<void*>（height））；
  }

  x=新节点（键，高度）；
  对于（int i=0；i<height；i++）
    //nobarrier_setnext（）足够了，因为我们将在
    //我们在prev[i]中发布一个指向“x”的指针。
    x->nobarrier_setnext（i，prev[i]->nobarrier_next（i））；
    上一个[i]->setnext（i，x）；
  }
}

template<typename key，class comparator>
bool skiplist<key，comparator>：：contains（const key&key）const_
  node*x=findCreaterRequal（key，空）；
  如果（X）！=空&&equal（key，x->key））
    回归真实；
  }否则{
    返回错误；
  }
}

//命名空间级别db

endif//存储级别db_db_skiplist_h_
