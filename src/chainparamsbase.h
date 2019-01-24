
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2014-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_CHAINPARAMSBASE_H
#define BITCOIN_CHAINPARAMSBASE_H

#include <memory>
#include <string>
#include <vector>

/*
 *cbasechainparams定义基本参数（bitcoin cli和bitcoind共享）
 *比特币系统的特定实例。
 **/

class CBaseChainParams
{
public:
    /*BIP70链名称字符串（MAIN、TEST或REGTEST）*/
    static const std::string MAIN;
    static const std::string TESTNET;
    static const std::string REGTEST;

    const std::string& DataDir() const { return strDataDir; }
    int RPCPort() const { return nRPCPort; }

    CBaseChainParams() = delete;
    CBaseChainParams(const std::string& data_dir, int rpc_port) : nRPCPort(rpc_port), strDataDir(data_dir) {}

private:
    int nRPCPort;
    std::string strDataDir;
};

/*
 *创建并返回所选链的std:：unique_ptr<cbasechainparams>。
 *@返回所选链的cbasechainparams*。
 *@如果不支持链，则抛出一个std:：runtime_错误。
 **/

std::unique_ptr<CBaseChainParams> CreateBaseChainParams(const std::string& chain);

/*
 *设置chainParams的参数
 **/

void SetupChainParamsBaseOptions();

/*
 *返回当前选择的参数。这在应用后不会改变
 *启动，单元测试除外。
 **/

const CBaseChainParams& BaseParams();

/*将params（）返回的参数设置为给定网络的参数。*/
void SelectBaseParams(const std::string& chain);

#endif //比特币链参数数据库
