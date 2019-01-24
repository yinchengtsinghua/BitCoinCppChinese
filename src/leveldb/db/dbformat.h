
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

#ifndef STORAGE_LEVELDB_DB_DBFORMAT_H_
#define STORAGE_LEVELDB_DB_DBFORMAT_H_

#include <stdio.h>
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/filter_policy.h"
#include "leveldb/slice.h"
#include "leveldb/table_builder.h"
#include "util/coding.h"
#include "util/logging.h"

namespace leveldb {

//常量分组。我们可以做一些
//通过选项设置参数。
namespace config {
static const int kNumLevels = 7;

//当我们点击这么多文件时，0级压缩就开始了。
static const int kL0_CompactionTrigger = 4;

//0级文件数的软限制。我们在这一点上放慢了写作速度。
static const int kL0_SlowdownWritesTrigger = 8;

//0级文件的最大数目。我们现在停止写作。
static const int kL0_StopWritesTrigger = 12;

//如果新的压缩MEMTABLE
//不创建重叠。我们试着推进到2级以避免
//相对昂贵的0级=>1次压实，并避免一些
//昂贵的清单文件操作。我们不会一直推到
//这是最大的一个级别，因为它会产生大量浪费的磁盘
//如果重复覆盖相同的密钥空间，则为空格。
static const int kMaxMemCompactLevel = 2;

//迭代期间读取的数据样本之间的大约字节间隙。
static const int kReadBytesPeriod = 1048576;

}  //命名空间配置

class InternalKey;

//值类型编码为内部键的最后一个组件。
//不要更改这些枚举值：它们嵌入在磁盘上
//数据结构。
enum ValueType {
  kTypeDeletion = 0x0,
  kTypeValue = 0x1
};
//kvaluetypeforseek定义当
//构造用于查找特定对象的ParsedinInternalKey对象
//序列号（因为我们按降序对序列号排序
//值类型嵌入为序列中的低8位
//在内部密钥中，我们需要使用最高编号的
//值类型，不是最低的）。
static const ValueType kValueTypeForSeek = kTypeValue;

typedef uint64_t SequenceNumber;

//我们在底部留有8个位，因此类型和序列
//可以打包成64位。
static const SequenceNumber kMaxSequenceNumber =
    ((0x1ull << 56) - 1);

struct ParsedInternalKey {
  Slice user_key;
  SequenceNumber sequence;
  ValueType type;

ParsedInternalKey() { }  //故意不初始化（用于速度）
  ParsedInternalKey(const Slice& u, const SequenceNumber& seq, ValueType t)
      : user_key(u), sequence(seq), type(t) { }
  std::string DebugString() const;
};

//返回“key”编码的长度。
inline size_t InternalKeyEncodingLength(const ParsedInternalKey& key) {
  return key.user_key.size() + 8;
}

//将“key”的序列化附加到*result。
extern void AppendInternalKey(std::string* result,
                              const ParsedInternalKey& key);

//尝试从“internal_key”分析内部密钥。关于成功，
//将解析后的数据存储在“*result”中，并返回true。
//
//出错时返回false，使“*result”处于未定义状态。
extern bool ParseInternalKey(const Slice& internal_key,
                             ParsedInternalKey* result);

//返回内部键的用户键部分。
inline Slice ExtractUserKey(const Slice& internal_key) {
  assert(internal_key.size() >= 8);
  return Slice(internal_key.data(), internal_key.size() - 8);
}

inline ValueType ExtractValueType(const Slice& internal_key) {
  assert(internal_key.size() >= 8);
  const size_t n = internal_key.size();
  uint64_t num = DecodeFixed64(internal_key.data() + n - 8);
  unsigned char c = num & 0xff;
  return static_cast<ValueType>(c);
}

//内部键的比较器，使用指定的比较器
//用户键部分，并通过减少序列号来断开连接。
class InternalKeyComparator : public Comparator {
 private:
  const Comparator* user_comparator_;
 public:
  explicit InternalKeyComparator(const Comparator* c) : user_comparator_(c) { }
  virtual const char* Name() const;
  virtual int Compare(const Slice& a, const Slice& b) const;
  virtual void FindShortestSeparator(
      std::string* start,
      const Slice& limit) const;
  virtual void FindShortSuccessor(std::string* key) const;

  const Comparator* user_comparator() const { return user_comparator_; }

  int Compare(const InternalKey& a, const InternalKey& b) const;
};

//筛选从内部密钥转换为用户密钥的策略包装器
class InternalFilterPolicy : public FilterPolicy {
 private:
  const FilterPolicy* const user_policy_;
 public:
  explicit InternalFilterPolicy(const FilterPolicy* p) : user_policy_(p) { }
  virtual const char* Name() const;
  virtual void CreateFilter(const Slice* keys, int n, std::string* dst) const;
  virtual bool KeyMayMatch(const Slice& key, const Slice& filter) const;
};

//
//下面的类而不是普通字符串，这样我们就不会
//错误地使用字符串比较而不是InternalKeyComparator。
class InternalKey {
 private:
  std::string rep_;
 public:
InternalKey() { }   //将rep_u保留为空，表示它无效
  InternalKey(const Slice& user_key, SequenceNumber s, ValueType t) {
    AppendInternalKey(&rep_, ParsedInternalKey(user_key, s, t));
  }

  void DecodeFrom(const Slice& s) { rep_.assign(s.data(), s.size()); }
  Slice Encode() const {
    assert(!rep_.empty());
    return rep_;
  }

  Slice user_key() const { return ExtractUserKey(rep_); }

  void SetFrom(const ParsedInternalKey& p) {
    rep_.clear();
    AppendInternalKey(&rep_, p);
  }

  void Clear() { rep_.clear(); }

  std::string DebugString() const;
};

inline int InternalKeyComparator::Compare(
    const InternalKey& a, const InternalKey& b) const {
  return Compare(a.Encode(), b.Encode());
}

inline bool ParseInternalKey(const Slice& internal_key,
                             ParsedInternalKey* result) {
  const size_t n = internal_key.size();
  if (n < 8) return false;
  uint64_t num = DecodeFixed64(internal_key.data() + n - 8);
  unsigned char c = num & 0xff;
  result->sequence = num >> 8;
  result->type = static_cast<ValueType>(c);
  result->user_key = Slice(internal_key.data(), n - 8);
  return (c <= static_cast<unsigned char>(kTypeValue));
}

//对dbimpl:：get（）有用的帮助程序类
class LookupKey {
 public:
//初始化*用于在快照中查找用户密钥
//指定的序列号。
  LookupKey(const Slice& user_key, SequenceNumber sequence);

  ~LookupKey();

//返回一个适合在memtable中查找的键。
  Slice memtable_key() const { return Slice(start_, end_ - start_); }

//返回内部键（适用于传递到内部迭代器）
  Slice internal_key() const { return Slice(kstart_, end_ - kstart_); }

//返回用户密钥
  Slice user_key() const { return Slice(kstart_, end_ - kstart_ - 8); }

 private:
//我们构造了一个char数组的形式：
//klength varint32<--开始
//userkey char[klength]<--kstart_
//标签UIT64
//< Enthi
//数组是一个合适的memtable键。
//以“userkey”开头的后缀可以用作internalkey。
  const char* start_;
  const char* kstart_;
  const char* end_;
char space_[200];      //避免为短键分配

//不允许复制
  LookupKey(const LookupKey&);
  void operator=(const LookupKey&);
};

inline LookupKey::~LookupKey() {
  if (start_ != space_) delete[] start_;
}

}  //命名空间级别数据库

#endif  //存储级别数据库格式
