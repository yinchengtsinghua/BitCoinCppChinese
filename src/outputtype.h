
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

#ifndef BITCOIN_OUTPUTTYPE_H
#define BITCOIN_OUTPUTTYPE_H

#include <attributes.h>
#include <keystore.h>
#include <script/standard.h>

#include <string>
#include <vector>

enum class OutputType {
    LEGACY,
    P2SH_SEGWIT,
    BECH32,

    /*
     *仅用于更改输出的特殊输出类型。自动选择类型
     *基于地址类型设置和其他非更改输出类型
     （请参阅中的-changetype选项文档和实现
     *cwallet:：TransactionChangeType了解详细信息）。
     **/

    CHANGE_AUTO,
};

NODISCARD bool ParseOutputType(const std::string& str, OutputType& output_type);
const std::string& FormatOutputType(OutputType type);

/*
 *将请求类型的目标（如果可能）获取到指定的密钥。
 *调用者必须确保事先调用了LearnRelatedScripts。
 **/

CTxDestination GetDestinationForKey(const CPubKey& key, OutputType);

/*获取给定密钥的钱包支持的所有目的地（可能）。*/
std::vector<CTxDestination> GetAllDestinationsForKey(const CPubKey& key);

/*
 *将请求类型的目标（如果可能）获取到指定的脚本。
 *此函数将自动添加脚本（以及任何其他
 *必要的脚本）。
 **/

CTxDestination AddAndGetDestinationForScript(CKeyStore& keystore, const CScript& script, OutputType);

#endif //比特币输出类型

