
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

#ifndef BITCOIN_QT_WINSHUTDOWNMONITOR_H
#define BITCOIN_QT_WINSHUTDOWNMONITOR_H

#ifdef WIN32
#include <QByteArray>
#include <QString>

#include <windef.h> //对于HWND

#include <QAbstractNativeEventFilter>

class WinShutdownMonitor : public QAbstractNativeEventFilter
{
public:
    /*实现用于处理Windows消息的QabstractNativeEventFilter接口*/
    bool nativeEventFilter(const QByteArray &eventType, void *pMessage, long *pnResult);

    /*在Windows上注册阻止关闭的原因以允许干净的客户端退出*/
    static void registerShutdownBlockReason(const QString& strReason, const HWND& mainWinId);
};
#endif

#endif //比特币Qt_Winshutdown监测仪
