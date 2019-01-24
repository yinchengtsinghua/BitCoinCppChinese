
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

#include <util/system.h>

#include <chainparamsbase.h>
#include <random.h>
#include <serialize.h>
#include <util/strencodings.h>

#include <stdarg.h>

#if (defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
#include <pthread.h>
#include <pthread_np.h>
#endif

#ifndef WIN32
//对于身体姿势
#ifdef __linux__

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#define _POSIX_C_SOURCE 200112L

#endif //L·L·鲁努克斯

#include <algorithm>
#include <fcntl.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/stat.h>

#else

#ifdef _MSC_VER
#pragma warning(disable:4786)
#pragma warning(disable:4804)
#pragma warning(disable:4805)
#pragma warning(disable:4717)
#endif

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501

#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501

#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <codecvt>

/*clude<io.h>/*用于提交*/
include<shellapi.h>
include<shlobj.h>
第二节

ifdef有系统
include<sys/prctl.h>
第二节

ifdef有撘mallopt撘arena撘max
include<malloc.h>
第二节

include<openssl/crypto.h>
include<openssl/rand.h>
include<openssl/conf.h>
include<thread>

//应用程序启动时间（用于运行时间计算）
const int64_t instartuptime=gettime（）；

const char*const bitcoin_conf_filename=“bitcoin.conf”；
const char*const bitcoin_pid_filename=“bitcoind.pid”；

Argsmanager Gargs；

/**init openssl库多线程支持*/

static std::unique_ptr<CCriticalSection[]> ppmutexOpenSSL;
void locking_callback(int mode, int i, const char* file, int line) NO_THREAD_SAFETY_ANALYSIS
{
    if (mode & CRYPTO_LOCK) {
        ENTER_CRITICAL_SECTION(ppmutexOpenSSL[i]);
    } else {
        LEAVE_CRITICAL_SECTION(ppmutexOpenSSL[i]);
    }
}

//用于包装openssl安装/拆卸的singleton。
class CInit
{
public:
    CInit()
    {
//init openssl库多线程支持
        ppmutexOpenSSL.reset(new CCriticalSection[CRYPTO_num_locks()]);
        CRYPTO_set_locking_callback(locking_callback);

//openssl可以选择加载一个配置文件，其中列出了可选的可加载模块和引擎。
//我们不使用它们，所以不需要配置。然而，我们的一些libs可能调用函数
//尝试加载配置文件，如果丢失，可能导致exit（）或崩溃。
//或腐败。明确告诉openssl不要尝试加载文件。我们的LIB结果将是
//配置似乎已加载，并且没有可用的模块/引擎。
        OPENSSL_no_config();

#ifdef WIN32
//使用屏幕的当前内容为openssl prng设定种子
        RAND_screen();
#endif

//带性能计数器的SEED OpenSSL PRNG
        RandAddSeed();
    }
    ~CInit()
    {
//安全地擦除prng使用的内存
        RAND_cleanup();
//关闭OpenSSL库多线程支持
        CRYPTO_set_locking_callback(nullptr);
//现在清除锁集以保持与构造函数的对称性。
        ppmutexOpenSSL.reset();
    }
}
instance_of_cinit;

/*包含当前持有的所有目录锁的映射。后
 *成功锁定，这些将保留在此处，直到全局析构函数
 *将其清除，从而自动解锁，或释放DirectoryLock
 *被调用。
 **/

static std::map<std::string, std::unique_ptr<fsbridge::FileLock>> dir_locks;
/*互斥保护目录锁。*/
static std::mutex cs_dir_locks;

bool LockDirectory(const fs::path& directory, const std::string lockfile_name, bool probe_only)
{
    std::lock_guard<std::mutex> ulock(cs_dir_locks);
    fs::path pathLockFile = directory / lockfile_name;

//如果映射中已经存在此目录的锁，请不要尝试重新锁定它。
    if (dir_locks.count(pathLockFile.string())) {
        return true;
    }

//如果不存在，则创建空的锁文件。
    FILE* file = fsbridge::fopen(pathLockFile, "a");
    if (file) fclose(file);
    auto lock = MakeUnique<fsbridge::FileLock>(pathLockFile);
    if (!lock->TryLock()) {
        return error("Error while attempting to lock directory %s: %s", directory.string(), lock->GetReason());
    }
    if (!probe_only) {
//锁定成功，我们不只是探测，把它放到地图上
        dir_locks.emplace(pathLockFile.string(), std::move(lock));
    }
    return true;
}

void ReleaseDirectoryLocks()
{
    std::lock_guard<std::mutex> ulock(cs_dir_locks);
    dir_locks.clear();
}

bool DirIsWritable(const fs::path& directory)
{
    fs::path tmpFile = directory / fs::unique_path();

    FILE* file = fsbridge::fopen(tmpFile, "a");
    if (!file) return false;

    fclose(file);
    remove(tmpFile);

    return true;
}

/*
 *将字符串参数解释为布尔值。
 *
 *atoi（）的定义要求非数字字符串值，如“foo”，
 *返回0。这意味着如果用户无意中提供非整数
 *这里的参数，返回值总是错误的。这意味着-foo=false
 *执行用户可能期望的操作，但-foo=true定义良好，但确实如此
 *不要做他们可能期望的事情。
 *
 *当给定的输入不可表示为时，ATOI（）的返回值未定义。
 *在大多数系统上，这意味着字符串值介于“-2147483648”和
 *“2147483647”定义良好（此方法将返回true）。设置
 然而，在大多数系统上，txindex=2147483648可能是未定义的。
 *
 *更广泛地讨论这个话题（以及广泛的意见）
 *正确更改此代码的方法），请参阅pr12713。
 **/

static bool InterpretBool(const std::string& strValue)
{
    if (strValue.empty())
        return true;
    return (atoi(strValue) != 0);
}

/*argsManager的内部助手函数*/
class ArgsManagerHelper {
public:
    typedef std::map<std::string, std::vector<std::string>> MapArgs;

    /*确定是否在默认部分中使用配置设置，
     *另请参见下面关于argsManager:：argsManager（）的注释。*/

    static inline bool UseDefaultSection(const ArgsManager& am, const std::string& arg) EXCLUSIVE_LOCKS_REQUIRED(am.cs_args)
    {
        return (am.m_network == CBaseChainParams::MAIN || am.m_network_only_args.count(arg) == 0);
    }

    /*将常规参数转换为特定于网络的设置*/
    static inline std::string NetworkArg(const ArgsManager& am, const std::string& arg)
    {
        assert(arg.length() > 1 && arg[0] == '-');
        return "-" + am.m_network + "." + arg.substr(1);
    }

    /*在地图中查找参数并将其添加到矢量中*/
    static inline void AddArgs(std::vector<std::string>& res, const MapArgs& map_args, const std::string& arg)
    {
        auto it = map_args.find(arg);
        if (it != map_args.end()) {
            res.insert(res.end(), it->second.begin(), it->second.end());
        }
    }

    /*如果在映射中设置了参数，则返回true/false，并且
     *返回可能具有多个值的第一个（或最后一个）
     **/

    static inline std::pair<bool,std::string> GetArgHelper(const MapArgs& map_args, const std::string& arg, bool getLast = false)
    {
        auto it = map_args.find(arg);

        if (it == map_args.end() || it->second.empty()) {
            return std::make_pair(false, std::string());
        }

        if (getLast) {
            return std::make_pair(true, it->second.back());
        } else {
            return std::make_pair(true, it->second.front());
        }
    }

    /*获取参数的字符串值，返回一对布尔值
     *指示找到参数，以及参数的值
     *如果找到（如果没有找到，则为空字符串）。
     **/

    static inline std::pair<bool,std::string> GetArg(const ArgsManager &am, const std::string& arg)
    {
        LOCK(am.cs_args);
        std::pair<bool,std::string> found_result(false, std::string());

//我们将“true”传递给getargHelper以便返回最后一个
//从命令行看到的参数值（所以“bitcoind-foo=bar
//-foo=baz“给出getarg（am，“foo”）==真，“baz”
        found_result = GetArgHelper(am.m_override_args, arg, true);
        if (found_result.first) {
            return found_result;
        }

//但与此相反，我们返回配置文件中看到的第一个参数，
//所以配置文件中的“foo=bar\n foo=baz”给出了
//getarg（am，“foo”）=真，“bar”
        if (!am.m_network.empty()) {
            found_result = GetArgHelper(am.m_config_args, NetworkArg(am, arg));
            if (found_result.first) {
                return found_result;
            }
        }

        if (UseDefaultSection(am, arg)) {
            found_result = GetArgHelper(am.m_config_args, arg);
            if (found_result.first) {
                return found_result;
            }
        }

        return found_result;
    }

    /*-testnet和-regtest参数的特殊测试，因为
     *不要被类似“[regtest]testnet=1”的疯狂所迷惑。
     **/

    static inline bool GetNetBoolArg(const ArgsManager &am, const std::string& net_arg) EXCLUSIVE_LOCKS_REQUIRED(am.cs_args)
    {
        std::pair<bool,std::string> found_result(false,std::string());
        found_result = GetArgHelper(am.m_override_args, net_arg, true);
        if (!found_result.first) {
            found_result = GetArgHelper(am.m_config_args, net_arg, true);
            if (!found_result.first) {
return false; //未设置
            }
        }
return InterpretBool(found_result.second); //已设置，因此评估
    }
};

/*
 *将-nofoo解释为用户提供的-foo=0。
 *
 *此方法还跟踪何时提供了-no表单，如果提供了-no表单，
 *检查是否有双重阴性（-nofoo=0->-foo=1）。
 *
 *如果没有双负号，则从键中删除“否”。
 *并返回true，表示调用方应清除args向量
 *表示否定选项。
 *
 *如果有双负号，则从键中删除“否”，设置
 *值为“1”，返回false。
 *
 *如果没有“否”，则不会影响键和值并返回
 *假。
 *
 *如果某个选项被否定，可以稍后使用
 *IsargNegated（）方法。其中一个用例是有一种方法可以禁用
 *通常不是布尔值的选项（例如，使用-nodebuglogfile请求
 *调试日志输出根本不发送到任何文件）。
 **/

static bool InterpretNegatedOption(std::string& key, std::string& val)
{
    assert(key[0] == '-');

    size_t option_index = key.find('.');
    if (option_index == std::string::npos) {
        option_index = 1;
    } else {
        ++option_index;
    }
    if (key.substr(option_index, 2) == "no") {
        bool bool_val = InterpretBool(val);
        key.erase(option_index, 2);
        if (!bool_val ) {
//支持-nofoo=0等双重否定（但不鼓励）
            LogPrintf("Warning: parsed potentially confusing double-negative %s=%s\n", key, val);
            val = "1";
        } else {
            return true;
        }
    }
    return false;
}

ArgsManager::ArgsManager() :
    /*如果
     *在regtest/testnet上运行时使用了mainnet（反之亦然）。
     *将它们设置为\u only\u参数可确保共享配置文件
     *在mainnet和regtest/testnet之间不会因为这些而导致问题。
     *意外参数。*/

    m_network_only_args{
      "-addnode", "-connect",
      "-port", "-bind",
      "-rpcport", "-rpcbind",
      "-wallet",
    }
{
//无事可做
}

const std::set<std::string> ArgsManager::GetUnsuitableSectionOnlyArgs() const
{
    std::set<std::string> unsuitables;

    LOCK(cs_args);

//如果没有选择分区，不用担心
    if (m_network.empty()) return std::set<std::string> {};

//如果可以使用此网络的默认部分，请不要担心
    if (m_network == CBaseChainParams::MAIN) return std::set<std::string> {};

    for (const auto& arg : m_network_only_args) {
        std::pair<bool, std::string> found_result;

//如果这个选项被覆盖，就可以了
        found_result = ArgsManagerHelper::GetArgHelper(m_override_args, arg);
        if (found_result.first) continue;

//如果此选项有特定于网络的值，则可以
        found_result = ArgsManagerHelper::GetArgHelper(m_config_args, ArgsManagerHelper::NetworkArg(*this, arg));
        if (found_result.first) continue;

//如果这个选项没有默认值，就可以了
        found_result = ArgsManagerHelper::GetArgHelper(m_config_args, arg);
        if (!found_result.first) continue;

//否则，发出警告
        unsuitables.insert(arg);
    }
    return unsuitables;
}


const std::set<std::string> ArgsManager::GetUnrecognizedSections() const
{
//要在配置文件中识别的节名称。
    static const std::set<std::string> available_sections{
        CBaseChainParams::REGTEST,
        CBaseChainParams::TESTNET,
        CBaseChainParams::MAIN
    };
    std::set<std::string> diff;

    LOCK(cs_args);
    std::set_difference(
        m_config_sections.begin(), m_config_sections.end(),
        available_sections.begin(), available_sections.end(),
        std::inserter(diff, diff.end()));
    return diff;
}

void ArgsManager::SelectConfigNetwork(const std::string& network)
{
    LOCK(cs_args);
    m_network = network;
}

bool ArgsManager::ParseParameters(int argc, const char* const argv[], std::string& error)
{
    LOCK(cs_args);
    m_override_args.clear();

    for (int i = 1; i < argc; i++) {
        std::string key(argv[i]);
        std::string val;
        size_t is_index = key.find('=');
        if (is_index != std::string::npos) {
            val = key.substr(is_index + 1);
            key.erase(is_index);
        }
#ifdef WIN32
        std::transform(key.begin(), key.end(), key.begin(), ToLower);
        if (key[0] == '/')
            key[0] = '-';
#endif

        if (key[0] != '-')
            break;

//转换--foo到-foo
        if (key.length() > 1 && key[1] == '-')
            key.erase(0, 1);

//检查-nofoo
        if (InterpretNegatedOption(key, val)) {
            m_override_args[key].clear();
        } else {
            m_override_args[key].push_back(val);
        }

//检查arg是否已知
        if (!(IsSwitchChar(key[0]) && key.size() == 1)) {
            if (!IsArgKnown(key)) {
                error = strprintf("Invalid parameter %s", key.c_str());
                return false;
            }
        }
    }

//我们不允许来自命令行的-includeconf，所以我们在这里清除它
    auto it = m_override_args.find("-includeconf");
    if (it != m_override_args.end()) {
        if (it->second.size() > 0) {
            for (const auto& ic : it->second) {
                error += "-includeconf cannot be used from commandline; -includeconf=" + ic + "\n";
            }
            return false;
        }
    }
    return true;
}

bool ArgsManager::IsArgKnown(const std::string& key) const
{
    size_t option_index = key.find('.');
    std::string arg_no_net;
    if (option_index == std::string::npos) {
        arg_no_net = key;
    } else {
        arg_no_net = std::string("-") + key.substr(option_index + 1, std::string::npos);
    }

    LOCK(cs_args);
    for (const auto& arg_map : m_available_args) {
        if (arg_map.second.count(arg_no_net)) return true;
    }
    return false;
}

std::vector<std::string> ArgsManager::GetArgs(const std::string& strArg) const
{
    std::vector<std::string> result = {};
if (IsArgNegated(strArg)) return result; //特例

    LOCK(cs_args);

    ArgsManagerHelper::AddArgs(result, m_override_args, strArg);
    if (!m_network.empty()) {
        ArgsManagerHelper::AddArgs(result, m_config_args, ArgsManagerHelper::NetworkArg(*this, strArg));
    }

    if (ArgsManagerHelper::UseDefaultSection(*this, strArg)) {
        ArgsManagerHelper::AddArgs(result, m_config_args, strArg);
    }

    return result;
}

bool ArgsManager::IsArgSet(const std::string& strArg) const
{
if (IsArgNegated(strArg)) return true; //特例
    return ArgsManagerHelper::GetArg(*this, strArg).first;
}

bool ArgsManager::IsArgNegated(const std::string& strArg) const
{
    LOCK(cs_args);

    const auto& ov = m_override_args.find(strArg);
    if (ov != m_override_args.end()) return ov->second.empty();

    if (!m_network.empty()) {
        const auto& cfs = m_config_args.find(ArgsManagerHelper::NetworkArg(*this, strArg));
        if (cfs != m_config_args.end()) return cfs->second.empty();
    }

    const auto& cf = m_config_args.find(strArg);
    if (cf != m_config_args.end()) return cf->second.empty();

    return false;
}

std::string ArgsManager::GetArg(const std::string& strArg, const std::string& strDefault) const
{
    if (IsArgNegated(strArg)) return "0";
    std::pair<bool,std::string> found_res = ArgsManagerHelper::GetArg(*this, strArg);
    if (found_res.first) return found_res.second;
    return strDefault;
}

int64_t ArgsManager::GetArg(const std::string& strArg, int64_t nDefault) const
{
    if (IsArgNegated(strArg)) return 0;
    std::pair<bool,std::string> found_res = ArgsManagerHelper::GetArg(*this, strArg);
    if (found_res.first) return atoi64(found_res.second);
    return nDefault;
}

bool ArgsManager::GetBoolArg(const std::string& strArg, bool fDefault) const
{
    if (IsArgNegated(strArg)) return false;
    std::pair<bool,std::string> found_res = ArgsManagerHelper::GetArg(*this, strArg);
    if (found_res.first) return InterpretBool(found_res.second);
    return fDefault;
}

bool ArgsManager::SoftSetArg(const std::string& strArg, const std::string& strValue)
{
    LOCK(cs_args);
    if (IsArgSet(strArg)) return false;
    ForceSetArg(strArg, strValue);
    return true;
}

bool ArgsManager::SoftSetBoolArg(const std::string& strArg, bool fValue)
{
    if (fValue)
        return SoftSetArg(strArg, std::string("1"));
    else
        return SoftSetArg(strArg, std::string("0"));
}

void ArgsManager::ForceSetArg(const std::string& strArg, const std::string& strValue)
{
    LOCK(cs_args);
    m_override_args[strArg] = {strValue};
}

void ArgsManager::AddArg(const std::string& name, const std::string& help, const bool debug_only, const OptionsCategory& cat)
{
//从帮助参数中拆分参数名称
    size_t eq_index = name.find('=');
    if (eq_index == std::string::npos) {
        eq_index = name.size();
    }

    LOCK(cs_args);
    std::map<std::string, Arg>& arg_map = m_available_args[cat];
    auto ret = arg_map.emplace(name.substr(0, eq_index), Arg(name.substr(eq_index, name.size() - eq_index), help, debug_only));
assert(ret.second); //确保实际发生了插入
}

void ArgsManager::AddHiddenArgs(const std::vector<std::string>& names)
{
    for (const std::string& name : names) {
        AddArg(name, "", false, OptionsCategory::HIDDEN);
    }
}

std::string ArgsManager::GetHelpMessage() const
{
    const bool show_debug = gArgs.GetBoolArg("-help-debug", false);

    std::string usage = "";
    LOCK(cs_args);
    for (const auto& arg_map : m_available_args) {
        switch(arg_map.first) {
            case OptionsCategory::OPTIONS:
                usage += HelpMessageGroup("Options:");
                break;
            case OptionsCategory::CONNECTION:
                usage += HelpMessageGroup("Connection options:");
                break;
            case OptionsCategory::ZMQ:
                usage += HelpMessageGroup("ZeroMQ notification options:");
                break;
            case OptionsCategory::DEBUG_TEST:
                usage += HelpMessageGroup("Debugging/Testing options:");
                break;
            case OptionsCategory::NODE_RELAY:
                usage += HelpMessageGroup("Node relay options:");
                break;
            case OptionsCategory::BLOCK_CREATION:
                usage += HelpMessageGroup("Block creation options:");
                break;
            case OptionsCategory::RPC:
                usage += HelpMessageGroup("RPC server options:");
                break;
            case OptionsCategory::WALLET:
                usage += HelpMessageGroup("Wallet options:");
                break;
            case OptionsCategory::WALLET_DEBUG_TEST:
                if (show_debug) usage += HelpMessageGroup("Wallet debugging/testing options:");
                break;
            case OptionsCategory::CHAINPARAMS:
                usage += HelpMessageGroup("Chain selection options:");
                break;
            case OptionsCategory::GUI:
                usage += HelpMessageGroup("UI Options:");
                break;
            case OptionsCategory::COMMANDS:
                usage += HelpMessageGroup("Commands:");
                break;
            case OptionsCategory::REGISTER_COMMANDS:
                usage += HelpMessageGroup("Register Commands:");
                break;
            default:
                break;
        }

//当我们找到隐藏的选项时，停止
        if (arg_map.first == OptionsCategory::HIDDEN) break;

        for (const auto& arg : arg_map.second) {
            if (show_debug || !arg.second.m_debug_only) {
                std::string name;
                if (arg.second.m_help_param.empty()) {
                    name = arg.first;
                } else {
                    name = arg.first + arg.second.m_help_param;
                }
                usage += HelpMessageOpt(name, arg.second.m_help_text);
            }
        }
    }
    return usage;
}

bool HelpRequested(const ArgsManager& args)
{
    return args.IsArgSet("-?") || args.IsArgSet("-h") || args.IsArgSet("-help") || args.IsArgSet("-help-debug");
}

static const int screenWidth = 79;
static const int optIndent = 2;
static const int msgIndent = 7;

std::string HelpMessageGroup(const std::string &message) {
    return std::string(message) + std::string("\n\n");
}

std::string HelpMessageOpt(const std::string &option, const std::string &message) {
    return std::string(optIndent,' ') + std::string(option) +
           std::string("\n") + std::string(msgIndent,' ') +
           FormatParagraph(message, screenWidth - msgIndent, msgIndent) +
           std::string("\n\n");
}

static std::string FormatException(const std::exception* pex, const char* pszThread)
{
#ifdef WIN32
    char pszModule[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, pszModule, sizeof(pszModule));
#else
    const char* pszModule = "bitcoin";
#endif
    if (pex)
        return strprintf(
            "EXCEPTION: %s       \n%s       \n%s in %s       \n", typeid(*pex).name(), pex->what(), pszModule, pszThread);
    else
        return strprintf(
            "UNKNOWN EXCEPTION       \n%s in %s       \n", pszModule, pszThread);
}

void PrintExceptionContinue(const std::exception* pex, const char* pszThread)
{
    std::string message = FormatException(pex, pszThread);
    LogPrintf("\n\n************************\n%s\n", message);
    fprintf(stderr, "\n\n************************\n%s\n", message.c_str());
}

fs::path GetDefaultDataDir()
{
//Windows<Vista:C:\documents and settings\username\application data\bitcoin
//windows>=vista:c:\users\username\appdata\roaming\bitcoin
//MAC：~/library/application-support/bitcoin
//Unix：~/.bitcoin
#ifdef WIN32
//窗户
    return GetSpecialFolderPath(CSIDL_APPDATA) / "Bitcoin";
#else
    fs::path pathRet;
    char* pszHome = getenv("HOME");
    if (pszHome == nullptr || strlen(pszHome) == 0)
        pathRet = fs::path("/");
    else
        pathRet = fs::path(pszHome);
#ifdef MAC_OSX
//雨衣
    return pathRet / "Library/Application Support/Bitcoin";
#else
//UNIX
    return pathRet / ".bitcoin";
#endif
#endif
}

static fs::path g_blocks_path_cache_net_specific;
static fs::path pathCached;
static fs::path pathCachedNetSpecific;
static CCriticalSection csPathCached;

const fs::path &GetBlocksDir()
{

    LOCK(csPathCached);

    fs::path &path = g_blocks_path_cache_net_specific;

//这可以在logprintf（）的异常期间调用，因此我们缓存
//值，这样我们就不必在这之后进行内存分配了。
    if (!path.empty())
        return path;

    if (gArgs.IsArgSet("-blocksdir")) {
        path = fs::system_complete(gArgs.GetArg("-blocksdir", ""));
        if (!fs::is_directory(path)) {
            path = "";
            return path;
        }
    } else {
        path = GetDataDir(false);
    }

    path /= BaseParams().DataDir();
    path /= "blocks";
    fs::create_directories(path);
    return path;
}

const fs::path &GetDataDir(bool fNetSpecific)
{

    LOCK(csPathCached);

    fs::path &path = fNetSpecific ? pathCachedNetSpecific : pathCached;

//这可以在logprintf（）的异常期间调用，因此我们缓存
//值，这样我们就不必在这之后进行内存分配了。
    if (!path.empty())
        return path;

    if (gArgs.IsArgSet("-datadir")) {
        path = fs::system_complete(gArgs.GetArg("-datadir", ""));
        if (!fs::is_directory(path)) {
            path = "";
            return path;
        }
    } else {
        path = GetDefaultDataDir();
    }
    if (fNetSpecific)
        path /= BaseParams().DataDir();

    if (fs::create_directories(path)) {
//这是第一次运行，也创建钱包子目录
        fs::create_directories(path / "wallets");
    }

    return path;
}

void ClearDatadirCache()
{
    LOCK(csPathCached);

    pathCached = fs::path();
    pathCachedNetSpecific = fs::path();
    g_blocks_path_cache_net_specific = fs::path();
}

fs::path GetConfigFile(const std::string& confPath)
{
    return AbsPathForConfigVal(fs::path(confPath), false);
}

static std::string TrimString(const std::string& str, const std::string& pattern)
{
    std::string::size_type front = str.find_first_not_of(pattern);
    if (front == std::string::npos) {
        return std::string();
    }
    std::string::size_type end = str.find_last_not_of(pattern);
    return str.substr(front, end - front + 1);
}

static bool GetConfigOptions(std::istream& stream, std::string& error, std::vector<std::pair<std::string, std::string>>& options, std::set<std::string>& sections)
{
    std::string str, prefix;
    std::string::size_type pos;
    int linenr = 1;
    while (std::getline(stream, str)) {
        bool used_hash = false;
        if ((pos = str.find('#')) != std::string::npos) {
            str = str.substr(0, pos);
            used_hash = true;
        }
        const static std::string pattern = " \t\r\n";
        str = TrimString(str, pattern);
        if (!str.empty()) {
            if (*str.begin() == '[' && *str.rbegin() == ']') {
                const std::string section = str.substr(1, str.size() - 2);
                sections.insert(section);
                prefix = section + '.';
            } else if (*str.begin() == '-') {
                error = strprintf("parse error on line %i: %s, options in configuration file must be specified without leading -", linenr, str);
                return false;
            } else if ((pos = str.find('=')) != std::string::npos) {
                std::string name = prefix + TrimString(str.substr(0, pos), pattern);
                std::string value = TrimString(str.substr(pos + 1), pattern);
                if (used_hash && name.find("rpcpassword") != std::string::npos) {
                    error = strprintf("parse error on line %i, using # in rpcpassword can be ambiguous and should be avoided", linenr);
                    return false;
                }
                options.emplace_back(name, value);
                if ((pos = name.rfind('.')) != std::string::npos) {
                    sections.insert(name.substr(0, pos));
                }
            } else {
                error = strprintf("parse error on line %i: %s", linenr, str);
                if (str.size() >= 2 && str.substr(0, 2) == "no") {
                    error += strprintf(", if you intended to specify a negated option, use %s=1 instead", str);
                }
                return false;
            }
        }
        ++linenr;
    }
    return true;
}

bool ArgsManager::ReadConfigStream(std::istream& stream, std::string& error, bool ignore_invalid_keys)
{
    LOCK(cs_args);
    std::vector<std::pair<std::string, std::string>> options;
    m_config_sections.clear();
    if (!GetConfigOptions(stream, error, options, m_config_sections)) {
        return false;
    }
    for (const std::pair<std::string, std::string>& option : options) {
        std::string strKey = std::string("-") + option.first;
        std::string strValue = option.second;

        if (InterpretNegatedOption(strKey, strValue)) {
            m_config_args[strKey].clear();
        } else {
            m_config_args[strKey].push_back(strValue);
        }

//检查arg是否已知
        if (!IsArgKnown(strKey)) {
            if (!ignore_invalid_keys) {
                error = strprintf("Invalid configuration value %s", option.first.c_str());
                return false;
            } else {
                LogPrintf("Ignoring unknown configuration value %s\n", option.first);
            }
        }
    }
    return true;
}

bool ArgsManager::ReadConfigFiles(std::string& error, bool ignore_invalid_keys)
{
    {
        LOCK(cs_args);
        m_config_args.clear();
    }

    const std::string confPath = GetArg("-conf", BITCOIN_CONF_FILENAME);
    fsbridge::ifstream stream(GetConfigFile(confPath));

//确定没有配置文件
    if (stream.good()) {
        if (!ReadConfigStream(stream, error, ignore_invalid_keys)) {
            return false;
        }
//如果override参数中有-includeconf，但它为空，则表示用户
//在命令行上传递了“-noincludeconf”，在这种情况下，我们不应该包含任何内容
        bool emptyIncludeConf;
        {
            LOCK(cs_args);
            emptyIncludeConf = m_override_args.count("-includeconf") == 0;
        }
        if (emptyIncludeConf) {
            std::string chain_id = GetChainName();
            std::vector<std::string> includeconf(GetArgs("-includeconf"));
            {
//我们还没有设置m_网络（这在selectparams（）中发生），所以手动检查
//用于network.includeconf参数。
                std::vector<std::string> includeconf_net(GetArgs(std::string("-") + chain_id + ".includeconf"));
                includeconf.insert(includeconf.end(), includeconf_net.begin(), includeconf_net.end());
            }

//从配置中移除-includeconf，这样我们就可以警告递归
//后来
            {
                LOCK(cs_args);
                m_config_args.erase("-includeconf");
                m_config_args.erase(std::string("-") + chain_id + ".includeconf");
            }

            for (const std::string& to_include : includeconf) {
                fsbridge::ifstream include_config(GetConfigFile(to_include));
                if (include_config.good()) {
                    if (!ReadConfigStream(include_config, error, ignore_invalid_keys)) {
                        return false;
                    }
                    LogPrintf("Included configuration file %s\n", to_include.c_str());
                } else {
                    error = "Failed to include configuration file " + to_include;
                    return false;
                }
            }

//递归警告-includeconf
            includeconf = GetArgs("-includeconf");
            {
                std::vector<std::string> includeconf_net(GetArgs(std::string("-") + chain_id + ".includeconf"));
                includeconf.insert(includeconf.end(), includeconf_net.begin(), includeconf_net.end());
                std::string chain_id_final = GetChainName();
                if (chain_id_final != chain_id) {
//还警告在其中一个includeconfs中指定的链的递归includeconf
                    includeconf_net = GetArgs(std::string("-") + chain_id_final + ".includeconf");
                    includeconf.insert(includeconf.end(), includeconf_net.begin(), includeconf_net.end());
                }
            }
            for (const std::string& to_include : includeconf) {
                fprintf(stderr, "warning: -includeconf cannot be used from included files; ignoring -includeconf=%s\n", to_include.c_str());
            }
        }
    }

//如果在.conf文件中更改了datadir：
    ClearDatadirCache();
    if (!fs::is_directory(GetDataDir(false))) {
        error = strprintf("specified data directory \"%s\" does not exist.", gArgs.GetArg("-datadir", "").c_str());
        return false;
    }
    return true;
}

std::string ArgsManager::GetChainName() const
{
    LOCK(cs_args);
    bool fRegTest = ArgsManagerHelper::GetNetBoolArg(*this, "-regtest");
    bool fTestNet = ArgsManagerHelper::GetNetBoolArg(*this, "-testnet");

    if (fTestNet && fRegTest)
        throw std::runtime_error("Invalid combination of -regtest and -testnet.");
    if (fRegTest)
        return CBaseChainParams::REGTEST;
    if (fTestNet)
        return CBaseChainParams::TESTNET;
    return CBaseChainParams::MAIN;
}

#ifndef WIN32
fs::path GetPidFile()
{
    return AbsPathForConfigVal(fs::path(gArgs.GetArg("-pid", BITCOIN_PID_FILENAME)));
}

void CreatePidFile(const fs::path &path, pid_t pid)
{
    FILE* file = fsbridge::fopen(path, "w");
    if (file)
    {
        fprintf(file, "%d\n", pid);
        fclose(file);
    }
}
#endif

bool RenameOver(fs::path src, fs::path dest)
{
#ifdef WIN32
    return MoveFileExW(src.wstring().c_str(), dest.wstring().c_str(),
                       MOVEFILE_REPLACE_EXISTING) != 0;
#else
    int rc = std::rename(src.string().c_str(), dest.string().c_str());
    return (rc == 0);
/*DIF/*Win32＊
}

/**
 *忽略boost的create_目录（如果请求的目录存在）引发的异常。
 *专门处理路径p存在的情况，但用户不可能
 *写入父目录。
 **/

bool TryCreateDirectories(const fs::path& p)
{
    try
    {
        return fs::create_directories(p);
    } catch (const fs::filesystem_error&) {
        if (!fs::exists(p) || !fs::is_directory(p))
            throw;
    }

//创建目录没有创建目录，它必须已经存在
    return false;
}

bool FileCommit(FILE *file)
{
if (fflush(file) != 0) { //如果重复调用，则无害
        LogPrintf("%s: fflush failed: %d\n", __func__, errno);
        return false;
    }
#ifdef WIN32
    HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(file));
    if (FlushFileBuffers(hFile) == 0) {
        LogPrintf("%s: FlushFileBuffers failed: %d\n", __func__, GetLastError());
        return false;
    }
#else
    #if defined(__linux__) || defined(__NetBSD__)
if (fdatasync(fileno(file)) != 0 && errno != EINVAL) { //忽略不支持同步的文件系统的einval
        LogPrintf("%s: fdatasync failed: %d\n", __func__, errno);
        return false;
    }
    #elif defined(MAC_OSX) && defined(F_FULLFSYNC)
if (fcntl(fileno(file), F_FULLFSYNC, 0) == -1) { //手册页显示成功时返回“非-1的值”
        LogPrintf("%s: fcntl F_FULLFSYNC failed: %d\n", __func__, errno);
        return false;
    }
    #else
    if (fsync(fileno(file)) != 0 && errno != EINVAL) {
        LogPrintf("%s: fsync failed: %d\n", __func__, errno);
        return false;
    }
    #endif
#endif
    return true;
}

bool TruncateFile(FILE *file, unsigned int length) {
#if defined(WIN32)
    return _chsize(_fileno(file), length) == 0;
#else
    return ftruncate(fileno(file), length) == 0;
#endif
}

/*
 *此函数试图将文件描述符限制提高到请求的数目。
 *返回实际文件描述符限制（可能大于或小于nminfd）
 **/

int RaiseFileDescriptorLimit(int nMinFD) {
#if defined(WIN32)
    return 2048;
#else
    struct rlimit limitFD;
    if (getrlimit(RLIMIT_NOFILE, &limitFD) != -1) {
        if (limitFD.rlim_cur < (rlim_t)nMinFD) {
            limitFD.rlim_cur = nMinFD;
            if (limitFD.rlim_cur > limitFD.rlim_max)
                limitFD.rlim_cur = limitFD.rlim_max;
            setrlimit(RLIMIT_NOFILE, &limitFD);
            getrlimit(RLIMIT_NOFILE, &limitFD);
        }
        return limitFD.rlim_cur;
    }
return nMinFD; //getrlimit失败，假设没问题
#endif
}

/*
 *此函数试图使分配的文件的特定范围（对应于磁盘空间）
 *它是建议性的，参数中指定的范围永远不会包含实时数据。
 **/

void AllocateFileRange(FILE *file, unsigned int offset, unsigned int length) {
#if defined(WIN32)
//Windows特定版本
    HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(file));
    LARGE_INTEGER nFileSize;
    int64_t nEndPos = (int64_t)offset + length;
    nFileSize.u.LowPart = nEndPos & 0xFFFFFFFF;
    nFileSize.u.HighPart = nEndPos >> 32;
    SetFilePointerEx(hFile, nFileSize, 0, FILE_BEGIN);
    SetEndOfFile(hFile);
#elif defined(MAC_OSX)
//OSX特定版本
    fstore_t fst;
    fst.fst_flags = F_ALLOCATECONTIG;
    fst.fst_posmode = F_PEOFPOSMODE;
    fst.fst_offset = 0;
    fst.fst_length = (off_t)offset + length;
    fst.fst_bytesalloc = 0;
    if (fcntl(fileno(file), F_PREALLOCATE, &fst) == -1) {
        fst.fst_flags = F_ALLOCATEALL;
        fcntl(fileno(file), F_PREALLOCATE, &fst);
    }
    ftruncate(fileno(file), fst.fst_length);
#elif defined(__linux__)
//使用posix fallocate的版本
    off_t nEndPos = (off_t)offset + length;
    posix_fallocate(fileno(file), 0, nEndPos);
#else
//回退版本
//TODO:每个块只写一个字节
    static const char buf[65536] = {};
    if (fseek(file, offset, SEEK_SET)) {
        return;
    }
    while (length > 0) {
        unsigned int now = 65536;
        if (length < now)
            now = length;
fwrite(buf, 1, now, file); //允许失败；此函数无论如何都是建议函数
        length -= now;
    }
#endif
}

#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate)
{
    WCHAR pszPath[MAX_PATH] = L"";

    if(SHGetSpecialFolderPathW(nullptr, pszPath, nFolder, fCreate))
    {
        return fs::path(pszPath);
    }

    LogPrintf("SHGetSpecialFolderPathW() failed, could not obtain requested path.\n");
    return fs::path("");
}
#endif

void runCommand(const std::string& strCommand)
{
    if (strCommand.empty()) return;
#ifndef WIN32
    int nErr = ::system(strCommand.c_str());
#else
    int nErr = ::_wsystem(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>,wchar_t>().from_bytes(strCommand).c_str());
#endif
    if (nErr)
        LogPrintf("runCommand error: system(%s) returned %d\n", strCommand, nErr);
}

void RenameThread(const char* name)
{
#if defined(PR_SET_NAME)
//仅使用前15个字符（16-nul终止符）
    ::prctl(PR_SET_NAME, name, 0, 0, 0);
#elif (defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
    pthread_set_name_np(pthread_self(), name);

#elif defined(MAC_OSX)
    pthread_setname_np(name);
#else
//防止对未使用的参数发出警告…
    (void)name;
#endif
}

void SetupEnvironment()
{
#ifdef HAVE_MALLOPT_ARENA_MAX
//glibc-specific：在32位系统上，将arenas的数目设置为1。
//默认情况下，由于glibc 2.10，C库将创建最多两个堆
//砂芯。这会导致虚拟地址空间过大
//在我们的使用中的用法。通过设置
//竞技场到1号。
    if (sizeof(void*) == 4) {
        mallopt(M_ARENA_MAX, 1);
    }
#endif
//在大多数POSIX系统（如Linux，但不是BSD）上，环境的区域设置
//可能无效，在这种情况下，“c”区域设置用作回退。
#if !defined(WIN32) && !defined(MAC_OSX) && !defined(__FreeBSD__) && !defined(__OpenBSD__)
    try {
std::locale(""); //如果当前区域设置无效，则引发运行时错误
    } catch (const std::runtime_error&) {
        setenv("LC_ALL", "C", 1);
    }
#elif defined(WIN32)
//设置默认输入/输出字符集为utf-8
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
//路径区域设置被延迟初始化，以避免取消初始化错误。
//在多线程环境中，它是由主线程显式设置的。
//虚拟区域设置用于提取内部默认区域设置，由
//fs：：path，用于显式嵌入路径。
    std::locale loc = fs::path::imbue(std::locale::classic());
#ifndef WIN32
    fs::path::imbue(loc);
#else
    fs::path::imbue(std::locale(loc, new std::codecvt_utf8_utf16<wchar_t>()));
#endif
}

bool SetupNetworking()
{
#ifdef WIN32
//初始化Windows套接字
    WSADATA wsadata;
    int ret = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (ret != NO_ERROR || LOBYTE(wsadata.wVersion ) != 2 || HIBYTE(wsadata.wVersion) != 2)
        return false;
#endif
    return true;
}

int GetNumCores()
{
    return std::thread::hardware_concurrency();
}

std::string CopyrightHolders(const std::string& strPrefix)
{
    std::string strCopyrightHolders = strPrefix + strprintf(_(COPYRIGHT_HOLDERS), _(COPYRIGHT_HOLDERS_SUBSTITUTION));

//检查未翻译的替换，确保比特币核心版权不会被意外删除。
    if (strprintf(COPYRIGHT_HOLDERS, COPYRIGHT_HOLDERS_SUBSTITUTION).find("Bitcoin Core") == std::string::npos) {
        strCopyrightHolders += "\n" + strPrefix + "The Bitcoin Core developers";
    }
    return strCopyrightHolders;
}

//获取应用程序启动时间（用于运行时间计算）
int64_t GetStartupTime()
{
    return nStartupTime;
}

fs::path AbsPathForConfigVal(const fs::path& path, bool net_specific)
{
    return fs::absolute(path, GetDataDir(net_specific));
}

int ScheduleBatchPriority()
{
#ifdef SCHED_BATCH
    const static sched_param param{};
    if (int ret = pthread_setschedparam(pthread_self(), SCHED_BATCH, &param)) {
        LogPrintf("Failed to pthread_setschedparam: %s\n", strerror(errno));
        return ret;
    }
    return 0;
#else
    return 1;
#endif
}

namespace util {
#ifdef WIN32
WinCmdLineArgs::WinCmdLineArgs()
{
    wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_cvt;
    argv = new char*[argc];
    args.resize(argc);
    for (int i = 0; i < argc; i++) {
        args[i] = utf8_cvt.to_bytes(wargv[i]);
        argv[i] = &*args[i].begin();
    }
    LocalFree(wargv);
}

WinCmdLineArgs::~WinCmdLineArgs()
{
    delete[] argv;
}

std::pair<int, char**> WinCmdLineArgs::get()
{
    return std::make_pair(argc, argv);
}
#endif
} //命名空间使用
