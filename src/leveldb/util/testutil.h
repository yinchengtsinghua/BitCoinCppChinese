
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

#ifndef STORAGE_LEVELDB_UTIL_TESTUTIL_H_
#define STORAGE_LEVELDB_UTIL_TESTUTIL_H_

#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "util/random.h"

namespace leveldb {
namespace test {

//在*dst中存储一个长度为“len”的随机字符串，并返回一个
//引用生成的数据。
extern Slice RandomString(Random* rnd, int len, std::string* dst);

//返回具有指定长度的随机键，该长度可能包含有趣的
//字符（例如\x00、\xff等）。
extern std::string RandomKey(Random* rnd, int len);

//在*dst中存储一个长度为“len”的字符串，该字符串将压缩为
//“n*压缩分数”字节并返回引用的切片
//生成的数据。
extern Slice CompressibleString(Random* rnd, double compressed_fraction,
                                size_t len, std::string* dst);

//允许错误注入的包装器。
class ErrorEnv : public EnvWrapper {
 public:
  bool writable_file_error_;
  int num_writable_file_errors_;

  ErrorEnv() : EnvWrapper(Env::Default()),
               writable_file_error_(false),
               num_writable_file_errors_(0) { }

  virtual Status NewWritableFile(const std::string& fname,
                                 WritableFile** result) {
    if (writable_file_error_) {
      ++num_writable_file_errors_;
      *result = NULL;
      return Status::IOError(fname, "fake error");
    }
    return target()->NewWritableFile(fname, result);
  }

  virtual Status NewAppendableFile(const std::string& fname,
                                   WritableFile** result) {
    if (writable_file_error_) {
      ++num_writable_file_errors_;
      *result = NULL;
      return Status::IOError(fname, "fake error");
    }
    return target()->NewAppendableFile(fname, result);
  }
};

}  //命名空间测试
}  //命名空间级别数据库

#endif  //存储级别
