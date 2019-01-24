
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <sync.h>

#include <logging.h>
#include <util/strencodings.h>

#include <stdio.h>

#include <map>
#include <memory>
#include <set>

#ifdef DEBUG_LOCKCONTENTION
#if !defined(HAVE_THREAD_LOCAL)
static_assert(false, "thread_local is not supported");
#endif
void PrintLockContention(const char* pszName, const char* pszFile, int nLine)
{
    LogPrintf("LOCKCONTENTION: %s\n", pszName);
    LogPrintf("Locker: %s:%d\n", pszFile, nLine);
}
/*DIF/*调试锁定竞争*/

ifdef调试锁定顺序
/ /
//早期死锁检测。
//正在解决的问题：
//线程1锁定A、B、C
//线程2锁定d，然后锁定c，然后锁定a
//->可能导致两个线程之间死锁，具体取决于它们运行的时间。
//此处实现的解决方案：
//跟踪成对的锁：（A在B之前），（A在C之前）等。
//如果有线程试图以不同的顺序锁定，则会发出投诉。
/ /

结构时钟位置
    时钟位置（const char*pszname，const char*pszfile，int nline，bool ftryin）
    {
        mutexname=pszname；
        sourcefile=pszfile；
        sourceline=nline；
        FTRT＝FTRIN；
    }

    std:：string tostring（）常量
    {
        返回mutexname+“”+sourcefile+“：”+itostr（sourceline）+（ftry？“（尝试）“：”“”；
    }

私人：
    布尔；
    std：：字符串mutexname；
    std：：字符串源文件；
    In源代码；
}；

typedef std:：vector<std:：pair<void*，clocklocation>>锁堆栈；
typedef std:：map<std:：pair<void*，void*>，lockstack>lockorders；
typedef std:：set<std:：pair<void*，void*>>invLockOrders；

结构锁数据
    //非常难看的黑客：当全局构造和析构函数运行单个
    //线程化，我们使用这个布尔值来知道lockdata是否仍然存在，
    //因为DeleteLock可以由全局CcriticalSection析构函数调用
    //在lockdata消失后。
    可用；
    lockdata（）：可用（真）
    ~lockdata（）available=false；

    锁定命令锁定命令；
    库存订单库存订单；
    std：：mutex dd_mutex；
静态锁数据；

静态线程本地锁栈g锁栈；

检测到静态void潜在的死锁（const std:：pair<void*，void*>&mismatch，const lockstack&s1，const lockstack&s2）
{
    logprintf（“检测到潜在死锁\n”）；
    logprintf（“以前的锁定顺序是：\n”）；
    for（const std:：pair<void*，clocklocation>&i:s2）
        如果（i.first==mismatch.first）
            logprintf（“（1）”）；/*续*/

        }
        if (i.first == mismatch.second) {
            /*printf（“（2）”）；/*续*/
        }
        logprintf（“%s\n”，i.second.toString（））；
    }
    logprintf（“当前锁定顺序为：\n”）；
    对于（const std:：pair<void*，clocklocation>&i:s1）
        如果（i.first==mismatch.first）
            logprintf（“（1）”）；/*续*/

        }
        if (i.first == mismatch.second) {
            /*printf（“（2）”）；/*续*/
        }
        logprintf（“%s\n”，i.second.toString（））；
    }
    如果（G_debug_lockorder_abort）
        fprintf（stderr，“断言失败：在%s处检测到不一致的锁顺序：%i，详细信息在调试日志中。\n”，\u文件\uuuuuuuu行\）；
        中止（）；
    }
    throw std:：logic_error（“检测到潜在死锁”）；
}

静态空隙推送锁定（空隙*C，常量时钟位置和锁定位置）
{
    std:：lock_guard<std:：mutex>lock（lockdata.dd_mutex）；

    g_lockstack.push_back（std:：make_pair（c，locklocation））；

    for（const std:：pair<void*，clocklocation>&i:g_lockstack）
        如果（i.first==c）
            断裂；

        std:：pair<void*，void*>p1=std:：make_pair（i.first，c）；
        if（lockdata.lockorders.count（p1））。
            继续；
        lockdata.lockorders[p1]=g_锁栈；

        std:：pair<void*，void*>p2=std:：make_pair（c，i.first）；
        lockdata.invlockorders.insert（p2）；
        if（lockdata.lockorders.count（p2））。
            检测到潜在的_死锁_（p1，lockdata.lockorders[p2]，lockdata.lockorders[p1]）；
    }
}

静态void pop_lock（）
{
    g_lockstack.pop_back（）；
}

void EnterCritical（const char*pszname，const char*pszfile，int nline，void*cs，bool ftry）
{
    推锁（cs，时钟位置（pszname，pszfile，nline，ftry））；
}

void leavecritical（）。
{
    POPYROCK（）；
}

std：：字符串locksheld（））
{
    std：：字符串结果；
    for（const std:：pair<void*，clocklocation>和i:g_lockstack）
        result+=i.second.toString（）+std:：string（“\n”）；
    返回结果；
}

void断言lockheldinternal（const char*pszname，const char*pszfile，int nline，void*cs）
{
    for（const std:：pair<void*，clocklocation>和i:g_lockstack）
        如果（i.first==cs）
            返回；
    fprintf（stderr，“断言失败：锁%s未保留在%s中：%i；保留的锁：\n%s”，pszname，pszfile，nline，lock s held（）.c_str（））；
    中止（）；
}

void assertlocknotheldinternal（const char*pszname，const char*pszfile，int nline，void*cs）
{
    for（const std:：pair<void*，clocklocation>&i:g_lockstack）
        如果（i.first==cs）
            fprintf（stderr，“断言失败：锁%s保留在%s中：%i；保留的锁：\n%s”，pszname，pszfile，nline，lock s held（）.c_str（））；
            中止（）；
        }
    }
}

void删除锁定（void*cs）
{
    如果（！）LockData.可用）
        //我们已经关闭了。
        返回；
    }
    std:：lock_guard<std:：mutex>lock（lockdata.dd_mutex）；
    std:：pair<void*，void*>item=std:：make_pair（cs，nullptr）；
    lockorders:：iterator it=lockdata.lockorders.lower_bound（item）；
    尽管如此！=lockdata.lockorders.end（）&&it->first.first==cs）
        std:：pair<void*，void*>invitem=std:：make_pair（it->first.second，it->first.first）；
        lockdata.invlockorders.erase（invitem）删除；
        lockdata.lockorders.erase（it++）；
    }
    invLockOrders：：迭代器invit=lockdata.invLockOrders.lower_bound（item）；
    趁着！=lockdata.invlockorders.end（）&&invit->first==cs）
        std:：pair<void*，void*>invitem=std:：make_pair（invit->second，invit->first）；
        lockdata.lockorders.erase（invinvitem）；
        lockdata.invlockorders.erase（invit++）；
    }
}

bool g_debug_lockorder_abort=true；

endif/*调试锁定顺序*/

