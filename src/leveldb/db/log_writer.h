
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

#ifndef STORAGE_LEVELDB_DB_LOG_WRITER_H_
#define STORAGE_LEVELDB_DB_LOG_WRITER_H_

#include <stdint.h>
#include "db/log_format.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {

class WritableFile;

namespace log {

class Writer {
 public:
//创建一个将数据附加到“*dest”的写入程序。
//“*dest”最初必须为空。
//“*dest”在使用此编写器时必须保持活动状态。
  explicit Writer(WritableFile* dest);

//创建一个将数据附加到“*dest”的写入程序。
//“*dest”必须具有初始长度“dest\u length”。
//“*dest”在使用此编写器时必须保持活动状态。
  Writer(WritableFile* dest, uint64_t dest_length);

  ~Writer();

  Status AddRecord(const Slice& slice);

 private:
  WritableFile* dest_;
int block_offset_;       //块中的当前偏移量

//所有支持的记录类型的CRC32C值。这些是
//预先计算以减少计算
//标题中存储的记录类型。
  uint32_t type_crc_[kMaxRecordType + 1];

  Status EmitPhysicalRecord(RecordType type, const char* ptr, size_t length);

//不允许复制
  Writer(const Writer&);
  void operator=(const Writer&);
};

}  //命名空间日志
}  //命名空间级别数据库

#endif  //存储级别数据库日志写入程序
