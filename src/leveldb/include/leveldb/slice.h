
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
//slice是一个简单的结构，包含指向某个外部
//存储和大小。切片的用户必须确保切片
//在相应的外部存储
//解除分配。
//
//多个线程可以在一个切片上调用const方法，而不需要
//外部同步，但如果任何线程可以调用
//非常量方法，访问同一切片的所有线程都必须使用
//外部同步。

#ifndef STORAGE_LEVELDB_INCLUDE_SLICE_H_
#define STORAGE_LEVELDB_INCLUDE_SLICE_H_

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <string>

namespace leveldb {

class Slice {
 public:
//创建一个空切片。
  Slice() : data_(""), size_(0) { }

//创建一个引用d[0，n-1]的切片。
  Slice(const char* d, size_t n) : data_(d), size_(n) { }

//创建一个引用“s”内容的切片
  Slice(const std::string& s) : data_(s.data()), size_(s.size()) { }

//创建引用s[0，strlen（s）-1]的切片
  Slice(const char* s) : data_(s), size_(strlen(s)) { }

//返回指向引用数据开头的指针
  const char* data() const { return data_; }

//返回引用数据的长度（字节）
  size_t size() const { return size_; }

//返回真iff引用数据的长度为零
  bool empty() const { return size_ == 0; }

//返回引用数据中的第i个字节。
//要求：n<size（））
  char operator[](size_t n) const {
    assert(n < size());
    return data_[n];
  }

//更改此切片以引用空数组
  void clear() { data_ = ""; size_ = 0; }

//从这个切片中删除第一个“n”字节。
  void remove_prefix(size_t n) {
    assert(n <= size());
    data_ += n;
    size_ -= n;
  }

//返回包含引用数据副本的字符串。
  std::string ToString() const { return std::string(data_, size_); }

//三向比较。返回值：
//<0 iff“*this”<“b”，
//==0 iff“*此”==“b”，
//>0 iff“*此”>B”
  int compare(const Slice& b) const;

//返回真iff“x”是“*this”的前缀
  bool starts_with(const Slice& x) const {
    return ((size_ >= x.size_) &&
            (memcmp(data_, x.data_, x.size_) == 0));
  }

 private:
  const char* data_;
  size_t size_;

//有意复制
};

inline bool operator==(const Slice& x, const Slice& y) {
  return ((x.size() == y.size()) &&
          (memcmp(x.data(), y.data(), x.size()) == 0));
}

inline bool operator!=(const Slice& x, const Slice& y) {
  return !(x == y);
}

inline int Slice::compare(const Slice& b) const {
  const size_t min_len = (size_ < b.size_) ? size_ : b.size_;
  int r = memcmp(data_, b.data_, min_len);
  if (r == 0) {
    if (size_ < b.size_) r = -1;
    else if (size_ > b.size_) r = +1;
  }
  return r;
}

}  //命名空间级别数据库


#endif  //存储级别包括切片
