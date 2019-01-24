
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

#include <threadinterrupt.h>

#include <sync.h>

CThreadInterrupt::CThreadInterrupt() : flag(false) {}

CThreadInterrupt::operator bool() const
{
    return flag.load(std::memory_order_acquire);
}

void CThreadInterrupt::reset()
{
    flag.store(false, std::memory_order_release);
}

void CThreadInterrupt::operator()()
{
    {
        LOCK(mut);
        flag.store(true, std::memory_order_release);
    }
    cond.notify_all();
}

bool CThreadInterrupt::sleep_for(std::chrono::milliseconds rel_time)
{
    WAIT_LOCK(mut, lock);
    return !cond.wait_for(lock, rel_time, [this]() { return flag.load(std::memory_order_acquire); });
}

bool CThreadInterrupt::sleep_for(std::chrono::seconds rel_time)
{
    return sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(rel_time));
}

bool CThreadInterrupt::sleep_for(std::chrono::minutes rel_time)
{
    return sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(rel_time));
}
