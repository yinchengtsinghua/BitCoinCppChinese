
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2017-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <key.h>
#include <keystore.h>
#include <script/ismine.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/standard.h>
#include <test/test_bitcoin.h>

#include <boost/test/unit_test.hpp>


BOOST_FIXTURE_TEST_SUITE(script_standard_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(script_standard_Solver_success)
{
    CKey keys[3];
    CPubKey pubkeys[3];
    for (int i = 0; i < 3; i++) {
        keys[i].MakeNewKey(true);
        pubkeys[i] = keys[i].GetPubKey();
    }

    CScript s;
    std::vector<std::vector<unsigned char> > solutions;

//TXP-PUBKEY
    s.clear();
    s << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_PUBKEY);
    BOOST_CHECK_EQUAL(solutions.size(), 1U);
    BOOST_CHECK(solutions[0] == ToByteVector(pubkeys[0]));

//TXP- PUBKEHASH
    s.clear();
    s << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_PUBKEYHASH);
    BOOST_CHECK_EQUAL(solutions.size(), 1U);
    BOOST_CHECK(solutions[0] == ToByteVector(pubkeys[0].GetID()));

//塔克西斯塔什
CScript redeemScript(s); //用剩余的p2pkh脚本初始化
    s.clear();
    s << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_SCRIPTHASH);
    BOOST_CHECK_EQUAL(solutions.size(), 1U);
    BOOST_CHECK(solutions[0] == ToByteVector(CScriptID(redeemScript)));

//TX-Mulsisig
    s.clear();
    s << OP_1 <<
        ToByteVector(pubkeys[0]) <<
        ToByteVector(pubkeys[1]) <<
        OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_MULTISIG);
    BOOST_CHECK_EQUAL(solutions.size(), 4U);
    BOOST_CHECK(solutions[0] == std::vector<unsigned char>({1}));
    BOOST_CHECK(solutions[1] == ToByteVector(pubkeys[0]));
    BOOST_CHECK(solutions[2] == ToByteVector(pubkeys[1]));
    BOOST_CHECK(solutions[3] == std::vector<unsigned char>({2}));

    s.clear();
    s << OP_2 <<
        ToByteVector(pubkeys[0]) <<
        ToByteVector(pubkeys[1]) <<
        ToByteVector(pubkeys[2]) <<
        OP_3 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_MULTISIG);
    BOOST_CHECK_EQUAL(solutions.size(), 5U);
    BOOST_CHECK(solutions[0] == std::vector<unsigned char>({2}));
    BOOST_CHECK(solutions[1] == ToByteVector(pubkeys[0]));
    BOOST_CHECK(solutions[2] == ToByteVector(pubkeys[1]));
    BOOST_CHECK(solutions[3] == ToByteVector(pubkeys[2]));
    BOOST_CHECK(solutions[4] == std::vector<unsigned char>({3}));

//Tx~（null）数据
    s.clear();
    s << OP_RETURN <<
        std::vector<unsigned char>({0}) <<
        std::vector<unsigned char>({75}) <<
        std::vector<unsigned char>({255});
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_NULL_DATA);
    BOOST_CHECK_EQUAL(solutions.size(), 0U);

//Tx_见证人_v0_keyhash
    s.clear();
    s << OP_0 << ToByteVector(pubkeys[0].GetID());
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_WITNESS_V0_KEYHASH);
    BOOST_CHECK_EQUAL(solutions.size(), 1U);
    BOOST_CHECK(solutions[0] == ToByteVector(pubkeys[0].GetID()));

//Tx见证人脚本哈希
    uint256 scriptHash;
    CSHA256().Write(&redeemScript[0], redeemScript.size())
        .Finalize(scriptHash.begin());

    s.clear();
    s << OP_0 << ToByteVector(scriptHash);
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_WITNESS_V0_SCRIPTHASH);
    BOOST_CHECK_EQUAL(solutions.size(), 1U);
    BOOST_CHECK(solutions[0] == ToByteVector(scriptHash));

//TXX非标
    s.clear();
    s << OP_9 << OP_ADD << OP_11 << OP_EQUAL;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_NONSTANDARD);
}

BOOST_AUTO_TEST_CASE(script_standard_Solver_failure)
{
    CKey key;
    CPubKey pubkey;
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    CScript s;
    std::vector<std::vector<unsigned char> > solutions;

//tx_pubkey的pubkey大小不正确
    s.clear();
    s << std::vector<unsigned char>(30, 0x01) << OP_CHECKSIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_NONSTANDARD);

//Tx_PubKeyHash的密钥哈希大小不正确
    s.clear();
    s << OP_DUP << OP_HASH160 << ToByteVector(pubkey) << OP_EQUALVERIFY << OP_CHECKSIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_NONSTANDARD);

//Tx脚本哈希大小不正确
    s.clear();
    s << OP_HASH160 << std::vector<unsigned char>(21, 0x01) << OP_EQUAL;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_NONSTANDARD);

//TXXMulsig 0/2
    s.clear();
    s << OP_0 << ToByteVector(pubkey) << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_NONSTANDARD);

//TXXMulsig 2/1
    s.clear();
    s << OP_2 << ToByteVector(pubkey) << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_NONSTANDARD);

//tx_multisig n=2，带1个pubkey
    s.clear();
    s << OP_1 << ToByteVector(pubkey) << OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_NONSTANDARD);

//tx_multisig n=1，带0个公钥
    s.clear();
    s << OP_1 << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_NONSTANDARD);

//与其他操作码的Tx_空数据
    s.clear();
    s << OP_RETURN << std::vector<unsigned char>({75}) << OP_ADD;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_NONSTANDARD);

//Tx_见证程序大小不正确
    s.clear();
    s << OP_0 << std::vector<unsigned char>(19, 0x01);
    BOOST_CHECK_EQUAL(Solver(s, solutions), TX_NONSTANDARD);
}

BOOST_AUTO_TEST_CASE(script_standard_ExtractDestination)
{
    CKey key;
    CPubKey pubkey;
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    CScript s;
    CTxDestination address;

//TXP-PUBKEY
    s.clear();
    s << ToByteVector(pubkey) << OP_CHECKSIG;
    BOOST_CHECK(ExtractDestination(s, address));
    BOOST_CHECK(boost::get<CKeyID>(&address) &&
                *boost::get<CKeyID>(&address) == pubkey.GetID());

//TXP- PUBKEHASH
    s.clear();
    s << OP_DUP << OP_HASH160 << ToByteVector(pubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
    BOOST_CHECK(ExtractDestination(s, address));
    BOOST_CHECK(boost::get<CKeyID>(&address) &&
                *boost::get<CKeyID>(&address) == pubkey.GetID());

//塔克西斯塔什
CScript redeemScript(s); //用剩余的p2pkh脚本初始化
    s.clear();
    s << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
    BOOST_CHECK(ExtractDestination(s, address));
    BOOST_CHECK(boost::get<CScriptID>(&address) &&
                *boost::get<CScriptID>(&address) == CScriptID(redeemScript));

//TX-Mulsisig
    s.clear();
    s << OP_1 << ToByteVector(pubkey) << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK(!ExtractDestination(s, address));

//Tx~（null）数据
    s.clear();
    s << OP_RETURN << std::vector<unsigned char>({75});
    BOOST_CHECK(!ExtractDestination(s, address));

//Tx_见证人_v0_keyhash
    s.clear();
    s << OP_0 << ToByteVector(pubkey.GetID());
    BOOST_CHECK(ExtractDestination(s, address));
    WitnessV0KeyHash keyhash;
    CHash160().Write(pubkey.begin(), pubkey.size()).Finalize(keyhash.begin());
    BOOST_CHECK(boost::get<WitnessV0KeyHash>(&address) && *boost::get<WitnessV0KeyHash>(&address) == keyhash);

//Tx见证人脚本哈希
    s.clear();
    WitnessV0ScriptHash scripthash;
    CSHA256().Write(redeemScript.data(), redeemScript.size()).Finalize(scripthash.begin());
    s << OP_0 << ToByteVector(scripthash);
    BOOST_CHECK(ExtractDestination(s, address));
    BOOST_CHECK(boost::get<WitnessV0ScriptHash>(&address) && *boost::get<WitnessV0ScriptHash>(&address) == scripthash);

//未知版本的Tx_见证人
    s.clear();
    s << OP_1 << ToByteVector(pubkey);
    BOOST_CHECK(ExtractDestination(s, address));
    WitnessUnknown unk;
    unk.length = 33;
    unk.version = 1;
    std::copy(pubkey.begin(), pubkey.end(), unk.program);
    BOOST_CHECK(boost::get<WitnessUnknown>(&address) && *boost::get<WitnessUnknown>(&address) == unk);
}

BOOST_AUTO_TEST_CASE(script_standard_ExtractDestinations)
{
    CKey keys[3];
    CPubKey pubkeys[3];
    for (int i = 0; i < 3; i++) {
        keys[i].MakeNewKey(true);
        pubkeys[i] = keys[i].GetPubKey();
    }

    CScript s;
    txnouttype whichType;
    std::vector<CTxDestination> addresses;
    int nRequired;

//TXP-PUBKEY
    s.clear();
    s << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
    BOOST_CHECK(ExtractDestinations(s, whichType, addresses, nRequired));
    BOOST_CHECK_EQUAL(whichType, TX_PUBKEY);
    BOOST_CHECK_EQUAL(addresses.size(), 1U);
    BOOST_CHECK_EQUAL(nRequired, 1);
    BOOST_CHECK(boost::get<CKeyID>(&addresses[0]) &&
                *boost::get<CKeyID>(&addresses[0]) == pubkeys[0].GetID());

//TXP- PUBKEHASH
    s.clear();
    s << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
    BOOST_CHECK(ExtractDestinations(s, whichType, addresses, nRequired));
    BOOST_CHECK_EQUAL(whichType, TX_PUBKEYHASH);
    BOOST_CHECK_EQUAL(addresses.size(), 1U);
    BOOST_CHECK_EQUAL(nRequired, 1);
    BOOST_CHECK(boost::get<CKeyID>(&addresses[0]) &&
                *boost::get<CKeyID>(&addresses[0]) == pubkeys[0].GetID());

//塔克西斯塔什
CScript redeemScript(s); //用剩余的p2pkh脚本初始化
    s.clear();
    s << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
    BOOST_CHECK(ExtractDestinations(s, whichType, addresses, nRequired));
    BOOST_CHECK_EQUAL(whichType, TX_SCRIPTHASH);
    BOOST_CHECK_EQUAL(addresses.size(), 1U);
    BOOST_CHECK_EQUAL(nRequired, 1);
    BOOST_CHECK(boost::get<CScriptID>(&addresses[0]) &&
                *boost::get<CScriptID>(&addresses[0]) == CScriptID(redeemScript));

//TX-Mulsisig
    s.clear();
    s << OP_2 <<
        ToByteVector(pubkeys[0]) <<
        ToByteVector(pubkeys[1]) <<
        OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK(ExtractDestinations(s, whichType, addresses, nRequired));
    BOOST_CHECK_EQUAL(whichType, TX_MULTISIG);
    BOOST_CHECK_EQUAL(addresses.size(), 2U);
    BOOST_CHECK_EQUAL(nRequired, 2);
    BOOST_CHECK(boost::get<CKeyID>(&addresses[0]) &&
                *boost::get<CKeyID>(&addresses[0]) == pubkeys[0].GetID());
    BOOST_CHECK(boost::get<CKeyID>(&addresses[1]) &&
                *boost::get<CKeyID>(&addresses[1]) == pubkeys[1].GetID());

//Tx~（null）数据
    s.clear();
    s << OP_RETURN << std::vector<unsigned char>({75});
    BOOST_CHECK(!ExtractDestinations(s, whichType, addresses, nRequired));
}

BOOST_AUTO_TEST_CASE(script_standard_GetScriptFor_)
{
    CKey keys[3];
    CPubKey pubkeys[3];
    for (int i = 0; i < 3; i++) {
        keys[i].MakeNewKey(true);
        pubkeys[i] = keys[i].GetPubKey();
    }

    CScript expected, result;

//克基
    expected.clear();
    expected << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
    result = GetScriptForDestination(pubkeys[0].GetID());
    BOOST_CHECK(result == expected);

//克斯皮蒂德
    CScript redeemScript(result);
    expected.clear();
    expected << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
    result = GetScriptForDestination(CScriptID(redeemScript));
    BOOST_CHECK(result == expected);

//中心目的地
    expected.clear();
    result = GetScriptForDestination(CNoDestination());
    BOOST_CHECK(result == expected);

//获取scriptforrawpubkey
    expected.clear();
    expected << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
    result = GetScriptForRawPubKey(pubkeys[0]);
    BOOST_CHECK(result == expected);

//获取脚本多信号
    expected.clear();
    expected << OP_2 <<
        ToByteVector(pubkeys[0]) <<
        ToByteVector(pubkeys[1]) <<
        ToByteVector(pubkeys[2]) <<
        OP_3 << OP_CHECKMULTISIG;
    result = GetScriptForMultisig(2, std::vector<CPubKey>(pubkeys, pubkeys + 3));
    BOOST_CHECK(result == expected);

//获取脚本见证
    CScript witnessScript;

    witnessScript << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
    expected.clear();
    expected << OP_0 << ToByteVector(pubkeys[0].GetID());
    result = GetScriptForWitness(witnessScript);
    BOOST_CHECK(result == expected);

    witnessScript.clear();
    witnessScript << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
    result = GetScriptForWitness(witnessScript);
    BOOST_CHECK(result == expected);

    witnessScript.clear();
    witnessScript << OP_1 << ToByteVector(pubkeys[0]) << OP_1 << OP_CHECKMULTISIG;

    uint256 scriptHash;
    CSHA256().Write(&witnessScript[0], witnessScript.size())
        .Finalize(scriptHash.begin());

    expected.clear();
    expected << OP_0 << ToByteVector(scriptHash);
    result = GetScriptForWitness(witnessScript);
    BOOST_CHECK(result == expected);
}

BOOST_AUTO_TEST_CASE(script_standard_IsMine)
{
    CKey keys[2];
    CPubKey pubkeys[2];
    for (int i = 0; i < 2; i++) {
        keys[i].MakeNewKey(true);
        pubkeys[i] = keys[i].GetPubKey();
    }

    CKey uncompressedKey;
    uncompressedKey.MakeNewKey(false);
    CPubKey uncompressedPubkey = uncompressedKey.GetPubKey();

    CScript scriptPubKey;
    isminetype result;

//压缩2PK
    {
        CBasicKeyStore keystore;
        scriptPubKey = GetScriptForRawPubKey(pubkeys[0]);

//密钥库没有密钥
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//密钥库有密钥
        BOOST_CHECK(keystore.AddKey(keys[0]));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
    }

//p2pk未压缩
    {
        CBasicKeyStore keystore;
        scriptPubKey = GetScriptForRawPubKey(uncompressedPubkey);

//密钥库没有密钥
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//密钥库有密钥
        BOOST_CHECK(keystore.AddKey(uncompressedKey));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
    }

//p2pkh压缩
    {
        CBasicKeyStore keystore;
        scriptPubKey = GetScriptForDestination(pubkeys[0].GetID());

//密钥库没有密钥
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//密钥库有密钥
        BOOST_CHECK(keystore.AddKey(keys[0]));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
    }

//p2pkh未压缩
    {
        CBasicKeyStore keystore;
        scriptPubKey = GetScriptForDestination(uncompressedPubkey.GetID());

//密钥库没有密钥
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//密钥库有密钥
        BOOST_CHECK(keystore.AddKey(uncompressedKey));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
    }

//2SH
    {
        CBasicKeyStore keystore;

        CScript redeemScript = GetScriptForDestination(pubkeys[0].GetID());
        scriptPubKey = GetScriptForDestination(CScriptID(redeemScript));

//keystore没有redeemscript或key
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//keystore有redeemscript，但没有密钥
        BOOST_CHECK(keystore.AddCScript(redeemScript));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//keystore具有redeemscript和key
        BOOST_CHECK(keystore.AddKey(keys[0]));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
    }

//（内部p2pkh）p2sh内部p2sh（无效）
    {
        CBasicKeyStore keystore;

        CScript redeemscript_inner = GetScriptForDestination(pubkeys[0].GetID());
        CScript redeemscript = GetScriptForDestination(CScriptID(redeemscript_inner));
        scriptPubKey = GetScriptForDestination(CScriptID(redeemscript));

        BOOST_CHECK(keystore.AddCScript(redeemscript));
        BOOST_CHECK(keystore.AddCScript(redeemscript_inner));
        BOOST_CHECK(keystore.AddCScript(scriptPubKey));
        BOOST_CHECK(keystore.AddKey(keys[0]));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }

//（p2pkh内部）p2sh内部p2wsh（无效）
    {
        CBasicKeyStore keystore;

        CScript redeemscript = GetScriptForDestination(pubkeys[0].GetID());
        CScript witnessscript = GetScriptForDestination(CScriptID(redeemscript));
        scriptPubKey = GetScriptForDestination(WitnessV0ScriptHash(witnessscript));

        BOOST_CHECK(keystore.AddCScript(witnessscript));
        BOOST_CHECK(keystore.AddCScript(redeemscript));
        BOOST_CHECK(keystore.AddCScript(scriptPubKey));
        BOOST_CHECK(keystore.AddKey(keys[0]));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }

//p2wpkh内部p2wsh（无效）
    {
        CBasicKeyStore keystore;

        CScript witnessscript = GetScriptForDestination(WitnessV0KeyHash(pubkeys[0].GetID()));
        scriptPubKey = GetScriptForDestination(WitnessV0ScriptHash(witnessscript));

        BOOST_CHECK(keystore.AddCScript(witnessscript));
        BOOST_CHECK(keystore.AddCScript(scriptPubKey));
        BOOST_CHECK(keystore.AddKey(keys[0]));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }

//（内部p2pkh）p2wsh内部p2wsh（无效）
    {
        CBasicKeyStore keystore;

        CScript witnessscript_inner = GetScriptForDestination(pubkeys[0].GetID());
        CScript witnessscript = GetScriptForDestination(WitnessV0ScriptHash(witnessscript_inner));
        scriptPubKey = GetScriptForDestination(WitnessV0ScriptHash(witnessscript));

        BOOST_CHECK(keystore.AddCScript(witnessscript_inner));
        BOOST_CHECK(keystore.AddCScript(witnessscript));
        BOOST_CHECK(keystore.AddCScript(scriptPubKey));
        BOOST_CHECK(keystore.AddKey(keys[0]));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }

//p2wpkh压缩
    {
        CBasicKeyStore keystore;
        BOOST_CHECK(keystore.AddKey(keys[0]));

        scriptPubKey = GetScriptForDestination(WitnessV0KeyHash(pubkeys[0].GetID()));

//keystore隐式包含key和p2sh redeemscript
        BOOST_CHECK(keystore.AddCScript(scriptPubKey));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
    }

//p2wpkh未压缩
    {
        CBasicKeyStore keystore;
        BOOST_CHECK(keystore.AddKey(uncompressedKey));

        scriptPubKey = GetScriptForDestination(WitnessV0KeyHash(uncompressedPubkey.GetID()));

//密钥库有密钥，但没有p2sh redeemscript
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//keystore有key和p2sh redeemscript
        BOOST_CHECK(keystore.AddCScript(scriptPubKey));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }

//脚本pubkey multisig
    {
        CBasicKeyStore keystore;

        scriptPubKey = GetScriptForMultisig(2, {uncompressedPubkey, pubkeys[1]});

//密钥库没有任何密钥
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//密钥库有1/2个密钥
        BOOST_CHECK(keystore.AddKey(uncompressedKey));

        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//密钥库有2/2个密钥
        BOOST_CHECK(keystore.AddKey(keys[1]));

        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//keystore有2/2个键和脚本
        BOOST_CHECK(keystore.AddCScript(scriptPubKey));

        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }

//2SH多硅
    {
        CBasicKeyStore keystore;
        BOOST_CHECK(keystore.AddKey(uncompressedKey));
        BOOST_CHECK(keystore.AddKey(keys[1]));

        CScript redeemScript = GetScriptForMultisig(2, {uncompressedPubkey, pubkeys[1]});
        scriptPubKey = GetScriptForDestination(CScriptID(redeemScript));

//keystore没有redeemscript
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//keystore有redeemscript
        BOOST_CHECK(keystore.AddCScript(redeemScript));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
    }

//p2wsh带压缩键的multisig
    {
        CBasicKeyStore keystore;
        BOOST_CHECK(keystore.AddKey(keys[0]));
        BOOST_CHECK(keystore.AddKey(keys[1]));

        CScript witnessScript = GetScriptForMultisig(2, {pubkeys[0], pubkeys[1]});
        scriptPubKey = GetScriptForDestination(WitnessV0ScriptHash(witnessScript));

//keystore有密钥，但没有witnessscript或p2sh redeemscript
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//keystore有keys和witnessscript，但没有p2sh redeemscript
        BOOST_CHECK(keystore.AddCScript(witnessScript));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//keystore有keys、witnessscript、p2sh redeemscript
        BOOST_CHECK(keystore.AddCScript(scriptPubKey));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
    }

//带未压缩密钥的p2wsh multisig
    {
        CBasicKeyStore keystore;
        BOOST_CHECK(keystore.AddKey(uncompressedKey));
        BOOST_CHECK(keystore.AddKey(keys[1]));

        CScript witnessScript = GetScriptForMultisig(2, {uncompressedPubkey, pubkeys[1]});
        scriptPubKey = GetScriptForDestination(WitnessV0ScriptHash(witnessScript));

//keystore有密钥，但没有witnessscript或p2sh redeemscript
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//keystore有keys和witnessscript，但没有p2sh redeemscript
        BOOST_CHECK(keystore.AddCScript(witnessScript));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//keystore有keys、witnessscript、p2sh redeemscript
        BOOST_CHECK(keystore.AddCScript(scriptPubKey));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }

//p2wsh multisig包裹在p2sh中
    {
        CBasicKeyStore keystore;

        CScript witnessScript = GetScriptForMultisig(2, {pubkeys[0], pubkeys[1]});
        CScript redeemScript = GetScriptForDestination(WitnessV0ScriptHash(witnessScript));
        scriptPubKey = GetScriptForDestination(CScriptID(redeemScript));

//keystore没有witnessscript、p2sh redeemscript或keys
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//keystore有witnessscript和p2sh redeemscript，但没有密钥
        BOOST_CHECK(keystore.AddCScript(redeemScript));
        BOOST_CHECK(keystore.AddCScript(witnessScript));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

//keystore有keys、witnessscript、p2sh redeemscript
        BOOST_CHECK(keystore.AddKey(keys[0]));
        BOOST_CHECK(keystore.AddKey(keys[1]));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
    }

//opr返回
    {
        CBasicKeyStore keystore;
        BOOST_CHECK(keystore.AddKey(keys[0]));

        scriptPubKey.clear();
        scriptPubKey << OP_RETURN << ToByteVector(pubkeys[0]);

        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }

//证人不可依赖
    {
        CBasicKeyStore keystore;
        BOOST_CHECK(keystore.AddKey(keys[0]));

        scriptPubKey.clear();
        scriptPubKey << OP_0 << ToByteVector(ParseHex("aabb"));

        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }

//证人未知
    {
        CBasicKeyStore keystore;
        BOOST_CHECK(keystore.AddKey(keys[0]));

        scriptPubKey.clear();
        scriptPubKey << OP_16 << ToByteVector(ParseHex("aabb"));

        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }

//非标准的
    {
        CBasicKeyStore keystore;
        BOOST_CHECK(keystore.AddKey(keys[0]));

        scriptPubKey.clear();
        scriptPubKey << OP_9 << OP_ADD << OP_11 << OP_EQUAL;

        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }
}

BOOST_AUTO_TEST_SUITE_END()
