
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有2016 LevelDB作者。版权所有。
//此源代码的使用受可以
//在许可证文件中找到。有关参与者的名称，请参阅作者文件。
//
//CRC32C的可移植实现，优化处理
//一次四个字节。
//
//在单独的源文件中，允许加速的CRC32C函数
//使用适当的编译器标志编译以启用x86 SSE 4.2
//指令。

#include <stdint.h>
#include <string.h>
#include "port/port.h"

#if defined(LEVELDB_PLATFORM_POSIX_SSE)

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) && defined(__SSE4_2__)
#include <nmmintrin.h>
#endif

#endif  //已定义（级别数据库平台位置）

namespace leveldb {
namespace port {

#if defined(LEVELDB_PLATFORM_POSIX_SSE)

//用于以小尾数字节顺序获取自然对齐的32位字
static inline uint32_t LE_LOAD32(const uint8_t *p) {
//SSE仅限x86，因此确保了p_始终是小endian。
  uint32_t word;
  memcpy(&word, p, sizeof(word));
  return word;
}

#if defined(_M_X64) || defined(__x86_64__)  //Le_Load64仅用于x64。

//用于以小尾数字节顺序获取自然对齐的64位字
static inline uint64_t LE_LOAD64(const uint8_t *p) {
  uint64_t dword;
  memcpy(&dword, p, sizeof(dword));
  return dword;
}

#endif  //已定义（_m_x64）已定义（u x86_64_uuuu）

#endif  //已定义（级别数据库平台位置）

//如需进一步改进，请参阅英特尔出版物：
//http://download.intel.com/design/intarch/papers/323405.pdf下载
uint32_t AcceleratedCRC32C(uint32_t crc, const char* buf, size_t size) {
#if !defined(LEVELDB_PLATFORM_POSIX_SSE)
  return 0;
#else

  const uint8_t *p = reinterpret_cast<const uint8_t *>(buf);
  const uint8_t *e = p + size;
  uint32_t l = crc ^ 0xffffffffu;

#define STEP1 do {                              \
    l = _mm_crc32_u8(l, *p++);                  \
} while (0)
#define STEP4 do {                              \
    l = _mm_crc32_u32(l, LE_LOAD32(p));         \
    p += 4;                                     \
} while (0)
#define STEP8 do {                              \
    l = _mm_crc32_u64(l, LE_LOAD64(p));         \
    p += 8;                                     \
} while (0)

  if (size > 16) {
//处理未对齐的字节
    for (unsigned int i = reinterpret_cast<uintptr_t>(p) % 8; i; --i) {
      STEP1;
    }

//_mm_crc32_u64仅在x64上可用。
#if defined(_M_X64) || defined(__x86_64__)
//一次处理8个字节
    while ((e-p) >= 8) {
      STEP8;
    }
//一次处理4个字节
    if ((e-p) >= 4) {
      STEP4;
    }
#else  //！（已定义（_m_x64）已定义（u x86_64_uuuuu））。
//一次处理4个字节
    while ((e-p) >= 4) {
      STEP4;
    }
#endif  //已定义（_m_x64）已定义（u x86_64_uuuu）
  }
//处理最后几个字节
  while (p != e) {
    STEP1;
  }
#undef STEP8
#undef STEP4
#undef STEP1
  return l ^ 0xffffffffu;
#endif  //已定义（级别数据库平台位置）
}

}  //命名空间端口
}  //命名空间级别数据库
