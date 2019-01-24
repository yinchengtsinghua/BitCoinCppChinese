
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
//状态封装操作的结果。这可能意味着成功，
//或者，它可能指示相关错误消息的错误。
//
//多个线程可以在没有
//外部同步，但如果任何线程可以调用
//非常量方法，访问相同状态的所有线程都必须使用
//外部同步。

#ifndef STORAGE_LEVELDB_INCLUDE_STATUS_H_
#define STORAGE_LEVELDB_INCLUDE_STATUS_H_

#include <string>
#include "leveldb/slice.h"

namespace leveldb {

class Status {
 public:
//创建成功状态。
  Status() : state_(NULL) { }
  ~Status() { delete[] state_; }

//复制指定的状态。
  Status(const Status& s);
  void operator=(const Status& s);

//返回成功状态。
  static Status OK() { return Status(); }

//返回适当类型的错误状态。
  static Status NotFound(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kNotFound, msg, msg2);
  }
  static Status Corruption(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kCorruption, msg, msg2);
  }
  static Status NotSupported(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kNotSupported, msg, msg2);
  }
  static Status InvalidArgument(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kInvalidArgument, msg, msg2);
  }
  static Status IOError(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kIOError, msg, msg2);
  }

//如果状态指示成功，则返回true。
  bool ok() const { return (state_ == NULL); }

//返回true如果状态指示NotFound错误。
  bool IsNotFound() const { return code() == kNotFound; }

//返回true如果状态指示损坏错误。
  bool IsCorruption() const { return code() == kCorruption; }

//返回true如果状态指示IOError。
  bool IsIOError() const { return code() == kIOError; }

//返回true如果状态指示NotSupportedError。
  bool IsNotSupportedError() const { return code() == kNotSupported; }

//返回true如果状态指示无效参数。
  bool IsInvalidArgument() const { return code() == kInvalidArgument; }

//返回适合打印的此状态的字符串表示形式。
//返回字符串“OK”以获得成功。
  std::string ToString() const;

 private:
//OK状态为空状态。否则，state_u是一个新的[]数组
//格式如下：
//state_[0..3]=消息长度
//状态[4]==代码
//state_[5..]==消息
  const char* state_;

  enum Code {
    kOk = 0,
    kNotFound = 1,
    kCorruption = 2,
    kNotSupported = 3,
    kInvalidArgument = 4,
    kIOError = 5
  };

  Code code() const {
    return (state_ == NULL) ? kOk : static_cast<Code>(state_[4]);
  }

  Status(Code code, const Slice& msg, const Slice& msg2);
  static const char* CopyState(const char* s);
};

inline Status::Status(const Status& s) {
  state_ = (s.state_ == NULL) ? NULL : CopyState(s.state_);
}
inline void Status::operator=(const Status& s) {
//以下条件捕获两个别名（当此值为=&s时），
//通常情况下，S和*都可以。
  if (state_ != s.state_) {
    delete[] state_;
    state_ = (s.state_ == NULL) ? NULL : CopyState(s.state_);
  }
}

}  //命名空间级别数据库

#endif  //存储级别包括状态
