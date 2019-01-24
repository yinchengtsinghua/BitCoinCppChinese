
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

//MSVC直到2010年才随此文件一起提供。

#ifndef STORAGE_LEVELDB_PORT_WIN_STDINT_H_
#define STORAGE_LEVELDB_PORT_WIN_STDINT_H_

#if !defined(_MSC_VER)
#error This file should only be included when compiling with MSVC.
#endif

//定义C99等效类型。
typedef signed char           int8_t;
typedef signed short          int16_t;
typedef signed int            int32_t;
typedef signed long long      int64_t;
typedef unsigned char         uint8_t;
typedef unsigned short        uint16_t;
typedef unsigned int          uint32_t;
typedef unsigned long long    uint64_t;

#endif  //存储级别b_port_win_stdint_h_
