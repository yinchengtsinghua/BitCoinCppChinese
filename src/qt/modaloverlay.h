
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2016-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_QT_MODALOVERLAY_H
#define BITCOIN_QT_MODALOVERLAY_H

#include <QDateTime>
#include <QWidget>

//！在显示IBD进度之前，所需的头与可用头的估计数量之差。
static constexpr int HEADER_HEIGHT_DELTA_SYNC = 24;

namespace Ui {
    class ModalOverlay;
}

/*模式覆盖以显示链同步状态的信息*/
class ModalOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit ModalOverlay(QWidget *parent);
    ~ModalOverlay();

public Q_SLOTS:
    void tipUpdate(int count, const QDateTime& blockDate, double nVerificationProgress);
    void setKnownBestHeight(int count, const QDateTime& blockDate);

    void toggleVisibility();
//将显示或隐藏模式层
    void showHide(bool hide = false, bool userRequested = false);
    void closeClicked();
    bool isLayerVisible() const { return layerIsVisible; }

protected:
    bool eventFilter(QObject * obj, QEvent * ev);
    bool event(QEvent* ev);

private:
    Ui::ModalOverlay *ui;
int bestHeaderHeight; //已知高度（基于标题）
    QDateTime bestHeaderDate;
    QVector<QPair<qint64, double> > blockProcessTime;
    bool layerIsVisible;
    bool userClosed;
};

#endif //比特币
