
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

#ifndef BITCOIN_INIT_H
#define BITCOIN_INIT_H

#include <memory>
#include <string>
#include <util/system.h>

namespace interfaces {
class Chain;
class ChainClient;
} //命名空间接口

//！指向在初始化期间使用的接口和在关闭时销毁的接口的指针。
struct InitInterfaces
{
    std::unique_ptr<interfaces::Chain> chain;
    std::vector<std::unique_ptr<interfaces::ChainClient>> chain_clients;
};

namespace boost
{
class thread_group;
} //命名空间提升

/*中断线程*/
void Interrupt();
void Shutdown(InitInterfaces& interfaces);
//！初始化日志基础结构
void InitLogging();
//！参数交互：根据各种规则更改当前参数
void InitParameterInteraction();

/*初始化比特币核心：基本上下文设置。
 *@注意，这可以在后台监控之前完成。如果函数失败，不要调用shutdown（）。
 应该分析@pre参数并读取配置文件。
 **/

bool AppInitBasicSetup();
/*
 *初始化：参数交互。
 *@注意，这可以在后台监控之前完成。如果函数失败，不要调用shutdown（）。
 应该分析@pre参数并读取配置文件，应该调用appInitbasicSetup。
 **/

bool AppInitParameterInteraction();
/*
 *初始化健全性检查：ecc init、健全性检查、dir lock。
 *@注意，这可以在后台监控之前完成。如果函数失败，不要调用shutdown（）。
 应该分析@pre参数并读取配置文件，应该调用appInitParameterInteraction。
 **/

bool AppInitSanityChecks();
/*
 *锁定比特币核心数据目录。
 *@注意，这只能在后台监控之后进行。如果函数失败，不要调用shutdown（）。
 应该分析@pre参数并读取配置文件，应该调用appInitSanityChecks。
 **/

bool AppInitLockDataDirectory();
/*
 *比特币核心主初始化。
 *@注意，这只能在后台监控之后进行。如果函数失败，则调用shutdown（）。
 应该分析@pre参数并读取配置文件，应该调用appinitlockdatadirectory。
 **/

bool AppInitMain(InitInterfaces& interfaces);

/*
 *设置Gargs的参数
 **/

void SetupServerArgs();

/*返回授权信息（用于-版本）*/
std::string LicenseInfo();

#endif //比特科尼
