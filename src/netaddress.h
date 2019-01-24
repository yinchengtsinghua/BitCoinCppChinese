
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

#ifndef BITCOIN_NETADDRESS_H
#define BITCOIN_NETADDRESS_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <compat.h>
#include <serialize.h>
#include <span.h>

#include <stdint.h>
#include <string>
#include <vector>

enum Network
{
    NET_UNROUTABLE = 0,
    NET_IPV4,
    NET_IPV6,
    NET_ONION,
    NET_INTERNAL,

    NET_MAX,
};

/*IP地址（使用映射的IPv6范围（：：ffff:0:0/96）的IPv6或IPv4）*/
class CNetAddr
{
    protected:
unsigned char ip[16]; //按网络字节顺序
uint32_t scopeId{0}; //对于作用域/链接本地IPv6地址

    public:
        CNetAddr();
        explicit CNetAddr(const struct in_addr& ipv4Addr);
        void SetIP(const CNetAddr& ip);

    private:
        /*
         *设置原始IPv4或IPv6地址（按网络字节顺序）
         *@注意：网络只允许使用net_ipv4和net_ipv6。
         **/

        void SetRaw(Network network, const uint8_t *data);

    public:
        /*
          *将任意字符串转换为不可路由的IPv6地址。
          *有助于将解析地址映射回其源。
         **/

        bool SetInternal(const std::string& name);

bool SetSpecial(const std::string &strName); //对于TOR地址
bool IsBindAny() const; //任何同等产品
bool IsIPv4() const;    //IPv4映射地址（：：ffff:0:0/96，0.0.0.0/0）
bool IsIPv6() const;    //ipv6地址（未映射ipv4，未映射tor）
bool IsRFC1918() const; //IPv4专用网络（10.0.0.0/8、192.168.0.0/16、172.16.0.0/12）
bool IsRFC2544() const; //IPv4网络间通信（192.18.0.0/15）
bool IsRFC6598() const; //IPv4 ISP级NAT（100.64.0.0/10）
bool IsRFC5737() const; //IPv4文档地址（192.0.2.0/24、198.51.100.0/24、203.0.113.0/24）
bool IsRFC3849() const; //IPv6文档地址（2001:0db8:：/32）
bool IsRFC3927() const; //IPv4自动配置（169.254.0.0/16）
bool IsRFC3964() const; //IPv6 6to4隧道（2002:：/16）
bool IsRFC4193() const; //ipv6唯一本地（fc00:：/7）
bool IsRFC4380() const; //IPv6 Teredo隧道（2001:：/32）
bool IsRFC4843() const; //IPv6兰花（2001:10:：/28）
bool IsRFC4862() const; //IPv6自动配置（fe80:：/64）
bool IsRFC6052() const; //IPv6已知前缀（64:ff9b:：/96）
bool IsRFC6145() const; //IPv6 IPv4转换地址（：：ffff:0:0:0/96）
        bool IsTor() const;
        bool IsLocal() const;
        bool IsRoutable() const;
        bool IsInternal() const;
        bool IsValid() const;
        enum Network GetNetwork() const;
        std::string ToString() const;
        std::string ToStringIP() const;
        unsigned int GetByte(int n) const;
        uint64_t GetHash() const;
        bool GetInAddr(struct in_addr* pipv4Addr) const;
        std::vector<unsigned char> GetGroup() const;
        int GetReachabilityFrom(const CNetAddr *paddrPartner = nullptr) const;

        explicit CNetAddr(const struct in6_addr& pipv6Addr, const uint32_t scope = 0);
        bool GetIn6Addr(struct in6_addr* pipv6Addr) const;

        friend bool operator==(const CNetAddr& a, const CNetAddr& b);
        friend bool operator!=(const CNetAddr& a, const CNetAddr& b) { return !(a == b); }
        friend bool operator<(const CNetAddr& a, const CNetAddr& b);

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action) {
            READWRITE(ip);
        }

        friend class CSubNet;
};

class CSubNet
{
    protected:
///网络（基）地址
        CNetAddr network;
///netmask，按网络字节顺序
        uint8_t netmask[16];
///此值有效吗？（仅用于表示分析错误）
        bool valid;

    public:
        CSubNet();
        CSubNet(const CNetAddr &addr, int32_t mask);
        CSubNet(const CNetAddr &addr, const CNetAddr &mask);

//单个IP子网的构造函数（<ipv4>/32或<ipv6>/128）
        explicit CSubNet(const CNetAddr &addr);

        bool Match(const CNetAddr &addr) const;

        std::string ToString() const;
        bool IsValid() const;

        friend bool operator==(const CSubNet& a, const CSubNet& b);
        friend bool operator!=(const CSubNet& a, const CSubNet& b) { return !(a == b); }
        friend bool operator<(const CSubNet& a, const CSubNet& b);

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action) {
            READWRITE(network);
            READWRITE(netmask);
            READWRITE(valid);
        }
};

/*网络地址（cnetaddr）和（tcp）端口的组合*/
class CService : public CNetAddr
{
    protected:
uint16_t port; //主机顺序

    public:
        CService();
        CService(const CNetAddr& ip, unsigned short port);
        CService(const struct in_addr& ipv4Addr, unsigned short port);
        explicit CService(const struct sockaddr_in& addr);
        unsigned short GetPort() const;
        bool GetSockAddr(struct sockaddr* paddr, socklen_t *addrlen) const;
        bool SetSockAddr(const struct sockaddr* paddr);
        friend bool operator==(const CService& a, const CService& b);
        friend bool operator!=(const CService& a, const CService& b) { return !(a == b); }
        friend bool operator<(const CService& a, const CService& b);
        std::vector<unsigned char> GetKey() const;
        std::string ToString() const;
        std::string ToStringPort() const;
        std::string ToStringIPPort() const;

        CService(const struct in6_addr& ipv6Addr, unsigned short port);
        explicit CService(const struct sockaddr_in6& addr);

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action) {
            READWRITE(ip);
            READWRITE(WrapBigEndian(port));
        }
};

#endif //比特币网络地址
