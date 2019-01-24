
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011 LevelDB作者。版权所有。
//此源代码的使用受可以
//
//
//缓存是一个将键映射到值的接口。它有内部的
//同步，可以从
//多线程。它可以自动收回条目以腾出空间
//对于新条目。值对缓存具有指定的费用
//容量。例如，值为变量的缓存
//长度字符串，可以使用字符串的长度作为
//字符串。
//
//具有最近使用最少的逐出的内置缓存实现
//提供了策略。如果
//他们想要更复杂的东西（比如扫描电阻，
//自定义逐出策略、可变缓存大小等）

#ifndef STORAGE_LEVELDB_INCLUDE_CACHE_H_
#define STORAGE_LEVELDB_INCLUDE_CACHE_H_

#include <stdint.h>
#include "leveldb/slice.h"

namespace leveldb {

class Cache;

//创建具有固定大小容量的新缓存。本次实施
//的缓存使用最近使用最少的收回策略。
extern Cache* NewLRUCache(size_t capacity);

class Cache {
 public:
  Cache() { }

//通过调用“deleter”销毁所有现有条目
//传递给构造函数的函数。
  virtual ~Cache();

//存储在缓存中的条目的不透明句柄。
  struct Handle { };

//将key->value的映射插入缓存并分配给它
//对总缓存容量的指定费用。
//
//返回与映射对应的句柄。呼叫者
//当返回的映射为no时，必须调用此->release（handle）
//需要更长的时间。
//
//当不再需要插入的条目时，密钥和
//值将传递给“deleter”。
  virtual Handle* Insert(const Slice& key, void* value, size_t charge,
                         void (*deleter)(const Slice& key, void* value)) = 0;

//如果缓存没有“key”的映射，则返回空。
//
//否则返回对应于映射的句柄。呼叫者
//当返回的映射为no时，必须调用此->release（handle）
//需要更长的时间。
  virtual Handle* Lookup(const Slice& key) = 0;

//释放先前查找（）返回的映射。
//要求：句柄必须尚未释放。
//要求：句柄必须已由*this上的方法返回。
  virtual void Release(Handle* handle) = 0;

//返回由返回的句柄中封装的值
//查找成功（）。
//要求：句柄必须尚未释放。
//要求：句柄必须已由*this上的方法返回。
  virtual void* Value(Handle* handle) = 0;

//如果缓存包含键的条目，请将其清除。请注意
//基础条目将一直保留到所有现有句柄
//它已经被释放了。
  virtual void Erase(const Slice& key) = 0;

//返回一个新的数字ID。可以由多个
//共享相同的缓存来划分密钥空间。通常情况下
//客户端将在启动时分配一个新的ID，并将该ID预先发送到
//它的缓存密钥。
  virtual uint64_t NewId() = 0;

//删除所有未在使用中的缓存项。内存受限
//应用程序可能希望调用此方法以减少内存使用。
//prune（）的默认实现不起任何作用。子类是强
//鼓励重写默认实现。未来发布的
//leveldb可以将prune（）更改为纯抽象方法。
  virtual void Prune() {}

//返回存储在
//隐藏物。
  virtual size_t TotalCharge() const = 0;

 private:
  void LRU_Remove(Handle* e);
  void LRU_Append(Handle* e);
  void Unref(Handle* e);

  struct Rep;
  Rep* rep_;

//不允许复制
  Cache(const Cache&);
  void operator=(const Cache&);
};

}  //命名空间级别数据库

#endif  //存储级别包括缓存
