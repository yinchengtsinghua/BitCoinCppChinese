
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

#include <string.h>
#include "util/coding.h"
#include "util/hash.h"

//fall through_intended宏可用于注释隐式fall through
//在开关标签之间。真正的定义应该在外部提供。
//此版本是不支持的编译器的回退版本。
#ifndef FALLTHROUGH_INTENDED
#define FALLTHROUGH_INTENDED do { } while (0)
#endif

namespace leveldb {

uint32_t Hash(const char* data, size_t n, uint32_t seed) {
//类似于杂音杂音杂音
  const uint32_t m = 0xc6a4a793;
  const uint32_t r = 24;
  const char* limit = data + n;
  uint32_t h = seed ^ (n * m);

//一次接收四个字节
  while (data + 4 <= limit) {
    uint32_t w = DecodeFixed32(data);
    data += 4;
    h += w;
    h *= m;
    h ^= (h >> 16);
  }

//提取剩余字节
  switch (limit - data) {
    case 3:
      h += static_cast<unsigned char>(data[2]) << 16;
      FALLTHROUGH_INTENDED;
    case 2:
      h += static_cast<unsigned char>(data[1]) << 8;
      FALLTHROUGH_INTENDED;
    case 1:
      h += static_cast<unsigned char>(data[0]);
      h *= m;
      h ^= (h >> r);
      break;
  }
  return h;
}


}  //命名空间级别数据库
