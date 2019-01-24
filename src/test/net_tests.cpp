
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2012-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。
#include <addrman.h>
#include <test/test_bitcoin.h>
#include <string>
#include <boost/test/unit_test.hpp>
#include <hash.h>
#include <serialize.h>
#include <streams.h>
#include <net.h>
#include <netbase.h>
#include <chainparams.h>
#include <util/system.h>

#include <memory>

class CAddrManSerializationMock : public CAddrMan
{
public:
    virtual void Serialize(CDataStream& s) const = 0;

//！为了测试目的，确保桶的放置总是相同的。
    void MakeDeterministic()
    {
        nKey.SetNull();
        insecure_rand = FastRandomContext(true);
    }
};

class CAddrManUncorrupted : public CAddrManSerializationMock
{
public:
    void Serialize(CDataStream& s) const override
    {
        CAddrMan::Serialize(s);
    }
};

class CAddrManCorrupted : public CAddrManSerializationMock
{
public:
    void Serialize(CDataStream& s) const override
    {
//生成损坏的输出，声明addrman只有一个addr时有20个addr。
        unsigned char nVersion = 1;
        s << nVersion;
        s << ((unsigned char)32);
        s << nKey;
s << 10; //NNEW
s << 10; //未试验过的

        int nUBuckets = ADDRMAN_NEW_BUCKET_COUNT ^ (1 << 30);
        s << nUBuckets;

        CService serv;
        BOOST_CHECK(Lookup("252.1.1.1", serv, 7777, false));
        CAddress addr = CAddress(serv, NODE_NONE);
        CNetAddr resolved;
        BOOST_CHECK(LookupHost("252.2.2.2", resolved, false));
        CAddrInfo info = CAddrInfo(addr, resolved);
        s << info;
    }
};

static CDataStream AddrmanToStream(CAddrManSerializationMock& _addrman)
{
    CDataStream ssPeersIn(SER_DISK, CLIENT_VERSION);
    ssPeersIn << Params().MessageStart();
    ssPeersIn << _addrman;
    std::string str = ssPeersIn.str();
    std::vector<unsigned char> vchData(str.begin(), str.end());
    return CDataStream(vchData, SER_DISK, CLIENT_VERSION);
}

BOOST_FIXTURE_TEST_SUITE(net_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(cnode_listen_port)
{
//测试默认值
    unsigned short port = GetListenPort();
    BOOST_CHECK(port == Params().GetDefaultPort());
//测试集端口
    unsigned short altPort = 12345;
    BOOST_CHECK(gArgs.SoftSetArg("-port", std::to_string(altPort)));
    port = GetListenPort();
    BOOST_CHECK(port == altPort);
}

BOOST_AUTO_TEST_CASE(caddrdb_read)
{
    SetDataDir("caddrdb_read");
    CAddrManUncorrupted addrmanUncorrupted;
    addrmanUncorrupted.MakeDeterministic();

    CService addr1, addr2, addr3;
    BOOST_CHECK(Lookup("250.7.1.1", addr1, 8333, false));
    BOOST_CHECK(Lookup("250.7.2.2", addr2, 9999, false));
    BOOST_CHECK(Lookup("250.7.3.3", addr3, 9999, false));

//向新表添加三个地址。
    CService source;
    BOOST_CHECK(Lookup("252.5.1.1", source, 8333, false));
    BOOST_CHECK(addrmanUncorrupted.Add(CAddress(addr1, NODE_NONE), source));
    BOOST_CHECK(addrmanUncorrupted.Add(CAddress(addr2, NODE_NONE), source));
    BOOST_CHECK(addrmanUncorrupted.Add(CAddress(addr3, NODE_NONE), source));

//测试反序列化是否不会引发异常。
    CDataStream ssPeers1 = AddrmanToStream(addrmanUncorrupted);
    bool exceptionThrown = false;
    CAddrMan addrman1;

    BOOST_CHECK(addrman1.size() == 0);
    try {
        unsigned char pchMsgTmp[4];
        ssPeers1 >> pchMsgTmp;
        ssPeers1 >> addrman1;
    } catch (const std::exception&) {
        exceptionThrown = true;
    }

    BOOST_CHECK(addrman1.size() == 3);
    BOOST_CHECK(exceptionThrown == false);

//测试caddrdb:：read是否使用正确数量的addr创建addrman。
    CDataStream ssPeers2 = AddrmanToStream(addrmanUncorrupted);

    CAddrMan addrman2;
    CAddrDB adb;
    BOOST_CHECK(addrman2.size() == 0);
    BOOST_CHECK(adb.Read(addrman2, ssPeers2));
    BOOST_CHECK(addrman2.size() == 3);
}


BOOST_AUTO_TEST_CASE(caddrdb_read_corrupted)
{
    SetDataDir("caddrdb_read_corrupted");
    CAddrManCorrupted addrmanCorrupted;
    addrmanCorrupted.MakeDeterministic();

//测试损坏的addrman的反序列化是否引发异常。
    CDataStream ssPeers1 = AddrmanToStream(addrmanCorrupted);
    bool exceptionThrown = false;
    CAddrMan addrman1;
    BOOST_CHECK(addrman1.size() == 0);
    try {
        unsigned char pchMsgTmp[4];
        ssPeers1 >> pchMsgTmp;
        ssPeers1 >> addrman1;
    } catch (const std::exception&) {
        exceptionThrown = true;
    }
//即使通过反序列化失败，addrman也不会保持干净状态。
    BOOST_CHECK(addrman1.size() == 1);
    BOOST_CHECK(exceptionThrown);

//如果反序列化失败，测试caddrdb:：read是否使addrman保持干净状态。
    CDataStream ssPeers2 = AddrmanToStream(addrmanCorrupted);

    CAddrMan addrman2;
    CAddrDB adb;
    BOOST_CHECK(addrman2.size() == 0);
    BOOST_CHECK(!adb.Read(addrman2, ssPeers2));
    BOOST_CHECK(addrman2.size() == 0);
}

BOOST_AUTO_TEST_CASE(cnode_simple_test)
{
    SOCKET hSocket = INVALID_SOCKET;
    NodeId id = 0;
    int height = 0;

    in_addr ipv4Addr;
    ipv4Addr.s_addr = 0xa0b0c001;

    CAddress addr = CAddress(CService(ipv4Addr, 7777), NODE_NETWORK);
    std::string pszDest;
    bool fInboundIn = false;

//测试默认情况下ffeeler是否为false。
    std::unique_ptr<CNode> pnode1 = MakeUnique<CNode>(id++, NODE_NETWORK, height, hSocket, addr, 0, 0, CAddress(), pszDest, fInboundIn);
    BOOST_CHECK(pnode1->fInbound == false);
    BOOST_CHECK(pnode1->fFeeler == false);

    fInboundIn = true;
    std::unique_ptr<CNode> pnode2 = MakeUnique<CNode>(id++, NODE_NETWORK, height, hSocket, addr, 1, 1, CAddress(), pszDest, fInboundIn);
    BOOST_CHECK(pnode2->fInbound == true);
    BOOST_CHECK(pnode2->fFeeler == false);
}

//在pr_14728之前，此测试将触发未定义的行为
BOOST_AUTO_TEST_CASE(ipv4_peer_with_ipv6_addrMe_test)
{
//设置本地地址；复制错误所需的全部内容是
//在条目中有一个普通的IPv4地址，但是如果该地址是
//！isroutable未定义的行为更容易确定地触发
    {
        LOCK(cs_mapLocalHost);
        in_addr ipv4AddrLocal;
        ipv4AddrLocal.s_addr = 0x0100007f;
        CNetAddr addr = CNetAddr(ipv4AddrLocal);
        LocalServiceInfo lsi;
        lsi.nScore = 23;
        lsi.nPort = 42;
        mapLocalHost[addr] = lsi;
    }

//使用IPv4地址创建对等机
    in_addr ipv4AddrPeer;
    ipv4AddrPeer.s_addr = 0xa0b0c001;
    CAddress addr = CAddress(CService(ipv4AddrPeer, 7777), NODE_NETWORK);
    std::unique_ptr<CNode> pnode = MakeUnique<CNode>(0, NODE_NETWORK, 0, INVALID_SOCKET, addr, 0, 0, CAddress{}, std::string{}, false);
    pnode->fSuccessfullyConnected.store(true);

//同行声称通过ipv6与我们取得联系
    in6_addr ipv6AddrLocal;
    memset(ipv6AddrLocal.s6_addr, 0, 16);
    ipv6AddrLocal.s6_addr[0] = 0xcc;
    CAddress addrLocal = CAddress(CService(ipv6AddrLocal, 7777), NODE_NETWORK);
    pnode->SetAddrLocal(addrLocal);

//在修补之前，这会导致无法定义的行为，可以用clang的-fsanitize=memory检测到。
    AdvertiseLocal(&*pnode);

//禁止任何检查运行警告；如果此测试失败，则通过触发消毒剂
    BOOST_CHECK(1);
}


BOOST_AUTO_TEST_CASE(LimitedAndReachable_Network)
{
    BOOST_CHECK_EQUAL(IsReachable(NET_IPV4), true);
    BOOST_CHECK_EQUAL(IsReachable(NET_IPV6), true);
    BOOST_CHECK_EQUAL(IsReachable(NET_ONION), true);

    SetReachable(NET_IPV4, false);
    SetReachable(NET_IPV6, false);
    SetReachable(NET_ONION, false);

    BOOST_CHECK_EQUAL(IsReachable(NET_IPV4), false);
    BOOST_CHECK_EQUAL(IsReachable(NET_IPV6), false);
    BOOST_CHECK_EQUAL(IsReachable(NET_ONION), false);

    SetReachable(NET_IPV4, true);
    SetReachable(NET_IPV6, true);
    SetReachable(NET_ONION, true);

    BOOST_CHECK_EQUAL(IsReachable(NET_IPV4), true);
    BOOST_CHECK_EQUAL(IsReachable(NET_IPV6), true);
    BOOST_CHECK_EQUAL(IsReachable(NET_ONION), true);
}

BOOST_AUTO_TEST_CASE(LimitedAndReachable_NetworkCaseUnroutableAndInternal)
{
    BOOST_CHECK_EQUAL(IsReachable(NET_UNROUTABLE), true);
    BOOST_CHECK_EQUAL(IsReachable(NET_INTERNAL), true);

    SetReachable(NET_UNROUTABLE, false);
    SetReachable(NET_INTERNAL, false);

BOOST_CHECK_EQUAL(IsReachable(NET_UNROUTABLE), true); //两个网络都忽略
    BOOST_CHECK_EQUAL(IsReachable(NET_INTERNAL), true);
}

CNetAddr UtilBuildAddress(unsigned char p1, unsigned char p2, unsigned char p3, unsigned char p4)
{
    unsigned char ip[] = {p1, p2, p3, p4};

    struct sockaddr_in sa;
memset(&sa, 0, sizeof(sockaddr_in)); //初始化内存块
    memcpy(&(sa.sin_addr), &ip, sizeof(ip));
    return CNetAddr(sa.sin_addr);
}


BOOST_AUTO_TEST_CASE(LimitedAndReachable_CNetAddr)
{
CNetAddr addr = UtilBuildAddress(0x001, 0x001, 0x001, 0x001); //1.1.1.1

    SetReachable(NET_IPV4, true);
    BOOST_CHECK_EQUAL(IsReachable(addr), true);

    SetReachable(NET_IPV4, false);
    BOOST_CHECK_EQUAL(IsReachable(addr), false);

SetReachable(NET_IPV4, true); //必须重置这个，因为它是有状态的。
}


BOOST_AUTO_TEST_CASE(LocalAddress_BasicLifecycle)
{
CService addr = CService(UtilBuildAddress(0x002, 0x001, 0x001, 0x001), 1000); //2.1.1.1:1000

    SetReachable(NET_IPV4, true);

    BOOST_CHECK_EQUAL(IsLocal(addr), false);
    BOOST_CHECK_EQUAL(AddLocal(addr, 1000), true);
    BOOST_CHECK_EQUAL(IsLocal(addr), true);

    RemoveLocal(addr);
    BOOST_CHECK_EQUAL(IsLocal(addr), false);
}


BOOST_AUTO_TEST_SUITE_END()
