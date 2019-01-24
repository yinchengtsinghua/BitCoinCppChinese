
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_QT_NOTIFICATOR_H
#define BITCOIN_QT_NOTIFICATOR_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <QIcon>
#include <QObject>

QT_BEGIN_NAMESPACE
class QSystemTrayIcon;

#ifdef USE_DBUS
class QDBusInterface;
#endif
QT_END_NAMESPACE

/*跨平台桌面通知客户端。*/
class Notificator: public QObject
{
    Q_OBJECT

public:
    /*新建通知程序。
       @note trayicon的所有权不会转移到此对象。
    **/

    Notificator(const QString &programName, QSystemTrayIcon *trayIcon, QWidget *parent);
    ~Notificator();

//消息类
    enum Class
    {
        /*格式，/**<信息性消息*/
        警告，/**<通知用户潜在问题*/

        /*tical/**<发生错误*/
    }；

公共QS槽：
    /**显示通知消息。
       @param[in]cls常规消息类
       @param[in]标题与消息一起显示
       @param[in]文本消息内容
       @param[in]图标可选图标与消息一起显示
       @param[in]毫秒超时通知超时（毫秒）（默认为10秒）
       @note平台实现可以忽略提供的任何字段，除了\一个文本。
     **/

    void notify(Class cls, const QString &title, const QString &text,
                const QIcon &icon = QIcon(), int millisTimeout = 10000);

private:
    QWidget *parent;
    enum Mode {
        /*e，/**<忽略信息通知，并显示关键通知的模式弹出对话框。*/
        freedesktop，/**<使用dbus org.freedesktop.notifications*/

        /*stemtray，/**<使用qSystemTrayIcon:：ShowMessage（）*/
        user notification center/**<使用10.8+用户通知中心（仅限Mac）*/

    };
    QString programName;
    Mode mode;
    QSystemTrayIcon *trayIcon;
#ifdef USE_DBUS
    QDBusInterface *interface;

    void notifyDBus(Class cls, const QString &title, const QString &text, const QIcon &icon, int millisTimeout);
#endif
    void notifySystray(Class cls, const QString &title, const QString &text, int millisTimeout);
#ifdef Q_OS_MAC
    void notifyMacUserNotificationCenter(const QString &title, const QString &text);
#endif
};

#endif //比特币qt通知器
