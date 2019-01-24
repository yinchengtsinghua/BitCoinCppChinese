
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <boost/test/unit_test.hpp>

#include <test/test_bitcoin.h>
#include <wallet/test/init_test_fixture.h>

#include <init.h>
#include <walletinitinterface.h>
#include <wallet/wallet.h>


BOOST_FIXTURE_TEST_SUITE(init_tests, InitWalletDirTestingSetup)

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_default)
{
    SetWalletDir(m_walletdir_path_cases["default"]);
    bool result = m_chain_client->verify();
    BOOST_CHECK(result == true);
    fs::path walletdir = gArgs.GetArg("-walletdir", "");
    fs::path expected_path = fs::canonical(m_walletdir_path_cases["default"]);
    BOOST_CHECK(walletdir == expected_path);
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_custom)
{
    SetWalletDir(m_walletdir_path_cases["custom"]);
    bool result = m_chain_client->verify();
    BOOST_CHECK(result == true);
    fs::path walletdir = gArgs.GetArg("-walletdir", "");
    fs::path expected_path = fs::canonical(m_walletdir_path_cases["custom"]);
    BOOST_CHECK(walletdir == expected_path);
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_does_not_exist)
{
    SetWalletDir(m_walletdir_path_cases["nonexistent"]);
    bool result = m_chain_client->verify();
    BOOST_CHECK(result == false);
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_is_not_directory)
{
    SetWalletDir(m_walletdir_path_cases["file"]);
    bool result = m_chain_client->verify();
    BOOST_CHECK(result == false);
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_is_not_relative)
{
    SetWalletDir(m_walletdir_path_cases["relative"]);
    bool result = m_chain_client->verify();
    BOOST_CHECK(result == false);
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_no_trailing)
{
    SetWalletDir(m_walletdir_path_cases["trailing"]);
    bool result = m_chain_client->verify();
    BOOST_CHECK(result == true);
    fs::path walletdir = gArgs.GetArg("-walletdir", "");
    fs::path expected_path = fs::canonical(m_walletdir_path_cases["default"]);
    BOOST_CHECK(walletdir == expected_path);
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_no_trailing2)
{
    SetWalletDir(m_walletdir_path_cases["trailing2"]);
    bool result = m_chain_client->verify();
    BOOST_CHECK(result == true);
    fs::path walletdir = gArgs.GetArg("-walletdir", "");
    fs::path expected_path = fs::canonical(m_walletdir_path_cases["default"]);
    BOOST_CHECK(walletdir == expected_path);
}

BOOST_AUTO_TEST_SUITE_END()
