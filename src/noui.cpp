
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

#include <noui.h>

#include <ui_interface.h>
#include <util/system.h>

#include <cstdio>
#include <stdint.h>
#include <string>

#include <boost/signals2/connection.hpp>

bool noui_ThreadSafeMessageBox(const std::string& message, const std::string& caption, unsigned int style)
{
    bool fSecure = style & CClientUIInterface::SECURE;
    style &= ~CClientUIInterface::SECURE;

    std::string strCaption;
//检查预定义标题的用法
    switch (style) {
    case CClientUIInterface::MSG_ERROR:
        strCaption += _("Error");
        break;
    case CClientUIInterface::MSG_WARNING:
        strCaption += _("Warning");
        break;
    case CClientUIInterface::MSG_INFORMATION:
        strCaption += _("Information");
        break;
    default:
strCaption += caption; //使用提供的标题（可以为空）
    }

    if (!fSecure)
        LogPrintf("%s: %s\n", strCaption, message);
    fprintf(stderr, "%s: %s\n", strCaption.c_str(), message.c_str());
    return false;
}

/*l noui_threasafequestion（const std:：string&/*忽略的交互式消息*/，const std:：string&message，const std:：string&caption，无符号int样式）
{
    返回noui-threasafemessagebox（消息、标题、样式）；
}

void noui initmessage（const std:：string&message）
{
    logprintf（“初始化消息：%s\n”，消息）；
}

void noui_connect（）无效
{
    uiinterface.threasafemessagebox_connect（noui_threasafemessagebox）；
    uiinterface.threasafequestion_connect（noui_threasafequestion）；
    uiinterface.initmessage连接（noui-initmessage）；
}
