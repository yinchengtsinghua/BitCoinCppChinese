
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

#ifndef STORAGE_LEVELDB_DB_MEMTABLE_H_
#define STORAGE_LEVELDB_DB_MEMTABLE_H_

#include <string>
#include "leveldb/db.h"
#include "db/dbformat.h"
#include "db/skiplist.h"
#include "util/arena.h"

namespace leveldb {

class InternalKeyComparator;
class Mutex;
class MemTableIterator;

class MemTable {
 public:
//memtables被引用计数。初始参考计数
//为零，调用方必须至少调用一次ref（）。
  explicit MemTable(const InternalKeyComparator& comparator);

//增加引用计数。
  void Ref() { ++refs_; }

//删除引用计数。如果不存在其他引用，则删除。
  void Unref() {
    --refs_;
    assert(refs_ >= 0);
    if (refs_ <= 0) {
      delete this;
    }
  }

//返回由此使用的数据字节数的估计值
//数据结构。修改memtable时可以安全调用。
  size_t ApproximateMemoryUsage();

//返回生成memtable内容的迭代器。
//
//调用方必须确保基础memtable保持活动状态
//当返回的迭代器处于活动状态时。这个归还的钥匙
//迭代器是由AppendInternalKey在
//数据库/格式。H，CC模块。
  Iterator* NewIterator();

//在memtable中添加一个条目，该条目将键映射到
//指定的序列号和指定的类型。
//如果type==ktypedelection，通常值为空。
  void Add(SequenceNumber seq, ValueType type,
           const Slice& key,
           const Slice& value);

//如果memtable包含key的值，请将其存储在*value中并返回true。
//如果memtable包含键的删除，则存储notfound（）错误
//状态为*并返回“真”。
//否则，返回false。
  bool Get(const LookupKey& key, std::string* value, Status* s);

 private:
~MemTable();  //private，因为只应使用unref（）删除它

  struct KeyComparator {
    const InternalKeyComparator comparator;
    explicit KeyComparator(const InternalKeyComparator& c) : comparator(c) { }
    int operator()(const char* a, const char* b) const;
  };
  friend class MemTableIterator;
  friend class MemTableBackwardIterator;

  typedef SkipList<const char*, KeyComparator> Table;

  KeyComparator comparator_;
  int refs_;
  Arena arena_;
  Table table_;

//不允许复制
  MemTable(const MemTable&);
  void operator=(const MemTable&);
};

}  //命名空间级别数据库

#endif  //存储级别数据库内存表
