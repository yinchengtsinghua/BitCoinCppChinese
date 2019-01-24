
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

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <clientversion.h>
#include <coins.h>
#include <consensus/consensus.h>
#include <core_io.h>
#include <key_io.h>
#include <keystore.h>
#include <policy/policy.h>
#include <policy/rbf.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/sign.h>
#include <univalue.h>
#include <util/system.h>
#include <util/moneystr.h>
#include <util/strencodings.h>

#include <memory>
#include <stdio.h>

#include <boost/algorithm/string.hpp>

static bool fCreateBlank;
static std::map<std::string,UniValue> registers;
static const int CONTINUE_EXECUTION=-1;

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

static void SetupBitcoinTxArgs()
{
    gArgs.AddArg("-?", "This help message", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-create", "Create new, empty TX.", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-json", "Select JSON output", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-txid", "Output only the hex-encoded transaction id of the resultant transaction.", false, OptionsCategory::OPTIONS);
    SetupChainParamsBaseOptions();

    gArgs.AddArg("delin=N", "Delete input N from TX", false, OptionsCategory::COMMANDS);
    gArgs.AddArg("delout=N", "Delete output N from TX", false, OptionsCategory::COMMANDS);
    gArgs.AddArg("in=TXID:VOUT(:SEQUENCE_NUMBER)", "Add input to TX", false, OptionsCategory::COMMANDS);
    gArgs.AddArg("locktime=N", "Set TX lock time to N", false, OptionsCategory::COMMANDS);
    gArgs.AddArg("nversion=N", "Set TX version to N", false, OptionsCategory::COMMANDS);
    gArgs.AddArg("outaddr=VALUE:ADDRESS", "Add address-based output to TX", false, OptionsCategory::COMMANDS);
    gArgs.AddArg("outdata=[VALUE:]DATA", "Add data-based output to TX", false, OptionsCategory::COMMANDS);
    gArgs.AddArg("outmultisig=VALUE:REQUIRED:PUBKEYS:PUBKEY1:PUBKEY2:....[:FLAGS]", "Add Pay To n-of-m Multi-sig output to TX. n = REQUIRED, m = PUBKEYS. "
        "Optionally add the \"W\" flag to produce a pay-to-witness-script-hash output. "
        "Optionally add the \"S\" flag to wrap the output in a pay-to-script-hash.", false, OptionsCategory::COMMANDS);
    gArgs.AddArg("outpubkey=VALUE:PUBKEY[:FLAGS]", "Add pay-to-pubkey output to TX. "
        "Optionally add the \"W\" flag to produce a pay-to-witness-pubkey-hash output. "
        "Optionally add the \"S\" flag to wrap the output in a pay-to-script-hash.", false, OptionsCategory::COMMANDS);
    gArgs.AddArg("outscript=VALUE:SCRIPT[:FLAGS]", "Add raw script output to TX. "
        "Optionally add the \"W\" flag to produce a pay-to-witness-script-hash output. "
        "Optionally add the \"S\" flag to wrap the output in a pay-to-script-hash.", false, OptionsCategory::COMMANDS);
    gArgs.AddArg("replaceable(=N)", "Set RBF opt-in sequence number for input N (if not provided, opt-in all available inputs)", false, OptionsCategory::COMMANDS);
    gArgs.AddArg("sign=SIGHASH-FLAGS", "Add zero or more signatures to transaction. "
        "This command requires JSON registers:"
        "prevtxs=JSON object, "
        "privatekeys=JSON object. "
        "See signrawtransaction docs for format of sighash flags, JSON objects.", false, OptionsCategory::COMMANDS);

    gArgs.AddArg("load=NAME:FILENAME", "Load JSON file FILENAME into register NAME", false, OptionsCategory::REGISTER_COMMANDS);
    gArgs.AddArg("set=NAME:JSON-STRING", "Set register NAME to given JSON-STRING", false, OptionsCategory::REGISTER_COMMANDS);

//隐藏的
    gArgs.AddArg("-h", "", false, OptionsCategory::HIDDEN);
    gArgs.AddArg("-help", "", false, OptionsCategory::HIDDEN);
}

//
//当需要停止进程或
//当预期继续执行时，继续执行。
//
static int AppInitRawTx(int argc, char* argv[])
{
//
//参数
//
    SetupBitcoinTxArgs();
    std::string error;
    if (!gArgs.ParseParameters(argc, argv, error)) {
        fprintf(stderr, "Error parsing command line arguments: %s\n", error.c_str());
        return EXIT_FAILURE;
    }

//检查-testnet或-regtest参数（params（）调用仅在此子句之后有效）
    try {
        SelectParams(gArgs.GetChainName());
    } catch (const std::exception& e) {
        fprintf(stderr, "Error: %s\n", e.what());
        return EXIT_FAILURE;
    }

    fCreateBlank = gArgs.GetBoolArg("-create", false);

    if (argc < 2 || HelpRequested(gArgs)) {
//帮助消息的第一部分特定于此实用程序
        std::string strUsage = PACKAGE_NAME " bitcoin-tx utility version " + FormatFullVersion() + "\n\n" +
            "Usage:  bitcoin-tx [options] <hex-tx> [commands]  Update hex-encoded bitcoin transaction\n" +
            "or:     bitcoin-tx [options] -create [commands]   Create hex-encoded bitcoin transaction\n" +
            "\n";
        strUsage += gArgs.GetHelpMessage();

        fprintf(stdout, "%s", strUsage.c_str());

        if (argc < 2) {
            fprintf(stderr, "Error: too few parameters\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    return CONTINUE_EXECUTION;
}

static void RegisterSetJson(const std::string& key, const std::string& rawJson)
{
    UniValue val;
    if (!val.read(rawJson)) {
        std::string strErr = "Cannot parse JSON for key " + key;
        throw std::runtime_error(strErr);
    }

    registers[key] = val;
}

static void RegisterSet(const std::string& strInput)
{
//单独的名称：字符串中的值
    size_t pos = strInput.find(':');
    if ((pos == std::string::npos) ||
        (pos == 0) ||
        (pos == (strInput.size() - 1)))
        throw std::runtime_error("Register input requires NAME:VALUE");

    std::string key = strInput.substr(0, pos);
    std::string valStr = strInput.substr(pos + 1, std::string::npos);

    RegisterSetJson(key, valStr);
}

static void RegisterLoad(const std::string& strInput)
{
//单独的名称：字符串中的文件名
    size_t pos = strInput.find(':');
    if ((pos == std::string::npos) ||
        (pos == 0) ||
        (pos == (strInput.size() - 1)))
        throw std::runtime_error("Register load requires NAME:FILENAME");

    std::string key = strInput.substr(0, pos);
    std::string filename = strInput.substr(pos + 1, std::string::npos);

    FILE *f = fopen(filename.c_str(), "r");
    if (!f) {
        std::string strErr = "Cannot open file " + filename;
        throw std::runtime_error(strErr);
    }

//将文件块加载到一个大缓冲区中
    std::string valStr;
    while ((!feof(f)) && (!ferror(f))) {
        char buf[4096];
        int bread = fread(buf, 1, sizeof(buf), f);
        if (bread <= 0)
            break;

        valStr.insert(valStr.size(), buf, bread);
    }

    int error = ferror(f);
    fclose(f);

    if (error) {
        std::string strErr = "Error reading file " + filename;
        throw std::runtime_error(strErr);
    }

//评估为JSON缓冲寄存器
    RegisterSetJson(key, valStr);
}

static CAmount ExtractAndValidateValue(const std::string& strValue)
{
    CAmount value;
    if (!ParseMoney(strValue, value))
        throw std::runtime_error("invalid TX output value");
    return value;
}

static void MutateTxVersion(CMutableTransaction& tx, const std::string& cmdVal)
{
    int64_t newVersion;
    if (!ParseInt64(cmdVal, &newVersion) || newVersion < 1 || newVersion > CTransaction::MAX_STANDARD_VERSION)
        throw std::runtime_error("Invalid TX version requested: '" + cmdVal + "'");

    tx.nVersion = (int) newVersion;
}

static void MutateTxLocktime(CMutableTransaction& tx, const std::string& cmdVal)
{
    int64_t newLocktime;
    if (!ParseInt64(cmdVal, &newLocktime) || newLocktime < 0LL || newLocktime > 0xffffffffLL)
        throw std::runtime_error("Invalid TX locktime requested: '" + cmdVal + "'");

    tx.nLockTime = (unsigned int) newLocktime;
}

static void MutateTxRBFOptIn(CMutableTransaction& tx, const std::string& strInIdx)
{
//分析请求的索引
    int64_t inIdx;
    if (!ParseInt64(strInIdx, &inIdx) || inIdx < 0 || inIdx >= static_cast<int64_t>(tx.vin.size())) {
        throw std::runtime_error("Invalid TX input index '" + strInIdx + "'");
    }

//将Nsequence设置为max_int-2（=rbf opt in标志）
    int cnt = 0;
    for (CTxIn& txin : tx.vin) {
        if (strInIdx == "" || cnt == inIdx) {
            if (txin.nSequence > MAX_BIP125_RBF_SEQUENCE) {
                txin.nSequence = MAX_BIP125_RBF_SEQUENCE;
            }
        }
        ++cnt;
    }
}

static void MutateTxAddInput(CMutableTransaction& tx, const std::string& strInput)
{
    std::vector<std::string> vStrInputParts;
    boost::split(vStrInputParts, strInput, boost::is_any_of(":"));

//单独的txid:vout在字符串中
    if (vStrInputParts.size()<2)
        throw std::runtime_error("TX input missing separator");

//提取并验证TxID
    uint256 txid;
    if (!ParseHashStr(vStrInputParts[0], txid)) {
        throw std::runtime_error("invalid TX input txid");
    }

    static const unsigned int minTxOutSz = 9;
    static const unsigned int maxVout = MAX_BLOCK_WEIGHT / (WITNESS_SCALE_FACTOR * minTxOutSz);

//提取并验证VOUT
    const std::string& strVout = vStrInputParts[1];
    int64_t vout;
    if (!ParseInt64(strVout, &vout) || vout < 0 || vout > static_cast<int64_t>(maxVout))
        throw std::runtime_error("invalid TX input vout '" + strVout + "'");

//提取可选序列号
    uint32_t nSequenceIn = CTxIn::SEQUENCE_FINAL;
    if (vStrInputParts.size() > 2)
        nSequenceIn = std::stoul(vStrInputParts[2]);

//追加到事务输入列表
    CTxIn txin(txid, vout, CScript(), nSequenceIn);
    tx.vin.push_back(txin);
}

static void MutateTxAddOutAddr(CMutableTransaction& tx, const std::string& strInput)
{
//分值：地址
    std::vector<std::string> vStrInputParts;
    boost::split(vStrInputParts, strInput, boost::is_any_of(":"));

    if (vStrInputParts.size() != 2)
        throw std::runtime_error("TX output missing or too many separators");

//提取并验证值
    CAmount value = ExtractAndValidateValue(vStrInputParts[0]);

//提取并验证地址
    std::string strAddr = vStrInputParts[1];
    CTxDestination destination = DecodeDestination(strAddr);
    if (!IsValidDestination(destination)) {
        throw std::runtime_error("invalid TX output address");
    }
    CScript scriptPubKey = GetScriptForDestination(destination);

//构造txout，追加到事务输出列表
    CTxOut txout(value, scriptPubKey);
    tx.vout.push_back(txout);
}

static void MutateTxAddOutPubKey(CMutableTransaction& tx, const std::string& strInput)
{
//分隔为值：pubkey[：flags]
    std::vector<std::string> vStrInputParts;
    boost::split(vStrInputParts, strInput, boost::is_any_of(":"));

    if (vStrInputParts.size() < 2 || vStrInputParts.size() > 3)
        throw std::runtime_error("TX output missing or too many separators");

//提取并验证值
    CAmount value = ExtractAndValidateValue(vStrInputParts[0]);

//提取并验证pubkey
    CPubKey pubkey(ParseHex(vStrInputParts[1]));
    if (!pubkey.IsFullyValid())
        throw std::runtime_error("invalid TX output pubkey");
    CScript scriptPubKey = GetScriptForRawPubKey(pubkey);

//提取和验证标志
    bool bSegWit = false;
    bool bScriptHash = false;
    if (vStrInputParts.size() == 3) {
        std::string flags = vStrInputParts[2];
        bSegWit = (flags.find('W') != std::string::npos);
        bScriptHash = (flags.find('S') != std::string::npos);
    }

    if (bSegWit) {
        if (!pubkey.IsCompressed()) {
            throw std::runtime_error("Uncompressed pubkeys are not useable for SegWit outputs");
        }
//调用getscriptforwitness（）生成p2wsh scriptpubkey
        scriptPubKey = GetScriptForWitness(scriptPubKey);
    }
    if (bScriptHash) {
//获取脚本的ID，然后为其构造一个p2sh目标。
        scriptPubKey = GetScriptForDestination(CScriptID(scriptPubKey));
    }

//构造txout，追加到事务输出列表
    CTxOut txout(value, scriptPubKey);
    tx.vout.push_back(txout);
}

static void MutateTxAddOutMultiSig(CMutableTransaction& tx, const std::string& strInput)
{
//分隔为值：必需：numkeys:pubkey1:pubkey2:……[：标志]
    std::vector<std::string> vStrInputParts;
    boost::split(vStrInputParts, strInput, boost::is_any_of(":"));

//检查是否有足够的参数
    if (vStrInputParts.size()<3)
        throw std::runtime_error("Not enough multisig parameters");

//提取并验证值
    CAmount value = ExtractAndValidateValue(vStrInputParts[0]);

//需要提取
    uint32_t required = stoul(vStrInputParts[1]);

//提取数字
    uint32_t numkeys = stoul(vStrInputParts[2]);

//验证是否有正确数量的公钥
    if (vStrInputParts.size() < numkeys + 3)
        throw std::runtime_error("incorrect number of multisig pubkeys");

    if (required < 1 || required > MAX_PUBKEYS_PER_MULTISIG || numkeys < 1 || numkeys > MAX_PUBKEYS_PER_MULTISIG || numkeys < required)
        throw std::runtime_error("multisig parameter mismatch. Required " \
                            + std::to_string(required) + " of " + std::to_string(numkeys) + "signatures.");

//提取和验证公钥
    std::vector<CPubKey> pubkeys;
    for(int pos = 1; pos <= int(numkeys); pos++) {
        CPubKey pubkey(ParseHex(vStrInputParts[pos + 2]));
        if (!pubkey.IsFullyValid())
            throw std::runtime_error("invalid TX output pubkey");
        pubkeys.push_back(pubkey);
    }

//提取标志
    bool bSegWit = false;
    bool bScriptHash = false;
    if (vStrInputParts.size() == numkeys + 4) {
        std::string flags = vStrInputParts.back();
        bSegWit = (flags.find('W') != std::string::npos);
        bScriptHash = (flags.find('S') != std::string::npos);
    }
    else if (vStrInputParts.size() > numkeys + 4) {
//验证是否不再传递参数
        throw std::runtime_error("Too many parameters");
    }

    CScript scriptPubKey = GetScriptForMultisig(required, pubkeys);

    if (bSegWit) {
        for (const CPubKey& pubkey : pubkeys) {
            if (!pubkey.IsCompressed()) {
                throw std::runtime_error("Uncompressed pubkeys are not useable for SegWit outputs");
            }
        }
//调用getscriptforwitness（）生成p2wsh scriptpubkey
        scriptPubKey = GetScriptForWitness(scriptPubKey);
    }
    if (bScriptHash) {
        if (scriptPubKey.size() > MAX_SCRIPT_ELEMENT_SIZE) {
            throw std::runtime_error(strprintf(
                        "redeemScript exceeds size limit: %d > %d", scriptPubKey.size(), MAX_SCRIPT_ELEMENT_SIZE));
        }
//获取脚本的ID，然后为其构造一个p2sh目标。
        scriptPubKey = GetScriptForDestination(CScriptID(scriptPubKey));
    }

//构造txout，追加到事务输出列表
    CTxOut txout(value, scriptPubKey);
    tx.vout.push_back(txout);
}

static void MutateTxAddOutData(CMutableTransaction& tx, const std::string& strInput)
{
    CAmount value = 0;

//分隔字符串中的[值：]data
    size_t pos = strInput.find(':');

    if (pos==0)
        throw std::runtime_error("TX output value not specified");

    if (pos != std::string::npos) {
//提取并验证值
        value = ExtractAndValidateValue(strInput.substr(0, pos));
    }

//提取和验证数据
    std::string strData = strInput.substr(pos + 1, std::string::npos);

    if (!IsHex(strData))
        throw std::runtime_error("invalid TX output data");

    std::vector<unsigned char> data = ParseHex(strData);

    CTxOut txout(value, CScript() << OP_RETURN << data);
    tx.vout.push_back(txout);
}

static void MutateTxAddOutScript(CMutableTransaction& tx, const std::string& strInput)
{
//单独的值：script[：flags]
    std::vector<std::string> vStrInputParts;
    boost::split(vStrInputParts, strInput, boost::is_any_of(":"));
    if (vStrInputParts.size() < 2)
        throw std::runtime_error("TX output missing separator");

//提取并验证值
    CAmount value = ExtractAndValidateValue(vStrInputParts[0]);

//提取并验证脚本
    std::string strScript = vStrInputParts[1];
    CScript scriptPubKey = ParseScript(strScript);

//提取标志
    bool bSegWit = false;
    bool bScriptHash = false;
    if (vStrInputParts.size() == 3) {
        std::string flags = vStrInputParts.back();
        bSegWit = (flags.find('W') != std::string::npos);
        bScriptHash = (flags.find('S') != std::string::npos);
    }

    if (scriptPubKey.size() > MAX_SCRIPT_SIZE) {
        throw std::runtime_error(strprintf(
                    "script exceeds size limit: %d > %d", scriptPubKey.size(), MAX_SCRIPT_SIZE));
    }

    if (bSegWit) {
        scriptPubKey = GetScriptForWitness(scriptPubKey);
    }
    if (bScriptHash) {
        if (scriptPubKey.size() > MAX_SCRIPT_ELEMENT_SIZE) {
            throw std::runtime_error(strprintf(
                        "redeemScript exceeds size limit: %d > %d", scriptPubKey.size(), MAX_SCRIPT_ELEMENT_SIZE));
        }
        scriptPubKey = GetScriptForDestination(CScriptID(scriptPubKey));
    }

//构造txout，追加到事务输出列表
    CTxOut txout(value, scriptPubKey);
    tx.vout.push_back(txout);
}

static void MutateTxDelInput(CMutableTransaction& tx, const std::string& strInIdx)
{
//分析请求的删除索引
    int64_t inIdx;
    if (!ParseInt64(strInIdx, &inIdx) || inIdx < 0 || inIdx >= static_cast<int64_t>(tx.vin.size())) {
        throw std::runtime_error("Invalid TX input index '" + strInIdx + "'");
    }

//从事务中删除输入
    tx.vin.erase(tx.vin.begin() + inIdx);
}

static void MutateTxDelOutput(CMutableTransaction& tx, const std::string& strOutIdx)
{
//分析请求的删除索引
    int64_t outIdx;
    if (!ParseInt64(strOutIdx, &outIdx) || outIdx < 0 || outIdx >= static_cast<int64_t>(tx.vout.size())) {
        throw std::runtime_error("Invalid TX output index '" + strOutIdx + "'");
    }

//从事务中删除输出
    tx.vout.erase(tx.vout.begin() + outIdx);
}

static const unsigned int N_SIGHASH_OPTS = 6;
static const struct {
    const char *flagStr;
    int flags;
} sighashOptions[N_SIGHASH_OPTS] = {
    {"ALL", SIGHASH_ALL},
    {"NONE", SIGHASH_NONE},
    {"SINGLE", SIGHASH_SINGLE},
    {"ALL|ANYONECANPAY", SIGHASH_ALL|SIGHASH_ANYONECANPAY},
    {"NONE|ANYONECANPAY", SIGHASH_NONE|SIGHASH_ANYONECANPAY},
    {"SINGLE|ANYONECANPAY", SIGHASH_SINGLE|SIGHASH_ANYONECANPAY},
};

static bool findSighashFlags(int& flags, const std::string& flagStr)
{
    flags = 0;

    for (unsigned int i = 0; i < N_SIGHASH_OPTS; i++) {
        if (flagStr == sighashOptions[i].flagStr) {
            flags = sighashOptions[i].flags;
            return true;
        }
    }

    return false;
}

static CAmount AmountFromValue(const UniValue& value)
{
    if (!value.isNum() && !value.isStr())
        throw std::runtime_error("Amount is not a number or string");
    CAmount amount;
    if (!ParseFixedPoint(value.getValStr(), 8, &amount))
        throw std::runtime_error("Invalid amount");
    if (!MoneyRange(amount))
        throw std::runtime_error("Amount out of range");
    return amount;
}

static void MutateTxSign(CMutableTransaction& tx, const std::string& flagStr)
{
    int nHashType = SIGHASH_ALL;

    if (flagStr.size() > 0)
        if (!findSighashFlags(nHashType, flagStr))
            throw std::runtime_error("unknown sighash flag/sign option");

//mergedtx将以所有签名结束；它
//以原始Tx的克隆开始：
    CMutableTransaction mergedTx{tx};
    const CMutableTransaction txv{tx};
    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);

    if (!registers.count("privatekeys"))
        throw std::runtime_error("privatekeys register variable must be set.");
    CBasicKeyStore tempKeystore;
    UniValue keysObj = registers["privatekeys"];

    for (unsigned int kidx = 0; kidx < keysObj.size(); kidx++) {
        if (!keysObj[kidx].isStr())
            throw std::runtime_error("privatekey not a std::string");
        CKey key = DecodeSecret(keysObj[kidx].getValStr());
        if (!key.IsValid()) {
            throw std::runtime_error("privatekey not valid");
        }
        tempKeystore.AddKey(key);
    }

//添加在RPC调用中给定的先前txout:
    if (!registers.count("prevtxs"))
        throw std::runtime_error("prevtxs register variable must be set.");
    UniValue prevtxsObj = registers["prevtxs"];
    {
        for (unsigned int previdx = 0; previdx < prevtxsObj.size(); previdx++) {
            UniValue prevOut = prevtxsObj[previdx];
            if (!prevOut.isObject())
                throw std::runtime_error("expected prevtxs internal object");

            std::map<std::string, UniValue::VType> types = {
                {"txid", UniValue::VSTR},
                {"vout", UniValue::VNUM},
                {"scriptPubKey", UniValue::VSTR},
            };
            if (!prevOut.checkObject(types))
                throw std::runtime_error("prevtxs internal object typecheck fail");

            uint256 txid;
            if (!ParseHashStr(prevOut["txid"].get_str(), txid)) {
                throw std::runtime_error("txid must be hexadecimal string (not '" + prevOut["txid"].get_str() + "')");
            }

            const int nOut = prevOut["vout"].get_int();
            if (nOut < 0)
                throw std::runtime_error("vout must be positive");

            COutPoint out(txid, nOut);
            std::vector<unsigned char> pkData(ParseHexUV(prevOut["scriptPubKey"], "scriptPubKey"));
            CScript scriptPubKey(pkData.begin(), pkData.end());

            {
                const Coin& coin = view.AccessCoin(out);
                if (!coin.IsSpent() && coin.out.scriptPubKey != scriptPubKey) {
                    std::string err("Previous output scriptPubKey mismatch:\n");
                    err = err + ScriptToAsmStr(coin.out.scriptPubKey) + "\nvs:\n"+
                        ScriptToAsmStr(scriptPubKey);
                    throw std::runtime_error(err);
                }
                Coin newcoin;
                newcoin.out.scriptPubKey = scriptPubKey;
                newcoin.out.nValue = 0;
                if (prevOut.exists("amount")) {
                    newcoin.out.nValue = AmountFromValue(prevOut["amount"]);
                }
                newcoin.nHeight = 1;
                view.AddCoin(out, std::move(newcoin), true);
            }

//如果提供了redeemscript和私钥，
//将redeemscript添加到tempkeystore，以便对其进行签名：
            if ((scriptPubKey.IsPayToScriptHash() || scriptPubKey.IsPayToWitnessScriptHash()) &&
                prevOut.exists("redeemScript")) {
                UniValue v = prevOut["redeemScript"];
                std::vector<unsigned char> rsData(ParseHexUV(v, "redeemScript"));
                CScript redeemScript(rsData.begin(), rsData.end());
                tempKeystore.AddCScript(redeemScript);
            }
        }
    }

    const CKeyStore& keystore = tempKeystore;

    bool fHashSingle = ((nHashType & ~SIGHASH_ANYONECANPAY) == SIGHASH_SINGLE);

//签署我们能签署的：
    for (unsigned int i = 0; i < mergedTx.vin.size(); i++) {
        CTxIn& txin = mergedTx.vin[i];
        const Coin& coin = view.AccessCoin(txin.prevout);
        if (coin.IsSpent()) {
            continue;
        }
        const CScript& prevPubKey = coin.out.scriptPubKey;
        const CAmount& amount = coin.out.nValue;

        SignatureData sigdata = DataFromTransaction(mergedTx, i, coin.out);
//只有在有相应输出的情况下，才能在SIGHASH_SINGLE上签名：
        if (!fHashSingle || (i < mergedTx.vout.size()))
            ProduceSignature(keystore, MutableTransactionSignatureCreator(&mergedTx, i, amount, nHashType), prevPubKey, sigdata);

        UpdateInput(txin, sigdata);
    }

    tx = mergedTx;
}

class Secp256k1Init
{
    ECCVerifyHandle globalVerifyHandle;

public:
    Secp256k1Init() {
        ECC_Start();
    }
    ~Secp256k1Init() {
        ECC_Stop();
    }
};

static void MutateTx(CMutableTransaction& tx, const std::string& command,
                     const std::string& commandVal)
{
    std::unique_ptr<Secp256k1Init> ecc;

    if (command == "nversion")
        MutateTxVersion(tx, commandVal);
    else if (command == "locktime")
        MutateTxLocktime(tx, commandVal);
    else if (command == "replaceable") {
        MutateTxRBFOptIn(tx, commandVal);
    }

    else if (command == "delin")
        MutateTxDelInput(tx, commandVal);
    else if (command == "in")
        MutateTxAddInput(tx, commandVal);

    else if (command == "delout")
        MutateTxDelOutput(tx, commandVal);
    else if (command == "outaddr")
        MutateTxAddOutAddr(tx, commandVal);
    else if (command == "outpubkey") {
        ecc.reset(new Secp256k1Init());
        MutateTxAddOutPubKey(tx, commandVal);
    } else if (command == "outmultisig") {
        ecc.reset(new Secp256k1Init());
        MutateTxAddOutMultiSig(tx, commandVal);
    } else if (command == "outscript")
        MutateTxAddOutScript(tx, commandVal);
    else if (command == "outdata")
        MutateTxAddOutData(tx, commandVal);

    else if (command == "sign") {
        ecc.reset(new Secp256k1Init());
        MutateTxSign(tx, commandVal);
    }

    else if (command == "load")
        RegisterLoad(commandVal);

    else if (command == "set")
        RegisterSet(commandVal);

    else
        throw std::runtime_error("unknown command");
}

static void OutputTxJSON(const CTransaction& tx)
{
    UniValue entry(UniValue::VOBJ);
    TxToUniv(tx, uint256(), entry);

    std::string jsonOutput = entry.write(4);
    fprintf(stdout, "%s\n", jsonOutput.c_str());
}

static void OutputTxHash(const CTransaction& tx)
{
std::string strHexHash = tx.GetHash().GetHex(); //十六进制编码的事务哈希（也称为事务ID）

    fprintf(stdout, "%s\n", strHexHash.c_str());
}

static void OutputTxHex(const CTransaction& tx)
{
    std::string strHex = EncodeHexTx(tx);

    fprintf(stdout, "%s\n", strHex.c_str());
}

static void OutputTx(const CTransaction& tx)
{
    if (gArgs.GetBoolArg("-json", false))
        OutputTxJSON(tx);
    else if (gArgs.GetBoolArg("-txid", false))
        OutputTxHash(tx);
    else
        OutputTxHex(tx);
}

static std::string readStdin()
{
    char buf[4096];
    std::string ret;

    while (!feof(stdin)) {
        size_t bread = fread(buf, 1, sizeof(buf), stdin);
        ret.append(buf, bread);
        if (bread < sizeof(buf))
            break;
    }

    if (ferror(stdin))
        throw std::runtime_error("error reading stdin");

    boost::algorithm::trim_right(ret);

    return ret;
}

static int CommandLineRawTx(int argc, char* argv[])
{
    std::string strPrint;
    int nRet = 0;
    try {
//跳过开关；允许通用stdin约定“—”
        while (argc > 1 && IsSwitchChar(argv[1][0]) &&
               (argv[1][1] != 0)) {
            argc--;
            argv++;
        }

        CMutableTransaction tx;
        int startArg;

        if (!fCreateBlank) {
//至少需要一个参数
            if (argc < 2)
                throw std::runtime_error("too few parameters");

//参数：十六进制编码比特币交易
            std::string strHexTx(argv[1]);
if (strHexTx == "-")                 //“-”表示标准输入
                strHexTx = readStdin();

            if (!DecodeHexTx(tx, strHexTx, true))
                throw std::runtime_error("invalid transaction encoding");

            startArg = 2;
        } else
            startArg = 1;

        for (int i = startArg; i < argc; i++) {
            std::string arg = argv[i];
            std::string key, value;
            size_t eqpos = arg.find('=');
            if (eqpos == std::string::npos)
                key = arg;
            else {
                key = arg.substr(0, eqpos);
                value = arg.substr(eqpos + 1);
            }

            MutateTx(tx, key, value);
        }

        OutputTx(tx);
    }
    catch (const std::exception& e) {
        strPrint = std::string("error: ") + e.what();
        nRet = EXIT_FAILURE;
    }
    catch (...) {
        PrintExceptionContinue(nullptr, "CommandLineRawTx()");
        throw;
    }

    if (strPrint != "") {
        fprintf((nRet == 0 ? stdout : stderr), "%s\n", strPrint.c_str());
    }
    return nRet;
}

int main(int argc, char* argv[])
{
    SetupEnvironment();

    try {
        int ret = AppInitRawTx(argc, argv);
        if (ret != CONTINUE_EXECUTION)
            return ret;
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "AppInitRawTx()");
        return EXIT_FAILURE;
    } catch (...) {
        PrintExceptionContinue(nullptr, "AppInitRawTx()");
        return EXIT_FAILURE;
    }

    int ret = EXIT_FAILURE;
    try {
        ret = CommandLineRawTx(argc, argv);
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "CommandLineRawTx()");
    } catch (...) {
        PrintExceptionContinue(nullptr, "CommandLineRawTx()");
    }
    return ret;
}
