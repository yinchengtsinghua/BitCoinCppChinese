
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

#include <qt/walletview.h>

#include <qt/addressbookpage.h>
#include <qt/askpassphrasedialog.h>
#include <qt/bitcoingui.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/overviewpage.h>
#include <qt/platformstyle.h>
#include <qt/receivecoinsdialog.h>
#include <qt/sendcoinsdialog.h>
#include <qt/signverifymessagedialog.h>
#include <qt/transactiontablemodel.h>
#include <qt/transactionview.h>
#include <qt/walletmodel.h>

#include <interfaces/node.h>
#include <ui_interface.h>

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QProgressDialog>
#include <QPushButton>
#include <QVBoxLayout>

WalletView::WalletView(const PlatformStyle *_platformStyle, QWidget *parent):
    QStackedWidget(parent),
    clientModel(nullptr),
    walletModel(nullptr),
    platformStyle(_platformStyle)
{
//创建标签
    overviewPage = new OverviewPage(platformStyle);

    transactionsPage = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout();
    QHBoxLayout *hbox_buttons = new QHBoxLayout();
    transactionView = new TransactionView(platformStyle, this);
    vbox->addWidget(transactionView);
    QPushButton *exportButton = new QPushButton(tr("&Export"), this);
    exportButton->setToolTip(tr("Export the data in the current tab to a file"));
    if (platformStyle->getImagesOnButtons()) {
        exportButton->setIcon(platformStyle->SingleColorIcon(":/icons/export"));
    }
    hbox_buttons->addStretch();
    hbox_buttons->addWidget(exportButton);
    vbox->addLayout(hbox_buttons);
    transactionsPage->setLayout(vbox);

    receiveCoinsPage = new ReceiveCoinsDialog(platformStyle);
    sendCoinsPage = new SendCoinsDialog(platformStyle);

    usedSendingAddressesPage = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::SendingTab, this);
    usedReceivingAddressesPage = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::ReceivingTab, this);

    addWidget(overviewPage);
    addWidget(transactionsPage);
    addWidget(receiveCoinsPage);
    addWidget(sendCoinsPage);

//点击总览上的交易，在交易历史页面上预先选择交易。
    connect(overviewPage, &OverviewPage::transactionClicked, transactionView, static_cast<void (TransactionView::*)(const QModelIndex&)>(&TransactionView::focusTransaction));

    connect(overviewPage, &OverviewPage::outOfSyncWarningClicked, this, &WalletView::requestedSyncWarningInfo);

//发送后突出显示事务
    connect(sendCoinsPage, &SendCoinsDialog::coinsSent, transactionView, static_cast<void (TransactionView::*)(const uint256&)>(&TransactionView::focusTransaction));

//点击“导出”可以导出交易列表
    connect(exportButton, &QPushButton::clicked, transactionView, &TransactionView::exportClicked);

//从sendconispage传递消息
    connect(sendCoinsPage, &SendCoinsDialog::message, this, &WalletView::message);
//从TransactionView传递消息
    connect(transactionView, &TransactionView::message, this, &WalletView::message);
}

WalletView::~WalletView()
{
}

void WalletView::setBitcoinGUI(BitcoinGUI *gui)
{
    if (gui)
    {
//单击概述页面上的事务，只需将您发送到事务历史页面。
        connect(overviewPage, &OverviewPage::transactionClicked, gui, &BitcoinGUI::gotoHistoryPage);

//发送后导航到“事务历史记录”页
        connect(sendCoinsPage, &SendCoinsDialog::coinsSent, gui, &BitcoinGUI::gotoHistoryPage);

//接收和报告消息
        connect(this, &WalletView::message, [gui](const QString &title, const QString &message, unsigned int style) {
            gui->message(title, message, style);
        });

//通过加密状态更改的信号
        connect(this, &WalletView::encryptionStatusChanged, gui, &BitcoinGUI::updateWalletStatus);

//直通事务通知
        connect(this, &WalletView::incomingTransaction, gui, &BitcoinGUI::incomingTransaction);

//连接高清启用状态信号
        connect(this, &WalletView::hdEnabledStatusChanged, gui, &BitcoinGUI::updateWalletStatus);
    }
}

void WalletView::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;

    overviewPage->setClientModel(_clientModel);
    sendCoinsPage->setClientModel(_clientModel);
}

void WalletView::setWalletModel(WalletModel *_walletModel)
{
    this->walletModel = _walletModel;

//将事务列表置于选项卡中
    transactionView->setModel(_walletModel);
    overviewPage->setWalletModel(_walletModel);
    receiveCoinsPage->setModel(_walletModel);
    sendCoinsPage->setModel(_walletModel);
    usedReceivingAddressesPage->setModel(_walletModel ? _walletModel->getAddressTableModel() : nullptr);
    usedSendingAddressesPage->setModel(_walletModel ? _walletModel->getAddressTableModel() : nullptr);

    if (_walletModel)
    {
//接收和传递来自钱包模型的消息
        connect(_walletModel, &WalletModel::message, this, &WalletView::message);

//处理加密状态的更改
        connect(_walletModel, &WalletModel::encryptionStatusChanged, this, &WalletView::encryptionStatusChanged);
        updateEncryptionStatus();

//更新HD状态
        Q_EMIT hdEnabledStatusChanged();

//新交易的气球弹出窗口
        connect(_walletModel->getTransactionTableModel(), &TransactionTableModel::rowsInserted, this, &WalletView::processNewTransaction);

//如果需要，请询问密码。
        connect(_walletModel, &WalletModel::requireUnlock, this, &WalletView::unlockWallet);

//显示进度对话框
        connect(_walletModel, &WalletModel::showProgress, this, &WalletView::showProgress);
    }
}

/*d walletview:：processnewtransaction（const qmodelindex&parent，int start，int/*end*/）
{
    //在进行初始块下载时防止气球垃圾邮件
    如果（！）钱包型号！clientModel clientModel->node（）.isInitialBlockDownload（））
        返回；

    transactionTableModel*ttm=walletModel->getTransactionTableModel（）；
    如果（！）ttm ttm->processingQueuedTransactions（））
        返回；

    qString date=ttm->index（start，transactionTableModel:：date，parent）.data（）.toString（）；
    qint64 amount=ttm->index（start，transactionTableModel:：amount，parent）.data（qt:：editRole）.touloglong（）；
    qString type=ttm->index（start，transactionTableModel:：type，parent）.data（）.toString（）；
    qmodelindex=ttm->index（start，0，parent）；
    qString address=ttm->data（index，transactionTableModel:：addressRole）.toString（）；
    qString label=ttm->data（index，transactionTableModel:：labelRole）.toString（）；

    q_emit incomingTransaction（日期，walletModel->getOptionsModel（）->getDisplayUnit（），金额，类型，地址，标签，walletModel->getwalletName（））；
}

void walletview:：gotooverviewpage（）。
{
    setcurrentwidget（概述页面）；
}

空的WalletView:：GoToHistoryPage（）
{
    setcurrentwidget（事务页面）；
}

空的WalletView:：GoToReceiveCoinspage（）
{
    setcurrentwidget（接收Coinspage）；
}

void walletview:：goToSendCoinspage（qString addr）
{
    setcurrentwidget（sendcoinspage）；

    如果（！）AdDr.ISUNTY（）
        sendcoinspage->setaddress（addr）；
}

void walletview:：gotosignmessagetab（qstring addr）
{
    //在showtab_sm（）中调用show（）。
    signverifymessagedialog*signverifymessagedialog=new signverifymessagedialog（platformstyle，this）；
    signverifymessagedialog->setattribute（qt:：wa_deleteonclose）；
    signverifymessagedialog->setmodel（walletmodel）；
    signverifymessagedialog->showtab_sm（真）；

    如果（！）AdDr.ISUNTY（）
        signverifymessagedialog->setaddress_sm（addr）；
}

void walletview:：gotovereifymessagetab（qstring addr）
{
    //在showtab_vm（）中调用show（）。
    signverifymessagedialog*signverifymessagedialog=new signverifymessagedialog（platformstyle，this）；
    signverifymessagedialog->setattribute（qt:：wa_deleteonclose）；
    signverifymessagedialog->setmodel（walletmodel）；
    signverifymessagedialog->showtab_vm（真）；

    如果（！）AdDr.ISUNTY（）
        signverifymessagedialog->setaddress_vm（addr）；
}

bool walletview:：handlePaymentRequest（const sendcoinsRecipient和recipient）
{
    返回sendcoinspage->handlePaymentRequest（收件人）；
}

void walletview:：showoutofsyncwarning（bool fshow）
{
    概述page->showoutofsyncwarning（fshow）；
}

void walletview:：updateEncryptionStatus（）。
{
    q_emit encryptionstatuschanged（）；
}

void walletview:：encryptwallet（bool状态）
{
    如果（！）瓦莱特模型
        返回；
    askpassphraseadialog dlg（状态？askpassphrasedialog:：encrypt:askpassphrasedialog:：decrypt，this）；
    dlg.setmodel（墙模型）；
    DLG.执行（）；

    updateEncryptionStatus（）；
}

空的walletview:：backupwallet（）。
{
    qstring filename=guiutil:：getsavefilename（此，
        tr（“备份钱包”），qstring（），
        tr（“钱包数据（*.dat）”），nullptr）；

    if（文件名.isEmpty（））
        返回；

    如果（！）walletmodel->wallet（）.backupwallet（filename.tolocal8bit（）.data（））
        q_emit message（tr（“backup failed”）、tr（“尝试将钱包数据保存到%1.”时出错）。arg（文件名），
            cclientuiinterface:：msg_错误）；
        }
    否则{
        q_emit message（tr（“备份成功”），tr（“钱包数据已成功保存到%1。”）.arg（文件名），
            cclientuiinterface:：msg_信息）；
    }
}

void walletview:：changepassphrase（）。
{
    askpassphraseadialog dlg（askpassphraseadialog:：changepass，this）；
    dlg.setmodel（墙模型）；
    DLG.执行（）；
}

空的WalletView:：UnlockWallet（）。
{
    如果（！）瓦莱特模型
        返回；
    //按钱包型号要求解锁钱包
    if（walletmodel->getEncryptionStatus（）==walletmodel:：locked）
    {
        askpassphrasedialog dlg（askpassphrasedialog:：unlock，this）；
        dlg.setmodel（墙模型）；
        DLG.执行（）；
    }
}

空的WalletView:：usedEndingAddresses（）。
{
    如果（！）瓦莱特模型
        返回；

    guiutil:：bringtofront（使用结束地址空间）；
}

void walletview:：usedReceivingAddresses（）。
{
    如果（！）瓦莱特模型
        返回；

    guiutil:：bringtofront（使用接收地址页面）；
}

void walletview:：showprogress（const qstring&title，int progress）
{
    如果（progress==0）
        progressDialog=new qprogressDialog（标题，tr（“取消”），0，100）；
        guiutil：：polishProgressDialog（ProgressDialog）；
        ProgressDialog->SetWindowModality（qt:：applicationModal）；
        ProgressDialog->SetMinimumDuration（0）；
        ProgressDialog->SetAutoClose（假）；
        ProgressDialog->SetValue（0）；
    否则，如果（progress==100）
        如果（ProgressDialog）
            ProgressDialog->Close（）；
            ProgressDialog->DeleteLater（）；
        }
    其他if（progressdialog）
        if（progressDialog->wasCanceled（））
            getWalletModel（）->Wallet（）.AbortRescan（）；
        }否则{
            progressdialog->setvalue（progress）；
        }
    }
}

void walletview:：requestedsyncwarninginfo（）。
{
    发出OutofSyncWarningClicked（）；
}
