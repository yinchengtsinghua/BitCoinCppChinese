
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

#ifndef STORAGE_LEVELDB_INCLUDE_DB_H_
#define STORAGE_LEVELDB_INCLUDE_DB_H_

#include <stdint.h>
#include <stdio.h>
#include "leveldb/iterator.h"
#include "leveldb/options.h"

namespace leveldb {

//如果更改这些，请更新makefile
static const int kMajorVersion = 1;
static const int kMinorVersion = 20;

struct Options;
struct ReadOptions;
struct WriteOptions;
class WriteBatch;

//数据库特定状态的抽象句柄。
//快照是不可变的对象，因此可以安全地
//在没有任何外部同步的情况下从多个线程访问。
class Snapshot {
 protected:
  virtual ~Snapshot();
};

//一串钥匙
struct Range {
Slice start;          //包括在范围内
Slice limit;          //不包括在范围内

  Range() { }
  Range(const Slice& s, const Slice& l) : start(s), limit(l) { }
};

//数据库是从键到值的持久有序映射。
//数据库对于来自多个线程的并发访问是安全的，没有
//任何外部同步。
class DB {
 public:
//用指定的“名称”打开数据库。
//将指向堆分配数据库的指针存储在*dbptr中并返回
//成功就好了。
//将空值存储在*dbptr中，并在出错时返回非OK状态。
//当不再需要*dbptr时，调用方应将其删除。
  static Status Open(const Options& options,
                     const std::string& name,
                     DB** dbptr);

  DB() { }
  virtual ~DB();

//将“key”的数据库项设置为“value”。成功后返回OK，
//以及出错时的非OK状态。
//注意：考虑设置options.sync=true。
  virtual Status Put(const WriteOptions& options,
                     const Slice& key,
                     const Slice& value) = 0;

//删除“key”的数据库条目（如果有）。返回OK
//成功，错误时为非OK状态。“key”不是一个错误
//数据库中不存在。
//注意：考虑设置options.sync=true。
  virtual Status Delete(const WriteOptions& options, const Slice& key) = 0;

//将指定的更新应用于数据库。
//成功时返回OK，失败时返回非OK。
//注意：考虑设置options.sync=true。
  virtual Status Write(const WriteOptions& options, WriteBatch* updates) = 0;

//如果数据库包含“key”的条目，则存储
//*值中对应的值，返回OK。
//
//如果没有输入“key”，则保持*值不变并返回
//状态：：isNotFound（）返回true的状态。
//
//可能返回错误的其他状态。
  virtual Status Get(const ReadOptions& options,
                     const Slice& key, std::string* value) = 0;

//返回对数据库内容分配的堆迭代器。
//newIterator（）的结果最初无效（调用方必须
//在使用迭代器之前调用其中一个seek方法）。
//
//当不再需要迭代器时，调用方应该删除它。
//在删除此数据库之前，应删除返回的迭代器。
  virtual Iterator* NewIterator(const ReadOptions& options) = 0;

//将句柄返回到当前数据库状态。使用创建的迭代器
//此句柄将观察当前数据库的稳定快照。
//状态。调用方必须在
//不再需要快照。
  virtual const Snapshot* GetSnapshot() = 0;

//释放以前获取的快照。呼叫方不得
//此调用后使用“快照”。
  virtual void ReleaseSnapshot(const Snapshot* snapshot) = 0;

//DB实现可以导出关于其状态的属性
//通过这种方法。如果“属性”是此理解的有效属性
//DB实现，用其当前值填充“*value”，并返回
//真的。否则返回false。
//
//
//有效的属性名称包括：
//
//“leveldb.num files at level<n>”-返回level<n>的文件数，
//其中，<n>是级别编号的ASCII表示（例如“0”）。
//“leveldb.stats”-返回描述统计信息的多行字符串
//关于数据库的内部操作。
//“leveldb.sstables”-返回描述所有
//组成数据库内容的sstables。
//“leveldb.approximate memory usage”-返回
//数据库使用的内存字节。
  virtual bool GetProperty(const Slice& property, std::string* value) = 0;

//对于[0，n-1]中的每个i，存储在“大小[i]”，大约
//“[范围[i].中的键使用的文件系统空间。开始..范围[i].限制）”。
//
//请注意，返回的大小度量文件系统空间使用情况，因此
//如果用户数据压缩了10倍，则返回
//大小将是相应用户数据大小的十分之一。
//
//结果可能不包括最近写入数据的大小。
  virtual void GetApproximateSizes(const Range* range, int n,
                                   uint64_t* sizes) = 0;

//压缩密钥范围的底层存储[*开始，*结束]。
//尤其是，删除和覆盖的版本被丢弃，
//重新排列数据以降低运营成本
//需要访问数据。此操作通常只应
//由了解底层实现的用户调用。
//
//begin==null被视为数据库中所有键之前的键。
//end==null在数据库中的所有键之后被视为键。
//因此，以下调用将压缩整个数据库：
//db->compactrange（空，空）；
  virtual void CompactRange(const Slice* begin, const Slice* end) = 0;

 private:
//不允许复制
  DB(const DB&);
  void operator=(const DB&);
};

//销毁指定数据库的内容。
//使用这种方法要非常小心。
Status DestroyDB(const std::string& name, const Options& options);

//如果无法打开数据库，则可以尝试将此方法调用为
//恢复尽可能多的数据库内容。
//某些数据可能丢失，因此在调用此函数时要小心
//在包含重要信息的数据库上。
Status RepairDB(const std::string& dbname, const Options& options);

}  //命名空间级别数据库

#endif  //存储级别包括
