
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有2014 Bitpay Inc.
//在MIT/X11软件许可证下分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cassert>
#include <string>
#include "univalue.h"

#ifndef JSON_TEST_SRC
#error JSON_TEST_SRC must point to test source directory
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

std::string srcdir(JSON_TEST_SRC);
static bool test_failed = false;

#define d_assert(expr) { if (!(expr)) { test_failed = true; fprintf(stderr, "%s failed\n", filename.c_str()); } }
#define f_assert(expr) { if (!(expr)) { test_failed = true; fprintf(stderr, "%s failed\n", __func__); } }

static std::string rtrim(std::string s)
{
    s.erase(s.find_last_not_of(" \n\r\t")+1);
    return s;
}

static void runtest(std::string filename, const std::string& jdata)
{
        std::string prefix = filename.substr(0, 4);

        bool wantPass = (prefix == "pass") || (prefix == "roun");
        bool wantFail = (prefix == "fail");
        bool wantRoundTrip = (prefix == "roun");
        assert(wantPass || wantFail);

        UniValue val;
        bool testResult = val.read(jdata);

        if (wantPass) {
            d_assert(testResult == true);
        } else {
            d_assert(testResult == false);
        }

        if (wantRoundTrip) {
            std::string odata = val.write(0, 0);
            assert(odata == rtrim(jdata));
        }
}

static void runtest_file(const char *filename_)
{
        std::string basename(filename_);
        std::string filename = srcdir + "/" + basename;
        FILE *f = fopen(filename.c_str(), "r");
        assert(f != NULL);

        std::string jdata;

        char buf[4096];
        while (!feof(f)) {
                int bread = fread(buf, 1, sizeof(buf), f);
                assert(!ferror(f));

                std::string s(buf, bread);
                jdata += s;
        }

        assert(!ferror(f));
        fclose(f);

        runtest(basename, jdata);
}

static const char *filenames[] = {
        "fail10.json",
        "fail11.json",
        "fail12.json",
        "fail13.json",
        "fail14.json",
        "fail15.json",
        "fail16.json",
        "fail17.json",
//“fail18.json”，//调查
        "fail19.json",
        "fail1.json",
        "fail20.json",
        "fail21.json",
        "fail22.json",
        "fail23.json",
        "fail24.json",
        "fail25.json",
        "fail26.json",
        "fail27.json",
        "fail28.json",
        "fail29.json",
        "fail2.json",
        "fail30.json",
        "fail31.json",
        "fail32.json",
        "fail33.json",
        "fail34.json",
        "fail35.json",
        "fail36.json",
        "fail37.json",
"fail38.json",               //无效的Unicode:仅代理项对的前半部分
"fail39.json",               //无效的Unicode:仅代理项对的后半部分
"fail40.json",               //无效的unicode:中断的utf-8
"fail41.json",               //无效的unicode:未完成的utf-8
"fail42.json",               //有效的json，垃圾跟在nul字节后面
"fail44.json",               //未终止的字符串
        "fail3.json",
"fail4.json",                //额外逗号
        "fail5.json",
        "fail6.json",
        "fail7.json",
        "fail8.json",
"fail9.json",               //额外逗号
        "pass1.json",
        "pass2.json",
        "pass3.json",
"round1.json",              //往返试验
"round2.json",              //统一码
"round3.json",              //裸弦
"round4.json",              //裸数
"round5.json",              //裸真
"round6.json",              //裸假
"round7.json",              //裸空
};

//测试\u处理
void unescape_unicode_test()
{
    UniValue val;
    bool testResult;
//转义的ASCII（引号）
    testResult = val.read("[\"\\u0022\"]");
    f_assert(testResult);
    f_assert(val[0].get_str() == "\"");
//转义的基本平面字符，双字节UTF-8
    testResult = val.read("[\"\\u0191\"]");
    f_assert(testResult);
    f_assert(val[0].get_str() == "\xc6\x91");
//转义的基本平面字符，三字节UTF-8
    testResult = val.read("[\"\\u2191\"]");
    f_assert(testResult);
    f_assert(val[0].get_str() == "\xe2\x86\x91");
//转义的辅助平面字符U+1d161
    testResult = val.read("[\"\\ud834\\udd61\"]");
    f_assert(testResult);
    f_assert(val[0].get_str() == "\xf0\x9d\x85\xa1");
}

int main (int argc, char *argv[])
{
    for (unsigned int fidx = 0; fidx < ARRAY_SIZE(filenames); fidx++) {
        runtest_file(filenames[fidx]);
    }

    unescape_unicode_test();

    return test_failed ? 1 : 0;
}

