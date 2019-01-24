
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2015比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_QT_PLATFORMSTYLE_H
#define BITCOIN_QT_PLATFORMSTYLE_H

#include <QIcon>
#include <QPixmap>
#include <QString>

/*特定于Coin网络的GUI样式信息*/
class PlatformStyle
{
public:
    /*获取与提供的平台名称关联的样式，如果不知道，则为0*/
    static const PlatformStyle *instantiate(const QString &platformId);

    const QString &getName() const { return name; }

    bool getImagesOnButtons() const { return imagesOnButtons; }
    bool getUseExtraSpacing() const { return useExtraSpacing; }

    QColor TextColor() const { return textColor; }
    QColor SingleColor() const { return singleColor; }

    /*用图标颜色给图像（给定文件名）着色*/
    QImage SingleColorImage(const QString& filename) const;

    /*用图标颜色为图标（给定文件名）着色*/
    QIcon SingleColorIcon(const QString& filename) const;

    /*用图标颜色给图标（给定对象）上色*/
    QIcon SingleColorIcon(const QIcon& icon) const;

    /*用文本颜色给图标（给定文件名）上色*/
    QIcon TextColorIcon(const QString& filename) const;

    /*用文本颜色给图标（给定对象）上色*/
    QIcon TextColorIcon(const QIcon& icon) const;

private:
    PlatformStyle(const QString &name, bool imagesOnButtons, bool colorizeIcons, bool useExtraSpacing);

    QString name;
    bool imagesOnButtons;
    bool colorizeIcons;
    bool useExtraSpacing;
    QColor singleColor;
    QColor textColor;
    /*…以后还会有更多*/
};

#endif //比特币qt平台式

