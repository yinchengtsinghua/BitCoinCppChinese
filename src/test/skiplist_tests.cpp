
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
#include <util/system.h>
#include <test/test_bitcoin.h>

#include <vector>

#include <boost/test/unit_test.hpp>

#define SKIPLIST_LENGTH 300000

BOOST_FIXTURE_TEST_SUITE(skiplist_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(skiplist_test)
{
    std::vector<CBlockIndex> vIndex(SKIPLIST_LENGTH);

    for (int i=0; i<SKIPLIST_LENGTH; i++) {
        vIndex[i].nHeight = i;
        vIndex[i].pprev = (i == 0) ? nullptr : &vIndex[i - 1];
        vIndex[i].BuildSkip();
    }

    for (int i=0; i<SKIPLIST_LENGTH; i++) {
        if (i > 0) {
            BOOST_CHECK(vIndex[i].pskip == &vIndex[vIndex[i].pskip->nHeight]);
            BOOST_CHECK(vIndex[i].pskip->nHeight < i);
        } else {
            BOOST_CHECK(vIndex[i].pskip == nullptr);
        }
    }

    for (int i=0; i < 1000; i++) {
        int from = InsecureRandRange(SKIPLIST_LENGTH - 1);
        int to = InsecureRandRange(from + 1);

        BOOST_CHECK(vIndex[SKIPLIST_LENGTH - 1].GetAncestor(from) == &vIndex[from]);
        BOOST_CHECK(vIndex[from].GetAncestor(to) == &vIndex[to]);
        BOOST_CHECK(vIndex[from].GetAncestor(0) == vIndex.data());
    }
}

BOOST_AUTO_TEST_CASE(getlocator_test)
{
//建造一条100000个街区长的主链。
    std::vector<uint256> vHashMain(100000);
    std::vector<CBlockIndex> vBlocksMain(100000);
    for (unsigned int i=0; i<vBlocksMain.size(); i++) {
vHashMain[i] = ArithToUint256(i); //将散列值设置为高度，这样我们可以快速检查距离。
        vBlocksMain[i].nHeight = i;
        vBlocksMain[i].pprev = i ? &vBlocksMain[i - 1] : nullptr;
        vBlocksMain[i].phashBlock = &vHashMain[i];
        vBlocksMain[i].BuildSkip();
        BOOST_CHECK_EQUAL((int)UintToArith256(vBlocksMain[i].GetBlockHash()).GetLow64(), vBlocksMain[i].nHeight);
        BOOST_CHECK(vBlocksMain[i].pprev == nullptr || vBlocksMain[i].nHeight == vBlocksMain[i].pprev->nHeight + 1);
    }

//建立一个分支，在4999950000个街区处分开。
    std::vector<uint256> vHashSide(50000);
    std::vector<CBlockIndex> vBlocksSide(50000);
    for (unsigned int i=0; i<vBlocksSide.size(); i++) {
vHashSide[i] = ArithToUint256(i + 50000 + (arith_uint256(1) << 128)); //将1<<128添加到哈希中，因此getlow64（）仍然返回高度。
        vBlocksSide[i].nHeight = i + 50000;
        vBlocksSide[i].pprev = i ? &vBlocksSide[i - 1] : (vBlocksMain.data()+49999);
        vBlocksSide[i].phashBlock = &vHashSide[i];
        vBlocksSide[i].BuildSkip();
        BOOST_CHECK_EQUAL((int)UintToArith256(vBlocksSide[i].GetBlockHash()).GetLow64(), vBlocksSide[i].nHeight);
        BOOST_CHECK(vBlocksSide[i].pprev == nullptr || vBlocksSide[i].nHeight == vBlocksSide[i].pprev->nHeight + 1);
    }

//为总分行建立一个CChain。
    CChain chain;
    chain.SetTip(&vBlocksMain.back());

//测试100个定位器的随机起始点。
    for (int n=0; n<100; n++) {
        int r = InsecureRandRange(150000);
        CBlockIndex* tip = (r < 100000) ? &vBlocksMain[r] : &vBlocksSide[r - 100000];
        CBlockLocator locator = chain.GetLocator(tip);

//第一个结果必须是块本身，最后一个结果必须是起源。
        BOOST_CHECK(locator.vHave.front() == tip->GetBlockHash());
        BOOST_CHECK(locator.vHave.back() == vBlocksMain[0].GetBlockHash());

//条目1到11（包括11）每退一步。
        for (unsigned int i = 1; i < 12 && i < locator.vHave.size() - 1; i++) {
            BOOST_CHECK_EQUAL(UintToArith256(locator.vHave[i]).GetLow64(), tip->nHeight - i);
        }

//后面的（不包括最后一个）是指数级的。
        unsigned int dist = 2;
        for (unsigned int i = 12; i < locator.vHave.size() - 1; i++) {
            BOOST_CHECK_EQUAL(UintToArith256(locator.vHave[i - 1]).GetLow64() - UintToArith256(locator.vHave[i]).GetLow64(), dist);
            dist *= 2;
        }
    }
}

BOOST_AUTO_TEST_CASE(findearliestatleast_test)
{
    std::vector<uint256> vHashMain(100000);
    std::vector<CBlockIndex> vBlocksMain(100000);
    for (unsigned int i=0; i<vBlocksMain.size(); i++) {
vHashMain[i] = ArithToUint256(i); //将哈希设置为高度
        vBlocksMain[i].nHeight = i;
        vBlocksMain[i].pprev = i ? &vBlocksMain[i - 1] : nullptr;
        vBlocksMain[i].phashBlock = &vHashMain[i];
        vBlocksMain[i].BuildSkip();
        if (i < 10) {
            vBlocksMain[i].nTime = i;
            vBlocksMain[i].nTimeMax = i;
        } else {
//随机选择范围内的内容[mtp，mtp*2]
            int64_t medianTimePast = vBlocksMain[i].GetMedianTimePast();
            int r = InsecureRandRange(medianTimePast);
            vBlocksMain[i].nTime = r + medianTimePast;
            vBlocksMain[i].nTimeMax = std::max(vBlocksMain[i].nTime, vBlocksMain[i-1].nTimeMax);
        }
    }
//检查是否正确设置了ntimemax。
    unsigned int curTimeMax = 0;
    for (unsigned int i=0; i<vBlocksMain.size(); ++i) {
        curTimeMax = std::max(curTimeMax, vBlocksMain[i].nTime);
        BOOST_CHECK(curTimeMax == vBlocksMain[i].nTimeMax);
    }

//为总分行建立一个CChain。
    CChain chain;
    chain.SetTip(&vBlocksMain.back());

//验证findearliestatleast是否正确。
    for (unsigned int i=0; i<10000; ++i) {
//在vblocksmain中选择一个随机元素。
        int r = InsecureRandRange(vBlocksMain.size());
        int64_t test_time = vBlocksMain[r].nTime;
        CBlockIndex *ret = chain.FindEarliestAtLeast(test_time);
        BOOST_CHECK(ret->nTimeMax >= test_time);
        BOOST_CHECK((ret->pprev==nullptr) || ret->pprev->nTimeMax < test_time);
        BOOST_CHECK(vBlocksMain[r].GetAncestor(ret->nHeight) == ret);
    }
}

BOOST_AUTO_TEST_CASE(findearliestatleast_edge_test)
{
    std::list<CBlockIndex> blocks;
    for (const unsigned int timeMax : {100, 100, 100, 200, 200, 200, 300, 300, 300}) {
        CBlockIndex* prev = blocks.empty() ? nullptr : &blocks.back();
        blocks.emplace_back();
        blocks.back().nHeight = prev ? prev->nHeight + 1 : 0;
        blocks.back().pprev = prev;
        blocks.back().BuildSkip();
        blocks.back().nTimeMax = timeMax;
    }

    CChain chain;
    chain.SetTip(&blocks.back());

    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(50)->nHeight, 0);
    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(100)->nHeight, 0);
    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(150)->nHeight, 3);
    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(200)->nHeight, 3);
    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(250)->nHeight, 6);
    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(300)->nHeight, 6);
    BOOST_CHECK(!chain.FindEarliestAtLeast(350));

    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(0)->nHeight, 0);
    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(-1)->nHeight, 0);

    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(std::numeric_limits<int64_t>::min())->nHeight, 0);
    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(-int64_t(std::numeric_limits<unsigned int>::max()) - 1)->nHeight, 0);
    BOOST_CHECK(!chain.FindEarliestAtLeast(std::numeric_limits<int64_t>::max()));
    BOOST_CHECK(!chain.FindEarliestAtLeast(std::numeric_limits<unsigned int>::max()));
    BOOST_CHECK(!chain.FindEarliestAtLeast(int64_t(std::numeric_limits<unsigned int>::max()) + 1));
}

BOOST_AUTO_TEST_SUITE_END()
