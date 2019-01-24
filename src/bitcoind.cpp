
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <chainparams.h>
#include <clientversion.h>
#include <compat.h>
#include <fs.h>
#include <interfaces/chain.h>
#include <rpc/server.h>
#include <init.h>
#include <noui.h>
#include <shutdown.h>
#include <util/system.h>
#include <httpserver.h>
#include <httprpc.h>
#include <util/strencodings.h>
#include <walletinitinterface.h>

#include <stdio.h>

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

/*Doxygen的介绍文本：*/

/*\主页开发人员文档
 *
 *节简介
 *
 *这是一种称为比特币的实验性新数字货币的参考客户的开发人员文档，
 *它可以即时支付给世界任何地方的任何人。比特币使用点对点技术进行操作
 *没有中央权力：管理交易和发行资金由网络共同进行。
 *
 *该软件是一个社区驱动的开源项目，根据MIT许可证发布。
 *
 *有关该项目的更多信息，请参阅https://github.com/bitcoin/bitcoin和https://bitcoincore.org/。
 *
 *\节导航
 *使用页面顶部的按钮开始导航代码。
 **/


static void WaitForShutdown()
{
    while (!ShutdownRequested())
    {
        MilliSleep(200);
    }
    Interrupt();
}

//////////////////////////////////////////////////
//
//起点
//
static bool AppInit(int argc, char* argv[])
{
    InitInterfaces interfaces;
    interfaces.chain = interfaces::MakeChain();

    bool fRet = false;

//
//参数
//
//如果使用qt，参数/bitcoin.conf将在qt/bitcoin.cpp的main（）中解析。
    SetupServerArgs();
    std::string error;
    if (!gArgs.ParseParameters(argc, argv, error)) {
        fprintf(stderr, "Error parsing command line arguments: %s\n", error.c_str());
        return false;
    }

//在处理datadir之前处理帮助和版本
    if (HelpRequested(gArgs) || gArgs.IsArgSet("-version")) {
        std::string strUsage = PACKAGE_NAME " Daemon version " + FormatFullVersion() + "\n";

        if (gArgs.IsArgSet("-version"))
        {
            strUsage += FormatParagraph(LicenseInfo()) + "\n";
        }
        else
        {
            strUsage += "\nUsage:  bitcoind [options]                     Start " PACKAGE_NAME " Daemon\n";
            strUsage += "\n" + gArgs.GetHelpMessage();
        }

        fprintf(stdout, "%s", strUsage.c_str());
        return true;
    }

    try
    {
        if (!fs::is_directory(GetDataDir(false)))
        {
            fprintf(stderr, "Error: Specified data directory \"%s\" does not exist.\n", gArgs.GetArg("-datadir", "").c_str());
            return false;
        }
        if (!gArgs.ReadConfigFiles(error, true)) {
            fprintf(stderr, "Error reading configuration file: %s\n", error.c_str());
            return false;
        }
//检查-testnet或-regtest参数（params（）调用仅在此子句之后有效）
        try {
            SelectParams(gArgs.GetChainName());
        } catch (const std::exception& e) {
            fprintf(stderr, "Error: %s\n", e.what());
            return false;
        }

//在命令行上遇到松散的非参数标记时出错
        for (int i = 1; i < argc; i++) {
            if (!IsSwitchChar(argv[i][0])) {
                fprintf(stderr, "Error: Command line contains unexpected token '%s', see bitcoind -h for a list of options.\n", argv[i]);
                return false;
            }
        }

//-对于比特币，服务器默认为“真”，但对于GUI，则不为“真”，因此在此处执行此操作
        gArgs.SoftSetBoolArg("-server", true);
//尽早设置，以便参数交互转到控制台
        InitLogging();
        InitParameterInteraction();
        if (!AppInitBasicSetup())
        {
//将使用详细的错误调用initrerror，该错误最终出现在控制台上
            return false;
        }
        if (!AppInitParameterInteraction())
        {
//将使用详细的错误调用initrerror，该错误最终出现在控制台上
            return false;
        }
        if (!AppInitSanityChecks())
        {
//将使用详细的错误调用initrerror，该错误最终出现在控制台上
            return false;
        }
        if (gArgs.GetBoolArg("-daemon", false))
        {
#if HAVE_DECL_DAEMON
#if defined(MAC_OSX)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
            fprintf(stdout, "Bitcoin server starting\n");

//使成为魔鬼
if (daemon(1, 0)) { //不要更改目录（1），关闭FDS（0）
                fprintf(stderr, "Error: daemon() failed: %s\n", strerror(errno));
                return false;
            }
#if defined(MAC_OSX)
#pragma GCC diagnostic pop
#endif
#else
            fprintf(stderr, "Error: -daemon is not supported on this operating system\n");
            return false;
#endif //拥有\u decl \u守护进程
        }
//后台监控后锁定数据目录
        if (!AppInitLockDataDirectory())
        {
//如果锁定数据目录失败，请立即退出
            return false;
        }
        fRet = AppInitMain(interfaces);
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "AppInit()");
    } catch (...) {
        PrintExceptionContinue(nullptr, "AppInit()");
    }

    if (!fRet)
    {
        Interrupt();
    } else {
        WaitForShutdown();
    }
    Shutdown(interfaces);

    return fRet;
}

int main(int argc, char* argv[])
{
#ifdef WIN32
    util::WinCmdLineArgs winArgs;
    std::tie(argc, argv) = winArgs.get();
#endif
    SetupEnvironment();

//连接比特币信号处理程序
    noui_connect();

    return (AppInit(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE);
}
