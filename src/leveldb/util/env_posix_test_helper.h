
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有2017 LevelDB作者。版权所有。
//此源代码的使用受可以
//在许可证文件中找到。有关参与者的名称，请参阅作者文件。

#ifndef STORAGE_LEVELDB_UTIL_ENV_POSIX_TEST_HELPER_H_
#define STORAGE_LEVELDB_UTIL_ENV_POSIX_TEST_HELPER_H_

namespace leveldb {

class EnvPosixTest;

//posix env的助手，用于促进测试。
class EnvPosixTestHelper {
 private:
  friend class EnvPosixTest;

//设置将打开的只读文件的最大数目。
//必须在创建env之前调用。
  static void SetReadOnlyFDLimit(int limit);

//设置将通过mmap映射的只读文件的最大数目。
//必须在创建env之前调用。
  static void SetReadOnlyMMapLimit(int limit);
};

}  //命名空间级别数据库

#endif  //存储级别
