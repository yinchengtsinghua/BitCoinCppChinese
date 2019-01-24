
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
//解码block_builder.cc生成的块。

#include "table/block.h"

#include <vector>
#include <algorithm>
#include "leveldb/comparator.h"
#include "table/format.h"
#include "util/coding.h"
#include "util/logging.h"

namespace leveldb {

inline uint32_t Block::NumRestarts() const {
  assert(size_ >= sizeof(uint32_t));
  return DecodeFixed32(data_ + size_ - sizeof(uint32_t));
}

Block::Block(const BlockContents& contents)
    : data_(contents.data.data()),
      size_(contents.data.size()),
      owned_(contents.heap_allocated) {
  if (size_ < sizeof(uint32_t)) {
size_ = 0;  //错误标记
  } else {
    size_t max_restarts_allowed = (size_-sizeof(uint32_t)) / sizeof(uint32_t);
    if (NumRestarts() > max_restarts_allowed) {
//大小太小，无法进行numsrestarts（）。
      size_ = 0;
    } else {
      restart_offset_ = size_ - (1 + NumRestarts()) * sizeof(uint32_t);
    }
  }
}

Block::~Block() {
  if (owned_) {
    delete[] data_;
  }
}

//helper例程：从“p”开始解码下一个块条目，
//存储共享密钥字节数、非共享密钥字节数，
//以及“*共享”、“*非共享”中值的长度，以及
//分别为“*value_length”。不会取消引用超过“限制”。
//
//如果检测到任何错误，则返回空。否则，返回
//指向键增量的指针（刚好超过三个解码值）。
static inline const char* DecodeEntry(const char* p, const char* limit,
                                      uint32_t* shared,
                                      uint32_t* non_shared,
                                      uint32_t* value_length) {
  if (limit - p < 3) return NULL;
  *shared = reinterpret_cast<const unsigned char*>(p)[0];
  *non_shared = reinterpret_cast<const unsigned char*>(p)[1];
  *value_length = reinterpret_cast<const unsigned char*>(p)[2];
  if ((*shared | *non_shared | *value_length) < 128) {
//快速路径：所有三个值都编码在一个字节中
    p += 3;
  } else {
    if ((p = GetVarint32Ptr(p, limit, shared)) == NULL) return NULL;
    if ((p = GetVarint32Ptr(p, limit, non_shared)) == NULL) return NULL;
    if ((p = GetVarint32Ptr(p, limit, value_length)) == NULL) return NULL;
  }

  if (static_cast<uint32_t>(limit - p) < (*non_shared + *value_length)) {
    return NULL;
  }
  return p;
}

class Block::Iter : public Iterator {
 private:
  const Comparator* const comparator_;
const char* const data_;      //基础块内容
uint32_t const restarts_;     //重新启动数组的偏移量（Fixed32列表）
uint32_t const num_restarts_; //重新启动数组中的uint32条目数

//当前_u是当前条目的数据_u中的偏移量。>=重新启动uuuif！有效的
  uint32_t current_;
uint32_t restart_index_;  //电流下降的重启块索引
  std::string key_;
  Slice value_;
  Status status_;

  inline int Compare(const Slice& a, const Slice& b) const {
    return comparator_->Compare(a, b);
  }

//返回刚好超过当前条目的数据偏移量。
  inline uint32_t NextEntryOffset() const {
    return (value_.data() + value_.size()) - data_;
  }

  uint32_t GetRestartPoint(uint32_t index) {
    assert(index < num_restarts_);
    return DecodeFixed32(data_ + restarts_ + index * sizeof(uint32_t));
  }

  void SeekToRestartPoint(uint32_t index) {
    key_.clear();
    restart_index_ = index;
//当前_u将由parseNextKey（）固定；

//parseNextKey（）从值_uu的末尾开始，因此相应地设置值_u。
    uint32_t offset = GetRestartPoint(index);
    value_ = Slice(data_ + offset, 0);
  }

 public:
  Iter(const Comparator* comparator,
       const char* data,
       uint32_t restarts,
       uint32_t num_restarts)
      : comparator_(comparator),
        data_(data),
        restarts_(restarts),
        num_restarts_(num_restarts),
        current_(restarts_),
        restart_index_(num_restarts_) {
    assert(num_restarts_ > 0);
  }

  virtual bool Valid() const { return current_ < restarts_; }
  virtual Status status() const { return status_; }
  virtual Slice key() const {
    assert(Valid());
    return key_;
  }
  virtual Slice value() const {
    assert(Valid());
    return value_;
  }

  virtual void Next() {
    assert(Valid());
    ParseNextKey();
  }

  virtual void Prev() {
    assert(Valid());

//向后扫描到当前\u之前的重新启动点
    const uint32_t original = current_;
    while (GetRestartPoint(restart_index_) >= original) {
      if (restart_index_ == 0) {
//不再参赛
        current_ = restarts_;
        restart_index_ = num_restarts_;
        return;
      }
      restart_index_--;
    }

    SeekToRestartPoint(restart_index_);
    do {
//循环直到当前条目的结尾与原始条目的开头匹配
    } while (ParseNextKey() && NextEntryOffset() < original);
  }

  virtual void Seek(const Slice& target) {
//在重新启动数组中进行二进制搜索以查找最后一个重新启动点
//有一个键<目标
    uint32_t left = 0;
    uint32_t right = num_restarts_ - 1;
    while (left < right) {
      uint32_t mid = (left + right + 1) / 2;
      uint32_t region_offset = GetRestartPoint(mid);
      uint32_t shared, non_shared, value_length;
      const char* key_ptr = DecodeEntry(data_ + region_offset,
                                        data_ + restarts_,
                                        &shared, &non_shared, &value_length);
      if (key_ptr == NULL || (shared != 0)) {
        CorruptionError();
        return;
      }
      Slice mid_key(key_ptr, non_shared);
      if (Compare(mid_key, target) < 0) {
//“Mid”处的键小于“Target”。因此所有
//“mid”之前的块是无趣的。
        left = mid;
      } else {
//“Mid”处的键大于等于“Target”。因此，所有块位于或
//“mid”之后就没意思了。
        right = mid - 1;
      }
    }

//第一个键>=目标的线性搜索（在重新启动块内）
    SeekToRestartPoint(left);
    while (true) {
      if (!ParseNextKey()) {
        return;
      }
      if (Compare(key_, target) >= 0) {
        return;
      }
    }
  }

  virtual void SeekToFirst() {
    SeekToRestartPoint(0);
    ParseNextKey();
  }

  virtual void SeekToLast() {
    SeekToRestartPoint(num_restarts_ - 1);
    while (ParseNextKey() && NextEntryOffset() < restarts_) {
//跳绳
    }
  }

 private:
  void CorruptionError() {
    current_ = restarts_;
    restart_index_ = num_restarts_;
    status_ = Status::Corruption("bad entry in block");
    key_.clear();
    value_.clear();
  }

  bool ParseNextKey() {
    current_ = NextEntryOffset();
    const char* p = data_ + current_;
const char* limit = data_ + restarts_;  //数据后立即重新启动
    if (p >= limit) {
//没有要返回的条目。标记为无效。
      current_ = restarts_;
      restart_index_ = num_restarts_;
      return false;
    }

//解码下一个条目
    uint32_t shared, non_shared, value_length;
    p = DecodeEntry(p, limit, &shared, &non_shared, &value_length);
    if (p == NULL || key_.size() < shared) {
      CorruptionError();
      return false;
    } else {
      key_.resize(shared);
      key_.append(p, non_shared);
      value_ = Slice(p + non_shared, value_length);
      while (restart_index_ + 1 < num_restarts_ &&
             GetRestartPoint(restart_index_ + 1) < current_) {
        ++restart_index_;
      }
      return true;
    }
  }
};

Iterator* Block::NewIterator(const Comparator* cmp) {
  if (size_ < sizeof(uint32_t)) {
    return NewErrorIterator(Status::Corruption("bad block contents"));
  }
  const uint32_t num_restarts = NumRestarts();
  if (num_restarts == 0) {
    return NewEmptyIterator();
  } else {
    return new Iter(cmp, data_, restart_offset_, num_restarts);
  }
}

}  //命名空间级别数据库
