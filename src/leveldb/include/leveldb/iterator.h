
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
//迭代器从源中生成一个键/值对序列。
//下面的类定义接口。多个实现
//由本图书馆提供。特别是，提供了迭代器
//访问表或数据库的内容。
//
//多个线程可以在迭代器上调用const方法，而不需要
//外部同步，但如果任何线程可以调用
//非常量方法，访问同一迭代器的所有线程都必须使用
//外部同步。

#ifndef STORAGE_LEVELDB_INCLUDE_ITERATOR_H_
#define STORAGE_LEVELDB_INCLUDE_ITERATOR_H_

#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {

class Iterator {
 public:
  Iterator();
  virtual ~Iterator();

//迭代器要么定位在键/值对上，要么
//无效。如果迭代器有效，则此方法返回true。
  virtual bool Valid() const = 0;

//在震源中的第一个键处定位。迭代器有效（）
//调用iff后，源不为空。
  virtual void SeekToFirst() = 0;

//定位在源中的最后一个键。迭代器是
//在此调用之后，如果源不是空的，则返回valid（）。
  virtual void SeekToLast() = 0;

//在源中位于目标位置或超过目标位置的第一个键。
//在调用源包含的iff之后，迭代器是有效的（）。
//到达或超过目标的条目。
  virtual void Seek(const Slice& target) = 0;

//移动到源中的下一个条目。在此调用之后，valid（）是
//如果迭代器未定位在源中的最后一个条目，则返回true。
//要求：有效（）
  virtual void Next() = 0;

//移动到源中的上一个条目。在此调用之后，valid（）是
//如果迭代器未定位在源中的第一个条目，则返回true。
//要求：有效（）
  virtual void Prev() = 0;

//返回当前条目的键。的基础存储
//返回的切片仅在下一次修改
//迭代器。
//要求：有效（）
  virtual Slice key() const = 0;

//返回当前条目的值。的基础存储
//返回的切片仅在下一次修改
//迭代器。
//要求：有效（）
  virtual Slice value() const = 0;

//如果发生错误，请返回。否则返回OK状态。
  virtual Status status() const = 0;

//允许客户端注册函数/arg1/arg2三倍于
//将在销毁此迭代器时调用。
//
//请注意，与前面的所有方法不同，此方法是
//不是抽象的，因此客户端不应重写它。
  typedef void (*CleanupFunction)(void* arg1, void* arg2);
  void RegisterCleanup(CleanupFunction function, void* arg1, void* arg2);

 private:
  struct Cleanup {
    CleanupFunction function;
    void* arg1;
    void* arg2;
    Cleanup* next;
  };
  Cleanup cleanup_;

//不允许复制
  Iterator(const Iterator&);
  void operator=(const Iterator&);
};

//返回一个空的迭代器（不产生任何结果）。
extern Iterator* NewEmptyIterator();

//返回具有指定状态的空迭代器。
extern Iterator* NewErrorIterator(const Status& status);

}  //命名空间级别数据库

#endif  //存储级别包括迭代器
