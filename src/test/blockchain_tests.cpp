
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
#include <boost/test/unit_test.hpp>

#include <stdlib.h>

#include <rpc/blockchain.h>
#include <test/test_bitcoin.h>

/*双打之间的均等是不精确的。应进行比较
 *公差阈值较小，而不是完全相等。
 **/

static bool DoubleEquals(double a, double b, double epsilon)
{
    return std::abs(a - b) < epsilon;
}

static CBlockIndex* CreateBlockIndexWithNbits(uint32_t nbits)
{
    CBlockIndex* block_index = new CBlockIndex();
    block_index->nHeight = 46367;
    block_index->nTime = 1269211443;
    block_index->nBits = nbits;
    return block_index;
}

static void RejectDifficultyMismatch(double difficulty, double expected_difficulty) {
     BOOST_CHECK_MESSAGE(
        DoubleEquals(difficulty, expected_difficulty, 0.00001),
        "Difficulty was " + std::to_string(difficulty)
            + " but was expected to be " + std::to_string(expected_difficulty));
}

/*给定一个具有所提供nbits的blockindex，
 *验证预期的困难结果。
 **/

static void TestDifficulty(uint32_t nbits, double expected_difficulty)
{
    CBlockIndex* block_index = CreateBlockIndexWithNbits(nbits);
    double difficulty = GetDifficulty(block_index);
    delete block_index;

    RejectDifficultyMismatch(difficulty, expected_difficulty);
}

BOOST_FIXTURE_TEST_SUITE(blockchain_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(get_difficulty_for_very_low_target)
{
    TestDifficulty(0x1f111111, 0.000001);
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_low_target)
{
    TestDifficulty(0x1ef88f6f, 0.000016);
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_mid_target)
{
    TestDifficulty(0x1df88f6f, 0.004023);
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_high_target)
{
    TestDifficulty(0x1cf88f6f, 1.029916);
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_very_high_target)
{
    TestDifficulty(0x12345678, 5913134931067755359633408.0);
}

BOOST_AUTO_TEST_SUITE_END()
