
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

#ifndef STORAGE_LEVELDB_UTIL_CRC32C_H_
#define STORAGE_LEVELDB_UTIL_CRC32C_H_

#include <stddef.h>
#include <stdint.h>

namespace leveldb {
namespace crc32c {

//返回concat的crc32c（a，data[0，n-1]），其中init_crc是
//一些字符串a.extend（）的crc32c通常用于维护
//数据流的CRC32C。
extern uint32_t Extend(uint32_t init_crc, const char* data, size_t n);

//返回数据的CRC32C[0，n-1]
inline uint32_t Value(const char* data, size_t n) {
  return Extend(0, data, n);
}

static const uint32_t kMaskDelta = 0xa282ead8ul;

//返回CRC的屏蔽表示。
//
//动机：计算一个字符串的CRC是有问题的
//包含嵌入的CRC。因此，我们建议存储CRC
//在存储之前，应该屏蔽某个地方（例如，在文件中）。
inline uint32_t Mask(uint32_t crc) {
//向右旋转15位并添加一个常量。
  return ((crc >> 15) | (crc << 17)) + kMaskDelta;
}

//返回其屏蔽表示被屏蔽的CRC。
inline uint32_t Unmask(uint32_t masked_crc) {
  uint32_t rot = masked_crc - kMaskDelta;
  return ((rot >> 17) | (rot << 15));
}

}  //命名空间CRC32C
}  //命名空间级别数据库

#endif  //存储级别
