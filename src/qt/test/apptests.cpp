
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <qt/test/apptests.h>

#include <chainparams.h>
#include <init.h>
#include <qt/bitcoin.h>
#include <qt/bitcoingui.h>
#include <qt/networkstyle.h>
#include <qt/rpcconsole.h>
#include <shutdown.h>
#include <validation.h>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif
#ifdef ENABLE_WALLET
#include <wallet/db.h>
#endif

#include <QAction>
#include <QEventLoop>
#include <QLineEdit>
#include <QScopedPointer>
#include <QTest>
#include <QTextEdit>
#include <QtGlobal>
#if QT_VERSION >= 0x050000
#include <QtTest/QtTestWidgets>
#endif
#include <QtTest/QtTestGui>
#include <new>
#include <string>
#include <univalue.h>

namespace {
//！调用getblockchaininfo rpc并检查json输出的第一个字段。
void TestRpcCommand(RPCConsole* console)
{
    QEventLoop loop;
    QTextEdit* messagesWidget = console->findChild<QTextEdit*>("messagesWidget");
    QObject::connect(messagesWidget, &QTextEdit::textChanged, &loop, &QEventLoop::quit);
    QLineEdit* lineEdit = console->findChild<QLineEdit*>("lineEdit");
    QTest::keyClicks(lineEdit, "getblockchaininfo");
    QTest::keyClick(lineEdit, Qt::Key_Return);
    loop.exec();
    QString output = messagesWidget->toPlainText();
    UniValue value;
    value.read(output.right(output.size() - output.lastIndexOf(QChar::ObjectReplacementCharacter) - 1).toStdString());
    QCOMPARE(value["chain"].get_str(), std::string("regtest"));
}
} //命名空间

//！比特币应用程序测试的入口点。
void AppTests::appTests()
{
#ifdef Q_OS_MAC
    if (QApplication::platformName() == "minimal") {
//在“最小”平台上禁用Mac，以避免qt内部崩溃
//当框架试图查找未实现的cocoa函数时，
//无法处理返回的空值
//（https://bugreports.qt.io/browse/qtbug-49686）。
        QWARN("Skipping AppTests on mac build with 'minimal' platform set due to Qt bugs. To run AppTests, invoke "
              "with 'test_bitcoin-qt -platform cocoa' on mac, or else use a linux or windows build.");
        return;
    }
#endif

    m_app.parameterSetup();
    /*pp.createOptionsModel（真/*重置设置*/）；
    qscopedpointer<const networkstyle>style（
        networkStyle：：实例化（qString:：FromStdString（params（）.networkIdString（））；
    m_app.setupplatformstyle（）；
    m_app.createWindow（style.data（））；
    连接（&m_app，&bitcoinapplication:：windowshown，this，&apptests:：guitests）；
    expectcallback（“guitests”）；
    m_app.baseinitialize（）；
    m_app.requestInitialize（）；
    MyApp.Excor（）；
    m_app.requestShutdown（）；
    MyApp.Excor（）；

    //重置全局状态以避免干扰以后的测试。
    AbortShutdown（）；
    UnloadBlockIndex（）；
}

/ /！比特币GUI测试的入口点。
void apptests:：guitests（bitcoingui*窗口）
{
    handleCallback回调“guitests”，
    连接（窗口，&bitcoingui:：consoleshown，this，&apptests:：consoletests）；
    ExpectCallback（“控制台测试”）；
    qaction*action=window->findchild<qaction*>（“openRpconsoleaction”）；
    操作->激活（qaction:：trigger）；
}

/ /！rpcconsole测试的入口点。
void apptests:：consoletests（rpcconsole*控制台）
{
    handleCallback回调“控制台测试”，
    testrpcommand（控制台）；
}

/ /！析构函数在最后一个预期回调完成后关闭。
应用程序测试：：handleCallback:：~ handleCallback（））
{
    auto&callbacks=m_app_tests.m_回调；
    auto it=callbacks.find（m_callback）；
    断言（它）！=回调.end（））；
    回拨。删除（it）；
    if（callbacks.empty（））
        m_app_tests.m_app.quit（）；
    }
}
