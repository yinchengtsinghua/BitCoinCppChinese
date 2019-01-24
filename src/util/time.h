
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_UTIL_TIME_H
#define BITCOIN_UTIL_TIME_H

#include <stdint.h>
#include <string>

/*
 *GetTimeMicros（）和GetTimeMillis（）都返回系统时间，但在
 *不同单位。getTime（）返回以秒为单位的系统时间，但也返回
 *支持mocktime，用户可以指定时间，例如
 *测试（例如使用setmocktime rpc或-mocktime参数）。
 *
 *TODO:重新编写这些函数以确保类型安全（这样我们不会无意中
 *比较不同单位的数字，或将模拟时间与系统时间进行比较）。
 **/


int64_t GetTime();
int64_t GetTimeMillis();
int64_t GetTimeMicros();
int64_t GetSystemTimeInSeconds(); //像getTime（），但不可模拟
void SetMockTime(int64_t nMockTimeIn);
int64_t GetMockTime();
void MilliSleep(int64_t n);

/*
 *首选ISO 8601格式。使用格式is8601日期时间、日期、时间
 *如果可能，帮助器功能。
 **/

std::string FormatISO8601DateTime(int64_t nTime);
std::string FormatISO8601Date(int64_t nTime);
std::string FormatISO8601Time(int64_t nTime);

#endif //比特币使用时间
