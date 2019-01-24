
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

#ifndef BITCOIN_QT_GUIUTIL_H
#define BITCOIN_QT_GUIUTIL_H

#include <amount.h>
#include <fs.h>

#include <QEvent>
#include <QHeaderView>
#include <QItemDelegate>
#include <QMessageBox>
#include <QObject>
#include <QProgressBar>
#include <QString>
#include <QTableView>
#include <QLabel>

class QValidatedLineEdit;
class SendCoinsRecipient;

namespace interfaces
{
    class Node;
}

QT_BEGIN_NAMESPACE
class QAbstractItemView;
class QDateTime;
class QFont;
class QLineEdit;
class QProgressDialog;
class QUrl;
class QWidget;
QT_END_NAMESPACE

/*比特币qt用户界面使用的实用功能。
 **/

namespace GUIUtil
{
//从日期创建可读字符串
    QString dateTimeStr(const QDateTime &datetime);
    QString dateTimeStr(qint64 nTime);

//返回单空格字体
    QFont fixedPitchFont();

//设置地址小部件
    void setupAddressWidget(QValidatedLineEdit *widget, QWidget *parent);

//将“bitcoin:”uri解析到收件人对象中，成功解析后返回true
    bool parseBitcoinURI(const QUrl &uri, SendCoinsRecipient *out);
    bool parseBitcoinURI(QString uri, SendCoinsRecipient *out);
    QString formatBitcoinURI(const SendCoinsRecipient &info);

//如果给定地址+金额满足“灰尘”定义，则返回true
    bool isDust(interfaces::Node& node, const QString& address, const CAmount& amount);

//富文本控件的HTML转义
    QString HtmlEscape(const QString& str, bool fMultiLine=false);
    QString HtmlEscape(const std::string& str, bool fMultiLine=false);

    /*将视图当前选定条目的字段复制到剪贴板。什么都不做
        被选中。
       要从模型中提取的@param[in]列数据列
       要从模型中提取的@param[in]角色数据角色
       @请参见TransactionView:：CopyLabel、TransactionView:：CopyAmount、TransactionView:：CopyAddress
     **/

    void copyEntryData(QAbstractItemView *view, int column, int role=Qt::EditRole);

    /*返回当前所选条目的字段作为qstring。什么都不做
        被选中。
       要从模型中提取的@param[in]列数据列
       @请参见TransactionView:：CopyLabel、TransactionView:：CopyAmount、TransactionView:：CopyAddress
     **/

    QList<QModelIndex> getEntryData(QAbstractItemView *view, int column);

    void setClipboard(const QString& str);

    /*获取保存文件名，模仿qfiledialog:：get save filename，只是它附加了一个默认后缀
        当用户没有提供后缀时。

      @param[in]父窗口（或0）
      @param[in]标题窗口标题（默认为空）
      @param[in]dir启动目录（或为空，默认为documents目录）
      @param[in]筛选器筛选器规范，如“逗号分隔的文件（*.csv）”
      @param[out]selected suffix out指针返回所选后缀（文件类型）（或0）。
                  在根据后缀选择保存文件格式时非常有用。
     **/

    QString getSaveFileName(QWidget *parent, const QString &caption, const QString &dir,
        const QString &filter,
        QString *selectedSuffixOut);

    /*获取打开文件名，qfiledialog:：get open filename的方便包装。

      @param[in]父窗口（或0）
      @param[in]标题窗口标题（默认为空）
      @param[in]dir启动目录（或为空，默认为documents目录）
      @param[in]筛选器筛选器规范，如“逗号分隔的文件（*.csv）”
      @param[out]selected suffix out指针返回所选后缀（文件类型）（或0）。
                  在根据后缀选择保存文件格式时非常有用。
     **/

    QString getOpenFileName(QWidget *parent, const QString &caption, const QString &dir,
        const QString &filter,
        QString *selectedSuffixOut);

    /*获取连接类型以使用InvokeMethod调用GUI线程中的对象槽。呼叫将被阻塞。

       @如果从GUI线程调用，则返回qt:：directconnection。
                如果从另一个线程调用，则返回qt:：BlockingQueuedConnection。
    **/

    Qt::ConnectionType blockingGUIThreadConnection();

//确定小部件是否隐藏在其他窗口后面
    bool isObscured(QWidget *w);

//激活、显示和提升小部件
    void bringToFront(QWidget* w);

//打开调试日志
    void openDebugLogfile();

//打开配置文件
    bool openBitcoinConf();

    /*qt事件筛选器，用于截取tooltipchange事件，并用富文本替换工具提示
      如有需要，请提供代表。这可以确保qt可以自动换行长的工具提示消息。
      超过所提供的大小阈值（以字符为单位）的工具提示将被包装。
     **/

    class ToolTipToRichTextFilter : public QObject
    {
        Q_OBJECT

    public:
        explicit ToolTipToRichTextFilter(int size_threshold, QObject *parent = nullptr);

    protected:
        bool eventFilter(QObject *obj, QEvent *evt);

    private:
        int size_threshold;
    };

    /*
     *使qTableView最后一列感觉好像是从左边框调整大小。
     *还要确保列宽永远不会大于表的视区。
     *在qt中，所有列都可以从右侧调整大小，但是从右侧调整最后一列的大小并不是直观的。
     *通常，我们的第二列到最后一列的行为就像拉伸一样，当处于拉伸模式时，列的大小不可调整。
     *以交互方式或编程方式。
     *
     *此助手对象处理此问题。
     *
     **/

    class TableViewLastColumnResizingFixer: public QObject
    {
        Q_OBJECT

        public:
            TableViewLastColumnResizingFixer(QTableView* table, int lastColMinimumWidth, int allColsMinimumWidth, QObject *parent);
            void stretchColumnWidth(int column);

        private:
            QTableView* tableView;
            int lastColumnMinimumWidth;
            int allColumnsMinimumWidth;
            int lastColumnIndex;
            int columnCount;
            int secondToLastColumnIndex;

            void adjustTableColumnsWidth();
            int getAvailableWidthForColumn(int column);
            int getColumnsWidth();
            void connectViewHeadersSignals();
            void disconnectViewHeadersSignals();
            void setViewHeaderResizeMode(int logicalIndex, QHeaderView::ResizeMode resizeMode);
            void resizeColumn(int nColumnIndex, int width);

        private Q_SLOTS:
            void on_sectionResized(int logicalIndex, int oldSize, int newSize);
            void on_geometriesChanged();
    };

    bool GetStartOnSystemStartup();
    bool SetStartOnSystemStartup(bool fAutoStart);

    /*通过utf-8将qstring转换为操作系统特定的增强路径*/
    fs::path qstringToBoostPath(const QString &path);

    /*通过utf-8将操作系统特定的增强路径转换为qstring*/
    QString boostPathToQString(const fs::path &path);

    /*将秒转换为具有天、小时、分钟、秒的qstring*/
    QString formatDurationStr(int secs);

    /*将cnodestats.services位掩码格式化为用户可读字符串*/
    QString formatServicesStr(quint64 mask);

    /*将cnodecombinedstats.dpingtime格式化为用户可读字符串或显示N/A，如果*/
    QString formatPingTime(double dPingTime);

    /*将cnodecombinedstats.ntimeoffset格式化为用户可读的字符串。*/
    QString formatTimeOffset(int64_t nTimeOffset);

    QString formatNiceTimeOffset(qint64 secs);

    QString formatBytes(uint64_t bytes);

    qreal calculateIdealFontSize(int width, const QString& text, QFont font, qreal minPointSize = 4, qreal startPointSize = 14);

    class ClickableLabel : public QLabel
    {
        Q_OBJECT

    Q_SIGNALS:
        /*单击标签时发出。单击的相对鼠标坐标为
         *传递给信号。
         **/

        void clicked(const QPoint& point);
    protected:
        void mouseReleaseEvent(QMouseEvent *event);
    };

    class ClickableProgressBar : public QProgressBar
    {
        Q_OBJECT

    Q_SIGNALS:
        /*单击ProgressBar时发出。单击的相对鼠标坐标为
         *传递给信号。
         **/

        void clicked(const QPoint& point);
    protected:
        void mouseReleaseEvent(QMouseEvent *event);
    };

    typedef ClickableProgressBar ProgressBar;

    class ItemDelegate : public QItemDelegate
    {
        Q_OBJECT
    public:
        ItemDelegate(QObject* parent) : QItemDelegate(parent) {}

    Q_SIGNALS:
        void keyEscapePressed();

    private:
        bool eventFilter(QObject *object, QEvent *event);
    };

//修复qprogressdialog类中的已知错误。
    void PolishProgressDialog(QProgressDialog* dialog);
} //命名空间guiutil

#endif //比特币
