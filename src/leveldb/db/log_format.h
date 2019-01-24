
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
//读写器共享的日志格式信息。
//有关详细信息，请参阅../doc/log_format.md。

#ifndef STORAGE_LEVELDB_DB_LOG_FORMAT_H_
#define STORAGE_LEVELDB_DB_LOG_FORMAT_H_

namespace leveldb {
namespace log {

enum RecordType {
//零是为预分配文件保留的
  kZeroType = 0,

  kFullType = 1,

//为碎片
  kFirstType = 2,
  kMiddleType = 3,
  kLastType = 4
};
static const int kMaxRecordType = kLastType;

static const int kBlockSize = 32768;

//头是校验和（4字节）、长度（2字节）、类型（1字节）。
static const int kHeaderSize = 4 + 2 + 1;

}  //命名空间日志
}  //命名空间级别数据库

#endif  //存储级别数据库日志格式
