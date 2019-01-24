
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

#include <qt/test/rpcnestedtests.h>

#include <chainparams.h>
#include <consensus/validation.h>
#include <fs.h>
#include <interfaces/node.h>
#include <validation.h>
#include <rpc/register.h>
#include <rpc/server.h>
#include <qt/rpcconsole.h>
#include <test/test_bitcoin.h>
#include <univalue.h>
#include <util/system.h>

#include <QDir>
#include <QtGlobal>

static UniValue rpcNestedTest_rpc(const JSONRPCRequest& request)
{
    if (request.fHelp) {
        return "help message";
    }
    return request.params.write(0, 0);
}

static const CRPCCommand vRPCCommands[] =
{
    { "test", "rpcNestedTest", &rpcNestedTest_rpc, {} },
};

void RPCNestedTests::rpcNestedTests()
{
//做一些测试设置
//当我们在qt级别上添加更多测试时，可以将其移动到更通用的位置
    tableRPC.appendCommand("rpcNestedTest", &vRPCCommands[0]);
//mempool.setsanitycheck（1.0）；

    TestingSetup test;

    if (RPCIsInWarmup(nullptr)) SetRPCWarmupFinished();

    std::string result;
    std::string result2;
    std::string filtered;
    auto node = interfaces::MakeNode();
RPCConsole::RPCExecuteCommandLine(*node, result, "getblockchaininfo()[chain]", &filtered); //带有路径的简单结果过滤
    QVERIFY(result=="main");
    QVERIFY(filtered == "getblockchaininfo()[chain]");

RPCConsole::RPCExecuteCommandLine(*node, result, "getblock(getbestblockhash())"); //简单的2级嵌套
    RPCConsole::RPCExecuteCommandLine(*node, result, "getblock(getblock(getbestblockhash())[hash], true)");

RPCConsole::RPCExecuteCommandLine(*node, result, "getblock( getblock( getblock(getbestblockhash())[hash] )[hash], true)"); //带空白、过滤路径和布尔参数的4级嵌套

    RPCConsole::RPCExecuteCommandLine(*node, result, "getblockchaininfo");
    QVERIFY(result.substr(0,1) == "{");

    RPCConsole::RPCExecuteCommandLine(*node, result, "getblockchaininfo()");
    QVERIFY(result.substr(0,1) == "{");

RPCConsole::RPCExecuteCommandLine(*node, result, "getblockchaininfo "); //最后的空白将被容忍。
    QVERIFY(result.substr(0,1) == "{");

(RPCConsole::RPCExecuteCommandLine(*node, result, "getblockchaininfo()[\"chain\"]")); //允许使用引号路径标识符，但要注意键中包含引号的子级
    QVERIFY(result == "null");

(RPCConsole::RPCExecuteCommandLine(*node, result, "createrawtransaction [] {} 0")); //参数不在括号内是允许的
(RPCConsole::RPCExecuteCommandLine(*node, result2, "createrawtransaction([],{},0)")); //括号中的参数是允许的
    QVERIFY(result == result2);
(RPCConsole::RPCExecuteCommandLine(*node, result2, "createrawtransaction( [],  {} , 0   )")); //参数之间允许空白
    QVERIFY(result == result2);

    RPCConsole::RPCExecuteCommandLine(*node, result, "getblock(getbestblockhash())[tx][0]", &filtered);
    QVERIFY(result == "4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b");
    QVERIFY(filtered == "getblock(getbestblockhash())[tx][0]");

    RPCConsole::RPCParseCommandLine(nullptr, result, "importprivkey", false, &filtered);
    QVERIFY(filtered == "importprivkey(…)");
    RPCConsole::RPCParseCommandLine(nullptr, result, "signmessagewithprivkey abc", false, &filtered);
    QVERIFY(filtered == "signmessagewithprivkey(…)");
    RPCConsole::RPCParseCommandLine(nullptr, result, "signmessagewithprivkey abc,def", false, &filtered);
    QVERIFY(filtered == "signmessagewithprivkey(…)");
    RPCConsole::RPCParseCommandLine(nullptr, result, "signrawtransactionwithkey(abc)", false, &filtered);
    QVERIFY(filtered == "signrawtransactionwithkey(…)");
    RPCConsole::RPCParseCommandLine(nullptr, result, "walletpassphrase(help())", false, &filtered);
    QVERIFY(filtered == "walletpassphrase(…)");
    RPCConsole::RPCParseCommandLine(nullptr, result, "walletpassphrasechange(help(walletpassphrasechange(abc)))", false, &filtered);
    QVERIFY(filtered == "walletpassphrasechange(…)");
    RPCConsole::RPCParseCommandLine(nullptr, result, "help(encryptwallet(abc, def))", false, &filtered);
    QVERIFY(filtered == "help(encryptwallet(…))");
    RPCConsole::RPCParseCommandLine(nullptr, result, "help(importprivkey())", false, &filtered);
    QVERIFY(filtered == "help(importprivkey(…))");
    RPCConsole::RPCParseCommandLine(nullptr, result, "help(importprivkey(help()))", false, &filtered);
    QVERIFY(filtered == "help(importprivkey(…))");
    RPCConsole::RPCParseCommandLine(nullptr, result, "help(importprivkey(abc), walletpassphrase(def))", false, &filtered);
    QVERIFY(filtered == "help(importprivkey(…), walletpassphrase(…))");

    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest");
    QVERIFY(result == "[]");
    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest ''");
    QVERIFY(result == "[\"\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest \"\"");
    QVERIFY(result == "[\"\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest '' abc");
    QVERIFY(result == "[\"\",\"abc\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest abc '' abc");
    QVERIFY(result == "[\"abc\",\"\",\"abc\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest abc  abc");
    QVERIFY(result == "[\"abc\",\"abc\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest abc\t\tabc");
    QVERIFY(result == "[\"abc\",\"abc\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest(abc )");
    QVERIFY(result == "[\"abc\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest( abc )");
    QVERIFY(result == "[\"abc\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest(   abc   ,   cba )");
    QVERIFY(result == "[\"abc\",\"cba\"]");

#if QT_VERSION >= 0x050300
//仅对qt5.3及更高版本执行qverify_exception_through检查（qt5.3中引入了qverify_exception_through）
QVERIFY_EXCEPTION_THROWN(RPCConsole::RPCExecuteCommandLine(*node, result, "getblockchaininfo() .\n"), std::runtime_error); //无效语法
QVERIFY_EXCEPTION_THROWN(RPCConsole::RPCExecuteCommandLine(*node, result, "getblockchaininfo() getblockchaininfo()"), std::runtime_error); //无效语法
(RPCConsole::RPCExecuteCommandLine(*node, result, "getblockchaininfo(")); //如果没有参数，则允许使用非右括号
(RPCConsole::RPCExecuteCommandLine(*node, result, "getblockchaininfo()()()")); //容忍非命令支架
QVERIFY_EXCEPTION_THROWN(RPCConsole::RPCExecuteCommandLine(*node, result, "getblockchaininfo(True)"), UniValue); //无效参数
QVERIFY_EXCEPTION_THROWN(RPCConsole::RPCExecuteCommandLine(*node, result, "a(getblockchaininfo(True))"), UniValue); //找不到方法
QVERIFY_EXCEPTION_THROWN(RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest abc,,abc"), std::runtime_error); //使用时不要使用空参数，
QVERIFY_EXCEPTION_THROWN(RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest(abc,,abc)"), std::runtime_error); //使用时不要使用空参数，
QVERIFY_EXCEPTION_THROWN(RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest(abc,,)"), std::runtime_error); //使用时不要使用空参数，
#endif
}
