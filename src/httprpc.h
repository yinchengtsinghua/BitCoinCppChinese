
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2015-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_HTTPRPC_H
#define BITCOIN_HTTPRPC_H

#include <string>
#include <map>

/*启动HTTP RPC子系统。
 *前提条件；HTTP和RPC已启动。
 **/

bool StartHTTPRPC();
/*中断HTTP RPC子系统。
 **/

void InterruptHTTPRPC();
/*停止HTTP RPC子系统。
 *前提条件；HTTP和RPC已停止。
 **/

void StopHTTPRPC();

/*启动HTTP REST子系统。
 *前提条件；HTTP和RPC已启动。
 **/

void StartREST();
/*中断rpc rest子系统。
 **/

void InterruptREST();
/*停止HTTP REST子系统。
 *前提条件；HTTP和RPC已停止。
 **/

void StopREST();

#endif
