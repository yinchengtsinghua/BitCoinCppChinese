
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
#ifndef BITCOIN_POLICY_FEES_H
#define BITCOIN_POLICY_FEES_H

#include <amount.h>
#include <policy/feerate.h>
#include <uint256.h>
#include <random.h>
#include <sync.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

class CAutoFile;
class CFeeRate;
class CTxMemPoolEntry;
class CTxMemPool;
class TxConfirmStats;

/*将跟踪的3个不同txconfirmstats的每个标识符
 *不同时间范围的历史。*/

enum class FeeEstimateHorizon {
    SHORT_HALFLIFE = 0,
    MED_HALFLIFE = 1,
    LONG_HALFLIFE = 2
};

std::string StringForFeeEstimateHorizon(FeeEstimateHorizon horizon);

/*退回费用估算原因的列举*/
enum class FeeReason {
    NONE,
    HALF_ESTIMATE,
    FULL_ESTIMATE,
    DOUBLE_ESTIMATE,
    CONSERVATIVE,
    MEMPOOL_MIN,
    PAYTXFEE,
    FALLBACK,
    REQUIRED,
    MAXTXFEE,
};

std::string StringForFeeReason(FeeReason reason);

/*用于确定请求的费用估算类型*/
enum class FeeEstimateMode {
UNSET,        //！<根据其他条件使用默认设置
ECONOMICAL,   //！<强制估计artfee使用非保守估计
CONSERVATIVE, //！<强制估计artfee使用保守估计
};

bool FeeModeFromString(const std::string& mode_string, FeeEstimateMode& fee_estimate_mode);

/*用于返回有关feerate bucket的详细信息*/
struct EstimatorBucket
{
    double start = -1;
    double end = -1;
    double withinTarget = 0;
    double totalConfirmed = 0;
    double inMempool = 0;
    double leftMempool = 0;
};

/*用于返回有关费用估计计算的详细信息*/
struct EstimationResult
{
    EstimatorBucket pass;
    EstimatorBucket fail;
    double decay = 0;
    unsigned int scale = 0;
};

struct FeeCalculation
{
    EstimationResult est;
    FeeReason reason = FeeReason::NONE;
    int desiredTarget = 0;
    int returnedTarget = 0;
};

/*类cblockpolicyEstimator
 *BlockPolicyEstimator用于估计所需的频率。
 *对于一个块中包含的事务，在
 *块。
 *
 *在较高的层次上，该算法通过将事务分组为存储桶来工作。
 *基于相似的频率，然后跟踪它的持续时间
 *在要挖掘的各个存储桶中进行事务处理。它是在
 *假设在一般较高利率的交易中
 *包含在低利率交易前的区块中。所以
 *例如，如果您想知道应该将交易提交到什么位置
 *在接下来的5个块中包含在一个块中，您将首先查看
 *在具有最高事务的存储桶中，并验证
 *在5个区块内确认了足够高的百分比，并且
 *然后你会看到下一个最高的桶，等等，停在
 *最后一个通过测试的桶。此中交易的平均次数
 *水桶会给你一个最低温度的指示。
 *交易且仍有足够高的确认概率
 *在您想要的5个街区内。
 *
 *以下是实施的简要说明：
 *当事务进入mempool时，我们跟踪区块链的高度。
 *进入时。所有进一步的计算只在这组“seen”上进行。
 *交易。每当一个块进入时，我们就计算事务的数量。
 *每个桶和每个桶支付的总金额。然后我们
 *计算每个事务挖掘所用的块数Y。我们转换
 *从多个块到多个周期y'每个周期包含“比例”
 *块。这是在3个不同的数据集中跟踪的，每个数据集的最大值为
 *期间数。在每个数据集中，我们在每个
 *feerate bucket，我们将所有计数器从y'增加到max periods。
 *表示在小于或等于
 *很多时期。我们想保存这个信息的历史记录，所以
 *时间我们有一个计数器，显示在
 *给出一个桶和每个桶中确认的总数
 *任何桶的周期或更短。我们通过保存
 *这些数据的指数衰减移动平均值。这是
 *针对3个数据集中的每个数据集进行不同的衰减，以保留相关数据。
 *来自不同的时间范围。此外，我们还跟踪数字
 *未链接（在mempool或左mempool中，不包含在块中）
 *每个存储桶中的事务，以及它们已存在的块数
 *未完成，并使用这两个数字来增加交易数量
 *在计算任何数字的估计值时，我们已经看到在那个桶里。
 *低于未完成的块数的确认。
 *
 *我们希望能够估计德克萨斯州所需的费用。
 *一定数量的块。每次将块添加到最佳链时，此类都会记录
 *该块中包含的事务的统计信息
 **/

class CBlockPolicyEstimator
{
private:
    /*轨道确认短距离延迟最多12个街区*/
    static constexpr unsigned int SHORT_BLOCK_PERIODS = 12;
    static constexpr unsigned int SHORT_SCALE = 1;
    /*中层轨道确认延迟高达48个街区*/
    static constexpr unsigned int MED_BLOCK_PERIODS = 24;
    static constexpr unsigned int MED_SCALE = 2;
    /*长地平线的轨道确认延迟高达1008个街区*/
    static constexpr unsigned int LONG_BLOCK_PERIODS = 42;
    static constexpr unsigned int LONG_SCALE = 24;
    /*超过此时间的历史估计值无效*/
    static const unsigned int OLDEST_ESTIMATE_HISTORY = 6 * 1008;

    /*.962的衰变是18个街区或大约3小时的半衰期。*/
    static constexpr double SHORT_DECAY = .962;
    /*.998的衰变是144个街区或大约1天的半衰期。*/
    static constexpr double MED_DECAY = .9952;
    /*.9995的衰变是1008个街区或大约1周的半衰期。*/
    static constexpr double LONG_DECAY = .99931;

    /*要求在Y/2区内确认超过60%的交易*/
    static constexpr double HALF_SUCCESS_PCT = .6;
    /*要求在Y区内确认超过85%的交易*/
    static constexpr double SUCCESS_PCT = .85;
    /*要求在2*Y区内确认超过95%的交易*/
    static constexpr double DOUBLE_SUCCESS_PCT = .95;

    /*要求每个数据块组合feerate bucket的平均值为0.1tx，以具有统计意义。*/
    static constexpr double SUFFICIENT_FEETXS = 0.1;
    /*使用短衰减时需要0.5tx的平均值，因为考虑的块较少*/
    static constexpr double SUFFICIENT_TXS_SHORT = 0.5;

    /*跟踪频率的最小值和最大值
     *Min'u Bucket Feerate应设置为最低合理Feerate We
     *可能永远都想追踪。从历史上看，这已经是1000年了
     *继承默认的中继费并更改它会造成中断
     *使旧评估文件失效。所以把它留在1000，除非它变成
     *必须降低，然后大幅降低。
     **/

    static constexpr double MIN_BUCKET_FEERATE = 1000;
    static constexpr double MAX_BUCKET_FEERATE = 1e7;

    /*动叶间距
     *我们必须根据汇率将交易集中到桶中，但我们希望能够
     *在很大范围内给出准确的估计
     *因此，对桶进行指数空间划分是有意义的。
     **/

    static constexpr double FEE_SPACING = 1.05;

public:
    /*创建新的BlockPolicyEstimator并用默认值初始化统计跟踪类*/
    CBlockPolicyEstimator();
    ~CBlockPolicyEstimator();

    /*处理块中包含的所有事务*/
    void processBlock(unsigned int nBlockHeight,
                      std::vector<const CTxMemPoolEntry*>& entries);

    /*处理向mempoo接受的事务*/
    void processTransaction(const CTxMemPoolEntry& entry, bool validFeeEstimate);

    /*从mempool跟踪状态中删除事务*/
    bool removeTx(uint256 hash, bool inBlock);

    /*不赞成的返回估计值*/
    CFeeRate estimateFee(int confTarget) const;

    /*需要包含在confTarget中的块中的估计次数
     *块。如果ConfTarget无法给出答案，则返回估计值
     *最接近的目标。”保守的估计是
     *在较长的时间范围内也有效。
     **/

    CFeeRate estimateSmartFee(int confTarget, FeeCalculation *feeCalc, bool conservative) const;

    /*返回具有给定成功的特定费用估算计算
     *阈值和时间范围，并可选择返回有关
     ＊计算
     **/

    CFeeRate estimateRawFee(int confTarget, double successThreshold, FeeEstimateHorizon horizon, EstimationResult *result = nullptr) const;

    /*将估计数据写入文件*/
    bool Write(CAutoFile& fileout) const;

    /*从文件中读取估计数据*/
    bool Read(CAutoFile& filein);

    /*关闭时清空mempool事务，以记录无法确认仍在mempool中的tx*/
    void FlushUnconfirmed();

    /*估计跟踪的最高目标的计算*/
    unsigned int HighestTargetTracked(FeeEstimateHorizon horizon) const;

private:
    mutable CCriticalSection m_cs_fee_estimator;

    unsigned int nBestSeenHeight GUARDED_BY(m_cs_fee_estimator);
    unsigned int firstRecordedHeight GUARDED_BY(m_cs_fee_estimator);
    unsigned int historicalFirst GUARDED_BY(m_cs_fee_estimator);
    unsigned int historicalBest GUARDED_BY(m_cs_fee_estimator);

    struct TxStatsInfo
    {
        unsigned int blockHeight;
        unsigned int bucketIndex;
        TxStatsInfo() : blockHeight(0), bucketIndex(0) {}
    };

//将txid映射到有关该事务的信息
    std::map<uint256, TxStatsInfo> mapMemPoolTxs GUARDED_BY(m_cs_fee_estimator);

    /*用于跟踪交易确认的历史数据的类*/
    std::unique_ptr<TxConfirmStats> feeStats PT_GUARDED_BY(m_cs_fee_estimator);
    std::unique_ptr<TxConfirmStats> shortStats PT_GUARDED_BY(m_cs_fee_estimator);
    std::unique_ptr<TxConfirmStats> longStats PT_GUARDED_BY(m_cs_fee_estimator);

    unsigned int trackedTxs GUARDED_BY(m_cs_fee_estimator);
    unsigned int untrackedTxs GUARDED_BY(m_cs_fee_estimator);

std::vector<double> buckets GUARDED_BY(m_cs_fee_estimator); //桶范围的上限（包含）
std::map<double, unsigned int> bucketMap GUARDED_BY(m_cs_fee_estimator); //Bucket上界到Bucket索引到所有向量的映射

    /*处理集团确认的交易*/
    bool processBlockTx(unsigned int nBlockHeight, const CTxMemPoolEntry* entry) EXCLUSIVE_LOCKS_REQUIRED(m_cs_fee_estimator);

    /*评估助手artfee*/
    double estimateCombinedFee(unsigned int confTarget, double successThreshold, bool checkShorterHorizon, EstimationResult *result) const EXCLUSIVE_LOCKS_REQUIRED(m_cs_fee_estimator);
    /*评估助手artfee*/
    double estimateConservativeFee(unsigned int doubleTarget, EstimationResult *result) const EXCLUSIVE_LOCKS_REQUIRED(m_cs_fee_estimator);
    /*运行费用估计时记录的数据块数*/
    unsigned int BlockSpan() const EXCLUSIVE_LOCKS_REQUIRED(m_cs_fee_estimator);
    /*在已保存数据文件中表示的已记录费用估算数据块数*/
    unsigned int HistoricalBlockSpan() const EXCLUSIVE_LOCKS_REQUIRED(m_cs_fee_estimator);
    /*可提供合理估计的最高目标的计算*/
    unsigned int MaxUsableEstimate() const EXCLUSIVE_LOCKS_REQUIRED(m_cs_fee_estimator);
};

class FeeFilterRounder
{
private:
    static constexpr double MAX_FILTER_FEERATE = 1e7;
    /*fee_filter_spacing仅用于提供一些费用量化
     *过滤结果。从历史上看，它重复使用费用间隔，但它完全是
     *不相关，并且是单独的常量，因此这两个概念不是
     *绑在一起*/

    static constexpr double FEE_FILTER_SPACING = 1.1;

public:
    /*创建新的feefilterrounder*/
    explicit FeeFilterRounder(const CFeeRate& minIncrementalFee);

    /*在广播前为隐私目的量化最低费用*/
    CAmount round(CAmount currentMinFee);

private:
    std::set<double> feeset;
    FastRandomContext insecure_rand;
};

#endif //比特币政策费
