
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

#ifndef __cplusplus
#error This header can only be compiled as C++.
#endif

#ifndef BITCOIN_PROTOCOL_H
#define BITCOIN_PROTOCOL_H

#include <netaddress.h>
#include <serialize.h>
#include <uint256.h>
#include <version.h>

#include <atomic>
#include <stdint.h>
#include <string>

/*消息标题。
 （4）消息开始。
 *（12）命令。
 *（4）大小。
 *（4）校验和。
 **/

class CMessageHeader
{
public:
    static constexpr size_t MESSAGE_START_SIZE = 4;
    static constexpr size_t COMMAND_SIZE = 12;
    static constexpr size_t MESSAGE_SIZE_SIZE = 4;
    static constexpr size_t CHECKSUM_SIZE = 4;
    static constexpr size_t MESSAGE_SIZE_OFFSET = MESSAGE_START_SIZE + COMMAND_SIZE;
    static constexpr size_t CHECKSUM_OFFSET = MESSAGE_SIZE_OFFSET + MESSAGE_SIZE_SIZE;
    static constexpr size_t HEADER_SIZE = MESSAGE_START_SIZE + COMMAND_SIZE + MESSAGE_SIZE_SIZE + CHECKSUM_SIZE;
    typedef unsigned char MessageStartChars[MESSAGE_START_SIZE];

    explicit CMessageHeader(const MessageStartChars& pchMessageStartIn);
    CMessageHeader(const MessageStartChars& pchMessageStartIn, const char* pszCommand, unsigned int nMessageSizeIn);

    std::string GetCommand() const;
    bool IsValid(const MessageStartChars& messageStart) const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(pchMessageStart);
        READWRITE(pchCommand);
        READWRITE(nMessageSize);
        READWRITE(pchChecksum);
    }

    char pchMessageStart[MESSAGE_START_SIZE];
    char pchCommand[COMMAND_SIZE];
    uint32_t nMessageSize;
    uint8_t pchChecksum[CHECKSUM_SIZE];
};

/*
 *比特币协议报文类型。添加新消息类型时，不要忘记
 *更新protocol.cpp中的所有netmessagetype。
 **/

namespace NetMsgType {

/*
 *版本消息提供了有关将节点发送到
 *连接开始时的接收节点。
 *@请参见https://bitcoin.org/en/developer-reference版本
 **/

extern const char *VERSION;
/*
 *verack消息确认先前收到的版本消息，
 *通知连接节点可以开始发送其他消息。
 *@参见https://bitcoin.org/en/developer-reference verack
 **/

extern const char *VERACK;
/*
 *地址（IP地址）消息为上的对等端中继连接信息。
 *网络。
 *@见https://bitcoin.org/en/developer-reference addr
 **/

extern const char *ADDR;
/*
 *INV报文（库存报文）发送一个或多个
 *传输对等端已知的对象。
 *@参见https://bitcoin.org/en/developer-reference inv
 **/

extern const char *INV;
/*
 *getdata消息从另一个节点请求一个或多个数据对象。
 *@见https://bitcoin.org/en/developer-reference getdata
 **/

extern const char *GETDATA;
/*
 *MerkleBlock消息是对请求
 *使用库存类型msg_merkleblock阻止。
 *@自协议版本70001起，如BIP37所述。
 *@见https://bitcoin.org/en/developer-reference merkleblock
 **/

extern const char *MERKLEBLOCK;
/*
 *getBlocks消息请求提供块头的inv消息
 *从区块链中的特定点开始散列。
 *@参见https://bitcoin.org/en/developer-reference getblocks
 **/

extern const char *GETBLOCKS;
/*
 *getheaders消息请求提供块的headers消息
 *从区块链中的特定点开始的收割台。
 *@自协议版本31800起。
 *@参见https://bitcoin.org/en/developer-reference getheaders
 **/

extern const char *GETHEADERS;
/*
 *Tx消息发送单个事务。
 *@见https://bitcoin.org/en/developer-reference tx
 **/

extern const char *TX;
/*
 *headers消息将一个或多个块头发送到一个节点，该节点
 *以前请求了带有getheaders消息的某些头。
 *@自协议版本31800起。
 *@请参见https://bitcoin.org/en/developer-reference headers
 **/

extern const char *HEADERS;
/*
 *块消息传输单个序列化块。
 *@参见https://bitcoin.org/en/developer-reference block
 **/

extern const char *BLOCK;
/*
 *getaddr消息从接收节点请求addr消息，
 *最好是一个具有许多其他接收节点IP地址的节点。
 *@参见https://bitcoin.org/en/developer-reference getaddr
 **/

extern const char *GETADDR;
/*
 *mempool消息请求接收
 *节点已验证为有效，但尚未出现在块中。
 *@自协议版本60002起。
 *@参见https://bitcoin.org/en/developer-reference mempool
 **/

extern const char *MEMPOOL;
/*
 *定期发送ping消息以帮助确认接收
 *对等机仍处于连接状态。
 *@见https://bitcoin.org/en/developer-reference ping
 **/

extern const char *PING;
/*
 *pong消息回复ping消息，向ping节点证明
 *积水节点仍处于活动状态。
 *@自协议版本60001起，如BIP31所述。
 *@见https://bitcoin.org/en/developer-reference pong
 **/

extern const char *PONG;
/*
 *NotFound消息是对请求
 *对象接收节点没有可用于中继的对象。
 *@自协议版本70001起。
 *@见https://bitcoin.org/en/developer-reference未找到
 **/

extern const char *NOTFOUND;
/*
 *filterLoad消息通知接收端过滤所有中继
 *事务和通过提供的过滤器请求的merkle块。
 *@自协议版本70001起，如BIP37所述。
 *仅适用于自协议版本以来的服务位节点_bloom
 *70011，如BIP111所述。
 *@参见https://bitcoin.org/en/developer-reference filterload
 **/

extern const char *FILTERLOAD;
/*
 *filterAdd消息通知接收对等方将单个元素添加到
 *以前设置Bloom过滤器，如新的公钥。
 *@自协议版本70001起，如BIP37所述。
 *仅适用于自协议版本以来的服务位节点_bloom
 *70011，如BIP111所述。
 *@参见https://bitcoin.org/en/developer-reference filteradd
 **/

extern const char *FILTERADD;
/*
 *filterclear消息告诉接收端删除以前设置的
 *布隆过滤器。
 *@自协议版本70001起，如BIP37所述。
 *仅适用于自协议版本以来的服务位节点_bloom
 *70011，如BIP111所述。
 *@参见https://bitcoin.org/en/developer-reference filterclear
 **/

extern const char *FILTERCLEAR;
/*
 *拒绝消息通知接收节点
 *邮件已被拒绝。
 *@自协议版本70002起，如BIP61所述。
 *@参见https://bitcoin.org/en/developer-reference拒绝
 **/

extern const char *REJECT;
/*
 *表示节点希望通过
 *“headers”消息而不是“inv”。
 *@自协议版本70012起，如BIP130所述。
 *@请参见https://bitcoin.org/en/developer-reference sendheaders
 **/

extern const char *SENDHEADERS;
/*
 *feefilter消息告诉接收点不要向我们输入任何Txs
 *不符合规定的最低费率。
 *@自协议版本70013起，如BIP133所述
 **/

extern const char *FEEFILTER;
/*
 *包含一个1字节的bool和8字节的le版本号。
 *表示节点愿意通过“cmpctblock”消息提供块。
 *可能表示节点希望通过
 *“cmpctblock”消息而不是“inv”，具体取决于消息内容。
 *@自协议版本70014起，如BIP 152所述
 **/

extern const char *SENDCMPCT;
/*
 *包含cblockheaderandshorttxids对象-提供头和
 *短TxID列表。
 *@自协议版本70014起，如BIP 152所述
 **/

extern const char *CMPCTBLOCK;
/*
 *包含BlockTransactions请求
 *对等端应以“blocktxn”消息响应。
 *@自协议版本70014起，如BIP 152所述
 **/

extern const char *GETBLOCKTXN;
/*
 *包含BlockTransactions。
 *响应“GetBlockTxn”消息发送。
 *@自协议版本70014起，如BIP 152所述
 **/

extern const char *BLOCKTXN;
};

/*获取所有有效消息类型的向量（见上文）*/
const std::vector<std::string> &getAllNetMessageTypes();

/*NServices旗*/
enum ServiceFlags : uint64_t {
//没有什么
    NODE_NONE = 0,
//节点网络是指节点能够服务于整个区块链。目前
//由所有比特币核心非删减节点设置，由SPV客户端或其他轻客户端取消设置。
    NODE_NETWORK = (1 << 0),
//node_getutxo表示节点能够响应getutxo协议请求。
//比特币核心不支持这一点，但名为比特币XT的补丁集支持这一点。
//有关如何实现的详细信息，请参阅BIP 64。
    NODE_GETUTXO = (1 << 1),
//node_bloom表示节点能够并且愿意处理bloom过滤连接。
//默认情况下用于支持此功能的比特币核心节点，而不公布此位，
//但从协议版本70011起不再执行（=no-bloom-version）
    NODE_BLOOM = (1 << 2),
//节点见证表示可以向节点请求块和事务，包括
//见证数据。
    NODE_WITNESS = (1 << 3),
//节点xthin表示节点支持xtreme-thinblocks
//如果关闭此选项，则节点将不提供服务，也不会发出XTHIN请求。
    NODE_XTHIN = (1 << 4),
//“节点网络限制”是指与“节点网络”相同，仅限于
//服务最后288（2天）街区
//有关如何实现的详细信息，请参阅BIP159。
    NODE_NETWORK_LIMITED = (1 << 10),

//位24-31保留用于临时实验。就挑一点吧
//未被使用，或使用不多，并通知
//比特币开发邮件列表。记住服务位只是
//未经验证的广告，因此您的代码必须对
//冲突和其他节点可能正在发布服务的情况
//实际上不支持。其他服务位应通过
//BIP工艺
};

/*
 *获取一组服务标志，这些标志对于给定的对等机是“可取的”。
 *
 *这些是对等机支持它们所需的标志。
 *对我们来说是“有趣的”（对我们来说是希望利用我们少数人中的一个）。
 *出站连接槽，用于或用于我们希望优先保留
 *它们之间的联系。
 *
 *相关的服务标志可能是特定于对等和状态的
 *对等机的版本可以确定需要哪些标志（例如
 *在我们寻找节点网络对等点的节点网络有限公司案例
 *除非他们设置node_network_limited，而我们没有IBD，其中
 *case node_network_limited就足够了）。
 *
 *因此，通常避免使用peerservices==node_none进行调用，除非
 *必须绝对避免使用特定于状态的标志。当与呼叫时
 *peerservices==node_none，返回的理想服务标志是
 *保证不会因状态而改变-即它们适用于
 *在描述我们认为是可取的同龄人时使用，但是
 *我们没有一组已确认的服务标志。
 *
 *如果更改了节点“无”返回值，则contrib/seeds/makeseeds.py
 *应适当更新以过滤相同节点。
 **/

ServiceFlags GetDesirableServiceFlags(ServiceFlags services);

/*设置当前IBD状态，以确定所需的服务标志*/
void SetServiceFlagsIBDCache(bool status);

/*
 *的快捷方式（services&getDesireableServiceFlags（services））。
 *==GetDesireableServiceFlags（服务），即确定给定的
 *一组服务标志足以使对等机“相关”。
 **/

static inline bool HasAllDesirableServiceFlags(ServiceFlags services) {
    return !(GetDesirableServiceFlags(services) & (~services));
}

/*
 *检查具有给定服务标志的对等机是否能够
 *强大的地址存储数据库。
 **/

static inline bool MayHaveUsefulAddressDB(ServiceFlags services) {
    return (services & NODE_NETWORK) || (services & NODE_NETWORK_LIMITED);
}

/*一个将有关它的信息作为对等端的C服务*/
class CAddress : public CService
{
public:
    CAddress();
    explicit CAddress(CService ipIn, ServiceFlags nServicesIn);

    void Init();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (ser_action.ForRead())
            Init();
        int nVersion = s.GetVersion();
        if (s.GetType() & SER_DISK)
            READWRITE(nVersion);
        if ((s.GetType() & SER_DISK) ||
            (nVersion >= CADDR_TIME_VERSION && !(s.GetType() & SER_GETHASH)))
            READWRITE(nTime);
        uint64_t nServicesInt = nServices;
        READWRITE(nServicesInt);
        nServices = static_cast<ServiceFlags>(nServicesInt);
        READWRITEAS(CService, *this);
    }

//TODO:设为私有（改进封装）
public:
    ServiceFlags nServices;

//仅限磁盘和网络
    unsigned int nTime;
};

/*GetData消息类型标志*/
const uint32_t MSG_WITNESS_FLAG = 1 << 30;
const uint32_t MSG_TYPE_MASK    = 0xffffffff >> 2;

/*获取数据/投资信息类型。
 *这些数字由协议定义。添加新值时，请确保
 *在相应的BIP中提及。
 **/

enum GetDataMsg
{
    UNDEFINED = 0,
    MSG_TX = 1,
    MSG_BLOCK = 2,
//以下只能出现在GetData中。INV总是使用TX或BLOCK。
MSG_FILTERED_BLOCK = 3,  //！<在BIP37中定义
MSG_CMPCT_BLOCK = 4,     //！<在BIP152中定义
MSG_WITNESS_BLOCK = MSG_BLOCK | MSG_WITNESS_FLAG, //！<在BIP144中定义
MSG_WITNESS_TX = MSG_TX | MSG_WITNESS_FLAG,       //！<在BIP144中定义
    MSG_FILTERED_WITNESS_BLOCK = MSG_FILTERED_BLOCK | MSG_WITNESS_FLAG,
};

/*发票报文数据*/
class CInv
{
public:
    CInv();
    CInv(int typeIn, const uint256& hashIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(type);
        READWRITE(hash);
    }

    friend bool operator<(const CInv& a, const CInv& b);

    std::string GetCommand() const;
    std::string ToString() const;

public:
    int type;
    uint256 hash;
};

#endif //比特币协议
