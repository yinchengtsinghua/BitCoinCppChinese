
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
#include <qt/test/addressbooktests.h>
#include <qt/test/util.h>
#include <test/test_bitcoin.h>

#include <interfaces/chain.h>
#include <interfaces/node.h>
#include <qt/addressbookpage.h>
#include <qt/addresstablemodel.h>
#include <qt/editaddressdialog.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/qvalidatedlineedit.h>
#include <qt/walletmodel.h>

#include <key.h>
#include <key_io.h>
#include <pubkey.h>
#include <wallet/wallet.h>

#include <QApplication>
#include <QTimer>
#include <QMessageBox>

namespace
{

/*
 *在“编辑地址”对话框中填入数据，然后提交，并确保
 *结果信息符合预期。
 **/

void EditAddressAndSubmit(
        EditAddressDialog* dialog,
        const QString& label, const QString& address, QString expected_msg)
{
    QString warning_text;

    dialog->findChild<QLineEdit*>("labelEdit")->setText(label);
    dialog->findChild<QValidatedLineEdit*>("addressEdit")->setText(address);

    ConfirmMessage(&warning_text, 5);
    dialog->accept();
    QCOMPARE(warning_text, expected_msg);
}

/*
 *测试将各种发送地址添加到通讯簿。
 *
 *测试了三个案例：
 *
 *-新地址：应成功添加为发送地址的新地址。
 *-现有的发送地址：无法成功添加的现有发送地址。
 *-现有接收地址：无法成功添加的现有接收地址。
 *
 *在每种情况下，请验证通讯簿的结果状态，并且可以选择
 *向用户显示的警告信息。
 **/

void TestAddAddressesToSendBook()
{
    TestChain100Setup test;
    auto chain = interfaces::MakeChain();
    std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>(*chain, WalletLocation(), WalletDatabase::CreateMock());
    bool firstRun;
    wallet->LoadWallet(firstRun);

    auto build_address = [&wallet]() {
        CKey key;
        key.MakeNewKey(true);
        CTxDestination dest(GetDestinationForKey(
            key.GetPubKey(), wallet->m_default_address_type));

        return std::make_pair(dest, QString::fromStdString(EncodeDestination(dest)));
    };

    CTxDestination r_key_dest, s_key_dest;

//在通讯簿中添加以前存在的“接收”条目。
    QString preexisting_r_address;
    QString r_label("already here (r)");

//在通讯簿中添加以前存在的“发送”条目。
    QString preexisting_s_address;
    QString s_label("already here (s)");

//定义新地址（应成功添加到通讯簿中）。
    QString new_address;

    std::tie(r_key_dest, preexisting_r_address) = build_address();
    std::tie(s_key_dest, preexisting_s_address) = build_address();
    std::tie(std::ignore, new_address) = build_address();

    {
        LOCK(wallet->cs_wallet);
        wallet->SetAddressBook(r_key_dest, r_label.toStdString(), "receive");
        wallet->SetAddressBook(s_key_dest, s_label.toStdString(), "send");
    }

    auto check_addbook_size = [&wallet](int expected_size) {
        QCOMPARE(static_cast<int>(wallet->mapAddressBook.size()), expected_size);
    };

//我们应该从前面添加的两个地址开始，而不是其他地址。
    check_addbook_size(2);

//初始化相关的Qt模型。
    std::unique_ptr<const PlatformStyle> platformStyle(PlatformStyle::instantiate("other"));
    auto node = interfaces::MakeNode();
    OptionsModel optionsModel(*node);
    AddWallet(wallet);
    WalletModel walletModel(std::move(node->getWallets()[0]), *node, platformStyle.get(), &optionsModel);
    RemoveWallet(wallet);
    EditAddressDialog editAddressDialog(EditAddressDialog::NewSendingAddress);
    editAddressDialog.setModel(walletModel.getAddressTableModel());

    EditAddressAndSubmit(
        &editAddressDialog, QString("uhoh"), preexisting_r_address,
        QString(
            "Address \"%1\" already exists as a receiving address with label "
            "\"%2\" and so cannot be added as a sending address."
            ).arg(preexisting_r_address).arg(r_label));

    check_addbook_size(2);

    EditAddressAndSubmit(
        &editAddressDialog, QString("uhoh, different"), preexisting_s_address,
        QString(
            "The entered address \"%1\" is already in the address book with "
            "label \"%2\"."
            ).arg(preexisting_s_address).arg(s_label));

    check_addbook_size(2);

//提交应成功添加的新地址-我们希望
//警告消息为空。
    EditAddressAndSubmit(
        &editAddressDialog, QString("new"), new_address, QString(""));

    check_addbook_size(3);
}

} //命名空间

void AddressBookTests::addressBookTests()
{
#ifdef Q_OS_MAC
    if (QApplication::platformName() == "minimal") {
//在“最小”平台上禁用Mac，以避免qt内部崩溃
//当框架试图查找未实现的cocoa函数时，
//无法处理返回的空值
//（https://bugreports.qt.io/browse/qtbug-49686）。
        QWARN("Skipping AddressBookTests on mac build with 'minimal' platform set due to Qt bugs. To run AppTests, invoke "
              "with 'test_bitcoin-qt -platform cocoa' on mac, or else use a linux or windows build.");
        return;
    }
#endif
    TestAddAddressesToSendBook();
}
