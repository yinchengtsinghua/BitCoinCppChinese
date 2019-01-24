
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
//
#include <timedata.h>
#include <test/test_bitcoin.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(timedata_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(util_MedianFilter)
{
    CMedianFilter<int> filter(5, 15);

    BOOST_CHECK_EQUAL(filter.median(), 15);

filter.input(20); //〔15 20〕
    BOOST_CHECK_EQUAL(filter.median(), 17);

filter.input(30); //〔15 20 30〕
    BOOST_CHECK_EQUAL(filter.median(), 20);

filter.input(3); //〔3 15 20 20〕
    BOOST_CHECK_EQUAL(filter.median(), 17);

filter.input(7); //〔3 7 15 15 20〕
    BOOST_CHECK_EQUAL(filter.median(), 15);

filter.input(18); //〔3 7 18 18 20〕
    BOOST_CHECK_EQUAL(filter.median(), 18);

filter.input(0); //〔0 3 7 7 18〕
    BOOST_CHECK_EQUAL(filter.median(), 7);
}

BOOST_AUTO_TEST_SUITE_END()
