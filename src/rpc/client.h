
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

#ifndef BITCOIN_RPC_CLIENT_H
#define BITCOIN_RPC_CLIENT_H

#include <univalue.h>

/*将位置参数转换为命令特定的RPC表示形式*/
UniValue RPCConvertValues(const std::string& strMethod, const std::vector<std::string>& strParams);

/*将命名参数转换为命令特定的RPC表示形式*/
UniValue RPCConvertNamedValues(const std::string& strMethod, const std::vector<std::string>& strParams);

/*非rfc4627 JSON解析器，接受内部值（如数字、真、假、空）
 以及对象和数组。
 **/

UniValue ParseNonRFCJSONValue(const std::string& strVal);

#endif //比特币RPC客户机
