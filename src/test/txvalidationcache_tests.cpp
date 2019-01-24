
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

#include <consensus/validation.h>
#include <key.h>
#include <validation.h>
#include <miner.h>
#include <pubkey.h>
#include <txmempool.h>
#include <random.h>
#include <script/standard.h>
#include <script/sign.h>
#include <test/test_bitcoin.h>
#include <util/time.h>
#include <core_io.h>
#include <keystore.h>
#include <policy/policy.h>

#include <boost/test/unit_test.hpp>

bool CheckInputs(const CTransaction& tx, CValidationState &state, const CCoinsViewCache &inputs, bool fScriptChecks, unsigned int flags, bool cacheSigStore, bool cacheFullScriptStore, PrecomputedTransactionData& txdata, std::vector<CScriptCheck> *pvChecks);

BOOST_AUTO_TEST_SUITE(tx_validationcache_tests)

static bool
ToMemPool(const CMutableTransaction& tx)
{
    LOCK(cs_main);

    CValidationState state;
    /*urn accepttomorypool（mempool，state，makeTransactionRef（tx），nullptr/*pfMissingInputs*/，
                              nullptr/*pltxnreplaced（空指针/*pltxnreplaced）*/, true /* bypass_limits */, 0 /* nAbsurdFee */);

}

BOOST_FIXTURE_TEST_CASE(tx_mempool_block_doublespend, TestChain100Setup)
{
//确保跳过验证
//验证进入内存池不允许
//在不应通过验证的情况下，在块中花费双倍的时间来通过验证。

    CScript scriptPubKey = CScript() <<  ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;

//创建成熟Coinbase TXN的双倍消费：
    std::vector<CMutableTransaction> spends;
    spends.resize(2);
    for (int i = 0; i < 2; i++)
    {
        spends[i].nVersion = 1;
        spends[i].vin.resize(1);
        spends[i].vin[0].prevout.hash = m_coinbase_txns[0]->GetHash();
        spends[i].vin[0].prevout.n = 0;
        spends[i].vout.resize(1);
        spends[i].vout[0].nValue = 11*CENT;
        spends[i].vout[0].scriptPubKey = scriptPubKey;

//符号：
        std::vector<unsigned char> vchSig;
        uint256 hash = SignatureHash(scriptPubKey, spends[i], 0, SIGHASH_ALL, 0, SigVersion::BASE);
        BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
        vchSig.push_back((unsigned char)SIGHASH_ALL);
        spends[i].vin[0].scriptSig << vchSig;
    }

    CBlock block;

//测试1：应该拒绝包含这两个事务的块。
    block = CreateAndProcessBlock(spends, scriptPubKey);
    BOOST_CHECK(chainActive.Tip()->GetBlockHash() != block.GetHash());

//测试2：…如果spend1在内存池中，则应拒绝
    BOOST_CHECK(ToMemPool(spends[0]));
    block = CreateAndProcessBlock(spends, scriptPubKey);
    BOOST_CHECK(chainActive.Tip()->GetBlockHash() != block.GetHash());
    mempool.clear();

//测试3：…如果spend2在内存池中，则应拒绝
    BOOST_CHECK(ToMemPool(spends[1]));
    block = CreateAndProcessBlock(spends, scriptPubKey);
    BOOST_CHECK(chainActive.Tip()->GetBlockHash() != block.GetHash());
    mempool.clear();

//最后的健全测试：第一次花在mempool，第二次花在block，没关系：
    std::vector<CMutableTransaction> oneSpend;
    oneSpend.push_back(spends[0]);
    BOOST_CHECK(ToMemPool(spends[1]));
    block = CreateAndProcessBlock(oneSpend, scriptPubKey);
    BOOST_CHECK(chainActive.Tip()->GetBlockHash() == block.GetHash());
//当
//已接受支出为[0]的块：
    BOOST_CHECK_EQUAL(mempool.size(), 0U);
}

//为所有脚本在给定事务上运行checkinputs（使用pcoinstip）
//旗帜。测试checkinputs是否通过所有不与
//失败的标记参数，否则失败。
//CheckLockTimeVerify和CheckSequenceVerify（以及将来可能出现的NOP代码
//重新分配）与不鼓励升级的用户进行交互：如果
//使用的脚本标志包含不鼓励升级的标记，但不包含
//checkLockTimeVerify（或checkSequenceVerify），但脚本不包含
//op_checklocktimeverify（或op_checksequenceverify），然后执行脚本
//应该失败。
//用升级后的参数捕获此交互：在评估时设置它
//作为升级的nop代码实现的任何脚本标志。
static void ValidateCheckInputsForAllFlags(const CTransaction &tx, uint32_t failing_flags, bool add_to_cache)
{
    PrecomputedTransactionData txdata(tx);
//如果我们添加更多的标志，这个循环可能会变得过于昂贵，但是我们可以
//在将来重写以随机选择一组要评估的标志。
    for (uint32_t test_flags=0; test_flags < (1U << 16); test_flags += 1) {
        CValidationState state;
//筛选出不兼容的标志选项
        if ((test_flags & SCRIPT_VERIFY_CLEANSTACK)) {
//cleanstack需要p2sh和见证，请参阅中的verifyscript（）。
//脚本/解释器.cpp
            test_flags |= SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS;
        }
        if ((test_flags & SCRIPT_VERIFY_WITNESS)) {
//证人需要p2sh
            test_flags |= SCRIPT_VERIFY_P2SH;
        }
        bool ret = CheckInputs(tx, state, pcoinsTip.get(), true, test_flags, true, add_to_cache, txdata, nullptr);
//如果测试标记不与
//故障标志
        bool expected_return_value = !(test_flags & failing_flags);
        BOOST_CHECK_EQUAL(ret, expected_return_value);

//测试缓存
        if (ret && add_to_cache) {
//检查是否在Tx有效的情况下缓存命中
            std::vector<CScriptCheck> scriptchecks;
            BOOST_CHECK(CheckInputs(tx, state, pcoinsTip.get(), true, test_flags, true, add_to_cache, txdata, &scriptchecks));
            BOOST_CHECK(scriptchecks.empty());
        } else {
//检查是否获取要检查的脚本执行，如果事务
//无效，或者我们没有添加到缓存。
            std::vector<CScriptCheck> scriptchecks;
            BOOST_CHECK(CheckInputs(tx, state, pcoinsTip.get(), true, test_flags, true, add_to_cache, txdata, &scriptchecks));
            BOOST_CHECK_EQUAL(scriptchecks.size(), tx.vin.size());
        }
    }
}

BOOST_FIXTURE_TEST_CASE(checkinputs_test, TestChain100Setup)
{
//用一组脚本标志传递checkinputs并不意味着
//我们将用另一组标志再次传递。
    {
        LOCK(cs_main);
        InitScriptExecutionCache();
    }

    CScript p2pk_scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    CScript p2sh_scriptPubKey = GetScriptForDestination(CScriptID(p2pk_scriptPubKey));
    CScript p2pkh_scriptPubKey = GetScriptForDestination(coinbaseKey.GetPubKey().GetID());
    CScript p2wpkh_scriptPubKey = GetScriptForWitness(p2pkh_scriptPubKey);

    CBasicKeyStore keystore;
    BOOST_CHECK(keystore.AddKey(coinbaseKey));
    BOOST_CHECK(keystore.AddCScript(p2pk_scriptPubKey));

//要测试的标志：script_verify_checklocktimeverify，script_verify_checksequence_verify，script_verify_nulldummy，uncompressed pubkey thing

//创建两个与上面三个脚本匹配的输出，花费第一个
//科因巴斯TX.
    CMutableTransaction spend_tx;

    spend_tx.nVersion = 1;
    spend_tx.vin.resize(1);
    spend_tx.vin[0].prevout.hash = m_coinbase_txns[0]->GetHash();
    spend_tx.vin[0].prevout.n = 0;
    spend_tx.vout.resize(4);
    spend_tx.vout[0].nValue = 11*CENT;
    spend_tx.vout[0].scriptPubKey = p2sh_scriptPubKey;
    spend_tx.vout[1].nValue = 11*CENT;
    spend_tx.vout[1].scriptPubKey = p2wpkh_scriptPubKey;
    spend_tx.vout[2].nValue = 11*CENT;
    spend_tx.vout[2].scriptPubKey = CScript() << OP_CHECKLOCKTIMEVERIFY << OP_DROP << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    spend_tx.vout[3].nValue = 11*CENT;
    spend_tx.vout[3].scriptPubKey = CScript() << OP_CHECKSEQUENCEVERIFY << OP_DROP << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;

//签名，带有非der签名
    {
        std::vector<unsigned char> vchSig;
        uint256 hash = SignatureHash(p2pk_scriptPubKey, spend_tx, 0, SIGHASH_ALL, 0, SigVersion::BASE);
        BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
vchSig.push_back((unsigned char) 0); //填充字节使此非der
        vchSig.push_back((unsigned char)SIGHASH_ALL);
        spend_tx.vin[0].scriptSig << vchSig;
    }

//测试一组标志下的无效性不排除有效性
//在其他（如共识）旗帜下。
//根据Dersig，支出tx无效
    {
        LOCK(cs_main);

        CValidationState state;
        PrecomputedTransactionData ptd_spend_tx(spend_tx);

        BOOST_CHECK(!CheckInputs(CTransaction(spend_tx), state, pcoinsTip.get(), true, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DERSIG, true, true, ptd_spend_tx, nullptr));

//如果我们再次打电话要求脚本检查（如
//ConnectBlock），我们应该为此添加一个脚本检查对象——我们
//不缓存无效性（如果更改，请删除此测试用例）。
        std::vector<CScriptCheck> scriptchecks;
        BOOST_CHECK(CheckInputs(CTransaction(spend_tx), state, pcoinsTip.get(), true, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DERSIG, true, true, ptd_spend_tx, &scriptchecks));
        BOOST_CHECK_EQUAL(scriptchecks.size(), 1U);

//测试checkinputs是否返回真正的iff dersig强制标志为
//不在场。不要将这些检查添加到缓存中，这样我们可以
//稍后测试块验证在没有缓存的情况下工作正常
//成功。
        ValidateCheckInputsForAllFlags(CTransaction(spend_tx), SCRIPT_VERIFY_DERSIG | SCRIPT_VERIFY_LOW_S | SCRIPT_VERIFY_STRICTENC, false);
    }

//如果我们用这个tx生成一个块，它应该是有效的（dersig不是
//启用），即使没有缓存项。
    CBlock block;

    block = CreateAndProcessBlock({spend_tx}, p2pk_scriptPubKey);
    BOOST_CHECK(chainActive.Tip()->GetBlockHash() == block.GetHash());
    BOOST_CHECK(pcoinsTip->GetBestBlock() == block.GetHash());

    LOCK(cs_main);

//测试p2sh：构造一个没有p2sh的有效事务，以及
//然后用p2sh测试有效性。
    {
        CMutableTransaction invalid_under_p2sh_tx;
        invalid_under_p2sh_tx.nVersion = 1;
        invalid_under_p2sh_tx.vin.resize(1);
        invalid_under_p2sh_tx.vin[0].prevout.hash = spend_tx.GetHash();
        invalid_under_p2sh_tx.vin[0].prevout.n = 0;
        invalid_under_p2sh_tx.vout.resize(1);
        invalid_under_p2sh_tx.vout[0].nValue = 11*CENT;
        invalid_under_p2sh_tx.vout[0].scriptPubKey = p2pk_scriptPubKey;
        std::vector<unsigned char> vchSig2(p2pk_scriptPubKey.begin(), p2pk_scriptPubKey.end());
        invalid_under_p2sh_tx.vin[0].scriptSig << vchSig2;

        ValidateCheckInputsForAllFlags(CTransaction(invalid_under_p2sh_tx), SCRIPT_VERIFY_P2SH, true);
    }

//测试检查锁定时间验证
    {
        CMutableTransaction invalid_with_cltv_tx;
        invalid_with_cltv_tx.nVersion = 1;
        invalid_with_cltv_tx.nLockTime = 100;
        invalid_with_cltv_tx.vin.resize(1);
        invalid_with_cltv_tx.vin[0].prevout.hash = spend_tx.GetHash();
        invalid_with_cltv_tx.vin[0].prevout.n = 2;
        invalid_with_cltv_tx.vin[0].nSequence = 0;
        invalid_with_cltv_tx.vout.resize(1);
        invalid_with_cltv_tx.vout[0].nValue = 11*CENT;
        invalid_with_cltv_tx.vout[0].scriptPubKey = p2pk_scriptPubKey;

//符号
        std::vector<unsigned char> vchSig;
        uint256 hash = SignatureHash(spend_tx.vout[2].scriptPubKey, invalid_with_cltv_tx, 0, SIGHASH_ALL, 0, SigVersion::BASE);
        BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
        vchSig.push_back((unsigned char)SIGHASH_ALL);
        invalid_with_cltv_tx.vin[0].scriptSig = CScript() << vchSig << 101;

        ValidateCheckInputsForAllFlags(CTransaction(invalid_with_cltv_tx), SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY, true);

//使其有效，然后再次检查
        invalid_with_cltv_tx.vin[0].scriptSig = CScript() << vchSig << 100;
        CValidationState state;
        PrecomputedTransactionData txdata(invalid_with_cltv_tx);
        BOOST_CHECK(CheckInputs(CTransaction(invalid_with_cltv_tx), state, pcoinsTip.get(), true, SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY, true, true, txdata, nullptr));
    }

//测试检查顺序验证
    {
        CMutableTransaction invalid_with_csv_tx;
        invalid_with_csv_tx.nVersion = 2;
        invalid_with_csv_tx.vin.resize(1);
        invalid_with_csv_tx.vin[0].prevout.hash = spend_tx.GetHash();
        invalid_with_csv_tx.vin[0].prevout.n = 3;
        invalid_with_csv_tx.vin[0].nSequence = 100;
        invalid_with_csv_tx.vout.resize(1);
        invalid_with_csv_tx.vout[0].nValue = 11*CENT;
        invalid_with_csv_tx.vout[0].scriptPubKey = p2pk_scriptPubKey;

//符号
        std::vector<unsigned char> vchSig;
        uint256 hash = SignatureHash(spend_tx.vout[3].scriptPubKey, invalid_with_csv_tx, 0, SIGHASH_ALL, 0, SigVersion::BASE);
        BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
        vchSig.push_back((unsigned char)SIGHASH_ALL);
        invalid_with_csv_tx.vin[0].scriptSig = CScript() << vchSig << 101;

        ValidateCheckInputsForAllFlags(CTransaction(invalid_with_csv_tx), SCRIPT_VERIFY_CHECKSEQUENCEVERIFY, true);

//使其有效，然后再次检查
        invalid_with_csv_tx.vin[0].scriptSig = CScript() << vchSig << 100;
        CValidationState state;
        PrecomputedTransactionData txdata(invalid_with_csv_tx);
        BOOST_CHECK(CheckInputs(CTransaction(invalid_with_csv_tx), state, pcoinsTip.get(), true, SCRIPT_VERIFY_CHECKSEQUENCEVERIFY, true, true, txdata, nullptr));
    }

//TODO:为剩余的脚本标志添加测试

//测试通过具有有效见证的检查输入并不意味着成功
//因为同一个德克萨斯州有不同的证人。
    {
        CMutableTransaction valid_with_witness_tx;
        valid_with_witness_tx.nVersion = 1;
        valid_with_witness_tx.vin.resize(1);
        valid_with_witness_tx.vin[0].prevout.hash = spend_tx.GetHash();
        valid_with_witness_tx.vin[0].prevout.n = 1;
        valid_with_witness_tx.vout.resize(1);
        valid_with_witness_tx.vout[0].nValue = 11*CENT;
        valid_with_witness_tx.vout[0].scriptPubKey = p2pk_scriptPubKey;

//符号
        SignatureData sigdata;
        BOOST_CHECK(ProduceSignature(keystore, MutableTransactionSignatureCreator(&valid_with_witness_tx, 0, 11*CENT, SIGHASH_ALL), spend_tx.vout[1].scriptPubKey, sigdata));
        UpdateInput(valid_with_witness_tx.vin[0], sigdata);

//这应该在所有脚本标志下都有效。
        ValidateCheckInputsForAllFlags(CTransaction(valid_with_witness_tx), 0, true);

//移除证人，并检查其现在是否无效。
        valid_with_witness_tx.vin[0].scriptWitness.SetNull();
        ValidateCheckInputsForAllFlags(CTransaction(valid_with_witness_tx), SCRIPT_VERIFY_WITNESS, true);
    }

    {
//使用多个输入测试事务。
        CMutableTransaction tx;

        tx.nVersion = 1;
        tx.vin.resize(2);
        tx.vin[0].prevout.hash = spend_tx.GetHash();
        tx.vin[0].prevout.n = 0;
        tx.vin[1].prevout.hash = spend_tx.GetHash();
        tx.vin[1].prevout.n = 1;
        tx.vout.resize(1);
        tx.vout[0].nValue = 22*CENT;
        tx.vout[0].scriptPubKey = p2pk_scriptPubKey;

//符号
        for (int i=0; i<2; ++i) {
            SignatureData sigdata;
            BOOST_CHECK(ProduceSignature(keystore, MutableTransactionSignatureCreator(&tx, i, 11*CENT, SIGHASH_ALL), spend_tx.vout[i].scriptPubKey, sigdata));
            UpdateInput(tx.vin[i], sigdata);
        }

//这应该在所有脚本标志下都有效
        ValidateCheckInputsForAllFlags(CTransaction(tx), 0, true);

//检查第二个输入是否无效，但第一个输入是否
//有效，未缓存事务。
//使VIN失效[1]
        tx.vin[1].scriptWitness.SetNull();

        CValidationState state;
        PrecomputedTransactionData txdata(tx);
//由于第二个输入，此事务在segwit下现在无效。
        BOOST_CHECK(!CheckInputs(CTransaction(tx), state, pcoinsTip.get(), true, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS, true, true, txdata, nullptr));

        std::vector<CScriptCheck> scriptchecks;
//确保没有缓存此事务（即因为第一个
//输入有效）
        BOOST_CHECK(CheckInputs(CTransaction(tx), state, pcoinsTip.get(), true, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS, true, true, txdata, &scriptchecks));
//应该返回2个脚本检查——缓存是基于整个事务的。
        BOOST_CHECK_EQUAL(scriptchecks.size(), 2U);
    }
}

BOOST_AUTO_TEST_SUITE_END()
