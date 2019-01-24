
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2015-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <chain.h>
#include <chainparams.h>
#include <pow.h>
#include <random.h>
#include <util/system.h>
#include <test/test_bitcoin.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(pow_tests, BasicTestingSetup)

/*无约束下一难度目标的测试计算*/
BOOST_AUTO_TEST_CASE(get_next_work)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
int64_t nLastRetargetTime = 1261130161; //第30240块
    CBlockIndex pindexLast;
    pindexLast.nHeight = 32255;
pindexLast.nTime = 1262152739;  //第32255块
    pindexLast.nBits = 0x1d00ffff;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1d00d86aU);
}

/*为下一项工作测试上界的约束*/
BOOST_AUTO_TEST_CASE(get_next_work_pow_limit)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
int64_t nLastRetargetTime = 1231006505; //第0块
    CBlockIndex pindexLast;
    pindexLast.nHeight = 2015;
pindexLast.nTime = 1233061996;  //第2015块
    pindexLast.nBits = 0x1d00ffff;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1d00ffffU);
}

/*测试实际所用时间的下限约束*/
BOOST_AUTO_TEST_CASE(get_next_work_lower_limit_actual)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
int64_t nLastRetargetTime = 1279008237; //第66528块
    CBlockIndex pindexLast;
    pindexLast.nHeight = 68543;
pindexLast.nTime = 1279297671;  //第68543块
    pindexLast.nBits = 0x1c05a3f4;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1c0168fdU);
}

/*在上限上测试实际所用时间的约束*/
BOOST_AUTO_TEST_CASE(get_next_work_upper_limit_actual)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
int64_t nLastRetargetTime = 1263163443; //注意：不是实际的阻塞时间
    CBlockIndex pindexLast;
    pindexLast.nHeight = 46367;
pindexLast.nTime = 1269211443;  //第46367块
    pindexLast.nBits = 0x1c387f6f;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1d00e1fdU);
}

BOOST_AUTO_TEST_CASE(GetBlockProofEquivalentTime_test)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    std::vector<CBlockIndex> blocks(10000);
    for (int i = 0; i < 10000; i++) {
        blocks[i].pprev = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = i;
        blocks[i].nTime = 1269211443 + i * chainParams->GetConsensus().nPowTargetSpacing;
        /*cks[i].nbits=0x207ffff；/*目标0x7fffff000…*/
        块[i].nchainwork=i？块[i-1].nchainwork+getblockproof（块[i-1]）：算术值256（0）；
    }

    对于（int j=0；j<1000；j++）
        cBlockIndex*p1=&blocks[不安全范围（10000）]；
        cBlockIndex*p2=&blocks[不安全范围（10000）]；
        cBlockIndex*p3=&blocks[不安全范围（10000）]；

        int64_t tdiff=getBlockProofEquivalentTime（*p1，*p2，*p3，chainParams->getconsensus（））；
        boost_check_equal（tdiff，p1->getBlockTime（）-p2->getBlockTime（））；
    }
}

Boost_Auto_测试套件_end（）
