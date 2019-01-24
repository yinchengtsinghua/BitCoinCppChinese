
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
//数据库代码使用的文件名

#ifndef STORAGE_LEVELDB_DB_FILENAME_H_
#define STORAGE_LEVELDB_DB_FILENAME_H_

#include <stdint.h>
#include <string>
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "port/port.h"

namespace leveldb {

class Env;

enum FileType {
  kLogFile,
  kDBLockFile,
  kTableFile,
  kDescriptorFile,
  kCurrentFile,
  kTempFile,
kInfoLogFile  //要么是现在的，要么是旧的
};

//用指定的数字返回日志文件的名称
//在以“dbname”命名的数据库中。结果将以前缀
//“dBNEX”。
extern std::string LogFileName(const std::string& dbname, uint64_t number);

//用指定的数字返回sstable的名称
//在以“dbname”命名的数据库中。结果将以前缀
//“dBNEX”。
extern std::string TableFileName(const std::string& dbname, uint64_t number);

//返回具有指定数字的sstable的旧文件名
//在以“dbname”命名的数据库中。结果将以前缀
//“dBNEX”。
extern std::string SSTTableFileName(const std::string& dbname, uint64_t number);

//返回由命名的数据库的描述符文件名
//“dbname”和指定的化身号。结果是
//前缀为“dbname”。
extern std::string DescriptorFileName(const std::string& dbname,
                                      uint64_t number);

//返回当前文件的名称。此文件包含名称
//当前清单文件的。结果将以前缀
//“dBNEX”。
extern std::string CurrentFileName(const std::string& dbname);

//返回由命名的数据库的锁文件名
//“dBNEX”。结果将以“dbname”作为前缀。
extern std::string LockFileName(const std::string& dbname);

//返回名为“db name”的数据库所拥有的临时文件的名称。
//结果将以“dbname”作为前缀。
extern std::string TempFileName(const std::string& dbname, uint64_t number);

//返回“dbname”的信息日志文件名。
extern std::string InfoLogFileName(const std::string& dbname);

//返回“dbname”的旧信息日志文件的名称。
extern std::string OldInfoLogFileName(const std::string& dbname);

//如果文件名是LEVELDB文件，请将文件类型存储在*类型中。
//文件名中编码的数字存储在*数字中。如果
//已成功分析文件名，返回true。否则返回false。
extern bool ParseFileName(const std::string& filename,
                          uint64_t* number,
                          FileType* type);

//使当前文件指向描述符文件
//指定的数字。
extern Status SetCurrentFile(Env* env, const std::string& dbname,
                             uint64_t descriptor_number);


}  //命名空间级别数据库

#endif  //存储级别数据库文件名
