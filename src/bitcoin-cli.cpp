
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

#include <chainparamsbase.h>
#include <clientversion.h>
#include <fs.h>
#include <rpc/client.h>
#include <rpc/protocol.h>
#include <util/system.h>
#include <util/strencodings.h>

#include <memory>
#include <stdio.h>
#include <tuple>

#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include <support/events.h>

#include <univalue.h>

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

static const char DEFAULT_RPCCONNECT[] = "127.0.0.1";
static const int DEFAULT_HTTP_CLIENT_TIMEOUT=900;
static const bool DEFAULT_NAMED=false;
static const int CONTINUE_EXECUTION=-1;

static void SetupCliArgs()
{
    const auto defaultBaseParams = CreateBaseChainParams(CBaseChainParams::MAIN);
    const auto testnetBaseParams = CreateBaseChainParams(CBaseChainParams::TESTNET);
    const auto regtestBaseParams = CreateBaseChainParams(CBaseChainParams::REGTEST);

    gArgs.AddArg("-?", "This help message", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-version", "Print version and exit", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-conf=<file>", strprintf("Specify configuration file. Relative paths will be prefixed by datadir location. (default: %s)", BITCOIN_CONF_FILENAME), false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-datadir=<dir>", "Specify data directory", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-getinfo", "Get general information from the remote server. Note that unlike server-side RPC calls, the results of -getinfo is the result of multiple non-atomic requests. Some entries in the result may represent results from different states (e.g. wallet balance may be as of a different block from the chain state reported)", false, OptionsCategory::OPTIONS);
    SetupChainParamsBaseOptions();
    gArgs.AddArg("-named", strprintf("Pass named instead of positional arguments (default: %s)", DEFAULT_NAMED), false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-rpcclienttimeout=<n>", strprintf("Timeout in seconds during HTTP requests, or 0 for no timeout. (default: %d)", DEFAULT_HTTP_CLIENT_TIMEOUT), false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-rpcconnect=<ip>", strprintf("Send commands to node running on <ip> (default: %s)", DEFAULT_RPCCONNECT), false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-rpccookiefile=<loc>", "Location of the auth cookie. Relative paths will be prefixed by a net-specific datadir location. (default: data dir)", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-rpcpassword=<pw>", "Password for JSON-RPC connections", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-rpcport=<port>", strprintf("Connect to JSON-RPC on <port> (default: %u, testnet: %u, regtest: %u)", defaultBaseParams->RPCPort(), testnetBaseParams->RPCPort(), regtestBaseParams->RPCPort()), false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-rpcuser=<user>", "Username for JSON-RPC connections", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-rpcwait", "Wait for RPC server to start", false, OptionsCategory::OPTIONS);
gArgs.AddArg("-rpcwallet=<walletname>", "Send RPC for non-default wallet on RPC server (needs to exactly match corresponding -wallet option passed to bitcoind). This changes the RPC endpoint used, e.g. http://127.0.0.1:8332/wallet/<walletname>“，false，选项类别：：选项）；
    gArgs.AddArg("-stdin", "Read extra arguments from standard input, one per line until EOF/Ctrl-D (recommended for sensitive information such as passphrases). When combined with -stdinrpcpass, the first line from standard input is used for the RPC password.", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-stdinrpcpass", "Read RPC password from standard input as a single line. When combined with -stdin, the first line from standard input is used for the RPC password.", false, OptionsCategory::OPTIONS);

//隐藏的
    gArgs.AddArg("-h", "", false, OptionsCategory::HIDDEN);
    gArgs.AddArg("-help", "", false, OptionsCategory::HIDDEN);
}

/*libevent事件日志回调*/
static void libevent_log_cb(int severity, const char *msg)
{
#ifndef EVENT_LOG_ERR //在2.0.19中添加了事件日志错误；但在此之前存在事件日志错误。
# define EVENT_LOG_ERR _EVENT_LOG_ERR
#endif
//忽略错误以外的所有内容
    if (severity >= EVENT_LOG_ERR) {
        throw std::runtime_error(strprintf("libevent error: %s", msg));
    }
}

//////////////////////////////////////////////////
//
//起点
//

//
//连接错误引发异常。此错误用于确定
//如果给出-rpcwait，则何时等待。
//
class CConnectionFailed : public std::runtime_error
{
public:

    explicit inline CConnectionFailed(const std::string& msg) :
        std::runtime_error(msg)
    {}

};

//
//当需要停止进程或
//当预期继续执行时，继续执行。
//
static int AppInitRPC(int argc, char* argv[])
{
//
//参数
//
    SetupCliArgs();
    std::string error;
    if (!gArgs.ParseParameters(argc, argv, error)) {
        fprintf(stderr, "Error parsing command line arguments: %s\n", error.c_str());
        return EXIT_FAILURE;
    }
    if (argc < 2 || HelpRequested(gArgs) || gArgs.IsArgSet("-version")) {
        std::string strUsage = PACKAGE_NAME " RPC client version " + FormatFullVersion() + "\n";
        if (!gArgs.IsArgSet("-version")) {
            strUsage += "\n"
                "Usage:  bitcoin-cli [options] <command> [params]  Send command to " PACKAGE_NAME "\n"
                "or:     bitcoin-cli [options] -named <command> [name=value]...  Send command to " PACKAGE_NAME " (with named arguments)\n"
                "or:     bitcoin-cli [options] help                List commands\n"
                "or:     bitcoin-cli [options] help <command>      Get help for a command\n";
            strUsage += "\n" + gArgs.GetHelpMessage();
        }

        fprintf(stdout, "%s", strUsage.c_str());
        if (argc < 2) {
            fprintf(stderr, "Error: too few parameters\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    if (!fs::is_directory(GetDataDir(false))) {
        fprintf(stderr, "Error: Specified data directory \"%s\" does not exist.\n", gArgs.GetArg("-datadir", "").c_str());
        return EXIT_FAILURE;
    }
    if (!gArgs.ReadConfigFiles(error, true)) {
        fprintf(stderr, "Error reading configuration file: %s\n", error.c_str());
        return EXIT_FAILURE;
    }
//检查-testnet或-regtest参数（baseparams（）调用仅在此子句之后有效）
    try {
        SelectBaseParams(gArgs.GetChainName());
    } catch (const std::exception& e) {
        fprintf(stderr, "Error: %s\n", e.what());
        return EXIT_FAILURE;
    }
    return CONTINUE_EXECUTION;
}


/*请求填写的回复结构*/
struct HTTPReply
{
    HTTPReply(): status(0), error(-1) {}

    int status;
    int error;
    std::string body;
};

static const char *http_errorstring(int code)
{
    switch(code) {
#if LIBEVENT_VERSION_NUMBER >= 0x02010300
    case EVREQ_HTTP_TIMEOUT:
        return "timeout reached";
    case EVREQ_HTTP_EOF:
        return "EOF reached";
    case EVREQ_HTTP_INVALID_HEADER:
        return "error while reading header, or invalid header";
    case EVREQ_HTTP_BUFFER_ERROR:
        return "error encountered while reading or writing";
    case EVREQ_HTTP_REQUEST_CANCEL:
        return "request was canceled";
    case EVREQ_HTTP_DATA_TOO_LONG:
        return "response body is larger than allowed";
#endif
    default:
        return "unknown";
    }
}

static void http_request_done(struct evhttp_request *req, void *ctx)
{
    HTTPReply *reply = static_cast<HTTPReply*>(ctx);

    if (req == nullptr) {
        /*如果req为nullptr，则表示在连接时发生错误：
         *错误代码将被传递到http_error_cb。
         **/

        reply->status = 0;
        return;
    }

    reply->status = evhttp_request_get_response_code(req);

    struct evbuffer *buf = evhttp_request_get_input_buffer(req);
    if (buf)
    {
        size_t size = evbuffer_get_length(buf);
        const char *data = (const char*)evbuffer_pullup(buf, size);
        if (data)
            reply->body = std::string(data, size);
        evbuffer_drain(buf, size);
    }
}

#if LIBEVENT_VERSION_NUMBER >= 0x02010300
static void http_error_cb(enum evhttp_request_error err, void *ctx)
{
    HTTPReply *reply = static_cast<HTTPReply*>(ctx);
    reply->error = err;
}
#endif

/*类处理从命令行到JSON-RPC请求的转换，
 *以及转换回可以作为结果显示的JSON对象。
 **/

class BaseRequestHandler
{
public:
    virtual ~BaseRequestHandler() {}
    virtual UniValue PrepareRequest(const std::string& method, const std::vector<std::string>& args) = 0;
    virtual UniValue ProcessReply(const UniValue &batch_in) = 0;
};

/*处理GetInfo请求*/
class GetinfoRequestHandler: public BaseRequestHandler
{
public:
    const int ID_NETWORKINFO = 0;
    const int ID_BLOCKCHAININFO = 1;
    const int ID_WALLETINFO = 2;

    /*创建模拟的“getinfo”请求。*/
    UniValue PrepareRequest(const std::string& method, const std::vector<std::string>& args) override
    {
        if (!args.empty()) {
            throw std::runtime_error("-getinfo takes no arguments");
        }
        UniValue result(UniValue::VARR);
        result.push_back(JSONRPCRequestObj("getnetworkinfo", NullUniValue, ID_NETWORKINFO));
        result.push_back(JSONRPCRequestObj("getblockchaininfo", NullUniValue, ID_BLOCKCHAININFO));
        result.push_back(JSONRPCRequestObj("getwalletinfo", NullUniValue, ID_WALLETINFO));
        return result;
    }

    /*从批中收集值并形成模拟的“getinfo”答复。*/
    UniValue ProcessReply(const UniValue &batch_in) override
    {
        UniValue result(UniValue::VOBJ);
        std::vector<UniValue> batch = JSONRPCProcessBatchReply(batch_in, 3);
//getNetworkInfo（）和getBlockChainInfo（）中的错误是致命的，请将它们传递给
//如果没有钱包，则允许getwalletinfo（）失败。
        if (!batch[ID_NETWORKINFO]["error"].isNull()) {
            return batch[ID_NETWORKINFO];
        }
        if (!batch[ID_BLOCKCHAININFO]["error"].isNull()) {
            return batch[ID_BLOCKCHAININFO];
        }
        result.pushKV("version", batch[ID_NETWORKINFO]["result"]["version"]);
        result.pushKV("protocolversion", batch[ID_NETWORKINFO]["result"]["protocolversion"]);
        if (!batch[ID_WALLETINFO].isNull()) {
            result.pushKV("walletversion", batch[ID_WALLETINFO]["result"]["walletversion"]);
            result.pushKV("balance", batch[ID_WALLETINFO]["result"]["balance"]);
        }
        result.pushKV("blocks", batch[ID_BLOCKCHAININFO]["result"]["blocks"]);
        result.pushKV("timeoffset", batch[ID_NETWORKINFO]["result"]["timeoffset"]);
        result.pushKV("connections", batch[ID_NETWORKINFO]["result"]["connections"]);
        result.pushKV("proxy", batch[ID_NETWORKINFO]["result"]["networks"][0]["proxy"]);
        result.pushKV("difficulty", batch[ID_BLOCKCHAININFO]["result"]["difficulty"]);
        result.pushKV("testnet", UniValue(batch[ID_BLOCKCHAININFO]["result"]["chain"].get_str() == "test"));
        if (!batch[ID_WALLETINFO].isNull()) {
            result.pushKV("walletversion", batch[ID_WALLETINFO]["result"]["walletversion"]);
            result.pushKV("balance", batch[ID_WALLETINFO]["result"]["balance"]);
            result.pushKV("keypoololdest", batch[ID_WALLETINFO]["result"]["keypoololdest"]);
            result.pushKV("keypoolsize", batch[ID_WALLETINFO]["result"]["keypoolsize"]);
            if (!batch[ID_WALLETINFO]["result"]["unlocked_until"].isNull()) {
                result.pushKV("unlocked_until", batch[ID_WALLETINFO]["result"]["unlocked_until"]);
            }
            result.pushKV("paytxfee", batch[ID_WALLETINFO]["result"]["paytxfee"]);
        }
        result.pushKV("relayfee", batch[ID_NETWORKINFO]["result"]["relayfee"]);
        result.pushKV("warnings", batch[ID_NETWORKINFO]["result"]["warnings"]);
        return JSONRPCReplyObj(result, NullUniValue, 1);
    }
};

/*处理默认单个请求*/
class DefaultRequestHandler: public BaseRequestHandler {
public:
    UniValue PrepareRequest(const std::string& method, const std::vector<std::string>& args) override
    {
        UniValue params;
        if(gArgs.GetBoolArg("-named", DEFAULT_NAMED)) {
            params = RPCConvertNamedValues(method, args);
        } else {
            params = RPCConvertValues(method, args);
        }
        return JSONRPCRequestObj(method, params, 1);
    }

    UniValue ProcessReply(const UniValue &reply) override
    {
        return reply.get_obj();
    }
};

static UniValue CallRPC(BaseRequestHandler *rh, const std::string& strMethod, const std::vector<std::string>& args)
{
    std::string host;
//按照优先顺序，我们选择以下端口：
//1。-RPCPATH
//2。-rpcconnect中的端口（即：在ipv4中或：在ipv6中）
//三。链的默认端口
    int port = BaseParams().RPCPort();
    SplitHostPort(gArgs.GetArg("-rpcconnect", DEFAULT_RPCCONNECT), port, host);
    port = gArgs.GetArg("-rpcport", port);

//获取事件库
    raii_event_base base = obtain_event_base();

//同步查找主机名
    raii_evhttp_connection evcon = obtain_evhttp_connection_base(base.get(), host, port);
    evhttp_connection_set_timeout(evcon.get(), gArgs.GetArg("-rpcclienttimeout", DEFAULT_HTTP_CLIENT_TIMEOUT));

    HTTPReply response;
    raii_evhttp_request req = obtain_evhttp_request(http_request_done, (void*)&response);
    if (req == nullptr)
        throw std::runtime_error("create http request failed");
#if LIBEVENT_VERSION_NUMBER >= 0x02010300
    evhttp_request_set_error_cb(req.get(), http_error_cb);
#endif

//获得证书
    std::string strRPCUserColonPass;
    bool failedToGetAuthCookie = false;
    if (gArgs.GetArg("-rpcpassword", "") == "") {
//如果没有提供密码，请尝试返回到基于cookie的身份验证
        if (!GetAuthCookie(&strRPCUserColonPass)) {
            failedToGetAuthCookie = true;
        }
    } else {
        strRPCUserColonPass = gArgs.GetArg("-rpcuser", "") + ":" + gArgs.GetArg("-rpcpassword", "");
    }

    struct evkeyvalq* output_headers = evhttp_request_get_output_headers(req.get());
    assert(output_headers);
    evhttp_add_header(output_headers, "Host", host.c_str());
    evhttp_add_header(output_headers, "Connection", "close");
    evhttp_add_header(output_headers, "Authorization", (std::string("Basic ") + EncodeBase64(strRPCUserColonPass)).c_str());

//附加请求数据
    std::string strRequest = rh->PrepareRequest(strMethod, args).write() + "\n";
    struct evbuffer* output_buffer = evhttp_request_get_output_buffer(req.get());
    assert(output_buffer);
    evbuffer_add(output_buffer, strRequest.data(), strRequest.size());

//检查我们是否应该使用特殊的钱包端点
    std::string endpoint = "/";
    if (!gArgs.GetArgs("-rpcwallet").empty()) {
        std::string walletName = gArgs.GetArg("-rpcwallet", "");
        char *encodedURI = evhttp_uriencode(walletName.c_str(), walletName.size(), false);
        if (encodedURI) {
            endpoint = "/wallet/"+ std::string(encodedURI);
            free(encodedURI);
        }
        else {
            throw CConnectionFailed("uri-encode failed");
        }
    }
    int r = evhttp_make_request(evcon.get(), req.get(), EVHTTP_REQ_POST, endpoint.c_str());
req.release(); //在上述呼叫中所有权转移到了evcon
    if (r != 0) {
        throw CConnectionFailed("send http request failed");
    }

    event_base_dispatch(base.get());

    if (response.status == 0) {
        std::string responseErrorMessage;
        if (response.error != -1) {
            responseErrorMessage = strprintf(" (error code %d - \"%s\")", response.error, http_errorstring(response.error));
        }
        throw CConnectionFailed(strprintf("Could not connect to the server %s:%d%s\n\nMake sure the bitcoind server is running and that you are connecting to the correct RPC port.", host, port, responseErrorMessage));
    } else if (response.status == HTTP_UNAUTHORIZED) {
        if (failedToGetAuthCookie) {
            throw std::runtime_error(strprintf(
                "Could not locate RPC credentials. No authentication cookie could be found, and RPC password is not set.  See -rpcpassword and -stdinrpcpass.  Configuration file: (%s)",
                GetConfigFile(gArgs.GetArg("-conf", BITCOIN_CONF_FILENAME)).string().c_str()));
        } else {
            throw std::runtime_error("Authorization failed: Incorrect rpcuser or rpcpassword");
        }
    } else if (response.status >= 400 && response.status != HTTP_BAD_REQUEST && response.status != HTTP_NOT_FOUND && response.status != HTTP_INTERNAL_SERVER_ERROR)
        throw std::runtime_error(strprintf("server returned HTTP error %d", response.status));
    else if (response.body.empty())
        throw std::runtime_error("no response from server");

//解析应答
    UniValue valReply(UniValue::VSTR);
    if (!valReply.read(response.body))
        throw std::runtime_error("couldn't parse reply from server");
    const UniValue reply = rh->ProcessReply(valReply);
    if (reply.empty())
        throw std::runtime_error("expected reply to have result, error and id properties");

    return reply;
}

static int CommandLineRPC(int argc, char *argv[])
{
    std::string strPrint;
    int nRet = 0;
    try {
//跳过开关
        while (argc > 1 && IsSwitchChar(argv[1][0])) {
            argc--;
            argv++;
        }
        std::string rpcPass;
        if (gArgs.GetBoolArg("-stdinrpcpass", false)) {
            if (!std::getline(std::cin, rpcPass)) {
                throw std::runtime_error("-stdinrpcpass specified but failed to read from standard input");
            }
            gArgs.ForceSetArg("-rpcpassword", rpcPass);
        }
        std::vector<std::string> args = std::vector<std::string>(&argv[1], &argv[argc]);
        if (gArgs.GetBoolArg("-stdin", false)) {
//从stdin和append中每行读取一个参数
            std::string line;
            while (std::getline(std::cin, line)) {
                args.push_back(line);
            }
        }
        std::unique_ptr<BaseRequestHandler> rh;
        std::string method;
        if (gArgs.GetBoolArg("-getinfo", false)) {
            rh.reset(new GetinfoRequestHandler());
            method = "";
        } else {
            rh.reset(new DefaultRequestHandler());
            if (args.size() < 1) {
                throw std::runtime_error("too few parameters (need at least command)");
            }
            method = args[0];
args.erase(args.begin()); //从参数向量中移除尾随方法名
        }

//使用-rpcwait执行和处理连接故障
        const bool fWait = gArgs.GetBoolArg("-rpcwait", false);
        do {
            try {
                const UniValue reply = CallRPC(rh.get(), method, args);

//解析应答
                const UniValue& result = find_value(reply, "result");
                const UniValue& error  = find_value(reply, "error");

                if (!error.isNull()) {
//误差
                    int code = error["code"].get_int();
                    if (fWait && code == RPC_IN_WARMUP)
                        throw CConnectionFailed("server in warmup");
                    strPrint = "error: " + error.write();
                    nRet = abs(code);
                    if (error.isObject())
                    {
                        UniValue errCode = find_value(error, "code");
                        UniValue errMsg  = find_value(error, "message");
                        strPrint = errCode.isNull() ? "" : "error code: "+errCode.getValStr()+"\n";

                        if (errMsg.isStr())
                            strPrint += "error message:\n"+errMsg.get_str();

                        if (errCode.isNum() && errCode.get_int() == RPC_WALLET_NOT_SPECIFIED) {
                            strPrint += "\nTry adding \"-rpcwallet=<filename>\" option to bitcoin-cli command line.";
                        }
                    }
                } else {
//结果
                    if (result.isNull())
                        strPrint = "";
                    else if (result.isStr())
                        strPrint = result.get_str();
                    else
                        strPrint = result.write(2);
                }
//连接成功，无需重试。
                break;
            }
            catch (const CConnectionFailed&) {
                if (fWait)
                    MilliSleep(1000);
                else
                    throw;
            }
        } while (fWait);
    }
    catch (const std::exception& e) {
        strPrint = std::string("error: ") + e.what();
        nRet = EXIT_FAILURE;
    }
    catch (...) {
        PrintExceptionContinue(nullptr, "CommandLineRPC()");
        throw;
    }

    if (strPrint != "") {
        fprintf((nRet == 0 ? stdout : stderr), "%s\n", strPrint.c_str());
    }
    return nRet;
}

int main(int argc, char* argv[])
{
#ifdef WIN32
    util::WinCmdLineArgs winArgs;
    std::tie(argc, argv) = winArgs.get();
#endif
    SetupEnvironment();
    if (!SetupNetworking()) {
        fprintf(stderr, "Error: Initializing networking failed\n");
        return EXIT_FAILURE;
    }
    event_set_log_callback(&libevent_log_cb);

    try {
        int ret = AppInitRPC(argc, argv);
        if (ret != CONTINUE_EXECUTION)
            return ret;
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "AppInitRPC()");
        return EXIT_FAILURE;
    } catch (...) {
        PrintExceptionContinue(nullptr, "AppInitRPC()");
        return EXIT_FAILURE;
    }

    int ret = EXIT_FAILURE;
    try {
        ret = CommandLineRPC(argc, argv);
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "CommandLineRPC()");
    } catch (...) {
        PrintExceptionContinue(nullptr, "CommandLineRPC()");
    }
    return ret;
}
