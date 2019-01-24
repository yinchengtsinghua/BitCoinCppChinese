
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

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <cstddef>

#if defined(HAVE_SYS_SELECT_H)
#include <sys/select.h>
#endif

extern "C" void* memcpy(void* a, const void* b, size_t c);
void* memcpy_int(void* a, const void* b, size_t c)
{
    return memcpy(a, b, c);
}

namespace
{
//触发器：使用调用内部memcpy的memcpy_int包装器。
//直接调用memcpy可以被编译器优化掉。
//测试：用整数序列填充数组。新空数组的memcpy。
//验证数组是否相等。使用奇数大小来降低
//正在优化呼叫。
template <unsigned int T>
bool sanity_test_memcpy()
{
    unsigned int memcpy_test[T];
    unsigned int memcpy_verify[T] = {};
    for (unsigned int i = 0; i != T; ++i)
        memcpy_test[i] = i;

    memcpy_int(memcpy_verify, memcpy_test, sizeof(memcpy_test));

    for (unsigned int i = 0; i != T; ++i) {
        if (memcpy_verify[i] != i)
            return false;
    }
    return true;
}

#if defined(HAVE_SYS_SELECT_H)
//trigger：调用fd_set以触发。必须定义强化源
//当>0时，优化必须设置为至少-o2。
//测试：将文件描述符添加到空的fd_集合中。确认它已经
//正确添加。
bool sanity_test_fdelt()
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return FD_ISSET(0, &fds);
}
#endif

} //命名空间

bool glibc_sanity_test()
{
#if defined(HAVE_SYS_SELECT_H)
    if (!sanity_test_fdelt())
        return false;
#endif
    return sanity_test_memcpy<1025>();
}
