
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//JSON测试套件可以调用的测试程序
//https://github.com/nst/jContestSuite。
//
//它从stdin读取JSON输入，如果可以解析，则使用代码0退出。
//成功地。它还将解析的JSON值漂亮地打印到stdout。

#include <iostream>
#include <string>
#include "univalue.h"

using namespace std;

int main (int argc, char *argv[])
{
    UniValue val;
    if (val.read(string(istreambuf_iterator<char>(cin),
                        istreambuf_iterator<char>()))) {
        /*t<<val.write（1/*prettyindent*/，4/*indentlevel*/）<<endl；
        返回0；
    }否则{
        cerr<“json分析错误。”<<endl；
        返回1；
    }
}
