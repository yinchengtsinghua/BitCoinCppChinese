
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
#include <qt/test/wallettests.h>
#include <qt/test/util.h>

#include <interfaces/chain.h>
#include <interfaces/node.h>
#include <base58.h>
#include <qt/bitcoinamountfield.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/qvalidatedlineedit.h>
#include <qt/sendcoinsdialog.h>
#include <qt/sendcoinsentry.h>
#include <qt/transactiontablemodel.h>
#include <qt/transactionview.h>
#include <qt/walletmodel.h>
#include <key_io.h>
#include <test/test_bitcoin.h>
#include <validation.h>
#include <wallet/wallet.h>
#include <qt/overviewpage.h>
#include <qt/receivecoinsdialog.h>
#include <qt/recentrequeststablemodel.h>
#include <qt/receiverequestdialog.h>

#include <memory>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QListView>
#include <QDialogButtonBox>

namespace
{
//！在模式发送确认对话框中按“是”或“取消”按钮。
void ConfirmSend(QString* text = nullptr, bool cancel = false)
{
    QTimer::singleShot(0, [text, cancel]() {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (widget->inherits("SendConfirmationDialog")) {
                SendConfirmationDialog* dialog = qobject_cast<SendConfirmationDialog*>(widget);
                if (text) *text = dialog->text();
                QAbstractButton* button = dialog->button(cancel ? QMessageBox::Cancel : QMessageBox::Yes);
                button->setEnabled(true);
                button->click();
            }
        }
    });
}

//！将硬币寄到地址并返回txid。
uint256 SendCoins(CWallet& wallet, SendCoinsDialog& sendCoinsDialog, const CTxDestination& address, CAmount amount, bool rbf)
{
    QVBoxLayout* entries = sendCoinsDialog.findChild<QVBoxLayout*>("entries");
    SendCoinsEntry* entry = qobject_cast<SendCoinsEntry*>(entries->itemAt(0)->widget());
    entry->findChild<QValidatedLineEdit*>("payTo")->setText(QString::fromStdString(EncodeDestination(address)));
    entry->findChild<BitcoinAmountField*>("payAmount")->setValue(amount);
    sendCoinsDialog.findChild<QFrame*>("frameFee")
        ->findChild<QFrame*>("frameFeeSelection")
        ->findChild<QCheckBox*>("optInRBF")
        ->setCheckState(rbf ? Qt::Checked : Qt::Unchecked);
    uint256 txid;
    boost::signals2::scoped_connection c(wallet.NotifyTransactionChanged.connect([&txid](CWallet*, const uint256& hash, ChangeType status) {
        if (status == CT_NEW) txid = hash;
    }));
    ConfirmSend();
    QMetaObject::invokeMethod(&sendCoinsDialog, "on_sendButton_clicked");
    return txid;
}

//！在事务列表中查找txid的索引。
QModelIndex FindTx(const QAbstractItemModel& model, const uint256& txid)
{
    QString hash = QString::fromStdString(txid.ToString());
    int rows = model.rowCount({});
    for (int row = 0; row < rows; ++row) {
        QModelIndex index = model.index(row, 0, {});
        if (model.data(index, TransactionTableModel::TxHashRole) == hash) {
            return index;
        }
    }
    return {};
}

//！调用txid上的bumpfee并检查结果。
void BumpFee(TransactionView& view, const uint256& txid, bool expectDisabled, std::string expectError, bool cancel)
{
    QTableView* table = view.findChild<QTableView*>("transactionView");
    QModelIndex index = FindTx(*table->selectionModel()->model(), txid);
    QVERIFY2(index.isValid(), "Could not find BumpFee txid");

//选择表中的行，调用上下文菜单，并确保bumpfee操作为
//按预期启用或禁用。
    QAction* action = view.findChild<QAction*>("bumpFeeAction");
    table->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    action->setEnabled(expectDisabled);
    table->customContextMenuRequested({});
    QCOMPARE(action->isEnabled(), !expectDisabled);

    action->setEnabled(true);
    QString text;
    if (expectError.empty()) {
        ConfirmSend(&text, cancel);
    } else {
        ConfirmMessage(&text);
    }
    action->trigger();
    QVERIFY(text.indexOf(QString::fromStdString(expectError)) != -1);
}

//！简单的qt钱包测试。
//
//测试小部件可以交互地对其调用show（）进行调试，并且
//手动运行事件循环，例如：
//
//sendconisDialog.show（）；
//qEventLoop（）.exec（）；
//
//这还需要覆盖默认的最小qt平台：
//
//src/qt/test/test_bitcoin-qt-平台xcb linux
//src/qt/test/test_bitcoin-qt-平台窗口
//src/qt/test/test_比特币-qt-平台Cocoa MacOS
void TestGUI()
{
//设置钱包和连锁105个街区（5个成熟街区用于消费）。
    TestChain100Setup test;
    for (int i = 0; i < 5; ++i) {
        test.CreateAndProcessBlock({}, GetScriptForRawPubKey(test.coinbaseKey.GetPubKey()));
    }
    auto chain = interfaces::MakeChain();
    std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>(*chain, WalletLocation(), WalletDatabase::CreateMock());
    bool firstRun;
    wallet->LoadWallet(firstRun);
    {
        LOCK(wallet->cs_wallet);
        wallet->SetAddressBook(GetDestinationForKey(test.coinbaseKey.GetPubKey(), wallet->m_default_address_type), "", "receive");
        wallet->AddKeyPubKey(test.coinbaseKey, test.coinbaseKey.GetPubKey());
    }
    {
        auto locked_chain = wallet->chain().lock();
        WalletRescanReserver reserver(wallet.get());
        reserver.reserve();
        const CBlockIndex* const null_block = nullptr;
        const CBlockIndex *stop_block, *failed_block;
        QCOMPARE(
            /*let->scanForWalletTransactions（chainActive.genesis（），nullptr，reserver，failed_block，stop_block，true/*fupdate*/），
            cwallet:：scanresult:：成功）；
        qcompare（stop_block，chainactive.tip（））；
        QCompare（失败的_块，空的_块）；
    }
    Wallet->SetBroadcastTransactions（真）；

    //创建用于发送硬币和列出交易的小部件。
    std:：unique_ptr<const platformstyle>platformstyle（platformstyle:：instantate（“other”））；
    sendcoinsdialog sendcoinsdialog（platformstyle.get（））；
    TransactionView TransactionView（platformStyle.get（））；
    auto node=接口：：makenode（）；
    optionsModel optionsModel（*节点）；
    添加钱包（钱包）；
    walletmodel walletmodel（std:：move（node->getwallets（）.back（）），*node，platformstyle.get（），&optionsmodel）；
    取出钱包（钱包）；
    sendcoinsdialog.setmodel（&walletmodel）；
    transactionView.setModel（&walletModel）；

    //发送两个事务，并验证它们是否已添加到事务列表中。
    transactionTableModel*transactionTableModel=walletModel.getTransactionTableModel（）；
    QCompare（TransactionTableModel->RowCount（），105）；
    uint256 txid1=sendcoins（*wallet.get（），sendcoinsdialog，ckeyid（），5*coin，false/*rbf*/);

    /*t256 txid2=sendcoins（*wallet.get（），sendcoinsdialog，ckeyid（），10*coin，true/*rbf*/）；
    QCompare（TransactionTableModel->RowCount（），107）；
    qverify（findtx（*TransactionTableModel，txid1）.isvalid（））；
    qverify（findtx（*TransactionTableModel，txid2）.isvalid（））；

    //呼叫bumpfee。测试被禁用、取消、启用，然后失败。
    bumpfee（transactionview，txid1，true/*预期禁用*/, "not BIP 125 replaceable" /* expected error */, false /* cancel */);

    /*pfee（transactionview，txid2，false/*expect disabled*/，/*expected error*/，true/*cancel*/）；
    bumpfee（transactionview，txid2，false/*预期禁用*/, {} /* expected error */, false /* cancel */);

    /*pfee（transactionview，txid2，true/*expect disabled*/，“already bumped”/*expected error*/，false/*cancel*/）；

    //在Overview页检查当前余额
    overview页面overview页面（platformstyle.get（））；
    概述page.setwalletmodel（&walletmodel）；
    qlabel*balancellabel=概述page.findchild<qlabel/>（“labelbalance”）；
    qString BalanceText=BalanceLabel->Text（）；
    int unit=walletmodel.getOptionsModel（）->getDisplayUnit（）；
    camount balance=walletmodel.wallet（）.getbalance（）；
    qString BalanceConceComparison=bitcoinUnits:：FormatWithUnit（单位，余额，假，bitcoinUnits:：SeparatoralWays）；
    QCompare（平衡文本、平衡比较）；

    //检查请求付款按钮
    ReceiveCoinsDialog ReceiveCoinsDialog（platformStyle.get（））；
    receiveCoinsDialog.setModel（&walletModel）；
    recentRequestStableModel*requestTableModel=walletModel.getRecentRequestStableModel（）；

    //标签输入
    qlineedit*labelinput=receiveCoinsDialog.findchild<qlineedit/>（“reqLabel”）；
    labelinput->settext（“test_label_1”）；

    /金额输入
    比特币金额field*amountinput=receiveCoinsDialog.findchild<bitcointamountfield/>（“reqAmount”）；
    amountinput->setvalue（1）；

    //消息输入
    qlineedit*messageinput=receiveCoinsDialog.findchild<qlineedit/>（“reqmessage”）；
    messageinput->settext（“测试消息”）；
    int initialRowCount=requestTableModel->rowCount（）；
    qpushButton*requestPaymentButton=receiveCoinsDialog.findchild<qpushButton/>（“receiveButton”）；
    RequestPaymentButton->Click（）；
    for（qWidget*小部件：qApplication:：TopLevelWidgets（））
        if（widget->inherits（“receiverequestdialog”））；
            receiverequestdialog*receiverequestdialog=qObject_cast<receiverequestdialog*>（widget）；
            qtextedit*rlist=receiverequestdialog->qObject:：findchild<qtextedit/>（“outuri”）；
            qString paymentText=rlist->toplainText（）；
            qStringList paymentTextList=paymentText.split（'\n'）；
            qcompare（paymenttextlist.at（0），qstring（“付款信息”））；
            qverify（paymenttextlist.at（1）.indexof（qstring（“uri:bitcoin:”））！= -1）；
            qverify（paymenttextlist.at（2）.indexof（qstring（“地址：”））！= -1）；
            Qcompare（paymenttextlist.at（3），qstring（“amount:0.00000001”）+qstring:：fromstdstring（currency_unity））；
            Qcompare（paymenttextlist.at（4），qstring（“label:test_label_1”））；
            qcompare（paymenttextlist.at（5），qstring（“message:test_message_1”））；
        }
    }

    //清除按钮
    qpushbutton*clearbutton=receiveCoinsDialog.findchild<qpushbutton/>（“clearbutton”）；
    ClearButton->Click（）；
    Qcompare（labelinput->text（），qstring（“”）；
    QCompare（amountInput->value（），camount（0））；
    qCompare（messageinput->text（），qString（“”）；

    //检查历史记录的添加
    int currentRowCount=requestTableModel->rowCount（）；
    QCompare（当前行数，初始行数+1）；

    //检查删除按钮
    qtableView*table=receiveCoinsDialog.findchild<qtableView/>（“receiventRequestsView”）；
    table->selectrow（当前行数-1）；
    qpushButton*removeRequestButton=receiveCoinsDialog.findchild<qpushButton/>（“removeRequestButton”）；
    removeRequestButton->click（）；
    QCompare（RequestTableModel->RowCount（），CurrentRowCount-1）；
}

} / /命名空间

空的wallettests:：wallettests（）。
{
γ-干扰素
    if（qapplication:：platformname（）=“最小”）
        //在“最小”平台上禁用Mac，以避免qt内部崩溃
        //当框架试图查找未实现的cocoa函数时，
        //并且无法处理返回的空值
        //（https://bugreports.qt.io/browse/qtbug-49686）。
        qwarn（“由于qt错误，跳过mac build上设置了“最小”平台的walletsts）。要运行AppTests，请调用“
              “在Mac上使用‘test_bitcoin-qt-platform cococa’，或者使用Linux或Windows版本。”）；
        返回；
    }
第二节
    TestGuy（）；
}
