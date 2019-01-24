
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
//
//线程安全（提供内部同步）

#ifndef STORAGE_LEVELDB_DB_TABLE_CACHE_H_
#define STORAGE_LEVELDB_DB_TABLE_CACHE_H_

#include <string>
#include <stdint.h>
#include "db/dbformat.h"
#include "leveldb/cache.h"
#include "leveldb/table.h"
#include "port/port.h"

namespace leveldb {

class Env;

class TableCache {
 public:
  TableCache(const std::string& dbname, const Options* options, int entries);
  ~TableCache();

//返回指定文件号的迭代器（对应的
//文件长度必须正好是“文件大小”字节）。如果“tableptr”是
//非空，还将“*tableptr”设置为指向表对象
//在返回的迭代器的基础上，如果没有表对象作为基础，则为空
//返回的迭代器。返回的“*tableptr”对象归
//缓存和不应被删除，并且只要
//返回的迭代器是活动的。
  Iterator* NewIterator(const ReadOptions& options,
                        uint64_t file_number,
                        uint64_t file_size,
                        Table** tableptr = NULL);

//如果在指定文件中查找内部键“k”找到一个条目，
//调用（*handle_result）（arg，found_key，found_value）。
  Status Get(const ReadOptions& options,
             uint64_t file_number,
             uint64_t file_size,
             const Slice& k,
             void* arg,
             void (*handle_result)(void*, const Slice&, const Slice&));

//逐出指定文件号的任何条目
  void Evict(uint64_t file_number);

 private:
  Env* const env_;
  const std::string dbname_;
  const Options* options_;
  Cache* cache_;

  Status FindTable(uint64_t file_number, uint64_t file_size, Cache::Handle**);
};

}  //命名空间级别数据库

#endif  //存储级别数据库表缓存
