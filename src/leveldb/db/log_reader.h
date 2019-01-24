
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

#ifndef STORAGE_LEVELDB_DB_LOG_READER_H_
#define STORAGE_LEVELDB_DB_LOG_READER_H_

#include <stdint.h>

#include "db/log_format.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {

class SequentialFile;

namespace log {

class Reader {
 public:
//用于报告错误的接口。
  class Reporter {
   public:
    virtual ~Reporter();

//检测到一些损坏。”尺寸”是大概的数字
//由于损坏而丢弃的字节数。
    virtual void Corruption(size_t bytes, const Status& status) = 0;
  };

//创建将从“*文件”返回日志记录的读卡器。
//在使用此读卡器时，“*文件”必须保持活动状态。
//
//如果“reporter”不为空，则每当有数据
//由于检测到损坏而丢弃。“*记者“必须留下
//在使用此阅读器时进行直播。
//
//如果“校验和”为真，请验证校验和（如果可用）。
//
//读卡器将从物理位置的第一个记录开始读取
//位置>=文件中的初始偏移量。
  Reader(SequentialFile* file, Reporter* reporter, bool checksum,
         uint64_t initial_offset);

  ~Reader();

//将下一个记录读取到*记录中。如果读取，则返回true
//成功，如果我们到达输入的末尾，则返回false。可以使用
//“刮擦”作为临时存储。*记录中填写的内容
//只有在对其执行下一个变异操作之前有效
//读写器或下一个变异到*刮痕。
  bool ReadRecord(Slice* record, std::string* scratch);

//返回readrecord返回的最后一条记录的物理偏移量。
//
//在第一次调用readrecord之前未定义。
  uint64_t LastRecordOffset();

 private:
  SequentialFile* const file_;
  Reporter* const reporter_;
  bool const checksum_;
  char* const backing_store_;
  Slice buffer_;
bool eof_;   //最后一次读取（）通过返回<kblockSize指示EOF

//readrecord返回的最后一条记录的偏移量。
  uint64_t last_record_offset_;
//超过缓冲区末尾的第一个位置的偏移量。
  uint64_t end_of_buffer_offset_;

//开始查找要返回的第一条记录的偏移量
  uint64_t const initial_offset_;

//如果在搜索后重新同步（初始偏移量>0），则为真。在
//尤其是，运行kmiddletype和klasttype记录时，可以静默
//在此模式下跳过
  bool resyncing_;

//使用以下特殊值扩展记录类型
  enum {
    kEof = kMaxRecordType + 1,
//当我们发现无效的物理记录时返回。
//目前有三种情况会发生这种情况：
//*记录的CRC无效（readphysicalrecord报告丢弃）
//*记录长度为0（不报告下降）
//*记录低于构造函数的初始偏移量（不报告下降）
    kBadRecord = kMaxRecordType + 2
  };

//跳过“初始偏移”之前的所有块。
//
//成功时返回true。处理报告。
  bool SkipToInitialBlock();

//返回类型，或前面的一个特殊值
  unsigned int ReadPhysicalRecord(Slice* result);

//报告向报告者丢弃了字节。
//必须更新缓冲区，以便在调用之前删除掉的字节。
  void ReportCorruption(uint64_t bytes, const char* reason);
  void ReportDrop(uint64_t bytes, const Status& reason);

//不允许复制
  Reader(const Reader&);
  void operator=(const Reader&);
};

}  //命名空间日志
}  //命名空间级别数据库

#endif  //存储级别数据库日志阅读器
