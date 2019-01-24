
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

#ifndef STORAGE_LEVELDB_UTIL_ARENA_H_
#define STORAGE_LEVELDB_UTIL_ARENA_H_

#include <vector>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "port/port.h"

namespace leveldb {

class Arena {
 public:
  Arena();
  ~Arena();

//返回指向新分配的“字节”字节内存块的指针。
  char* Allocate(size_t bytes);

//使用malloc提供的正常对齐保证分配内存
  char* AllocateAligned(size_t bytes);

//返回已分配数据的总内存使用率估计值
//在竞技场上。
  size_t MemoryUsage() const {
    return reinterpret_cast<uintptr_t>(memory_usage_.NoBarrier_Load());
  }

 private:
  char* AllocateFallback(size_t bytes);
  char* AllocateNewBlock(size_t block_bytes);

//分配状态
  char* alloc_ptr_;
  size_t alloc_bytes_remaining_;

//新分配的[]内存块数组
  std::vector<char*> blocks_;

//竞技场的总内存使用量。
  port::AtomicPointer memory_usage_;

//不允许复制
  Arena(const Arena&);
  void operator=(const Arena&);
};

inline char* Arena::Allocate(size_t bytes) {
//如果我们允许，返回内容的语义有点混乱
//0字节分配，因此我们不允许在这里使用它们（我们不需要
//供我们内部使用）。
  assert(bytes > 0);
  if (bytes <= alloc_bytes_remaining_) {
    char* result = alloc_ptr_;
    alloc_ptr_ += bytes;
    alloc_bytes_remaining_ -= bytes;
    return result;
  }
  return AllocateFallback(bytes);
}

}  //命名空间级别数据库

#endif  //储存水平
