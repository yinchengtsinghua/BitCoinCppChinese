
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

//拒绝服务检测/预防代码的单元测试

#include <chainparams.h>
#include <keystore.h>
#include <net.h>
#include <net_processing.h>
#include <pow.h>
#include <script/sign.h>
#include <serialize.h>
#include <util/system.h>
#include <validation.h>

#include <test/test_bitcoin.h>

#include <stdint.h>

#include <boost/test/unit_test.hpp>

//测试这些内部到网络处理.cpp方法：
extern bool AddOrphanTx(const CTransactionRef& tx, NodeId peer);
extern void EraseOrphansFor(NodeId peer);
extern unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans);
extern void Misbehaving(NodeId nodeid, int howmuch, const std::string& message="");

struct COrphanTx {
    CTransactionRef tx;
    NodeId fromPeer;
    int64_t nTimeExpire;
};
extern CCriticalSection g_cs_orphans;
extern std::map<uint256, COrphanTx> mapOrphanTransactions GUARDED_BY(g_cs_orphans);

static CService ip(uint32_t i)
{
    struct in_addr s;
    s.s_addr = i;
    return CService(CNetAddr(s), Params().GetDefaultPort());
}

static NodeId id = 0;

void UpdateLastBlockAnnounceTime(NodeId node, int64_t time_in_seconds);

BOOST_FIXTURE_TEST_SUITE(denialofservice_tests, TestingSetup)

//测试逐出链从不前进的出站对等机
//模拟节点连接，并使用mocktime模拟对等节点
//它从不发送任何头消息。PeerLogic应该
//在适当的超时之后，决定退出该出站对等机。
//注意，我们保护4个出站节点不受
//此逻辑；此测试仅利用该保护
//应用于发送头文件的节点
//工作。
BOOST_AUTO_TEST_CASE(outbound_slow_chain_eviction)
{

//模拟出站对等机
    CAddress addr1(ip(0xa0b0c001), NODE_NONE);
    /*de dummynode1（id++，serviceflags（node_network_node_wistence），0，无效的_socket，addr1，0，0，caddress（），“”，/*finboundIn=*/false）；
    dummynode1.setsendversion（协议版本）；

    peerlogic->initializenode（&dummynode1）；
    dummynode1.nversion=1；
    dummynode1.fsaccessfullyconnected=true；

    //此测试要求我们有一个具有非零工作的链。
    {
        锁（CSKEMAN）；
        Boost_检查（chainActive.tip（）！= null pTr）；
        boost_check（chainactive.tip（）->nchainwork>0）；
    }

    //测试从这里开始
    {
        lock2（cs_-main，dummynode1.cs_-sendprocessing）；
        boost_check（peerlogic->sendmessages（&dummynode1））；//应导致getheaders
    }
    {
        锁2（cs_main，dummynode1.cs_vsend）；
        Boost_检查（dummynode1.vsendmsg.size（）>0）；
        dummynode1.vsendmsg.clear（）；
    }

    Int64_t InstartTime=GetTime（）；
    //等待21分钟
    setmocktime（开始时间+21*60）；
    {
        lock2（cs_-main，dummynode1.cs_-sendprocessing）；
        boost_check（peerlogic->sendmessages（&dummynode1））；//应导致getheaders
    }
    {
        锁2（cs_main，dummynode1.cs_vsend）；
        Boost_检查（dummynode1.vsendmsg.size（）>0）；
    }
    //再等3分钟
    setmocktime（开始时间+24*60）；
    {
        lock2（cs_-main，dummynode1.cs_-sendprocessing）；
        boost_check（peerlogic->sendmessages（&dummynode1））；//应导致断开连接
    }
    增强检查（dummynode1.fdisconnect==true）；
    StimoCKTIME（0）；

    布尔傀儡；
    peerlogic->finalizenode（dummynode1.getid（），dummy）；
}

静态void addrandomoutboundpeer（std:：vector<cnode/>&vnodes，peerlogicvalidation&peerlogic）
{
    caddress addr（ip（g_unsecure_rand_ctx.randbits（32）），node_none）；
    vnodes.emplace_back（新的cnode（id++，serviceflags（node_network node_envisence），0，无效的_socket，addr，0，0，caddress（），“”，/*finboundIn*/ false));

    CNode &node = *vNodes.back();
    node.SetSendVersion(PROTOCOL_VERSION);

    peerLogic.InitializeNode(&node);
    node.nVersion = 1;
    node.fSuccessfullyConnected = true;

    CConnmanTest::AddNode(node);
}

BOOST_AUTO_TEST_CASE(stale_tip_peer_management)
{
    const Consensus::Params& consensusParams = Params().GetConsensus();
    constexpr int nMaxOutbound = 8;
    CConnman::Options options;
    options.nMaxConnections = 125;
    options.nMaxOutbound = nMaxOutbound;
    options.nMaxFeeler = 1;

    connman->Init(options);
    std::vector<CNode *> vNodes;

//模拟一些出站对等机
    for (int i=0; i<nMaxOutbound; ++i) {
        AddRandomOutboundPeer(vNodes, *peerLogic);
    }

    peerLogic->CheckForStaleTipAndEvictPeers(consensusParams);

//当我们没有额外的对等节点时，不应将节点标记为断开连接。
    for (const CNode *node : vNodes) {
        BOOST_CHECK(node->fDisconnect == false);
    }

    SetMockTime(GetTime() + 3*consensusParams.nPowTargetSpacing + 1);

//现在小费应该已经过时了，我们应该再找一个
//出站对等体
    peerLogic->CheckForStaleTipAndEvictPeers(consensusParams);
    BOOST_CHECK(connman->GetTryNewOutboundPeer());

//仍然不应标记任何对等端断开连接
    for (const CNode *node : vNodes) {
        BOOST_CHECK(node->fDisconnect == false);
    }

//如果我们再添加一个对等点，则应将某些内容标记为要逐出。
//在下次检查时（因为我们在嘲笑未来的时间，
//应满足所需的连接时间检查）。
    AddRandomOutboundPeer(vNodes, *peerLogic);

    peerLogic->CheckForStaleTipAndEvictPeers(consensusParams);
    for (int i=0; i<nMaxOutbound; ++i) {
        BOOST_CHECK(vNodes[i]->fDisconnect == false);
    }
//最后添加的节点应标记为逐出
    BOOST_CHECK(vNodes.back()->fDisconnect == true);

    vNodes.back()->fDisconnect = false;

//更新最后一个公告的阻止时间
//对等，并检查下一个最新节点是否被逐出。
    UpdateLastBlockAnnounceTime(vNodes.back()->GetId(), GetTime());

    peerLogic->CheckForStaleTipAndEvictPeers(consensusParams);
    for (int i=0; i<nMaxOutbound-1; ++i) {
        BOOST_CHECK(vNodes[i]->fDisconnect == false);
    }
    BOOST_CHECK(vNodes[nMaxOutbound-1]->fDisconnect == true);
    BOOST_CHECK(vNodes.back()->fDisconnect == false);

    bool dummy;
    for (const CNode *node : vNodes) {
        peerLogic->FinalizeNode(node->GetId(), dummy);
    }

    CConnmanTest::ClearNodes();
}

BOOST_AUTO_TEST_CASE(DoS_banning)
{

    connman->ClearBanned();
    CAddress addr1(ip(0xa0b0c001), NODE_NONE);
    CNode dummyNode1(id++, NODE_NETWORK, 0, INVALID_SOCKET, addr1, 0, 0, CAddress(), "", true);
    dummyNode1.SetSendVersion(PROTOCOL_VERSION);
    peerLogic->InitializeNode(&dummyNode1);
    dummyNode1.nVersion = 1;
    dummyNode1.fSuccessfullyConnected = true;
    {
        LOCK(cs_main);
Misbehaving(dummyNode1.GetId(), 100); //应该被禁止
    }
    {
        LOCK2(cs_main, dummyNode1.cs_sendProcessing);
        BOOST_CHECK(peerLogic->SendMessages(&dummyNode1));
    }
    BOOST_CHECK(connman->IsBanned(addr1));
BOOST_CHECK(!connman->IsBanned(ip(0xa0b0c001|0x0000ff00))); //不同的IP，不禁止

    CAddress addr2(ip(0xa0b0c002), NODE_NONE);
    CNode dummyNode2(id++, NODE_NETWORK, 0, INVALID_SOCKET, addr2, 1, 1, CAddress(), "", true);
    dummyNode2.SetSendVersion(PROTOCOL_VERSION);
    peerLogic->InitializeNode(&dummyNode2);
    dummyNode2.nVersion = 1;
    dummyNode2.fSuccessfullyConnected = true;
    {
        LOCK(cs_main);
        Misbehaving(dummyNode2.GetId(), 50);
    }
    {
        LOCK2(cs_main, dummyNode2.cs_sendProcessing);
        BOOST_CHECK(peerLogic->SendMessages(&dummyNode2));
    }
BOOST_CHECK(!connman->IsBanned(addr2)); //2还没有被禁止……
BOOST_CHECK(connman->IsBanned(addr1));  //…但我还是应该
    {
        LOCK(cs_main);
        Misbehaving(dummyNode2.GetId(), 50);
    }
    {
        LOCK2(cs_main, dummyNode2.cs_sendProcessing);
        BOOST_CHECK(peerLogic->SendMessages(&dummyNode2));
    }
    BOOST_CHECK(connman->IsBanned(addr2));

    bool dummy;
    peerLogic->FinalizeNode(dummyNode1.GetId(), dummy);
    peerLogic->FinalizeNode(dummyNode2.GetId(), dummy);
}

BOOST_AUTO_TEST_CASE(DoS_banscore)
{

    connman->ClearBanned();
gArgs.ForceSetArg("-banscore", "111"); //因为11是我最喜欢的号码
    CAddress addr1(ip(0xa0b0c001), NODE_NONE);
    CNode dummyNode1(id++, NODE_NETWORK, 0, INVALID_SOCKET, addr1, 3, 1, CAddress(), "", true);
    dummyNode1.SetSendVersion(PROTOCOL_VERSION);
    peerLogic->InitializeNode(&dummyNode1);
    dummyNode1.nVersion = 1;
    dummyNode1.fSuccessfullyConnected = true;
    {
        LOCK(cs_main);
        Misbehaving(dummyNode1.GetId(), 100);
    }
    {
        LOCK2(cs_main, dummyNode1.cs_sendProcessing);
        BOOST_CHECK(peerLogic->SendMessages(&dummyNode1));
    }
    BOOST_CHECK(!connman->IsBanned(addr1));
    {
        LOCK(cs_main);
        Misbehaving(dummyNode1.GetId(), 10);
    }
    {
        LOCK2(cs_main, dummyNode1.cs_sendProcessing);
        BOOST_CHECK(peerLogic->SendMessages(&dummyNode1));
    }
    BOOST_CHECK(!connman->IsBanned(addr1));
    {
        LOCK(cs_main);
        Misbehaving(dummyNode1.GetId(), 1);
    }
    {
        LOCK2(cs_main, dummyNode1.cs_sendProcessing);
        BOOST_CHECK(peerLogic->SendMessages(&dummyNode1));
    }
    BOOST_CHECK(connman->IsBanned(addr1));
    gArgs.ForceSetArg("-banscore", std::to_string(DEFAULT_BANSCORE_THRESHOLD));

    bool dummy;
    peerLogic->FinalizeNode(dummyNode1.GetId(), dummy);
}

BOOST_AUTO_TEST_CASE(DoS_bantime)
{

    connman->ClearBanned();
    int64_t nStartTime = GetTime();
SetMockTime(nStartTime); //重写将来对gettime（）的调用

    CAddress addr(ip(0xa0b0c001), NODE_NONE);
    CNode dummyNode(id++, NODE_NETWORK, 0, INVALID_SOCKET, addr, 4, 4, CAddress(), "", true);
    dummyNode.SetSendVersion(PROTOCOL_VERSION);
    peerLogic->InitializeNode(&dummyNode);
    dummyNode.nVersion = 1;
    dummyNode.fSuccessfullyConnected = true;

    {
        LOCK(cs_main);
        Misbehaving(dummyNode.GetId(), 100);
    }
    {
        LOCK2(cs_main, dummyNode.cs_sendProcessing);
        BOOST_CHECK(peerLogic->SendMessages(&dummyNode));
    }
    BOOST_CHECK(connman->IsBanned(addr));

    SetMockTime(nStartTime+60*60);
    BOOST_CHECK(connman->IsBanned(addr));

    SetMockTime(nStartTime+60*60*24+1);
    BOOST_CHECK(!connman->IsBanned(addr));

    bool dummy;
    peerLogic->FinalizeNode(dummyNode.GetId(), dummy);
}

static CTransactionRef RandomOrphan()
{
    std::map<uint256, COrphanTx>::iterator it;
    LOCK2(cs_main, g_cs_orphans);
    it = mapOrphanTransactions.lower_bound(InsecureRand256());
    if (it == mapOrphanTransactions.end())
        it = mapOrphanTransactions.begin();
    return it->second.tx;
}

BOOST_AUTO_TEST_CASE(DoS_mapOrphans)
{
    CKey key;
    key.MakeNewKey(true);
    CBasicKeyStore keystore;
    BOOST_CHECK(keystore.AddKey(key));

//50个孤立事务：
    for (int i = 0; i < 50; i++)
    {
        CMutableTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout.n = 0;
        tx.vin[0].prevout.hash = InsecureRand256();
        tx.vin[0].scriptSig << OP_1;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

        AddOrphanTx(MakeTransactionRef(tx), i);
    }

//…还有50个依靠其他孤儿：
    for (int i = 0; i < 50; i++)
    {
        CTransactionRef txPrev = RandomOrphan();

        CMutableTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout.n = 0;
        tx.vin[0].prevout.hash = txPrev->GetHash();
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());
        BOOST_CHECK(SignSignature(keystore, *txPrev, tx, 0, SIGHASH_ALL));

        AddOrphanTx(MakeTransactionRef(tx), i);
    }

//这个真正的大孤儿应该被忽略：
    for (int i = 0; i < 10; i++)
    {
        CTransactionRef txPrev = RandomOrphan();

        CMutableTransaction tx;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());
        tx.vin.resize(2777);
        for (unsigned int j = 0; j < tx.vin.size(); j++)
        {
            tx.vin[j].prevout.n = j;
            tx.vin[j].prevout.hash = txPrev->GetHash();
        }
        BOOST_CHECK(SignSignature(keystore, *txPrev, tx, 0, SIGHASH_ALL));
//对其他输入重复使用相同的签名
//（他们不一定要在这个测试中有效）
        for (unsigned int j = 1; j < tx.vin.size(); j++)
            tx.vin[j].scriptSig = tx.vin[0].scriptSig;

        BOOST_CHECK(!AddOrphanTx(MakeTransactionRef(tx), i));
    }

    LOCK2(cs_main, g_cs_orphans);
//测试擦除方法：
    for (NodeId i = 0; i < 3; i++)
    {
        size_t sizeBefore = mapOrphanTransactions.size();
        EraseOrphansFor(i);
        BOOST_CHECK(mapOrphanTransactions.size() < sizeBefore);
    }

//test limitorphantxsize（）函数：
    LimitOrphanTxSize(40);
    BOOST_CHECK(mapOrphanTransactions.size() <= 40);
    LimitOrphanTxSize(10);
    BOOST_CHECK(mapOrphanTransactions.size() <= 10);
    LimitOrphanTxSize(0);
    BOOST_CHECK(mapOrphanTransactions.empty());
}

BOOST_AUTO_TEST_SUITE_END()
