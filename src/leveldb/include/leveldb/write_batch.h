
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
//writebatch保存一组更新，以原子方式应用于数据库。
//
//更新按添加顺序应用
//写入批处理。例如，“key”的值为“v3”
//在写入以下批之后：
//
//批量放置（“key”，“v1”）；
//批量删除（“key”）；
//批量放置（“key”，“v2”）；
//批量放置（“key”，“v3”）；
//
//多个线程可以在没有
//外部同步，但如果任何线程可以调用
//非常量方法，访问同一writebatch的所有线程都必须使用
//外部同步。

#ifndef STORAGE_LEVELDB_INCLUDE_WRITE_BATCH_H_
#define STORAGE_LEVELDB_INCLUDE_WRITE_BATCH_H_

#include <string>
#include "leveldb/status.h"

namespace leveldb {

class Slice;

class WriteBatch {
 public:
  WriteBatch();
  ~WriteBatch();

//将映射“key->value”存储在数据库中。
  void Put(const Slice& key, const Slice& value);

//如果数据库包含“key”的映射，请将其删除。否则什么也不做。
  void Delete(const Slice& key);

//清除此批处理中缓冲的所有更新。
  void Clear();

//支持对批内容进行迭代。
  class Handler {
   public:
    virtual ~Handler();
    virtual void Put(const Slice& key, const Slice& value) = 0;
    virtual void Delete(const Slice& key) = 0;
  };
  Status Iterate(Handler* handler) const;

 private:
  friend class WriteBatchInternal;

std::string rep_;  //rep的格式请参见write_batch.cc中的注释

//有意复制
};

}  //命名空间级别数据库

#endif  //存储级别包括写入批处理
