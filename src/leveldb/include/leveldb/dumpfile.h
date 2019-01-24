
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2014 LevelDB作者。版权所有。
//此源代码的使用受可以
//在许可证文件中找到。有关参与者的名称，请参阅作者文件。

#ifndef STORAGE_LEVELDB_INCLUDE_DUMPFILE_H_
#define STORAGE_LEVELDB_INCLUDE_DUMPFILE_H_

#include <string>
#include "leveldb/env.h"
#include "leveldb/status.h"

namespace leveldb {

//将由fname以文本格式命名的文件的内容转储到
//* DST。进行一系列dst->append（）调用；每个调用都被传递
//找到与单个项对应的以换行符结尾的文本
//在文件中。
//
//如果fname未命名leveldb存储，则返回非OK结果
//文件，或者如果文件无法读取。
Status DumpFile(Env* env, const std::string& fname, WritableFile* dst);

}  //命名空间级别数据库

#endif  //存储级别包括转储文件
