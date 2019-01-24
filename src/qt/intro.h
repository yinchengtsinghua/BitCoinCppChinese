
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

#ifndef BITCOIN_QT_INTRO_H
#define BITCOIN_QT_INTRO_H

#include <QDialog>
#include <QMutex>
#include <QThread>

static const bool DEFAULT_CHOOSE_DATADIR = false;

class FreespaceChecker;

namespace interfaces {
    class Node;
}

namespace Ui {
    class Intro;
}

/*简介屏幕（GUI启动前）。
  允许用户选择数据目录，
  存放钱包和锁链的地方。
 **/

class Intro : public QDialog
{
    Q_OBJECT

public:
    explicit Intro(QWidget *parent = nullptr,
                   uint64_t blockchain_size = 0, uint64_t chain_state_size = 0);
    ~Intro();

    QString getDataDirectory();
    void setDataDirectory(const QString &dataDir);

    /*
     *确定数据目录。让用户选择当前的不存在。
     *
     *@如果选择了数据目录，则返回“真”；如果用户取消了选择，则返回“假”。
     *对话框。
     *
     *@注意在调用此函数之前不要调用global getdatadir（），这是
     *将导致缓存错误的路径。
     **/

    static bool pickDataDirectory(interfaces::Node& node);

    /*
     *确定操作系统的默认数据目录。
     **/

    static QString getDefaultDataDirectory();

Q_SIGNALS:
    void requestCheck();

public Q_SLOTS:
    void setStatus(int status, const QString &message, quint64 bytesAvailable);

private Q_SLOTS:
    void on_dataDirectory_textChanged(const QString &arg1);
    void on_ellipsisButton_clicked();
    void on_dataDirDefault_clicked();
    void on_dataDirCustom_clicked();

private:
    Ui::Intro *ui;
    QThread *thread;
    QMutex mutex;
    bool signalled;
    QString pathToCheck;
    uint64_t m_blockchain_size;
    uint64_t m_chain_state_size;

    void startThread();
    void checkPath(const QString &dataDir);
    QString getPathToCheck();

    friend class FreespaceChecker;
};

#endif //比特币qt_简介
