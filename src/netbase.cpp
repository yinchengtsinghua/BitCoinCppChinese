
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

#include <netbase.h>

#include <hash.h>
#include <sync.h>
#include <uint256.h>
#include <random.h>
#include <tinyformat.h>
#include <util/system.h>
#include <util/strencodings.h>

#include <atomic>

#ifndef WIN32
#include <fcntl.h>
#else
#include <codecvt>
#endif

#ifdef USE_POLL
#include <poll.h>
#endif

#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

//设置
static CCriticalSection cs_proxyInfos;
static proxyType proxyInfo[NET_MAX] GUARDED_BY(cs_proxyInfos);
static proxyType nameProxy GUARDED_BY(cs_proxyInfos);
int nConnectTimeout = DEFAULT_CONNECT_TIMEOUT;
bool fNameLookup = DEFAULT_NAME_LOOKUP;

//对于非常慢的代理（如Tor），需要足够的协商时间（毫秒）
static const int SOCKS5_RECV_TIMEOUT = 20 * 1000;
static std::atomic<bool> interruptSocks5Recv(false);

enum Network ParseNetwork(std::string net) {
    Downcase(net);
    if (net == "ipv4") return NET_IPV4;
    if (net == "ipv6") return NET_IPV6;
    if (net == "onion") return NET_ONION;
    if (net == "tor") {
        LogPrintf("Warning: net name 'tor' is deprecated and will be removed in the future. You should use 'onion' instead.\n");
        return NET_ONION;
    }
    return NET_UNROUTABLE;
}

std::string GetNetworkName(enum Network net) {
    switch(net)
    {
    case NET_IPV4: return "ipv4";
    case NET_IPV6: return "ipv6";
    case NET_ONION: return "onion";
    default: return "";
    }
}

bool static LookupIntern(const char *pszName, std::vector<CNetAddr>& vIP, unsigned int nMaxSolutions, bool fAllowLookup)
{
    vIP.clear();

    {
        CNetAddr addr;
        if (addr.SetSpecial(std::string(pszName))) {
            vIP.push_back(addr);
            return true;
        }
    }

    struct addrinfo aiHint;
    memset(&aiHint, 0, sizeof(struct addrinfo));

    aiHint.ai_socktype = SOCK_STREAM;
    aiHint.ai_protocol = IPPROTO_TCP;
    aiHint.ai_family = AF_UNSPEC;
#ifdef WIN32
    aiHint.ai_flags = fAllowLookup ? 0 : AI_NUMERICHOST;
#else
    aiHint.ai_flags = fAllowLookup ? AI_ADDRCONFIG : AI_NUMERICHOST;
#endif
    struct addrinfo *aiRes = nullptr;
    int nErr = getaddrinfo(pszName, nullptr, &aiHint, &aiRes);
    if (nErr)
        return false;

    struct addrinfo *aiTrav = aiRes;
    while (aiTrav != nullptr && (nMaxSolutions == 0 || vIP.size() < nMaxSolutions))
    {
        CNetAddr resolved;
        if (aiTrav->ai_family == AF_INET)
        {
            assert(aiTrav->ai_addrlen >= sizeof(sockaddr_in));
            resolved = CNetAddr(((struct sockaddr_in*)(aiTrav->ai_addr))->sin_addr);
        }

        if (aiTrav->ai_family == AF_INET6)
        {
            assert(aiTrav->ai_addrlen >= sizeof(sockaddr_in6));
            struct sockaddr_in6* s6 = (struct sockaddr_in6*) aiTrav->ai_addr;
            resolved = CNetAddr(s6->sin6_addr, s6->sin6_scope_id);
        }
        /*从不允许解析为内部地址。认为任何这样的结果无效*/
        if (!resolved.IsInternal()) {
            vIP.push_back(resolved);
        }

        aiTrav = aiTrav->ai_next;
    }

    freeaddrinfo(aiRes);

    return (vIP.size() > 0);
}

bool LookupHost(const char *pszName, std::vector<CNetAddr>& vIP, unsigned int nMaxSolutions, bool fAllowLookup)
{
    std::string strHost(pszName);
    if (strHost.empty())
        return false;
    if (strHost.front() == '[' && strHost.back() == ']') {
        strHost = strHost.substr(1, strHost.size() - 2);
    }

    return LookupIntern(strHost.c_str(), vIP, nMaxSolutions, fAllowLookup);
}

bool LookupHost(const char *pszName, CNetAddr& addr, bool fAllowLookup)
{
    std::vector<CNetAddr> vIP;
    LookupHost(pszName, vIP, 1, fAllowLookup);
    if(vIP.empty())
        return false;
    addr = vIP.front();
    return true;
}

bool Lookup(const char *pszName, std::vector<CService>& vAddr, int portDefault, bool fAllowLookup, unsigned int nMaxSolutions)
{
    if (pszName[0] == 0)
        return false;
    int port = portDefault;
    std::string hostname;
    SplitHostPort(std::string(pszName), port, hostname);

    std::vector<CNetAddr> vIP;
    bool fRet = LookupIntern(hostname.c_str(), vIP, nMaxSolutions, fAllowLookup);
    if (!fRet)
        return false;
    vAddr.resize(vIP.size());
    for (unsigned int i = 0; i < vIP.size(); i++)
        vAddr[i] = CService(vIP[i], port);
    return true;
}

bool Lookup(const char *pszName, CService& addr, int portDefault, bool fAllowLookup)
{
    std::vector<CService> vService;
    bool fRet = Lookup(pszName, vService, portDefault, fAllowLookup, 1);
    if (!fRet)
        return false;
    addr = vService[0];
    return true;
}

CService LookupNumeric(const char *pszName, int portDefault)
{
    CService addr;
//“1.2:345”将无法解析IP，但仍将设置端口。
//如果IP无法解析，请重新初始化结果。
    if(!Lookup(pszName, addr, portDefault, false))
        addr = CService();
    return addr;
}

struct timeval MillisToTimeval(int64_t nTimeout)
{
    struct timeval timeout;
    timeout.tv_sec  = nTimeout / 1000;
    timeout.tv_usec = (nTimeout % 1000) * 1000;
    return timeout;
}

/*袜版*/
enum SOCKSVersion: uint8_t {
    SOCKS4 = 0x04,
    SOCKS5 = 0x05
};

/*为rfc1928中的方法定义的值*/
enum SOCKS5Method: uint8_t {
NOAUTH = 0x00,        //！<不需要身份验证
GSSAPI = 0x01,        //！< GSSAPI
USER_PASS = 0x02,     //！<用户名/密码
NO_ACCEPTABLE = 0xff, //！<没有可接受的方法
};

/*在rfc1928中为cmd定义的值*/
enum SOCKS5Command: uint8_t {
    CONNECT = 0x01,
    BIND = 0x02,
    UDP_ASSOCIATE = 0x03
};

/*在RFC1928中为rep定义的值*/
enum SOCKS5Reply: uint8_t {
SUCCEEDED = 0x00,        //！<成功
GENFAILURE = 0x01,       //！<一般故障
NOTALLOWED = 0x02,       //！<规则集不允许连接
NETUNREACHABLE = 0x03,   //！<无法访问网络
HOSTUNREACHABLE = 0x04,  //！<无法访问网络
CONNREFUSED = 0x05,      //！<连接被拒绝
TTLEXPIRED = 0x06,       //！<TTL过期
CMDUNSUPPORTED = 0x07,   //！<不支持命令
ATYPEUNSUPPORTED = 0x08, //！<不支持地址类型
};

/*在RFC1928中为ATYPE定义的值*/
enum SOCKS5Atyp: uint8_t {
    IPV4 = 0x01,
    DOMAINNAME = 0x03,
    IPV6 = 0x04,
};

/*可由中断接收返回的状态代码*/
enum class IntrRecvError {
    OK,
    Timeout,
    Disconnected,
    NetworkError,
    Interrupted
};

/*
 *从套接字读取字节。这将读取请求的完整字节数
 *或在出错或超时时返回false。
 *调用interruptsocks5（）可以中断此函数。
 *
 *@param要接收的数据缓冲区
 *@param len要接收的数据长度
 *@param receive操作超时（毫秒）
 *
 *@注意此功能要求hsocket处于非阻塞模式。
 **/

static IntrRecvError InterruptibleRecv(uint8_t* data, size_t len, int timeout, const SOCKET& hSocket)
{
    int64_t curTime = GetTimeMillis();
    int64_t endTime = curTime + timeout;
//在一个选择呼叫中等待的最长时间。它将一直持续到这个时间（毫秒）
//中断，中断在中断时中断
    const int64_t maxWait = 1000;
    while (len > 0 && curTime < endTime) {
ssize_t ret = recv(hSocket, (char*)data, len, 0); //乐观地首先尝试recv
        if (ret > 0) {
            len -= ret;
            data += ret;
} else if (ret == 0) { //意外断开
            return IntrRecvError::Disconnected;
} else { //其他错误或阻塞
            int nErr = WSAGetLastError();
            if (nErr == WSAEINPROGRESS || nErr == WSAEWOULDBLOCK || nErr == WSAEINVAL) {
                if (!IsSelectableSocket(hSocket)) {
                    return IntrRecvError::NetworkError;
                }
                int timeout_ms = std::min(endTime - curTime, maxWait);
#ifdef USE_POLL
                struct pollfd pollfd = {};
                pollfd.fd = hSocket;
                pollfd.events = POLLIN | POLLOUT;
                int nRet = poll(&pollfd, 1, timeout_ms);
#else
                struct timeval tval = MillisToTimeval(timeout_ms);
                fd_set fdset;
                FD_ZERO(&fdset);
                FD_SET(hSocket, &fdset);
                int nRet = select(hSocket + 1, &fdset, nullptr, nullptr, &tval);
#endif
                if (nRet == SOCKET_ERROR) {
                    return IntrRecvError::NetworkError;
                }
            } else {
                return IntrRecvError::NetworkError;
            }
        }
        if (interruptSocks5Recv)
            return IntrRecvError::Interrupted;
        curTime = GetTimeMillis();
    }
    return len == 0 ? IntrRecvError::OK : IntrRecvError::Timeout;
}

/*代理身份验证的凭据*/
struct ProxyCredentials
{
    std::string username;
    std::string password;
};

/*将socks5回复转换为错误消息*/
static std::string Socks5ErrorString(uint8_t err)
{
    switch(err) {
        case SOCKS5Reply::GENFAILURE:
            return "general failure";
        case SOCKS5Reply::NOTALLOWED:
            return "connection not allowed";
        case SOCKS5Reply::NETUNREACHABLE:
            return "network unreachable";
        case SOCKS5Reply::HOSTUNREACHABLE:
            return "host unreachable";
        case SOCKS5Reply::CONNREFUSED:
            return "connection refused";
        case SOCKS5Reply::TTLEXPIRED:
            return "TTL expired";
        case SOCKS5Reply::CMDUNSUPPORTED:
            return "protocol error";
        case SOCKS5Reply::ATYPEUNSUPPORTED:
            return "address type not supported";
        default:
            return "unknown";
    }
}

/*使用插座5连接（如RFC1928中所述）*/
static bool Socks5(const std::string& strDest, int port, const ProxyCredentials *auth, const SOCKET& hSocket)
{
    IntrRecvError recvr;
    LogPrint(BCLog::NET, "SOCKS5 connecting %s\n", strDest);
    if (strDest.size() > 255) {
        return error("Hostname too long");
    }
//接受的身份验证方法
    std::vector<uint8_t> vSocks5Init;
    vSocks5Init.push_back(SOCKSVersion::SOCKS5);
    if (auth) {
vSocks5Init.push_back(0x02); //方法的数量
        vSocks5Init.push_back(SOCKS5Method::NOAUTH);
        vSocks5Init.push_back(SOCKS5Method::USER_PASS);
    } else {
vSocks5Init.push_back(0x01); //方法的数量
        vSocks5Init.push_back(SOCKS5Method::NOAUTH);
    }
    ssize_t ret = send(hSocket, (const char*)vSocks5Init.data(), vSocks5Init.size(), MSG_NOSIGNAL);
    if (ret != (ssize_t)vSocks5Init.size()) {
        return error("Error sending to proxy");
    }
    uint8_t pchRet1[2];
    if ((recvr = InterruptibleRecv(pchRet1, 2, SOCKS5_RECV_TIMEOUT, hSocket)) != IntrRecvError::OK) {
        LogPrintf("Socks5() connect to %s:%d failed: InterruptibleRecv() timeout or other failure\n", strDest, port);
        return false;
    }
    if (pchRet1[0] != SOCKSVersion::SOCKS5) {
        return error("Proxy failed to initialize");
    }
    if (pchRet1[1] == SOCKS5Method::USER_PASS && auth) {
//执行用户名/密码验证（如RFC1929中所述）
        std::vector<uint8_t> vAuth;
vAuth.push_back(0x01); //用户/通过子协商的当前（且仅此）版本
        if (auth->username.size() > 255 || auth->password.size() > 255)
            return error("Proxy username or password too long");
        vAuth.push_back(auth->username.size());
        vAuth.insert(vAuth.end(), auth->username.begin(), auth->username.end());
        vAuth.push_back(auth->password.size());
        vAuth.insert(vAuth.end(), auth->password.begin(), auth->password.end());
        ret = send(hSocket, (const char*)vAuth.data(), vAuth.size(), MSG_NOSIGNAL);
        if (ret != (ssize_t)vAuth.size()) {
            return error("Error sending authentication to proxy");
        }
        LogPrint(BCLog::PROXY, "SOCKS5 sending proxy authentication %s:%s\n", auth->username, auth->password);
        uint8_t pchRetA[2];
        if ((recvr = InterruptibleRecv(pchRetA, 2, SOCKS5_RECV_TIMEOUT, hSocket)) != IntrRecvError::OK) {
            return error("Error reading proxy authentication response");
        }
        if (pchRetA[0] != 0x01 || pchRetA[1] != 0x00) {
            return error("Proxy authentication unsuccessful");
        }
    } else if (pchRet1[1] == SOCKS5Method::NOAUTH) {
//不执行身份验证
    } else {
        return error("Proxy requested wrong authentication method %02x", pchRet1[1]);
    }
    std::vector<uint8_t> vSocks5;
vSocks5.push_back(SOCKSVersion::SOCKS5); //版本协议版本
vSocks5.push_back(SOCKS5Command::CONNECT); //CMD连接
vSocks5.push_back(0x00); //保留的RSV必须为0
vSocks5.push_back(SOCKS5Atyp::DOMAINNAME); //ATYP域名
vSocks5.push_back(strDest.size()); //函数开始时检查长度<=255
    vSocks5.insert(vSocks5.end(), strDest.begin(), strDest.end());
    vSocks5.push_back((port >> 8) & 0xFF);
    vSocks5.push_back((port >> 0) & 0xFF);
    ret = send(hSocket, (const char*)vSocks5.data(), vSocks5.size(), MSG_NOSIGNAL);
    if (ret != (ssize_t)vSocks5.size()) {
        return error("Error sending to proxy");
    }
    uint8_t pchRet2[4];
    if ((recvr = InterruptibleRecv(pchRet2, 4, SOCKS5_RECV_TIMEOUT, hSocket)) != IntrRecvError::OK) {
        if (recvr == IntrRecvError::Timeout) {
            /*如果这里发生超时，这实际上意味着我们在连接时超时
             *到远程节点。这对于Tor很常见，因此不要打印
             *错误信息。*/

            return false;
        } else {
            return error("Error while reading proxy response");
        }
    }
    if (pchRet2[0] != SOCKSVersion::SOCKS5) {
        return error("Proxy failed to accept request");
    }
    if (pchRet2[1] != SOCKS5Reply::SUCCEEDED) {
//无法连接到不是代理错误的对等机
        LogPrintf("Socks5() connect to %s:%d failed: %s\n", strDest, port, Socks5ErrorString(pchRet2[1]));
        return false;
    }
if (pchRet2[2] != 0x00) { //保留字段必须为0
        return error("Error: malformed proxy response");
    }
    uint8_t pchRet3[256];
    switch (pchRet2[3])
    {
        case SOCKS5Atyp::IPV4: recvr = InterruptibleRecv(pchRet3, 4, SOCKS5_RECV_TIMEOUT, hSocket); break;
        case SOCKS5Atyp::IPV6: recvr = InterruptibleRecv(pchRet3, 16, SOCKS5_RECV_TIMEOUT, hSocket); break;
        case SOCKS5Atyp::DOMAINNAME:
        {
            recvr = InterruptibleRecv(pchRet3, 1, SOCKS5_RECV_TIMEOUT, hSocket);
            if (recvr != IntrRecvError::OK) {
                return error("Error reading from proxy");
            }
            int nRecv = pchRet3[0];
            recvr = InterruptibleRecv(pchRet3, nRecv, SOCKS5_RECV_TIMEOUT, hSocket);
            break;
        }
        default: return error("Error: malformed proxy response");
    }
    if (recvr != IntrRecvError::OK) {
        return error("Error reading from proxy");
    }
    if ((recvr = InterruptibleRecv(pchRet3, 2, SOCKS5_RECV_TIMEOUT, hSocket)) != IntrRecvError::OK) {
        return error("Error reading from proxy");
    }
    LogPrint(BCLog::NET, "SOCKS5 connected %s\n", strDest);
    return true;
}

SOCKET CreateSocket(const CService &addrConnect)
{
    struct sockaddr_storage sockaddr;
    socklen_t len = sizeof(sockaddr);
    if (!addrConnect.GetSockAddr((struct sockaddr*)&sockaddr, &len)) {
        LogPrintf("Cannot create socket for %s: unsupported network\n", addrConnect.ToString());
        return INVALID_SOCKET;
    }

    SOCKET hSocket = socket(((struct sockaddr*)&sockaddr)->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (hSocket == INVALID_SOCKET)
        return INVALID_SOCKET;

    if (!IsSelectableSocket(hSocket)) {
        CloseSocket(hSocket);
        LogPrintf("Cannot create connection: non-selectable socket created (fd >= FD_SETSIZE ?)\n");
        return INVALID_SOCKET;
    }

#ifdef SO_NOSIGPIPE
    int set = 1;
//在BSD上禁用SigPipe的不同方式
    setsockopt(hSocket, SOL_SOCKET, SO_NOSIGPIPE, (void*)&set, sizeof(int));
#endif

//禁用Nagle算法
    SetSocketNoDelay(hSocket);

//设置为非阻塞
    if (!SetSocketNonBlocking(hSocket, true)) {
        CloseSocket(hSocket);
        LogPrintf("ConnectSocketDirectly: Setting socket to non-blocking failed, error %s\n", NetworkErrorString(WSAGetLastError()));
    }
    return hSocket;
}

template<typename... Args>
static void LogConnectFailure(bool manual_connection, const char* fmt, const Args&... args) {
    std::string error_message = tfm::format(fmt, args...);
    if (manual_connection) {
        LogPrintf("%s\n", error_message);
    } else {
        LogPrint(BCLog::NET, "%s\n", error_message);
    }
}

bool ConnectSocketDirectly(const CService &addrConnect, const SOCKET& hSocket, int nTimeout, bool manual_connection)
{
    struct sockaddr_storage sockaddr;
    socklen_t len = sizeof(sockaddr);
    if (hSocket == INVALID_SOCKET) {
        LogPrintf("Cannot connect to %s: invalid socket\n", addrConnect.ToString());
        return false;
    }
    if (!addrConnect.GetSockAddr((struct sockaddr*)&sockaddr, &len)) {
        LogPrintf("Cannot connect to %s: unsupported network\n", addrConnect.ToString());
        return false;
    }
    if (connect(hSocket, (struct sockaddr*)&sockaddr, len) == SOCKET_ERROR)
    {
        int nErr = WSAGetLastError();
//wsaeinval在这里是因为Winsock的一些旧版本使用它
        if (nErr == WSAEINPROGRESS || nErr == WSAEWOULDBLOCK || nErr == WSAEINVAL)
        {
#ifdef USE_POLL
            struct pollfd pollfd = {};
            pollfd.fd = hSocket;
            pollfd.events = POLLIN | POLLOUT;
            int nRet = poll(&pollfd, 1, nTimeout);
#else
            struct timeval timeout = MillisToTimeval(nTimeout);
            fd_set fdset;
            FD_ZERO(&fdset);
            FD_SET(hSocket, &fdset);
            int nRet = select(hSocket + 1, nullptr, &fdset, nullptr, &timeout);
#endif
            if (nRet == 0)
            {
                LogPrint(BCLog::NET, "connection to %s timeout\n", addrConnect.ToString());
                return false;
            }
            if (nRet == SOCKET_ERROR)
            {
                LogPrintf("select() for %s failed: %s\n", addrConnect.ToString(), NetworkErrorString(WSAGetLastError()));
                return false;
            }
            socklen_t nRetSize = sizeof(nRet);
            if (getsockopt(hSocket, SOL_SOCKET, SO_ERROR, (sockopt_arg_type)&nRet, &nRetSize) == SOCKET_ERROR)
            {
                LogPrintf("getsockopt() for %s failed: %s\n", addrConnect.ToString(), NetworkErrorString(WSAGetLastError()));
                return false;
            }
            if (nRet != 0)
            {
                LogConnectFailure(manual_connection, "connect() to %s failed after select(): %s", addrConnect.ToString(), NetworkErrorString(nRet));
                return false;
            }
        }
#ifdef WIN32
        else if (WSAGetLastError() != WSAEISCONN)
#else
        else
#endif
        {
            LogConnectFailure(manual_connection, "connect() to %s failed: %s", addrConnect.ToString(), NetworkErrorString(WSAGetLastError()));
            return false;
        }
    }
    return true;
}

bool SetProxy(enum Network net, const proxyType &addrProxy) {
    assert(net >= 0 && net < NET_MAX);
    if (!addrProxy.IsValid())
        return false;
    LOCK(cs_proxyInfos);
    proxyInfo[net] = addrProxy;
    return true;
}

bool GetProxy(enum Network net, proxyType &proxyInfoOut) {
    assert(net >= 0 && net < NET_MAX);
    LOCK(cs_proxyInfos);
    if (!proxyInfo[net].IsValid())
        return false;
    proxyInfoOut = proxyInfo[net];
    return true;
}

bool SetNameProxy(const proxyType &addrProxy) {
    if (!addrProxy.IsValid())
        return false;
    LOCK(cs_proxyInfos);
    nameProxy = addrProxy;
    return true;
}

bool GetNameProxy(proxyType &nameProxyOut) {
    LOCK(cs_proxyInfos);
    if(!nameProxy.IsValid())
        return false;
    nameProxyOut = nameProxy;
    return true;
}

bool HaveNameProxy() {
    LOCK(cs_proxyInfos);
    return nameProxy.IsValid();
}

bool IsProxy(const CNetAddr &addr) {
    LOCK(cs_proxyInfos);
    for (int i = 0; i < NET_MAX; i++) {
        if (addr == static_cast<CNetAddr>(proxyInfo[i].proxy))
            return true;
    }
    return false;
}

bool ConnectThroughProxy(const proxyType &proxy, const std::string& strDest, int port, const SOCKET& hSocket, int nTimeout, bool *outProxyConnectionFailed)
{
//首先连接到代理服务器
    if (!ConnectSocketDirectly(proxy.proxy, hSocket, nTimeout, true)) {
        if (outProxyConnectionFailed)
            *outProxyConnectionFailed = true;
        return false;
    }
//做短袜谈判
    if (proxy.randomize_credentials) {
        ProxyCredentials random_auth;
        static std::atomic_int counter(0);
        random_auth.username = random_auth.password = strprintf("%i", counter++);
        if (!Socks5(strDest, (unsigned short)port, &random_auth, hSocket)) {
            return false;
        }
    } else {
        if (!Socks5(strDest, (unsigned short)port, 0, hSocket)) {
            return false;
        }
    }
    return true;
}

bool LookupSubNet(const char* pszName, CSubNet& ret)
{
    std::string strSubnet(pszName);
    size_t slash = strSubnet.find_last_of('/');
    std::vector<CNetAddr> vIP;

    std::string strAddress = strSubnet.substr(0, slash);
    if (LookupHost(strAddress.c_str(), vIP, 1, false))
    {
        CNetAddr network = vIP[0];
        if (slash != strSubnet.npos)
        {
            std::string strNetmask = strSubnet.substr(slash + 1);
            int32_t n;
//IPv4地址从偏移量12开始，前12个字节必须匹配，因此只需偏移量n
if (ParseInt32(strNetmask, &n)) { //如果有效数字，则采用/24语法
                ret = CSubNet(network, n);
                return ret.IsValid();
            }
else //如果不是有效数字，请尝试完整的网络掩码语法
            {
//从不允许查找网络掩码
                if (LookupHost(strNetmask.c_str(), vIP, 1, false)) {
                    ret = CSubNet(network, vIP[0]);
                    return ret.IsValid();
                }
            }
        }
        else
        {
            ret = CSubNet(network);
            return ret.IsValid();
        }
    }
    return false;
}

#ifdef WIN32
std::string NetworkErrorString(int err)
{
    wchar_t buf[256];
    buf[0] = 0;
    if(FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
            nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            buf, ARRAYSIZE(buf), nullptr))
    {
        return strprintf("%s (%d)", std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>,wchar_t>().to_bytes(buf), err);
    }
    else
    {
        return strprintf("Unknown error (%d)", err);
    }
}
#else
std::string NetworkErrorString(int err)
{
    char buf[256];
    buf[0] = 0;
    /*太糟糕了，有两个不兼容的实现
     *螺纹安全螺栓。*/

    const char *s;
/*def strerror_r_char_p/*gnu variant可以在传递的缓冲区外返回指针*/
    S=Strerrror_r（err，buf，sizeof（buf））；
else/*posix变量始终在缓冲区中返回消息*/

    s = buf;
    if (strerror_r(err, buf, sizeof(buf)))
        buf[0] = 0;
#endif
    return strprintf("%s (%d)", s, err);
}
#endif

bool CloseSocket(SOCKET& hSocket)
{
    if (hSocket == INVALID_SOCKET)
        return false;
#ifdef WIN32
    int ret = closesocket(hSocket);
#else
    int ret = close(hSocket);
#endif
    if (ret) {
        LogPrintf("Socket close failed: %d. Error: %s\n", hSocket, NetworkErrorString(WSAGetLastError()));
    }
    hSocket = INVALID_SOCKET;
    return ret != SOCKET_ERROR;
}

bool SetSocketNonBlocking(const SOCKET& hSocket, bool fNonBlocking)
{
    if (fNonBlocking) {
#ifdef WIN32
        u_long nOne = 1;
        if (ioctlsocket(hSocket, FIONBIO, &nOne) == SOCKET_ERROR) {
#else
        int fFlags = fcntl(hSocket, F_GETFL, 0);
        if (fcntl(hSocket, F_SETFL, fFlags | O_NONBLOCK) == SOCKET_ERROR) {
#endif
            return false;
        }
    } else {
#ifdef WIN32
        u_long nZero = 0;
        if (ioctlsocket(hSocket, FIONBIO, &nZero) == SOCKET_ERROR) {
#else
        int fFlags = fcntl(hSocket, F_GETFL, 0);
        if (fcntl(hSocket, F_SETFL, fFlags & ~O_NONBLOCK) == SOCKET_ERROR) {
#endif
            return false;
        }
    }

    return true;
}

bool SetSocketNoDelay(const SOCKET& hSocket)
{
    int set = 1;
    int rc = setsockopt(hSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&set, sizeof(int));
    return rc == 0;
}

void InterruptSocks5(bool interrupt)
{
    interruptSocks5Recv = interrupt;
}
