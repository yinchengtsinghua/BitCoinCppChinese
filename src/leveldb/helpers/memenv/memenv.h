
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

#ifndef STORAGE_LEVELDB_HELPERS_MEMENV_MEMENV_H_
#define STORAGE_LEVELDB_HELPERS_MEMENV_MEMENV_H_

namespace leveldb {

class Env;

//返回将其数据存储在内存和委托中的新环境
//所有非文件存储任务到基环境。调用方必须删除结果
//当它不再需要时。
//*使用结果时，基本环境必须保持活动状态。
Env* NewMemEnv(Env* base_env);

}  //命名空间级别数据库

#endif  //存储级别
