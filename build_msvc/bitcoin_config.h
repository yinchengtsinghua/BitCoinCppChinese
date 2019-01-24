
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
#ifndef BITCOIN_BITCOIN_CONFIG_H
#define BITCOIN_BITCOIN_CONFIG_H

/*定义是否生成通用（内部助手宏）*/
/*Undef AC_Apple_Universal_构建*/

/*版本生成*/
#define CLIENT_VERSION_BUILD 0

/*版本已发布*/
#define CLIENT_VERSION_IS_RELEASE false

/*主要版本*/
#define CLIENT_VERSION_MAJOR 1

/*次要版本*/
#define CLIENT_VERSION_MINOR 17

/*修编*/
#define CLIENT_VERSION_REVISION 99

/*%s替换前的版权所有者*/
#define COPYRIGHT_HOLDERS "The %s developers"

/*版权所有人*/
#define COPYRIGHT_HOLDERS_FINAL "The Bitcoin Core developers"

/*在版权所有者字符串中替换%s*/
#define COPYRIGHT_HOLDERS_SUBSTITUTION "Bitcoin Core"

/*版权年*/
#define COPYRIGHT_YEAR 2018

/*定义为1以启用钱包功能*/
#define ENABLE_WALLET 1

/*定义为1以启用ZMQ函数*/
#define ENABLE_ZMQ 1

/*参数和返回值类型*/
/*undef fdelt_型*/

/*定义Boost库是否可用*/
/*好的，加油/**/

/*定义boost:：chrono库是否可用*/

/*很好，让你加速。

/*定义boost:：filesystem库是否可用*/

/*很好，让\boost \u文件系统/**/

/*定义boost:：program_选项库是否可用*/

/*很好，有增强程序选项/**/

/*定义boost：：系统库是否可用*/

/*很好，让你的增压系统/**/

/*定义boost:：thread库是否可用*/

/*好的，让你的线/

/*定义Boost:：Unit_Test_框架库是否可用*/

/*很好，让你的主机测试框架/**/

/*如果有<byteswap.h>头文件，则定义为1。*/

/*亡灵有诳字节swap诳h*/

/*如果已建立共识库，则定义此符号*/
#define HAVE_CONSENSUS_LIB 1

/*定义编译器是否支持基本C++ 11语法*/
#define HAVE_CXX11 1

/*如果声明为“be16toh”，则定义为1；如果声明为“be16toh”，则定义为0
   不要。*/

#define HAVE_DECL_BE16TOH 0

/*如果声明为“be32toh”，则定义为1；如果声明为“be32toh”，则定义为0
   不要。*/

#define HAVE_DECL_BE32TOH 0

/*如果声明为“be64toh”，则定义为1；如果声明为“be64toh”，则定义为0
   不要。*/

#define HAVE_DECL_BE64TOH 0

/*如果声明为“bswap_16”，则定义为1；如果声明为“bswap_16”，则定义为0。
   不要。*/

#define HAVE_DECL_BSWAP_16 0

/*如果声明为“bswap_32”，则定义为1；如果声明为“bswap_32”，则定义为0。
   不要。*/

#define HAVE_DECL_BSWAP_32 0

/*如果声明为“bswap_64”，则定义为1；如果声明为“bswap_64”，则定义为0。
   不要。*/

#define HAVE_DECL_BSWAP_64 0

/*如果有'daemon'的声明，则定义为1；如果没有，则定义为0。
   **/

#define HAVE_DECL_DAEMON 0

/*如果有“evp-md-ctx-new”的声明，则定义为1；如果有，则定义为0
   你没有。*/

//define have诳decl诳evp诳md诳ctx诳new 1

/*如果声明为“htobe16”，则定义为1；如果声明为“htobe16”，则定义为0
   不要。*/

#define HAVE_DECL_HTOBE16 0

/*如果声明为“htobe32”，则定义为1；如果声明为“htobe32”，则定义为0
   不要。*/

#define HAVE_DECL_HTOBE32 0

/*如果声明为“htobe64”，则定义为1；如果声明为“htobe64”，则定义为0
   不要。*/

#define HAVE_DECL_HTOBE64 0

/*如果声明为“htole16”，则定义为1；如果声明为“htole16”，则定义为0
   不要。*/

#define HAVE_DECL_HTOLE16 0

/*如果声明为“htole32”，则定义为1；如果声明为“htole32”，则定义为0
   不要。*/

#define HAVE_DECL_HTOLE32 0

/*如果声明为“htole64”，则定义为1；如果声明为“htole64”，则定义为0
   不要。*/

#define HAVE_DECL_HTOLE64 0

/*如果声明为“le16toh”，则定义为1；如果声明为“le16toh”，则定义为0
   不要。*/

#define HAVE_DECL_LE16TOH 0

/*如果声明为“le32toh”，则定义为1；如果声明为“le32toh”，则定义为0
   不要。*/

#define HAVE_DECL_LE32TOH 0

/*如果声明为“le64toh”，则定义为1；如果声明为“le64toh”，则定义为0
   不要。*/

#define HAVE_DECL_LE64TOH 0

/*如果有“strError”的声明，则定义为1；如果有，则定义为0。
   不要。*/

#define HAVE_DECL_STRERROR_R 0

/*如果声明为“strnlen”，则定义为1；如果声明为“strnlen”，则定义为0。
   不要。*/

#define HAVE_DECL_STRNLEN 1

/*如果有“builtin”的声明，则定义为1；如果有声明，则定义为0。
   不要。*/

//定义have_decl_uuuu builtin_CLZ 1

/*如果您有“内置”clzl的声明，则定义为1；如果有，则定义为0
   你没有。*/

//define have_decl_uuuu builtin_clzl 1

/*如果您有“内置”clzll的声明，则定义为1；如果有，则定义为0
   你没有。*/

//定义have诳decl_uuuu builtin_clzll 1

/*如果您有<dlfcn.h>头文件，则定义为1。*/
/*不死族有诳dlfcn诳h*/

/*如果有<endian.h>头文件，则定义为1。*/
/*亡灵有诳*/

/*如果系统具有“dllexport”函数属性，则定义为1*/
#define HAVE_FUNC_ATTRIBUTE_DLLEXPORT 1

/*如果系统具有“dllimport”函数属性，则定义为1*/
#define HAVE_FUNC_ATTRIBUTE_DLLIMPORT 1

/*如果系统具有“可见性”功能属性，则定义为1*/
#define HAVE_FUNC_ATTRIBUTE_VISIBILITY 1

/*如果bsd get熵系统调用可用，则定义此符号*/
/*亡灵有熵*/

/*如果bsd get熵系统调用可用于
   系统/随机*/

/*亡灵有熵*/

/*如果有<inttypes.h>头文件，则定义为1。*/
#define HAVE_INTTYPES_H 1

/*如果有“advapi32”库（-ladvapi32），则定义为1。*/
#define HAVE_LIBADVAPI32 1

/*如果有“comctl32”库（-lcomctl32），则定义为1。*/
#define HAVE_LIBCOMCTL32 1

/*如果有“comdlg32”库（-lcomdlg32），则定义为1。*/
#define HAVE_LIBCOMDLG32 1

/*如果有“crypt32”库（-lcrypt32），则定义为1。*/
#define HAVE_LIBCRYPT32 1

/*如果有“gdi32”库（-lgdi32），则定义为1。*/
#define HAVE_LIBGDI32 1

/*如果有“imm32”库（-limm32），则定义为1。*/
#define HAVE_LIBIMM32 1

/*如果有“iphlapi”库（-liphlpapi），则定义为1。*/
#define HAVE_LIBIPHLPAPI 1

/*如果有“kernel32”库（-lkernel32），则定义为1。*/
#define HAVE_LIBKERNEL32 1

/*如果有“mingwthrd”库（-lmingwthrd），则定义为1。*/
#define HAVE_LIBMINGWTHRD 1

/*如果有“mswsock”库（-lmswsock），则定义为1。*/
#define HAVE_LIBMSWSOCK 1

/*如果有“ole32”库（-lole32），则定义为1。*/
#define HAVE_LIBOLE32 1

/*如果有“oleaut32”库（-loleaut32），则定义为1。*/
#define HAVE_LIBOLEAUT32 1

/*如果有'rpcrt4'库（-lrpcrt4），则定义为1。*/
#define HAVE_LIBRPCRT4 1

/*如果有“rt”库（-lrt），则定义为1。*/
/*不死族有自由*/

/*如果有“shell32”库（-lshell32），则定义为1。*/
#define HAVE_LIBSHELL32 1

/*如果有“shlwapi”库（-lshlwapi），则定义为1。*/
#define HAVE_LIBSHLWAPI 1

/*如果有“ssp”库（-lssp），则定义为1。*/
#define HAVE_LIBSSP 1

/*如果有“user32”库（-luser32），则定义为1。*/
#define HAVE_LIBUSER32 1

/*如果有“uuid”库（-luuid），则定义为1。*/
#define HAVE_LIBUUID 1

/*如果有“winmm”库（-lwinmm），则定义为1。*/
#define HAVE_LIBWINMM 1

/*如果有“winspool”库（-lwinspool），则定义为1。*/
#define HAVE_LIBWINSPOOL 1

/*如果有“ws2_32”库（-lws2_32），则定义为1。*/
#define HAVE_LIBWS2_32 1

/*如果有“z”库（-lz），则定义为1。*/
#define HAVE_LIBZ_ 1

/*如果您有malloc_信息，请定义此符号*/
/*亡灵有诳malloc诳信息*/

/*如果您的mallopt带有m洹arena洹max，请定义此符号*/
/*不死族拥有诳Mallopt诳Arena诳Max*/

/*如果有<memory.h>头文件，则定义为1。*/
#define HAVE_MEMORY_H 1

/*如果有<miniupnpc/miniupnpc.h>头文件，则定义为1。*/
#define HAVE_MINIUPNPC_MINIUPNPC_H 1

/*如果您有<miniupnpc/miniwget.h>头文件，则定义为1。*/
#define HAVE_MINIUPNPC_MINIWGET_H 1

/*如果您有<miniupnpc/upnpcommands.h>头文件，则定义为1。*/
#define HAVE_MINIUPNPC_UPNPCOMMANDS_H 1

/*如果您有<miniupNPC/upnpErrors.h>头文件，则定义为1。*/
#define HAVE_MINIUPNPC_UPNPERRORS_H 1

/*如果您有消息，请定义此符号*/
/*亡灵有味精*/

/*如果您有消息，请定义此符号*/
/*undef have诳msg诳nosignal*/

/*定义是否有POSIX线程库和头文件。*/
//定义有螺纹1

/*让pthread-prio-u继承。*/
//定义havepthread_prio_inherit 1

/*如果有<stdint.h>头文件，则定义为1。*/
#define HAVE_STDINT_H 1

/*如果有<stdio.h>头文件，则定义为1。*/
#define HAVE_STDIO_H 1

/*如果有<stdlib.h>头文件，则定义为1。*/
#define HAVE_STDLIB_H 1

/*如果有“strError”函数，则定义为1。*/
/*亡灵有灵魂*/

/*如果有<strings.h>头文件，则定义为1。*/
#define HAVE_STRINGS_H 1

/*如果有<string.h>头文件，则定义为1。*/
#define HAVE_STRING_H 1

/*如果bsd sysctl（kern-arnd）可用，则定义此符号*/
/*undef有系统控制*/

/*如果有<sys/endian.h>头文件，则定义为1。*/
/*亡灵有系统*/

/*如果Linux GetRandom系统调用可用，则定义此符号*/
/*不死族有撊sys撊getrandom*/

/*如果您有<sys/prctl.h>头文件，则定义为1。*/
/*亡灵有系统*/

/*如果有<sys/select.h>头文件，则定义为1。*/
/*亡灵让系统选择*/

/*如果您有<sys/stat.h>头文件，则定义为1。*/
#define HAVE_SYS_STAT_H 1

/*如果有<sys/types.h>头文件，则定义为1。*/
#define HAVE_SYS_TYPES_H 1

/*如果有<unistd.h>头文件，则定义为1。*/
//定义有统一的

/*定义是否支持可见性属性。*/
#define HAVE_VISIBILITY_ATTRIBUTE 1

/*如果增强睡眠有效，则定义此符号*/
/*亡灵有工作的睡眠*/

/*如果有效，请定义此符号。*/
#define HAVE_WORKING_BOOST_SLEEP_FOR 1

/*定义到libtool存储卸载库的子目录。*/
#define LT_OBJDIR ".libs/"

/*定义此包的错误报告应发送到的地址。*/
#define PACKAGE_BUGREPORT "https://github.com/比特币/比特币/问题”

/*定义为此包的全名。*/
#define PACKAGE_NAME "Bitcoin Core"

/*定义为此包的全名和版本。*/
#define PACKAGE_STRING "Bitcoin Core 0.17.99"

/*将此包的短名称定义为一个符号。*/
#define PACKAGE_TARNAME "bitcoin"

/*定义为此包的主页。*/
#define PACKAGE_URL "https://bitcoincore.org/”

/*定义为此包的版本。*/
#define PACKAGE_VERSION "0.17.99"

/*如果此常量在上使用非标准名称，则定义为必要的符号
   你的系统。*/

/*未定义pthread_创建可接合*/

/*如果qt平台是cocoa，则定义此符号*/
/*undef qt_qpa_平台_cocoa*/

/*如果存在最小qt平台，则定义此符号*/
#define QT_QPA_PLATFORM_MINIMAL 1

/*如果Qt平台是Windows，则定义此符号*/
#define QT_QPA_PLATFORM_WINDOWS 1

/*如果qt平台是xcb，则定义此符号*/
/*undef qt_qpa_平台_xcb*/

/*如果qt插件是静态的，则定义此符号*/
#define QT_STATICPLUGIN 1

/*如果有ansi c头文件，则定义为1。*/
#define STDC_HEADERS 1

/*如果strError返回char*，则定义为1。*/
/*undef strerror_r_char_p*/

/*定义此符号以内置程序集例程*/
//定义使用

/*如果启用了覆盖范围，则定义此符号*/
/*未定义使用范围*/

/*定义是否应在中编译DBUS支持*/
/*使用总线*/

/*定义是否应在*/
//定义使用代码1

/*如果未定义，则不编译UPNP支持，否则值（0或1）将确定
   默认状态*/

//定义使用

/*如果处理器存储的单词最多，则将单词“bigendian”定义为1。
   有效字节优先（如摩托罗拉和SPARC，与Intel不同）。*/

#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/*不死字诳bigendian*/
# endif
#endif

/*在Mac OS X 10.5上启用大型inode编号。*/
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/*在可设置文件偏移量的主机上，文件偏移量中的位数。*/
#define _FILE_OFFSET_BITS 64

/*在AIX样式的主机上为大型文件定义。*/
/*取消定义大文件*/

#endif //比特币配置
