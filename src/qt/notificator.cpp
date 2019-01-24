
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

#include <qt/notificator.h>

#include <QApplication>
#include <QByteArray>
#include <QImageWriter>
#include <QMessageBox>
#include <QMetaType>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTemporaryFile>
#include <QVariant>
#ifdef USE_DBUS
#include <stdint.h>
#include <QtDBus>
#endif
//在qtbus后面包含applicationservices.h，以避免重新定义check（）。
//这至少影响OSX 10.6。有关详细信息，请参阅/usr/include/assertmacros.h。
//注意：这也可以通过以下方法解决：
//定义\断言\宏\定义没有下划线0的\版本
#ifdef Q_OS_MAC
#include <ApplicationServices/ApplicationServices.h>
#include <qt/macnotificationhandler.h>
#endif


#ifdef USE_DBUS
//https://wiki.ubuntu.com/notificationdevelopmentguidelines建议至少128
const int FREEDESKTOP_NOTIFICATION_ICON_SIZE = 128;
#endif

Notificator::Notificator(const QString &_programName, QSystemTrayIcon *_trayIcon, QWidget *_parent) :
    QObject(_parent),
    parent(_parent),
    programName(_programName),
    mode(None),
    trayIcon(_trayIcon)
#ifdef USE_DBUS
    ,interface(nullptr)
#endif
{
    if(_trayIcon && _trayIcon->supportsMessages())
    {
        mode = QSystemTray;
    }
#ifdef USE_DBUS
    interface = new QDBusInterface("org.freedesktop.Notifications",
        "/org/freedesktop/Notifications", "org.freedesktop.Notifications");
    if(interface->isValid())
    {
        mode = Freedesktop;
    }
#endif
#ifdef Q_OS_MAC
//检查用户操作系统是否支持nsusernotification
    if( MacNotificationHandler::instance()->hasUserNotificationCenterSupport()) {
        mode = UserNotificationCenter;
    }
#endif
}

Notificator::~Notificator()
{
#ifdef USE_DBUS
    delete interface;
#endif
}

#ifdef USE_DBUS

//大致基于http://www.qtcenter.org/archive/index.php/t-25879.html
class FreedesktopImage
{
public:
    FreedesktopImage() {}
    explicit FreedesktopImage(const QImage &img);

    static int metaType();

//映像到变量，可以通过dbus进行编组
    static QVariant toVariant(const QImage &img);

private:
    int width, height, stride;
    bool hasAlpha;
    int channels;
    int bitsPerSample;
    QByteArray image;

    friend QDBusArgument &operator<<(QDBusArgument &a, const FreedesktopImage &i);
    friend const QDBusArgument &operator>>(const QDBusArgument &a, FreedesktopImage &i);
};

Q_DECLARE_METATYPE(FreedesktopImage);

//图像配置设置
const int CHANNELS = 4;
const int BYTES_PER_PIXEL = 4;
const int BITS_PER_SAMPLE = 8;

FreedesktopImage::FreedesktopImage(const QImage &img):
    width(img.width()),
    height(img.height()),
    stride(img.width() * BYTES_PER_PIXEL),
    hasAlpha(true),
    channels(CHANNELS),
    bitsPerSample(BITS_PER_SAMPLE)
{
//将00xaarrgbb转换为rgba bytewise（endian-independent）格式
    QImage tmp = img.convertToFormat(QImage::Format_ARGB32);
    const uint32_t *data = reinterpret_cast<const uint32_t*>(tmp.bits());

    unsigned int num_pixels = width * height;
    image.resize(num_pixels * BYTES_PER_PIXEL);

    for(unsigned int ptr = 0; ptr < num_pixels; ++ptr)
    {
image[ptr*BYTES_PER_PIXEL+0] = data[ptr] >> 16; //R
image[ptr*BYTES_PER_PIXEL+1] = data[ptr] >> 8;  //G
image[ptr*BYTES_PER_PIXEL+2] = data[ptr];       //乙
image[ptr*BYTES_PER_PIXEL+3] = data[ptr] >> 24; //一
    }
}

QDBusArgument &operator<<(QDBusArgument &a, const FreedesktopImage &i)
{
    a.beginStructure();
    a << i.width << i.height << i.stride << i.hasAlpha << i.bitsPerSample << i.channels << i.image;
    a.endStructure();
    return a;
}

const QDBusArgument &operator>>(const QDBusArgument &a, FreedesktopImage &i)
{
    a.beginStructure();
    a >> i.width >> i.height >> i.stride >> i.hasAlpha >> i.bitsPerSample >> i.channels >> i.image;
    a.endStructure();
    return a;
}

int FreedesktopImage::metaType()
{
    return qDBusRegisterMetaType<FreedesktopImage>();
}

QVariant FreedesktopImage::toVariant(const QImage &img)
{
    FreedesktopImage fimg(img);
    return QVariant(FreedesktopImage::metaType(), &fimg);
}

void Notificator::notifyDBus(Class cls, const QString &title, const QString &text, const QIcon &icon, int millisTimeout)
{
//https://developer.gnome.org/notification-spec/
//dbus“notify”调用的参数：
    QList<QVariant> args;

//程序名称：
    args.append(programName);

//替换ID；值为0表示此通知不会替换任何现有通知：
    args.append(0U);

//应用程序图标，空字符串
    args.append(QString());

//总结
    args.append(title);

//身体
    args.append(text);

//操作（无，操作已弃用）
    QStringList actions;
    args.append(actions);

//提示
    QVariantMap hints;

//如果没有指定图标，请根据类设置图标
    QIcon tmpicon;
    if(icon.isNull())
    {
        QStyle::StandardPixmap sicon = QStyle::SP_MessageBoxQuestion;
        switch(cls)
        {
        case Information: sicon = QStyle::SP_MessageBoxInformation; break;
        case Warning: sicon = QStyle::SP_MessageBoxWarning; break;
        case Critical: sicon = QStyle::SP_MessageBoxCritical; break;
        default: break;
        }
        tmpicon = QApplication::style()->standardIcon(sicon);
    }
    else
    {
        tmpicon = icon;
    }
    hints["icon_data"] = FreedesktopImage::toVariant(tmpicon.pixmap(FREEDESKTOP_NOTIFICATION_ICON_SIZE).toImage());
    args.append(hints);

//超时（毫秒）
    args.append(millisTimeout);

//“失火而忘”
    interface->callWithArgumentList(QDBus::NoBlock, "Notify", args);
}
#endif

void Notificator::notifySystray(Class cls, const QString &title, const QString &text, int millisTimeout)
{
    QSystemTrayIcon::MessageIcon sicon = QSystemTrayIcon::NoIcon;
switch(cls) //基于类设置图标
    {
    case Information: sicon = QSystemTrayIcon::Information; break;
    case Warning: sicon = QSystemTrayIcon::Warning; break;
    case Critical: sicon = QSystemTrayIcon::Critical; break;
    }
    trayIcon->showMessage(title, text, sicon, millisTimeout);
}

#ifdef Q_OS_MAC
void Notificator::notifyMacUserNotificationCenter(const QString &title, const QString &text)
{
//用户通知中心尚不支持图标。OSX将使用应用程序图标。
    MacNotificationHandler::instance()->showNotification(title, text);
}
#endif

void Notificator::notify(Class cls, const QString &title, const QString &text, const QIcon &icon, int millisTimeout)
{
    switch(mode)
    {
#ifdef USE_DBUS
    case Freedesktop:
        notifyDBus(cls, title, text, icon, millisTimeout);
        break;
#endif
    case QSystemTray:
        notifySystray(cls, title, text, millisTimeout);
        break;
#ifdef Q_OS_MAC
    case UserNotificationCenter:
        notifyMacUserNotificationCenter(title, text);
        break;
#endif
    default:
        if(cls == Critical)
        {
//如果重要且没有其他通知可用，则返回到老式弹出对话框
            QMessageBox::critical(parent, title, text, QMessageBox::Ok, QMessageBox::Ok);
        }
        break;
    }
}
