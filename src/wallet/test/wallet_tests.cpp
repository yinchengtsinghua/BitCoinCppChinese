
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2012-2019比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <wallet/wallet.h>

#include <memory>
#include <set>
#include <stdint.h>
#include <utility>
#include <vector>

#include <consensus/validation.h>
#include <interfaces/chain.h>
#include <rpc/server.h>
#include <test/test_bitcoin.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/test/wallet_test_fixture.h>
#include <policy/policy.h>

#include <boost/test/unit_test.hpp>
#include <univalue.h>

extern UniValue importmulti(const JSONRPCRequest& request);
extern UniValue dumpwallet(const JSONRPCRequest& request);
extern UniValue importwallet(const JSONRPCRequest& request);

BOOST_FIXTURE_TEST_SUITE(wallet_tests, WalletTestingSetup)

static void AddKey(CWallet& wallet, const CKey& key)
{
    LOCK(wallet.cs_wallet);
    wallet.AddKeyPubKey(key, key.GetPubKey());
}

BOOST_FIXTURE_TEST_CASE(scan_for_wallet_transactions, TestChain100Setup)
{
    auto chain = interfaces::MakeChain();

//限制最后一个块文件的大小，并在新块文件中挖掘新块。
    const CBlockIndex* const null_block = nullptr;
    CBlockIndex* oldTip = chainActive.Tip();
    GetBlockFileInfo(oldTip->GetBlockPos().nFile)->nSize = MAX_BLOCKFILE_SIZE;
    CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
    CBlockIndex* newTip = chainActive.Tip();

    auto locked_chain = chain->lock();

//验证scanforwallettransactions是否容纳空的起始块。
    {
        CWallet wallet(*chain, WalletLocation(), WalletDatabase::CreateDummy());
        AddKey(wallet, coinbaseKey);
        WalletRescanReserver reserver(&wallet);
        reserver.reserve();
        const CBlockIndex *stop_block = null_block + 1, *failed_block = null_block + 1;
        BOOST_CHECK_EQUAL(wallet.ScanForWalletTransactions(nullptr, nullptr, reserver, failed_block, stop_block), CWallet::ScanResult::SUCCESS);
        BOOST_CHECK_EQUAL(failed_block, null_block);
        BOOST_CHECK_EQUAL(stop_block, null_block);
        BOOST_CHECK_EQUAL(wallet.GetImmatureBalance(), 0);
    }

//验证scanforwallettransactions是否在两个旧的
//和新的块文件。
    {
        CWallet wallet(*chain, WalletLocation(), WalletDatabase::CreateDummy());
        AddKey(wallet, coinbaseKey);
        WalletRescanReserver reserver(&wallet);
        reserver.reserve();
        const CBlockIndex *stop_block = null_block + 1, *failed_block = null_block + 1;
        BOOST_CHECK_EQUAL(wallet.ScanForWalletTransactions(oldTip, nullptr, reserver, failed_block, stop_block), CWallet::ScanResult::SUCCESS);
        BOOST_CHECK_EQUAL(failed_block, null_block);
        BOOST_CHECK_EQUAL(stop_block, newTip);
        BOOST_CHECK_EQUAL(wallet.GetImmatureBalance(), 100 * COIN);
    }

//删除旧的块文件。
    PruneOneBlockFile(oldTip->GetBlockPos().nFile);
    UnlinkPrunedFiles({oldTip->GetBlockPos().nFile});

//验证scanforwallettransactions仅选择新块中的事务
//文件。
    {
        CWallet wallet(*chain, WalletLocation(), WalletDatabase::CreateDummy());
        AddKey(wallet, coinbaseKey);
        WalletRescanReserver reserver(&wallet);
        reserver.reserve();
        const CBlockIndex *stop_block = null_block + 1, *failed_block = null_block + 1;
        BOOST_CHECK_EQUAL(wallet.ScanForWalletTransactions(oldTip, nullptr, reserver, failed_block, stop_block), CWallet::ScanResult::FAILURE);
        BOOST_CHECK_EQUAL(failed_block, oldTip);
        BOOST_CHECK_EQUAL(stop_block, newTip);
        BOOST_CHECK_EQUAL(wallet.GetImmatureBalance(), 50 * COIN);
    }

//修剪剩余的块文件。
    PruneOneBlockFile(newTip->GetBlockPos().nFile);
    UnlinkPrunedFiles({newTip->GetBlockPos().nFile});

//验证scanforwallettransactions是否扫描无块。
    {
        CWallet wallet(*chain, WalletLocation(), WalletDatabase::CreateDummy());
        AddKey(wallet, coinbaseKey);
        WalletRescanReserver reserver(&wallet);
        reserver.reserve();
        const CBlockIndex *stop_block = null_block + 1, *failed_block = null_block + 1;
        BOOST_CHECK_EQUAL(wallet.ScanForWalletTransactions(oldTip, nullptr, reserver, failed_block, stop_block), CWallet::ScanResult::FAILURE);
        BOOST_CHECK_EQUAL(failed_block, newTip);
        BOOST_CHECK_EQUAL(stop_block, null_block);
        BOOST_CHECK_EQUAL(wallet.GetImmatureBalance(), 0);
    }
}

BOOST_FIXTURE_TEST_CASE(importmulti_rescan, TestChain100Setup)
{
    auto chain = interfaces::MakeChain();

//限制最后一个块文件的大小，并在新块文件中挖掘新块。
    CBlockIndex* oldTip = chainActive.Tip();
    GetBlockFileInfo(oldTip->GetBlockPos().nFile)->nSize = MAX_BLOCKFILE_SIZE;
    CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
    CBlockIndex* newTip = chainActive.Tip();

    auto locked_chain = chain->lock();

//删除旧的块文件。
    PruneOneBlockFile(oldTip->GetBlockPos().nFile);
    UnlinkPrunedFiles({oldTip->GetBlockPos().nFile});

//验证importmulti-rpc为其创建时间为的密钥返回失败
//在丢失的块之前，以及创建时间为
//之后。
    {
        std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>(*chain, WalletLocation(), WalletDatabase::CreateDummy());
        AddWallet(wallet);
        UniValue keys;
        keys.setArray();
        UniValue key;
        key.setObject();
        key.pushKV("scriptPubKey", HexStr(GetScriptForRawPubKey(coinbaseKey.GetPubKey())));
        key.pushKV("timestamp", 0);
        key.pushKV("internal", UniValue(true));
        keys.push_back(key);
        key.clear();
        key.setObject();
        CKey futureKey;
        futureKey.MakeNewKey(true);
        key.pushKV("scriptPubKey", HexStr(GetScriptForRawPubKey(futureKey.GetPubKey())));
        key.pushKV("timestamp", newTip->GetBlockTimeMax() + TIMESTAMP_WINDOW + 1);
        key.pushKV("internal", UniValue(true));
        keys.push_back(key);
        JSONRPCRequest request;
        request.params.setArray();
        request.params.push_back(keys);

        UniValue response = importmulti(request);
        BOOST_CHECK_EQUAL(response.write(),
            strprintf("[{\"success\":false,\"error\":{\"code\":-1,\"message\":\"Rescan failed for key with creation "
                      "timestamp %d. There was an error reading a block from time %d, which is after or within %d "
                      "seconds of key creation, and could contain transactions pertaining to the key. As a result, "
                      "transactions and coins using this key may not appear in the wallet. This error could be caused "
                      "by pruning or data corruption (see bitcoind log for details) and could be dealt with by "
                      "downloading and rescanning the relevant blocks (see -reindex and -rescan "
                      "options).\"}},{\"success\":true}]",
                              0, oldTip->GetBlockTimeMax(), TIMESTAMP_WINDOW));
        RemoveWallet(wallet);
    }
}

//验证importwallet rpc是否最早使用时间戳块重新扫描
//大于或等于密钥生日。以前有个bug
//importwallet rpc将在最新块开始扫描，时间戳较少
//大于或等于密钥生日。
BOOST_FIXTURE_TEST_CASE(importwallet_rescan, TestChain100Setup)
{
    auto chain = interfaces::MakeChain();

//创建两个具有相同时间戳的块，以验证importwallet是否重新扫描
//会把两个街区都捡起来，而不仅仅是第一个街区。
    const int64_t BLOCK_TIME = chainActive.Tip()->GetBlockTimeMax() + 5;
    SetMockTime(BLOCK_TIME);
    m_coinbase_txns.emplace_back(CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey())).vtx[0]);
    m_coinbase_txns.emplace_back(CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey())).vtx[0]);

//将“密钥生日”设置为阻止时间戳窗口增加的时间，因此
//重新扫描将在阻止时间开始。
    const int64_t KEY_TIME = BLOCK_TIME + TIMESTAMP_WINDOW;
    SetMockTime(KEY_TIME);
    m_coinbase_txns.emplace_back(CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey())).vtx[0]);

    auto locked_chain = chain->lock();

    std::string backup_file = (SetDataDir("importwallet_rescan") / "wallet.backup").string();

//将密钥导入钱包，然后调用dumpwallet创建备份文件。
    {
        std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>(*chain, WalletLocation(), WalletDatabase::CreateDummy());
        LOCK(wallet->cs_wallet);
        wallet->mapKeyMetadata[coinbaseKey.GetPubKey().GetID()].nCreateTime = KEY_TIME;
        wallet->AddKeyPubKey(coinbaseKey, coinbaseKey.GetPubKey());

        JSONRPCRequest request;
        request.params.setArray();
        request.params.push_back(backup_file);
        AddWallet(wallet);
        ::dumpwallet(request);
        RemoveWallet(wallet);
    }

//调用importwallet rpc并验证时间戳>=块时间的所有块
//扫描，没有先前的块被扫描。
    {
        std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>(*chain, WalletLocation(), WalletDatabase::CreateDummy());

        JSONRPCRequest request;
        request.params.setArray();
        request.params.push_back(backup_file);
        AddWallet(wallet);
        ::importwallet(request);
        RemoveWallet(wallet);

        LOCK(wallet->cs_wallet);
        BOOST_CHECK_EQUAL(wallet->mapWallet.size(), 3U);
        BOOST_CHECK_EQUAL(m_coinbase_txns.size(), 103U);
        for (size_t i = 0; i < m_coinbase_txns.size(); ++i) {
            bool found = wallet->GetWalletTx(m_coinbase_txns[i]->GetHash());
            bool expected = i >= 100;
            BOOST_CHECK_EQUAL(found, expected);
        }
    }

    SetMockTime(0);
}

//检查getUndulimeCredit（）是否返回新计算的值，而不是
//markdirty（）调用后的缓存值。
//
//这是一个回归测试，用于验证未到期信贷的错误修复。
//功能。类似的测试可能应该为另一个学分而编写，并且
//借记功能。
BOOST_FIXTURE_TEST_CASE(coin_mark_dirty_immature_credit, TestChain100Setup)
{
    auto chain = interfaces::MakeChain();
    CWallet wallet(*chain, WalletLocation(), WalletDatabase::CreateDummy());
    CWalletTx wtx(&wallet, m_coinbase_txns.back());
    auto locked_chain = chain->lock();
    LOCK(wallet.cs_wallet);
    wtx.hashBlock = chainActive.Tip()->GetBlockHash();
    wtx.nIndex = 0;

//在将密钥添加到钱包之前调用一次getUndulimeCredit（）。
//缓存当前未到期的信贷金额，即0。
    BOOST_CHECK_EQUAL(wtx.GetImmatureCredit(*locked_chain), 0);

//使缓存值无效，添加密钥，并确保新的不成熟
//计算贷方金额。
    wtx.MarkDirty();
    wallet.AddKeyPubKey(coinbaseKey, coinbaseKey.GetPubKey());
    BOOST_CHECK_EQUAL(wtx.GetImmatureCredit(*locked_chain), 50*COIN);
}

static int64_t AddTx(CWallet& wallet, uint32_t lockTime, int64_t mockTime, int64_t blockTime)
{
    CMutableTransaction tx;
    tx.nLockTime = lockTime;
    SetMockTime(mockTime);
    CBlockIndex* block = nullptr;
    if (blockTime > 0) {
        auto locked_chain = wallet.chain().lock();
        auto inserted = mapBlockIndex.emplace(GetRandHash(), new CBlockIndex);
        assert(inserted.second);
        const uint256& hash = inserted.first->first;
        block = inserted.first->second;
        block->nTime = blockTime;
        block->phashBlock = &hash;
    }

    CWalletTx wtx(&wallet, MakeTransactionRef(tx));
    if (block) {
        wtx.SetMerkleBranch(block, 0);
    }
    {
        LOCK(cs_main);
        wallet.AddToWallet(wtx);
    }
    LOCK(wallet.cs_wallet);
    return wallet.mapWallet.at(wtx.GetHash()).nTimeSmart;
}

//验证cwallettx:：nsmarttime值分配的简单测试。可以是
//扩展以涵盖更多智能时间逻辑的角落案例。
BOOST_AUTO_TEST_CASE(ComputeTimeSmart)
{
//新事务应使用时钟时间（如果低于阻塞时间）。
    BOOST_CHECK_EQUAL(AddTx(m_wallet, 1, 100, 120), 100);

//测试更新现有事务不会更改智能时间。
    BOOST_CHECK_EQUAL(AddTx(m_wallet, 1, 200, 220), 100);

//如果没有阻塞时间，新事务应该使用时钟时间。
    BOOST_CHECK_EQUAL(AddTx(m_wallet, 2, 300, 0), 300);

//如果低于时钟时间，则新事务应使用阻塞时间。
    BOOST_CHECK_EQUAL(AddTx(m_wallet, 3, 420, 400), 400);

//如果新事务的输入时间大于
//min（阻塞时间、时钟时间）。
    BOOST_CHECK_EQUAL(AddTx(m_wallet, 4, 500, 390), 400);

//如果有将来的条目，则新事务应使用
//最新的条目，比时钟时间提前不超过300秒。
    BOOST_CHECK_EQUAL(AddTx(m_wallet, 5, 50, 600), 300);

//重置其他测试的模拟时间。
    SetMockTime(0);
}

BOOST_AUTO_TEST_CASE(LoadReceiveRequests)
{
    CTxDestination dest = CKeyID();
    LOCK(m_wallet.cs_wallet);
    m_wallet.AddDestData(dest, "misc", "val_misc");
    m_wallet.AddDestData(dest, "rr0", "val_rr0");
    m_wallet.AddDestData(dest, "rr1", "val_rr1");

    auto values = m_wallet.GetDestValues("rr");
    BOOST_CHECK_EQUAL(values.size(), 2U);
    BOOST_CHECK_EQUAL(values[0], "val_rr0");
    BOOST_CHECK_EQUAL(values[1], "val_rr1");
}

class ListCoinsTestingSetup : public TestChain100Setup
{
public:
    ListCoinsTestingSetup()
    {
        CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        wallet = MakeUnique<CWallet>(*m_chain, WalletLocation(), WalletDatabase::CreateMock());
        bool firstRun;
        wallet->LoadWallet(firstRun);
        AddKey(*wallet, coinbaseKey);
        WalletRescanReserver reserver(wallet.get());
        reserver.reserve();
        const CBlockIndex* const null_block = nullptr;
        const CBlockIndex *stop_block = null_block + 1, *failed_block = null_block + 1;
        BOOST_CHECK_EQUAL(wallet->ScanForWalletTransactions(chainActive.Genesis(), nullptr, reserver, failed_block, stop_block), CWallet::ScanResult::SUCCESS);
        BOOST_CHECK_EQUAL(stop_block, chainActive.Tip());
        BOOST_CHECK_EQUAL(failed_block, null_block);
    }

    ~ListCoinsTestingSetup()
    {
        wallet.reset();
    }

    CWalletTx& AddTx(CRecipient recipient)
    {
        CTransactionRef tx;
        CReserveKey reservekey(wallet.get());
        CAmount fee;
        int changePos = -1;
        std::string error;
        CCoinControl dummy;
        BOOST_CHECK(wallet->CreateTransaction(*m_locked_chain, {recipient}, tx, reservekey, fee, changePos, error, dummy));
        CValidationState state;
        BOOST_CHECK(wallet->CommitTransaction(tx, {}, {}, reservekey, nullptr, state));
        CMutableTransaction blocktx;
        {
            LOCK(wallet->cs_wallet);
            blocktx = CMutableTransaction(*wallet->mapWallet.at(tx->GetHash()).tx);
        }
        CreateAndProcessBlock({CMutableTransaction(blocktx)}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        LOCK(wallet->cs_wallet);
        auto it = wallet->mapWallet.find(tx->GetHash());
        BOOST_CHECK(it != wallet->mapWallet.end());
        it->second.SetMerkleBranch(chainActive.Tip(), 1);
        return it->second;
    }

    std::unique_ptr<interfaces::Chain> m_chain = interfaces::MakeChain();
std::unique_ptr<interfaces::Chain::Lock> m_locked_chain = m_chain->assumeLocked();  //暂时的。在即将进行的锁清理中删除
    std::unique_ptr<CWallet> wallet;
};

BOOST_FIXTURE_TEST_CASE(ListCoins, ListCoinsTestingSetup)
{
    std::string coinbaseAddress = coinbaseKey.GetPubKey().GetID().ToString();

//确认listcoins最初返回1个硬币，分组在coinbasekey下
//地址。
    std::map<CTxDestination, std::vector<COutput>> list;
    {
        LOCK2(cs_main, wallet->cs_wallet);
        list = wallet->ListCoins(*m_locked_chain);
    }
    BOOST_CHECK_EQUAL(list.size(), 1U);
    BOOST_CHECK_EQUAL(boost::get<CKeyID>(list.begin()->first).ToString(), coinbaseAddress);
    BOOST_CHECK_EQUAL(list.begin()->second.size(), 1U);

//检查一个成熟的Coinbase交易的初始余额。
    BOOST_CHECK_EQUAL(50 * COIN, wallet->GetAvailableBalance());

//添加创建更改地址的交易，并确认listcoins仍然
//返回与下面的更改地址相关联的硬币
//coinbasekey pubkey，即使更改地址有不同的
//普基
    /*tx（crecipient getscriptforrawpubkey（），1*硬币，假/*减去费用*/）；
    {
        Lock2（钱夹，钱夹->钱夹）；
        list=wallet->listcoins（*m_locked_chain）；
    }
    boost_check_equal（list.size（），1U）；
    boost_check_equal（boost:：get<ckeyid>（list.begin（）->first）.toString（），coinBaseAddress）；
    boost_check_equal（list.begin（）->second.size（），2u）；

    //锁定两个硬币。确认可用硬币数量降至0。
    {
        Lock2（钱夹，钱夹->钱夹）；
        std:：vector<coutput>可用；
        钱包->可用硬币（*M_锁定的_链，可用）；
        boost_check_equal（available.size（），2u）；
    }
    对于（const auto&group:list）
        用于（const auto&coin:group.second）
            锁（钱包->cs_钱包）；
            wallet->lockcoin（coutpoint（coutpoint.tx->gethash（），coin.i））；
        }
    }
    {
        Lock2（钱夹，钱夹->钱夹）；
        std:：vector<coutput>可用；
        钱包->可用硬币（*M_锁定的_链，可用）；
        boost_check_equal（available.size（），0u）；
    }
    //确认listcoins仍然返回与以前相同的结果，尽管有硬币
    //被锁定。
    {
        Lock2（钱夹，钱夹->钱夹）；
        list=wallet->listcoins（*m_locked_chain）；
    }
    boost_check_equal（list.size（），1U）；
    boost_check_equal（boost:：get<ckeyid>（list.begin（）->first）.toString（），coinBaseAddress）；
    boost_check_equal（list.begin（）->second.size（），2u）；
}

Boost_fixture_test_case（wallet_disableprivkeys，testchain100设置）
{
    auto-chain=接口：：makechain（）；
    std:：shared_ptr<cwallet>wallet=std:：make_shared<cwallet>（*chain，walletlocation（），walletdatabase:：createdDummy（））；
    wallet->setwalletflag（wallet_flag_disable_private_keys）；
    加油！wallet->topupkeypool（1000））；
    CPubKey pubkey；
    加油！wallet->getkefrompool（pubkey，false））；
}

//用于测试钱包常数的显式计算
//由于四舍五入（weight/4），我们得到相同的虚拟大小，这两个值都使用了\u max_sig值
静态大小\u t calculateNestedKeyHashinputsize（bool使用\u max\u sig）
{
    //生成短暂有效的pubkey
    密钥；
    key.makenewkey（真）；
    cpubkey pubkey=key.getpubkey（）；

    //生成pubkey散列
    uint160 key_hash（hash160（pubkey.begin（），pubkey.end（））；

    //创建要进入密钥库的内部脚本。密钥哈希不能为0…
    cscript inner_script=cscript（）<<op_0<<std:：vector<unsigned char>（key_hash.begin（），key_hash.end（））；

    //为输出创建外部p2sh脚本
    uint160 script_id（hash160（inner_script.begin（），inner_script.end（））；
    cscript script_pubkey=cscript（）<<op_hash160<<std:：vector<unsigned char>（script_id.begin（），script_id.end（））<<op_equal；

    //将内部脚本添加到密钥存储，将密钥添加到仅监视
    cbasickeystore密钥库；
    keystore.addcscript（内部脚本）；
    keystore.addkeypubkey（key，pubkey）；

    //填写虚拟签名进行费用计算。
    签名数据；

    如果（！）ProduceSignature（keystore，使用\u max\u sig？dummy_maximum_signature_creator:dummy_signature_creator，script_pubkey，sig_data））
        //我们用手给它提供了正确的参数；不应该发生
        断言（假）；
    }

    CTX-TXIIN；
    更新输入（tx_-in，sig_-data）；
    返回（size_t）getVirtualTransactionInputSize（tx_in）；
}

Boost_fixture_test_case（虚拟_input_size_test，testchain100设置）
{
    boost_check_equal（calculateNestedKeyHashinputsize（false），dummy_nested_p2wpkh_input_size）；
    boost_check_equal（calculateNestedKeyHashinputsize（true），dummy_nested_p2wpkh_input_size）；
}

Boost_Auto_测试套件_end（）
