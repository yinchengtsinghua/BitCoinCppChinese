
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

#ifndef STORAGE_LEVELDB_TABLE_FORMAT_H_
#define STORAGE_LEVELDB_TABLE_FORMAT_H_

#include <string>
#include <stdint.h>
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "leveldb/table_builder.h"

namespace leveldb {

class Block;
class RandomAccessFile;
struct ReadOptions;

//blockhandle是指向存储数据的文件范围的指针。
//块或元块。
class BlockHandle {
 public:
  BlockHandle();

//文件中块的偏移量。
  uint64_t offset() const { return offset_; }
  void set_offset(uint64_t offset) { offset_ = offset; }

//存储块的大小
  uint64_t size() const { return size_; }
  void set_size(uint64_t size) { size_ = size; }

  void EncodeTo(std::string* dst) const;
  Status DecodeFrom(Slice* input);

//块句柄的最大编码长度
  enum { kMaxEncodedLength = 10 + 10 };

 private:
  uint64_t offset_;
  uint64_t size_;
};

//页脚封装存储在尾部的固定信息
//每个表文件的结尾。
class Footer {
 public:
  Footer() { }

//表的metaindex块的块句柄
  const BlockHandle& metaindex_handle() const { return metaindex_handle_; }
  void set_metaindex_handle(const BlockHandle& h) { metaindex_handle_ = h; }

//表的索引块的块句柄
  const BlockHandle& index_handle() const {
    return index_handle_;
  }
  void set_index_handle(const BlockHandle& h) {
    index_handle_ = h;
  }

  void EncodeTo(std::string* dst) const;
  Status DecodeFrom(Slice* input);

//页脚的编码长度。请注意，对
//页脚总是占用这么多字节。它包括
//两个块句柄和一个幻数。
  enum {
    kEncodedLength = 2*BlockHandle::kMaxEncodedLength + 8
  };

 private:
  BlockHandle metaindex_handle_;
  BlockHandle index_handle_;
};

//kTableMagicNumber是通过运行选择的
//回音http://code.google.com/p/leveldb/sha1sum
//取前导的64位。
static const uint64_t kTableMagicNumber = 0xdb4775248b80fb57ull;

//1字节类型+32位CRC
static const size_t kBlockTrailerSize = 5;

struct BlockContents {
Slice data;           //实际数据内容
bool cachable;        //可以缓存真正的iff数据
bool heap_allocated;  //如果为true，则调用方应删除[]数据。数据（）。
};

//从“文件”中读取由“句柄”标识的块。关于失败
//返回非OK。成功后填写*结果并返回OK。
extern Status ReadBlock(RandomAccessFile* file,
                        const ReadOptions& options,
                        const BlockHandle& handle,
                        BlockContents* result);

//实施细节如下。客户应忽略，

inline BlockHandle::BlockHandle()
    : offset_(~static_cast<uint64_t>(0)),
      size_(~static_cast<uint64_t>(0)) {
}

}  //命名空间级别数据库

#endif  //存储级别表格式
