
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

#ifndef BITCOIN_LOGGING_H
#define BITCOIN_LOGGING_H

#include <fs.h>
#include <tinyformat.h>

#include <atomic>
#include <cstdint>
#include <list>
#include <mutex>
#include <string>
#include <vector>

static const bool DEFAULT_LOGTIMEMICROS = false;
static const bool DEFAULT_LOGIPS        = false;
static const bool DEFAULT_LOGTIMESTAMPS = true;
extern const char * const DEFAULT_DEBUGLOGFILE;

extern bool fLogIPs;

struct CLogCategoryActive
{
    std::string category;
    bool active;
};

namespace BCLog {
    enum LogFlags : uint32_t {
        NONE        = 0,
        NET         = (1 <<  0),
        TOR         = (1 <<  1),
        MEMPOOL     = (1 <<  2),
        HTTP        = (1 <<  3),
        BENCH       = (1 <<  4),
        ZMQ         = (1 <<  5),
        DB          = (1 <<  6),
        RPC         = (1 <<  7),
        ESTIMATEFEE = (1 <<  8),
        ADDRMAN     = (1 <<  9),
        SELECTCOINS = (1 << 10),
        REINDEX     = (1 << 11),
        CMPCTBLOCK  = (1 << 12),
        RAND        = (1 << 13),
        PRUNE       = (1 << 14),
        PROXY       = (1 << 15),
        MEMPOOLREJ  = (1 << 16),
        LIBEVENT    = (1 << 17),
        COINDB      = (1 << 18),
        QT          = (1 << 19),
        LEVELDB     = (1 << 20),
        ALL         = ~(uint32_t)0,
    };

    class Logger
    {
    private:
        FILE* m_fileout = nullptr;
        std::mutex m_file_mutex;
        std::list<std::string> m_msgs_before_open;

        /*
         *m_started_new_line是一个状态变量，它将禁止打印
         *进行多个不以
         *换行符。
         **/

        std::atomic_bool m_started_new_line{true};

        /*日志类别位字段。*/
        std::atomic<uint32_t> m_categories{0};

        std::string LogTimestampStr(const std::string& str);

    public:
        bool m_print_to_console = false;
        bool m_print_to_file = false;

        bool m_log_timestamps = DEFAULT_LOGTIMESTAMPS;
        bool m_log_time_micros = DEFAULT_LOGTIMEMICROS;

        fs::path m_file_path;
        std::atomic<bool> m_reopen_file{false};

        /*向日志输出发送字符串*/
        void LogPrintStr(const std::string &str);

        /*返回是否将日志写入任何输出*/
        bool Enabled() const { return m_print_to_console || m_print_to_file; }

        bool OpenDebugLog();
        void ShrinkDebugFile();

        uint32_t GetCategoryMask() const { return m_categories.load(); }

        void EnableCategory(LogFlags flag);
        bool EnableCategory(const std::string& str);
        void DisableCategory(LogFlags flag);
        bool DisableCategory(const std::string& str);

        bool WillLogCategory(LogFlags category) const;

        bool DefaultShrinkDebugFile() const;
    };

} //名称空间BCUG

extern BCLog::Logger* const g_logger;

/*如果日志接受指定的类别，则返回true*/
static inline bool LogAcceptCategory(BCLog::LogFlags category)
{
    return g_logger->WillLogCategory(category);
}

/*返回具有日志类别的字符串。*/
std::string ListLogCategories();

/*返回活动日志类别的向量。*/
std::vector<CLogCategoryActive> ListActiveLogCategories();

/*如果str解析为日志类别并设置标志，则返回true*/
bool GetLogCategory(BCLog::LogFlags& flag, const std::string& str);

//使用logprintf/error或其他
//无条件登录debug.log！它不应该是这样的情况，一个入站
//对等机可以用debug.log条目填充用户磁盘。

template <typename... Args>
static inline void LogPrintf(const char* fmt, const Args&... args)
{
    if (g_logger->Enabled()) {
        std::string log_msg;
        try {
            log_msg = tfm::format(fmt, args...);
        } catch (tinyformat::format_error& fmterr) {
            /*原始格式字符串将有换行符，因此不要在此处添加换行符*/
            log_msg = "Error \"" + std::string(fmterr.what()) + "\" while formatting log message: " + fmt;
        }
        g_logger->LogPrintStr(log_msg);
    }
}

template <typename... Args>
static inline void LogPrint(const BCLog::LogFlags& category, const Args&... args)
{
    if (LogAcceptCategory((category))) {
        LogPrintf(args...);
    }
}

#endif //比特币记录
