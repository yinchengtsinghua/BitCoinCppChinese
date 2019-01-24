
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
#include <fs.h>
#include <test/test_bitcoin.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(fs_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(fsbridge_fstream)
{
    fs::path tmpfolder = SetDataDir("fsbridge_fstream");
//tmpfile1应与tmpfile2相同
    fs::path tmpfile1 = tmpfolder / "fs_tests_₿_🏃";
    fs::path tmpfile2 = tmpfolder / L"fs_tests_₿_🏃";
    {
        fsbridge::ofstream file(tmpfile1);
        file << "bitcoin";
    }
    {
        fsbridge::ifstream file(tmpfile2);
        std::string input_buffer;
        file >> input_buffer;
        BOOST_CHECK_EQUAL(input_buffer, "bitcoin");
    }
    {
        fsbridge::ifstream file(tmpfile1, std::ios_base::in | std::ios_base::ate);
        std::string input_buffer;
        file >> input_buffer;
        BOOST_CHECK_EQUAL(input_buffer, "");
    }
    {
        fsbridge::ofstream file(tmpfile2, std::ios_base::out | std::ios_base::app);
        file << "tests";
    }
    {
        fsbridge::ifstream file(tmpfile1);
        std::string input_buffer;
        file >> input_buffer;
        BOOST_CHECK_EQUAL(input_buffer, "bitcointests");
    }
    {
        fsbridge::ofstream file(tmpfile2, std::ios_base::out | std::ios_base::trunc);
        file << "bitcoin";
    }
    {
        fsbridge::ifstream file(tmpfile1);
        std::string input_buffer;
        file >> input_buffer;
        BOOST_CHECK_EQUAL(input_buffer, "bitcoin");
    }
}

BOOST_AUTO_TEST_SUITE_END()
