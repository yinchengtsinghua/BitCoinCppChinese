
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

#include <policy/fees.h>
#include <policy/policy.h>

#include <clientversion.h>
#include <primitives/transaction.h>
#include <streams.h>
#include <txmempool.h>
#include <util/system.h>

static constexpr double INF_FEERATE = 1e99;

std::string StringForFeeEstimateHorizon(FeeEstimateHorizon horizon) {
    static const std::map<FeeEstimateHorizon, std::string> horizon_strings = {
        {FeeEstimateHorizon::SHORT_HALFLIFE, "short"},
        {FeeEstimateHorizon::MED_HALFLIFE, "medium"},
        {FeeEstimateHorizon::LONG_HALFLIFE, "long"},
    };
    auto horizon_string = horizon_strings.find(horizon);

    if (horizon_string == horizon_strings.end()) return "unknown";

    return horizon_string->second;
}

std::string StringForFeeReason(FeeReason reason) {
    static const std::map<FeeReason, std::string> fee_reason_strings = {
        {FeeReason::NONE, "None"},
        {FeeReason::HALF_ESTIMATE, "Half Target 60% Threshold"},
        {FeeReason::FULL_ESTIMATE, "Target 85% Threshold"},
        {FeeReason::DOUBLE_ESTIMATE, "Double Target 95% Threshold"},
        {FeeReason::CONSERVATIVE, "Conservative Double Target longer horizon"},
        {FeeReason::MEMPOOL_MIN, "Mempool Min Fee"},
        {FeeReason::PAYTXFEE, "PayTxFee set"},
        {FeeReason::FALLBACK, "Fallback fee"},
        {FeeReason::REQUIRED, "Minimum Required Fee"},
        {FeeReason::MAXTXFEE, "MaxTxFee limit"}
    };
    auto reason_string = fee_reason_strings.find(reason);

    if (reason_string == fee_reason_strings.end()) return "Unknown";

    return reason_string->second;
}

bool FeeModeFromString(const std::string& mode_string, FeeEstimateMode& fee_estimate_mode) {
    static const std::map<std::string, FeeEstimateMode> fee_modes = {
        {"UNSET", FeeEstimateMode::UNSET},
        {"ECONOMICAL", FeeEstimateMode::ECONOMICAL},
        {"CONSERVATIVE", FeeEstimateMode::CONSERVATIVE},
    };
    auto mode = fee_modes.find(mode_string);

    if (mode == fee_modes.end()) return false;

    fee_estimate_mode = mode->second;
    return true;
}

/*
 *我们将实例化此类的一个实例，以跟踪
 *包含在一个块中。我们将根据他们的
 *大概费恩特，然后跟踪这些TXS包含在一个块中需要多长时间。
 *
 *跟踪未确认（mempool）交易完全独立于
 *对已在块中确认的交易进行历史跟踪。
 **/

class TxConfirmStats
{
private:
//定义将事务分组到的存储桶
const std::vector<double>& buckets;              //桶范围的上限（包含）
const std::map<double, unsigned int>& bucketMap; //Bucket上界到Bucket索引到所有向量的映射

//对于每个桶x：
//计算每个桶中TXS的总数
//跟踪此总数在块上的历史移动平均值
    std::vector<double> txCtAvg;

//计算每个桶中Y块内确认的TXS总数。
//追踪这些总数的历史移动平均值。
std::vector<std::vector<double>> confAvg; //康瓦夫[y] [X]

//跟踪已从mempool中逐出的Tx的移动平均值
//在Y区块内未确认后
std::vector<std::vector<double>> failAvg; //Reavavg [y] [X]

//合计每个桶中所有Tx的总流量
//跟踪此总数在块上的历史移动平均值
    std::vector<double> avg;

//将conf计数与tx计数相结合，计算每个y，x的确认百分比。
//将总值与Tx计数结合起来计算每个桶的平均速度

    double decay;

//用于跟踪确认的解决方案（块）
    unsigned int scale;

//未完成交易的内存池计数
//对于每个bucket x，跟踪mempool中的事务数
//对每个可能的确认值Y未确认
std::vector<std::vector<int> > unconfTxs;  //未混淆的[y] [x]
//在每个bucket的getmaxconfirms之后，事务仍然未确认
    std::vector<int> oldUnconfTxs;

    void resizeInMemoryCounters(size_t newbuckets);

public:
    /*
     *创建新的txconfirmstats。这是由BlockPolicyEstimator调用的
     *具有默认值的构造函数。
     *@param defaultbuckets包含桶边界的上限
     *@param max periods要跟踪的最大周期数
     *@param decay每个块的历史移动平均值的衰减量
     **/

    TxConfirmStats(const std::vector<double>& defaultBuckets, const std::map<double, unsigned int>& defaultBucketMap,
                   unsigned int maxPeriods, double decay, unsigned int scale);

    /*为未确认的Tx滚动圆形缓冲区*/
    void ClearCurrent(unsigned int nBlockHeight);

    /*
     *在当前块状态中记录新的事务数据点
     *@param blocks确认此事务要确认的块数
     *@param val交易日期
     *@warning blockstoconfirm是基于1的，必须大于等于1
     **/

    void Record(int blocksToConfirm, double val);

    /*记录进入mempoo的新事务*/
    unsigned int NewTx(unsigned int nBlockHeight, double val);

    /*从mempool跟踪状态中删除事务*/
    void removeTx(unsigned int entryHeight, unsigned int nBestSeenHeight,
                  unsigned int bucketIndex, bool inBlock);

    /*通过降低历史移动平均值和更新
        从当前块收集的数据*/

    void UpdateMovingAverages();

    /*
     *计算一个估计值。找到最低值的桶（或桶的范围
     *确保我们有足够的数据点），其交易仍有足够的可能性
     *在目标确认数量内确认
     *@param conftarget目标确认次数
     *@param sufficienttxval在存储桶范围内每个块所需的平均事务数
     *@param minsuccess我们需要的成功概率
     *@param requiregreater返回最低频率，以便所有较高的值都能通过minsuccess或
     *返回最高值，使所有较低的值都无法成功访问。
     *@param nblockheight当前块高度
     **/

    double EstimateMedianVal(int confTarget, double sufficientTxVal,
                             double minSuccess, bool requireGreater, unsigned int nBlockHeight,
                             EstimationResult *result = nullptr) const;

    /*返回我们正在跟踪的最大确认数*/
    unsigned int GetMaxConfirms() const { return scale * confAvg.size(); }

    /*将估计数据的状态写入FIL*/
    void Write(CAutoFile& fileout) const;

    /*
     *从文件中读取估算数据的保存状态，并替换所有内部数据结构和
     *此状态的变量。
     **/

    void Read(CAutoFile& filein, int nFileVersion, size_t numBuckets);
};


TxConfirmStats::TxConfirmStats(const std::vector<double>& defaultBuckets,
                                const std::map<double, unsigned int>& defaultBucketMap,
                               unsigned int maxPeriods, double _decay, unsigned int _scale)
    : buckets(defaultBuckets), bucketMap(defaultBucketMap)
{
    decay = _decay;
    assert(_scale != 0 && "_scale must be non-zero");
    scale = _scale;
    confAvg.resize(maxPeriods);
    for (unsigned int i = 0; i < maxPeriods; i++) {
        confAvg[i].resize(buckets.size());
    }
    failAvg.resize(maxPeriods);
    for (unsigned int i = 0; i < maxPeriods; i++) {
        failAvg[i].resize(buckets.size());
    }

    txCtAvg.resize(buckets.size());
    avg.resize(buckets.size());

    resizeInMemoryCounters(buckets.size());
}

void TxConfirmStats::resizeInMemoryCounters(size_t newbuckets) {
//必须传入newbuckets，因为在读取期间引用的bucket尚未更新。
    unconfTxs.resize(GetMaxConfirms());
    for (unsigned int i = 0; i < unconfTxs.size(); i++) {
        unconfTxs[i].resize(newbuckets);
    }
    oldUnconfTxs.resize(newbuckets);
}

//滚动未确认的TXS循环缓冲区
void TxConfirmStats::ClearCurrent(unsigned int nBlockHeight)
{
    for (unsigned int j = 0; j < buckets.size(); j++) {
        oldUnconfTxs[j] += unconfTxs[nBlockHeight%unconfTxs.size()][j];
        unconfTxs[nBlockHeight%unconfTxs.size()][j] = 0;
    }
}


void TxConfirmStats::Record(int blocksToConfirm, double val)
{
//blockstoconfirm是基于1的
    if (blocksToConfirm < 1)
        return;
    int periodsToConfirm = (blocksToConfirm + scale - 1)/scale;
    unsigned int bucketindex = bucketMap.lower_bound(val)->second;
    for (size_t i = periodsToConfirm; i <= confAvg.size(); i++) {
        confAvg[i - 1][bucketindex]++;
    }
    txCtAvg[bucketindex]++;
    avg[bucketindex] += val;
}

void TxConfirmStats::UpdateMovingAverages()
{
    for (unsigned int j = 0; j < buckets.size(); j++) {
        for (unsigned int i = 0; i < confAvg.size(); i++)
            confAvg[i][j] = confAvg[i][j] * decay;
        for (unsigned int i = 0; i < failAvg.size(); i++)
            failAvg[i][j] = failAvg[i][j] * decay;
        avg[j] = avg[j] * decay;
        txCtAvg[j] = txCtAvg[j] * decay;
    }
}

//出错时返回-1
double TxConfirmStats::EstimateMedianVal(int confTarget, double sufficientTxVal,
                                         double successBreakPoint, bool requireGreater,
                                         unsigned int nBlockHeight, EstimationResult *result) const
{
//桶的计数器（或桶的范围）
double nConf = 0; //ConfTarget中确认的Tx数
double totalNum = 0; //已确认的Tx总数
int extraNum = 0;  //ConfTarget或更长时间的Tx仍在mempool中的数目
double failNum = 0; //confTarget之后从未确认但从mempool中删除的Tx的数目
    int periodTarget = (confTarget + scale - 1)/scale;

    int maxbucketindex = buckets.size() - 1;

//要求更高意味着我们正在寻找最低的价格，以便所有更高的价格。
//值通过，所以我们从maxbucketindex（最高频率）开始依次查看
//小一点的桶，直到我们失败。否则，我们要找最高的
//当所有较低的值都失效时，我们就朝相反的方向走。
    unsigned int startbucket = requireGreater ? maxbucketindex : 0;
    int step = requireGreater ? -1 : 1;

//我们将把水桶搅拌到有足够的样品为止。
//近变量和远变量将定义我们组合的范围
//最好的变量是我们看到的最后一个仍然很高的范围
//足够的确认率可算作成功。
//cur变量是我们正在计算的当前范围。
    unsigned int curNearBucket = startbucket;
    unsigned int bestNearBucket = startbucket;
    unsigned int curFarBucket = startbucket;
    unsigned int bestFarBucket = startbucket;

    bool foundAnswer = false;
    unsigned int bins = unconfTxs.size();
    bool newBucketRange = true;
    bool passing = true;
    EstimatorBucket passBucket;
    EstimatorBucket failBucket;

//从最高（默认）或最低有效事务开始计数
    for (int bucket = startbucket; bucket >= 0 && bucket <= maxbucketindex; bucket += step) {
        if (newBucketRange) {
            curNearBucket = bucket;
            newBucketRange = false;
        }
        curFarBucket = bucket;
        nConf += confAvg[periodTarget - 1][bucket];
        totalNum += txCtAvg[bucket];
        failNum += failAvg[periodTarget - 1][bucket];
        for (unsigned int confct = confTarget; confct < GetMaxConfirms(); confct++)
            extraNum += unconfTxs[(nBlockHeight - confct)%bins][bucket];
        extraNum += oldUnconfTxs[bucket];
//如果在这个bucket范围内有足够的事务数据点，
//我们可以测试成功
//（仅对确认的数据点进行计数，以便每次确认计数
//将查看相同数量的数据和相同的存储桶中断）
        if (totalNum >= sufficientTxVal / (1 - decay)) {
            double curPct = nConf / (totalNum + failNum + extraNum);

//看看我们是否不再以成功率得到确认
            if ((requireGreater && curPct < successBreakPoint) || (!requireGreater && curPct > successBreakPoint)) {
                if (passing == true) {
//我们第一次打失败记录失败的桶
                    unsigned int failMinBucket = std::min(curNearBucket, curFarBucket);
                    unsigned int failMaxBucket = std::max(curNearBucket, curFarBucket);
                    failBucket.start = failMinBucket ? buckets[failMinBucket - 1] : 0;
                    failBucket.end = buckets[failMaxBucket];
                    failBucket.withinTarget = nConf;
                    failBucket.totalConfirmed = totalNum;
                    failBucket.inMempool = extraNum;
                    failBucket.leftMempool = failNum;
                    passing = false;
                }
                continue;
            }
//否则，更新累积统计和bucket变量
//重置计数器
            else {
failBucket = EstimatorBucket(); //重置当前通过的任何失败的存储桶
                foundAnswer = true;
                passing = true;
                passBucket.withinTarget = nConf;
                nConf = 0;
                passBucket.totalConfirmed = totalNum;
                totalNum = 0;
                passBucket.inMempool = extraNum;
                passBucket.leftMempool = failNum;
                failNum = 0;
                extraNum = 0;
                bestNearBucket = curNearBucket;
                bestFarBucket = curFarBucket;
                newBucketRange = true;
            }
        }
    }

    double median = -1;
    double txSum = 0;

//计算满足成功条件的最佳铲斗范围的“平均”速度
//找到具有中间事务的bucket，然后报告该bucket的平均值
//这是在找到中位数之间的折衷，因为我们不能保存所有的Tx
//并报告不太准确的平均值
    unsigned int minBucket = std::min(bestNearBucket, bestFarBucket);
    unsigned int maxBucket = std::max(bestNearBucket, bestFarBucket);
    for (unsigned int j = minBucket; j <= maxBucket; j++) {
        txSum += txCtAvg[j];
    }
    if (foundAnswer && txSum != 0) {
        txSum = txSum / 2;
        for (unsigned int j = minBucket; j <= maxBucket; j++) {
            if (txCtAvg[j] < txSum)
                txSum -= txCtAvg[j];
else { //我们在正确的桶里
                median = avg[j] / txCtAvg[j];
                break;
            }
        }

        passBucket.start = minBucket ? buckets[minBucket-1] : 0;
        passBucket.end = buckets[maxBucket];
    }

//如果我们路过，直到到达数据不足的最后几个存储桶，那么将这些存储桶报告为失败。
    if (passing && !newBucketRange) {
        unsigned int failMinBucket = std::min(curNearBucket, curFarBucket);
        unsigned int failMaxBucket = std::max(curNearBucket, curFarBucket);
        failBucket.start = failMinBucket ? buckets[failMinBucket - 1] : 0;
        failBucket.end = buckets[failMaxBucket];
        failBucket.withinTarget = nConf;
        failBucket.totalConfirmed = totalNum;
        failBucket.inMempool = extraNum;
        failBucket.leftMempool = failNum;
    }

    LogPrint(BCLog::ESTIMATEFEE, "FeeEst: %d %s%.0f%% decay %.5f: feerate: %g from (%g - %g) %.2f%% %.1f/(%.1f %d mem %.1f out) Fail: (%g - %g) %.2f%% %.1f/(%.1f %d mem %.1f out)\n",
             confTarget, requireGreater ? ">" : "<", 100.0 * successBreakPoint, decay,
             median, passBucket.start, passBucket.end,
             100 * passBucket.withinTarget / (passBucket.totalConfirmed + passBucket.inMempool + passBucket.leftMempool),
             passBucket.withinTarget, passBucket.totalConfirmed, passBucket.inMempool, passBucket.leftMempool,
             failBucket.start, failBucket.end,
             100 * failBucket.withinTarget / (failBucket.totalConfirmed + failBucket.inMempool + failBucket.leftMempool),
             failBucket.withinTarget, failBucket.totalConfirmed, failBucket.inMempool, failBucket.leftMempool);


    if (result) {
        result->pass = passBucket;
        result->fail = failBucket;
        result->decay = decay;
        result->scale = scale;
    }
    return median;
}

void TxConfirmStats::Write(CAutoFile& fileout) const
{
    fileout << decay;
    fileout << scale;
    fileout << avg;
    fileout << txCtAvg;
    fileout << confAvg;
    fileout << failAvg;
}

void TxConfirmStats::Read(CAutoFile& filein, int nFileVersion, size_t numBuckets)
{
//读取数据文件并进行一些非常基本的健全性检查
//bucket和bucketmap还没有更新，所以不要访问它们
//如果有读取失败，我们还是放弃整个对象。
    size_t maxConfirms, maxPeriods;

//当前版本将使用每个单独的txconfirmstats存储衰减，并保留一个比例因子。
    filein >> decay;
    if (decay <= 0 || decay >= 1) {
        throw std::runtime_error("Corrupt estimates file. Decay must be between 0 and 1 (non-inclusive)");
    }
    filein >> scale;
    if (scale == 0) {
        throw std::runtime_error("Corrupt estimates file. Scale must be non-zero");
    }

    filein >> avg;
    if (avg.size() != numBuckets) {
        throw std::runtime_error("Corrupt estimates file. Mismatch in feerate average bucket count");
    }
    filein >> txCtAvg;
    if (txCtAvg.size() != numBuckets) {
        throw std::runtime_error("Corrupt estimates file. Mismatch in tx count bucket count");
    }
    filein >> confAvg;
    maxPeriods = confAvg.size();
    maxConfirms = scale * maxPeriods;

if (maxConfirms <= 0 || maxConfirms > 6 * 24 * 7) { //一星期
        throw std::runtime_error("Corrupt estimates file.  Must maintain estimates for between 1 and 1008 (one week) confirms");
    }
    for (unsigned int i = 0; i < maxPeriods; i++) {
        if (confAvg[i].size() != numBuckets) {
            throw std::runtime_error("Corrupt estimates file. Mismatch in feerate conf average bucket count");
        }
    }

    filein >> failAvg;
    if (maxPeriods != failAvg.size()) {
        throw std::runtime_error("Corrupt estimates file. Mismatch in confirms tracked for failures");
    }
    for (unsigned int i = 0; i < maxPeriods; i++) {
        if (failAvg[i].size() != numBuckets) {
            throw std::runtime_error("Corrupt estimates file. Mismatch in one of failure average bucket counts");
        }
    }

//调整未存储在数据文件中的当前块变量的大小
//匹配确认数和存储桶数
    resizeInMemoryCounters(numBuckets);

    LogPrint(BCLog::ESTIMATEFEE, "Reading estimates: %u buckets counting confirms up to %u blocks\n",
             numBuckets, maxConfirms);
}

unsigned int TxConfirmStats::NewTx(unsigned int nBlockHeight, double val)
{
    unsigned int bucketindex = bucketMap.lower_bound(val)->second;
    unsigned int blockIndex = nBlockHeight % unconfTxs.size();
    unconfTxs[blockIndex][bucketindex]++;
    return bucketindex;
}

void TxConfirmStats::removeTx(unsigned int entryHeight, unsigned int nBestSeenHeight, unsigned int bucketindex, bool inBlock)
{
//nbestseenheight尚未为新块更新
    int blocksAgo = nBestSeenHeight - entryHeight;
if (nBestSeenHeight == 0)  //BlockPolicyEstimator还没有看到任何块
        blocksAgo = 0;
    if (blocksAgo < 0) {
        LogPrint(BCLog::ESTIMATEFEE, "Blockpolicy error, blocks ago is negative for mempool tx\n");
return;  //这是不可能发生的，因为我们称之为我们最好看到的高度，没有条目可以有更高的
    }

    if (blocksAgo >= (int)unconfTxs.size()) {
        if (oldUnconfTxs[bucketindex] > 0) {
            oldUnconfTxs[bucketindex]--;
        } else {
            LogPrint(BCLog::ESTIMATEFEE, "Blockpolicy error, mempool tx removed from >25 blocks,bucketIndex=%u already\n",
                     bucketindex);
        }
    }
    else {
        unsigned int blockIndex = entryHeight % unconfTxs.size();
        if (unconfTxs[blockIndex][bucketindex] > 0) {
            unconfTxs[blockIndex][bucketindex]--;
        } else {
            LogPrint(BCLog::ESTIMATEFEE, "Blockpolicy error, mempool tx removed from blockIndex=%u,bucketIndex=%u already\n",
                     blockIndex, bucketindex);
        }
    }
if (!inBlock && (unsigned int)blocksAgo >= scale) { //仅当整个期间未确认时才算作失败。
        assert(scale != 0);
        unsigned int periodsAgo = blocksAgo / scale;
        for (size_t i = 0; i < periodsAgo && i < failAvg.size(); i++) {
            failAvg[i][bucketindex]++;
        }
    }
}

//从ctxmempool:：removeunchecked调用此函数以确保
//出于任何原因从mempool中删除的tx不再是
//跟踪。块的一部分TXS已在
//processblocktx确保它们不会被双重跟踪，但它是
//尝试再次移除它们不会造成任何伤害。
bool CBlockPolicyEstimator::removeTx(uint256 hash, bool inBlock)
{
    LOCK(m_cs_fee_estimator);
    std::map<uint256, TxStatsInfo>::iterator pos = mapMemPoolTxs.find(hash);
    if (pos != mapMemPoolTxs.end()) {
        feeStats->removeTx(pos->second.blockHeight, nBestSeenHeight, pos->second.bucketIndex, inBlock);
        shortStats->removeTx(pos->second.blockHeight, nBestSeenHeight, pos->second.bucketIndex, inBlock);
        longStats->removeTx(pos->second.blockHeight, nBestSeenHeight, pos->second.bucketIndex, inBlock);
        mapMemPoolTxs.erase(hash);
        return true;
    } else {
        return false;
    }
}

CBlockPolicyEstimator::CBlockPolicyEstimator()
    : nBestSeenHeight(0), firstRecordedHeight(0), historicalFirst(0), historicalBest(0), trackedTxs(0), untrackedTxs(0)
{
    static_assert(MIN_BUCKET_FEERATE > 0, "Min feerate must be nonzero");
    size_t bucketIndex = 0;
    for (double bucketBoundary = MIN_BUCKET_FEERATE; bucketBoundary <= MAX_BUCKET_FEERATE; bucketBoundary *= FEE_SPACING, bucketIndex++) {
        buckets.push_back(bucketBoundary);
        bucketMap[bucketBoundary] = bucketIndex;
    }
    buckets.push_back(INF_FEERATE);
    bucketMap[INF_FEERATE] = bucketIndex;
    assert(bucketMap.size() == buckets.size());

    feeStats = std::unique_ptr<TxConfirmStats>(new TxConfirmStats(buckets, bucketMap, MED_BLOCK_PERIODS, MED_DECAY, MED_SCALE));
    shortStats = std::unique_ptr<TxConfirmStats>(new TxConfirmStats(buckets, bucketMap, SHORT_BLOCK_PERIODS, SHORT_DECAY, SHORT_SCALE));
    longStats = std::unique_ptr<TxConfirmStats>(new TxConfirmStats(buckets, bucketMap, LONG_BLOCK_PERIODS, LONG_DECAY, LONG_SCALE));
}

CBlockPolicyEstimator::~CBlockPolicyEstimator()
{
}

void CBlockPolicyEstimator::processTransaction(const CTxMemPoolEntry& entry, bool validFeeEstimate)
{
    LOCK(m_cs_fee_estimator);
    unsigned int txHeight = entry.GetHeight();
    uint256 hash = entry.GetTx().GetHash();
    if (mapMemPoolTxs.count(hash)) {
        LogPrint(BCLog::ESTIMATEFEE, "Blockpolicy error mempool tx %s already being tracked\n",
                 hash.ToString().c_str());
        return;
    }

    if (txHeight != nBestSeenHeight) {
//忽略侧链并重新组织；假设它们是随机的，则不会
//影响评估。我们可能会在一个块重新排序中对事务进行双重计数。
//如果blockPolicyEstimator与chainActive.tip（）不同步，则忽略txs。
//它将在下次处理块时同步。
        return;
    }

//只想在我们的区块链同步时更新估计，
//否则，我们将错误计算要包含多少个块。
    if (!validFeeEstimate) {
        untrackedTxs++;
        return;
    }
    trackedTxs++;

//feerates存储并报告为每kb的btc:
    CFeeRate feeRate(entry.GetFee(), entry.GetTxSize());

    mapMemPoolTxs[hash].blockHeight = txHeight;
    unsigned int bucketIndex = feeStats->NewTx(txHeight, (double)feeRate.GetFeePerK());
    mapMemPoolTxs[hash].bucketIndex = bucketIndex;
    unsigned int bucketIndex2 = shortStats->NewTx(txHeight, (double)feeRate.GetFeePerK());
    assert(bucketIndex == bucketIndex2);
    unsigned int bucketIndex3 = longStats->NewTx(txHeight, (double)feeRate.GetFeePerK());
    assert(bucketIndex == bucketIndex3);
}

bool CBlockPolicyEstimator::processBlockTx(unsigned int nBlockHeight, const CTxMemPoolEntry* entry)
{
    if (!removeTx(entry->GetTx().GetHash(), true)) {
//未跟踪此交易以估计费用
        return false;
    }

//矿工需要多少块才能包含此事务？
//blockstoconfirm是基于1的，因此最早包含的事务
//可能的块的确认计数为1
    int blocksToConfirm = nBlockHeight - entry->GetHeight();
    if (blocksToConfirm <= 0) {
//这是不可能发生的，因为我们不处理高度为
//低于我们看到的最大高度
        LogPrint(BCLog::ESTIMATEFEE, "Blockpolicy error Transaction had negative blocksToConfirm\n");
        return false;
    }

//feerates存储并报告为每kb的btc:
    CFeeRate feeRate(entry->GetFee(), entry->GetTxSize());

    feeStats->Record(blocksToConfirm, (double)feeRate.GetFeePerK());
    shortStats->Record(blocksToConfirm, (double)feeRate.GetFeePerK());
    longStats->Record(blocksToConfirm, (double)feeRate.GetFeePerK());
    return true;
}

void CBlockPolicyEstimator::processBlock(unsigned int nBlockHeight,
                                         std::vector<const CTxMemPoolEntry*>& entries)
{
    LOCK(m_cs_fee_estimator);
    if (nBlockHeight <= nBestSeenHeight) {
//忽略侧链并重新组织；假设它们是随机的
//它们不会影响评估。
//如果攻击者可以随意重新组织链，那么
//你有比“攻击者能影响”更大的问题
//交易费用。”
        return;
    }

//必须将nbestseenheight与clearcurrent同步，以便
//调用removetx（通过processblocktx）正确计算年龄
//要从跟踪中删除的未确认tx。
    nBestSeenHeight = nBlockHeight;

//更新未确认的循环缓冲区
    feeStats->ClearCurrent(nBlockHeight);
    shortStats->ClearCurrent(nBlockHeight);
    longStats->ClearCurrent(nBlockHeight);

//衰减所有指数平均值
    feeStats->UpdateMovingAverages();
    shortStats->UpdateMovingAverages();
    longStats->UpdateMovingAverages();

    unsigned int countedTxs = 0;
//用当前块中的数据点更新平均值
    for (const auto& entry : entries) {
        if (processBlockTx(nBlockHeight, entry))
            countedTxs++;
    }

    if (firstRecordedHeight == 0 && countedTxs > 0) {
        firstRecordedHeight = nBestSeenHeight;
        LogPrint(BCLog::ESTIMATEFEE, "Blockpolicy first recorded height %u\n", firstRecordedHeight);
    }


    LogPrint(BCLog::ESTIMATEFEE, "Blockpolicy estimates updated by %u of %u block txs, since last block %u of %u tracked, mempool map size %u, max target %u from %s\n",
             countedTxs, entries.size(), trackedTxs, trackedTxs + untrackedTxs, mapMemPoolTxs.size(),
             MaxUsableEstimate(), HistoricalBlockSpan() > BlockSpan() ? "historical" : "current");

    trackedTxs = 0;
    untrackedTxs = 0;
}

CFeeRate CBlockPolicyEstimator::estimateFee(int confTarget) const
{
//无法对confTarget 1进行合理估计
    if (confTarget <= 1)
        return CFeeRate(0);

    return estimateRawFee(confTarget, DOUBLE_SUCCESS_PCT, FeeEstimateHorizon::MED_HALFLIFE);
}

CFeeRate CBlockPolicyEstimator::estimateRawFee(int confTarget, double successThreshold, FeeEstimateHorizon horizon, EstimationResult* result) const
{
    TxConfirmStats* stats;
    double sufficientTxs = SUFFICIENT_FEETXS;
    switch (horizon) {
    case FeeEstimateHorizon::SHORT_HALFLIFE: {
        stats = shortStats.get();
        sufficientTxs = SUFFICIENT_TXS_SHORT;
        break;
    }
    case FeeEstimateHorizon::MED_HALFLIFE: {
        stats = feeStats.get();
        break;
    }
    case FeeEstimateHorizon::LONG_HALFLIFE: {
        stats = longStats.get();
        break;
    }
    default: {
        throw std::out_of_range("CBlockPolicyEstimator::estimateRawFee unknown FeeEstimateHorizon");
    }
    }

    LOCK(m_cs_fee_estimator);
//如果试图分析我们没有跟踪的目标，返回失败
    if (confTarget <= 0 || (unsigned int)confTarget > stats->GetMaxConfirms())
        return CFeeRate(0);
    if (successThreshold > 1)
        return CFeeRate(0);

    double median = stats->EstimateMedianVal(confTarget, sufficientTxs, successThreshold, true, nBestSeenHeight, result);

    if (median < 0)
        return CFeeRate(0);

    return CFeeRate(llround(median));
}

unsigned int CBlockPolicyEstimator::HighestTargetTracked(FeeEstimateHorizon horizon) const
{
    LOCK(m_cs_fee_estimator);
    switch (horizon) {
    case FeeEstimateHorizon::SHORT_HALFLIFE: {
        return shortStats->GetMaxConfirms();
    }
    case FeeEstimateHorizon::MED_HALFLIFE: {
        return feeStats->GetMaxConfirms();
    }
    case FeeEstimateHorizon::LONG_HALFLIFE: {
        return longStats->GetMaxConfirms();
    }
    default: {
        throw std::out_of_range("CBlockPolicyEstimator::HighestTargetTracked unknown FeeEstimateHorizon");
    }
    }
}

unsigned int CBlockPolicyEstimator::BlockSpan() const
{
    if (firstRecordedHeight == 0) return 0;
    assert(nBestSeenHeight >= firstRecordedHeight);

    return nBestSeenHeight - firstRecordedHeight;
}

unsigned int CBlockPolicyEstimator::HistoricalBlockSpan() const
{
    if (historicalFirst == 0) return 0;
    assert(historicalBest >= historicalFirst);

    if (nBestSeenHeight - historicalBest > OLDEST_ESTIMATE_HISTORY) return 0;

    return historicalBest - historicalFirst;
}

unsigned int CBlockPolicyEstimator::MaxUsableEstimate() const
{
//块跨度除以2，以确保有足够的潜在失败数据点用于估计。
    return std::min(longStats->GetMaxConfirms(), std::max(BlockSpan(), HistoricalBlockSpan()) / 2);
}

/*以所需成功阈值从最短值返回费用估计值
 *跟踪到所需目标的确认的时间范围。如果
 *请检查shorterhorizon，也允许短时间范围估计
 *对于较低目标，减少给定答案*/

double CBlockPolicyEstimator::estimateCombinedFee(unsigned int confTarget, double successThreshold, bool checkShorterHorizon, EstimationResult *result) const
{
    double estimate = -1;
    if (confTarget >= 1 && confTarget <= longStats->GetMaxConfirms()) {
//从可能的最短时间范围查找估计
if (confTarget <= shortStats->GetMaxConfirms()) { //短视界
            estimate = shortStats->EstimateMedianVal(confTarget, SUFFICIENT_TXS_SHORT, successThreshold, true, nBestSeenHeight, result);
        }
else if (confTarget <= feeStats->GetMaxConfirms()) { //中视界
            estimate = feeStats->EstimateMedianVal(confTarget, SUFFICIENT_FEETXS, successThreshold, true, nBestSeenHeight, result);
        }
else { //长地平线
            estimate = longStats->EstimateMedianVal(confTarget, SUFFICIENT_FEETXS, successThreshold, true, nBestSeenHeight, result);
        }
        if (checkShorterHorizon) {
            EstimationResult tempResult;
//如果来自最近地平线的较低ConfTarget返回较低的答案，请使用它。
            if (confTarget > feeStats->GetMaxConfirms()) {
                double medMax = feeStats->EstimateMedianVal(feeStats->GetMaxConfirms(), SUFFICIENT_FEETXS, successThreshold, true, nBestSeenHeight, &tempResult);
                if (medMax > 0 && (estimate == -1 || medMax < estimate)) {
                    estimate = medMax;
                    if (result) *result = tempResult;
                }
            }
            if (confTarget > shortStats->GetMaxConfirms()) {
                double shortMax = shortStats->EstimateMedianVal(shortStats->GetMaxConfirms(), SUFFICIENT_TXS_SHORT, successThreshold, true, nBestSeenHeight, &tempResult);
                if (shortMax > 0 && (estimate == -1 || shortMax < estimate)) {
                    estimate = shortMax;
                    if (result) *result = tempResult;
                }
            }
        }
    }
    return estimate;
}

/*确保保守估计也满足双重成功率
 *2*目标时间范围更长。
 **/

double CBlockPolicyEstimator::estimateConservativeFee(unsigned int doubleTarget, EstimationResult *result) const
{
    double estimate = -1;
    EstimationResult tempResult;
    if (doubleTarget <= shortStats->GetMaxConfirms()) {
        estimate = feeStats->EstimateMedianVal(doubleTarget, SUFFICIENT_FEETXS, DOUBLE_SUCCESS_PCT, true, nBestSeenHeight, result);
    }
    if (doubleTarget <= feeStats->GetMaxConfirms()) {
        double longEstimate = longStats->EstimateMedianVal(doubleTarget, SUFFICIENT_FEETXS, DOUBLE_SUCCESS_PCT, true, nBestSeenHeight, &tempResult);
        if (longEstimate > estimate) {
            estimate = longEstimate;
            if (result) *result = tempResult;
        }
    }
    return estimate;
}

/*EstimateMartfee返回以60%计算的最大值
 *目标/2要求的阈值，目标和A要求的85%阈值
 *2*目标要求95%阈值。每次计算都是在
 *跟踪所需目标的最短时间范围。保守的
 *然而，估计需要满足2*目标的95%阈值
 *时间范围也更长。
 **/

CFeeRate CBlockPolicyEstimator::estimateSmartFee(int confTarget, FeeCalculation *feeCalc, bool conservative) const
{
    LOCK(m_cs_fee_estimator);

    if (feeCalc) {
        feeCalc->desiredTarget = confTarget;
        feeCalc->returnedTarget = confTarget;
    }

    double median = -1;
    EstimationResult tempResult;

//如果试图分析我们没有跟踪的目标，返回失败
    if (confTarget <= 0 || (unsigned int)confTarget > longStats->GetMaxConfirms()) {
return CFeeRate(0);  //误差条件
    }

//无法对confTarget 1进行合理估计
    if (confTarget == 1) confTarget = 2;

    unsigned int maxUsableEstimate = MaxUsableEstimate();
    if ((unsigned int)confTarget > maxUsableEstimate) {
        confTarget = maxUsableEstimate;
    }
    if (feeCalc) feeCalc->returnedTarget = confTarget;

if (confTarget <= 1) return CFeeRate(0); //误差条件

assert(confTarget > 0); //EstimateCombinedFee和EstimateReservativeFee采用无符号整数
    /*true传递给target/2和target的估计组合费用，因此
     *我们也检查了最短时间范围的确认值。
     *这对于保持单调递增的估计值是必要的。
     *对于非保守估计，我们对2*目标做同样的事情，但是
     *对于保守估计，我们希望跳过这些较短的范围。
     *检查2*目标，因为我们一直在取最大值
     *层位，因此我们已经有了单调递增的估计和
     *保守估计的目的是不允许短期
     *波动使我们的估计值降低了太多。
     **/

    double halfEst = estimateCombinedFee(confTarget/2, HALF_SUCCESS_PCT, true, &tempResult);
    if (feeCalc) {
        feeCalc->est = tempResult;
        feeCalc->reason = FeeReason::HALF_ESTIMATE;
    }
    median = halfEst;
    double actualEst = estimateCombinedFee(confTarget, SUCCESS_PCT, true, &tempResult);
    if (actualEst > median) {
        median = actualEst;
        if (feeCalc) {
            feeCalc->est = tempResult;
            feeCalc->reason = FeeReason::FULL_ESTIMATE;
        }
    }
    double doubleEst = estimateCombinedFee(2 * confTarget, DOUBLE_SUCCESS_PCT, !conservative, &tempResult);
    if (doubleEst > median) {
        median = doubleEst;
        if (feeCalc) {
            feeCalc->est = tempResult;
            feeCalc->reason = FeeReason::DOUBLE_ESTIMATE;
        }
    }

    if (conservative || median == -1) {
        double consEst =  estimateConservativeFee(2 * confTarget, &tempResult);
        if (consEst > median) {
            median = consEst;
            if (feeCalc) {
                feeCalc->est = tempResult;
                feeCalc->reason = FeeReason::CONSERVATIVE;
            }
        }
    }

if (median < 0) return CFeeRate(0); //误差条件

    return CFeeRate(llround(median));
}


bool CBlockPolicyEstimator::Write(CAutoFile& fileout) const
{
    try {
        LOCK(m_cs_fee_estimator);
fileout << 149900; //需要读取的版本：0.14.99或更高版本
fileout << CLIENT_VERSION; //写入文件的版本
        fileout << nBestSeenHeight;
        if (BlockSpan() > HistoricalBlockSpan()/2) {
            fileout << firstRecordedHeight << nBestSeenHeight;
        }
        else {
            fileout << historicalFirst << historicalBest;
        }
        fileout << buckets;
        feeStats->Write(fileout);
        shortStats->Write(fileout);
        longStats->Write(fileout);
    }
    catch (const std::exception&) {
        LogPrintf("CBlockPolicyEstimator::Write(): unable to write policy estimator data (non-fatal)\n");
        return false;
    }
    return true;
}

bool CBlockPolicyEstimator::Read(CAutoFile& filein)
{
    try {
        LOCK(m_cs_fee_estimator);
        int nVersionRequired, nVersionThatWrote;
        filein >> nVersionRequired >> nVersionThatWrote;
        if (nVersionRequired > CLIENT_VERSION)
            return error("CBlockPolicyEstimator::Read(): up-version (%d) fee estimate file", nVersionRequired);

//将费用估算文件读取到临时变量中，以便现有数据
//如果存在异常，则结构不会损坏。
        unsigned int nFileBestSeenHeight;
        filein >> nFileBestSeenHeight;

        if (nVersionRequired < 149900) {
            LogPrintf("%s: incompatible old fee estimation data (non-fatal). Version: %d\n", __func__, nVersionRequired);
} else { //149900年引入的新格式
            unsigned int nFileHistoricalFirst, nFileHistoricalBest;
            filein >> nFileHistoricalFirst >> nFileHistoricalBest;
            if (nFileHistoricalFirst > nFileHistoricalBest || nFileHistoricalBest > nFileBestSeenHeight) {
                throw std::runtime_error("Corrupt estimates file. Historical block range for estimates is invalid");
            }
            std::vector<double> fileBuckets;
            filein >> fileBuckets;
            size_t numBuckets = fileBuckets.size();
            if (numBuckets <= 1 || numBuckets > 1000)
                throw std::runtime_error("Corrupt estimates file. Must have between 2 and 1000 feerate buckets");

            std::unique_ptr<TxConfirmStats> fileFeeStats(new TxConfirmStats(buckets, bucketMap, MED_BLOCK_PERIODS, MED_DECAY, MED_SCALE));
            std::unique_ptr<TxConfirmStats> fileShortStats(new TxConfirmStats(buckets, bucketMap, SHORT_BLOCK_PERIODS, SHORT_DECAY, SHORT_SCALE));
            std::unique_ptr<TxConfirmStats> fileLongStats(new TxConfirmStats(buckets, bucketMap, LONG_BLOCK_PERIODS, LONG_DECAY, LONG_SCALE));
            fileFeeStats->Read(filein, nVersionThatWrote, numBuckets);
            fileShortStats->Read(filein, nVersionThatWrote, numBuckets);
            fileLongStats->Read(filein, nVersionThatWrote, numBuckets);

//正确分析费用估算文件
//从文件中复制bucket并刷新bucketmap
            buckets = fileBuckets;
            bucketMap.clear();
            for (unsigned int i = 0; i < buckets.size(); i++) {
                bucketMap[buckets[i]] = i;
            }

//销毁旧的txconfirmstats并指向已经引用bucket和bucketmap的新的txconfirmstats
            feeStats = std::move(fileFeeStats);
            shortStats = std::move(fileShortStats);
            longStats = std::move(fileLongStats);

            nBestSeenHeight = nFileBestSeenHeight;
            historicalFirst = nFileHistoricalFirst;
            historicalBest = nFileHistoricalBest;
        }
    }
    catch (const std::exception& e) {
        LogPrintf("CBlockPolicyEstimator::Read(): unable to read policy estimator data (non-fatal): %s\n",e.what());
        return false;
    }
    return true;
}

void CBlockPolicyEstimator::FlushUnconfirmed() {
    int64_t startclear = GetTimeMicros();
    LOCK(m_cs_fee_estimator);
    size_t num_entries = mapMemPoolTxs.size();
//删除mapmempooltxs中的每个条目
    while (!mapMemPoolTxs.empty()) {
        auto mi = mapMemPoolTxs.begin();
removeTx(mi->first, false); //在mapmempooltxs上调用erase（）。
    }
    int64_t endclear = GetTimeMicros();
    LogPrint(BCLog::ESTIMATEFEE, "Recorded %u unconfirmed txs from mempool in %gs\n", num_entries, (endclear - startclear)*0.000001);
}

FeeFilterRounder::FeeFilterRounder(const CFeeRate& minIncrementalFee)
{
    CAmount minFeeLimit = std::max(CAmount(1), minIncrementalFee.GetFeePerK() / 2);
    feeset.insert(0);
    for (double bucketBoundary = minFeeLimit; bucketBoundary <= MAX_FILTER_FEERATE; bucketBoundary *= FEE_FILTER_SPACING) {
        feeset.insert(bucketBoundary);
    }
}

CAmount FeeFilterRounder::round(CAmount currentMinFee)
{
    std::set<double>::iterator it = feeset.lower_bound(currentMinFee);
    if ((it != feeset.begin() && insecure_rand.rand32() % 3 != 0) || it == feeset.end()) {
        it--;
    }
    return static_cast<CAmount>(*it);
}
