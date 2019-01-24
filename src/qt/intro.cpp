
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

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <fs.h>
#include <qt/intro.h>
#include <qt/forms/ui_intro.h>

#include <qt/guiutil.h>

#include <interfaces/node.h>
#include <util/system.h>

#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>

#include <cmath>

static const uint64_t GB_BYTES = 1000000000LL;
/*总所需空间（单位：GB），取决于用户选择（删减，而不是删减）*/
static uint64_t requiredSpace;

/*异步检查可用空间以防止挂起UI线程。

   最多有一个检查路径的请求正在传送到此线程；当检查（）时
   函数运行时，当前路径是从相关的intro对象请求的。
   回复通过信号发送回来。

   这样可以确保在用户
   仍在输入路径，并且始终将最近输入的路径选中为
   一旦线程可用。
**/

class FreespaceChecker : public QObject
{
    Q_OBJECT

public:
    explicit FreespaceChecker(Intro *intro);

    enum Status {
        ST_OK,
        ST_ERROR
    };

public Q_SLOTS:
    void check();

Q_SIGNALS:
    void reply(int status, const QString &message, quint64 available);

private:
    Intro *intro;
};

#include <qt/intro.moc>

FreespaceChecker::FreespaceChecker(Intro *_intro)
{
    this->intro = _intro;
}

void FreespaceChecker::check()
{
    QString dataDirStr = intro->getPathToCheck();
    fs::path dataDir = GUIUtil::qstringToBoostPath(dataDirStr);
    uint64_t freeBytesAvailable = 0;
    int replyStatus = ST_OK;
    QString replyMessage = tr("A new data directory will be created.");

    /*查找存在的第一个父级，以便fs:：space不会失败*/
    fs::path parentDir = dataDir;
    fs::path parentDirOld = fs::path();
    while(parentDir.has_parent_path() && !fs::exists(parentDir))
    {
        parentDir = parentDir.parent_path();

        /*检查我们是否有任何进展，如果没有，请中断以防止在这里出现无限循环*/
        if (parentDirOld == parentDir)
            break;

        parentDirOld = parentDir;
    }

    try {
        freeBytesAvailable = fs::space(parentDir).available;
        if(fs::exists(dataDir))
        {
            if(fs::is_directory(dataDir))
            {
                QString separator = "<code>" + QDir::toNativeSeparators("/") + tr("name") + "</code>";
                replyStatus = ST_OK;
                replyMessage = tr("Directory already exists. Add %1 if you intend to create a new directory here.").arg(separator);
            } else {
                replyStatus = ST_ERROR;
                replyMessage = tr("Path already exists, and is not a directory.");
            }
        }
    } catch (const fs::filesystem_error&)
    {
        /*父目录不存在或不可访问*/
        replyStatus = ST_ERROR;
        replyMessage = tr("Cannot create data directory here.");
    }
    Q_EMIT reply(replyStatus, replyMessage, freeBytesAvailable);
}


Intro::Intro(QWidget *parent, uint64_t blockchain_size, uint64_t chain_state_size) :
    QDialog(parent),
    ui(new Ui::Intro),
    thread(nullptr),
    signalled(false),
    m_blockchain_size(blockchain_size),
    m_chain_state_size(chain_state_size)
{
    ui->setupUi(this);
    ui->welcomeLabel->setText(ui->welcomeLabel->text().arg(tr(PACKAGE_NAME)));
    ui->storageLabel->setText(ui->storageLabel->text().arg(tr(PACKAGE_NAME)));

    ui->lblExplanation1->setText(ui->lblExplanation1->text()
        .arg(tr(PACKAGE_NAME))
        .arg(m_blockchain_size)
        .arg(2009)
        .arg(tr("Bitcoin"))
    );
    ui->lblExplanation2->setText(ui->lblExplanation2->text().arg(tr(PACKAGE_NAME)));

    uint64_t pruneTarget = std::max<int64_t>(0, gArgs.GetArg("-prune", 0));
    requiredSpace = m_blockchain_size;
    QString storageRequiresMsg = tr("At least %1 GB of data will be stored in this directory, and it will grow over time.");
    if (pruneTarget) {
        uint64_t prunedGBs = std::ceil(pruneTarget * 1024 * 1024.0 / GB_BYTES);
        if (prunedGBs <= requiredSpace) {
            requiredSpace = prunedGBs;
            storageRequiresMsg = tr("Approximately %1 GB of data will be stored in this directory.");
        }
        ui->lblExplanation3->setVisible(true);
    } else {
        ui->lblExplanation3->setVisible(false);
    }
    requiredSpace += m_chain_state_size;
    ui->sizeWarningLabel->setText(
        tr("%1 will download and store a copy of the Bitcoin block chain.").arg(tr(PACKAGE_NAME)) + " " +
        storageRequiresMsg.arg(requiredSpace) + " " +
        tr("The wallet will also be stored in this directory.")
    );
    startThread();
}

Intro::~Intro()
{
    delete ui;
    /*确保线程在删除前完成*/
    thread->quit();
    thread->wait();
}

QString Intro::getDataDirectory()
{
    return ui->dataDirectory->text();
}

void Intro::setDataDirectory(const QString &dataDir)
{
    ui->dataDirectory->setText(dataDir);
    if(dataDir == getDefaultDataDirectory())
    {
        ui->dataDirDefault->setChecked(true);
        ui->dataDirectory->setEnabled(false);
        ui->ellipsisButton->setEnabled(false);
    } else {
        ui->dataDirCustom->setChecked(true);
        ui->dataDirectory->setEnabled(true);
        ui->ellipsisButton->setEnabled(true);
    }
}

QString Intro::getDefaultDataDirectory()
{
    return GUIUtil::boostPathToQString(GetDefaultDataDir());
}

bool Intro::pickDataDirectory(interfaces::Node& node)
{
    QSettings settings;
    /*如果命令行上提供了数据目录，则无需查看设置
       或显示拣配对话框*/

    if(!gArgs.GetArg("-datadir", "").empty())
        return true;
    /*1）操作系统默认数据目录*/
    QString dataDir = getDefaultDataDirectory();
    /*2）允许qsettings覆盖默认目录*/
    dataDir = settings.value("strDataDir", dataDir).toString();

    if(!fs::exists(GUIUtil::qstringToBoostPath(dataDir)) || gArgs.GetBoolArg("-choosedatadir", DEFAULT_CHOOSE_DATADIR) || settings.value("fReset", false).toBool() || gArgs.GetBoolArg("-resetguisettings", false))
    {
        /*此处使用selectparams确保params（）可由节点接口使用*/
        try {
            node.selectParams(gArgs.GetChainName());
        } catch (const std::exception&) {
            return false;
        }

        /*如果当前默认数据目录不存在，则让用户选择一个*/
        Intro intro(0, node.getAssumedBlockchainSize(), node.getAssumedChainStateSize());
        intro.setDataDirectory(dataDir);
        intro.setWindowIcon(QIcon(":icons/bitcoin"));

        while(true)
        {
            if(!intro.exec())
            {
                /*取消单击*/
                return false;
            }
            dataDir = intro.getDataDirectory();
            try {
                if (TryCreateDirectories(GUIUtil::qstringToBoostPath(dataDir))) {
//如果创建了一个新的数据目录，也可以创建wallets子目录。
                    TryCreateDirectories(GUIUtil::qstringToBoostPath(dataDir) / "wallets");
                }
                break;
            } catch (const fs::filesystem_error&) {
                QMessageBox::critical(nullptr, tr(PACKAGE_NAME),
                    tr("Error: Specified data directory \"%1\" cannot be created.").arg(dataDir));
                /*通过，返回选择屏幕*/
            }
        }

        settings.setValue("strDataDir", dataDir);
        settings.setValue("fReset", false);
    }
    /*仅当与默认值不同时才重写-datadir，以便
     *覆盖-默认数据目录中bitcoin.conf文件中的datadir
     （与比特币行为一致）
     **/

    if(dataDir != getDefaultDataDirectory()) {
node.softSetArg("-datadir", GUIUtil::qstringToBoostPath(dataDir).string()); //使用操作系统区域设置进行路径设置
    }
    return true;
}

void Intro::setStatus(int status, const QString &message, quint64 bytesAvailable)
{
    switch(status)
    {
    case FreespaceChecker::ST_OK:
        ui->errorMessage->setText(message);
        ui->errorMessage->setStyleSheet("");
        break;
    case FreespaceChecker::ST_ERROR:
        ui->errorMessage->setText(tr("Error") + ": " + message);
        ui->errorMessage->setStyleSheet("QLabel { color: #800000 }");
        break;
    }
    /*指示可用字节数*/
    if(status == FreespaceChecker::ST_ERROR)
    {
        ui->freeSpace->setText("");
    } else {
        QString freeString = tr("%n GB of free space available", "", bytesAvailable/GB_BYTES);
        if(bytesAvailable < requiredSpace * GB_BYTES)
        {
            freeString += " " + tr("(of %n GB needed)", "", requiredSpace);
            ui->freeSpace->setStyleSheet("QLabel { color: #800000 }");
        } else {
            ui->freeSpace->setStyleSheet("");
        }
        ui->freeSpace->setText(freeString + ".");
    }
    /*不允许在错误状态下确认*/
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(status != FreespaceChecker::ST_ERROR);
}

void Intro::on_dataDirectory_textChanged(const QString &dataDirStr)
{
    /*禁用“确定”按钮，直到检查结果出现*/
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    checkPath(dataDirStr);
}

void Intro::on_ellipsisButton_clicked()
{
    QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(nullptr, "Choose data directory", ui->dataDirectory->text()));
    if(!dir.isEmpty())
        ui->dataDirectory->setText(dir);
}

void Intro::on_dataDirDefault_clicked()
{
    setDataDirectory(getDefaultDataDirectory());
}

void Intro::on_dataDirCustom_clicked()
{
    ui->dataDirectory->setEnabled(true);
    ui->ellipsisButton->setEnabled(true);
}

void Intro::startThread()
{
    thread = new QThread(this);
    FreespaceChecker *executor = new FreespaceChecker(this);
    executor->moveToThread(thread);

    connect(executor, &FreespaceChecker::reply, this, &Intro::setStatus);
    connect(this, &Intro::requestCheck, executor, &FreespaceChecker::check);
    /*确保执行器对象在其自己的线程中被删除*/
    connect(thread, &QThread::finished, executor, &QObject::deleteLater);

    thread->start();
}

void Intro::checkPath(const QString &dataDir)
{
    mutex.lock();
    pathToCheck = dataDir;
    if(!signalled)
    {
        signalled = true;
        Q_EMIT requestCheck();
    }
    mutex.unlock();
}

QString Intro::getPathToCheck()
{
    QString retval;
    mutex.lock();
    retval = pathToCheck;
    /*nalled=false；/*新请求现在可以排队*/
    互斥锁。
    返回；
}
