
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
//Endian中性编码：
//*固定长度的数字先用最低有效字节编码
//*此外，我们还支持可变长度“varint”编码
//*字符串以变量格式的长度作为前缀进行编码。

#ifndef STORAGE_LEVELDB_UTIL_CODING_H_
#define STORAGE_LEVELDB_UTIL_CODING_H_

#include <stdint.h>
#include <string.h>
#include <string>
#include "leveldb/slice.h"
#include "port/port.h"

namespace leveldb {

//标准投入…附加到字符串的例程
extern void PutFixed32(std::string* dst, uint32_t value);
extern void PutFixed64(std::string* dst, uint64_t value);
extern void PutVarint32(std::string* dst, uint32_t value);
extern void PutVarint64(std::string* dst, uint64_t value);
extern void PutLengthPrefixedSlice(std::string* dst, const Slice& value);

//标准获取…例程从切片的开头分析值
//并将切片推进到解析值之后。
extern bool GetVarint32(Slice* input, uint32_t* value);
extern bool GetVarint64(Slice* input, uint64_t* value);
extern bool GetLengthPrefixedSlice(Slice* input, Slice* result);

//基于指针的getvarint变量…它们要么存储一个值
//在*v中，返回一个指针，刚好超过解析值，或者返回
//出错时为空。这些例程只查看范围内的字节
//[P.LimIT-1 ]
extern const char* GetVarint32Ptr(const char* p,const char* limit, uint32_t* v);
extern const char* GetVarint64Ptr(const char* p,const char* limit, uint64_t* v);

//返回varint32或varint64编码“v”的长度
extern int VarintLength(uint64_t v);

//Put的低级版本…直接写入字符缓冲区的
//要求：DST有足够的空间用于写入值
extern void EncodeFixed32(char* dst, uint32_t value);
extern void EncodeFixed64(char* dst, uint64_t value);

//Put的低级版本…直接写入字符缓冲区的
//返回一个指针，刚好超过最后一个写入的字节。
//要求：DST有足够的空间用于写入值
extern char* EncodeVarint32(char* dst, uint32_t value);
extern char* EncodeVarint64(char* dst, uint64_t value);

//GeT的低级版本…直接从字符缓冲区读取的
//没有任何边界检查。

inline uint32_t DecodeFixed32(const char* ptr) {
  if (port::kLittleEndian) {
//加载原始字节
    uint32_t result;
memcpy(&result, ptr, sizeof(result));  //GCC将其优化为普通负载
    return result;
  } else {
    return ((static_cast<uint32_t>(static_cast<unsigned char>(ptr[0])))
        | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[1])) << 8)
        | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[2])) << 16)
        | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[3])) << 24));
  }
}

inline uint64_t DecodeFixed64(const char* ptr) {
  if (port::kLittleEndian) {
//加载原始字节
    uint64_t result;
memcpy(&result, ptr, sizeof(result));  //GCC将其优化为普通负载
    return result;
  } else {
    uint64_t lo = DecodeFixed32(ptr);
    uint64_t hi = DecodeFixed32(ptr + 4);
    return (hi << 32) | lo;
  }
}

//供getvarint32ptr的回退路径使用的内部例程
extern const char* GetVarint32PtrFallback(const char* p,
                                          const char* limit,
                                          uint32_t* value);
inline const char* GetVarint32Ptr(const char* p,
                                  const char* limit,
                                  uint32_t* value) {
  if (p < limit) {
    uint32_t result = *(reinterpret_cast<const unsigned char*>(p));
    if ((result & 128) == 0) {
      *value = result;
      return p + 1;
    }
  }
  return GetVarint32PtrFallback(p, limit, value);
}

}  //命名空间级别数据库

#endif  //存储级别
