
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

#include <qt/networkstyle.h>

#include <qt/guiconstants.h>

#include <QApplication>

static const struct {
    const char *networkId;
    const char *appName;
    const int iconColorHueShift;
    const int iconColorSaturationReduction;
    const char *titleAddText;
} network_styles[] = {
    {"main", QAPP_APP_NAME_DEFAULT, 0, 0, ""},
    {"test", QAPP_APP_NAME_TESTNET, 70, 30, QT_TRANSLATE_NOOP("SplashScreen", "[testnet]")},
    {"regtest", QAPP_APP_NAME_REGTEST, 160, 30, "[regtest]"}
};
static const unsigned network_styles_count = sizeof(network_styles)/sizeof(*network_styles);

//titleAddText必须是tr（）的const char*。
NetworkStyle::NetworkStyle(const QString &_appName, const int iconColorHueShift, const int iconColorSaturationReduction, const char *_titleAddText):
    appName(_appName),
    titleAddText(qApp->translate("SplashScreen", _titleAddText))
{
//加载像素映射
    QPixmap pixmap(":/icons/bitcoin");

    if(iconColorHueShift != 0 && iconColorSaturationReduction != 0)
    {
//从qpixmap生成qimage
        QImage img = pixmap.toImage();

        int h,s,l,a;

//穿过直线
        for(int y=0;y<img.height();y++)
        {
            QRgb *scL = reinterpret_cast< QRgb *>( img.scanLine( y ) );

//循环通过像素
            for(int x=0;x<img.width();x++)
            {
//保留alpha，因为qcolor:：gethsl不返回alpha值
                a = qAlpha(scL[x]);
                QColor col(scL[x]);

//获得色调值
                col.getHsl(&h,&s,&l);

//在RGB颜色圆上旋转颜色
//70°应以典型的“TESTNET”绿色结束。
                h+=iconColorHueShift;

//更改饱和值
                if(s>iconColorSaturationReduction)
                {
                    s -= iconColorSaturationReduction;
                }
                col.setHsl(h,s,l,a);

//设置像素
                scL[x] = col.rgba();
            }
        }

//转换回qpixmap
        pixmap.convertFromImage(img);
    }

    appIcon             = QIcon(pixmap);
    trayAndWindowIcon   = QIcon(pixmap.scaled(QSize(256,256)));
}

const NetworkStyle *NetworkStyle::instantiate(const QString &networkId)
{
    for (unsigned x=0; x<network_styles_count; ++x)
    {
        if (networkId == network_styles[x].networkId)
        {
            return new NetworkStyle(
                    network_styles[x].appName,
                    network_styles[x].iconColorHueShift,
                    network_styles[x].iconColorSaturationReduction,
                    network_styles[x].titleAddText);
        }
    }
    return nullptr;
}
