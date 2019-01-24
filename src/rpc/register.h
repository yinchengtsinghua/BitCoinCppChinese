
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_RPC_REGISTER_H
#define BITCOIN_RPC_REGISTER_H

/*它们位于一个头文件中，以避免创建大量的单个函数
 *SRC/RPC/*/

class CRPCTable;

/*注册块链RPC命令*/
void RegisterBlockchainRPCCommands(CRPCTable &tableRPC);
/*注册p2p网络RPC命令*/
void RegisterNetRPCCommands(CRPCTable &tableRPC);
/*注册其他RPC命令*/
void RegisterMiscRPCCommands(CRPCTable &tableRPC);
/*注册挖掘RPC命令*/
void RegisterMiningRPCCommands(CRPCTable &tableRPC);
/*注册原始事务RPC命令*/
void RegisterRawTransactionRPCCommands(CRPCTable &tableRPC);

static inline void RegisterAllCoreRPCCommands(CRPCTable &t)
{
    RegisterBlockchainRPCCommands(t);
    RegisterNetRPCCommands(t);
    RegisterMiscRPCCommands(t);
    RegisterMiningRPCCommands(t);
    RegisterRawTransactionRPCCommands(t);
}

#endif //比特币注册
