
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
//TableBuilder提供用于生成表的接口
//（从键到值的不可变和排序映射）。
//
//多个线程可以在TableBuilder上调用const方法，而不需要
//外部同步，但如果任何线程可以调用
//非常量方法，访问同一TableBuilder的所有线程都必须使用
//外部同步。

#ifndef STORAGE_LEVELDB_INCLUDE_TABLE_BUILDER_H_
#define STORAGE_LEVELDB_INCLUDE_TABLE_BUILDER_H_

#include <stdint.h>
#include "leveldb/options.h"
#include "leveldb/status.h"

namespace leveldb {

class BlockBuilder;
class BlockHandle;
class WritableFile;

class TableBuilder {
 public:
//创建一个将存储其所属表内容的生成器
//在*文件中生成。不关闭文件。这取决于
//调用方在调用finish（）后关闭文件。
  TableBuilder(const Options& options, WritableFile* file);

//要求：已调用finish（）或放弃（）。
  ~TableBuilder();

//更改此生成器使用的选项。注：只有部分
//选项字段可以在构造后更改。如果字段是
//不允许动态更改及其在结构中的值
//传递给构造函数的值与
//结构传递给此方法，此方法将返回一个错误
//不更改任何字段。
  Status ChangeOptions(const Options& options);

//向正在构造的表添加键和值。
//要求：根据比较器，键在任何先前添加的键之后。
//要求：finish（），放弃（）尚未调用
  void Add(const Slice& key, const Slice& value);

//高级操作：将任何缓冲的键/值对刷新到文件。
//可用于确保两个相邻条目永远不存在
//相同的数据块。大多数客户机不需要使用此方法。
//要求：finish（），放弃（）尚未调用
  void Flush();

//如果检测到一些错误，返回非OK。
  Status status() const;

//把桌子盖好。停止使用传递给
//此函数后的构造函数返回。
//要求：finish（），放弃（）尚未调用
  Status Finish();

//指示应放弃此生成器的内容。停止
//使用此函数返回后传递给构造函数的文件。
//如果调用者不想调用finish（），它必须调用放弃（）。
//在摧毁这个建造者之前。
//要求：finish（），放弃（）尚未调用
  void Abandon();

//到目前为止，要添加的调用数（）。
  uint64_t NumEntries() const;

//到目前为止生成的文件的大小。如果在成功后调用
//finish（）调用，返回最终生成文件的大小。
  uint64_t FileSize() const;

 private:
  bool ok() const { return status().ok(); }
  void WriteBlock(BlockBuilder* block, BlockHandle* handle);
  void WriteRawBlock(const Slice& data, CompressionType, BlockHandle* handle);

  struct Rep;
  Rep* rep_;

//不允许复制
  TableBuilder(const TableBuilder&);
  void operator=(const TableBuilder&);
};

}  //命名空间级别数据库

#endif  //存储级别包括表生成器
