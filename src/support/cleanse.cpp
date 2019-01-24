
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <support/cleanse.h>

#include <cstring>

#if defined(_MSC_VER)
#include <Windows.h> //对于SecureZeroMemory。
#endif

/*编译器有一个坏习惯，即删除“多余”的memset调用
 *正在尝试将内存归零。例如，当memset（）使用缓冲区和
 *然后使用free（），编译器可能会决定memset是
 *不可观察，因此可以移除。
 *
 *以前我们使用OpenSSL，它试图通过a）实现
 *memset in assembly on x86和b）将函数放入自己的文件中
 *对于其他平台。
 *
 *此更改消除了使用asm指令
 *吓跑编译器。正如我们的编译人员所说，这是
 *足够，并将继续如此。
 *
 *亚当·兰利<agl@google.com>
 *承诺：AD1907FE73334D6C696C8539646C211B11178F20F
 *Boringssl（许可证：ISC）
 **/

void memory_cleanse(void *ptr, size_t len)
{
    std::memset(ptr, 0, len);

    /*据我们所知，这足以打破任何
       可能会试图消除“多余”的记忆。如果有一个简单的方法
       检测memset_，最好使用它。*/

#if defined(_MSC_VER)
    SecureZeroMemory(ptr, len);
#else
    __asm__ __volatile__("" : : "r"(ptr) : "memory");
#endif
}
