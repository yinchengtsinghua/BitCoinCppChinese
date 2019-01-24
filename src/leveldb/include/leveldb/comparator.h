
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

#ifndef STORAGE_LEVELDB_INCLUDE_COMPARATOR_H_
#define STORAGE_LEVELDB_INCLUDE_COMPARATOR_H_

#include <string>

namespace leveldb {

class Slice;

//比较器对象提供了跨切片的总顺序
//用作sstable或数据库中的键。比较器实现
//必须是线程安全的，因为leveldb可以同时调用其方法
//来自多个线程。
class Comparator {
 public:
  virtual ~Comparator();

//三向比较。返回值：
//<0 iff“a”<“b”，
//==0 iff“a”==“b”，
//>0如果“A”>“B”
  virtual int Compare(const Slice& a, const Slice& b) const = 0;

//比较器的名称。用于检查比较器
//不匹配（即使用一个比较器创建的数据库
//使用不同的比较器访问。
//
//每当
//比较器的实现会以某种方式发生变化，从而导致
//要更改的任意两个键的相对顺序。
//
//以“leveldb.”开头的名称是保留的，不应使用
//由此包的任何客户端。
  virtual const char* Name() const = 0;

//高级功能：用于减少空间需求
//用于索引块等内部数据结构。

//如果*start<limit，将*start更改为[start，limit]中的短字符串。
//简单的比较器实现可以在*start不变的情况下返回，
//也就是说，这个方法的一个不做任何事情的实现是正确的。
  virtual void FindShortestSeparator(
      std::string* start,
      const Slice& limit) const = 0;

//将*键更改为短字符串>=*键。
//简单的比较器实现可以在*键不变的情况下返回，
//也就是说，这个方法的一个不做任何事情的实现是正确的。
  virtual void FindShortSuccessor(std::string* key) const = 0;
};

//返回使用字典式字节顺序的内置比较器
//排序。结果仍然是该模块的属性，并且
//不能删除。
extern const Comparator* BytewiseComparator();

}  //命名空间级别数据库

#endif  //存储级别b_包括\u比较器\u h_
