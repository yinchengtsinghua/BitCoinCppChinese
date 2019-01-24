
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2010 Satoshi Nakamoto
//版权所有（c）2009-2019比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_NET_H
#define BITCOIN_NET_H

#include <addrdb.h>
#include <addrman.h>
#include <amount.h>
#include <bloom.h>
#include <compat.h>
#include <crypto/siphash.h>
#include <hash.h>
#include <limitedmap.h>
#include <netaddress.h>
#include <policy/feerate.h>
#include <protocol.h>
#include <random.h>
#include <streams.h>
#include <sync.h>
#include <uint256.h>
#include <threadinterrupt.h>

#include <atomic>
#include <deque>
#include <stdint.h>
#include <thread>
#include <memory>
#include <condition_variable>

#ifndef WIN32
#include <arpa/inet.h>
#endif


class CScheduler;
class CNode;

/*Ping自动发出用于延迟探测和保留的间隔时间（秒）。*/
static const int PING_INTERVAL = 2 * 60;
/*等待ping响应（或不活动）后断开连接的时间。*/
static const int TIMEOUT_INTERVAL = 20 * 60;
/*每2分钟或120秒运行一次测隙器连接回路。*/
static const int FEELER_INTERVAL = 120;
/*“inv”协议消息中的最大条目数*/
static const unsigned int MAX_INV_SZ = 50000;
/*定位器中的最大条目数*/
static const unsigned int MAX_LOCATOR_SZ = 101;
/*在宣布前要累积的新地址的最大数目。*/
static const unsigned int MAX_ADDR_TO_SEND = 1000;
/*传入协议消息的最大长度（当前不接受超过4 MB的消息）。*/
static const unsigned int MAX_PROTOCOL_MESSAGE_LENGTH = 4 * 1000 * 1000;
/*'version'消息中strsubser的最大长度*/
static const unsigned int MAX_SUBVERSION_LENGTH = 256;
/*自动传出节点的最大数目*/
static const int MAX_OUTBOUND_CONNECTIONS = 8;
/*addnode传出节点的最大数目*/
static const int MAX_ADDNODE_CONNECTIONS = 8;
/*侦听默认值*/
static const bool DEFAULT_LISTEN = true;
/*-UPnP默认值*/
#ifdef USE_UPNP
static const bool DEFAULT_UPNP = USE_UPNP;
#else
static const bool DEFAULT_UPNP = false;
#endif
/*mapaskfor中的最大条目数*/
static const size_t MAPASKFOR_MAX_SZ = MAX_INV_SZ;
/*setaskfor中的最大条目数（由于getdata延迟而变大）*/
static const size_t SETASKFOR_MAX_SZ = 2 * MAX_INV_SZ;
/*要维护的最大对等连接数。*/
static const unsigned int DEFAULT_MAX_PEER_CONNECTIONS = 125;
/*-maxuploadTarget的默认值。0 =无限*/
static const uint64_t DEFAULT_MAX_UPLOAD_TARGET = 0;
/*-maxuploadTarget的默认时间范围。1天。*/
static const uint64_t MAX_UPLOAD_TIMEFRAME = 60 * 60 * 24;
/*仅限块的默认值*/
static const bool DEFAULT_BLOCKSONLY = false;
/*-对等超时默认值*/
static const int64_t DEFAULT_PEER_CONNECT_TIMEOUT = 60;

static const bool DEFAULT_FORCEDNSSEED = false;
static const size_t DEFAULT_MAXRECEIVEBUFFER = 5 * 1000;
static const size_t DEFAULT_MAXSENDBUFFER    = 1 * 1000;

//注意：调整此项时，请更新rpcnet:setban的帮助（“24h”）。
static const unsigned int DEFAULT_MISBEHAVING_BANTIME = 60 * 60 * 24;  //默认24小时禁令

typedef int64_t NodeId;

struct AddedNodeInfo
{
    std::string strAddedNode;
    CService resolvedAddress;
    bool fConnected;
    bool fInbound;
};

class CNodeStats;
class CClientUIInterface;

struct CSerializedNetMsg
{
    CSerializedNetMsg() = default;
    CSerializedNetMsg(CSerializedNetMsg&&) = default;
    CSerializedNetMsg& operator=(CSerializedNetMsg&&) = default;
//不复制，只移动。
    CSerializedNetMsg(const CSerializedNetMsg& msg) = delete;
    CSerializedNetMsg& operator=(const CSerializedNetMsg&) = delete;

    std::vector<unsigned char> data;
    std::string command;
};

class NetEventsInterface;
class CConnman
{
public:

    enum NumConnections {
        CONNECTIONS_NONE = 0,
        CONNECTIONS_IN = (1U << 0),
        CONNECTIONS_OUT = (1U << 1),
        CONNECTIONS_ALL = (CONNECTIONS_IN | CONNECTIONS_OUT),
    };

    struct Options
    {
        ServiceFlags nLocalServices = NODE_NONE;
        int nMaxConnections = 0;
        int nMaxOutbound = 0;
        int nMaxAddnode = 0;
        int nMaxFeeler = 0;
        int nBestHeight = 0;
        CClientUIInterface* uiInterface = nullptr;
        NetEventsInterface* m_msgproc = nullptr;
        unsigned int nSendBufferMaxSize = 0;
        unsigned int nReceiveFloodSize = 0;
        uint64_t nMaxOutboundTimeframe = 0;
        uint64_t nMaxOutboundLimit = 0;
        int64_t m_peer_connect_timeout = DEFAULT_PEER_CONNECT_TIMEOUT;
        std::vector<std::string> vSeedNodes;
        std::vector<CSubNet> vWhitelistedRange;
        std::vector<CService> vBinds, vWhiteBinds;
        bool m_use_addrman_outgoing = true;
        std::vector<std::string> m_specified_outgoing;
        std::vector<std::string> m_added_nodes;
    };

    void Init(const Options& connOptions) {
        nLocalServices = connOptions.nLocalServices;
        nMaxConnections = connOptions.nMaxConnections;
        nMaxOutbound = std::min(connOptions.nMaxOutbound, connOptions.nMaxConnections);
        m_use_addrman_outgoing = connOptions.m_use_addrman_outgoing;
        nMaxAddnode = connOptions.nMaxAddnode;
        nMaxFeeler = connOptions.nMaxFeeler;
        nBestHeight = connOptions.nBestHeight;
        clientInterface = connOptions.uiInterface;
        m_msgproc = connOptions.m_msgproc;
        nSendBufferMaxSize = connOptions.nSendBufferMaxSize;
        nReceiveFloodSize = connOptions.nReceiveFloodSize;
        m_peer_connect_timeout = connOptions.m_peer_connect_timeout;
        {
            LOCK(cs_totalBytesSent);
            nMaxOutboundTimeframe = connOptions.nMaxOutboundTimeframe;
            nMaxOutboundLimit = connOptions.nMaxOutboundLimit;
        }
        vWhitelistedRange = connOptions.vWhitelistedRange;
        {
            LOCK(cs_vAddedNodes);
            vAddedNodes = connOptions.m_added_nodes;
        }
    }

    CConnman(uint64_t seed0, uint64_t seed1);
    ~CConnman();
    bool Start(CScheduler& scheduler, const Options& options);
    void Stop();
    void Interrupt();
    bool GetNetworkActive() const { return fNetworkActive; };
    bool GetUseAddrmanOutgoing() const { return m_use_addrman_outgoing; };
    void SetNetworkActive(bool active);
    void OpenNetworkConnection(const CAddress& addrConnect, bool fCountFailure, CSemaphoreGrant *grantOutbound = nullptr, const char *strDest = nullptr, bool fOneShot = false, bool fFeeler = false, bool manual_connection = false);
    bool CheckIncomingNonce(uint64_t nonce);

    bool ForNode(NodeId id, std::function<bool(CNode* pnode)> func);

    void PushMessage(CNode* pnode, CSerializedNetMsg&& msg);

    template<typename Callable>
    void ForEachNode(Callable&& func)
    {
        LOCK(cs_vNodes);
        for (auto&& node : vNodes) {
            if (NodeFullyConnected(node))
                func(node);
        }
    };

    template<typename Callable>
    void ForEachNode(Callable&& func) const
    {
        LOCK(cs_vNodes);
        for (auto&& node : vNodes) {
            if (NodeFullyConnected(node))
                func(node);
        }
    };

    template<typename Callable, typename CallableAfter>
    void ForEachNodeThen(Callable&& pre, CallableAfter&& post)
    {
        LOCK(cs_vNodes);
        for (auto&& node : vNodes) {
            if (NodeFullyConnected(node))
                pre(node);
        }
        post();
    };

    template<typename Callable, typename CallableAfter>
    void ForEachNodeThen(Callable&& pre, CallableAfter&& post) const
    {
        LOCK(cs_vNodes);
        for (auto&& node : vNodes) {
            if (NodeFullyConnected(node))
                pre(node);
        }
        post();
    };

//addrman函数
    size_t GetAddressCount() const;
    void SetServices(const CService &addr, ServiceFlags nServices);
    void MarkAddressGood(const CAddress& addr);
    void AddNewAddresses(const std::vector<CAddress>& vAddr, const CAddress& addrFrom, int64_t nTimePenalty = 0);
    std::vector<CAddress> GetAddresses();

//拒绝服务检测/预防
//其目的是检测行为正常的同龄人。
//很糟糕，断开/禁止它们，但要在
//一个编码错误不会破坏整个网络
//方式。
//重要提示：我不能给任何东西
//它将在其上转发的节点将使
//节点的对等机将其丢弃。如果有，攻击者
//可以隔离节点和/或尝试拆分网络。
//删除用于发送无效内容的节点
//现在，但可能在更高版本中有效
//危险，因为它会导致网络分裂
//在运行旧代码的节点和运行的节点之间
//新代码。
    void Ban(const CNetAddr& netAddr, const BanReason& reason, int64_t bantimeoffset = 0, bool sinceUnixEpoch = false);
    void Ban(const CSubNet& subNet, const BanReason& reason, int64_t bantimeoffset = 0, bool sinceUnixEpoch = false);
void ClearBanned(); //单元测试所需
    bool IsBanned(CNetAddr ip);
    bool IsBanned(CSubNet subnet);
    bool Unban(const CNetAddr &ip);
    bool Unban(const CSubNet &ip);
    void GetBanned(banmap_t &banmap);
    void SetBanned(const banmap_t &banmap);

//这允许临时超过nmaxoutbound，以查找
//比我们现在所有的同行都好的同行。
    void SetTryNewOutboundPeer(bool flag);
    bool GetTryNewOutboundPeer();

//返回超出目标的出站对等机数量（例如，
//如果我们以前调用过setRynewOutboundPeer（true），然后
//要判断错误，我们可能需要断开其他对等机的连接）。这可能
//返回小于的值（num_outbound_connections-num_outbound_slots）
//在某些出站连接尚未完全连接的情况下，或
//尚未完全断开。
    int GetExtraOutboundCount();

    bool AddNode(const std::string& node);
    bool RemoveAddedNode(const std::string& node);
    std::vector<AddedNodeInfo> GetAddedNodeInfo();

    size_t GetNodeCount(NumConnections num);
    void GetNodeStats(std::vector<CNodeStats>& vstats);
    bool DisconnectNode(const std::string& node);
    bool DisconnectNode(NodeId id);

    ServiceFlags GetLocalServices() const;

//！以字节为单位设置最大出站目标
    void SetMaxOutboundTarget(uint64_t limit);
    uint64_t GetMaxOutboundTarget();

//！设置最大出站目标的时间范围
    void SetMaxOutboundTimeframe(uint64_t timeframe);
    uint64_t GetMaxOutboundTimeframe();

//！检查是否达到出站目标
//如果参数HistoricalBlocksServingLimit设置为true，则函数将
//如果达到历史数据块的服务限制，则响应为真
    bool OutboundTargetReached(bool historicalBlockServingLimit);

//！响应当前最大出站周期中剩余的字节数
//如果没有限制，它将始终响应0
    uint64_t GetOutboundTargetBytesLeft();

//！响应当前最大出站周期中剩余的秒数
//如果没有限制，它将始终响应0
    uint64_t GetMaxOutboundTimeLeftInCycle();

    uint64_t GetTotalBytesRecv();
    uint64_t GetTotalBytesSent();

    void SetBestHeight(int height);
    int GetBestHeight() const;

    /*得到一个唯一的确定性随机化器。*/
    CSipHasher GetDeterministicRandomizer(uint64_t id) const;

    unsigned int GetReceiveFloodSize() const;

    void WakeMessageHandler();

    /*试图通过指数分布的发射来混淆发送时间。
        假设使用单个间隔。
        可变的间隔将导致隐私减少。
    **/

    int64_t PoissonNextSendInbound(int64_t now, int average_interval_seconds);

private:
    struct ListenSocket {
        SOCKET socket;
        bool whitelisted;

        ListenSocket(SOCKET socket_, bool whitelisted_) : socket(socket_), whitelisted(whitelisted_) {}
    };

    bool BindListenPort(const CService &bindAddr, std::string& strError, bool fWhitelisted = false);
    bool Bind(const CService &addr, unsigned int flags);
    bool InitBinds(const std::vector<CService>& binds, const std::vector<CService>& whiteBinds);
    void ThreadOpenAddedConnections();
    void AddOneShot(const std::string& strDest);
    void ProcessOneShot();
    void ThreadOpenConnections(std::vector<std::string> connect);
    void ThreadMessageHandler();
    void AcceptConnection(const ListenSocket& hListenSocket);
    void DisconnectNodes();
    void NotifyNumConnectionsChanged();
    void InactivityCheck(CNode *pnode);
    bool GenerateSelectSet(std::set<SOCKET> &recv_set, std::set<SOCKET> &send_set, std::set<SOCKET> &error_set);
    void SocketEvents(std::set<SOCKET> &recv_set, std::set<SOCKET> &send_set, std::set<SOCKET> &error_set);
    void SocketHandler();
    void ThreadSocketHandler();
    void ThreadDNSAddressSeed();

    uint64_t CalculateKeyedNetGroup(const CAddress& ad) const;

    CNode* FindNode(const CNetAddr& ip);
    CNode* FindNode(const CSubNet& subNet);
    CNode* FindNode(const std::string& addrName);
    CNode* FindNode(const CService& addr);

    bool AttemptToEvictConnection();
    CNode* ConnectNode(CAddress addrConnect, const char *pszDest, bool fCountFailure, bool manual_connection);
    bool IsWhitelistedRange(const CNetAddr &addr);

    void DeleteNode(CNode* pnode);

    NodeId GetNewNodeId();

    size_t SocketSendData(CNode *pnode) const;
//！检查禁止列表是否有未写入的更改
    bool BannedSetIsDirty();
//！为BanList设置“脏”标志
    void SetBannedSetDirty(bool dirty=true);
//！清除未使用的条目（如果BANTIME已过期）
    void SweepBanned();
    void DumpAddresses();
    void DumpData();
    void DumpBanlist();

//网络统计
    void RecordBytesRecv(uint64_t bytes);
    void RecordBytesSent(uint64_t bytes);

//是否应在foreach*回调中传递节点
    static bool NodeFullyConnected(const CNode* pnode);

//网络使用总数
    CCriticalSection cs_totalBytesRecv;
    CCriticalSection cs_totalBytesSent;
    uint64_t nTotalBytesRecv GUARDED_BY(cs_totalBytesRecv);
    uint64_t nTotalBytesSent GUARDED_BY(cs_totalBytesSent);

//出站限制和统计
    uint64_t nMaxOutboundTotalBytesSentInCycle GUARDED_BY(cs_totalBytesSent);
    uint64_t nMaxOutboundCycleStartTime GUARDED_BY(cs_totalBytesSent);
    uint64_t nMaxOutboundLimit GUARDED_BY(cs_totalBytesSent);
    uint64_t nMaxOutboundTimeframe GUARDED_BY(cs_totalBytesSent);

//p2p超时（秒）
    int64_t m_peer_connect_timeout;

//白名单范围。从这些节点连接的任何节点都将自动
//白名单（以及那些连接到白名单绑定的绑定）。
    std::vector<CSubNet> vWhitelistedRange;

    unsigned int nSendBufferMaxSize{0};
    unsigned int nReceiveFloodSize{0};

    std::vector<ListenSocket> vhListenSocket;
    std::atomic<bool> fNetworkActive{true};
    banmap_t setBanned GUARDED_BY(cs_setBanned);
    CCriticalSection cs_setBanned;
    bool setBannedIsDirty GUARDED_BY(cs_setBanned){false};
    bool fAddressesInitialized{false};
    CAddrMan addrman;
    std::deque<std::string> vOneShots GUARDED_BY(cs_vOneShots);
    CCriticalSection cs_vOneShots;
    std::vector<std::string> vAddedNodes GUARDED_BY(cs_vAddedNodes);
    CCriticalSection cs_vAddedNodes;
    std::vector<CNode*> vNodes;
    std::list<CNode*> vNodesDisconnected;
    mutable CCriticalSection cs_vNodes;
    std::atomic<NodeId> nLastNodeId{0};
    unsigned int nPrevNodeCount{0};

    /*此实例提供的服务*/
    ServiceFlags nLocalServices;

    std::unique_ptr<CSemaphore> semOutbound;
    std::unique_ptr<CSemaphore> semAddnode;
    int nMaxConnections;
    int nMaxOutbound;
    int nMaxAddnode;
    int nMaxFeeler;
    bool m_use_addrman_outgoing;
    std::atomic<int> nBestHeight;
    CClientUIInterface* clientInterface;
    NetEventsInterface* m_msgproc;

    /*确定性随机性的siphasher种子*/
    const uint64_t nSeed0, nSeed1;

    /*用于唤醒消息处理器的标志。*/
    bool fMsgProcWake;

    std::condition_variable condMsgProc;
    Mutex mutexMsgProc;
    std::atomic<bool> flagInterruptMsgProc{false};

    CThreadInterrupt interruptNet;

    std::thread threadDNSAddressSeed;
    std::thread threadSocketHandler;
    std::thread threadOpenAddedConnections;
    std::thread threadOpenConnections;
    std::thread threadMessageHandler;

    /*决定连接到额外出站对等机的标志，
     *超出正常约束
     *这取代了触角连接。*/

    std::atomic_bool m_try_another_outbound_peer;

    std::atomic<int64_t> m_next_send_inv_to_incoming{0};

    friend struct CConnmanTest;
};
extern std::unique_ptr<CConnman> g_connman;
void Discover();
void StartMapPort();
void InterruptMapPort();
void StopMapPort();
unsigned short GetListenPort();
bool BindListenPort(const CService &bindAddr, std::string& strError, bool fWhitelisted = false);

struct CombinerAll
{
    typedef bool result_type;

    template<typename I>
    bool operator()(I first, I last) const
    {
        while (first != last) {
            if (!(*first)) return false;
            ++first;
        }
        return true;
    }
};

/*
 *消息处理接口
 **/

class NetEventsInterface
{
public:
    virtual bool ProcessMessages(CNode* pnode, std::atomic<bool>& interrupt) = 0;
    virtual bool SendMessages(CNode* pnode) = 0;
    virtual void InitializeNode(CNode* pnode) = 0;
    virtual void FinalizeNode(NodeId id, bool& update_connection_time) = 0;

protected:
    /*
     *受保护的析构函数，因此实例只能由派生类删除。
     *如果不再需要该限制，则应将其公开并虚拟化。
     **/

    ~NetEventsInterface() = default;
};

enum
{
LOCAL_NONE,   //未知的
LOCAL_IF,     //本地接口监听的地址
LOCAL_BIND,   //地址显式绑定到
LOCAL_UPNP,   //UPNP报告的地址
LOCAL_MANUAL, //显式指定的地址（-ExternalIP=）

    LOCAL_MAX
};

bool IsPeerAddrLocalGood(CNode *pnode);
void AdvertiseLocal(CNode *pnode);

/*
 *将网络标记为可访问或不可访问（无自动连接）
 *默认情况下，可以访问@note网络
 **/

void SetReachable(enum Network net, bool reachable);
/*@如果网络可以访问，则返回“真”，否则返回“假”*/
bool IsReachable(enum Network net);
/*@如果地址在可访问网络中，则返回“真”，否则返回“假”*/
bool IsReachable(const CNetAddr& addr);

bool AddLocal(const CService& addr, int nScore = LOCAL_NONE);
bool AddLocal(const CNetAddr& addr, int nScore = LOCAL_NONE);
void RemoveLocal(const CService& addr);
bool SeenLocal(const CService& addr);
bool IsLocal(const CService& addr);
bool GetLocal(CService &addr, const CNetAddr *paddrPeer = nullptr);
CAddress GetLocalAddress(const CNetAddr *paddrPeer, ServiceFlags nLocalServices);


extern bool fDiscover;
extern bool fListen;
extern bool fRelayTxes;

extern limitedmap<uint256, int64_t> mapAlreadyAskedFor;

/*在“version”消息中发送到P2P网络的Subversion*/
extern std::string strSubVersion;

struct LocalServiceInfo {
    int nScore;
    int nPort;
};

extern CCriticalSection cs_mapLocalHost;
extern std::map<CNetAddr, LocalServiceInfo> mapLocalHost GUARDED_BY(cs_mapLocalHost);

extern const std::string NET_MESSAGE_COMMAND_OTHER;
typedef std::map<std::string, uint64_t> mapMsgCmdSize; //命令，总字节数

class CNodeStats
{
public:
    NodeId nodeid;
    ServiceFlags nServices;
    bool fRelayTxes;
    int64_t nLastSend;
    int64_t nLastRecv;
    int64_t nTimeConnected;
    int64_t nTimeOffset;
    std::string addrName;
    int nVersion;
    std::string cleanSubVer;
    bool fInbound;
    bool m_manual_connection;
    int nStartingHeight;
    uint64_t nSendBytes;
    mapMsgCmdSize mapSendBytesPerMsgCmd;
    uint64_t nRecvBytes;
    mapMsgCmdSize mapRecvBytesPerMsgCmd;
    bool fWhitelisted;
    double dPingTime;
    double dPingWait;
    double dMinPing;
    CAmount minFeeFilter;
//我们的地址，由同行报告
    std::string addrLocal;
//此对等方的地址
    CAddress addr;
//连接方的绑定地址
    CAddress addrBind;
};




class CNetMessage {
private:
    mutable CHash256 hasher;
    mutable uint256 data_hash;
public:
bool in_data;                   //分析头（假）或数据（真）

CDataStream hdrbuf;             //部分接收的标题
CMessageHeader hdr;             //完整报头
    unsigned int nHdrPos;

CDataStream vRecv;              //接收到的消息数据
    unsigned int nDataPos;

int64_t nTime;                  //接收消息的时间（以微秒计）。

    CNetMessage(const CMessageHeader::MessageStartChars& pchMessageStartIn, int nTypeIn, int nVersionIn) : hdrbuf(nTypeIn, nVersionIn), hdr(pchMessageStartIn), vRecv(nTypeIn, nVersionIn) {
        hdrbuf.resize(24);
        in_data = false;
        nHdrPos = 0;
        nDataPos = 0;
        nTime = 0;
    }

    bool complete() const
    {
        if (!in_data)
            return false;
        return (hdr.nMessageSize == nDataPos);
    }

    const uint256& GetMessageHash() const;

    void SetVersion(int nVersionIn)
    {
        hdrbuf.SetVersion(nVersionIn);
        vRecv.SetVersion(nVersionIn);
    }

    int readHeader(const char *pch, unsigned int nBytes);
    int readData(const char *pch, unsigned int nBytes);
};


/*同伴信息*/
class CNode
{
    friend class CConnman;
public:
//插座
    std::atomic<ServiceFlags> nServices{NODE_NONE};
    SOCKET hSocket GUARDED_BY(cs_hSocket);
size_t nSendSize{0}; //所有vsendmsg项的总大小
size_t nSendOffset{0}; //已发送的第一个vsendmsg内的偏移量
    uint64_t nSendBytes GUARDED_BY(cs_vSend){0};
    std::deque<std::vector<unsigned char>> vSendMsg GUARDED_BY(cs_vSend);
    CCriticalSection cs_vSend;
    CCriticalSection cs_hSocket;
    CCriticalSection cs_vRecv;

    CCriticalSection cs_vProcessMsg;
    std::list<CNetMessage> vProcessMsg GUARDED_BY(cs_vProcessMsg);
    size_t nProcessQueueSize{0};

    CCriticalSection cs_sendProcessing;

    std::deque<CInv> vRecvGetData;
    uint64_t nRecvBytes GUARDED_BY(cs_vRecv){0};
    std::atomic<int> nRecvVersion{INIT_PROTO_VERSION};

    std::atomic<int64_t> nLastSend{0};
    std::atomic<int64_t> nLastRecv{0};
    const int64_t nTimeConnected;
    std::atomic<int64_t> nTimeOffset{0};
//此对等方的地址
    const CAddress addr;
//连接方的绑定地址
    const CAddress addrBind;
    std::atomic<int> nVersion{0};
//strSubfer是我们从线路中读取的任何字节数组。但是，此字段是用于
//打印出来，以各种形式展示给人类等等。所以我们对它进行消毒
//将已消毒的版本存储在CleanSubfer中。处理时应使用原件
//显示或记录时使用的网络或导线类型和清理后的字符串。
    std::string strSubVer GUARDED_BY(cs_SubVer), cleanSubVer GUARDED_BY(cs_SubVer);
CCriticalSection cs_SubVer; //用于CleanSubfer和StrSubfer
bool fWhitelisted{false}; //此对等机可以绕过DoS禁止。
bool fFeeler{false}; //如果为真，则此节点将用作短寿命的触角。
    bool fOneShot{false};
    bool m_manual_connection{false};
bool fClient{false}; //按版本消息设置
bool m_limited_node{false}; //在bip159之后，由版本消息设置
    const bool fInbound;
    std::atomic_bool fSuccessfullyConnected{false};
//将fdisconnect设置为true将导致节点断开
//下次运行disconnectnodes（）时
    std::atomic_bool fDisconnect{false};
//我们使用frelaytxes有两个目的-
//a）它允许我们在收到对等方的版本消息之前不中继TX INV。
//b）对等方可能在其版本消息中告诉我们，我们不应中继TX INV。
//除非装上布卢姆过滤器。
    bool fRelayTxes GUARDED_BY(cs_filter){false};
    bool fSentAddr{false};
    CSemaphoreGrant grantOutbound;
    mutable CCriticalSection cs_filter;
    std::unique_ptr<CBloomFilter> pfilter PT_GUARDED_BY(cs_filter);
    std::atomic<int> nRefCount{0};

    const uint64_t nKeyedNetGroup;
    std::atomic_bool fPauseRecv{false};
    std::atomic_bool fPauseSend{false};

protected:
    mapMsgCmdSize mapSendBytesPerMsgCmd;
    mapMsgCmdSize mapRecvBytesPerMsgCmd GUARDED_BY(cs_vRecv);

public:
    uint256 hashContinue;
    std::atomic<int> nStartingHeight{-1};

//洪水中继
    std::vector<CAddress> vAddrToSend;
    CRollingBloomFilter addrKnown;
    bool fGetAddr{false};
    std::set<uint256> setKnown;
    int64_t nNextAddrSend GUARDED_BY(cs_sendProcessing){0};
    int64_t nNextLocalAddrSend GUARDED_BY(cs_sendProcessing){0};

//基于库存的继电器
    CRollingBloomFilter filterInventoryKnown GUARDED_BY(cs_inventory);
//我们还必须公布一组事务ID。
//它们是由中继前的内存池排序的，所以顺序并不重要。
    std::set<uint256> setInventoryTxToSend;
//我们仍然公布的块ID列表。
//发送前没有最终排序，因为它们总是立即发送
//按照要求的顺序。
    std::vector<uint256> vInventoryBlockToSend GUARDED_BY(cs_inventory);
    CCriticalSection cs_inventory;
    std::set<uint256> setAskFor;
    std::multimap<int64_t, CInv> mapAskFor;
    int64_t nNextInvSend{0};
//用于标题通知-要中继的未筛选块
    std::vector<uint256> vBlockHashesToAnnounce GUARDED_BY(cs_inventory);
//用于BIP35内存池发送
    bool fSendMempool GUARDED_BY(cs_inventory){false};

//上次服务“mempool”请求时。
    std::atomic<int64_t> timeLastMempoolReq{0};

//阻止和txn接受时间
    std::atomic<int64_t> nLastBlockTime{0};
    std::atomic<int64_t> nLastTXTime{0};

//Ping时间测量：
//我们期望的pong回复，如果没有pong，则为0。
    std::atomic<uint64_t> nPingNonceSent{0};
//上次发送ping的时间（在USEC中），如果没有发送ping，则为0。
    std::atomic<int64_t> nPingUsecStart{0};
//上次测量的往返时间。
    std::atomic<int64_t> nPingUsecTime{0};
//最佳测量的往返时间。
    std::atomic<int64_t> nMinPingUsecTime{std::numeric_limits<int64_t>::max()};
//是否请求ping。
    std::atomic<bool> fPingQueued{false};
//筛选此节点的投资的最低费率
    CAmount minFeeFilter GUARDED_BY(cs_feeFilter){0};
    CCriticalSection cs_feeFilter;
    CAmount lastSentFeeFilter{0};
    int64_t nextSendTimeFeeFilter{0};

    CNode(NodeId id, ServiceFlags nLocalServicesIn, int nMyStartingHeightIn, SOCKET hSocketIn, const CAddress &addrIn, uint64_t nKeyedNetGroupIn, uint64_t nLocalHostNonceIn, const CAddress &addrBindIn, const std::string &addrNameIn = "", bool fInboundIn = false);
    ~CNode();
    CNode(const CNode&) = delete;
    CNode& operator=(const CNode&) = delete;

private:
    const NodeId id;
    const uint64_t nLocalHostNonce;
//向该对等方提供的服务
    const ServiceFlags nLocalServices;
    const int nMyStartingHeight;
    int nSendVersion{0};
std::list<CNetMessage> vRecvMsg;  //仅由sockethandler线程使用

    mutable CCriticalSection cs_addrName;
    std::string addrName GUARDED_BY(cs_addrName);

//我们的地址，由同行报告
    CService addrLocal GUARDED_BY(cs_addrLocal);
    mutable CCriticalSection cs_addrLocal;
public:

    NodeId GetId() const {
        return id;
    }

    uint64_t GetLocalNonce() const {
        return nLocalHostNonce;
    }

    int GetMyStartingHeight() const {
        return nMyStartingHeight;
    }

    int GetRefCount() const
    {
        assert(nRefCount >= 0);
        return nRefCount;
    }

    bool ReceiveMsgBytes(const char *pch, unsigned int nBytes, bool& complete);

    void SetRecvVersion(int nVersionIn)
    {
        nRecvVersion = nVersionIn;
    }
    int GetRecvVersion() const
    {
        return nRecvVersion;
    }
    void SetSendVersion(int nVersionIn);
    int GetSendVersion() const;

    CService GetAddrLocal() const;
//！不能多次调用
    void SetAddrLocal(const CService& addrLocalIn);

    CNode* AddRef()
    {
        nRefCount++;
        return this;
    }

    void Release()
    {
        nRefCount--;
    }



    void AddAddressKnown(const CAddress& _addr)
    {
        addrKnown.insert(_addr.GetKey());
    }

    void PushAddress(const CAddress& _addr, FastRandomContext &insecure_rand)
    {
//此处的已知检查仅用于节省重复项的空间。
//sendmessages将再次筛选添加的knowns。
//地址被推后。
        if (_addr.IsValid() && !addrKnown.contains(_addr.GetKey())) {
            if (vAddrToSend.size() >= MAX_ADDR_TO_SEND) {
                vAddrToSend[insecure_rand.randrange(vAddrToSend.size())] = _addr;
            } else {
                vAddrToSend.push_back(_addr);
            }
        }
    }


    void AddInventoryKnown(const CInv& inv)
    {
        {
            LOCK(cs_inventory);
            filterInventoryKnown.insert(inv.hash);
        }
    }

    void PushInventory(const CInv& inv)
    {
        LOCK(cs_inventory);
        if (inv.type == MSG_TX) {
            if (!filterInventoryKnown.contains(inv.hash)) {
                setInventoryTxToSend.insert(inv.hash);
            }
        } else if (inv.type == MSG_BLOCK) {
            vInventoryBlockToSend.push_back(inv.hash);
        }
    }

    void PushBlockHash(const uint256 &hash)
    {
        LOCK(cs_inventory);
        vBlockHashesToAnnounce.push_back(hash);
    }

    void AskFor(const CInv& inv);

    void CloseSocketDisconnect();

    void copyStats(CNodeStats &stats);

    ServiceFlags GetLocalServices() const
    {
        return nLocalServices;
    }

    std::string GetAddrName() const;
//！仅当以前未设置addrname时才设置addrname
    void MaybeSetAddrName(const std::string& addrNameIn);
};





/*返回指数分布事件的未来时间戳（以微秒为单位）。*/
int64_t PoissonNextSend(int64_t now, int average_interval_seconds);

#endif //比特科尼
