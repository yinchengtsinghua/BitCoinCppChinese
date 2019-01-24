
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

#ifndef BITCOIN_NETBASE_H
#define BITCOIN_NETBASE_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <compat.h>
#include <netaddress.h>
#include <serialize.h>

#include <stdint.h>
#include <string>
#include <vector>

extern int nConnectTimeout;
extern bool fNameLookup;

//！-超时默认值
static const int DEFAULT_CONNECT_TIMEOUT = 5000;
//！- DNS默认值
static const int DEFAULT_NAME_LOOKUP = true;

class proxyType
{
public:
    proxyType(): randomize_credentials(false) {}
    explicit proxyType(const CService &_proxy, bool _randomize_credentials=false): proxy(_proxy), randomize_credentials(_randomize_credentials) {}

    bool IsValid() const { return proxy.IsValid(); }

    CService proxy;
    bool randomize_credentials;
};

enum Network ParseNetwork(std::string net);
std::string GetNetworkName(enum Network net);
bool SetProxy(enum Network net, const proxyType &addrProxy);
bool GetProxy(enum Network net, proxyType &proxyInfoOut);
bool IsProxy(const CNetAddr &addr);
bool SetNameProxy(const proxyType &addrProxy);
bool HaveNameProxy();
bool GetNameProxy(proxyType &nameProxyOut);
bool LookupHost(const char *pszName, std::vector<CNetAddr>& vIP, unsigned int nMaxSolutions, bool fAllowLookup);
bool LookupHost(const char *pszName, CNetAddr& addr, bool fAllowLookup);
bool Lookup(const char *pszName, CService& addr, int portDefault, bool fAllowLookup);
bool Lookup(const char *pszName, std::vector<CService>& vAddr, int portDefault, bool fAllowLookup, unsigned int nMaxSolutions);
CService LookupNumeric(const char *pszName, int portDefault = 0);
bool LookupSubNet(const char *pszName, CSubNet& subnet);
SOCKET CreateSocket(const CService &addrConnect);
bool ConnectSocketDirectly(const CService &addrConnect, const SOCKET& hSocketRet, int nTimeout, bool manual_connection);
bool ConnectThroughProxy(const proxyType &proxy, const std::string& strDest, int port, const SOCKET& hSocketRet, int nTimeout, bool *outProxyConnectionFailed);
/*返回网络错误代码的可读错误字符串*/
std::string NetworkErrorString(int err);
/*关闭插座并将hsocket设置为无效的插座*/
bool CloseSocket(SOCKET& hSocket);
/*禁用或启用套接字的阻塞模式*/
bool SetSocketNonBlocking(const SOCKET& hSocket, bool fNonBlocking);
/*在套接字上设置TCP节点标志*/
bool SetSocketNoDelay(const SOCKET& hSocket);
/*
 *将毫秒转换为结构时间值，例如select。
 **/

struct timeval MillisToTimeval(int64_t nTimeout);
void InterruptSocks5(bool interrupt);

#endif //比特币
