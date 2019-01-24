
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2013-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_NOUI_H
#define BITCOIN_NOUI_H

#include <string>

/*非GUI处理程序，用于记录和打印消息。*/
bool noui_ThreadSafeMessageBox(const std::string& message, const std::string& caption, unsigned int style);
/*非GUI处理程序，用于记录和打印问题。*/
/*noui_threasafequestion（const std:：string&/*忽略的交互式消息*/，const std:：string&message，const std:：string&caption，无符号int样式）；
/**非GUI处理程序，只记录消息。*/

void noui_InitMessage(const std::string& message);

/*连接所有比特币信号处理程序*/
void noui_connect();

#endif //比特科尼
