
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
//不能包含在任何.h文件中，以避免污染命名空间
//用宏。

#ifndef STORAGE_LEVELDB_UTIL_LOGGING_H_
#define STORAGE_LEVELDB_UTIL_LOGGING_H_

#include <stdio.h>
#include <stdint.h>
#include <string>
#include "port/port.h"

namespace leveldb {

class Slice;
class WritableFile;

//将“num”的可读打印输出附加到*str
extern void AppendNumberTo(std::string* str, uint64_t num);

//将“value”的可读打印输出附加到*str。
//转义在“value”中找到的任何不可打印字符。
extern void AppendEscapedStringTo(std::string* str, const Slice& value);

//返回“num”的可读打印输出
extern std::string NumberToString(uint64_t num);

//返回“value”的可读版本。
//转义在“value”中找到的任何不可打印字符。
extern std::string EscapeString(const Slice& value);

//将人类可读的数字从“*in”解析为“*value”。关于成功，
//提前“*in”超过消耗的数量，并将“*val”设置为
//数值。否则，返回false并在
//未指定状态。
extern bool ConsumeDecimalNumber(Slice* in, uint64_t* val);

}  //命名空间级别数据库

#endif  //存储级别
