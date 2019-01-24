
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_RPC_SERVER_H
#define BITCOIN_RPC_SERVER_H

#include <amount.h>
#include <rpc/protocol.h>
#include <uint256.h>

#include <list>
#include <map>
#include <stdint.h>
#include <string>

#include <univalue.h>

static const unsigned int DEFAULT_RPC_SERIALIZE_VERSION = 1;

class CRPCCommand;

namespace RPCServer
{
    void OnStarted(std::function<void ()> slot);
    void OnStopped(std::function<void ()> slot);
}

/*univalue:：vtype的包装，其中包括typeany:
 *用于表示不在乎类型。*/

struct UniValueType {
    UniValueType(UniValue::VType _type) : typeAny(false), type(_type) {}
    UniValueType() : typeAny(true) {}
    bool typeAny;
    UniValue::VType type;
};

class JSONRPCRequest
{
public:
    UniValue id;
    std::string strMethod;
    UniValue params;
    bool fHelp;
    std::string URI;
    std::string authUser;
    std::string peerAddr;

    JSONRPCRequest() : id(NullUniValue), params(NullUniValue), fHelp(false) {}
    void parse(const UniValue& valRequest);
};

/*查询RPC是否正在运行*/
bool IsRPCRunning();

/*
 *设置RPC预热状态。完成此操作后，所有RPC调用都将出错。
 *立即将rpc_置于预热状态。
 **/

void SetRPCWarmupStatus(const std::string& newStatus);
/*将预热标记为“完成”。从现在起将处理RPC调用。*/
void SetRPCWarmupFinished();

/*返回当前预热状态。*/
bool RPCIsInWarmup(std::string *outStatus);

/*
 *类型检查参数；如果给定的类型错误，则抛出jsonrpcerror。不检查那个
 *传递正确数量的参数，只要传递的任何参数都是正确的类型。
 **/

void RPCTypeCheck(const UniValue& params,
                  const std::list<UniValueType>& typesExpected, bool fAllowNull=false);

/*
 *类型检查一个参数；如果给定的类型错误，则抛出jsonrpcerror。
 **/

void RPCTypeCheckArgument(const UniValue& value, const UniValueType& typeExpected);

/*
  检查对象中的预期键/值类型。
**/

void RPCTypeCheckObj(const UniValue& o,
    const std::map<std::string, UniValueType>& typesExpected,
    bool fAllowNull = false,
    bool fStrict = false);

/*newtimerfunc返回的计时器的不透明基类。
 *目前不提供任何方法，但请确保删除
 *清理整个状态。
 **/

class RPCTimerBase
{
public:
    virtual ~RPCTimerBase() {}
};

/*
 *RPC计时器“驱动程序”。
 **/

class RPCTimerInterface
{
public:
    virtual ~RPCTimerInterface() {}
    /*实施名称*/
    virtual const char *Name() = 0;
    /*计时器的工厂功能。
     *rpc将调用函数以创建一个计时器，该计时器将在*毫秒*毫秒内调用func。
     *@注意，由于RPC机制是后端中立的，因此它可以使用不同的计时器实现。
     *这是为了应对没有HTTP服务器的情况，但是
     *仅限GUI RPC控制台，中断PCServer对httprpc的依赖。
     **/

    virtual RPCTimerBase* NewTimer(std::function<void()>& func, int64_t millis) = 0;
};

/*设置计时器的出厂功能*/
void RPCSetTimerInterface(RPCTimerInterface *iface);
/*为计时器设置出厂功能，但仅当未设置时*/
void RPCSetTimerInterfaceIfUnset(RPCTimerInterface *iface);
/*取消设置计时器的工厂功能*/
void RPCUnsetTimerInterface(RPCTimerInterface *iface);

/*
 *从现在开始运行func nseconds。
 *覆盖上一个计时器（如果有）。
 **/

void RPCRunLater(const std::string& name, std::function<void()> func, int64_t nSeconds);

typedef UniValue(*rpcfn_type)(const JSONRPCRequest& jsonRequest);

class CRPCCommand
{
public:
    std::string category;
    std::string name;
    rpcfn_type actor;
    std::vector<std::string> argNames;
};

/*
 *比特币RPC命令调度程序。
 **/

class CRPCTable
{
private:
    std::map<std::string, const CRPCCommand*> mapCommands;
public:
    CRPCTable();
    const CRPCCommand* operator[](const std::string& name) const;
    std::string help(const std::string& name, const JSONRPCRequest& helpreq) const;

    /*
     *执行一个方法。
     *@param请求jsonrpcrequest执行
     *@返回调用结果。
     *@在发生错误时引发异常（单值）。
     **/

    UniValue execute(const JSONRPCRequest &request) const;

    /*
    *返回已注册命令的列表
    *@返回已注册命令的列表。
    **/

    std::vector<std::string> listCommands() const;


    /*
     *在调度表中添加一个CRPCCommand。
     *
     *如果RPC服务器已在运行，则返回false（转储并发保护）。
     *
     *无法覆盖命令（返回false）。
     *
     *具有不同方法名但相同回调函数的命令将
     *被视为别名，只有第一个注册的方法名
     *显示在帮助文本命令列表中。别名命令没有
     *有同样的行为。服务器和客户端代码可以区分
     *在基于方法名的调用之间，别名命令也可以
     *注册不同的名称、类型和参数编号。
     **/

    bool appendCommand(const std::string& name, const CRPCCommand* pcmd);
};

bool IsDeprecatedRPCEnabled(const std::string& method);

extern CRPCTable tableRPC;

/*
 *实用程序：转换十六进制编码值
 （如果不是十六进制，则抛出错误）。
 **/

extern uint256 ParseHashV(const UniValue& v, std::string strName);
extern uint256 ParseHashO(const UniValue& o, std::string strKey);
extern std::vector<unsigned char> ParseHexV(const UniValue& v, std::string strName);
extern std::vector<unsigned char> ParseHexO(const UniValue& o, std::string strKey);

extern CAmount AmountFromValue(const UniValue& value);
extern std::string HelpExampleCli(const std::string& methodname, const std::string& args);
extern std::string HelpExampleRpc(const std::string& methodname, const std::string& args);

void StartRPC();
void InterruptRPC();
void StopRPC();
std::string JSONRPCExecBatch(const JSONRPCRequest& jreq, const UniValue& vReq);

//检索命令行参数中请求的任何序列化标志
int RPCSerializationFlags();

#endif //比特币RPC服务器
