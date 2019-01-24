
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

#ifndef STORAGE_LEVELDB_INCLUDE_TABLE_H_
#define STORAGE_LEVELDB_INCLUDE_TABLE_H_

#include <stdint.h>
#include "leveldb/iterator.h"

namespace leveldb {

class Block;
class BlockHandle;
class Footer;
struct Options;
class RandomAccessFile;
struct ReadOptions;
class TableCache;

//表是从字符串到字符串的排序映射。表是
//不变的和持久的。可以从中安全地访问表
//多个线程没有外部同步。
class Table {
 public:
//尝试打开以字节[0..file_size]存储的表
//并读取允许
//正在从表中检索数据。
//
//如果成功，返回OK并将“*table”设置为新打开的
//表。当不再需要时，客户机应该删除“*table”。
//如果在初始化表时出错，则设置“*表”
//返回非OK状态。不拥有
//“*源”，但客户端必须确保“源”保持活动状态
//在返回表的生存期内。
//
//*使用此表时，文件必须保持活动状态。
  static Status Open(const Options& options,
                     RandomAccessFile* file,
                     uint64_t file_size,
                     Table** table);

  ~Table();

//返回表内容的新迭代器。
//newIterator（）的结果最初无效（调用方必须
//在使用迭代器之前调用其中一个seek方法）。
  Iterator* NewIterator(const ReadOptions&) const;

//给定一个键，返回文件中的一个近似字节偏移量，其中
//该键的数据将开始（如果该键是
//存在于文件中）。返回值以文件形式表示
//字节，因此包括压缩基础数据等效果。
//例如，表中最后一个键的近似偏移量将
//接近文件长度。
  uint64_t ApproximateOffsetOf(const Slice& key) const;

 private:
  struct Rep;
  Rep* rep_;

  explicit Table(Rep* rep) { rep_ = rep; }
  static Iterator* BlockReader(void*, const ReadOptions&, const Slice&);

//调用（*handle_result）（arg，…）并在调用后找到条目
//寻找（钥匙）如果筛选策略说
//那把钥匙不存在。
  friend class TableCache;
  Status InternalGet(
      const ReadOptions&, const Slice& key,
      void* arg,
      void (*handle_result)(void* arg, const Slice& k, const Slice& v));


  void ReadMeta(const Footer& footer);
  void ReadFilter(const Slice& filter_handle_value);

//不允许复制
  Table(const Table&);
  void operator=(const Table&);
};

}  //命名空间级别数据库

#endif  //存储级别包括表
