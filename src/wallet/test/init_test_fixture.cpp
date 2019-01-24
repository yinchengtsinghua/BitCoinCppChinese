
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

#include <fs.h>

#include <wallet/test/init_test_fixture.h>

InitWalletDirTestingSetup::InitWalletDirTestingSetup(const std::string& chainName): BasicTestingSetup(chainName)
{
    m_chain_client = MakeWalletClient(*m_chain, {});

    std::string sep;
    sep += fs::path::preferred_separator;

    m_datadir = SetDataDir("tempdir");
    m_cwd = fs::current_path();

    m_walletdir_path_cases["default"] = m_datadir / "wallets";
    m_walletdir_path_cases["custom"] = m_datadir / "my_wallets";
    m_walletdir_path_cases["nonexistent"] = m_datadir / "path_does_not_exist";
    m_walletdir_path_cases["file"] = m_datadir / "not_a_directory.dat";
    m_walletdir_path_cases["trailing"] = m_datadir / "wallets" / sep;
    m_walletdir_path_cases["trailing2"] = m_datadir / "wallets" / sep / sep;

    fs::current_path(m_datadir);
    m_walletdir_path_cases["relative"] = "wallets";

    fs::create_directories(m_walletdir_path_cases["default"]);
    fs::create_directories(m_walletdir_path_cases["custom"]);
    fs::create_directories(m_walletdir_path_cases["relative"]);
    std::ofstream f(m_walletdir_path_cases["file"].BOOST_FILESYSTEM_C_STR);
    f.close();
}

InitWalletDirTestingSetup::~InitWalletDirTestingSetup()
{
    fs::current_path(m_cwd);
}

void InitWalletDirTestingSetup::SetWalletDir(const fs::path& walletdir_path)
{
    gArgs.ForceSetArg("-walletdir", walletdir_path.string());
}