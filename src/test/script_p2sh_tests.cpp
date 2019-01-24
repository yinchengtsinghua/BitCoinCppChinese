
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

#include <consensus/tx_verify.h>
#include <core_io.h>
#include <key.h>
#include <keystore.h>
#include <validation.h>
#include <policy/policy.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/sign.h>
#include <script/ismine.h>
#include <test/test_bitcoin.h>

#include <vector>

#include <boost/test/unit_test.hpp>

//帮手：
static std::vector<unsigned char>
Serialize(const CScript& s)
{
    std::vector<unsigned char> sSerialized(s.begin(), s.end());
    return sSerialized;
}

static bool
Verify(const CScript& scriptSig, const CScript& scriptPubKey, bool fStrict, ScriptError& err)
{
//创建虚拟的往来交易：
    CMutableTransaction txFrom;
    txFrom.vout.resize(1);
    txFrom.vout[0].scriptPubKey = scriptPubKey;

    CMutableTransaction txTo;
    txTo.vin.resize(1);
    txTo.vout.resize(1);
    txTo.vin[0].prevout.n = 0;
    txTo.vin[0].prevout.hash = txFrom.GetHash();
    txTo.vin[0].scriptSig = scriptSig;
    txTo.vout[0].nValue = 1;

    return VerifyScript(scriptSig, scriptPubKey, nullptr, fStrict ? SCRIPT_VERIFY_P2SH : SCRIPT_VERIFY_NONE, MutableTransactionSignatureChecker(&txTo, 0, txFrom.vout[0].nValue), &err);
}


BOOST_FIXTURE_TEST_SUITE(script_p2sh_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(sign)
{
    LOCK(cs_main);
//付费脚本哈希如下所示：
//脚本sig:<sig><sig…><serialized_script>
//scriptPubKey:hash160<hash>equal

//test signsignature（）（因此是solver（）的版本，它对事务进行签名）
    CBasicKeyStore keystore;
    CKey key[4];
    for (int i = 0; i < 4; i++)
    {
        key[i].MakeNewKey(true);
        BOOST_CHECK(keystore.AddKey(key[i]));
    }

//8脚本：检查
//不同的键，直/p2sh，pubkey/pubkeyhash
    CScript standardScripts[4];
    standardScripts[0] << ToByteVector(key[0].GetPubKey()) << OP_CHECKSIG;
    standardScripts[1] = GetScriptForDestination(key[1].GetPubKey().GetID());
    standardScripts[2] << ToByteVector(key[1].GetPubKey()) << OP_CHECKSIG;
    standardScripts[3] = GetScriptForDestination(key[2].GetPubKey().GetID());
    CScript evalScripts[4];
    for (int i = 0; i < 4; i++)
    {
        BOOST_CHECK(keystore.AddCScript(standardScripts[i]));
        evalScripts[i] = GetScriptForDestination(CScriptID(standardScripts[i]));
    }

CMutableTransaction txFrom;  //融资交易：
    std::string reason;
    txFrom.vout.resize(8);
    for (int i = 0; i < 4; i++)
    {
        txFrom.vout[i].scriptPubKey = evalScripts[i];
        txFrom.vout[i].nValue = COIN;
        txFrom.vout[i+4].scriptPubKey = standardScripts[i];
        txFrom.vout[i+4].nValue = COIN;
    }
    BOOST_CHECK(IsStandardTx(CTransaction(txFrom), reason));

CMutableTransaction txTo[8]; //支出交易
    for (int i = 0; i < 8; i++)
    {
        txTo[i].vin.resize(1);
        txTo[i].vout.resize(1);
        txTo[i].vin[0].prevout.n = i;
        txTo[i].vin[0].prevout.hash = txFrom.GetHash();
        txTo[i].vout[0].nValue = 1;
        BOOST_CHECK_MESSAGE(IsMine(keystore, txFrom.vout[i].scriptPubKey), strprintf("IsMine %d", i));
    }
    for (int i = 0; i < 8; i++)
    {
        BOOST_CHECK_MESSAGE(SignSignature(keystore, CTransaction(txFrom), txTo[i], 0, SIGHASH_ALL), strprintf("SignSignature %d", i));
    }
//以上都应该是正常的，txto有有效的签名
//如果使用错误的scriptsig，请检查以确保签名验证失败：
    for (int i = 0; i < 8; i++) {
        PrecomputedTransactionData txdata(txTo[i]);
        for (int j = 0; j < 8; j++)
        {
            CScript sigSave = txTo[i].vin[0].scriptSig;
            txTo[i].vin[0].scriptSig = txTo[j].vin[0].scriptSig;
            bool sigOK = CScriptCheck(txFrom.vout[txTo[i].vin[0].prevout.n], CTransaction(txTo[i]), 0, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC, false, &txdata)();
            if (i == j)
                BOOST_CHECK_MESSAGE(sigOK, strprintf("VerifySignature %d %d", i, j));
            else
                BOOST_CHECK_MESSAGE(!sigOK, strprintf("VerifySignature %d %d", i, j));
            txTo[i].vin[0].scriptSig = sigSave;
        }
    }
}

BOOST_AUTO_TEST_CASE(norecurse)
{
    ScriptError err;
//确保只有外部付费脚本哈希
//额外验证内容：
    CScript invalidAsScript;
    invalidAsScript << OP_INVALIDOPCODE << OP_INVALIDOPCODE;

    CScript p2sh = GetScriptForDestination(CScriptID(invalidAsScript));

    CScript scriptSig;
    scriptSig << Serialize(invalidAsScript);

//不应验证，因为它将尝试执行opu invalidopcode
    BOOST_CHECK(!Verify(scriptSig, p2sh, true, err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_BAD_OPCODE, ScriptErrorString(err));

//尝试重复，验证应该成功，因为
//内部hash160<>等于只应检查hash：
    CScript p2sh2 = GetScriptForDestination(CScriptID(p2sh));
    CScript scriptSig2;
    scriptSig2 << Serialize(invalidAsScript) << Serialize(p2sh);

    BOOST_CHECK(Verify(scriptSig2, p2sh2, true, err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));
}

BOOST_AUTO_TEST_CASE(set)
{
    LOCK(cs_main);
//测试cscript:：set*方法
    CBasicKeyStore keystore;
    CKey key[4];
    std::vector<CPubKey> keys;
    for (int i = 0; i < 4; i++)
    {
        key[i].MakeNewKey(true);
        BOOST_CHECK(keystore.AddKey(key[i]));
        keys.push_back(key[i].GetPubKey());
    }

    CScript inner[4];
    inner[0] = GetScriptForDestination(key[0].GetPubKey().GetID());
    inner[1] = GetScriptForMultisig(2, std::vector<CPubKey>(keys.begin(), keys.begin()+2));
    inner[2] = GetScriptForMultisig(1, std::vector<CPubKey>(keys.begin(), keys.begin()+2));
    inner[3] = GetScriptForMultisig(2, std::vector<CPubKey>(keys.begin(), keys.begin()+3));

    CScript outer[4];
    for (int i = 0; i < 4; i++)
    {
        outer[i] = GetScriptForDestination(CScriptID(inner[i]));
        BOOST_CHECK(keystore.AddCScript(inner[i]));
    }

CMutableTransaction txFrom;  //融资交易：
    std::string reason;
    txFrom.vout.resize(4);
    for (int i = 0; i < 4; i++)
    {
        txFrom.vout[i].scriptPubKey = outer[i];
        txFrom.vout[i].nValue = CENT;
    }
    BOOST_CHECK(IsStandardTx(CTransaction(txFrom), reason));

CMutableTransaction txTo[4]; //支出交易
    for (int i = 0; i < 4; i++)
    {
        txTo[i].vin.resize(1);
        txTo[i].vout.resize(1);
        txTo[i].vin[0].prevout.n = i;
        txTo[i].vin[0].prevout.hash = txFrom.GetHash();
        txTo[i].vout[0].nValue = 1*CENT;
        txTo[i].vout[0].scriptPubKey = inner[i];
        BOOST_CHECK_MESSAGE(IsMine(keystore, txFrom.vout[i].scriptPubKey), strprintf("IsMine %d", i));
    }
    for (int i = 0; i < 4; i++)
    {
        BOOST_CHECK_MESSAGE(SignSignature(keystore, CTransaction(txFrom), txTo[i], 0, SIGHASH_ALL), strprintf("SignSignature %d", i));
        BOOST_CHECK_MESSAGE(IsStandardTx(CTransaction(txTo[i]), reason), strprintf("txTo[%d].IsStandard", i));
    }
}

BOOST_AUTO_TEST_CASE(is)
{
//测试cscript:：isPayToScriptHash（）。
    uint160 dummy;
    CScript p2sh;
    p2sh << OP_HASH160 << ToByteVector(dummy) << OP_EQUAL;
    BOOST_CHECK(p2sh.IsPayToScriptHash());

//如果使用其中一个ophu pushdata操作码，则不视为付费脚本哈希：
    std::vector<unsigned char> direct = {OP_HASH160, 20};
    direct.insert(direct.end(), 20, 0);
    direct.push_back(OP_EQUAL);
    BOOST_CHECK(CScript(direct.begin(), direct.end()).IsPayToScriptHash());
    std::vector<unsigned char> pushdata1 = {OP_HASH160, OP_PUSHDATA1, 20};
    pushdata1.insert(pushdata1.end(), 20, 0);
    pushdata1.push_back(OP_EQUAL);
    BOOST_CHECK(!CScript(pushdata1.begin(), pushdata1.end()).IsPayToScriptHash());
    std::vector<unsigned char> pushdata2 = {OP_HASH160, 20, 0};
    pushdata2.insert(pushdata2.end(), 20, 0);
    pushdata2.push_back(OP_EQUAL);
    BOOST_CHECK(!CScript(pushdata2.begin(), pushdata2.end()).IsPayToScriptHash());
    std::vector<unsigned char> pushdata4 = {OP_HASH160, 20, 0, 0, 0};
    pushdata4.insert(pushdata4.end(), 20, 0);
    pushdata4.push_back(OP_EQUAL);
    BOOST_CHECK(!CScript(pushdata4.begin(), pushdata4.end()).IsPayToScriptHash());

    CScript not_p2sh;
    BOOST_CHECK(!not_p2sh.IsPayToScriptHash());

    not_p2sh.clear(); not_p2sh << OP_HASH160 << ToByteVector(dummy) << ToByteVector(dummy) << OP_EQUAL;
    BOOST_CHECK(!not_p2sh.IsPayToScriptHash());

    not_p2sh.clear(); not_p2sh << OP_NOP << ToByteVector(dummy) << OP_EQUAL;
    BOOST_CHECK(!not_p2sh.IsPayToScriptHash());

    not_p2sh.clear(); not_p2sh << OP_HASH160 << ToByteVector(dummy) << OP_CHECKSIG;
    BOOST_CHECK(!not_p2sh.IsPayToScriptHash());
}

BOOST_AUTO_TEST_CASE(switchover)
{
//测试切换代码
    CScript notValid;
    ScriptError err;
    notValid << OP_11 << OP_12 << OP_EQUALVERIFY;
    CScript scriptSig;
    scriptSig << Serialize(notValid);

    CScript fund = GetScriptForDestination(CScriptID(notValid));


//在旧规则下验证应成功（哈希正确）：
    BOOST_CHECK(Verify(scriptSig, fund, false, err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));
//新失败：
    BOOST_CHECK(!Verify(scriptSig, fund, true, err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EQUALVERIFY, ScriptErrorString(err));
}

BOOST_AUTO_TEST_CASE(AreInputsStandard)
{
    LOCK(cs_main);
    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    CBasicKeyStore keystore;
    CKey key[6];
    std::vector<CPubKey> keys;
    for (int i = 0; i < 6; i++)
    {
        key[i].MakeNewKey(true);
        BOOST_CHECK(keystore.AddKey(key[i]));
    }
    for (int i = 0; i < 3; i++)
        keys.push_back(key[i].GetPubKey());

    CMutableTransaction txFrom;
    txFrom.vout.resize(7);

//前三个是标准的：
    CScript pay1 = GetScriptForDestination(key[0].GetPubKey().GetID());
    BOOST_CHECK(keystore.AddCScript(pay1));
    CScript pay1of3 = GetScriptForMultisig(1, keys);

txFrom.vout[0].scriptPubKey = GetScriptForDestination(CScriptID(pay1)); //p2sh（操作检查信号）
    txFrom.vout[0].nValue = 1000;
txFrom.vout[1].scriptPubKey = pay1; //普通支票
    txFrom.vout[1].nValue = 2000;
txFrom.vout[2].scriptPubKey = pay1of3; //普通支票多张
    txFrom.vout[2].nValue = 3000;

//VOUT[3]是复杂的三分之一和三分之二
//…如果用p2sh包装就可以了：
    CScript oneAndTwo;
    oneAndTwo << OP_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey());
    oneAndTwo << OP_3 << OP_CHECKMULTISIGVERIFY;
    oneAndTwo << OP_2 << ToByteVector(key[3].GetPubKey()) << ToByteVector(key[4].GetPubKey()) << ToByteVector(key[5].GetPubKey());
    oneAndTwo << OP_3 << OP_CHECKMULTISIG;
    BOOST_CHECK(keystore.AddCScript(oneAndTwo));
    txFrom.vout[3].scriptPubKey = GetScriptForDestination(CScriptID(oneAndTwo));
    txFrom.vout[3].nValue = 4000;

//vout[4]是max sigops:
    CScript fifteenSigops; fifteenSigops << OP_1;
    for (unsigned i = 0; i < MAX_P2SH_SIGOPS; i++)
        fifteenSigops << ToByteVector(key[i%3].GetPubKey());
    fifteenSigops << OP_15 << OP_CHECKMULTISIG;
    BOOST_CHECK(keystore.AddCScript(fifteenSigops));
    txFrom.vout[4].scriptPubKey = GetScriptForDestination(CScriptID(fifteenSigops));
    txFrom.vout[4].nValue = 5000;

//VOUT[5/6]是非标准的，因为它们超过了最大值
    CScript sixteenSigops; sixteenSigops << OP_16 << OP_CHECKMULTISIG;
    BOOST_CHECK(keystore.AddCScript(sixteenSigops));
    txFrom.vout[5].scriptPubKey = GetScriptForDestination(CScriptID(sixteenSigops));
    txFrom.vout[5].nValue = 5000;
    CScript twentySigops; twentySigops << OP_CHECKMULTISIG;
    BOOST_CHECK(keystore.AddCScript(twentySigops));
    txFrom.vout[6].scriptPubKey = GetScriptForDestination(CScriptID(twentySigops));
    txFrom.vout[6].nValue = 6000;

    AddCoins(coins, CTransaction(txFrom), 0);

    CMutableTransaction txTo;
    txTo.vout.resize(1);
    txTo.vout[0].scriptPubKey = GetScriptForDestination(key[1].GetPubKey().GetID());

    txTo.vin.resize(5);
    for (int i = 0; i < 5; i++)
    {
        txTo.vin[i].prevout.n = i;
        txTo.vin[i].prevout.hash = txFrom.GetHash();
    }
    BOOST_CHECK(SignSignature(keystore, CTransaction(txFrom), txTo, 0, SIGHASH_ALL));
    BOOST_CHECK(SignSignature(keystore, CTransaction(txFrom), txTo, 1, SIGHASH_ALL));
    BOOST_CHECK(SignSignature(keystore, CTransaction(txFrom), txTo, 2, SIGHASH_ALL));
//签名签名不知道如何签名。我们是
//不测试验证签名，因此只需创建
//包含正确p2sh脚本的虚拟签名：
    txTo.vin[3].scriptSig << OP_11 << OP_11 << std::vector<unsigned char>(oneAndTwo.begin(), oneAndTwo.end());
    txTo.vin[4].scriptSig << std::vector<unsigned char>(fifteenSigops.begin(), fifteenSigops.end());

    BOOST_CHECK(::AreInputsStandard(CTransaction(txTo), coins));
//22个P2SH信号用于所有输入（1用于VIN[0]，6用于VIN[3]，15用于VIN[4]
    BOOST_CHECK_EQUAL(GetP2SHSigOpCount(CTransaction(txTo), coins), 22U);

    CMutableTransaction txToNonStd1;
    txToNonStd1.vout.resize(1);
    txToNonStd1.vout[0].scriptPubKey = GetScriptForDestination(key[1].GetPubKey().GetID());
    txToNonStd1.vout[0].nValue = 1000;
    txToNonStd1.vin.resize(1);
    txToNonStd1.vin[0].prevout.n = 5;
    txToNonStd1.vin[0].prevout.hash = txFrom.GetHash();
    txToNonStd1.vin[0].scriptSig << std::vector<unsigned char>(sixteenSigops.begin(), sixteenSigops.end());

    BOOST_CHECK(!::AreInputsStandard(CTransaction(txToNonStd1), coins));
    BOOST_CHECK_EQUAL(GetP2SHSigOpCount(CTransaction(txToNonStd1), coins), 16U);

    CMutableTransaction txToNonStd2;
    txToNonStd2.vout.resize(1);
    txToNonStd2.vout[0].scriptPubKey = GetScriptForDestination(key[1].GetPubKey().GetID());
    txToNonStd2.vout[0].nValue = 1000;
    txToNonStd2.vin.resize(1);
    txToNonStd2.vin[0].prevout.n = 6;
    txToNonStd2.vin[0].prevout.hash = txFrom.GetHash();
    txToNonStd2.vin[0].scriptSig << std::vector<unsigned char>(twentySigops.begin(), twentySigops.end());

    BOOST_CHECK(!::AreInputsStandard(CTransaction(txToNonStd2), coins));
    BOOST_CHECK_EQUAL(GetP2SHSigOpCount(CTransaction(txToNonStd2), coins), 20U);
}

BOOST_AUTO_TEST_SUITE_END()
