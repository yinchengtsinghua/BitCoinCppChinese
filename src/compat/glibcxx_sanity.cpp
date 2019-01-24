
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <list>
#include <locale>
#include <stdexcept>

namespace
{
//触发器：使用ctype<char>：：widen触发ctype<char>：_m_widen_init（）。
//测试：将字符从窄转换为宽，然后再转换回来。验证结果
//与原件匹配。
bool sanity_test_widen(char testchar)
{
    const std::ctype<char>& test(std::use_facet<std::ctype<char> >(std::locale()));
    return test.narrow(test.widen(testchar), 'b') == testchar;
}

//触发器：使用list：：push_back和list：：pop_back来触发挂钩和
//解开钩。
//测试：将一个整数序列推送到一个列表中。弹出并验证
//它们与原始序列匹配。
bool sanity_test_list(unsigned int size)
{
    std::list<unsigned int> test;
    for (unsigned int i = 0; i != size; ++i)
        test.push_back(i + 1);

    if (test.size() != size)
        return false;

    while (!test.empty()) {
        if (test.back() != test.size())
            return false;
        test.pop_back();
    }
    return true;
}

} //命名空间

//触发器：字符串：：在空字符串上的（x）处触发“抛出”超出“范围”的“fmt”。
//测试：强制std：：string抛出\u范围异常。验证
//它捕捉正确。
bool sanity_test_range_fmt()
{
    std::string test;
    try {
        test.at(1);
    } catch (const std::out_of_range&) {
        return true;
    } catch (...) {
    }
    return false;
}

bool glibcxx_sanity_test()
{
    return sanity_test_widen('a') && sanity_test_list(100) && sanity_test_range_fmt();
}
