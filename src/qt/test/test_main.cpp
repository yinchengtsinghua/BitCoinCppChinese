
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <chainparams.h>
#include <interfaces/node.h>
#include <qt/bitcoin.h>
#include <qt/test/apptests.h>
#include <qt/test/rpcnestedtests.h>
#include <util/system.h>
#include <qt/test/uritests.h>
#include <qt/test/compattests.h>

#ifdef ENABLE_WALLET
#include <qt/test/addressbooktests.h>
#ifdef ENABLE_BIP70
#include <qt/test/paymentservertests.h>
#endif //Enableγ-BIP70
#include <qt/test/wallettests.h>
#endif //允许钱包

#include <QApplication>
#include <QObject>
#include <QTest>

#include <openssl/ssl.h>

#if defined(QT_STATICPLUGIN)
#include <QtPlugin>
#if defined(QT_QPA_PLATFORM_MINIMAL)
Q_IMPORT_PLUGIN(QMinimalIntegrationPlugin);
#endif
#if defined(QT_QPA_PLATFORM_XCB)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_WINDOWS)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_COCOA)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#endif
#endif

extern void noui_connect();

//这就是运行所有测试所需的全部内容
int main(int argc, char *argv[])
{
    SetupEnvironment();
    SetupNetworking();
    SelectParams(CBaseChainParams::REGTEST);
    noui_connect();
    ClearDatadirCache();
    fs::path pathTemp = fs::temp_directory_path() / strprintf("test_bitcoin-qt_%lu_%i", (unsigned long)GetTime(), (int)GetRand(100000));
    fs::create_directories(pathTemp);
    gArgs.ForceSetArg("-datadir", pathTemp.string());
    auto node = interfaces::MakeNode();

    bool fInvalid = false;

//首选测试的“最小”平台，而不是常规默认平台
//平台（“xcb”、“windows”或“cocoa”），以便测试不会无意中
//干扰任何后台GUI，不需要额外的资源。
    #if defined(WIN32)
        _putenv_s("QT_QPA_PLATFORM", "minimal");
    #else
        setenv("QT_QPA_PLATFORM", "minimal", 0);
    #endif

//不要删除此，需要访问它
//测试中的qApplication：：和qCoreApplication：：。
    BitcoinApplication app(*node, argc, argv);
    app.setApplicationName("Bitcoin-Qt-test");

    SSL_library_init();

    AppTests app_tests(app);
    if (QTest::qExec(&app_tests) != 0) {
        fInvalid = true;
    }
    URITests test1;
    if (QTest::qExec(&test1) != 0) {
        fInvalid = true;
    }
#if defined(ENABLE_WALLET) && defined(ENABLE_BIP70)
    PaymentServerTests test2;
    if (QTest::qExec(&test2) != 0) {
        fInvalid = true;
    }
#endif
    RPCNestedTests test3;
    if (QTest::qExec(&test3) != 0) {
        fInvalid = true;
    }
    CompatTests test4;
    if (QTest::qExec(&test4) != 0) {
        fInvalid = true;
    }
#ifdef ENABLE_WALLET
    WalletTests test5;
    if (QTest::qExec(&test5) != 0) {
        fInvalid = true;
    }
    AddressBookTests test6;
    if (QTest::qExec(&test6) != 0) {
        fInvalid = true;
    }
#endif

    fs::remove_all(pathTemp);

    return fInvalid;
}
