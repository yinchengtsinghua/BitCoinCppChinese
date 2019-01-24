
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

/*
 *服务器/客户端环境：参数处理、配置文件解析，
 *线程包装器，启动时间
 **/

#ifndef BITCOIN_UTIL_SYSTEM_H
#define BITCOIN_UTIL_SYSTEM_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <attributes.h>
#include <compat.h>
#include <fs.h>
#include <logging.h>
#include <sync.h>
#include <tinyformat.h>
#include <util/memory.h>
#include <util/time.h>

#include <atomic>
#include <exception>
#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <boost/thread/condition_variable.hpp> //对于boost：：线程中断

//应用程序启动时间（用于运行时间计算）
int64_t GetStartupTime();

extern const char * const BITCOIN_CONF_FILENAME;
extern const char * const BITCOIN_PID_FILENAME;

/*将消息翻译为用户的母语。*/
const extern std::function<std::string(const char*)> G_TRANSLATION_FUN;

/*
 *翻译功能。
 *如果没有设置翻译功能，只需返回输入。
 **/

inline std::string _(const char* psz)
{
    return G_TRANSLATION_FUN ? (G_TRANSLATION_FUN)(psz) : psz;
}

void SetupEnvironment();
bool SetupNetworking();

template<typename... Args>
bool error(const char* fmt, const Args&... args)
{
    LogPrintf("ERROR: %s\n", tfm::format(fmt, args...));
    return false;
}

void PrintExceptionContinue(const std::exception *pex, const char* pszThread);
bool FileCommit(FILE *file);
bool TruncateFile(FILE *file, unsigned int length);
int RaiseFileDescriptorLimit(int nMinFD);
void AllocateFileRange(FILE *file, unsigned int offset, unsigned int length);
bool RenameOver(fs::path src, fs::path dest);
bool LockDirectory(const fs::path& directory, const std::string lockfile_name, bool probe_only=false);
bool DirIsWritable(const fs::path& directory);

/*释放所有目录锁。这仅用于运行时的单元测试
 *全局析构函数将处理锁。
 **/

void ReleaseDirectoryLocks();

bool TryCreateDirectories(const fs::path& p);
fs::path GetDefaultDataDir();
//blocks目录始终是网络特定的。
const fs::path &GetBlocksDir();
const fs::path &GetDataDir(bool fNetSpecific = true);
void ClearDatadirCache();
fs::path GetConfigFile(const std::string& confPath);
#ifndef WIN32
fs::path GetPidFile();
void CreatePidFile(const fs::path &path, pid_t pid);
#endif
#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate = true);
#endif
void runCommand(const std::string& strCommand);

/*
 *大多数作为配置参数传递的路径都被视为相对于
 *如果不是绝对的，则为datadir。
 *
 *@param path要以datadir作为条件前缀的路径。
 *@param net_-specific转发到getdatadir（）。
 *@返回规范化路径。
 **/

fs::path AbsPathForConfigVal(const fs::path& path, bool net_specific = true);

inline bool IsSwitchChar(char c)
{
#ifdef WIN32
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}

enum class OptionsCategory {
    OPTIONS,
    CONNECTION,
    WALLET,
    WALLET_DEBUG_TEST,
    ZMQ,
    DEBUG_TEST,
    CHAINPARAMS,
    NODE_RELAY,
    BLOCK_CREATION,
    RPC,
    GUI,
    COMMANDS,
    REGISTER_COMMANDS,

HIDDEN //始终是避免在帮助中打印这些内容的最后一个选项
};

class ArgsManager
{
protected:
    friend class ArgsManagerHelper;

    struct Arg
    {
        std::string m_help_param;
        std::string m_help_text;
        bool m_debug_only;

        Arg(const std::string& help_param, const std::string& help_text, bool debug_only) : m_help_param(help_param), m_help_text(help_text), m_debug_only(debug_only) {};
    };

    mutable CCriticalSection cs_args;
    std::map<std::string, std::vector<std::string>> m_override_args GUARDED_BY(cs_args);
    std::map<std::string, std::vector<std::string>> m_config_args GUARDED_BY(cs_args);
    std::string m_network GUARDED_BY(cs_args);
    std::set<std::string> m_network_only_args GUARDED_BY(cs_args);
    std::map<OptionsCategory, std::map<std::string, Arg>> m_available_args GUARDED_BY(cs_args);
    std::set<std::string> m_config_sections GUARDED_BY(cs_args);

    NODISCARD bool ReadConfigStream(std::istream& stream, std::string& error, bool ignore_invalid_keys = false);

public:
    ArgsManager();

    /*
     *选择正在使用的网络
     **/

    void SelectConfigNetwork(const std::string& network);

    NODISCARD bool ParseParameters(int argc, const char* const argv[], std::string& error);
    NODISCARD bool ReadConfigFiles(std::string& error, bool ignore_invalid_keys = false);

    /*
     *仅在以下情况下记录m_节\中选项的警告：
     *它们在默认部分中指定，但不被重写。
     *在命令行或网络特定部分
     *配置文件。
     **/

    const std::set<std::string> GetUnsuitableSectionOnlyArgs() const;

    /*
     *在配置文件中记录无法识别的节名的警告。
     **/

    const std::set<std::string> GetUnrecognizedSections() const;

    /*
     *返回给定参数的字符串向量
     *
     *@param strarg要获取的参数（例如“-foo”）
     *@返回命令行参数
     **/

    std::vector<std::string> GetArgs(const std::string& strArg) const;

    /*
     *如果已手动设置给定参数，则返回true。
     *
     *@param strarg要获取的参数（例如“-foo”）
     *@如果参数已设置，则返回true
     **/

    bool IsArgSet(const std::string& strArg) const;

    /*
     *如果参数最初作为否定选项传递，则返回true，
     *即-NOFOO。
     *
     *@param strarg要获取的参数（例如“-foo”）
     如果参数被否定，则返回true
     **/

    bool IsArgNegated(const std::string& strArg) const;

    /*
     *返回字符串参数或默认值
     *
     *@param strarg要获取的参数（例如“-foo”）
     *@param strdefault（例如“1”）。
     *@返回命令行参数或默认值
     **/

    std::string GetArg(const std::string& strArg, const std::string& strDefault) const;

    /*
     *返回整数参数或默认值
     *
     *@param strarg要获取的参数（例如“-foo”）
     *@参数ndefault（例如1）
     *@返回命令行参数（如果数字无效，则为0）或默认值
     **/

    int64_t GetArg(const std::string& strArg, int64_t nDefault) const;

    /*
     *返回布尔参数或默认值
     *
     *@param strarg要获取的参数（例如“-foo”）
     *@param fdefault（真或假）
     *@返回命令行参数或默认值
     **/

    bool GetBoolArg(const std::string& strArg, bool fDefault) const;

    /*
     *如果参数没有值，则设置该参数。
     *
     *@param strarg要设置的参数（例如“-foo”）
     *@param strvalue值（例如“1”）。
     *@如果参数已设置，则返回true；如果参数已设置值，则返回false
     **/

    bool SoftSetArg(const std::string& strArg, const std::string& strValue);

    /*
     *如果布尔参数还没有值，则设置该参数。
     *
     *@param strarg要设置的参数（例如“-foo”）
     *@param fvalue值（例如，false）
     *@如果参数已设置，则返回true；如果参数已设置值，则返回false
     **/

    bool SoftSetBoolArg(const std::string& strArg, bool fValue);

//强制参数设置。如果arg尚未调用，则由SoftSetArg（）调用
//被设定。在测试中也直接调用。
    void ForceSetArg(const std::string& strArg, const std::string& strValue);

    /*
     *查找-regtest、-testnet并返回相应的bip70链名称。
     *@return cbasechainparams:：main默认情况下；如果给定的组合无效，则会引发运行时错误。
     **/

    std::string GetChainName() const;

    /*
     *添加参数
     **/

    void AddArg(const std::string& name, const std::string& help, const bool debug_only, const OptionsCategory& cat);

    /*
     *添加许多隐藏参数
     **/

    void AddHiddenArgs(const std::vector<std::string>& args);

    /*
     *清除可用参数
     **/

    void ClearArgs() {
        LOCK(cs_args);
        m_available_args.clear();
    }

    /*
     *获取帮助字符串
     **/

    std::string GetHelpMessage() const;

    /*
     *检查我们是否知道这个参数
     **/

    bool IsArgKnown(const std::string& key) const;
};

extern ArgsManager gArgs;

/*
 *@如果通过命令行参数请求帮助，则返回true
 **/

bool HelpRequested(const ArgsManager& args);

/*
 *格式化一个字符串，用作帮助消息中的选项组
 *
 *@param message group name（例如“rpc server options:”）
 *@返回格式化字符串
 **/

std::string HelpMessageGroup(const std::string& message);

/*
 *设置在帮助消息中用作选项描述的字符串的格式
 *
 *@param选项消息（例如“-rpcuser=<user>”）
 *@param message选项说明（例如，“json-rpc连接的用户名”）。
 *@返回格式化字符串
 **/

std::string HelpMessageOpt(const std::string& option, const std::string& message);

/*
 *返回当前系统上可用的内核数。
 *@注意，这会计算虚拟核心，例如由超线程提供的核心。
 **/

int GetNumCores();

void RenameThread(const char* name);

/*
 *一个只调用一次func的包装器
 **/

template <typename Callable> void TraceThread(const char* name,  Callable func)
{
    std::string s = strprintf("bitcoin-%s", name);
    RenameThread(s.c_str());
    try
    {
        LogPrintf("%s thread start\n", name);
        func();
        LogPrintf("%s thread exit\n", name);
    }
    catch (const boost::thread_interrupted&)
    {
        LogPrintf("%s thread interrupt\n", name);
        throw;
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, name);
        throw;
    }
    catch (...) {
        PrintExceptionContinue(nullptr, name);
        throw;
    }
}

std::string CopyrightHolders(const std::string& strPrefix);

/*
 *在支持它的平台上，告诉内核调用线程是
 *CPU密集型和非交互式。详情见附表（7）中的附表批次。
 *
 *@返回sched_setschedule（）的返回值，或在没有
 *sched_setschedule（）。
 **/

int ScheduleBatchPriority();

namespace util {

//！标准插入的简化
template <typename Tdst, typename Tsrc>
inline void insert(Tdst& dst, const Tsrc& src) {
    dst.insert(dst.begin(), src.begin(), src.end());
}
template <typename TsetT, typename Tsrc>
inline void insert(std::set<TsetT>& dst, const Tsrc& src) {
    dst.insert(src.begin(), src.end());
}

#ifdef WIN32
class WinCmdLineArgs
{
public:
    WinCmdLineArgs();
    ~WinCmdLineArgs();
    std::pair<int, char**> get();

private:
    int argc;
    char** argv;
    std::vector<std::string> args;
};
#endif

} //命名空间使用

#endif //比特币使用系统
