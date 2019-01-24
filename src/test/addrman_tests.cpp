
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
#include <netbase.h>
#include <random.h>

class CAddrManTest : public CAddrMan
{
public:
    explicit CAddrManTest(bool makeDeterministic = true)
    {
        if (makeDeterministic) {
//将addrman addr placement设置为确定性。
            MakeDeterministic();
        }
    }

//！为了测试目的，确保桶的放置总是相同的。
    void MakeDeterministic()
    {
        nKey.SetNull();
        insecure_rand = FastRandomContext(true);
    }

    CAddrInfo* Find(const CNetAddr& addr, int* pnId = nullptr)
    {
        LOCK(cs);
        return CAddrMan::Find(addr, pnId);
    }

    CAddrInfo* Create(const CAddress& addr, const CNetAddr& addrSource, int* pnId = nullptr)
    {
        LOCK(cs);
        return CAddrMan::Create(addr, addrSource, pnId);
    }

    void Delete(int nId)
    {
        LOCK(cs);
        CAddrMan::Delete(nId);
    }

//模拟连接失败，以便测试是否收回脱机节点
    void SimConnFail(CService& addr)
    {
         LOCK(cs);
         int64_t nLastSuccess = 1;
Good_(addr, true, nLastSuccess); //在过去的深处建立起最后一个良好的联系。

         bool count_failure = false;
         int64_t nLastTry = GetAdjustedTime()-61;
         Attempt(addr, count_failure, nLastTry);
     }
};

static CNetAddr ResolveIP(const char* ip)
{
    CNetAddr addr;
    BOOST_CHECK_MESSAGE(LookupHost(ip, addr, false), strprintf("failed to resolve: %s", ip));
    return addr;
}

static CNetAddr ResolveIP(std::string ip)
{
    return ResolveIP(ip.c_str());
}

static CService ResolveService(const char* ip, int port = 0)
{
    CService serv;
    BOOST_CHECK_MESSAGE(Lookup(ip, serv, port, false), strprintf("failed to resolve: %s:%i", ip, port));
    return serv;
}

static CService ResolveService(std::string ip, int port = 0)
{
    return ResolveService(ip.c_str(), port);
}

BOOST_FIXTURE_TEST_SUITE(addrman_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(addrman_simple)
{
    CAddrManTest addrman;

    CNetAddr source = ResolveIP("252.2.2.2");

//测试：当addrman为空时是否正确响应。
    BOOST_CHECK_EQUAL(addrman.size(), 0U);
    CAddrInfo addr_null = addrman.Select();
    BOOST_CHECK_EQUAL(addr_null.ToString(), "[::]:0");

//测试：addrman：：是否按预期添加工作。
    CService addr1 = ResolveService("250.1.1.1", 8333);
    BOOST_CHECK(addrman.Add(CAddress(addr1, NODE_NONE), source));
    BOOST_CHECK_EQUAL(addrman.size(), 1U);
    CAddrInfo addr_ret1 = addrman.Select();
    BOOST_CHECK_EQUAL(addr_ret1.ToString(), "250.1.1.1:8333");

//测试：IP地址重复数据消除是否正常工作。
//不应添加预期的DUP IP。
    CService addr1_dup = ResolveService("250.1.1.1", 8333);
    BOOST_CHECK(!addrman.Add(CAddress(addr1_dup, NODE_NONE), source));
    BOOST_CHECK_EQUAL(addrman.size(), 1U);


//测试：新表有一个地址，我们应该添加一个diff addr
//至少有一个地址。
//请注意，addrman的大小在插入后无法可靠地进行测试，因为
//可能会发生哈希冲突。但我们可以肯定至少有一个
//成功。

    CService addr2 = ResolveService("250.1.1.2", 8333);
    BOOST_CHECK(addrman.Add(CAddress(addr2, NODE_NONE), source));
    BOOST_CHECK(addrman.size() >= 1);

//测试：addrman:：clear（）应清空新表。
    addrman.Clear();
    BOOST_CHECK_EQUAL(addrman.size(), 0U);
    CAddrInfo addr_null2 = addrman.Select();
    BOOST_CHECK_EQUAL(addr_null2.ToString(), "[::]:0");

//测试：addrman:：add multiple addresses按预期工作
    std::vector<CAddress> vAddr;
    vAddr.push_back(CAddress(ResolveService("250.1.1.3", 8333), NODE_NONE));
    vAddr.push_back(CAddress(ResolveService("250.1.1.4", 8333), NODE_NONE));
    BOOST_CHECK(addrman.Add(vAddr, source));
    BOOST_CHECK(addrman.size() >= 1);
}

BOOST_AUTO_TEST_CASE(addrman_ports)
{
    CAddrManTest addrman;

    CNetAddr source = ResolveIP("252.2.2.2");

    BOOST_CHECK_EQUAL(addrman.size(), 0U);

//测试7；具有相同IP但diff端口的addr不替换现有addr。
    CService addr1 = ResolveService("250.1.1.1", 8333);
    BOOST_CHECK(addrman.Add(CAddress(addr1, NODE_NONE), source));
    BOOST_CHECK_EQUAL(addrman.size(), 1U);

    CService addr1_port = ResolveService("250.1.1.1", 8334);
    BOOST_CHECK(!addrman.Add(CAddress(addr1_port, NODE_NONE), source));
    BOOST_CHECK_EQUAL(addrman.size(), 1U);
    CAddrInfo addr_ret2 = addrman.Select();
    BOOST_CHECK_EQUAL(addr_ret2.ToString(), "250.1.1.1:8333");

//测试：将相同的IP但不同的端口添加到已尝试的表中，不会添加它。
//也许这不是理想的行为，但它是当前的行为。
    addrman.Good(CAddress(addr1_port, NODE_NONE));
    BOOST_CHECK_EQUAL(addrman.size(), 1U);
    bool newOnly = true;
    CAddrInfo addr_ret3 = addrman.Select(newOnly);
    BOOST_CHECK_EQUAL(addr_ret3.ToString(), "250.1.1.1:8333");
}


BOOST_AUTO_TEST_CASE(addrman_select)
{
    CAddrManTest addrman;

    CNetAddr source = ResolveIP("252.2.2.2");

//测试：从new中选择1 addr in new。
    CService addr1 = ResolveService("250.1.1.1", 8333);
    BOOST_CHECK(addrman.Add(CAddress(addr1, NODE_NONE), source));
    BOOST_CHECK_EQUAL(addrman.size(), 1U);

    bool newOnly = true;
    CAddrInfo addr_ret1 = addrman.Select(newOnly);
    BOOST_CHECK_EQUAL(addr_ret1.ToString(), "250.1.1.1:8333");

//测试：将addr移到tryed，从new中选择不需要返回任何内容。
    addrman.Good(CAddress(addr1, NODE_NONE));
    BOOST_CHECK_EQUAL(addrman.size(), 1U);
    CAddrInfo addr_ret2 = addrman.Select(newOnly);
    BOOST_CHECK_EQUAL(addr_ret2.ToString(), "[::]:0");

    CAddrInfo addr_ret3 = addrman.Select();
    BOOST_CHECK_EQUAL(addr_ret3.ToString(), "250.1.1.1:8333");

    BOOST_CHECK_EQUAL(addrman.size(), 1U);


//向新表添加三个地址。
    CService addr2 = ResolveService("250.3.1.1", 8333);
    CService addr3 = ResolveService("250.3.2.2", 9999);
    CService addr4 = ResolveService("250.3.3.3", 9999);

    BOOST_CHECK(addrman.Add(CAddress(addr2, NODE_NONE), ResolveService("250.3.1.1", 8333)));
    BOOST_CHECK(addrman.Add(CAddress(addr3, NODE_NONE), ResolveService("250.3.1.1", 8333)));
    BOOST_CHECK(addrman.Add(CAddress(addr4, NODE_NONE), ResolveService("250.4.1.1", 8333)));

//将三个地址添加到已尝试的表中。
    CService addr5 = ResolveService("250.4.4.4", 8333);
    CService addr6 = ResolveService("250.4.5.5", 7777);
    CService addr7 = ResolveService("250.4.6.6", 8333);

    BOOST_CHECK(addrman.Add(CAddress(addr5, NODE_NONE), ResolveService("250.3.1.1", 8333)));
    addrman.Good(CAddress(addr5, NODE_NONE));
    BOOST_CHECK(addrman.Add(CAddress(addr6, NODE_NONE), ResolveService("250.3.1.1", 8333)));
    addrman.Good(CAddress(addr6, NODE_NONE));
    BOOST_CHECK(addrman.Add(CAddress(addr7, NODE_NONE), ResolveService("250.1.1.3", 8333)));
    addrman.Good(CAddress(addr7, NODE_NONE));

//测试：6个加法器+上次测试的1个加法器=7。
    BOOST_CHECK_EQUAL(addrman.size(), 7U);

//测试：无论端口号是多少，选择“从新拉入”并尝试。
    std::set<uint16_t> ports;
    for (int i = 0; i < 20; ++i) {
        ports.insert(addrman.Select().GetPort());
    }
    BOOST_CHECK_EQUAL(ports.size(), 3U);
}

BOOST_AUTO_TEST_CASE(addrman_new_collisions)
{
    CAddrManTest addrman;

    CNetAddr source = ResolveIP("252.2.2.2");

    BOOST_CHECK_EQUAL(addrman.size(), 0U);

    for (unsigned int i = 1; i < 18; i++) {
        CService addr = ResolveService("250.1.1." + std::to_string(i));
        BOOST_CHECK(addrman.Add(CAddress(addr, NODE_NONE), source));

//测试：新表中还没有冲突。
        BOOST_CHECK_EQUAL(addrman.size(), i);
    }

//测试：新表冲突！
    CService addr1 = ResolveService("250.1.1.18");
    BOOST_CHECK(addrman.Add(CAddress(addr1, NODE_NONE), source));
    BOOST_CHECK_EQUAL(addrman.size(), 17U);

    CService addr2 = ResolveService("250.1.1.19");
    BOOST_CHECK(addrman.Add(CAddress(addr2, NODE_NONE), source));
    BOOST_CHECK_EQUAL(addrman.size(), 18U);
}

BOOST_AUTO_TEST_CASE(addrman_tried_collisions)
{
    CAddrManTest addrman;

    CNetAddr source = ResolveIP("252.2.2.2");

    BOOST_CHECK_EQUAL(addrman.size(), 0U);

    for (unsigned int i = 1; i < 80; i++) {
        CService addr = ResolveService("250.1.1." + std::to_string(i));
        BOOST_CHECK(addrman.Add(CAddress(addr, NODE_NONE), source));
        addrman.Good(CAddress(addr, NODE_NONE));

//测试：在已尝试的表中没有冲突。
        BOOST_CHECK_EQUAL(addrman.size(), i);
    }

//测试：已尝试表碰撞！
    CService addr1 = ResolveService("250.1.1.80");
    BOOST_CHECK(addrman.Add(CAddress(addr1, NODE_NONE), source));
    BOOST_CHECK_EQUAL(addrman.size(), 79U);

    CService addr2 = ResolveService("250.1.1.81");
    BOOST_CHECK(addrman.Add(CAddress(addr2, NODE_NONE), source));
    BOOST_CHECK_EQUAL(addrman.size(), 80U);
}

BOOST_AUTO_TEST_CASE(addrman_find)
{
    CAddrManTest addrman;

    BOOST_CHECK_EQUAL(addrman.size(), 0U);

    CAddress addr1 = CAddress(ResolveService("250.1.2.1", 8333), NODE_NONE);
    CAddress addr2 = CAddress(ResolveService("250.1.2.1", 9999), NODE_NONE);
    CAddress addr3 = CAddress(ResolveService("251.255.2.1", 8333), NODE_NONE);

    CNetAddr source1 = ResolveIP("250.1.2.1");
    CNetAddr source2 = ResolveIP("250.1.2.2");

    BOOST_CHECK(addrman.Add(addr1, source1));
    BOOST_CHECK(!addrman.Add(addr2, source2));
    BOOST_CHECK(addrman.Add(addr3, source1));

//测试：确保find返回与我们搜索内容匹配的IP。
    CAddrInfo* info1 = addrman.Find(addr1);
    BOOST_REQUIRE(info1);
    BOOST_CHECK_EQUAL(info1->ToString(), "250.1.2.1:8333");

//测试18；find不通过端口号区分。
    CAddrInfo* info2 = addrman.Find(addr2);
    BOOST_REQUIRE(info2);
    BOOST_CHECK_EQUAL(info2->ToString(), info1->ToString());

//测试：find返回另一个与搜索内容匹配的IP。
    CAddrInfo* info3 = addrman.Find(addr3);
    BOOST_REQUIRE(info3);
    BOOST_CHECK_EQUAL(info3->ToString(), "251.255.2.1:8333");
}

BOOST_AUTO_TEST_CASE(addrman_create)
{
    CAddrManTest addrman;

    BOOST_CHECK_EQUAL(addrman.size(), 0U);

    CAddress addr1 = CAddress(ResolveService("250.1.2.1", 8333), NODE_NONE);
    CNetAddr source1 = ResolveIP("250.1.2.1");

    int nId;
    CAddrInfo* pinfo = addrman.Create(addr1, source1, &nId);

//测试：结果应与输入地址相同。
    BOOST_CHECK_EQUAL(pinfo->ToString(), "250.1.2.1:8333");

    CAddrInfo* info2 = addrman.Find(addr1);
    BOOST_CHECK_EQUAL(info2->ToString(), "250.1.2.1:8333");
}


BOOST_AUTO_TEST_CASE(addrman_delete)
{
    CAddrManTest addrman;

    BOOST_CHECK_EQUAL(addrman.size(), 0U);

    CAddress addr1 = CAddress(ResolveService("250.1.2.1", 8333), NODE_NONE);
    CNetAddr source1 = ResolveIP("250.1.2.1");

    int nId;
    addrman.Create(addr1, source1, &nId);

//测试：delete应该实际删除地址。
    BOOST_CHECK_EQUAL(addrman.size(), 1U);
    addrman.Delete(nId);
    BOOST_CHECK_EQUAL(addrman.size(), 0U);
    CAddrInfo* info2 = addrman.Find(addr1);
    BOOST_CHECK(info2 == nullptr);
}

BOOST_AUTO_TEST_CASE(addrman_getaddr)
{
    CAddrManTest addrman;

//测试：健全性检查，如果addrman，getaddr不应返回任何内容
//是空的。
    BOOST_CHECK_EQUAL(addrman.size(), 0U);
    std::vector<CAddress> vAddr1 = addrman.GetAddr();
    BOOST_CHECK_EQUAL(vAddr1.size(), 0U);

    CAddress addr1 = CAddress(ResolveService("250.250.2.1", 8333), NODE_NONE);
addr1.nTime = GetAdjustedTime(); //设置时间，使可注册=假
    CAddress addr2 = CAddress(ResolveService("250.251.2.2", 9999), NODE_NONE);
    addr2.nTime = GetAdjustedTime();
    CAddress addr3 = CAddress(ResolveService("251.252.2.3", 8333), NODE_NONE);
    addr3.nTime = GetAdjustedTime();
    CAddress addr4 = CAddress(ResolveService("252.253.3.4", 8333), NODE_NONE);
    addr4.nTime = GetAdjustedTime();
    CAddress addr5 = CAddress(ResolveService("252.254.4.5", 8333), NODE_NONE);
    addr5.nTime = GetAdjustedTime();
    CNetAddr source1 = ResolveIP("250.1.2.1");
    CNetAddr source2 = ResolveIP("250.2.3.3");

//测试：确保getaddr与新地址一起工作。
    BOOST_CHECK(addrman.Add(addr1, source1));
    BOOST_CHECK(addrman.Add(addr2, source2));
    BOOST_CHECK(addrman.Add(addr3, source1));
    BOOST_CHECK(addrman.Add(addr4, source2));
    BOOST_CHECK(addrman.Add(addr5, source1));

//getaddr返回23%的地址，5个地址中的23%向下取整。
    BOOST_CHECK_EQUAL(addrman.GetAddr().size(), 1U);

//测试：确保getaddr使用新地址和尝试过的地址。
    addrman.Good(CAddress(addr1, NODE_NONE));
    addrman.Good(CAddress(addr2, NODE_NONE));
    BOOST_CHECK_EQUAL(addrman.GetAddr().size(), 1U);

//测试：当addrman有多个addr时，确保getaddr仍然返回23%。
    for (unsigned int i = 1; i < (8 * 256); i++) {
        int octet1 = i % 256;
        int octet2 = i >> 8 % 256;
        std::string strAddr = std::to_string(octet1) + "." + std::to_string(octet2) + ".1.23";
        CAddress addr = CAddress(ResolveService(strAddr), NODE_NONE);

//确保对于addrman中的所有addr，isterible==false。
        addr.nTime = GetAdjustedTime();
        addrman.Add(addr, ResolveIP(strAddr));
        if (i % 8 == 0)
            addrman.Good(addr);
    }
    std::vector<CAddress> vAddr = addrman.GetAddr();

    size_t percent23 = (addrman.size() * 23) / 100;
    BOOST_CHECK_EQUAL(vAddr.size(), percent23);
    BOOST_CHECK_EQUAL(vAddr.size(), 461U);
//（addrman.size（）<添加的地址数）由于地址冲突。
    BOOST_CHECK_EQUAL(addrman.size(), 2006U);
}


BOOST_AUTO_TEST_CASE(caddrinfo_get_tried_bucket)
{
    CAddrManTest addrman;

    CAddress addr1 = CAddress(ResolveService("250.1.1.1", 8333), NODE_NONE);
    CAddress addr2 = CAddress(ResolveService("250.1.1.1", 9999), NODE_NONE);

    CNetAddr source1 = ResolveIP("250.1.1.1");


    CAddrInfo info1 = CAddrInfo(addr1, source1);

    uint256 nKey1 = (uint256)(CHashWriter(SER_GETHASH, 0) << 1).GetHash();
    uint256 nKey2 = (uint256)(CHashWriter(SER_GETHASH, 0) << 2).GetHash();


    BOOST_CHECK_EQUAL(info1.GetTriedBucket(nKey1), 40);

//测试：确保键实际上随机化了桶的放置。失败了
//此测试可能是一个安全问题。
    BOOST_CHECK(info1.GetTriedBucket(nKey1) != info1.GetTriedBucket(nKey2));

//测试：两个IP相同但端口不同的地址可以映射到
//不同的桶，因为它们有不同的键。
    CAddrInfo info2 = CAddrInfo(addr2, source1);

    BOOST_CHECK(info1.GetKey() != info2.GetKey());
    BOOST_CHECK(info1.GetTriedBucket(nKey1) != info2.GetTriedBucket(nKey1));

    std::set<int> buckets;
    for (int i = 0; i < 255; i++) {
        CAddrInfo infoi = CAddrInfo(
            CAddress(ResolveService("250.1.1." + std::to_string(i)), NODE_NONE),
            ResolveIP("250.1.1." + std::to_string(i)));
        int bucket = infoi.GetTriedBucket(nKey1);
        buckets.insert(bucket);
    }
//测试：同一组中的IP地址（\16 ipv4前缀）应该
//不要超过8桶
    BOOST_CHECK_EQUAL(buckets.size(), 8U);

    buckets.clear();
    for (int j = 0; j < 255; j++) {
        CAddrInfo infoj = CAddrInfo(
            CAddress(ResolveService("250." + std::to_string(j) + ".1.1"), NODE_NONE),
            ResolveIP("250." + std::to_string(j) + ".1.1"));
        int bucket = infoj.GetTriedBucket(nKey1);
        buckets.insert(bucket);
    }
//测试：不同组中的IP地址应映射到
//8桶。
    BOOST_CHECK_EQUAL(buckets.size(), 160U);
}

BOOST_AUTO_TEST_CASE(caddrinfo_get_new_bucket)
{
    CAddrManTest addrman;

    CAddress addr1 = CAddress(ResolveService("250.1.2.1", 8333), NODE_NONE);
    CAddress addr2 = CAddress(ResolveService("250.1.2.1", 9999), NODE_NONE);

    CNetAddr source1 = ResolveIP("250.1.2.1");

    CAddrInfo info1 = CAddrInfo(addr1, source1);

    uint256 nKey1 = (uint256)(CHashWriter(SER_GETHASH, 0) << 1).GetHash();
    uint256 nKey2 = (uint256)(CHashWriter(SER_GETHASH, 0) << 2).GetHash();

//测试：确保桶是我们期望的。
    BOOST_CHECK_EQUAL(info1.GetNewBucket(nKey1), 786);
    BOOST_CHECK_EQUAL(info1.GetNewBucket(nKey1, source1), 786);

//测试：确保键实际上随机化了桶的放置。失败了
//此测试可能是一个安全问题。
    BOOST_CHECK(info1.GetNewBucket(nKey1) != info1.GetNewBucket(nKey2));

//测试：端口不应影响addr中的bucket位置
    CAddrInfo info2 = CAddrInfo(addr2, source1);
    BOOST_CHECK(info1.GetKey() != info2.GetKey());
    BOOST_CHECK_EQUAL(info1.GetNewBucket(nKey1), info2.GetNewBucket(nKey1));

    std::set<int> buckets;
    for (int i = 0; i < 255; i++) {
        CAddrInfo infoi = CAddrInfo(
            CAddress(ResolveService("250.1.1." + std::to_string(i)), NODE_NONE),
            ResolveIP("250.1.1." + std::to_string(i)));
        int bucket = infoi.GetNewBucket(nKey1);
        buckets.insert(bucket);
    }
//测试：同一组中的IP地址（\16 ipv4前缀）应该
//始终映射到同一个存储桶。
    BOOST_CHECK_EQUAL(buckets.size(), 1U);

    buckets.clear();
    for (int j = 0; j < 4 * 255; j++) {
        CAddrInfo infoj = CAddrInfo(CAddress(
                                        ResolveService(
                                            std::to_string(250 + (j / 255)) + "." + std::to_string(j % 256) + ".1.1"), NODE_NONE),
            ResolveIP("251.4.1.1"));
        int bucket = infoj.GetNewBucket(nKey1);
        buckets.insert(bucket);
    }
//测试：同一源组中的IP地址应不再映射到
//超过64个桶。
    BOOST_CHECK(buckets.size() <= 64);

    buckets.clear();
    for (int p = 0; p < 255; p++) {
        CAddrInfo infoj = CAddrInfo(
            CAddress(ResolveService("250.1.1.1"), NODE_NONE),
            ResolveIP("250." + std::to_string(p) + ".1.1"));
        int bucket = infoj.GetNewBucket(nKey1);
        buckets.insert(bucket);
    }
//测试：不同源组中的IP地址应映射到更多
//超过64个桶。
    BOOST_CHECK(buckets.size() > 64);
}


BOOST_AUTO_TEST_CASE(addrman_selecttriedcollision)
{
    CAddrManTest addrman;

//将addrman addr placement设置为确定性。
    addrman.MakeDeterministic();

    BOOST_CHECK(addrman.size() == 0);

//空的addrman应返回空的addrman信息。
    BOOST_CHECK(addrman.SelectTriedCollision().ToString() == "[::]:0");

//加上22个地址。
    CNetAddr source = ResolveIP("252.2.2.2");
    for (unsigned int i = 1; i < 23; i++) {
        CService addr = ResolveService("250.1.1."+std::to_string(i));
        BOOST_CHECK(addrman.Add(CAddress(addr, NODE_NONE), source));
        addrman.Good(addr);

//还没有碰撞。
        BOOST_CHECK(addrman.size() == i);
        BOOST_CHECK(addrman.SelectTriedCollision().ToString() == "[::]:0");
    }

//确保良好的处理重复性。
    for (unsigned int i = 1; i < 23; i++) {
        CService addr = ResolveService("250.1.1."+std::to_string(i));
        addrman.Good(addr);

        BOOST_CHECK(addrman.size() == 22);
        BOOST_CHECK(addrman.SelectTriedCollision().ToString() == "[::]:0");
    }

}

BOOST_AUTO_TEST_CASE(addrman_noevict)
{
    CAddrManTest addrman;

//将addrman addr placement设置为确定性。
    addrman.MakeDeterministic();

//加上22个地址。
    CNetAddr source = ResolveIP("252.2.2.2");
    for (unsigned int i = 1; i < 23; i++) {
        CService addr = ResolveService("250.1.1."+std::to_string(i));
        BOOST_CHECK(addrman.Add(CAddress(addr, NODE_NONE), source));
        addrman.Good(addr);

//还没有碰撞。
        BOOST_CHECK(addrman.size() == i);
        BOOST_CHECK(addrman.SelectTriedCollision().ToString() == "[::]:0");
    }

//23和19之间的碰撞。
    CService addr23 = ResolveService("250.1.1.23");
    BOOST_CHECK(addrman.Add(CAddress(addr23, NODE_NONE), source));
    addrman.Good(addr23);

    BOOST_CHECK(addrman.size() == 23);
    BOOST_CHECK(addrman.SelectTriedCollision().ToString() == "250.1.1.19:0");

//23应该被丢弃，19不能被驱逐。
    addrman.ResolveCollisions();
    BOOST_CHECK(addrman.SelectTriedCollision().ToString() == "[::]:0");

//让我们创建两个碰撞。
    for (unsigned int i = 24; i < 33; i++) {
        CService addr = ResolveService("250.1.1."+std::to_string(i));
        BOOST_CHECK(addrman.Add(CAddress(addr, NODE_NONE), source));
        addrman.Good(addr);

        BOOST_CHECK(addrman.size() == i);
        BOOST_CHECK(addrman.SelectTriedCollision().ToString() == "[::]:0");
    }

//引起碰撞。
    CService addr33 = ResolveService("250.1.1.33");
    BOOST_CHECK(addrman.Add(CAddress(addr33, NODE_NONE), source));
    addrman.Good(addr33);
    BOOST_CHECK(addrman.size() == 33);

    BOOST_CHECK(addrman.SelectTriedCollision().ToString() == "250.1.1.27:0");

//造成第二次碰撞。
    BOOST_CHECK(!addrman.Add(CAddress(addr23, NODE_NONE), source));
    addrman.Good(addr23);
    BOOST_CHECK(addrman.size() == 33);

    BOOST_CHECK(addrman.SelectTriedCollision().ToString() != "[::]:0");
    addrman.ResolveCollisions();
    BOOST_CHECK(addrman.SelectTriedCollision().ToString() == "[::]:0");
}

BOOST_AUTO_TEST_CASE(addrman_evictionworks)
{
    CAddrManTest addrman;

//将addrman addr placement设置为确定性。
    addrman.MakeDeterministic();

    BOOST_CHECK(addrman.size() == 0);

//空的addrman应返回空的addrman信息。
    BOOST_CHECK(addrman.SelectTriedCollision().ToString() == "[::]:0");

//加上22个地址。
    CNetAddr source = ResolveIP("252.2.2.2");
    for (unsigned int i = 1; i < 23; i++) {
        CService addr = ResolveService("250.1.1."+std::to_string(i));
        BOOST_CHECK(addrman.Add(CAddress(addr, NODE_NONE), source));
        addrman.Good(addr);

//还没有碰撞。
        BOOST_CHECK(addrman.size() == i);
        BOOST_CHECK(addrman.SelectTriedCollision().ToString() == "[::]:0");
    }

//23和19之间的碰撞。
    CService addr = ResolveService("250.1.1.23");
    BOOST_CHECK(addrman.Add(CAddress(addr, NODE_NONE), source));
    addrman.Good(addr);

    BOOST_CHECK(addrman.size() == 23);
    CAddrInfo info = addrman.SelectTriedCollision();
    BOOST_CHECK(info.ToString() == "250.1.1.19:0");

//确保地址测试失败，以便将其逐出。
    addrman.SimConnFail(info);

//应该把23换成19。
    addrman.ResolveCollisions();
    BOOST_CHECK(addrman.SelectTriedCollision().ToString() == "[::]:0");

//如果把23换成19，那么就不会发生碰撞。
    BOOST_CHECK(!addrman.Add(CAddress(addr, NODE_NONE), source));
    addrman.Good(addr);

    BOOST_CHECK(addrman.SelectTriedCollision().ToString() == "[::]:0");

//如果我们插入19，则应与23碰撞。
    CService addr19 = ResolveService("250.1.1.19");
    BOOST_CHECK(!addrman.Add(CAddress(addr19, NODE_NONE), source));
    addrman.Good(addr19);

    BOOST_CHECK(addrman.SelectTriedCollision().ToString() == "250.1.1.23:0");

    addrman.ResolveCollisions();
    BOOST_CHECK(addrman.SelectTriedCollision().ToString() == "[::]:0");
}


BOOST_AUTO_TEST_SUITE_END()
