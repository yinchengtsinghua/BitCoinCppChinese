
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

#ifndef BITCOIN_COMPAT_H
#define BITCOIN_COMPAT_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#ifdef WIN32
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef FD_SETSIZE
#undef FD_SETSIZE //防止重新定义编译器警告
#endif
#define FD_SETSIZE 1024 //fd_集合中fds的最大数目

#include <winsock2.h>     //必须在mswsock.h和windows.h之前包含

#include <mswsock.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdint.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <limits.h>
#include <netdb.h>
#include <unistd.h>
#endif

#ifndef WIN32
typedef unsigned int SOCKET;
#include <errno.h>
#define WSAGetLastError()   errno
#define WSAEINVAL           EINVAL
#define WSAEALREADY         EALREADY
#define WSAEWOULDBLOCK      EWOULDBLOCK
#define WSAEMSGSIZE         EMSGSIZE
#define WSAEINTR            EINTR
#define WSAEINPROGRESS      EINPROGRESS
#define WSAEADDRINUSE       EADDRINUSE
#define WSAENOTSOCK         EBADF
#define INVALID_SOCKET      (SOCKET)(~0)
#define SOCKET_ERROR        -1
#endif

#ifdef WIN32
#ifndef S_IRUSR
#define S_IRUSR             0400
#define S_IWUSR             0200
#endif
#else
#define MAX_PATH            1024
#endif
#ifdef _MSC_VER
#if !defined(ssize_t)
#ifdef _WIN64
typedef int64_t ssize_t;
#else
typedef int32_t ssize_t;
#endif
#endif
#endif

#if HAVE_DECL_STRNLEN == 0
size_t strnlen( const char *start, size_t max_len);
#endif //哈维克

#ifndef WIN32
typedef void* sockopt_arg_type;
#else
typedef char* sockopt_arg_type;
#endif

//注意，这两种方法都应该与当前使用的轮询方法一起使用，但最好是安全的。
//win32 poll已中断https://daniel.haxx.se/blog/2012/10/10/wsapoll-is-break/
//_uuu Apple_uuuu民意测验被打破https://github.com/bitcoin/bitcoin/pull/14336 issuecomment-437384408
#if defined(__linux__)
#define USE_POLL
#endif

bool static inline IsSelectableSocket(const SOCKET& s) {
#if defined(USE_POLL) || defined(WIN32)
    return true;
#else
    return (s < FD_SETSIZE);
#endif
}

#endif //比特币兼容
