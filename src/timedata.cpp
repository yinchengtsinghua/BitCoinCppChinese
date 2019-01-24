
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2014-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <timedata.h>

#include <netaddress.h>
#include <sync.h>
#include <ui_interface.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <warnings.h>


static CCriticalSection cs_nTimeOffset;
static int64_t nTimeOffset GUARDED_BY(cs_nTimeOffset) = 0;

/*
 *“千万不要带着两个计时器出海；带上一两个。”
 *我们的三个时间来源是：
 *系统时钟
 *其他节点时钟的中位数
 *用户（如果前两个不一致，请用户修复系统时钟）
 **/

int64_t GetTimeOffset()
{
    LOCK(cs_nTimeOffset);
    return nTimeOffset;
}

int64_t GetAdjustedTime()
{
    return GetTime() + GetTimeOffset();
}

static int64_t abs64(int64_t n)
{
    return (n >= 0 ? n : -n);
}

#define BITCOIN_TIMEDATA_MAX_SAMPLES 200

void AddTimeData(const CNetAddr& ip, int64_t nOffsetSample)
{
    LOCK(cs_nTimeOffset);
//忽略重复项
    static std::set<CNetAddr> setKnown;
    if (setKnown.size() == BITCOIN_TIMEDATA_MAX_SAMPLES)
        return;
    if (!setKnown.insert(ip).second)
        return;

//添加数据
    static CMedianFilter<int64_t> vTimeOffsets(BITCOIN_TIMEDATA_MAX_SAMPLES, 0);
    vTimeOffsets.input(nOffsetSample);
    LogPrint(BCLog::NET,"added time data, samples %d, offset %+d (%+d minutes)\n", vTimeOffsets.size(), nOffsetSample, nOffsetSample/60);

//这里有一个已知问题（见问题4521）：
//
//-结构vTimeOffsets包含多达200个元素，之后
//添加到它的任何新元素都不会增加其大小，从而替换
//最古老元素。
//
//-更新ntimeoffset的条件包括检查
//vTimeOffsets中的元素数目是奇数，在
//有200个元素。
//
//但在这种情况下，“bug”可以抵御一些攻击，并且可能
//事实上，解释为什么我们从未见过操纵
//时钟偏移。
//
//所以我们应该推迟修理这个，把它作为
//定时清理，以许多其他方式强化它。
//
    if (vTimeOffsets.size() >= 5 && vTimeOffsets.size() % 2 == 1)
    {
        int64_t nMedian = vTimeOffsets.median();
        std::vector<int64_t> vSorted = vTimeOffsets.sorted();
//只让其他节点改变我们的时间这么多
        if (abs64(nMedian) <= std::max<int64_t>(0, gArgs.GetArg("-maxtimeadjustment", DEFAULT_MAX_TIME_ADJUSTMENT)))
        {
            nTimeOffset = nMedian;
        }
        else
        {
            nTimeOffset = 0;

            static bool fDone;
            if (!fDone)
            {
//如果没有人的时间与我们的时间不同，但在我们的5分钟内，发出警告
                bool fMatch = false;
                for (const int64_t nOffset : vSorted)
                    if (nOffset != 0 && abs64(nOffset) < 5 * 60)
                        fMatch = true;

                if (!fMatch)
                {
                    fDone = true;
                    std::string strMessage = strprintf(_("Please check that your computer's date and time are correct! If your clock is wrong, %s will not work properly."), _(PACKAGE_NAME));
                    SetMiscWarning(strMessage);
                    uiInterface.ThreadSafeMessageBox(strMessage, "", CClientUIInterface::MSG_WARNING);
                }
            }
        }

        if (LogAcceptCategory(BCLog::NET)) {
            for (const int64_t n : vSorted) {
                /*打印（bclog:：net，“%+d”，n）；/*续*/
            }
            logprint（bclog:：net，“”）；/*续*/


            LogPrint(BCLog::NET, "nTimeOffset = %+d  (%+d minutes)\n", nTimeOffset, nTimeOffset/60);
        }
    }
}
