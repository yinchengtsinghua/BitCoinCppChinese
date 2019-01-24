
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

#include <chain.h>
#include <versionbits.h>
#include <test/test_bitcoin.h>
#include <chainparams.h>
#include <validation.h>
#include <consensus/params.h>

#include <boost/test/unit_test.hpp>

/*定义虚拟数据块时间，2014年11月14日之后每10分钟一个数据块，上午0:55:36*/
static int32_t TestTime(int nHeight) { return 1415926536 + 600 * nHeight; }

static const Consensus::Params paramsDummy = Consensus::Params();

class TestConditionChecker : public AbstractThresholdConditionChecker
{
private:
    mutable ThresholdConditionCache cache;

public:
    int64_t BeginTime(const Consensus::Params& params) const override { return TestTime(10000); }
    int64_t EndTime(const Consensus::Params& params) const override { return TestTime(20000); }
    int Period(const Consensus::Params& params) const override { return 1000; }
    int Threshold(const Consensus::Params& params) const override { return 900; }
    bool Condition(const CBlockIndex* pindex, const Consensus::Params& params) const override { return (pindex->nVersion & 0x100); }

    ThresholdState GetStateFor(const CBlockIndex* pindexPrev) const { return AbstractThresholdConditionChecker::GetStateFor(pindexPrev, paramsDummy, cache); }
    int GetStateSinceHeightFor(const CBlockIndex* pindexPrev) const { return AbstractThresholdConditionChecker::GetStateSinceHeightFor(pindexPrev, paramsDummy, cache); }
};

class TestAlwaysActiveConditionChecker : public TestConditionChecker
{
public:
    int64_t BeginTime(const Consensus::Params& params) const override { return Consensus::BIP9Deployment::ALWAYS_ACTIVE; }
};

#define CHECKERS 6

class VersionBitsTester
{
//假区块链
    std::vector<CBlockIndex*> vpblock;

//6个相同位的独立检查程序。
//第一个执行所有检查，第二个仅50%，第三个仅25%，等等…
//这是为了测试缺少缓存信息是否会导致相同的结果。
    TestConditionChecker checker[CHECKERS];
//另外6个假设始终激活
    TestAlwaysActiveConditionChecker checker_always[CHECKERS];

//测试计数器（用于识别故障）
    int num;

public:
    VersionBitsTester() : num(0) {}

    VersionBitsTester& Reset() {
        for (unsigned int i = 0; i < vpblock.size(); i++) {
            delete vpblock[i];
        }
        for (unsigned int  i = 0; i < CHECKERS; i++) {
            checker[i] = TestConditionChecker();
            checker_always[i] = TestAlwaysActiveConditionChecker();
        }
        vpblock.clear();
        return *this;
    }

    ~VersionBitsTester() {
         Reset();
    }

    VersionBitsTester& Mine(unsigned int height, int32_t nTime, int32_t nVersion) {
        while (vpblock.size() < height) {
            CBlockIndex* pindex = new CBlockIndex();
            pindex->nHeight = vpblock.size();
            pindex->pprev = vpblock.size() > 0 ? vpblock.back() : nullptr;
            pindex->nTime = nTime;
            pindex->nVersion = nVersion;
            pindex->BuildSkip();
            vpblock.push_back(pindex);
        }
        return *this;
    }

    VersionBitsTester& TestStateSinceHeight(int height) {
        for (int i = 0; i < CHECKERS; i++) {
            if (InsecureRandBits(i) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateSinceHeightFor(vpblock.empty() ? nullptr : vpblock.back()) == height, strprintf("Test %i for StateSinceHeight", num));
                BOOST_CHECK_MESSAGE(checker_always[i].GetStateSinceHeightFor(vpblock.empty() ? nullptr : vpblock.back()) == 0, strprintf("Test %i for StateSinceHeight (always active)", num));
            }
        }
        num++;
        return *this;
    }

    VersionBitsTester& TestDefined() {
        for (int i = 0; i < CHECKERS; i++) {
            if (InsecureRandBits(i) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == ThresholdState::DEFINED, strprintf("Test %i for DEFINED", num));
                BOOST_CHECK_MESSAGE(checker_always[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == ThresholdState::ACTIVE, strprintf("Test %i for ACTIVE (always active)", num));
            }
        }
        num++;
        return *this;
    }

    VersionBitsTester& TestStarted() {
        for (int i = 0; i < CHECKERS; i++) {
            if (InsecureRandBits(i) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == ThresholdState::STARTED, strprintf("Test %i for STARTED", num));
                BOOST_CHECK_MESSAGE(checker_always[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == ThresholdState::ACTIVE, strprintf("Test %i for ACTIVE (always active)", num));
            }
        }
        num++;
        return *this;
    }

    VersionBitsTester& TestLockedIn() {
        for (int i = 0; i < CHECKERS; i++) {
            if (InsecureRandBits(i) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == ThresholdState::LOCKED_IN, strprintf("Test %i for LOCKED_IN", num));
                BOOST_CHECK_MESSAGE(checker_always[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == ThresholdState::ACTIVE, strprintf("Test %i for ACTIVE (always active)", num));
            }
        }
        num++;
        return *this;
    }

    VersionBitsTester& TestActive() {
        for (int i = 0; i < CHECKERS; i++) {
            if (InsecureRandBits(i) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == ThresholdState::ACTIVE, strprintf("Test %i for ACTIVE", num));
                BOOST_CHECK_MESSAGE(checker_always[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == ThresholdState::ACTIVE, strprintf("Test %i for ACTIVE (always active)", num));
            }
        }
        num++;
        return *this;
    }

    VersionBitsTester& TestFailed() {
        for (int i = 0; i < CHECKERS; i++) {
            if (InsecureRandBits(i) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == ThresholdState::FAILED, strprintf("Test %i for FAILED", num));
                BOOST_CHECK_MESSAGE(checker_always[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == ThresholdState::ACTIVE, strprintf("Test %i for ACTIVE (always active)", num));
            }
        }
        num++;
        return *this;
    }

    CBlockIndex * Tip() { return vpblock.size() ? vpblock.back() : nullptr; }
};

BOOST_FIXTURE_TEST_SUITE(versionbits_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(versionbits_test)
{
    for (int i = 0; i < 64; i++) {
//defined->失败
        VersionBitsTester().TestDefined().TestStateSinceHeight(0)
                           .Mine(1, TestTime(1), 0x100).TestDefined().TestStateSinceHeight(0)
                           .Mine(11, TestTime(11), 0x100).TestDefined().TestStateSinceHeight(0)
                           .Mine(989, TestTime(989), 0x100).TestDefined().TestStateSinceHeight(0)
                           .Mine(999, TestTime(20000), 0x100).TestDefined().TestStateSinceHeight(0)
                           .Mine(1000, TestTime(20000), 0x100).TestFailed().TestStateSinceHeight(1000)
                           .Mine(1999, TestTime(30001), 0x100).TestFailed().TestStateSinceHeight(1000)
                           .Mine(2000, TestTime(30002), 0x100).TestFailed().TestStateSinceHeight(1000)
                           .Mine(2001, TestTime(30003), 0x100).TestFailed().TestStateSinceHeight(1000)
                           .Mine(2999, TestTime(30004), 0x100).TestFailed().TestStateSinceHeight(1000)
                           .Mine(3000, TestTime(30005), 0x100).TestFailed().TestStateSinceHeight(1000)

//已定义->已启动->失败
                           .Reset().TestDefined().TestStateSinceHeight(0)
                           .Mine(1, TestTime(1), 0).TestDefined().TestStateSinceHeight(0)
.Mine(1000, TestTime(10000) - 1, 0x100).TestDefined().TestStateSinceHeight(0) //再过一秒钟，它就会被定义
.Mine(2000, TestTime(10000), 0x100).TestStarted().TestStateSinceHeight(2000) //所以下一个周期就是这样
.Mine(2051, TestTime(10010), 0).TestStarted().TestStateSinceHeight(2000) //51旧街区
.Mine(2950, TestTime(10020), 0x100).TestStarted().TestStateSinceHeight(2000) //899个新街区
.Mine(3000, TestTime(20000), 0).TestFailed().TestStateSinceHeight(3000) //50个旧街区（过去1000个街区中有899个）
                           .Mine(4000, TestTime(20010), 0x100).TestFailed().TestStateSinceHeight(3000)

//已定义->已启动->达到阈值时失败
                           .Reset().TestDefined().TestStateSinceHeight(0)
                           .Mine(1, TestTime(1), 0).TestDefined().TestStateSinceHeight(0)
.Mine(1000, TestTime(10000) - 1, 0x101).TestDefined().TestStateSinceHeight(0) //再过一秒钟，它就会被定义
.Mine(2000, TestTime(10000), 0x101).TestStarted().TestStateSinceHeight(2000) //所以下一个周期就是这样
.Mine(2999, TestTime(30000), 0x100).TestStarted().TestStateSinceHeight(2000) //999个新街区
.Mine(3000, TestTime(30000), 0x100).TestFailed().TestStateSinceHeight(3000) //1个新街区（过去1000个街区中有1000个是新的）
                           .Mine(3999, TestTime(30001), 0).TestFailed().TestStateSinceHeight(3000)
                           .Mine(4000, TestTime(30002), 0).TestFailed().TestStateSinceHeight(3000)
                           .Mine(14333, TestTime(30003), 0).TestFailed().TestStateSinceHeight(3000)
                           .Mine(24000, TestTime(40000), 0).TestFailed().TestStateSinceHeight(3000)

//defined->started->lockedin at the last minute->active
                           .Reset().TestDefined()
                           .Mine(1, TestTime(1), 0).TestDefined().TestStateSinceHeight(0)
.Mine(1000, TestTime(10000) - 1, 0x101).TestDefined().TestStateSinceHeight(0) //再过一秒钟，它就会被定义
.Mine(2000, TestTime(10000), 0x101).TestStarted().TestStateSinceHeight(2000) //所以下一个周期就是这样
.Mine(2050, TestTime(10010), 0x200).TestStarted().TestStateSinceHeight(2000) //50旧街区
.Mine(2950, TestTime(10020), 0x100).TestStarted().TestStateSinceHeight(2000) //900个新街区
.Mine(2999, TestTime(19999), 0x200).TestStarted().TestStateSinceHeight(2000) //49旧街区
.Mine(3000, TestTime(29999), 0x200).TestLockedIn().TestStateSinceHeight(3000) //1个旧街区（过去1000个街区中有900个）
                           .Mine(3999, TestTime(30001), 0).TestLockedIn().TestStateSinceHeight(3000)
                           .Mine(4000, TestTime(30002), 0).TestActive().TestStateSinceHeight(4000)
                           .Mine(14333, TestTime(30003), 0).TestActive().TestStateSinceHeight(4000)
                           .Mine(24000, TestTime(40000), 0).TestActive().TestStateSinceHeight(4000)

//定义的多个期间->开始的多个期间->失败
                           .Reset().TestDefined().TestStateSinceHeight(0)
                           .Mine(999, TestTime(999), 0).TestDefined().TestStateSinceHeight(0)
                           .Mine(1000, TestTime(1000), 0).TestDefined().TestStateSinceHeight(0)
                           .Mine(2000, TestTime(2000), 0).TestDefined().TestStateSinceHeight(0)
                           .Mine(3000, TestTime(10000), 0).TestStarted().TestStateSinceHeight(3000)
                           .Mine(4000, TestTime(10000), 0).TestStarted().TestStateSinceHeight(3000)
                           .Mine(5000, TestTime(10000), 0).TestStarted().TestStateSinceHeight(3000)
                           .Mine(6000, TestTime(20000), 0).TestFailed().TestStateSinceHeight(6000)
                           .Mine(7000, TestTime(20000), 0x100).TestFailed().TestStateSinceHeight(6000);
    }

//版本位部署的健全性检查
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    const Consensus::Params &mainnetParams = chainParams->GetConsensus();
    for (int i=0; i<(int) Consensus::MAX_VERSION_BITS_DEPLOYMENTS; i++) {
        uint32_t bitmask = VersionBitsMask(mainnetParams, static_cast<Consensus::DeploymentPos>(i));
//确保没有任何部署尝试设置无效位。
        BOOST_CHECK_EQUAL(bitmask & ~(uint32_t)VERSIONBITS_TOP_MASK, bitmask);

//使用验证不同部署的部署窗口
//同一位是不相交的。
//此测试可能需要在新部署时进行修改
//建议在
//那个软叉子的结束时间。（或者，结束时间
//激活的软叉可以稍后更改为更早以避免
//重叠）
        for (int j=i+1; j<(int) Consensus::MAX_VERSION_BITS_DEPLOYMENTS; j++) {
            if (VersionBitsMask(mainnetParams, static_cast<Consensus::DeploymentPos>(j)) == bitmask) {
                BOOST_CHECK(mainnetParams.vDeployments[j].nStartTime > mainnetParams.vDeployments[i].nTimeout ||
                        mainnetParams.vDeployments[i].nStartTime > mainnetParams.vDeployments[j].nTimeout);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(versionbits_computeblockversion)
{
//检查ComputeBlockVersion是否将正确设置适当的位
//在MNETNET上。
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    const Consensus::Params &mainnetParams = chainParams->GetConsensus();

//出于测试目的，使用testdummy部署。
    int64_t bit = mainnetParams.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit;
    int64_t nStartTime = mainnetParams.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime;
    int64_t nTimeout = mainnetParams.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout;

    assert(nStartTime < nTimeout);

//在第一个链中，测试该位是否由CBV设置，直到失败为止。
//在第二个链中，测试位在启动时由CBV设置，并且
//锁定，然后在激活时不再设置。
    VersionBitsTester firstChain, secondChain;

//在开始时间之前开始生成块
    int64_t nTime = nStartTime - 1;

//在链的MediantimePost越过StartTime之前，位
//不应设置。
    CBlockIndex *lastBlock = nullptr;
    lastBlock = firstChain.Mine(2016, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
    BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & (1<<bit), 0);

//在旧的时间开采更多的2011块，并检查CBV是否还没有设置钻头。
    for (int i=1; i<2012; i++) {
        lastBlock = firstChain.Mine(2016+i, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
//这是因为版本位\最后一个\旧的\块\版本发生了
//是4，我们测试的是28位。
        BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & (1<<bit), 0);
    }
//现在在开始的时候再挖5个街区——MTP应该还没有通过，所以
//CBV仍不应该设置位。
    nTime = nStartTime;
    for (int i=2012; i<=2016; i++) {
        lastBlock = firstChain.Mine(2016+i, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
        BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & (1<<bit), 0);
    }

//进入下一阶段，过渡到开始阶段，
    lastBlock = firstChain.Mine(6048, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
//所以computeBlockVersion现在应该设置位，
    BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1<<bit)) != 0);
//也应该使用versionBits_Top_位。
    BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & VERSIONBITS_TOP_MASK, VERSIONBITS_TOP_BITS);

//检查ComputeBlockVersion是否将位设置为直到超时
    nTime += 600;
int blocksToMine = 4032; //最多2个时间段的试块
    int nHeight = 6048;
//这些块都在达到超时之前。
    while (nTime < nTimeout && blocksToMine > 0) {
        lastBlock = firstChain.Mine(nHeight+1, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
        BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1<<bit)) != 0);
        BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & VERSIONBITS_TOP_MASK, VERSIONBITS_TOP_BITS);
        blocksToMine--;
        nTime += 600;
        nHeight += 1;
    }

    nTime = nTimeout;
//失败仅在周期结束时触发，因此应设置CBV
//直到周期转换的位。
    for (int i=0; i<2015; i++) {
        lastBlock = firstChain.Mine(nHeight+1, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
        BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1<<bit)) != 0);
        nHeight += 1;
    }
//下一个块应该触发不再设置位。
    lastBlock = firstChain.Mine(nHeight+1, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
    BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & (1<<bit), 0);

//一个新的链条：
//确认在锁定后位将被设置，然后停止设置。
//激活后。
    nTime = nStartTime;

//挖掘一个周期值的块，并检查该位是否为
//下一期。
    lastBlock = secondChain.Mine(2016, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
    BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1<<bit)) != 0);

//挖掘另一个时期价值块，发出新位的信号。
    lastBlock = secondChain.Mine(4032, nTime, VERSIONBITS_TOP_BITS | (1<<bit)).Tip();
//在每个块上设置位一段时间后，它应该已经锁定。
//不过，在激活之前，我们会再设置一段时间。
    BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1<<bit)) != 0);

//现在检查我们是否继续开采该区块，直到本期结束，以及
//然后在下一个周期开始时停止。
    lastBlock = secondChain.Mine(6047, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
    BOOST_CHECK((ComputeBlockVersion(lastBlock, mainnetParams) & (1<<bit)) != 0);
    lastBlock = secondChain.Mine(6048, nTime, VERSIONBITS_LAST_OLD_BLOCK_VERSION).Tip();
    BOOST_CHECK_EQUAL(ComputeBlockVersion(lastBlock, mainnetParams) & (1<<bit), 0);

//最后，验证软叉激活后，CBV不再使用
//版本位\上一个\块\版本。
//boost_check_equal（computeblockversion（lastblock，mainnetparams）&versionbits_top_mask，versionbits_top_bits）；
}


BOOST_AUTO_TEST_SUITE_END()
