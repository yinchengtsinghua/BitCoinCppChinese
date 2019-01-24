
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

#ifndef BITCOIN_RPC_PROTOCOL_H
#define BITCOIN_RPC_PROTOCOL_H

#include <fs.h>

#include <list>
#include <map>
#include <stdint.h>
#include <string>

#include <univalue.h>

//！状态代码
enum HTTPStatusCode
{
    HTTP_OK                    = 200,
    HTTP_BAD_REQUEST           = 400,
    HTTP_UNAUTHORIZED          = 401,
    HTTP_FORBIDDEN             = 403,
    HTTP_NOT_FOUND             = 404,
    HTTP_BAD_METHOD            = 405,
    HTTP_INTERNAL_SERVER_ERROR = 500,
    HTTP_SERVICE_UNAVAILABLE   = 503,
};

//！比特币RPC错误代码
enum RPCErrorCode
{
//！标准JSON-RPC 2.0错误
//rpc_invalid_请求在内部映射到http_bad_请求（400）。
//它不应该用于应用程序层错误。
    RPC_INVALID_REQUEST  = -32600,
//rpc_method_not_found在内部映射到http_not_found（404）。
//它不应该用于应用程序层错误。
    RPC_METHOD_NOT_FOUND = -32601,
    RPC_INVALID_PARAMS   = -32602,
//rpc_内部_错误应仅用于比特币中的真正错误。
//（例如，datadir损坏）。
    RPC_INTERNAL_ERROR   = -32603,
    RPC_PARSE_ERROR      = -32700,

//！一般应用程序定义的错误
RPC_MISC_ERROR                  = -1,  //！<std：：在命令处理中引发异常
RPC_TYPE_ERROR                  = -3,  //！<意外类型作为参数传递
RPC_INVALID_ADDRESS_OR_KEY      = -5,  //！<无效的地址或密钥
RPC_OUT_OF_MEMORY               = -7,  //！<操作期间内存不足
RPC_INVALID_PARAMETER           = -8,  //！<无效、缺少或重复参数
RPC_DATABASE_ERROR              = -20, //！<数据库错误
RPC_DESERIALIZATION_ERROR       = -22, //！<分析或验证原始格式的结构时出错
RPC_VERIFY_ERROR                = -25, //！<事务或块提交过程中的一般错误
RPC_VERIFY_REJECTED             = -26, //！<网络规则拒绝了事务或块
RPC_VERIFY_ALREADY_IN_CHAIN     = -27, //！<事务已在链中
RPC_IN_WARMUP                   = -28, //！<客户端仍在预热
RPC_METHOD_DEPRECATED           = -32, //！<rpc方法已弃用

//！向后兼容的别名
    RPC_TRANSACTION_ERROR           = RPC_VERIFY_ERROR,
    RPC_TRANSACTION_REJECTED        = RPC_VERIFY_REJECTED,
    RPC_TRANSACTION_ALREADY_IN_CHAIN= RPC_VERIFY_ALREADY_IN_CHAIN,

//！P2P客户端错误
RPC_CLIENT_NOT_CONNECTED        = -9,  //！<比特币未连接
RPC_CLIENT_IN_INITIAL_DOWNLOAD  = -10, //！<仍在下载初始块
RPC_CLIENT_NODE_ALREADY_ADDED   = -23, //！<节点已添加
RPC_CLIENT_NODE_NOT_ADDED       = -24, //！<之前未添加节点
RPC_CLIENT_NODE_NOT_CONNECTED   = -29, //！<在已连接的节点中找不到要断开连接的节点
RPC_CLIENT_INVALID_IP_OR_SUBNET = -30, //！<无效的IP/子网
RPC_CLIENT_P2P_DISABLED         = -31, //！<未找到有效的连接管理器实例

//！钱包错误
RPC_WALLET_ERROR                = -4,  //！<钱包未指明问题（未找到钥匙等）
RPC_WALLET_INSUFFICIENT_FUNDS   = -6,  //！<钱包或账户中没有足够的资金
RPC_WALLET_INVALID_LABEL_NAME   = -11, //！<无效标签名称
RPC_WALLET_KEYPOOL_RAN_OUT      = -12, //！<keypool用完，请先调用keypoolrefill
RPC_WALLET_UNLOCK_NEEDED        = -13, //！<Enter the wallet passphrase with wallet passphrase first
RPC_WALLET_PASSPHRASE_INCORRECT = -14, //！<输入的钱包密码不正确
RPC_WALLET_WRONG_ENC_STATE      = -15, //！<在错误的钱包加密状态下发出的命令（加密加密钱包等）
RPC_WALLET_ENCRYPTION_FAILED    = -16, //！<加密钱包失败
RPC_WALLET_ALREADY_UNLOCKED     = -17, //！<钱包已解锁
RPC_WALLET_NOT_FOUND            = -18, //！<指定的钱包无效
RPC_WALLET_NOT_SPECIFIED        = -19, //！<未指定钱包（加载多个钱包时出错）

//！向后兼容别名
    RPC_WALLET_INVALID_ACCOUNT_NAME = RPC_WALLET_INVALID_LABEL_NAME,

//！未使用的保留代码，为向后兼容而保留。不要重复使用。
RPC_FORBIDDEN_BY_SAFE_MODE      = -2,  //！<服务器处于安全模式，安全模式下不允许使用命令
};

UniValue JSONRPCRequestObj(const std::string& strMethod, const UniValue& params, const UniValue& id);
UniValue JSONRPCReplyObj(const UniValue& result, const UniValue& error, const UniValue& id);
std::string JSONRPCReply(const UniValue& result, const UniValue& error, const UniValue& id);
UniValue JSONRPCError(int code, const std::string& message);

/*生成新的RPC身份验证cookie并将其写入磁盘*/
bool GenerateAuthCookie(std::string *cookie_out);
/*从磁盘读取RPC身份验证cookie*/
bool GetAuthCookie(std::string *cookie_out);
/*从磁盘删除RPC身份验证cookie*/
void DeleteAuthCookie();
/*将json-rpc批处理回复解析为向量*/
std::vector<UniValue> JSONRPCProcessBatchReply(const UniValue &in, size_t num);

#endif //比特币RPC协议
