
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

#include <util/system.h>

#include <clientversion.h>
#include <primitives/transaction.h>
#include <sync.h>
#include <util/strencodings.h>
#include <util/moneystr.h>
#include <test/test_bitcoin.h>

#include <stdint.h>
#include <vector>
#ifndef WIN32
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(util_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(util_criticalsection)
{
    CCriticalSection cs;

    do {
        LOCK(cs);
        break;

        BOOST_ERROR("break was swallowed!");
    } while(0);

    do {
        TRY_LOCK(cs, lockTest);
        if (lockTest)
            break;

        BOOST_ERROR("break was swallowed!");
    } while(0);
}

static const unsigned char ParseHex_expected[65] = {
    0x04, 0x67, 0x8a, 0xfd, 0xb0, 0xfe, 0x55, 0x48, 0x27, 0x19, 0x67, 0xf1, 0xa6, 0x71, 0x30, 0xb7,
    0x10, 0x5c, 0xd6, 0xa8, 0x28, 0xe0, 0x39, 0x09, 0xa6, 0x79, 0x62, 0xe0, 0xea, 0x1f, 0x61, 0xde,
    0xb6, 0x49, 0xf6, 0xbc, 0x3f, 0x4c, 0xef, 0x38, 0xc4, 0xf3, 0x55, 0x04, 0xe5, 0x1e, 0xc1, 0x12,
    0xde, 0x5c, 0x38, 0x4d, 0xf7, 0xba, 0x0b, 0x8d, 0x57, 0x8a, 0x4c, 0x70, 0x2b, 0x6b, 0xf1, 0x1d,
    0x5f
};
BOOST_AUTO_TEST_CASE(util_ParseHex)
{
    std::vector<unsigned char> result;
    std::vector<unsigned char> expected(ParseHex_expected, ParseHex_expected + sizeof(ParseHex_expected));
//基本测试向量
    result = ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f");
    BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());

//必须支持字节之间的空格
    result = ParseHex("12 34 56 78");
    BOOST_CHECK(result.size() == 4 && result[0] == 0x12 && result[1] == 0x34 && result[2] == 0x56 && result[3] == 0x78);

//必须支持前导空格（用于Berkeleyenvironment:：Salvage）
    result = ParseHex(" 89 34 56 78");
    BOOST_CHECK(result.size() == 4 && result[0] == 0x89 && result[1] == 0x34 && result[2] == 0x56 && result[3] == 0x78);

//以无效值停止分析
    result = ParseHex("1234 invalid 1234");
    BOOST_CHECK(result.size() == 2 && result[0] == 0x12 && result[1] == 0x34);
}

BOOST_AUTO_TEST_CASE(util_HexStr)
{
    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected + sizeof(ParseHex_expected)),
        "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected + 5, true),
        "04 67 8a fd b0");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected + sizeof(ParseHex_expected),
               ParseHex_expected + sizeof(ParseHex_expected)),
        "");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected + sizeof(ParseHex_expected),
               ParseHex_expected + sizeof(ParseHex_expected), true),
        "");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected),
        "");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected, true),
        "");

    std::vector<unsigned char> ParseHex_vec(ParseHex_expected, ParseHex_expected + 5);

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_vec, true),
        "04 67 8a fd b0");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_vec.rbegin(), ParseHex_vec.rend()),
        "b0fd8a6704"
    );

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_vec.rbegin(), ParseHex_vec.rend(), true),
        "b0 fd 8a 67 04"
    );

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected)),
        ""
    );

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected), true),
        ""
    );

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected + 1),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected)),
        "04"
    );

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected + 1),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected), true),
        "04"
    );

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected + 5),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected)),
        "b0fd8a6704"
    );

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected + 5),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected), true),
        "b0 fd 8a 67 04"
    );

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected + 65),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected)),
        "5f1df16b2b704c8a578d0bbaf74d385cde12c11ee50455f3c438ef4c3fbcf649b6de611feae06279a60939e028a8d65c10b73071a6f16719274855feb0fd8a6704"
    );
}


BOOST_AUTO_TEST_CASE(util_FormatISO8601DateTime)
{
    BOOST_CHECK_EQUAL(FormatISO8601DateTime(1317425777), "2011-09-30T23:36:17Z");
}

BOOST_AUTO_TEST_CASE(util_FormatISO8601Date)
{
    BOOST_CHECK_EQUAL(FormatISO8601Date(1317425777), "2011-09-30");
}

BOOST_AUTO_TEST_CASE(util_FormatISO8601Time)
{
    BOOST_CHECK_EQUAL(FormatISO8601Time(1317425777), "23:36:17Z");
}

struct TestArgsManager : public ArgsManager
{
    TestArgsManager() { m_network_only_args.clear(); }
    std::map<std::string, std::vector<std::string> >& GetOverrideArgs() { return m_override_args; }
    std::map<std::string, std::vector<std::string> >& GetConfigArgs() { return m_config_args; }
    void ReadConfigString(const std::string str_config)
    {
        std::istringstream streamConfig(str_config);
        {
            LOCK(cs_args);
            m_config_args.clear();
        }
        std::string error;
        BOOST_REQUIRE(ReadConfigStream(streamConfig, error));
    }
    void SetNetworkOnlyArg(const std::string arg)
    {
        LOCK(cs_args);
        m_network_only_args.insert(arg);
    }
    void SetupArgs(int argv, const char* args[])
    {
        for (int i = 0; i < argv; ++i) {
            AddArg(args[i], "", false, OptionsCategory::OPTIONS);
        }
    }
};

BOOST_AUTO_TEST_CASE(util_ParseParameters)
{
    TestArgsManager testArgs;
    const char* avail_args[] = {"-a", "-b", "-ccc", "-d"};
    const char *argv_test[] = {"-ignored", "-a", "-b", "-ccc=argument", "-ccc=multiple", "f", "-d=e"};

    std::string error;
    testArgs.SetupArgs(4, avail_args);
    BOOST_CHECK(testArgs.ParseParameters(0, (char**)argv_test, error));
    BOOST_CHECK(testArgs.GetOverrideArgs().empty() && testArgs.GetConfigArgs().empty());

    BOOST_CHECK(testArgs.ParseParameters(1, (char**)argv_test, error));
    BOOST_CHECK(testArgs.GetOverrideArgs().empty() && testArgs.GetConfigArgs().empty());

    BOOST_CHECK(testArgs.ParseParameters(7, (char**)argv_test, error));
//期望值：-忽略（程序名参数），
//-a、-b和-ccc以map结尾，-d被忽略，因为它在后面
//非选项参数（非GNU选项解析）
    BOOST_CHECK(testArgs.GetOverrideArgs().size() == 3 && testArgs.GetConfigArgs().empty());
    BOOST_CHECK(testArgs.IsArgSet("-a") && testArgs.IsArgSet("-b") && testArgs.IsArgSet("-ccc")
                && !testArgs.IsArgSet("f") && !testArgs.IsArgSet("-d"));
    BOOST_CHECK(testArgs.GetOverrideArgs().count("-a") && testArgs.GetOverrideArgs().count("-b") && testArgs.GetOverrideArgs().count("-ccc")
                && !testArgs.GetOverrideArgs().count("f") && !testArgs.GetOverrideArgs().count("-d"));

    BOOST_CHECK(testArgs.GetOverrideArgs()["-a"].size() == 1);
    BOOST_CHECK(testArgs.GetOverrideArgs()["-a"].front() == "");
    BOOST_CHECK(testArgs.GetOverrideArgs()["-ccc"].size() == 2);
    BOOST_CHECK(testArgs.GetOverrideArgs()["-ccc"].front() == "argument");
    BOOST_CHECK(testArgs.GetOverrideArgs()["-ccc"].back() == "multiple");
    BOOST_CHECK(testArgs.GetArgs("-ccc").size() == 2);
}

BOOST_AUTO_TEST_CASE(util_GetBoolArg)
{
    TestArgsManager testArgs;
    const char* avail_args[] = {"-a", "-b", "-c", "-d", "-e", "-f"};
    const char *argv_test[] = {
        "ignored", "-a", "-nob", "-c=0", "-d=1", "-e=false", "-f=true"};
    std::string error;
    testArgs.SetupArgs(6, avail_args);
    BOOST_CHECK(testArgs.ParseParameters(7, (char**)argv_test, error));

//每个字母都应该设置。
    for (const char opt : "abcdef")
        BOOST_CHECK(testArgs.IsArgSet({'-', opt}) || !opt);

//地图上不应该有其他东西
    BOOST_CHECK(testArgs.GetOverrideArgs().size() == 6 &&
                testArgs.GetConfigArgs().empty());

//-no前缀在进入时应该被去掉。
    BOOST_CHECK(!testArgs.IsArgSet("-nob"));

//-b选项被标记为否定，而其他任何选项都不是否定的。
    BOOST_CHECK(testArgs.IsArgNegated("-b"));
    BOOST_CHECK(!testArgs.IsArgNegated("-a"));

//检查期望值。
    BOOST_CHECK(testArgs.GetBoolArg("-a", false) == true);
    BOOST_CHECK(testArgs.GetBoolArg("-b", true) == false);
    BOOST_CHECK(testArgs.GetBoolArg("-c", true) == false);
    BOOST_CHECK(testArgs.GetBoolArg("-d", false) == true);
    BOOST_CHECK(testArgs.GetBoolArg("-e", true) == false);
    BOOST_CHECK(testArgs.GetBoolArg("-f", true) == false);
}

BOOST_AUTO_TEST_CASE(util_GetBoolArgEdgeCases)
{
//测试一些可怕的边缘情况，希望没有用户会锻炼。
    TestArgsManager testArgs;

//帕拉姆斯试验
    const char* avail_args[] = {"-foo", "-bar"};
    const char *argv_test[] = {"ignored", "-nofoo", "-foo", "-nobar=0"};
    testArgs.SetupArgs(2, avail_args);
    std::string error;
    BOOST_CHECK(testArgs.ParseParameters(4, (char**)argv_test, error));

//这被传递了两次，第二次覆盖了负设置。
    BOOST_CHECK(!testArgs.IsArgNegated("-foo"));
    BOOST_CHECK(testArgs.GetArg("-foo", "xxx") == "");

//双负是一个正的，没有标记为负。
    BOOST_CHECK(!testArgs.IsArgNegated("-bar"));
    BOOST_CHECK(testArgs.GetArg("-bar", "xxx") == "1");

//配置测试
    const char *conf_test = "nofoo=1\nfoo=1\nnobar=0\n";
    BOOST_CHECK(testArgs.ParseParameters(1, (char**)argv_test, error));
    testArgs.ReadConfigString(conf_test);

//这被传递了两次，第二次覆盖了负设置，
//和价值。
    BOOST_CHECK(!testArgs.IsArgNegated("-foo"));
    BOOST_CHECK(testArgs.GetArg("-foo", "xxx") == "1");

//双负数是一个正数，不算作负数。
    BOOST_CHECK(!testArgs.IsArgNegated("-bar"));
    BOOST_CHECK(testArgs.GetArg("-bar", "xxx") == "1");

//组合试验
    const char *combo_test_args[] = {"ignored", "-nofoo", "-bar"};
    const char *combo_test_conf = "foo=1\nnobar=1\n";
    BOOST_CHECK(testArgs.ParseParameters(3, (char**)combo_test_args, error));
    testArgs.ReadConfigString(combo_test_conf);

//命令行替代，但不删除旧设置
    BOOST_CHECK(testArgs.IsArgNegated("-foo"));
    BOOST_CHECK(testArgs.GetArg("-foo", "xxx") == "0");
    BOOST_CHECK(testArgs.GetArgs("-foo").size() == 0);

//命令行替代，但不删除旧设置
    BOOST_CHECK(!testArgs.IsArgNegated("-bar"));
    BOOST_CHECK(testArgs.GetArg("-bar", "xxx") == "");
    BOOST_CHECK(testArgs.GetArgs("-bar").size() == 1
                && testArgs.GetArgs("-bar").front() == "");
}

BOOST_AUTO_TEST_CASE(util_ReadConfigStream)
{
    const char *str_config =
       "a=\n"
       "b=1\n"
       "ccc=argument\n"
       "ccc=multiple\n"
       "d=e\n"
       "nofff=1\n"
       "noggg=0\n"
       "h=1\n"
       "noh=1\n"
       "noi=1\n"
       "i=1\n"
       "sec1.ccc=extend1\n"
       "\n"
       "[sec1]\n"
       "ccc=extend2\n"
       "d=eee\n"
       "h=1\n"
       "[sec2]\n"
       "ccc=extend3\n"
       "iii=2\n";

    TestArgsManager test_args;
    const char* avail_args[] = {"-a", "-b", "-ccc", "-d", "-e", "-fff", "-ggg", "-h", "-i", "-iii"};
    test_args.SetupArgs(10, avail_args);

    test_args.ReadConfigString(str_config);
//期望值：A、B、CCC、D、FFF、GGG、H、I以地图结尾
//同样，sec1.ccc，sec1.d，sec1.h，sec2.ccc，sec2.iii

    BOOST_CHECK(test_args.GetOverrideArgs().empty());
    BOOST_CHECK(test_args.GetConfigArgs().size() == 13);

    BOOST_CHECK(test_args.GetConfigArgs().count("-a")
                && test_args.GetConfigArgs().count("-b")
                && test_args.GetConfigArgs().count("-ccc")
                && test_args.GetConfigArgs().count("-d")
                && test_args.GetConfigArgs().count("-fff")
                && test_args.GetConfigArgs().count("-ggg")
                && test_args.GetConfigArgs().count("-h")
                && test_args.GetConfigArgs().count("-i")
               );
    BOOST_CHECK(test_args.GetConfigArgs().count("-sec1.ccc")
                && test_args.GetConfigArgs().count("-sec1.h")
                && test_args.GetConfigArgs().count("-sec2.ccc")
                && test_args.GetConfigArgs().count("-sec2.iii")
               );

    BOOST_CHECK(test_args.IsArgSet("-a")
                && test_args.IsArgSet("-b")
                && test_args.IsArgSet("-ccc")
                && test_args.IsArgSet("-d")
                && test_args.IsArgSet("-fff")
                && test_args.IsArgSet("-ggg")
                && test_args.IsArgSet("-h")
                && test_args.IsArgSet("-i")
                && !test_args.IsArgSet("-zzz")
                && !test_args.IsArgSet("-iii")
               );

    BOOST_CHECK(test_args.GetArg("-a", "xxx") == ""
                && test_args.GetArg("-b", "xxx") == "1"
                && test_args.GetArg("-ccc", "xxx") == "argument"
                && test_args.GetArg("-d", "xxx") == "e"
                && test_args.GetArg("-fff", "xxx") == "0"
                && test_args.GetArg("-ggg", "xxx") == "1"
                && test_args.GetArg("-h", "xxx") == "0"
                && test_args.GetArg("-i", "xxx") == "1"
                && test_args.GetArg("-zzz", "xxx") == "xxx"
                && test_args.GetArg("-iii", "xxx") == "xxx"
               );

    for (const bool def : {false, true}) {
        BOOST_CHECK(test_args.GetBoolArg("-a", def)
                     && test_args.GetBoolArg("-b", def)
                     && !test_args.GetBoolArg("-ccc", def)
                     && !test_args.GetBoolArg("-d", def)
                     && !test_args.GetBoolArg("-fff", def)
                     && test_args.GetBoolArg("-ggg", def)
                     && !test_args.GetBoolArg("-h", def)
                     && test_args.GetBoolArg("-i", def)
                     && test_args.GetBoolArg("-zzz", def) == def
                     && test_args.GetBoolArg("-iii", def) == def
                   );
    }

    BOOST_CHECK(test_args.GetArgs("-a").size() == 1
                && test_args.GetArgs("-a").front() == "");
    BOOST_CHECK(test_args.GetArgs("-b").size() == 1
                && test_args.GetArgs("-b").front() == "1");
    BOOST_CHECK(test_args.GetArgs("-ccc").size() == 2
                && test_args.GetArgs("-ccc").front() == "argument"
                && test_args.GetArgs("-ccc").back() == "multiple");
    BOOST_CHECK(test_args.GetArgs("-fff").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-nofff").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-ggg").size() == 1
                && test_args.GetArgs("-ggg").front() == "1");
    BOOST_CHECK(test_args.GetArgs("-noggg").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-h").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-noh").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-i").size() == 1
                && test_args.GetArgs("-i").front() == "1");
    BOOST_CHECK(test_args.GetArgs("-noi").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-zzz").size() == 0);

    BOOST_CHECK(!test_args.IsArgNegated("-a"));
    BOOST_CHECK(!test_args.IsArgNegated("-b"));
    BOOST_CHECK(!test_args.IsArgNegated("-ccc"));
    BOOST_CHECK(!test_args.IsArgNegated("-d"));
    BOOST_CHECK(test_args.IsArgNegated("-fff"));
    BOOST_CHECK(!test_args.IsArgNegated("-ggg"));
BOOST_CHECK(test_args.IsArgNegated("-h")); //最后一个设置优先
BOOST_CHECK(!test_args.IsArgNegated("-i")); //最后一个设置优先
    BOOST_CHECK(!test_args.IsArgNegated("-zzz"));

//试验段工程
    test_args.SelectConfigNetwork("sec1");

//与原件相同
    BOOST_CHECK(test_args.GetArg("-a", "xxx") == ""
                && test_args.GetArg("-b", "xxx") == "1"
                && test_args.GetArg("-fff", "xxx") == "0"
                && test_args.GetArg("-ggg", "xxx") == "1"
                && test_args.GetArg("-zzz", "xxx") == "xxx"
                && test_args.GetArg("-iii", "xxx") == "xxx"
               );
//D被重写
    BOOST_CHECK(test_args.GetArg("-d", "xxx") == "eee");
//区段特定设置
    BOOST_CHECK(test_args.GetArg("-h", "xxx") == "1");
//对于多个值，节优先
    BOOST_CHECK(test_args.GetArg("-ccc", "xxx") == "extend1");
//检查多个值是否有效
    const std::vector<std::string> sec1_ccc_expected = {"extend1","extend2","argument","multiple"};
    const auto& sec1_ccc_res = test_args.GetArgs("-ccc");
    BOOST_CHECK_EQUAL_COLLECTIONS(sec1_ccc_res.begin(), sec1_ccc_res.end(), sec1_ccc_expected.begin(), sec1_ccc_expected.end());

    test_args.SelectConfigNetwork("sec2");

//与原件相同
    BOOST_CHECK(test_args.GetArg("-a", "xxx") == ""
                && test_args.GetArg("-b", "xxx") == "1"
                && test_args.GetArg("-d", "xxx") == "e"
                && test_args.GetArg("-fff", "xxx") == "0"
                && test_args.GetArg("-ggg", "xxx") == "1"
                && test_args.GetArg("-zzz", "xxx") == "xxx"
                && test_args.GetArg("-h", "xxx") == "0"
               );
//区段特定设置
    BOOST_CHECK(test_args.GetArg("-iii", "xxx") == "2");
//对于多个值，节优先
    BOOST_CHECK(test_args.GetArg("-ccc", "xxx") == "extend3");
//检查多个值是否有效
    const std::vector<std::string> sec2_ccc_expected = {"extend3","argument","multiple"};
    const auto& sec2_ccc_res = test_args.GetArgs("-ccc");
    BOOST_CHECK_EQUAL_COLLECTIONS(sec2_ccc_res.begin(), sec2_ccc_res.end(), sec2_ccc_expected.begin(), sec2_ccc_expected.end());

//仅测试部分选项

    test_args.SetNetworkOnlyArg("-d");
    test_args.SetNetworkOnlyArg("-ccc");
    test_args.SetNetworkOnlyArg("-h");

    test_args.SelectConfigNetwork(CBaseChainParams::MAIN);
    BOOST_CHECK(test_args.GetArg("-d", "xxx") == "e");
    BOOST_CHECK(test_args.GetArgs("-ccc").size() == 2);
    BOOST_CHECK(test_args.GetArg("-h", "xxx") == "0");

    test_args.SelectConfigNetwork("sec1");
    BOOST_CHECK(test_args.GetArg("-d", "xxx") == "eee");
    BOOST_CHECK(test_args.GetArgs("-d").size() == 1);
    BOOST_CHECK(test_args.GetArgs("-ccc").size() == 2);
    BOOST_CHECK(test_args.GetArg("-h", "xxx") == "1");

    test_args.SelectConfigNetwork("sec2");
    BOOST_CHECK(test_args.GetArg("-d", "xxx") == "xxx");
    BOOST_CHECK(test_args.GetArgs("-d").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-ccc").size() == 1);
    BOOST_CHECK(test_args.GetArg("-h", "xxx") == "0");
}

BOOST_AUTO_TEST_CASE(util_GetArg)
{
    TestArgsManager testArgs;
    testArgs.GetOverrideArgs().clear();
    testArgs.GetOverrideArgs()["strtest1"] = {"string..."};
//strtest2故意未定义
    testArgs.GetOverrideArgs()["inttest1"] = {"12345"};
    testArgs.GetOverrideArgs()["inttest2"] = {"81985529216486895"};
//IntTest3故意未定义
    testArgs.GetOverrideArgs()["booltest1"] = {""};
//booltest2故意未定义
    testArgs.GetOverrideArgs()["booltest3"] = {"0"};
    testArgs.GetOverrideArgs()["booltest4"] = {"1"};

//优先顺序
    testArgs.GetOverrideArgs()["pritest1"] = {"a", "b"};
    testArgs.GetConfigArgs()["pritest2"] = {"a", "b"};
    testArgs.GetOverrideArgs()["pritest3"] = {"a"};
    testArgs.GetConfigArgs()["pritest3"] = {"b"};
    testArgs.GetOverrideArgs()["pritest4"] = {"a","b"};
    testArgs.GetConfigArgs()["pritest4"] = {"c","d"};

    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest1", "default"), "string...");
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest2", "default"), "default");
    BOOST_CHECK_EQUAL(testArgs.GetArg("inttest1", -1), 12345);
    BOOST_CHECK_EQUAL(testArgs.GetArg("inttest2", -1), 81985529216486895LL);
    BOOST_CHECK_EQUAL(testArgs.GetArg("inttest3", -1), -1);
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest1", false), true);
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest2", false), false);
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest3", false), false);
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest4", false), true);

    BOOST_CHECK_EQUAL(testArgs.GetArg("pritest1", "default"), "b");
    BOOST_CHECK_EQUAL(testArgs.GetArg("pritest2", "default"), "a");
    BOOST_CHECK_EQUAL(testArgs.GetArg("pritest3", "default"), "a");
    BOOST_CHECK_EQUAL(testArgs.GetArg("pritest4", "default"), "b");
}

BOOST_AUTO_TEST_CASE(util_GetChainName)
{
    TestArgsManager test_args;
    const char* avail_args[] = {"-testnet", "-regtest"};
    test_args.SetupArgs(2, avail_args);

    const char* argv_testnet[] = {"cmd", "-testnet"};
    const char* argv_regtest[] = {"cmd", "-regtest"};
    const char* argv_test_no_reg[] = {"cmd", "-testnet", "-noregtest"};
    const char* argv_both[] = {"cmd", "-testnet", "-regtest"};

//相当于“-testnet”
//忽略testnet节中的regtest
    const char* testnetconf = "testnet=1\nregtest=0\n[test]\nregtest=1";
    std::string error;

    BOOST_CHECK(test_args.ParseParameters(0, (char**)argv_testnet, error));
    BOOST_CHECK_EQUAL(test_args.GetChainName(), "main");

    BOOST_CHECK(test_args.ParseParameters(2, (char**)argv_testnet, error));
    BOOST_CHECK_EQUAL(test_args.GetChainName(), "test");

    BOOST_CHECK(test_args.ParseParameters(2, (char**)argv_regtest, error));
    BOOST_CHECK_EQUAL(test_args.GetChainName(), "regtest");

    BOOST_CHECK(test_args.ParseParameters(3, (char**)argv_test_no_reg, error));
    BOOST_CHECK_EQUAL(test_args.GetChainName(), "test");

    BOOST_CHECK(test_args.ParseParameters(3, (char**)argv_both, error));
    BOOST_CHECK_THROW(test_args.GetChainName(), std::runtime_error);

    BOOST_CHECK(test_args.ParseParameters(0, (char**)argv_testnet, error));
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_EQUAL(test_args.GetChainName(), "test");

    BOOST_CHECK(test_args.ParseParameters(2, (char**)argv_testnet, error));
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_EQUAL(test_args.GetChainName(), "test");

    BOOST_CHECK(test_args.ParseParameters(2, (char**)argv_regtest, error));
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_THROW(test_args.GetChainName(), std::runtime_error);

    BOOST_CHECK(test_args.ParseParameters(3, (char**)argv_test_no_reg, error));
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_EQUAL(test_args.GetChainName(), "test");

    BOOST_CHECK(test_args.ParseParameters(3, (char**)argv_both, error));
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_THROW(test_args.GetChainName(), std::runtime_error);

//检查设置要测试的网络（从而使
//[测试]regtest=1潜在相关）不会破坏事物
    test_args.SelectConfigNetwork("test");

    BOOST_CHECK(test_args.ParseParameters(0, (char**)argv_testnet, error));
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_EQUAL(test_args.GetChainName(), "test");

    BOOST_CHECK(test_args.ParseParameters(2, (char**)argv_testnet, error));
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_EQUAL(test_args.GetChainName(), "test");

    BOOST_CHECK(test_args.ParseParameters(2, (char**)argv_regtest, error));
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_THROW(test_args.GetChainName(), std::runtime_error);

    BOOST_CHECK(test_args.ParseParameters(2, (char**)argv_test_no_reg, error));
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_EQUAL(test_args.GetChainName(), "test");

    BOOST_CHECK(test_args.ParseParameters(3, (char**)argv_both, error));
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_THROW(test_args.GetChainName(), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(util_FormatMoney)
{
    BOOST_CHECK_EQUAL(FormatMoney(0), "0.00");
    BOOST_CHECK_EQUAL(FormatMoney((COIN/10000)*123456789), "12345.6789");
    BOOST_CHECK_EQUAL(FormatMoney(-COIN), "-1.00");

    BOOST_CHECK_EQUAL(FormatMoney(COIN*100000000), "100000000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*10000000), "10000000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*1000000), "1000000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*100000), "100000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*10000), "10000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*1000), "1000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*100), "100.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*10), "10.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN), "1.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/10), "0.10");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/100), "0.01");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/1000), "0.001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/10000), "0.0001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/100000), "0.00001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/1000000), "0.000001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/10000000), "0.0000001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/100000000), "0.00000001");
}

BOOST_AUTO_TEST_CASE(util_ParseMoney)
{
    CAmount ret = 0;
    BOOST_CHECK(ParseMoney("0.0", ret));
    BOOST_CHECK_EQUAL(ret, 0);

    BOOST_CHECK(ParseMoney("12345.6789", ret));
    BOOST_CHECK_EQUAL(ret, (COIN/10000)*123456789);

    BOOST_CHECK(ParseMoney("100000000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*100000000);
    BOOST_CHECK(ParseMoney("10000000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*10000000);
    BOOST_CHECK(ParseMoney("1000000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*1000000);
    BOOST_CHECK(ParseMoney("100000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*100000);
    BOOST_CHECK(ParseMoney("10000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*10000);
    BOOST_CHECK(ParseMoney("1000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*1000);
    BOOST_CHECK(ParseMoney("100.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*100);
    BOOST_CHECK(ParseMoney("10.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*10);
    BOOST_CHECK(ParseMoney("1.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN);
    BOOST_CHECK(ParseMoney("1", ret));
    BOOST_CHECK_EQUAL(ret, COIN);
    BOOST_CHECK(ParseMoney("0.1", ret));
    BOOST_CHECK_EQUAL(ret, COIN/10);
    BOOST_CHECK(ParseMoney("0.01", ret));
    BOOST_CHECK_EQUAL(ret, COIN/100);
    BOOST_CHECK(ParseMoney("0.001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/1000);
    BOOST_CHECK(ParseMoney("0.0001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/10000);
    BOOST_CHECK(ParseMoney("0.00001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/100000);
    BOOST_CHECK(ParseMoney("0.000001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/1000000);
    BOOST_CHECK(ParseMoney("0.0000001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/10000000);
    BOOST_CHECK(ParseMoney("0.00000001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/100000000);

//尝试的63位溢出应失败
    BOOST_CHECK(!ParseMoney("92233720368.54775808", ret));

//分析负数必须失败
    BOOST_CHECK(!ParseMoney("-1", ret));
}

BOOST_AUTO_TEST_CASE(util_IsHex)
{
    BOOST_CHECK(IsHex("00"));
    BOOST_CHECK(IsHex("00112233445566778899aabbccddeeffAABBCCDDEEFF"));
    BOOST_CHECK(IsHex("ff"));
    BOOST_CHECK(IsHex("FF"));

    BOOST_CHECK(!IsHex(""));
    BOOST_CHECK(!IsHex("0"));
    BOOST_CHECK(!IsHex("a"));
    BOOST_CHECK(!IsHex("eleven"));
    BOOST_CHECK(!IsHex("00xx00"));
    BOOST_CHECK(!IsHex("0x0000"));
}

BOOST_AUTO_TEST_CASE(util_IsHexNumber)
{
    BOOST_CHECK(IsHexNumber("0x0"));
    BOOST_CHECK(IsHexNumber("0"));
    BOOST_CHECK(IsHexNumber("0x10"));
    BOOST_CHECK(IsHexNumber("10"));
    BOOST_CHECK(IsHexNumber("0xff"));
    BOOST_CHECK(IsHexNumber("ff"));
    BOOST_CHECK(IsHexNumber("0xFfa"));
    BOOST_CHECK(IsHexNumber("Ffa"));
    BOOST_CHECK(IsHexNumber("0x00112233445566778899aabbccddeeffAABBCCDDEEFF"));
    BOOST_CHECK(IsHexNumber("00112233445566778899aabbccddeeffAABBCCDDEEFF"));

BOOST_CHECK(!IsHexNumber(""));   //不允许空字符串
BOOST_CHECK(!IsHexNumber("0x")); //前缀后不允许有空字符串
BOOST_CHECK(!IsHexNumber("0x0 ")); //末尾没有空格，
BOOST_CHECK(!IsHexNumber(" 0x0")); //或开始，
BOOST_CHECK(!IsHexNumber("0x 0")); //或者中间，
BOOST_CHECK(!IsHexNumber(" "));    //等。
BOOST_CHECK(!IsHexNumber("0x0ga")); //无效字符
BOOST_CHECK(!IsHexNumber("x0"));    //断前缀
BOOST_CHECK(!IsHexNumber("0x0x00")); //不允许有两个前缀

}

BOOST_AUTO_TEST_CASE(util_seed_insecure_rand)
{
    SeedInsecureRand(true);
    for (int mod=2;mod<11;mod++)
    {
        int mask = 1;
//真正粗糙的二项置信近似。
        int err = 30*10000./mod*sqrt((1./mod*(1-1./mod))/10000.);
//掩码为2^ceil（log2（mod））-1
        while(mask<mod-1)mask=(mask<<1)+1;

        int count = 0;
//从均匀范围[0，mod]得到零的频率是多少？
        for (int i = 0; i < 10000; i++) {
            uint32_t rval;
            do{
                rval=InsecureRand32()&mask;
            }while(rval>=(uint32_t)mod);
            count += rval==0;
        }
        BOOST_CHECK(count<=10000/mod+err);
        BOOST_CHECK(count>=10000/mod-err);
    }
}

BOOST_AUTO_TEST_CASE(util_TimingResistantEqual)
{
    BOOST_CHECK(TimingResistantEqual(std::string(""), std::string("")));
    BOOST_CHECK(!TimingResistantEqual(std::string("abc"), std::string("")));
    BOOST_CHECK(!TimingResistantEqual(std::string(""), std::string("abc")));
    BOOST_CHECK(!TimingResistantEqual(std::string("a"), std::string("aa")));
    BOOST_CHECK(!TimingResistantEqual(std::string("aa"), std::string("a")));
    BOOST_CHECK(TimingResistantEqual(std::string("abc"), std::string("abc")));
    BOOST_CHECK(!TimingResistantEqual(std::string("abc"), std::string("aba")));
}

/*测试strprintf格式指令。
 *前后放置一个字符串，以确保堆栈上元素大小的完备性。*/

#define B "check_prefix"
#define E "check_postfix"
BOOST_AUTO_TEST_CASE(strprintf_numbers)
{
    /*64_t s64t=-9223372036854775807ll；/*有符号64位测试值*/
    uint64_t u64t=18446744073709551615ull；/*无符号64位测试值*/

    BOOST_CHECK(strprintf("%s %d %s", B, s64t, E) == B" -9223372036854775807 " E);
    BOOST_CHECK(strprintf("%s %u %s", B, u64t, E) == B" 18446744073709551615 " E);
    BOOST_CHECK(strprintf("%s %x %s", B, u64t, E) == B" ffffffffffffffff " E);

    /*e_t st=12345678；/*无符号大小_t测试值*/
    ssize_t sst=-12345678；/*有符号大小t测试值*/

    BOOST_CHECK(strprintf("%s %d %s", B, sst, E) == B" -12345678 " E);
    BOOST_CHECK(strprintf("%s %u %s", B, st, E) == B" 12345678 " E);
    BOOST_CHECK(strprintf("%s %x %s", B, st, E) == B" bc614e " E);

    /*diff_t pt=87654321；/*正的ptr diff_t测试值*/
    ptrDiff_t spt=-87654321；/*负ptrDiff_t测试值*/

    BOOST_CHECK(strprintf("%s %d %s", B, spt, E) == B" -87654321 " E);
    BOOST_CHECK(strprintf("%s %u %s", B, pt, E) == B" 87654321 " E);
    BOOST_CHECK(strprintf("%s %x %s", B, pt, E) == B" 5397fb1 " E);
}
#undef B
#undef E

/*检查明格酒/葡萄酒问题3494
 *在time.ctime（0xffffffff）='太阳2月7日07:28:15 2106'
 **/

BOOST_AUTO_TEST_CASE(gettime)
{
    BOOST_CHECK((GetTime() & ~0xFFFFFFFFLL) == 0);
}

BOOST_AUTO_TEST_CASE(test_IsDigit)
{
    BOOST_CHECK_EQUAL(IsDigit('0'), true);
    BOOST_CHECK_EQUAL(IsDigit('1'), true);
    BOOST_CHECK_EQUAL(IsDigit('8'), true);
    BOOST_CHECK_EQUAL(IsDigit('9'), true);

    BOOST_CHECK_EQUAL(IsDigit('0' - 1), false);
    BOOST_CHECK_EQUAL(IsDigit('9' + 1), false);
    BOOST_CHECK_EQUAL(IsDigit(0), false);
    BOOST_CHECK_EQUAL(IsDigit(1), false);
    BOOST_CHECK_EQUAL(IsDigit(8), false);
    BOOST_CHECK_EQUAL(IsDigit(9), false);
}

BOOST_AUTO_TEST_CASE(test_ParseInt32)
{
    int32_t n;
//有效值
    BOOST_CHECK(ParseInt32("1234", nullptr));
    BOOST_CHECK(ParseInt32("0", &n) && n == 0);
    BOOST_CHECK(ParseInt32("1234", &n) && n == 1234);
BOOST_CHECK(ParseInt32("01234", &n) && n == 1234); //无八进制
    BOOST_CHECK(ParseInt32("2147483647", &n) && n == 2147483647);
BOOST_CHECK(ParseInt32("-2147483648", &n) && n == (-2147483647 - 1)); //（-2147483647-1）等于int_min
    BOOST_CHECK(ParseInt32("-1234", &n) && n == -1234);
//无效值
    BOOST_CHECK(!ParseInt32("", &n));
BOOST_CHECK(!ParseInt32(" 1", &n)); //里面没有填充物
    BOOST_CHECK(!ParseInt32("1 ", &n));
    BOOST_CHECK(!ParseInt32("1a", &n));
    BOOST_CHECK(!ParseInt32("aap", &n));
BOOST_CHECK(!ParseInt32("0x1", &n)); //无十六进制
BOOST_CHECK(!ParseInt32("0x1", &n)); //无十六进制
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
BOOST_CHECK(!ParseInt32(teststr, &n)); //无嵌入式Nuls
//溢出和下溢
    BOOST_CHECK(!ParseInt32("-2147483649", nullptr));
    BOOST_CHECK(!ParseInt32("2147483648", nullptr));
    BOOST_CHECK(!ParseInt32("-32482348723847471234", nullptr));
    BOOST_CHECK(!ParseInt32("32482348723847471234", nullptr));
}

BOOST_AUTO_TEST_CASE(test_ParseInt64)
{
    int64_t n;
//有效值
    BOOST_CHECK(ParseInt64("1234", nullptr));
    BOOST_CHECK(ParseInt64("0", &n) && n == 0LL);
    BOOST_CHECK(ParseInt64("1234", &n) && n == 1234LL);
BOOST_CHECK(ParseInt64("01234", &n) && n == 1234LL); //无八进制
    BOOST_CHECK(ParseInt64("2147483647", &n) && n == 2147483647LL);
    BOOST_CHECK(ParseInt64("-2147483648", &n) && n == -2147483648LL);
    BOOST_CHECK(ParseInt64("9223372036854775807", &n) && n == (int64_t)9223372036854775807);
    BOOST_CHECK(ParseInt64("-9223372036854775808", &n) && n == (int64_t)-9223372036854775807-1);
    BOOST_CHECK(ParseInt64("-1234", &n) && n == -1234LL);
//无效值
    BOOST_CHECK(!ParseInt64("", &n));
BOOST_CHECK(!ParseInt64(" 1", &n)); //里面没有填充物
    BOOST_CHECK(!ParseInt64("1 ", &n));
    BOOST_CHECK(!ParseInt64("1a", &n));
    BOOST_CHECK(!ParseInt64("aap", &n));
BOOST_CHECK(!ParseInt64("0x1", &n)); //无十六进制
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
BOOST_CHECK(!ParseInt64(teststr, &n)); //无嵌入式Nuls
//溢出和下溢
    BOOST_CHECK(!ParseInt64("-9223372036854775809", nullptr));
    BOOST_CHECK(!ParseInt64("9223372036854775808", nullptr));
    BOOST_CHECK(!ParseInt64("-32482348723847471234", nullptr));
    BOOST_CHECK(!ParseInt64("32482348723847471234", nullptr));
}

BOOST_AUTO_TEST_CASE(test_ParseUInt32)
{
    uint32_t n;
//有效值
    BOOST_CHECK(ParseUInt32("1234", nullptr));
    BOOST_CHECK(ParseUInt32("0", &n) && n == 0);
    BOOST_CHECK(ParseUInt32("1234", &n) && n == 1234);
BOOST_CHECK(ParseUInt32("01234", &n) && n == 1234); //无八进制
    BOOST_CHECK(ParseUInt32("2147483647", &n) && n == 2147483647);
    BOOST_CHECK(ParseUInt32("2147483648", &n) && n == (uint32_t)2147483648);
    BOOST_CHECK(ParseUInt32("4294967295", &n) && n == (uint32_t)4294967295);
//无效值
    BOOST_CHECK(!ParseUInt32("", &n));
BOOST_CHECK(!ParseUInt32(" 1", &n)); //里面没有填充物
    BOOST_CHECK(!ParseUInt32(" -1", &n));
    BOOST_CHECK(!ParseUInt32("1 ", &n));
    BOOST_CHECK(!ParseUInt32("1a", &n));
    BOOST_CHECK(!ParseUInt32("aap", &n));
BOOST_CHECK(!ParseUInt32("0x1", &n)); //无十六进制
BOOST_CHECK(!ParseUInt32("0x1", &n)); //无十六进制
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
BOOST_CHECK(!ParseUInt32(teststr, &n)); //无嵌入式Nuls
//溢出和下溢
    BOOST_CHECK(!ParseUInt32("-2147483648", &n));
    BOOST_CHECK(!ParseUInt32("4294967296", &n));
    BOOST_CHECK(!ParseUInt32("-1234", &n));
    BOOST_CHECK(!ParseUInt32("-32482348723847471234", nullptr));
    BOOST_CHECK(!ParseUInt32("32482348723847471234", nullptr));
}

BOOST_AUTO_TEST_CASE(test_ParseUInt64)
{
    uint64_t n;
//有效值
    BOOST_CHECK(ParseUInt64("1234", nullptr));
    BOOST_CHECK(ParseUInt64("0", &n) && n == 0LL);
    BOOST_CHECK(ParseUInt64("1234", &n) && n == 1234LL);
BOOST_CHECK(ParseUInt64("01234", &n) && n == 1234LL); //无八进制
    BOOST_CHECK(ParseUInt64("2147483647", &n) && n == 2147483647LL);
    BOOST_CHECK(ParseUInt64("9223372036854775807", &n) && n == 9223372036854775807ULL);
    BOOST_CHECK(ParseUInt64("9223372036854775808", &n) && n == 9223372036854775808ULL);
    BOOST_CHECK(ParseUInt64("18446744073709551615", &n) && n == 18446744073709551615ULL);
//无效值
    BOOST_CHECK(!ParseUInt64("", &n));
BOOST_CHECK(!ParseUInt64(" 1", &n)); //里面没有填充物
    BOOST_CHECK(!ParseUInt64(" -1", &n));
    BOOST_CHECK(!ParseUInt64("1 ", &n));
    BOOST_CHECK(!ParseUInt64("1a", &n));
    BOOST_CHECK(!ParseUInt64("aap", &n));
BOOST_CHECK(!ParseUInt64("0x1", &n)); //无十六进制
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
BOOST_CHECK(!ParseUInt64(teststr, &n)); //无嵌入式Nuls
//溢出和下溢
    BOOST_CHECK(!ParseUInt64("-9223372036854775809", nullptr));
    BOOST_CHECK(!ParseUInt64("18446744073709551616", nullptr));
    BOOST_CHECK(!ParseUInt64("-32482348723847471234", nullptr));
    BOOST_CHECK(!ParseUInt64("-2147483648", &n));
    BOOST_CHECK(!ParseUInt64("-9223372036854775808", &n));
    BOOST_CHECK(!ParseUInt64("-1234", &n));
}

BOOST_AUTO_TEST_CASE(test_ParseDouble)
{
    double n;
//有效值
    BOOST_CHECK(ParseDouble("1234", nullptr));
    BOOST_CHECK(ParseDouble("0", &n) && n == 0.0);
    BOOST_CHECK(ParseDouble("1234", &n) && n == 1234.0);
BOOST_CHECK(ParseDouble("01234", &n) && n == 1234.0); //无八进制
    BOOST_CHECK(ParseDouble("2147483647", &n) && n == 2147483647.0);
    BOOST_CHECK(ParseDouble("-2147483648", &n) && n == -2147483648.0);
    BOOST_CHECK(ParseDouble("-1234", &n) && n == -1234.0);
    BOOST_CHECK(ParseDouble("1e6", &n) && n == 1e6);
    BOOST_CHECK(ParseDouble("-1e6", &n) && n == -1e6);
//无效值
    BOOST_CHECK(!ParseDouble("", &n));
BOOST_CHECK(!ParseDouble(" 1", &n)); //里面没有填充物
    BOOST_CHECK(!ParseDouble("1 ", &n));
    BOOST_CHECK(!ParseDouble("1a", &n));
    BOOST_CHECK(!ParseDouble("aap", &n));
BOOST_CHECK(!ParseDouble("0x1", &n)); //无十六进制
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
BOOST_CHECK(!ParseDouble(teststr, &n)); //无嵌入式Nuls
//溢出和下溢
    BOOST_CHECK(!ParseDouble("-1e10000", nullptr));
    BOOST_CHECK(!ParseDouble("1e10000", nullptr));
}

BOOST_AUTO_TEST_CASE(test_FormatParagraph)
{
    BOOST_CHECK_EQUAL(FormatParagraph("", 79, 0), "");
    BOOST_CHECK_EQUAL(FormatParagraph("test", 79, 0), "test");
    BOOST_CHECK_EQUAL(FormatParagraph(" test", 79, 0), " test");
    BOOST_CHECK_EQUAL(FormatParagraph("test test", 79, 0), "test test");
    BOOST_CHECK_EQUAL(FormatParagraph("test test", 4, 0), "test\ntest");
    BOOST_CHECK_EQUAL(FormatParagraph("testerde test", 4, 0), "testerde\ntest");
    BOOST_CHECK_EQUAL(FormatParagraph("test test", 4, 4), "test\n    test");

//请确保不要在结束行太长后缩进完全新行。
    BOOST_CHECK_EQUAL(FormatParagraph("test test\nabc", 4, 4), "test\n    test\nabc");

    BOOST_CHECK_EQUAL(FormatParagraph("This_is_a_very_long_test_string_without_any_spaces_so_it_should_just_get_returned_as_is_despite_the_length until it gets here", 79), "This_is_a_very_long_test_string_without_any_spaces_so_it_should_just_get_returned_as_is_despite_the_length\nuntil it gets here");

//测试包装长度准确
    BOOST_CHECK_EQUAL(FormatParagraph("a b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de f g h i j k l m n o p", 79), "a b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de\nf g h i j k l m n o p");
    BOOST_CHECK_EQUAL(FormatParagraph("x\na b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de f g h i j k l m n o p", 79), "x\na b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de\nf g h i j k l m n o p");
//缩进应包括在行的长度中。
    BOOST_CHECK_EQUAL(FormatParagraph("x\na b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de f g h i j k l m n o p q r s t u v w x y z 0 1 2 3 4 5 6 7 8 9 a b c d e fg h i j k", 79, 4), "x\na b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de\n    f g h i j k l m n o p q r s t u v w x y z 0 1 2 3 4 5 6 7 8 9 a b c d e fg\n    h i j k");

    BOOST_CHECK_EQUAL(FormatParagraph("This is a very long test string. This is a second sentence in the very long test string.", 79), "This is a very long test string. This is a second sentence in the very long\ntest string.");
    BOOST_CHECK_EQUAL(FormatParagraph("This is a very long test string.\nThis is a second sentence in the very long test string. This is a third sentence in the very long test string.", 79), "This is a very long test string.\nThis is a second sentence in the very long test string. This is a third\nsentence in the very long test string.");
    BOOST_CHECK_EQUAL(FormatParagraph("This is a very long test string.\n\nThis is a second sentence in the very long test string. This is a third sentence in the very long test string.", 79), "This is a very long test string.\n\nThis is a second sentence in the very long test string. This is a third\nsentence in the very long test string.");
    BOOST_CHECK_EQUAL(FormatParagraph("Testing that normal newlines do not get indented.\nLike here.", 79), "Testing that normal newlines do not get indented.\nLike here.");
}

BOOST_AUTO_TEST_CASE(test_FormatSubVersion)
{
    std::vector<std::string> comments;
    comments.push_back(std::string("comment1"));
    std::vector<std::string> comments2;
    comments2.push_back(std::string("comment1"));
comments2.push_back(SanitizeString(std::string("Comment2; .,_?@-; !\"#$%&'()*+/<=>[]\\^`{|}~"), SAFE_CHARS_UA_COMMENT)); //BIP-0014不鼓励使用分号，但不禁止使用分号。
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99900, std::vector<std::string>()),std::string("/Test:0.9.99/"));
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99900, comments),std::string("/Test:0.9.99(comment1)/"));
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99900, comments2),std::string("/Test:0.9.99(comment1; Comment2; .,_?@-; )/"));
}

BOOST_AUTO_TEST_CASE(test_ParseFixedPoint)
{
    int64_t amount = 0;
    BOOST_CHECK(ParseFixedPoint("0", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 0LL);
    BOOST_CHECK(ParseFixedPoint("1", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 100000000LL);
    BOOST_CHECK(ParseFixedPoint("0.0", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 0LL);
    BOOST_CHECK(ParseFixedPoint("-0.1", 8, &amount));
    BOOST_CHECK_EQUAL(amount, -10000000LL);
    BOOST_CHECK(ParseFixedPoint("1.1", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 110000000LL);
    BOOST_CHECK(ParseFixedPoint("1.10000000000000000", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 110000000LL);
    BOOST_CHECK(ParseFixedPoint("1.1e1", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 1100000000LL);
    BOOST_CHECK(ParseFixedPoint("1.1e-1", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 11000000LL);
    BOOST_CHECK(ParseFixedPoint("1000", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 100000000000LL);
    BOOST_CHECK(ParseFixedPoint("-1000", 8, &amount));
    BOOST_CHECK_EQUAL(amount, -100000000000LL);
    BOOST_CHECK(ParseFixedPoint("0.00000001", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 1LL);
    BOOST_CHECK(ParseFixedPoint("0.0000000100000000", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 1LL);
    BOOST_CHECK(ParseFixedPoint("-0.00000001", 8, &amount));
    BOOST_CHECK_EQUAL(amount, -1LL);
    BOOST_CHECK(ParseFixedPoint("1000000000.00000001", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 100000000000000001LL);
    BOOST_CHECK(ParseFixedPoint("9999999999.99999999", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 999999999999999999LL);
    BOOST_CHECK(ParseFixedPoint("-9999999999.99999999", 8, &amount));
    BOOST_CHECK_EQUAL(amount, -999999999999999999LL);

    BOOST_CHECK(!ParseFixedPoint("", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("a-1000", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-a1000", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-1000a", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-01000", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("00.1", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint(".1", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("--0.1", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("0.000000001", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-0.000000001", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("0.00000001000000001", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-10000000000.00000000", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("10000000000.00000000", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-10000000000.00000001", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("10000000000.00000001", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-10000000000.00000009", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("10000000000.00000009", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-99999999999.99999999", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("99999909999.09999999", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("92233720368.54775807", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("92233720368.54775808", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-92233720368.54775808", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-92233720368.54775809", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("1.1e", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("1.1e-", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("1.", 8, &amount));
}

static void TestOtherThread(fs::path dirname, std::string lockname, bool *result)
{
    *result = LockDirectory(dirname, lockname);
}

#ifndef WIN32 //由于缺少fork（），无法在win32上执行此测试
static constexpr char LockCommand = 'L';
static constexpr char UnlockCommand = 'U';
static constexpr char ExitCommand = 'X';

static void TestOtherProcess(fs::path dirname, std::string lockname, int fd)
{
    char ch;
    while (true) {
int rv = read(fd, &ch, 1); //等待命令
        assert(rv == 1);
        switch(ch) {
        case LockCommand:
            ch = LockDirectory(dirname, lockname);
            rv = write(fd, &ch, 1);
            assert(rv == 1);
            break;
        case UnlockCommand:
            ReleaseDirectoryLocks();
ch = true; //总是成功
            rv = write(fd, &ch, 1);
            assert(rv == 1);
            break;
        case ExitCommand:
            close(fd);
            exit(0);
        default:
            assert(0);
        }
    }
}
#endif

BOOST_AUTO_TEST_CASE(test_LockDirectory)
{
    fs::path dirname = SetDataDir("test_LockDirectory") / fs::unique_path();
    const std::string lockname = ".lock";
#ifndef WIN32
//将sigchld恢复为默认值，否则boost.test将捕获并失败。
//它：有boost测试，忽略信号，但只有在定义时才起作用。
//在Boost库的构建时
    void (*old_handler)(int) = signal(SIGCHLD, SIG_DFL);

//在创建锁之前分叉另一个测试过程，以便
//持有锁时不会分叉（这可能是未定义的，并且不是
//与测试用例相关，因为使用-daemonize可以避免这种情况）。
    int fd[2];
    BOOST_CHECK_EQUAL(socketpair(AF_UNIX, SOCK_STREAM, 0, fd), 0);
    pid_t pid = fork();
    if (!pid) {
BOOST_CHECK_EQUAL(close(fd[1]), 0); //子：关闭父端
        TestOtherProcess(dirname, lockname, fd[0]);
    }
BOOST_CHECK_EQUAL(close(fd[0]), 0); //父级：关闭子级端
#endif
//锁定不存在的目录失败
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname), false);

    fs::create_directories(dirname);

//在新目录上探测锁应该成功
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname, true), true);

//新目录上的永久锁定应成功
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname), true);

//来自同一线程的目录上的另一个锁应该成功
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname), true);

//同一进程中来自不同线程的目录上的另一个锁应该成功
    bool threadresult;
    std::thread thr(TestOtherThread, dirname, lockname, &threadresult);
    thr.join();
    BOOST_CHECK_EQUAL(threadresult, true);
#ifndef WIN32
//尝试获取子进程中的锁，而我们持有它，这应该失败。
    char ch;
    BOOST_CHECK_EQUAL(write(fd[1], &LockCommand, 1), 1);
    BOOST_CHECK_EQUAL(read(fd[1], &ch, 1), 1);
    BOOST_CHECK_EQUAL((bool)ch, false);

//放弃我们的锁
    ReleaseDirectoryLocks();
//现在从我们这边探测锁应该是成功的，但不能抓住锁。
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname, true), true);

//尝试获取子进程中的锁，这应该是成功的。
    BOOST_CHECK_EQUAL(write(fd[1], &LockCommand, 1), 1);
    BOOST_CHECK_EQUAL(read(fd[1], &ch, 1), 1);
    BOOST_CHECK_EQUAL((bool)ch, true);

//当我们现在试图探测锁时，它应该会失效。
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname, true), false);

//在子进程中解除锁定
    BOOST_CHECK_EQUAL(write(fd[1], &UnlockCommand, 1), 1);
    BOOST_CHECK_EQUAL(read(fd[1], &ch, 1), 1);
    BOOST_CHECK_EQUAL((bool)ch, true);

//当我们现在试图探测锁时，它应该会成功。
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname, true), true);

//在子进程中重新锁定锁，然后等待它退出，检查
//成功返回。之后，我们检查退出进程
//已经像我们预期的那样通过探测释放了锁。
    int processstatus;
    BOOST_CHECK_EQUAL(write(fd[1], &LockCommand, 1), 1);
    BOOST_CHECK_EQUAL(write(fd[1], &ExitCommand, 1), 1);
    BOOST_CHECK_EQUAL(waitpid(pid, &processstatus, 0), pid);
    BOOST_CHECK_EQUAL(processstatus, 0);
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname, true), true);

//恢复SigCHLD
    signal(SIGCHLD, old_handler);
BOOST_CHECK_EQUAL(close(fd[1]), 0); //合上我们的袜子
#endif
//清理
    ReleaseDirectoryLocks();
    fs::remove_all(dirname);
}

BOOST_AUTO_TEST_CASE(test_DirIsWritable)
{
//应该能够写入数据目录。
    fs::path tmpdirname = SetDataDir("test_DirIsWritable");
    BOOST_CHECK_EQUAL(DirIsWritable(tmpdirname), true);

//不能写入不存在的目录。
    tmpdirname = tmpdirname / fs::unique_path();
    BOOST_CHECK_EQUAL(DirIsWritable(tmpdirname), false);

    fs::create_directory(tmpdirname);
//现在应该可以给它写信了。
    BOOST_CHECK_EQUAL(DirIsWritable(tmpdirname), true);
    fs::remove(tmpdirname);
}

BOOST_AUTO_TEST_CASE(test_ToLower)
{
    BOOST_CHECK_EQUAL(ToLower('@'), '@');
    BOOST_CHECK_EQUAL(ToLower('A'), 'a');
    BOOST_CHECK_EQUAL(ToLower('Z'), 'z');
    BOOST_CHECK_EQUAL(ToLower('['), '[');
    BOOST_CHECK_EQUAL(ToLower(0), 0);
    BOOST_CHECK_EQUAL(ToLower('\xff'), '\xff');

    std::string testVector;
    Downcase(testVector);
    BOOST_CHECK_EQUAL(testVector, "");

    testVector = "#HODL";
    Downcase(testVector);
    BOOST_CHECK_EQUAL(testVector, "#hodl");

    testVector = "\x00\xfe\xff";
    Downcase(testVector);
    BOOST_CHECK_EQUAL(testVector, "\x00\xfe\xff");
}

BOOST_AUTO_TEST_CASE(test_ToUpper)
{
    BOOST_CHECK_EQUAL(ToUpper('`'), '`');
    BOOST_CHECK_EQUAL(ToUpper('a'), 'A');
    BOOST_CHECK_EQUAL(ToUpper('z'), 'Z');
    BOOST_CHECK_EQUAL(ToUpper('{'), '{');
    BOOST_CHECK_EQUAL(ToUpper(0), 0);
    BOOST_CHECK_EQUAL(ToUpper('\xff'), '\xff');
}

BOOST_AUTO_TEST_CASE(test_Capitalize)
{
    BOOST_CHECK_EQUAL(Capitalize(""), "");
    BOOST_CHECK_EQUAL(Capitalize("bitcoin"), "Bitcoin");
    BOOST_CHECK_EQUAL(Capitalize("\x00\xfe\xff"), "\x00\xfe\xff");
}

BOOST_AUTO_TEST_SUITE_END()
