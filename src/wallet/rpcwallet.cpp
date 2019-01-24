
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <amount.h>
#include <chain.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <httpserver.h>
#include <init.h>
#include <interfaces/chain.h>
#include <validation.h>
#include <key_io.h>
#include <net.h>
#include <outputtype.h>
#include <policy/feerate.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <policy/rbf.h>
#include <rpc/mining.h>
#include <rpc/rawtransaction.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <script/descriptor.h>
#include <script/sign.h>
#include <shutdown.h>
#include <timedata.h>
#include <util/system.h>
#include <util/moneystr.h>
#include <wallet/coincontrol.h>
#include <wallet/feebumper.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>
#include <wallet/walletutil.h>

#include <stdint.h>

#include <univalue.h>

#include <functional>

static const std::string WALLET_ENDPOINT_BASE = "/wallet/";

bool GetWalletNameFromJSONRPCRequest(const JSONRPCRequest& request, std::string& wallet_name)
{
    if (request.URI.substr(0, WALLET_ENDPOINT_BASE.size()) == WALLET_ENDPOINT_BASE) {
//使用了钱包端点
        wallet_name = urlDecode(request.URI.substr(WALLET_ENDPOINT_BASE.size()));
        return true;
    }
    return false;
}

std::shared_ptr<CWallet> GetWalletForJSONRPCRequest(const JSONRPCRequest& request)
{
    std::string wallet_name;
    if (GetWalletNameFromJSONRPCRequest(request, wallet_name)) {
        std::shared_ptr<CWallet> pwallet = GetWallet(wallet_name);
        if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Requested wallet does not exist or is not loaded");
        return pwallet;
    }

    std::vector<std::shared_ptr<CWallet>> wallets = GetWallets();
    return wallets.size() == 1 || (request.fHelp && wallets.size() > 0) ? wallets[0] : nullptr;
}

std::string HelpRequiringPassphrase(CWallet * const pwallet)
{
    return pwallet && pwallet->IsCrypted()
        ? "\nRequires wallet passphrase to be set with walletpassphrase call."
        : "";
}

bool EnsureWalletIsAvailable(CWallet * const pwallet, bool avoidException)
{
    if (pwallet) return true;
    if (avoidException) return false;
    if (!HasWallets()) {
        throw JSONRPCError(
            RPC_METHOD_NOT_FOUND, "Method not found (wallet method is disabled because no wallet is loaded)");
    }
    throw JSONRPCError(RPC_WALLET_NOT_SPECIFIED,
        "Wallet file not specified (must request wallet RPC through /wallet/<filename> uri-path).");
}

void EnsureWalletIsUnlocked(CWallet * const pwallet)
{
    if (pwallet->IsLocked()) {
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    }
}

static void WalletTxToJSON(interfaces::Chain& chain, interfaces::Chain::Lock& locked_chain, const CWalletTx& wtx, UniValue& entry)
{
    int confirms = wtx.GetDepthInMainChain(locked_chain);
    entry.pushKV("confirmations", confirms);
    if (wtx.IsCoinBase())
        entry.pushKV("generated", true);
    if (confirms > 0)
    {
        entry.pushKV("blockhash", wtx.hashBlock.GetHex());
        entry.pushKV("blockindex", wtx.nIndex);
        entry.pushKV("blocktime", LookupBlockIndex(wtx.hashBlock)->GetBlockTime());
    } else {
        entry.pushKV("trusted", wtx.IsTrusted(locked_chain));
    }
    uint256 hash = wtx.GetHash();
    entry.pushKV("txid", hash.GetHex());
    UniValue conflicts(UniValue::VARR);
    for (const uint256& conflict : wtx.GetConflicts())
        conflicts.push_back(conflict.GetHex());
    entry.pushKV("walletconflicts", conflicts);
    entry.pushKV("time", wtx.GetTxTime());
    entry.pushKV("timereceived", (int64_t)wtx.nTimeReceived);

//添加选择加入RBF状态
    std::string rbfStatus = "no";
    if (confirms <= 0) {
        LOCK(mempool.cs);
        RBFTransactionState rbfState = IsRBFOptIn(*wtx.tx, mempool);
        if (rbfState == RBFTransactionState::UNKNOWN)
            rbfStatus = "unknown";
        else if (rbfState == RBFTransactionState::REPLACEABLE_BIP125)
            rbfStatus = "yes";
    }
    entry.pushKV("bip125-replaceable", rbfStatus);

    for (const std::pair<const std::string, std::string>& item : wtx.mapValue)
        entry.pushKV(item.first, item.second);
}

static std::string LabelFromValue(const UniValue& value)
{
    std::string label = value.get_str();
    if (label == "*")
        throw JSONRPCError(RPC_WALLET_INVALID_LABEL_NAME, "Invalid label name");
    return label;
}

static UniValue getnewaddress(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            RPCHelpMan{"getnewaddress",
                "\nReturns a new Bitcoin address for receiving payments.\n"
                "If 'label' is specified, it is added to the address book \n"
                "so payments received with the address will be associated with 'label'.\n",
                {
                    /*abel“，rpcarg:：type:：str，/*opt*/true，/*default_val*/”null“，”要链接的地址的标签名称。如果未提供，则使用默认标签\“\”。也可以将其设置为空字符串“以表示默认标签”。标签不需要存在，如果没有给定名称的标签，将创建它。“，
                    “地址类型”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "set by -addresstype", "The address type to use. Options are \"legacy\", \"p2sh-segwit\", and \"bech32\"."},

                }}
                .ToString() +
            "\nResult:\n"
            "\"address\"    (string) The new bitcoin address\n"
            "\nExamples:\n"
            + HelpExampleCli("getnewaddress", "")
            + HelpExampleRpc("getnewaddress", "")
        );

    if (pwallet->IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Private keys are disabled for this wallet");
    }

    LOCK(pwallet->cs_wallet);

//首先分析标签，以便在出现错误时不生成密钥
    std::string label;
    if (!request.params[0].isNull())
        label = LabelFromValue(request.params[0]);

    OutputType output_type = pwallet->m_default_address_type;
    if (!request.params[1].isNull()) {
        if (!ParseOutputType(request.params[1].get_str(), output_type)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Unknown address type '%s'", request.params[1].get_str()));
        }
    }

    if (!pwallet->IsLocked()) {
        pwallet->TopUpKeyPool();
    }

//生成添加到钱包的新密钥
    CPubKey newKey;
    if (!pwallet->GetKeyFromPool(newKey)) {
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    }
    pwallet->LearnRelatedScripts(newKey, output_type);
    CTxDestination dest = GetDestinationForKey(newKey, output_type);

    pwallet->SetAddressBook(dest, label, "receive");

    return EncodeDestination(dest);
}

static UniValue getrawchangeaddress(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            RPCHelpMan{"getrawchangeaddress",
                "\nReturns a new Bitcoin address, for receiving change.\n"
                "This is for use with raw transactions, NOT normal use.\n",
                {
                    /*地址“type”，rpcarg:：type:：str，/*opt*/true，/*默认值“set by-changetype”，“要使用的地址类型”。选项有“传统”、“p2sh segwit”和“bech32”。
                }
                toSTRIN（）+
            “\NESRES:\N”
            “address”（字符串）地址\n”
            “\n实例：\n”
            +helpExamplecli（“getrawchangeaddress”，“”）
            +helpExampleRPC（“GetRawChangeAddress”，“”）
       ；

    if（pwallet->iswalletflagset（wallet_flag_disable_private_keys））
        throw jsonrpcerror（rpc_wallet_error，“错误：此钱包禁用私钥”）；
    }

    锁（pwallet->cs_wallet）；

    如果（！）pwallet->islocked（））
        pWallet->TopupKeyPool（）；
    }

    output type output_type=pwallet->m_默认_更改_类型！=outputtype：：更改“自动”？pwallet->m_default_change_type:pwallet->m_default_address_type；
    如果（！）request.params[0].isNull（））
        如果（！）parseOutputType（request.params[0].get_str（），output_type））
            throw jsonrpcerror（rpc_invalid_address_or_key，strprintf（“未知地址类型'%s'”，request.params[0].get_str（））；
        }
    }

    CReservekey reservekey（pwallet）；
    cpubkey-vchSubkey；
    如果（！）reservekey.getreservedkey（vchSubkey，真）
        throw jsonrpcerror（rpc_wallet_keypool_run_out，“错误：keypool已用完，请先调用keypoolrefill”）；

    reservekey.keepkey（）；

    pwallet->learnrelatedscripts（vchSubkey，output_type）；
    ctxdestination dest=getdestinationforkey（vchSubkey，output_type）；

    返回编码目的地（dest）；
}


静态单值集合标签（const-jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    如果（request.fhelp request.params.size（）！= 2）
        throw std:：runtime_错误（
            rpchelpman“设置标签”，
                \n设置与给定地址关联的标签。\n“，
                {
                    “地址”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The bitcoin address to be associated with a label."},

                    /*abel“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”，“要分配给地址的标签。”，
                }
                toSTRIN（）+
            “\n实例：\n”
            +helpExamplecli（“setlabel”，“1d1zrzne3juo7zyckeyqqqawd9y54f4xx\”“tabby\”）
            +helpExampleRPC（“setLabel”，“1d1zrzne3juo7zyckeyqqqawd9y54f4xx”，“tabby\”）
        ；

    锁（pwallet->cs_wallet）；

    ctxdestination dest=decodedestination（request.params[0].get_str（））；
    如果（！）isvaliddestination（目的地））
        throw jsonrpcerror（rpc_invalid_address_or_key，“无效比特币地址”）；
    }

    std:：string label=labelfromvalue（请求.params[1]）；

    if（ismine（*pwallet，dest））
        pwallet->setaddressbook（dest，label，“receive”）；
    }否则{
        pwallet->setaddressbook（dest，label，“send”）；
    }

    返回nullunivalue；
}


静态ctransactionref sendmoney（接口：：chain:：lock&locked_chain，cwallet*const pwallet，const ctxdestination&address，camount nvalue，bool fsubtractfefromamount，const ccoincontrol&coinou control，mapvalue_t mapvalue）
{
    camount cobblance=pwallet->getbalance（）；

    /检查数量
    if（nvalue<=0）
        throw jsonrpcerror（rpc_invalid_参数，“invalid amount”）；

    if（nvalue>限制平衡）
        抛出jsonrpcerror（rpc_wallet_insufficient_funds，“insufficient funds”）；

    如果（pwallet->getBroadcastTransactions（）&&！G-康曼）
        throw jsonrpcerror（rpc_client_p2p_disabled，“错误：缺少或禁用对等功能”）；
    }

    //解析比特币地址
    cscript scriptPubkey=getscriptForDestination（地址）；

    //创建并发送事务
    CReservekey reservekey（pwallet）；
    伪装成被要求的样子；
    std：：字符串strError；
    std:：vector<crecipient>vecsend；
    int nchangeposret=-1；
    crecipient recipient=scriptpubkey，nvalue，fsubtractfeefromomamount_
    vecsend.push_back（收件人）；
    ctransactionref发送；
    如果（！）pwallet->createTransaction（锁定的_-chain、vecsend、tx、reservekey、nfeereequired、nchangeposret、strerror、coin_-control））；
        如果（！）fsubtractfefromamount和nvalue+nfeereequired>路缘平衡）
            strerror=strprintf（“错误：此事务要求至少%s”的事务费，formatmoney（nfeerequired））；
        抛出jsonrpcerror（rpc_wallet_error，strerror）；
    }
    cvalidationState状态；
    如果（！）pwallet->commitTransaction（tx，std:：move（mapvalue），/*订单*/, reservekey, g_connman.get(), state)) {

        strError = strprintf("Error: The transaction was rejected! Reason given: %s", FormatStateMessage(state));
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    return tx;
}

static UniValue sendtoaddress(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 2 || request.params.size() > 8)
        throw std::runtime_error(
            RPCHelpMan{"sendtoaddress",
                "\nSend an amount to a given address." +
                    HelpRequiringPassphrase(pwallet) + "\n",
                {
                    /*地址“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”，“要发送到的比特币地址。”，
                    “金额”，rpcarg:：type:：amount，/*opt*/ false, /* default_val */ "", "The amount in " + CURRENCY_UNIT + " to send. eg 0.1"},

                    /*comment“，rpcarg:：type:：str，/*opt*/true，/*default_val*/”null“，”用于存储事务用途的注释。\n”
            “这不是交易的一部分，只是放在你的钱包里。”，
                    “comment_to”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "null", "A comment to store the name of the person or organization\n"

            "                             to which you're sending the transaction. This is not part of the \n"
            "                             transaction, just kept in your wallet."},
                    /*ubtractfeefromomamount“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”false“，”费用将从发送的金额中扣除。\n”
            “收件人将收到比您在“金额”字段中输入的比特币更少的比特币。”，
                    “可替换”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "fallback to wallet's default", "Allow this transaction to be replaced by a transaction with higher fees via BIP 125"},

                    /*onf_target“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”回退到wallet的默认值“，”确认目标（以块为单位）”，
                    “估算模式”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "UNSET", "The fee estimate mode, must be one of:\n"

            "       \"UNSET\"\n"
            "       \"ECONOMICAL\"\n"
            "       \"CONSERVATIVE\""},
                }}
                .ToString() +
            "\nResult:\n"
            "\"txid\"                  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1")
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 \"donation\" \"seans outpost\"")
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 \"\" \"\" true")
            + HelpExampleRpc("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, \"donation\", \"seans outpost\"")
        );

//确保结果至少在最新块之前有效
//在此之前，用户可能已经从另一个rpc命令获得了
    pwallet->BlockUntilSyncedToCurrentChain();

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    CTxDestination dest = DecodeDestination(request.params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

//数量
    CAmount nAmount = AmountFromValue(request.params[1]);
    if (nAmount <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount for send");

//钱包评论
    mapValue_t mapValue;
    if (!request.params[2].isNull() && !request.params[2].get_str().empty())
        mapValue["comment"] = request.params[2].get_str();
    if (!request.params[3].isNull() && !request.params[3].get_str().empty())
        mapValue["to"] = request.params[3].get_str();

    bool fSubtractFeeFromAmount = false;
    if (!request.params[4].isNull()) {
        fSubtractFeeFromAmount = request.params[4].get_bool();
    }

    CCoinControl coin_control;
    if (!request.params[5].isNull()) {
        coin_control.m_signal_bip125_rbf = request.params[5].get_bool();
    }

    if (!request.params[6].isNull()) {
        coin_control.m_confirm_target = ParseConfirmTarget(request.params[6]);
    }

    if (!request.params[7].isNull()) {
        if (!FeeModeFromString(request.params[7].get_str(), coin_control.m_fee_mode)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid estimate_mode parameter");
        }
    }


    EnsureWalletIsUnlocked(pwallet);

    CTransactionRef tx = SendMoney(*locked_chain, pwallet, dest, nAmount, fSubtractFeeFromAmount, coin_control, std::move(mapValue));
    return tx->GetHash().GetHex();
}

static UniValue listaddressgroupings(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            RPCHelpMan{"listaddressgroupings",
                "\nLists groups of addresses which have had their common ownership\n"
                "made public by common use as inputs or as the resulting change\n"
                "in past transactions\n",
                {}}
                .ToString() +
            "\nResult:\n"
            "[\n"
            "  [\n"
            "    [\n"
            "      \"address\",            (string) The bitcoin address\n"
            "      amount,                 (numeric) The amount in " + CURRENCY_UNIT + "\n"
            "      \"label\"               (string, optional) The label\n"
            "    ]\n"
            "    ,...\n"
            "  ]\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("listaddressgroupings", "")
            + HelpExampleRpc("listaddressgroupings", "")
        );

//确保结果至少在最新块之前有效
//在此之前，用户可能已经从另一个rpc命令获得了
    pwallet->BlockUntilSyncedToCurrentChain();

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    UniValue jsonGroupings(UniValue::VARR);
    std::map<CTxDestination, CAmount> balances = pwallet->GetAddressBalances(*locked_chain);
    for (const std::set<CTxDestination>& grouping : pwallet->GetAddressGroupings()) {
        UniValue jsonGrouping(UniValue::VARR);
        for (const CTxDestination& address : grouping)
        {
            UniValue addressInfo(UniValue::VARR);
            addressInfo.push_back(EncodeDestination(address));
            addressInfo.push_back(ValueFromAmount(balances[address]));
            {
                if (pwallet->mapAddressBook.find(address) != pwallet->mapAddressBook.end()) {
                    addressInfo.push_back(pwallet->mapAddressBook.find(address)->second.name);
                }
            }
            jsonGrouping.push_back(addressInfo);
        }
        jsonGroupings.push_back(jsonGrouping);
    }
    return jsonGroupings;
}

static UniValue signmessage(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            RPCHelpMan{"signmessage",
                "\nSign a message with the private key of an address" +
                    HelpRequiringPassphrase(pwallet) + "\n",
                {
                    /*地址“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”，“用于私钥的比特币地址。”，
                    “消息”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The message to create a signature of."},

                }}
                .ToString() +
            "\nResult:\n"
            "\"signature\"          (string) The signature of the message encoded in base 64\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"signature\" \"my message\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\", \"my message\"")
        );

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    std::string strAddress = request.params[0].get_str();
    std::string strMessage = request.params[1].get_str();

    CTxDestination dest = DecodeDestination(strAddress);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");
    }

    const CKeyID *keyID = boost::get<CKeyID>(&dest);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");
    }

    CKey key;
    if (!pwallet->GetKey(*keyID, key)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key not available");
    }

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    std::vector<unsigned char> vchSig;
    if (!key.SignCompact(ss.GetHash(), vchSig))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");

    return EncodeBase64(vchSig.data(), vchSig.size());
}

static UniValue getreceivedbyaddress(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            RPCHelpMan{"getreceivedbyaddress",
                "\nReturns the total amount received by the given address in transactions with at least minconf confirmations.\n",
                {
                    /*地址“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，“交易比特币地址。”，
                    “minconf”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "1", "Only include transactions confirmed at least this many times."},

                }}
                .ToString() +
            "\nResult:\n"
            "amount   (numeric) The total amount in " + CURRENCY_UNIT + " received at this address.\n"
            "\nExamples:\n"
            "\nThe amount from transactions with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\"") +
            "\nThe amount including unconfirmed transactions, zero confirmations\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" 0") +
            "\nThe amount with at least 6 confirmations\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" 6") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\", 6")
       );

//确保结果至少在最新块之前有效
//在此之前，用户可能已经从另一个rpc命令获得了
    pwallet->BlockUntilSyncedToCurrentChain();

LockAnnotation lock(::cs_main); //临时，用于下面的checkfinaltx。在即将到来的提交中删除。
    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

//比特币地址
    CTxDestination dest = DecodeDestination(request.params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Bitcoin address");
    }
    CScript scriptPubKey = GetScriptForDestination(dest);
    if (!IsMine(*pwallet, scriptPubKey)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Address not found in wallet");
    }

//最低确认数
    int nMinDepth = 1;
    if (!request.params[1].isNull())
        nMinDepth = request.params[1].get_int();

//计数
    CAmount nAmount = 0;
    for (const std::pair<const uint256, CWalletTx>& pairWtx : pwallet->mapWallet) {
        const CWalletTx& wtx = pairWtx.second;
        if (wtx.IsCoinBase() || !CheckFinalTx(*wtx.tx))
            continue;

        for (const CTxOut& txout : wtx.tx->vout)
            if (txout.scriptPubKey == scriptPubKey)
                if (wtx.GetDepthInMainChain(*locked_chain) >= nMinDepth)
                    nAmount += txout.nValue;
    }

    return  ValueFromAmount(nAmount);
}


static UniValue getreceivedbylabel(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            RPCHelpMan{"getreceivedbylabel",
                "\nReturns the total amount received by addresses with <label> in transactions with at least [minconf] confirmations.\n",
                {
                    /*abel“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，”所选标签可能是使用\“\”“”的默认标签，
                    “minconf”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "1", "Only include transactions confirmed at least this many times."},

                }}
                .ToString() +
            "\nResult:\n"
            "amount              (numeric) The total amount in " + CURRENCY_UNIT + " received for this label.\n"
            "\nExamples:\n"
            "\nAmount received by the default label with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbylabel", "\"\"") +
            "\nAmount received at the tabby label including unconfirmed amounts with zero confirmations\n"
            + HelpExampleCli("getreceivedbylabel", "\"tabby\" 0") +
            "\nThe amount with at least 6 confirmations\n"
            + HelpExampleCli("getreceivedbylabel", "\"tabby\" 6") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("getreceivedbylabel", "\"tabby\", 6")
        );

//确保结果至少在最新块之前有效
//在此之前，用户可能已经从另一个rpc命令获得了
    pwallet->BlockUntilSyncedToCurrentChain();

LockAnnotation lock(::cs_main); //临时，用于下面的checkfinaltx。在即将到来的提交中删除。
    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

//最低确认数
    int nMinDepth = 1;
    if (!request.params[1].isNull())
        nMinDepth = request.params[1].get_int();

//获取分配给标签的一组发布键
    std::string label = LabelFromValue(request.params[0]);
    std::set<CTxDestination> setAddress = pwallet->GetLabelAddresses(label);

//计数
    CAmount nAmount = 0;
    for (const std::pair<const uint256, CWalletTx>& pairWtx : pwallet->mapWallet) {
        const CWalletTx& wtx = pairWtx.second;
        if (wtx.IsCoinBase() || !CheckFinalTx(*wtx.tx))
            continue;

        for (const CTxOut& txout : wtx.tx->vout)
        {
            CTxDestination address;
            if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*pwallet, address) && setAddress.count(address)) {
                if (wtx.GetDepthInMainChain(*locked_chain) >= nMinDepth)
                    nAmount += txout.nValue;
            }
        }
    }

    return ValueFromAmount(nAmount);
}


static UniValue getbalance(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || (request.params.size() > 3 ))
        throw std::runtime_error(
            RPCHelpMan{"getbalance",
                "\nReturns the total available balance.\n"
                "The available balance is what the wallet considers currently spendable, and is\n"
                "thus affected by options which limit spendability such as -spendzeroconfchange.\n",
                {
                    /*ummy“，rpcarg:：type:：str，/*opt*/true，/*default_val*/”null“，”保持向后兼容。必须排除或设置为\“*\”。
                    “minconf”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "0", "Only include transactions confirmed at least this many times."},

                    /*nclude_watch only“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”false“，”还包括仅监视地址中的余额（请参见'importaddress'）”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “金额（数字）为这个钱包收到的以“+货币单位+的总金额。\n”
            “\n实例：\n”
            \n钱包中包含1个或多个确认的总金额\n
            +helpExamplecli（“getbalance”，“）+
            \n钱包中的总金额至少已确认6个区块\n
            +helpExamplecli（“getbalance”，“\”*\“6”）+
            “作为JSON-RPC调用\n”
            +helpexamplerpc（“getbalance”，“\”*\“，6”）。
        ；

    //确保结果至少在最新块之前有效
    //在此之前，用户可能已经从另一个rpc命令获取了
    pwallet->blockUntilsynchedTocurrentchain（）；

    自动锁定的_chain=pwallet->chain（）.lock（）；
    锁（pwallet->cs_wallet）；

    const univalue&dummy_value=request.params[0]；
    如果（！）dummy_value.isNull（）&&dummy_value.get_str（）！=“*”{
        throw jsonrpcerror（rpc_method_deprecated，“必须排除第一个伪参数或将其设置为\”*\“；”
    }

    int最小深度=0；
    如果（！）request.params[1].isNull（））
        min_depth=request.params[1].get_int（）；
    }

    ismine filter filter=isminesuspenable；
    如果（！）request.params[2].isNull（）&&request.params[2].getbool（））
        filter=filter_ismine_watch_only；
    }

    返回值FromAmount（pWallet->GetBalance（filter，min_depth））；
}

静态单值getunconfirmedbalance（const jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    if（request.fhelp request.params.size（）>0）
        throw std:：runtime_错误（
            rpchelpman“获取未确认余额”，
                “返回服务器未确认的总余额\n”，
                toSTRIN（））；

    //确保结果至少在最新块之前有效
    //在此之前，用户可能已经从另一个rpc命令获取了
    pwallet->blockUntilsynchedTocurrentchain（）；

    自动锁定的_chain=pwallet->chain（）.lock（）；
    锁（pwallet->cs_wallet）；

    返回值FromAmount（pWallet->GetUnconfirmedBalance（））；
}


静态单值sendmany（const-jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    if（request.fhelp request.params.size（）<2 request.params.size（）>8）
        throw std:：runtime_错误（
            rpchelpman“sendmany”，
                \n发送多次。金额是双精度浮点数。“+
                    帮助要求密码（pwallet）+\n“，
                {
                    “dummy”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "Must be set to \"\" for backwards compatibility.", "\"\""},

                    /*mounts“，rpcarg:：type:：obj，/*opt*/false，/*default_val*/”“，“带有地址和数量的JSON对象”，
                        {
                            “地址”，rpcarg:：type:：amount，/*opt*/ false, /* default_val */ "", "The bitcoin address is the key, the numeric amount (can be string) in " + CURRENCY_UNIT + " is the value"},

                        },
                    },
                    /*incof“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”1“，”仅使用至少多次确认的余额。“，
                    “comment”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "null", "A comment"},

                    /*ubtractfeefrom“，rpcarg:：type:：arr，/*opt*/true，/*default_val*/”null“，”一个带地址的json数组。\n”
            “费用将从每个选定地址的金额中等额扣除。\n”
            “这些收件人将收到比您在其相应金额字段中输入的比特币更少的比特币。\n”
            “如果此处未指定地址，则发件人将支付费用。”，
                        {
                            “地址”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "", "Subtract fee from this address"},

                        },
                    },
                    /*eplaceable“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”回退到wallet的默认值“，”允许通过bip 125将此交易替换为收费较高的交易”，
                    “conf_target”，rpcarg:：type：：num，/*opt*/ true, /* default_val */ "fallback to wallet's default", "Confirmation target (in blocks)"},

                    /*stimate_模式“，rpcarg:：type:：str，/*opt*/true，/*default_val*/”unset“，”费用估算模式，必须是以下之一：\n”
            “取消设置\ \ \n”
            “经济型\ \ \ \n”
            “保守派”
                }
                toSTRIN（）+
             “\NESRES:\N”
            “\”txid\“（string）发送的事务ID。不管\n“
            “地址数。\n”
            “\n实例：\n”
            \n将两个数字发送到两个不同的地址：\n
            +helpExamplecli（“sendmany”，“\”“\”“\”“1d1zrzne3juo7zyckeyqqqawd9y54f4xx\”：0.01，“\”“1353tse8ymta4euv7dguxgjnff9kpvvkhz\”：0.02 \”）+
            \n将两个数量发送到设置确认和注释的两个不同的地址：\n
            +helpExamplecli（“sendmany”，“\”“\”“\”“1d1zrzne3juo7zyckeyqqqawd9y54f4xx\”：0.01，“\”“1353tse8ymta4euv7dguxgjnff9kpvvkhz\”：0.02“6\”测试\”）+
            将两个金额发送到两个不同的地址，从金额中减去费用：\n
            
            “作为JSON-RPC调用\n”
            +helpExampleRPC（“sendmany”，“\”，“1d1zrzne3juo7zyckeyqqqqawd9y54f4xx\”：0.01，“1353tse8ymta4euv7dguxgjnff9kpvvkHz\”：0.02，“6”，“testing\”）
        ；

    //确保结果至少在最新块之前有效
    //在此之前，用户可能已经从另一个rpc命令获取了
    pwallet->blockUntilsynchedTocurrentchain（）；

    自动锁定的_chain=pwallet->chain（）.lock（）；
    锁（pwallet->cs_wallet）；

    如果（pwallet->getBroadcastTransactions（）&&！G-康曼）
        throw jsonrpcerror（rpc_client_p2p_disabled，“错误：缺少或禁用对等功能”）；
    }

    如果（！）请求.params[0].isNull（）&！request.params[0].get_str（）.empty（））
        throw jsonrpcerror（rpc_invalid_参数，“dummy value must be set to \”“”“）；
    }
    univalue sendto=request.params[1].get_obj（）；
    int nmindepth=1；
    如果（！）请求.params[2].isNull（））
        nmindepth=request.params[2].get_int（）；

    mapvalue_t mapvalue；
    如果（！）请求.params[3].isNull（）&！request.params[3].get_str（）.empty（））
        mapvalue[“comment”]=request.params[3].get_str（）；

    单值减法（单值：：varr）；
    如果（！）请求.params[4].isNull（））
        subtractfeefromamount=request.params[4].get_array（）；

    CCoincontrol硬币控制；
    如果（！）request.params[5].isNull（））
        coin_control.m_signal_bip125_rbf=request.params[5].get_bool（）；
    }

    如果（！）request.params[6].isNull（））
        coin_control.m_confirm_target=parseConfirmTarget（request.params[6]）；
    }

    如果（！）request.params[7].isNull（））
        如果（！）feemodefromstring（request.params[7].get_str（），coin_control.m_fee_mode））
            throw jsonrpcerror（rpc_invalid_参数，“invalid estimate_mode参数”）；
        }
    }

    std:：set<ctxdestination>destinations；
    std:：vector<crecipient>vecsend；

    camount总金额=0；
    std:：vector<std:：string>keys=sendto.getkeys（）；
    for（const std：：string&name_:keys）
        ctxdestination dest=解码目的地（name_uu）；
        如果（！）isvaliddestination（目的地））
            throw jsonrpcerror（rpc_invalid_address_或_key，std:：string（“无效比特币地址：”）+name_u）；
        }

        if（destinations.count（dest））
            throw jsonrpcerror（rpc_invalid_参数，std:：string（“无效参数，重复地址：”）+name_u）；
        }
        目的地。插入（dest）；

        cscript scriptPubkey=getscriptForDestination（目标）；
        camount namount=amountFromValue（发送到[名称]）；
        如果（n计数<=0）
            throw jsonrpcerror（rpc_type_error，“发送的金额无效”）；
        总金额+=金额；

        bool fsubtractfeefromomount=false；
        for（unsigned int idx=0；idx<subtractfeefromamount.size（）；idx++）
            const univalue&addr=减去feefromamount[idx]；
            if（addr.get_str（）==名称）
                fsubtractfeefromamount=真；
        }

        crecipient recipient=scriptpubkey，namount，fsubtractfeefromomamount
        vecsend.push_back（收件人）；
    }

    确保已解锁（PWallet）；

    /支票基金
    if（totalamount>pwallet->getlegacybalance（ismine_spenable，nmindepth））
        throw jsonrpcerror（rpc_wallet_insufficient_funds，“wallet没有足够的资金”）；
    }

    //随机播放收件人列表
    std:：shuffle（vecsend.begin（），vecsend.end（），fastRandomContext（））；

    /发送
    CReservekey keychange（pwallet）；
    camount nfeereequired=0；
    int nchangeposret=-1；
    std：：字符串strfailreason；
    ctransactionref发送；
    bool fcreated=pwallet->createTransaction（*locked_chain，vecsend，tx，keychange，nfeereequired，nchangeposret，strfailreason，coin_control）；
    如果（！）被创造的）
        抛出jsonrpcerror（rpc_wallet_insufficient_funds，strfailreason）；
    cvalidationState状态；
    如果（！）pwallet->commitTransaction（tx，std:：move（mapvalue），/*订单*/, keyChange, g_connman.get(), state)) {

        strFailReason = strprintf("Transaction commit failed:: %s", FormatStateMessage(state));
        throw JSONRPCError(RPC_WALLET_ERROR, strFailReason);
    }

    return tx->GetHash().GetHex();
}

static UniValue addmultisigaddress(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 2 || request.params.size() > 4) {
        std::string msg =
            RPCHelpMan{"addmultisigaddress",
                "\nAdd a nrequired-to-sign multisignature address to the wallet. Requires a new wallet backup.\n"
                "Each key is a Bitcoin address or hex-encoded public key.\n"
                "This functionality is only intended for use with non-watchonly addresses.\n"
                "See `importaddress` for watchonly p2sh address support.\n"
                "If 'label' is specified, assign address to that label.\n",
                {
                    /*“必需”，rpcarg:：type:：num，/*opt*/false，/*default_val*/“”，“N个键或地址中所需签名的数目。”，
                    “键”，rpcarg:：type:：arr，/*opt*/ false, /* default_val */ "", "A json array of bitcoin addresses or hex-encoded public keys",

                        {
                            /*ey“，rpcarg:：type:：str，/*opt*/false，/*默认值“/”，“比特币地址或十六进制编码的公钥”，
                        }
                        }
                    “label”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "null", "A label to assign the addresses to."},

                    /*地址“type”，rpcarg:：type:：str，/*opt*/true，/*默认值“set by-address type”，“要使用的地址类型”。选项有“传统”、“p2sh segwit”和“bech32”。
                }
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            “address\”：“multisig address\”，（string）新multisig地址的值。\n”
            “\”redeemscript\“：\”script\“（string）十六进制编码的赎回脚本的字符串值。\n”
            “}\n”
            “\n实例：\n”
            \n从2个地址中添加多组地址\n
            +helpExamplecli（“addmultisigaddress”，“2\”[\\\“16ssausf5p2ukuvkgqqjnrzbzyqgel5\”，\\\“171sgjn4ytpu27adkkgrdwzrtxnrkbfkv\”]\“”）+
            “作为JSON-RPC调用\n”
            +helpExampleRpc（“addmultisigaddress”，“2”，“2”，“16ssausf5p2ukuvkgqqjnrzbzyqgel5”，“171sgjn4ytpu27adkkgddwzrtxnrkbfkv\\\”“]”）
        ；
        throw std：：运行时出错（msg）；
    }

    自动锁定的_chain=pwallet->chain（）.lock（）；
    锁（pwallet->cs_wallet）；

    std：：字符串标签；
    如果（！）请求.params[2].isNull（））
        label=labelfromvalue（请求参数[2]）；

    int required=request.params[0].get_int（）；

    //获取公钥
    const univalue&keys_or_addrs=request.params[1].get_array（）；
    std:：vector<cpubkey>pubkeys；
    for（unsigned int i=0；i<keys_or_addrs.size（）；++i）
        if（ishex（keys_or_addrs[i].get_str（））&&（keys_or_addrs[i].get_str（）.length（）==66 keys_or_addrs[i].get_str（）.length（）==130））
            pubkeys.push_back（hextopubkey（keys_or_addrs[i].get_str（））；
        }否则{
            pubkeys.push_back（addrtopubkey（pwallet，keys_或_addrs[i].get_str（））；
        }
    }

    output type output_type=pwallet->m_default_address_type；
    如果（！）request.params[3].isNull（））
        如果（！）parseOutputType（request.params[3].get_str（），output_type））
            throw jsonrpcerror（rpc_invalid_address_or_key，strprintf（“未知地址类型'%s'”，request.params[3].get_str（））；
        }
    }

    //使用付薪脚本哈希构造：
    cscript inner=createMulsigRedeemscript（必需，pubKeys）；
    ctxdestination dest=addandgetdestinationforscript（*pwallet，inner，output_type）；
    pwallet->setaddressbook（dest，label，“send”）；

    单值结果（单值：：vobj）；
    结果.pushkv（“地址”，编码目的地（dest））；
    result.pushkv（“redeemscript”，hexstr（inner.begin（），inner.end（））；
    返回结果；
}

结构物项
{
    camount namount 0
    int ncoff std:：numeric_limits<int>：：max（）
    std:：vector<uint256>txids；
    bool fiswatchoonly假
    TalyItMe（）
    {
    }
}；

静态单值列表已接收（接口：：chain:：lock&locked\u chain，cwallet*const pwallet，const univalue&params，bool by_label）所需的专用锁（pwallet->cs_wallet）
{
    lockAnnotation lock（：：cs_main）；//临时，用于下面的checkFinalTx。在即将到来的提交中删除。

    //最低确认数
    int nmindepth=1；
    如果（！）参数[0].isNull（））
        nmindepth=params[0].获取int（）；

    //是否包含空标签
    bool fincludeempty=假；
    如果（！）参数[1].isNull（））
        fincludempty=params[1].获取bool（）；

    ismine filter filter=isminesuspenable；
    如果（！）参数[2].isNull（））
        if（参数[2].获取bool（））
            filter=filter_ismine_watch_only；

    bool has_filtered_address=false；
    ctxDestination filtered_address=cnotDestination（）；
    如果（！）按“label&&params.size（）>3）”；
        如果（！）isvalidDestinationString（params[3].get_str（））
            throw jsonrpcerror（rpc_wallet_error，“address_filter参数无效”）；
        }
        filtered_address=decodedestination（params[3].get_str（））；
        已过滤的地址为真；
    }

    /计数
    std:：map<ctxdestination，tallyitem>maptally；
    for（const std:：pair<const uint256，cwallettx>和pairwtx:pwallet->mapwallet）
        const cwallettx&wtx=pairwtx.second；

        如果（wtx.iscoinBase（）！检查最终x（*wtx.tx））
            继续；

        int ndepth=wtx.getDepthinMainchain（锁定的_链）；
        如果（ndepth<nmindepth）
            继续；

        for（const ctxout&txout:wtx.tx->vout）
        {
            CTX目标地址；
            如果（！）提取目标（txout.scriptpubkey，地址）
                继续；

            如果（已经过滤了地址&！（过滤后的_address==address））
                继续；
            }

            isminefilter mine=ismine（*pwallet，地址）；
            如果（！）（矿井和过滤器）
                继续；

            tallyitem&item=maptally[地址]；
            item.namount+=txout.n值；
            item.ncof=std：：min（item.ncof，ndepth）；
            item.txids.push_back（wtx.gethash（））；
            如果（仅限Mine&Ismine_Watch_）
                item.fiswatchoonly=真；
        }
    }

    /回复
    单值ret（单值：：varr）；
    std：：map<std：：string，tallyitem>label tally；

    //创建mapAddressBook迭代器
    //如果不进行筛选，请从begin（）转到end（）。
    auto start=pwallet->mapAddressBook.begin（）；
    auto end=pwallet->mapAddressBook.end（）；
    //如果我们正在筛选，find（）适用的条目
    如果（有过滤过的地址）
        start=pwallet->mapaddressbook.find（过滤后的地址）；
        如果（开始）！{结束）{
            结束=标准：：下一个（开始）；
        }
    }

    对于（auto item_it=start；item_it！）=结束；++项目\u it）
    {
        const ctxdestination&address=item_it->first；
        const std:：string&label=item\u it->second.name；
        auto it=maptally.find（地址）；
        如果（它==maptally.end（）&&！取消空）
            继续；

        camount namount=0；
        int ncoff=std:：numeric_limits<int>：：max（）；
        bool fiswatchoonly=false；
        如果（它）！=maptally.end（））
        {
            namount=（*it）.second.namount；
            ncof=（*it）.second.ncof；
            fiswatchoonly=（*it）.second.fiswatchoonly；
        }

        如果（字节标记）
        {
            TALLYITEM&_ITEM=标签_TALLY[标签]；
            _item.namount+=namount；
            _item.ncof=std:：min（_item.ncof，ncof）；
            _item.fiswatchoonly=fiswatchoonly；
        }
        其他的
        {
            单值对象（单值：：vobj）；
            如果（Fiswatchoonly）
                obj.pushkv（“InvolvesWatchOnly”，真）；
            对象pushkv（“地址”，编码目的地（地址））；
            对象pushkv（“金额”，valuefromamount（namount））；
            obj.pushkv（“确认”，（ncoff==std:：numeric_limits<int>：：max（）？0：NCONF）；
            对象pushkv（“标签”，标签）；
            单值事务（单值：：varr）；
            如果（它）！=maptally.end（））
            {
                for（const uint256&_item:（*it）.second.txids）
                {
                    transactions.push_back（_item.gethex（））；
                }
            }
            对象pushkv（“txids”，交易）；
            后退（obj）；
        }
    }

    如果（字节标记）
    {
        for（const auto&entry:标签计数）
        {
            camount namount=entry.second.namount；
            int ncof=entry.second.ncof；
            单值对象（单值：：vobj）；
            如果（entry.second.fiswatchoonly）
                obj.pushkv（“InvolvesWatchOnly”，真）；
            对象pushkv（“金额”，valuefromamount（namount））；
            obj.pushkv（“确认”，（ncoff==std:：numeric_limits<int>：：max（）？0：NCONF）；
            对象pushkv（“label”，entry.first）；
            后退（obj）；
        }
    }

    返回RET；
}

静态单值ListReceivedByAddress（const jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    if（request.fhelp request.params.size（）>4）
        throw std:：runtime_错误（
            rpchelpman“ListReceivedByAddress”，
                “\n按接收地址列出余额。\n”，
                {
                    “minconf”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "1", "The minimum number of confirmations before payments are included."},

                    /*nclude_empty“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/“false”，“是否包括未收到任何付款的地址。”，
                    “include_watchonly”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "false", "Whether to include watch-only addresses (see 'importaddress')."},

                    /*地址_filter“，rpcarg:：type:：str，/*opt*/true，/*默认值_val*/”null“，”如果存在，仅返回此地址的信息。“，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “\n”
            “{n”
            “involvesWatchOnly”：true，（bool）仅在导入的地址涉及事务时返回\n”
            “\”地址\“：\”接收地址\“，（字符串）接收地址\n”
            “amount\”：x.x x x，（数字）地址收到的以“+货币\单位+”表示的总金额\n”
            “\”确认\“：n，（数字）包含的最近交易的确认数\n”
            “\”label\“：\”label\“，（string）接收地址的标签。默认标签为\“0”。\n
            “txids\”：[\n“
            ““txid\”，（string）使用地址接收的事务的ID\n”
            “……n”
            “\n”
            “}\n”
            “……\n”
            “\n”

            “\n实例：\n”
            +helpexamplecli（“lisreceivedbyaddress”，“”）
            +helpexamplecli（“lisreceivedbyaddress”，“6 true”）。
            +helpExampleRpc（“listReceivedByAddress”，“6，true，true”）。
            +helpExampleRpc（“listReceivedByAddress”，“6，true，true”，“1M72sfpbz1bpxfhz9m3cdqatr44jvayd\”）
        ；

    //确保结果至少在最新块之前有效
    //在此之前，用户可能已经从另一个rpc命令获取了
    pwallet->blockUntilsynchedTocurrentchain（）；

    自动锁定的_chain=pwallet->chain（）.lock（）；
    锁（pwallet->cs_wallet）；

    已接收返回列表（*locked_chain，pwallet，request.params，false）；
}

静态单值ListReceiveDBylabel（const-jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    if（request.fhelp request.params.size（）>3）
        throw std:：runtime_错误（
            rpchelpman“lisreceivedbylabel”，
                “\n按标签列出已接收的事务。\n”，
                {
                    “minconf”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "1", "The minimum number of confirmations before payments are included."},

                    /*包括“空”，rpcarg:：type:：bool，/*opt*/true，/*default“假”，“是否包括未收到任何付款的标签。”，
                    “include_watchonly”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "false", "Whether to include watch-only addresses (see 'importaddress')."},

                }}
                .ToString() +
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : true,   (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"amount\" : x.xxx,             (numeric) The total amount received by addresses with this label\n"
            "    \"confirmations\" : n,          (numeric) The number of confirmations of the most recent transaction included\n"
            "    \"label\" : \"label\"           (string) The label of the receiving address. The default label is \"\".\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbylabel", "")
            + HelpExampleCli("listreceivedbylabel", "6 true")
            + HelpExampleRpc("listreceivedbylabel", "6, true, true")
        );

//确保结果至少在最新块之前有效
//在此之前，用户可能已经从另一个rpc命令获得了
    pwallet->BlockUntilSyncedToCurrentChain();

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    return ListReceived(*locked_chain, pwallet, request.params, true);
}

static void MaybePushAddress(UniValue & entry, const CTxDestination &dest)
{
    if (IsValidDestination(dest)) {
        entry.pushKV("address", EncodeDestination(dest));
    }
}

/*
 *根据给定标准列出交易。
 *
 *@param从钱包中取出。
 *@param wtx钱包交易。
 *@param nmindepth最小确认深度。
 *@param flong是否包含事务的JSON版本。
 *@param ret存储结果的单值。
 *@param filter_是“is mine”筛选器标志。
 *@param filter_label可选标签字符串，用于筛选传入事务。
 **/

static void ListTransactions(interfaces::Chain::Lock& locked_chain, CWallet* const pwallet, const CWalletTx& wtx, int nMinDepth, bool fLong, UniValue& ret, const isminefilter& filter_ismine, const std::string* filter_label)
{
    CAmount nFee;
    std::list<COutputEntry> listReceived;
    std::list<COutputEntry> listSent;

    wtx.GetAmounts(listReceived, listSent, nFee, filter_ismine);

    bool involvesWatchonly = wtx.IsFromMe(ISMINE_WATCH_ONLY);

//发送
    if (!filter_label)
    {
        for (const COutputEntry& s : listSent)
        {
            UniValue entry(UniValue::VOBJ);
            if (involvesWatchonly || (::IsMine(*pwallet, s.destination) & ISMINE_WATCH_ONLY)) {
                entry.pushKV("involvesWatchonly", true);
            }
            MaybePushAddress(entry, s.destination);
            entry.pushKV("category", "send");
            entry.pushKV("amount", ValueFromAmount(-s.amount));
            if (pwallet->mapAddressBook.count(s.destination)) {
                entry.pushKV("label", pwallet->mapAddressBook[s.destination].name);
            }
            entry.pushKV("vout", s.vout);
            entry.pushKV("fee", ValueFromAmount(-nFee));
            if (fLong)
                WalletTxToJSON(pwallet->chain(), locked_chain, wtx, entry);
            entry.pushKV("abandoned", wtx.isAbandoned());
            ret.push_back(entry);
        }
    }

//收到
    if (listReceived.size() > 0 && wtx.GetDepthInMainChain(locked_chain) >= nMinDepth)
    {
        for (const COutputEntry& r : listReceived)
        {
            std::string label;
            if (pwallet->mapAddressBook.count(r.destination)) {
                label = pwallet->mapAddressBook[r.destination].name;
            }
            if (filter_label && label != *filter_label) {
                continue;
            }
            UniValue entry(UniValue::VOBJ);
            if (involvesWatchonly || (::IsMine(*pwallet, r.destination) & ISMINE_WATCH_ONLY)) {
                entry.pushKV("involvesWatchonly", true);
            }
            MaybePushAddress(entry, r.destination);
            if (wtx.IsCoinBase())
            {
                if (wtx.GetDepthInMainChain(locked_chain) < 1)
                    entry.pushKV("category", "orphan");
                else if (wtx.IsImmatureCoinBase(locked_chain))
                    entry.pushKV("category", "immature");
                else
                    entry.pushKV("category", "generate");
            }
            else
            {
                entry.pushKV("category", "receive");
            }
            entry.pushKV("amount", ValueFromAmount(r.amount));
            if (pwallet->mapAddressBook.count(r.destination)) {
                entry.pushKV("label", label);
            }
            entry.pushKV("vout", r.vout);
            if (fLong)
                WalletTxToJSON(pwallet->chain(), locked_chain, wtx, entry);
            ret.push_back(entry);
        }
    }
}

UniValue listtransactions(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 4)
        throw std::runtime_error(
            RPCHelpMan{"listtransactions",
                "\nIf a label name is provided, this will return only incoming transactions paying to addresses with the specified label.\n"
                "\nReturns up to 'count' most recent transactions skipping the first 'from' transactions.\n",
                {
                    /*abel“，rpcarg:：type:：str，/*opt*/true，/*default_val*/”null“，”如果设置，则应为有效的标签名称，以仅返回传入事务\n”
            “使用指定的标签，或\”*\“禁用筛选并返回所有事务。”，
                    “计数”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "10", "The number of transactions to return"},

                    /*kip“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”0“，要跳过的事务数”，
                    “include_watchonly”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "false", "Include transactions to watch-only addresses (see 'importaddress')"},

                }}
                .ToString() +
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"address\":\"address\",    (string) The bitcoin address of the transaction.\n"
            "    \"category\":               (string) The transaction category.\n"
            "                \"send\"                  Transactions sent.\n"
            "                \"receive\"               Non-coinbase transactions received.\n"
            "                \"generate\"              Coinbase transactions received with more than 100 confirmations.\n"
            "                \"immature\"              Coinbase transactions received with 100 or fewer confirmations.\n"
            "                \"orphan\"                Orphaned coinbase transactions received.\n"
            "    \"amount\": x.xxx,          (numeric) The amount in " + CURRENCY_UNIT + ". This is negative for the 'send' category, and is positive\n"
            "                                        for all other categories\n"
            "    \"label\": \"label\",       (string) A comment for the address/transaction, if any\n"
            "    \"vout\": n,                (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) The amount of the fee in " + CURRENCY_UNIT + ". This is negative and only available for the \n"
            "                                         'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction. Negative confirmations indicate the\n"
            "                                         transaction conflicts with the block chain\n"
            "    \"trusted\": xxx,           (bool) Whether we consider the outputs of this unconfirmed transaction safe to spend.\n"
            "    \"blockhash\": \"hashvalue\", (string) The block hash containing the transaction.\n"
            "    \"blockindex\": n,          (numeric) The index of the transaction in the block that includes it.\n"
            "    \"blocktime\": xxx,         (numeric) The block time in seconds since epoch (1 Jan 1970 GMT).\n"
            "    \"txid\": \"transactionid\", (string) The transaction id.\n"
            "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
            "    \"bip125-replaceable\": \"yes|no|unknown\",  (string) Whether this transaction could be replaced due to BIP125 (replace-by-fee);\n"
            "                                                     may be unknown for unconfirmed transactions not in the mempool\n"
            "    \"abandoned\": xxx          (bool) 'true' if the transaction has been abandoned (inputs are respendable). Only available for the \n"
            "                                         'send' category of transactions.\n"
            "  }\n"
            "]\n"

            "\nExamples:\n"
            "\nList the most recent 10 transactions in the systems\n"
            + HelpExampleCli("listtransactions", "") +
            "\nList transactions 100 to 120\n"
            + HelpExampleCli("listtransactions", "\"*\" 20 100") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("listtransactions", "\"*\", 20, 100")
        );

//确保结果至少在最新块之前有效
//在此之前，用户可能已经从另一个rpc命令获得了
    pwallet->BlockUntilSyncedToCurrentChain();

    const std::string* filter_label = nullptr;
    if (!request.params[0].isNull() && request.params[0].get_str() != "*") {
        filter_label = &request.params[0].get_str();
        if (filter_label->empty()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Label argument must be a valid label name or \"*\".");
        }
    }
    int nCount = 10;
    if (!request.params[1].isNull())
        nCount = request.params[1].get_int();
    int nFrom = 0;
    if (!request.params[2].isNull())
        nFrom = request.params[2].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if(!request.params[3].isNull())
        if(request.params[3].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    UniValue ret(UniValue::VARR);

    {
        auto locked_chain = pwallet->chain().lock();
        LOCK(pwallet->cs_wallet);

        const CWallet::TxItems & txOrdered = pwallet->wtxOrdered;

//向后迭代，直到我们有ncount项返回：
        for (CWallet::TxItems::const_reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
        {
            CWalletTx *const pwtx = (*it).second;
            ListTransactions(*locked_chain, pwallet, *pwtx, 0, true, ret, filter, filter_label);
            if ((int)ret.size() >= (nCount+nFrom)) break;
        }
    }

//RET从最新到最旧

    if (nFrom > (int)ret.size())
        nFrom = ret.size();
    if ((nFrom + nCount) > (int)ret.size())
        nCount = ret.size() - nFrom;

    std::vector<UniValue> arrTmp = ret.getValues();

    std::vector<UniValue>::iterator first = arrTmp.begin();
    std::advance(first, nFrom);
    std::vector<UniValue>::iterator last = arrTmp.begin();
    std::advance(last, nFrom+nCount);

    if (last != arrTmp.end()) arrTmp.erase(last, arrTmp.end());
    if (first != arrTmp.begin()) arrTmp.erase(arrTmp.begin(), first);

std::reverse(arrTmp.begin(), arrTmp.end()); //从旧到新返回

    ret.clear();
    ret.setArray();
    ret.push_backV(arrTmp);

    return ret;
}

static UniValue listsinceblock(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 4)
        throw std::runtime_error(
            RPCHelpMan{"listsinceblock",
                "\nGet all transactions in blocks since block [blockhash], or all transactions if omitted.\n"
                "If \"blockhash\" is no longer a part of the main chain, transactions from the fork point onward are included.\n"
                "Additionally, if include_removed is set, transactions affecting the wallet which were removed are returned in the \"removed\" array.\n",
                {
                    /*lockhash“，rpcarg:：type:：str，/*opt*/true，/*default_val*/”null“，”如果设置了，则块hash将列出自以来的事务，否则列出所有事务。“，
                    “目标_确认”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "1", "Return the nth block hash from the main chain. e.g. 1 would mean the best block hash. Note: this is not used as a filter, but only affects [lastblock] in the return value"},

                    /*nclude_watch only“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”false“，”包括只监视地址的事务（请参见'importaddress'）”，
                    “include_removed”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "true", "Show transactions that were removed due to a reorg in the \"removed\" array\n"

            "                                                           (not guaranteed to work on pruned nodes)"},
                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"transactions\": [\n"
            "    \"address\":\"address\",    (string) The bitcoin address of the transaction.\n"
            "    \"category\":               (string) The transaction category.\n"
            "                \"send\"                  Transactions sent.\n"
            "                \"receive\"               Non-coinbase transactions received.\n"
            "                \"generate\"              Coinbase transactions received with more than 100 confirmations.\n"
            "                \"immature\"              Coinbase transactions received with 100 or fewer confirmations.\n"
            "                \"orphan\"                Orphaned coinbase transactions received.\n"
            "    \"amount\": x.xxx,          (numeric) The amount in " + CURRENCY_UNIT + ". This is negative for the 'send' category, and is positive\n"
            "                                         for all other categories\n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) The amount of the fee in " + CURRENCY_UNIT + ". This is negative and only available for the 'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction.\n"
            "                                          When it's < 0, it means the transaction conflicted that many blocks ago.\n"
            "    \"blockhash\": \"hashvalue\",     (string) The block hash containing the transaction.\n"
            "    \"blockindex\": n,          (numeric) The index of the transaction in the block that includes it.\n"
            "    \"blocktime\": xxx,         (numeric) The block time in seconds since epoch (1 Jan 1970 GMT).\n"
            "    \"txid\": \"transactionid\",  (string) The transaction id.\n"
            "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (Jan 1 1970 GMT).\n"
            "    \"bip125-replaceable\": \"yes|no|unknown\",  (string) Whether this transaction could be replaced due to BIP125 (replace-by-fee);\n"
            "                                                   may be unknown for unconfirmed transactions not in the mempool\n"
            "    \"abandoned\": xxx,         (bool) 'true' if the transaction has been abandoned (inputs are respendable). Only available for the 'send' category of transactions.\n"
            "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
            "    \"label\" : \"label\"       (string) A comment for the address/transaction, if any\n"
            "    \"to\": \"...\",            (string) If a comment to is associated with the transaction.\n"
            "  ],\n"
            "  \"removed\": [\n"
            "    <structure is the same as \"transactions\" above, only present if include_removed=true>\n"
            "    Note: transactions that were re-added in the active chain will appear as-is in this array, and may thus have a positive confirmation count.\n"
            "  ],\n"
            "  \"lastblock\": \"lastblockhash\"     (string) The hash of the block (target_confirmations-1) from the best block on the main chain. This is typically used to feed back into listsinceblock the next time you call it. So you would generally use a target_confirmations of say 6, so you will be continually re-notified of transactions until they've reached 6 confirmations plus any new ones\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("listsinceblock", "")
            + HelpExampleCli("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\" 6")
            + HelpExampleRpc("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\", 6")
        );

//确保结果至少在最新块之前有效
//在此之前，用户可能已经从另一个rpc命令获得了
    pwallet->BlockUntilSyncedToCurrentChain();

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

const CBlockIndex* pindex = nullptr;    //指定块或公共祖先的块索引（如果提供的块位于已停用的链中）。
const CBlockIndex* paltindex = nullptr; //指定块的块索引，即使它位于已停用的链中。
    int target_confirms = 1;
    isminefilter filter = ISMINE_SPENDABLE;

    if (!request.params[0].isNull() && !request.params[0].get_str().empty()) {
        uint256 blockId(ParseHashV(request.params[0], "blockhash"));

        paltindex = pindex = LookupBlockIndex(blockId);
        if (!pindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
        if (chainActive[pindex->nHeight] != pindex) {
//被请求的块是停用链的一部分；
//我们不想依赖它在街区内的感知高度
//链，我们想用最后一个共同的祖先
            pindex = chainActive.FindFork(pindex);
        }
    }

    if (!request.params[1].isNull()) {
        target_confirms = request.params[1].get_int();

        if (target_confirms < 1) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter");
        }
    }

    if (!request.params[2].isNull() && request.params[2].get_bool()) {
        filter = filter | ISMINE_WATCH_ONLY;
    }

    bool include_removed = (request.params[3].isNull() || request.params[3].get_bool());

    int depth = pindex ? (1 + chainActive.Height() - pindex->nHeight) : -1;

    UniValue transactions(UniValue::VARR);

    for (const std::pair<const uint256, CWalletTx>& pairWtx : pwallet->mapWallet) {
        CWalletTx tx = pairWtx.second;

        if (depth == -1 || tx.GetDepthInMainChain(*locked_chain) < depth) {
            /*ttransactions（*锁定的_-chain、pwallet、tx、0、true、事务、筛选器、nullptr/*筛选器_-label*/）；
        }
    }

    //当请求REORG'D块时，我们还列出所有相关的事务
    //在分离的链块中
    删除的单值（单值：：varr）；
    while（包括删除的paltindex和paltindex！= pQueD）{
        块块；
        如果（！）readblockfromdisk（block，paltindex，params（）.getconsensus（））
            throw jsonrpcerror（rpc_internal_error，“can't read block from disk”）；
        }
        对于（const ctransactionref&tx:block.vtx）
            auto it=pwallet->mapwallet.find（tx->gethash（））；
            如果（它）！=pwallet->mapwallet.end（））
                //我们希望所有事务（不管确认计数如何）都显示在这里，
                //即使是否定的确认，也会有很大的否定。
                listTransactions（*锁定的_链，pwallet，it->second，-100000000，true，removed，filter，nullptr/*filter_标签*/);

            }
        }
        paltindex = paltindex->pprev;
    }

    CBlockIndex *pblockLast = chainActive[chainActive.Height() + 1 - target_confirms];
    uint256 lastblock = pblockLast ? pblockLast->GetBlockHash() : uint256();

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("transactions", transactions);
    if (include_removed) ret.pushKV("removed", removed);
    ret.pushKV("lastblock", lastblock.GetHex());

    return ret;
}

static UniValue gettransaction(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            RPCHelpMan{"gettransaction",
                "\nGet detailed information about in-wallet transaction <txid>\n",
                {
                    /*xid“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，“事务ID”，
                    “include_watchonly”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "false", "Whether to include watch-only addresses in balance calculation and details[]"},

                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"amount\" : x.xxx,        (numeric) The transaction amount in " + CURRENCY_UNIT + "\n"
            "  \"fee\": x.xxx,            (numeric) The amount of the fee in " + CURRENCY_UNIT + ". This is negative and only available for the \n"
            "                              'send' category of transactions.\n"
            "  \"confirmations\" : n,     (numeric) The number of confirmations\n"
            "  \"blockhash\" : \"hash\",  (string) The block hash\n"
            "  \"blockindex\" : xx,       (numeric) The index of the transaction in the block that includes it\n"
            "  \"blocktime\" : ttt,       (numeric) The time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"txid\" : \"transactionid\",   (string) The transaction id.\n"
            "  \"time\" : ttt,            (numeric) The transaction time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"timereceived\" : ttt,    (numeric) The time received in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"bip125-replaceable\": \"yes|no|unknown\",  (string) Whether this transaction could be replaced due to BIP125 (replace-by-fee);\n"
            "                                                   may be unknown for unconfirmed transactions not in the mempool\n"
            "  \"details\" : [\n"
            "    {\n"
            "      \"address\" : \"address\",          (string) The bitcoin address involved in the transaction\n"
            "      \"category\" :                      (string) The transaction category.\n"
            "                   \"send\"                  Transactions sent.\n"
            "                   \"receive\"               Non-coinbase transactions received.\n"
            "                   \"generate\"              Coinbase transactions received with more than 100 confirmations.\n"
            "                   \"immature\"              Coinbase transactions received with 100 or fewer confirmations.\n"
            "                   \"orphan\"                Orphaned coinbase transactions received.\n"
            "      \"amount\" : x.xxx,                 (numeric) The amount in " + CURRENCY_UNIT + "\n"
            "      \"label\" : \"label\",              (string) A comment for the address/transaction, if any\n"
            "      \"vout\" : n,                       (numeric) the vout value\n"
            "      \"fee\": x.xxx,                     (numeric) The amount of the fee in " + CURRENCY_UNIT + ". This is negative and only available for the \n"
            "                                           'send' category of transactions.\n"
            "      \"abandoned\": xxx                  (bool) 'true' if the transaction has been abandoned (inputs are respendable). Only available for the \n"
            "                                           'send' category of transactions.\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"hex\" : \"data\"         (string) Raw data for transaction\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" true")
            + HelpExampleRpc("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

//确保结果至少在最新块之前有效
//在此之前，用户可能已经从另一个rpc命令获得了
    pwallet->BlockUntilSyncedToCurrentChain();

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    uint256 hash(ParseHashV(request.params[0], "txid"));

    isminefilter filter = ISMINE_SPENDABLE;
    if(!request.params[1].isNull())
        if(request.params[1].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    UniValue entry(UniValue::VOBJ);
    auto it = pwallet->mapWallet.find(hash);
    if (it == pwallet->mapWallet.end()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    }
    const CWalletTx& wtx = it->second;

    CAmount nCredit = wtx.GetCredit(*locked_chain, filter);
    CAmount nDebit = wtx.GetDebit(filter);
    CAmount nNet = nCredit - nDebit;
    CAmount nFee = (wtx.IsFromMe(filter) ? wtx.tx->GetValueOut() - nDebit : 0);

    entry.pushKV("amount", ValueFromAmount(nNet - nFee));
    if (wtx.IsFromMe(filter))
        entry.pushKV("fee", ValueFromAmount(nFee));

    WalletTxToJSON(pwallet->chain(), *locked_chain, wtx, entry);

    UniValue details(UniValue::VARR);
    /*ttransactions（*锁定的_-chain、pwallet、wtx、0、false、details、filter、nullptr/*filter_-label*/）；
    条目.pushkv（“细节”，细节）；

    std:：string strhex=encodeHexTx（*wtx.tx，rpcserializationFlags（））；
    entry.pushkv（“十六进制”，strhex）；

    返回入口；
}

静态单值abandonTransaction（const-jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    如果（request.fhelp request.params.size（）！= 1）{
        throw std:：runtime_错误（
            rpchelpman“放弃事务”，
                “\n在wallet transaction<txid>中标记为已放弃\n”
                “这会将此交易及其在钱包中的所有后代标记为已放弃，这将允许\n”
                “让他们的投入得到响应。它可用于替换“卡住”或收回的事务。\n
                “它仅适用于不包含在块中且当前不在mempool中的事务。\n”
                “它对已经放弃的事务没有影响。\n”，
                {
                    “txid”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "The transaction id"},

                }}
                .ToString() +
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("abandontransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleRpc("abandontransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );
    }

//确保结果至少在最新块之前有效
//在此之前，用户可能已经从另一个rpc命令获得了
    pwallet->BlockUntilSyncedToCurrentChain();

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    uint256 hash(ParseHashV(request.params[0], "txid"));

    if (!pwallet->mapWallet.count(hash)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    }
    if (!pwallet->AbandonTransaction(*locked_chain, hash)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction not eligible for abandonment");
    }

    return NullUniValue;
}


static UniValue backupwallet(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"backupwallet",
                "\nSafely copies current wallet file to destination, which can be a directory or a path with filename.\n",
                {
                    /*estination“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，”目标目录或文件“，
                }
                toSTRIN（）+
            “\n实例：\n”
            +helpExamplecli（“备份钱包”，“backup.dat\”）
            +helpExampleRPC（“备份钱包”，“backup.dat\”）
        ；

    //确保结果至少在最新块之前有效
    //在此之前，用户可能已经从另一个rpc命令获取了
    pwallet->blockUntilsynchedTocurrentchain（）；

    自动锁定的_chain=pwallet->chain（）.lock（）；
    锁（pwallet->cs_wallet）；

    std:：string strdest=request.params[0].get_str（）；
    如果（！）pwallet->backupwallet（strdest））
        throw jsonrpcerror（rpc_wallet_错误，“错误：wallet备份失败！”）；
    }

    返回nullunivalue；
}


静态单值keypoolrefill（const-jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    if（request.fhelp request.params.size（）>1）
        throw std:：runtime_错误（
            rpchelpman“密钥池定义”，
                \n修复密钥池。“+
                    帮助要求密码（pwallet）+\n“，
                {
                    “新闻大小”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "100", "The new keypool size"},

                }}
                .ToString() +
            "\nExamples:\n"
            + HelpExampleCli("keypoolrefill", "")
            + HelpExampleRpc("keypoolrefill", "")
        );

    if (pwallet->IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Private keys are disabled for this wallet");
    }

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

//TopupKeyPool（）将0解释为-KeyPool给定的默认密钥池大小
    unsigned int kpSize = 0;
    if (!request.params[0].isNull()) {
        if (request.params[0].get_int() < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected valid size.");
        kpSize = (unsigned int)request.params[0].get_int();
    }

    EnsureWalletIsUnlocked(pwallet);
    pwallet->TopUpKeyPool(kpSize);

    if (pwallet->GetKeyPoolSize() < kpSize) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error refreshing keypool.");
    }

    return NullUniValue;
}


static UniValue walletpassphrase(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
            RPCHelpMan{"walletpassphrase",
                "\nStores the wallet decryption key in memory for 'timeout' seconds.\n"
                "This is needed prior to performing transactions related to private keys such as sending bitcoins\n",
                {
                    /*assphrase“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，“钱包密码”，
                    “超时”，rpcarg:：type:：num，/*opt*/ false, /* default_val */ "", "The time to keep the decryption key in seconds; capped at 100000000 (~3 years)."},

                }}
                .ToString() +
            "\nNote:\n"
            "Issuing the walletpassphrase command while the wallet is already unlocked will set a new unlock\n"
            "time that overrides the old one.\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 60 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 60") +
            "\nLock the wallet again (before 60 seconds)\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("walletpassphrase", "\"my pass phrase\", 60")
        );
    }

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    if (!pwallet->IsCrypted()) {
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrase was called.");
    }

//注意，walletpassphrase存储在request.params[0]中，而不是mlock（）ed。
    SecureString strWalletPass;
    strWalletPass.reserve(100);
//TODO:通过实现secureString：：operator=（std：：string）来消除此.c_str（）。
//或者，找到一种方法让request.params[0]mlock（）以开头。
    strWalletPass = request.params[0].get_str().c_str();

//获得超时
    int64_t nSleepTime = request.params[1].get_int64();
//超时不能为负，否则将立即重新锁定
    if (nSleepTime < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Timeout cannot be negative.");
    }
//箝位超时
constexpr int64_t MAX_SLEEP_TIME = 100000000; //较大的值会触发macos/libevent错误？
    if (nSleepTime > MAX_SLEEP_TIME) {
        nSleepTime = MAX_SLEEP_TIME;
    }

    if (strWalletPass.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "passphrase can not be empty");
    }

    if (!pwallet->Unlock(strWalletPass)) {
        throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");
    }

    pwallet->TopUpKeyPool();

    pwallet->nRelockTime = GetTime() + nSleepTime;

//保持一个指向钱包的弱指针，以便可以卸载
//调用以下回调之前的钱包。如果有效的共享指针
//在回调中获取，则钱包仍处于加载状态。
    std::weak_ptr<CWallet> weak_wallet = wallet;
    RPCRunLater(strprintf("lockwallet(%s)", pwallet->GetName()), [weak_wallet] {
        if (auto shared_wallet = weak_wallet.lock()) {
            LOCK(shared_wallet->cs_wallet);
            shared_wallet->Lock();
            shared_wallet->nRelockTime = 0;
        }
    }, nSleepTime);

    return NullUniValue;
}


static UniValue walletpassphrasechange(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
            RPCHelpMan{"walletpassphrasechange",
                "\nChanges the wallet passphrase from 'oldpassphrase' to 'newpassphrase'.\n",
                {
                    /*ldpassphrase“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，“当前密码”，
                    “newpassphrase”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The new passphrase"},

                }}
                .ToString() +
            "\nExamples:\n"
            + HelpExampleCli("walletpassphrasechange", "\"old one\" \"new one\"")
            + HelpExampleRpc("walletpassphrasechange", "\"old one\", \"new one\"")
        );
    }

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    if (!pwallet->IsCrypted()) {
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrasechange was called.");
    }

//TODO:通过实现secureString:：operator=（std:：string）来消除这些.c_str（）调用。
//或者，找到一种方法让request.params[0]mlock（）以开头。
    SecureString strOldWalletPass;
    strOldWalletPass.reserve(100);
    strOldWalletPass = request.params[0].get_str().c_str();

    SecureString strNewWalletPass;
    strNewWalletPass.reserve(100);
    strNewWalletPass = request.params[1].get_str().c_str();

    if (strOldWalletPass.empty() || strNewWalletPass.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "passphrase can not be empty");
    }

    if (!pwallet->ChangeWalletPassphrase(strOldWalletPass, strNewWalletPass)) {
        throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");
    }

    return NullUniValue;
}


static UniValue walletlock(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            RPCHelpMan{"walletlock",
                "\nRemoves the wallet encryption key from memory, locking the wallet.\n"
                "After calling this method, you will need to call walletpassphrase again\n"
                "before being able to call any methods which require the wallet to be unlocked.\n",
                {}}
                .ToString() +
            "\nExamples:\n"
            "\nSet the passphrase for 2 minutes to perform a transaction\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 120") +
            "\nPerform a send (requires passphrase set)\n"
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 1.0") +
            "\nClear the passphrase since we are done before 2 minutes is up\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("walletlock", "")
        );
    }

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    if (!pwallet->IsCrypted()) {
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletlock was called.");
    }

    pwallet->Lock();
    pwallet->nRelockTime = 0;

    return NullUniValue;
}


static UniValue encryptwallet(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"encryptwallet",
                "\nEncrypts the wallet with 'passphrase'. This is for first time encryption.\n"
                "After this, any calls that interact with private keys such as sending or signing \n"
                "will require the passphrase to be set prior the making these calls.\n"
                "Use the walletpassphrase call for this, and then walletlock call.\n"
                "If the wallet is already encrypted, use the walletpassphrasechange call.\n",
                {
                    /*assphrase“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，”用于加密钱包的密码短语。它必须至少为1个字符，但应为长。“，
                }
                toSTRIN（）+
            “\n实例：\n”
            \n加密您的钱包\n
            +helpExamplecli（“加密钱包”，“我的密码短语”）+
            \n不将密码短语设置为使用钱包，例如用于签名或发送比特币\n
            +helpexamplecli（“walletpassphrase”，“\”我的密码短语\“”）+
            “不，我们可以做类似签名的事情\n”
            +helpexamplecli（“signmessage”，“\”address\“\”test message\”“）+
            \n不要通过删除密码短语再次锁定钱包\n
            +helpExamplecli（“walletlock”，“）+
            “作为JSON-RPC调用\n”
            +helpExampleRpc（“加密钱包”，“我的密码短语”）。
        ；
    }

    自动锁定的_chain=pwallet->chain（）.lock（）；
    锁（pwallet->cs_wallet）；

    if（pwallet->iscrypted（））
        throw jsonrpcerror（rpc_wallet_wrong_enc_state，“错误：使用加密钱包运行，但调用了encryptwallet”）；
    }

    //TODO:通过实现secureString：：operator=（std：：string）来除去此.c_str（）。
    //或者，找到一种方法以发出请求。params[0]mlock（）开头。
    安全字符串strwalletpass；
    strwalletpass.储备（100）；
    strwalletpass=request.params[0].get_str（）.c_str（）；

    if（strwalletpass.empty（））
        throw jsonrpcerror（rpc_invalid_参数，“passphrase不能为空”）；
    }

    如果（！）pwallet->encryptwallet（strwalletpass））
        throw jsonrpcerror（rpc_wallet_encryption_failed，“错误：加密钱包失败”）；
    }

    return“钱包已加密；密钥池已刷新，并生成新的HD种子（如果您使用的是HD）。您需要做一个新的备份。”；
}

静态单值锁定（const-jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    if（request.fhelp request.params.size（）<1 request.params.size（）>2）
        throw std:：runtime_错误（
            rpchelpman“锁扣”，
                \n更新暂时不可挂起的输出列表。\n
                “临时锁定（unlock=false）或解锁（unlock=true）指定的事务输出。\n”
                “如果解锁时未指定事务输出，则所有当前锁定的事务输出都将解锁。\n”
                消费比特币时，自动硬币选择不会选择锁定的交易输出。\n
                “锁只存储在内存中。节点以零锁定输出和锁定输出列表开始\n“
                “当节点停止或失败时，总是清除（通过进程退出）。\n”
                “另请参阅listunspent调用\n”，
                {
                    “解锁”，rpcarg:：type:：bool，/*opt*/ false, /* default_val */ "", "Whether to unlock (true) or lock (false) the specified transactions"},

                    /*transactions“，rpcarg:：type:：arr，/*opt*/true，/*default_val*/”空数组“，”对象的JSON数组。每个对象的txid（字符串）vout（数字）。”，
                        {
                            “”，rpcarg:：type:：obj，/*opt*/ true, /* default_val */ "", "",

                                {
                                    /*xid“，rpcarg:：type:：str_hex，/*opt*/false，/*默认值_val*/”“，“事务ID”，
                                    “vout”，rpcarg:：type:：num，/*opt*/ false, /* default_val */ "", "The output number"},

                                },
                            },
                        },
                    },
                }}
                .ToString() +
            "\nResult:\n"
            "true|false    (boolean) Whether the command was successful or not\n"

            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("lockunspent", "false, \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"")
        );

//确保结果至少在最新块之前有效
//在此之前，用户可能已经从另一个rpc命令获得了
    pwallet->BlockUntilSyncedToCurrentChain();

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    RPCTypeCheckArgument(request.params[0], UniValue::VBOOL);

    bool fUnlock = request.params[0].get_bool();

    if (request.params[1].isNull()) {
        if (fUnlock)
            pwallet->UnlockAllCoins();
        return true;
    }

    RPCTypeCheckArgument(request.params[1], UniValue::VARR);

    const UniValue& output_params = request.params[1];

//首先创建并验证coutpoints。

    std::vector<COutPoint> outputs;
    outputs.reserve(output_params.size());

    for (unsigned int idx = 0; idx < output_params.size(); idx++) {
        const UniValue& o = output_params[idx].get_obj();

        RPCTypeCheckObj(o,
            {
                {"txid", UniValueType(UniValue::VSTR)},
                {"vout", UniValueType(UniValue::VNUM)},
            });

        const uint256 txid(ParseHashO(o, "txid"));
        const int nOutput = find_value(o, "vout").get_int();
        if (nOutput < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");
        }

        const COutPoint outpt(txid, nOutput);

        const auto it = pwallet->mapWallet.find(outpt.hash);
        if (it == pwallet->mapWallet.end()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, unknown transaction");
        }

        const CWalletTx& trans = it->second;

        if (outpt.n >= trans.tx->vout.size()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout index out of bounds");
        }

        if (pwallet->IsSpent(*locked_chain, outpt.hash, outpt.n)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected unspent output");
        }

        const bool is_locked = pwallet->IsLockedCoin(outpt.hash, outpt.n);

        if (fUnlock && !is_locked) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected locked output");
        }

        if (!fUnlock && is_locked) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, output already locked");
        }

        outputs.push_back(outpt);
    }

//自动为输出设置（未）锁定状态。
    for (const COutPoint& outpt : outputs) {
        if (fUnlock) pwallet->UnlockCoin(outpt);
        else pwallet->LockCoin(outpt);
    }

    return true;
}

static UniValue listlockunspent(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
            RPCHelpMan{"listlockunspent",
                "\nReturns list of temporarily unspendable outputs.\n"
                "See the lockunspent call to lock and unlock transactions for spending.\n",
                {}}
                .ToString() +
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"txid\" : \"transactionid\",     (string) The transaction id locked\n"
            "    \"vout\" : n                      (numeric) The vout value\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("listlockunspent", "")
        );

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    std::vector<COutPoint> vOutpts;
    pwallet->ListLockedCoins(vOutpts);

    UniValue ret(UniValue::VARR);

    for (const COutPoint& outpt : vOutpts) {
        UniValue o(UniValue::VOBJ);

        o.pushKV("txid", outpt.hash.GetHex());
        o.pushKV("vout", (int)outpt.n);
        ret.push_back(o);
    }

    return ret;
}

static UniValue settxfee(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 1) {
        throw std::runtime_error(
            RPCHelpMan{"settxfee",
                "\nSet the transaction fee per kB for this wallet. Overrides the global -paytxfee command line parameter.\n",
                {
                    /*mount“，rpcarg：：type：：amount，/*opt*/false，/*default_val*/”，“以“+货币单位+”/kb”表示的交易费，
                }
                toSTRIN（）+
            “\NESREST \N”
            “如果成功，true false（布尔值）返回true \n”
            “\n实例：\n”
            +helpExamplecli（“settxfee”，“0.00001”）。
            +helpExampleRPC（“settxfee”，“0.00001”）。
        ；
    }

    自动锁定的_chain=pwallet->chain（）.lock（）；
    锁（pwallet->cs_wallet）；

    camount namount=amountFromValue（request.params[0]）；
    cfeerate-tx-费率（金额1000）；
    如果（Tx_费率=0）
        //自动选择
    否则，如果（Tx_费率<：MinRelayTxFee）
        throw jsonrpcerror（rpc_无效的_参数，strprintf（“tx fee不能小于min relay tx fee（%s）”，：：min relay tx fee.tostring（））；
    否则，如果（Tx砗费率<pWallet->M砗Min砗费用）
        throw jsonrpcerror（rpc_无效的_参数，strprintf（“txfee不能小于wallet min fee（%s）”，pwallet->m_min_fee.toString（））；
    }

    pwallet->m_pay_tx_fee=tx_fee_rate；
    回归真实；
}

静态单值getwalletinfo（const jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    如果（request.fhelp request.params.size（）！= 0）
        throw std:：runtime_错误（
            rpchelpman“getwalletinfo”，
                “返回包含各种钱包状态信息的对象。\n”，
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            “wallet name\”：xxxxx，（string）钱包名称\n“
            ““WalletVersion\”：xxxxx，（数字）钱包版本\n”
            ““余额”：XXXXXXX，（数字）钱包确认的总余额，单位为”+货币单位+“”\n“
            “\”未确认余额\“：xxx，（数字）钱包未确认余额的总和，单位为“+货币\单位+”\n“
            “未到期余额”：XXXXXX，（数字）钱包未到期余额的总和，单位为“+货币单位+”\n“
            “txcount”：xxxxx，（数字）钱包中的交易总数\n”
            “\”key pool oldest\“：xxxx，（数字）密钥池中最早预生成密钥的时间戳（从unix epoch开始的秒数）。\n”
            “\”keypoolsize\“：XXXX，（数字）预生成的新密钥数（仅统计外部密钥）。\n”
            “\”keypoolsize_hd_internal\“：XXXX，（数字）为内部使用预先生成了多少个新密钥（用于更改输出，仅当钱包使用此功能时才显示，否则使用外部密钥）。\n”
            “\”解锁\u直到\“：ttt，（数字）从epoch（1970年1月1日午夜，格林威治标准时间）起以秒为单位的时间戳，钱包在传输时解锁，或者如果钱包被锁定，则为0 \n”
            “payTxFee\”：x.x x x x，（数字）以“+货币\单位+”/kb设置的交易费用配置\n”
            “\”hdseeid\“：\”<hash160>\“（字符串，可选）hd种子的hash160（仅在启用hd时存在）\n”
            hdseeid的“hdmasterkeyid\”：\“<hash160>\”（字符串，可选）别名，保留后向兼容性。将在v0.18中删除。\n“
            “私人\密钥\启用\”：如果此钱包禁用了私人密钥，则为真假（布尔值）假（强制仅监视钱包）\n”
            “}\n”
            “\n实例：\n”
            +helpExamplecli（“getwalletinfo”，“”）
            +helpexamplerpc（“getwalletinfo”，“”）
        ；

    //确保结果至少在最新块之前有效
    //在此之前，用户可能已经从另一个rpc命令获取了
    pwallet->blockUntilsynchedTocurrentchain（）；

    自动锁定的_chain=pwallet->chain（）.lock（）；
    锁（pwallet->cs_wallet）；

    单值对象（单值：：vobj）；

    size_t kpExternalsize=pWallet->keypoolCounterExternalKeys（）；
    obj.pushkv（“walletname”，pwallet->getname（））；
    obj.pushkv（“walletversion”，pwallet->getversion（））；
    obj.pushkv（“余额”，valuefromamount（pwallet->getbalance（））；
    obj.pushkv（“未确认余额”，valuefromamount（pwallet->getUnconfirmedBalance（））；
    obj.pushkv（“未到期余额”，valuefromamount（pwallet->getUndulideBalance（））；
    obj.pushkv（“txcount”，（int）pwallet->mapwallet.size（））；
    obj.pushkv（“keypoololdest”，pwallet->getoldestkeypooltime（））；
    对象pushkv（“keypoolsize”，（int64_t）kExternalsize）；
    ckeyid seed_id=pwallet->gethdchain（）.seed_id；
    如果（！）seed_id.isNull（）&&pWallet->canSupportFeature（feature_hd_split））
        obj.pushkv（“keypoolsize_hd_internal”，（int64_t）（pwallet->getkeypoolsize（）-kpExternalsize））；
    }
    if（pwallet->iscrypted（））
        对象pushkv（“解锁时间”，pwallet->nrelocktime）；
    }
    obj.pushkv（“pay tx fee”，valuefromamount（pwallet->m_pay_tx_fee.getfeeperk（））；
    如果（！）种子_id.isNull（））
        obj.pushkv（“hdseeid”，seed_id.gethex（））；
        obj.pushkv（“hdmasterkeyid”，seed_id.gethex（））；
    }
    obj.pushkv（“已启用专用钥匙”！pwallet->iswalletflagset（wallet_flag_disable_private_keys））；
    返回对象；
}

静态单值listwalletdir（const jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 0）{
        throw std:：runtime_错误（
            rpchelpman“Listwalletdir”，
                “返回钱包目录中的钱包列表。\n”，
                toSTRIN（）+
            “{n”
            “钱包”：[（json对象数组）\n”
            “{n”
            “\”name\“：\”name\“（string）钱包名称\n”
            “}\n”
            “……\n”
            “\n”
            “}\n”
            “\n实例：\n”
            +helpExamplecli（“listwalletdir”，“”）
            +helpExampleRPC（“listwalletdir”，“”）
        ；
    }

    单值钱包（单值：：varr）；
    for（const auto&path:listwalletdir（））
        单值钱包（单值：：vobj）；
        wallet.pushkv（“名称”，path.string（））；
        钱包。推回（钱包）；
    }

    单值结果（单值：：vobj）；
    结果.pushkv（“钱包”，钱包）；
    返回结果；
}

静态单值列表钱包（const jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 0）
        throw std:：runtime_错误（
            rpchelpman“列出钱包”，
                “返回当前加载的钱包列表。\n”
                “有关钱包的完整信息，请使用\”getwalletinfo \“\n”，
                {}
                toSTRIN（）+
            “\NESRES:\N”
            “[（字符串的JSON数组\n”
            “wallet name”（string）钱包名称\n”
            “……n”
            “\n”
            “\n实例：\n”
            +helpExamplecli（“列表钱包”，“”）
            +helpExampleRpc（“列表钱包”，“”）
        ；

    单值对象（单值：：varr）；

    for（const std:：shared_ptr<cwallet>&wallet:getwallets（））
        如果（！）EnsureWalletisAvailable（wallet.get（），request.fhelp））
            返回nullunivalue；
        }

        锁（钱包->cs_钱包）；

        obj.push_back（wallet->getname（））；
    }

    返回对象；
}

静态单值加载钱包（const jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 1）
        throw std:：runtime_错误（
            rpchelpman“载入钱包”，
                \n从钱包文件或目录中下载钱包。
                请注意，启动比特币时使用的所有钱包命令行选项都将是
                \n应用于新钱包（例如-zapAllettXes、UpgradeWallet、Rescan等）。\n“，
                {
                    “文件名”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The wallet directory or .dat file."},

                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"name\" :    <wallet_name>,        (string) The wallet name if loaded successfully.\n"
            "  \"warning\" : <warning>,            (string) Warning message if wallet was not loaded cleanly.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("loadwallet", "\"test.dat\"")
            + HelpExampleRpc("loadwallet", "\"test.dat\"")
        );

    WalletLocation location(request.params[0].get_str());
    std::string error;

    if (!location.Exists()) {
        throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet " + location.GetName() + " not found.");
    } else if (fs::is_directory(location.GetPath())) {
//给定的文件名是一个目录。检查是否有wallet.dat文件。
        fs::path wallet_dat_file = location.GetPath() / "wallet.dat";
        if (fs::symlink_status(wallet_dat_file).type() == fs::file_not_found) {
            throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Directory " + location.GetName() + " does not contain a wallet.dat file.");
        }
    }

    std::string warning;
    if (!CWallet::Verify(*g_rpc_interfaces->chain, location, false, error, warning)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet file verification failed: " + error);
    }

    std::shared_ptr<CWallet> const wallet = CWallet::CreateWalletFromFile(*g_rpc_interfaces->chain, location);
    if (!wallet) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet loading failed.");
    }
    AddWallet(wallet);

    wallet->postInitProcess();

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("name", wallet->GetName());
    obj.pushKV("warning", warning);

    return obj;
}

static UniValue createwallet(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"createwallet",
                "\nCreates and loads a new wallet.\n",
                {
                    /*allet_name“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，”新钱包的名称。如果这是路径，钱包将在路径位置创建。“，
                    “禁用_私钥”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "false", "Disable the possibility of private keys (only watchonlys are possible in this mode)."},

                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"name\" :    <wallet_name>,        (string) The wallet name if created successfully. If the wallet was created using a full path, the wallet_name will be the full path.\n"
            "  \"warning\" : <warning>,            (string) Warning message if wallet was not loaded cleanly.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("createwallet", "\"testwallet\"")
            + HelpExampleRpc("createwallet", "\"testwallet\"")
        );
    }
    std::string error;
    std::string warning;

    bool disable_privatekeys = false;
    if (!request.params[1].isNull()) {
        disable_privatekeys = request.params[1].get_bool();
    }

    WalletLocation location(request.params[0].get_str());
    if (location.Exists()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet " + location.GetName() + " already exists.");
    }

//verify将检查我们是否试图创建具有重复名称的钱包。
    if (!CWallet::Verify(*g_rpc_interfaces->chain, location, false, error, warning)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet file verification failed: " + error);
    }

    std::shared_ptr<CWallet> const wallet = CWallet::CreateWalletFromFile(*g_rpc_interfaces->chain, location, (disable_privatekeys ? (uint64_t)WALLET_FLAG_DISABLE_PRIVATE_KEYS : 0));
    if (!wallet) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet creation failed.");
    }
    AddWallet(wallet);

    wallet->postInitProcess();

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("name", wallet->GetName());
    obj.pushKV("warning", warning);

    return obj;
}

static UniValue unloadwallet(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1) {
        throw std::runtime_error(
            RPCHelpMan{"unloadwallet",
                "Unloads the wallet referenced by the request endpoint otherwise unloads the wallet specified in the argument.\n"
                "Specifying the wallet name on a wallet endpoint is invalid.",
                {
                    /*allet_name“，rpcarg:：type:：str，/*opt*/true，/*default_val*/”来自rpc请求的钱包名称，“要卸载的钱包的名称。”，
                }
                toSTRIN（）+
            “\n实例：\n”
            +helpExamplecli（“UnloadWallet”，“Wallet_name”）。
            +HelpExampleRPC（“UnloadWallet”，“Wallet_name”）。
        ；
    }

    std：：字符串钱包名称；
    if（getwalletnamefromjsonrpcrequest（request，wallet_name））
        如果（！）request.params[0].isNull（））
            throw jsonrpcerror（rpc_无效的_参数，“无法卸载请求的钱包”）；
        }
    }否则{
        wallet_name=request.params[0].get_str（）；
    }

    std:：shared_ptr<cwallet>wallet=getwallet（wallet_name）；
    如果（！）钱包）
        throw jsonrpcerror（rpc_wallet_not_found，“请求的钱包不存在或未加载”）；
    }

    //释放“main”共享指针并阻止进一步通知。
    //请注意，任何加载同一钱包的尝试都将失败，直到钱包
    //已销毁（请参见checkuniquefileid）。
    如果（！）取出钱包（钱包）
        throw jsonrpcerror（rpc_misc_error，“请求的钱包已卸载”）；
    }

    卸载钱包（std:：move（wallet））；

    返回nullunivalue；
}

静态单值resendwallettransactions（const jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    如果（request.fhelp request.params.size（）！= 0）
        throw std:：runtime_错误（
            rpchelpman“重置墙传输”，
                “立即向所有对等方重新广播未确认的钱包交易。\n”
                “仅用于测试；钱包代码定期重新广播\n”，
                {}
                toSTRIN（）+
            “自动。\n”
            “如果-walletbroadcast设置为false，则返回RPC错误。\n”
            “返回重新广播的事务ID数组。\n”
            ；

    如果（！）吉康曼
        throw jsonrpcerror（rpc_client_p2p_disabled，“错误：缺少或禁用对等功能”）；

    自动锁定的_chain=pwallet->chain（）.lock（）；
    锁（pwallet->cs_wallet）；

    如果（！）pwallet->getBroadcastTransactions（））
        throw jsonrpcerror（rpc_wallet_error，“错误：wallet事务广播被-walletbroadcast禁用”）；
    }

    std:：vector<uint256>txids=pwallet->resendwallettransactionsbefore（*locked_chain，gettime（），g_connman.get（））；
    单值结果（单值：：varr）；
    for（const uint256&txid:txid）
    {
        result.push_back（txid.toString（））；
    }
    返回结果；
}

静态单值列表（const-jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    if（request.fhelp request.params.size（）>5）
        throw std:：runtime_错误（
            rpchelpman“listunspent”，
                \n返回未暂停的事务输出数组\n
                “在minconf和maxconf（包括）之间进行确认。\n”
                “可选筛选以仅包括支付给指定地址的txout。\n”，
                {
                    “minconf”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "1", "The minimum confirmations to filter"},

                    /*axconf“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”9999999“，要筛选的最大确认数”，
                    “地址”，rpcarg:：type:：arr，/*opt*/ true, /* default_val */ "empty array", "A json array of bitcoin addresses to filter",

                        {
                            /*地址“，rpcarg:：type:：str，/*opt*/true，/*default_val*/”“，“比特币地址”，
                        }
                    }
                    “include_unsafe”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "true", "Include outputs that are not safe to spend\n"

            "                  See description of \"safe\" attribute below."},
                    /*uery_options“，rpcarg:：type:：obj，/*opt*/true，/*default_val*/”null“，”json with query options”，
                        {
                            “最小金额”，rpcarg:：type:：amount，/*opt*/ true, /* default_val */ "0", "Minimum value of each UTXO in " + CURRENCY_UNIT + ""},

                            /*aximumamount“，rpcarg:：type:：amount，/*opt*/true，/*默认值val*/”unlimited“，”每个utxo的最大值，以“+货币单位+”“，
                            “最大计数”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "unlimited", "Maximum number of UTXOs"},

                            /*inimumsumamount“，rpcarg:：type：：amount，/*opt*/true，/*默认值_val*/”unlimited“，”以“+货币单位+”表示的所有utxo的最小和值，
                        }
                        “查询选项”，
                }
                toSTRIN（）+
            “\NESREST \N”
            “[（JSON对象数组\n”
            “{n”
            “txid\”：“txid\”，（string）事务ID \n”
            “vout”：n，（数字）vout值\n”
            “\”地址\“：\”地址\“，（字符串）比特币地址\n”
            “label\”：“label\”，（string）关联的标签，或\”“默认标签\n”
            “scriptpubkey\”：\“key\”，（string）脚本键\n”
            “金额\”：x.x x x，（数字）以“+货币\单位+”表示的交易输出金额\n”
            “\”确认\“：n，（数字）确认数\n”
            “redeemscript”：n（字符串）如果scriptPubKey是p2sh，则为redeemscript\n”
            “\”spenable\“：xxx，（bool）是否有私钥来使用此输出\n”
            “solvable”：xxx，（bool）是否知道如何使用此输出，忽略缺少键\n”
            ““desc”：xxx，（字符串，仅当可解时）用于使用此输出的描述符\n”
            “\”safe\“：xxx（bool）此输出是否被视为安全消费。未确认的事务\n“
            “从外部密钥和未确认的替换事务被认为不安全\n”
            “并且不符合FundRawTransaction和SendToAddress的支出条件。\n”
            “}\n”
            “……\n”
            “\n”

            “\Nase\n”
            +helpexamplecli（“listunspent”，“”）
            +helpExamplecli（“listunspent”，“6 99999999\”[\\\“1pgfqezfmqh1gkd3ra4k18pnj3ttuusqg\\\”，“1ltvqcaapedugfkkmmmstjcal4dkg8sp\\\”“]”）
            +helpExampleRpc（“listunspent”，“6，9999999\”[\\\“1pgfqezfmqh1gkd3ra4k18pnj3ttuusqg\\\”，“1ltvqcaapedugfkkmmmstjcal4dkg8sp\\\”“]”）
            +helpExamplecli（“listunspent”，“6 9999999'[]'真'\“最小金额\”：0.005'”）
            +helpExampleRpc（“listunspent”，“6，9999999，[]，true，\”minimumAmount\“：0.005”）。
        ；

    int nmindepth=1；
    如果（！）request.params[0].isNull（））
        rpctypecheckArgument（request.params[0]，univalue:：vnum）；
        nmindepth=request.params[0].get_int（）；
    }

    int nmaxdepth=9999999；
    如果（！）request.params[1].isNull（））
        rpctypecheckArgument（request.params[1]，univalue:：vnum）；
        nmaxDepth=request.params[1].get_int（）；
    }

    std:：set<ctxdestination>destinations；
    如果（！）request.params[2].isNull（））
        rpctypecheckArgument（request.params[2]，univalue:：varr）；
        univalue inputs=request.params[2].get_array（）；
        for（unsigned int idx=0；idx<inputs.size（）；idx++）
            const univalue&input=输入[idx]；
            ctxdestination dest=解码目的地（input.get_str（））；
            如果（！）isvaliddestination（目的地））
                throw jsonrpcerror（rpc_invalid_address_or_key，std:：string（“无效比特币地址：”）+input.get_str（））；
            }
            如果（！）目的地。插入（dest）.second）
                throw jsonrpcerror（rpc_invalid_参数，std:：string（“无效参数，重复地址：”）+input.get_str（））；
            }
        }
    }

    bool include_unsafe=true；
    如果（！）request.params[3].isNull（））
        rpctypecheckArgument（request.params[3]，univalue:：vbool）；
        include_unsafe=request.params[3].get_bool（）；
    }

    camount nminimumamount=0；
    camount nmaximumamount=最大金额；
    camount nminiumsumamount=最大金额；
    uint64最大计数=0；

    如果（！）request.params[4].isNull（））
        const univalue&options=request.params[4].get_obj（）；

        if（options.exists（“最低金额”））
            nminiumamount=amountFromValue（选项[“minimumAmount”]）；

        if（options.exists（“maximumamount”））
            nmaximumamount=amountFromValue（选项[“maximumamount”]）；

        if（options.exists（“最低金额”））
            nminiumsumamount=amountfromvalue（选项[“minimumsumamount”]）；

        if（options.exists（“maximumCount”））
            nmaximumCount=options[“maximumCount”]。get_int64（）；
    }

    //确保结果至少在最新块之前有效
    //在此之前，用户可能已经从另一个rpc命令获取了
    pwallet->blockUntilsynchedTocurrentchain（）；

    单值结果（单值：：varr）；
    std:：vector<coutput>vecOutputs；
    {
        自动锁定的_chain=pwallet->chain（）.lock（）；
        锁（pwallet->cs_wallet）；
        pwallet->available coins（*locked_chain，vecOutputs，！包括不安全、空指针、空指针、空指针、空指针、空指针、空指针、空指针、空指针、空指针、空指针、空指针、空指针）；
    }

    锁（pwallet->cs_wallet）；

    对于（const coutput&out:vecOutputs）
        CTX目标地址；
        const cscript&scriptpubkey=out.tx->tx->vout[out.i].scriptpubkey；
        bool fvalidAddress=提取目标（scriptPubKey，地址）；

        如果（destinations.size（）&&（！五分裙！destinations.count（地址）））
            继续；

        单值输入（单值：：vobj）；
        entry.pushkv（“txid”，out.tx->gethash（）.gethex（））；
        entry.pushkv（“vout”，out.i）；

        国际单项体育联合会（Fvalidadress）
            entry.pushkv（“地址”，编码目的地（地址））；

            auto i=pwallet->mapaddressbook.find（地址）；
            如果（我）！=pwallet->mapAddressBook.end（））
                entry.pushkv（“标签”，i->second.name）；
            }

            if（scriptpubkey.ispaytosscriptHash（））
                const cscriptid&hash=boost:：get<cscriptid>（地址）；
                CScript重新设计脚本；
                if（pwallet->getcscript（hash，redeemscript））
                    entry.pushkv（“redeemscript”，hexstr（redeemscript.begin（），redeemscript.end（））；
                }
            }
        }

        entry.pushkv（“scriptpubkey”，hexstr（scriptpubkey.begin（），scriptpubkey.end（））；
        entry.pushkv（“金额”，valuefromamount（out.tx->tx->vout[out.i].nvalue））；
        entry.pushkv（“确认”，out.ndepth）；
        entry.pushkv（“spenable”，out.fspendable）；
        entry.pushkv（“可解”，out.fsolvable）；
        如果（out.fsolvable）
            自动描述符=推断描述符（scriptpubkey，*pwallet）；
            entry.pushkv（“desc”，descriptor->toString（））；
        }
        entry.pushkv（“安全”，out.fsafe）；
        结果。向后推（输入）；
    }

    返回结果；
}

无效资金交易（cwallet*const pwallet、cmutabletransaction&tx、camount&fee-out、int&change-out、univalue选项）
{
    //确保结果至少在最新块之前有效
    //在此之前，用户可能已经从另一个rpc命令获取了
    pwallet->blockUntilsynchedTocurrentchain（）；

    CCoincontrol CoinControl公司；
    更改位置=-1；
    bool lockunspents=false；
    单值减去输出功率；
    std:：set<int>setsubtractfeefromoutputs；

    如果（！）选项.isNull（））
      if（options.type（）==univalue:：vbool）
        //仅向后兼容bool回退
        coinControl.fallowwatchonly=options.getou bool（）；
      }
      否则{
        rpctypecheckArgument（选项，univalue:：vobj）；
        rpctypecheckobj（选项，
            {
                “changeAddress”，uniValueType（uniValue:：vstr），
                “changeposition”，uniValueType（uniValue:：vNum），
                “更改_类型”，uniValueType（uniValue:：vstr），
                “includeWatching”，uniValueType（uniValue:：vBool），
                “lockunspents”，uniValueType（uniValue:：vBool），
                “feerate”，univalueType（），//将在下面选中
                “减去feefromoutputs”，uniValueType（uniValue:：varr），
                “可替换”，uniValueType（uniValue:：vBool），
                “conf_target”，uniValueType（uniValue:：vNum），
                “评估_模式”，uniValueType（uniValue:：vstr），
            }
            真的，真的；

        if（options.exists（“changeaddress”））
            ctxDestination dest=decodeDestination（选项[“changeAddress”].get_str（））；

            如果（！）isvaliddestination（目的地））
                throw jsonrpcerror（rpc_invalid_address_or_key，“changeaddress必须是有效的比特币地址”）；
            }

            coinControl.destChange=dest；
        }

        if（options.exists（“changeposition”））
            change_position=options[“change position”].get_int（）；

        if（options.exists（“change_type”））
            if（options.exists（“changeaddress”））
                throw jsonrpcerror（rpc_invalid_参数，“不能同时指定changeaddress和address_type选项”）；
            }
            coincontrol.m_change_type=pwallet->m_default_change_type；
            如果（！）parseOutputType（options[“change_type”].get_str（），*coinControl.m_change_type））
                throw jsonrpcerror（rpc_invalid_address_or_key，strprintf（“未知更改类型“%s”，选项[“Change_type”].get_str（））；
            }
        }

        if（options.exists（“includeWatching”））
            coinControl.fallowwatchonly=options[“includeWatching”].get_bool（）；

        if（options.exists（“lockunspents”））
            lockunspents=选项[“lockunspents”].获取bool（）；

        if（options.exists（“feerate”））
        {
            coinControl.m揯erite=cfeerate（amountFromValue（选项[“feerite”]）；
            coinControl.foverrideferate=真；
        }

        if（options.exists（“Subtractfeefromoutputs”））
            subtractfeefromoutputs=选项[“subtractfeefromoutputs”]。

        if（options.exists（“可替换”））
            coinControl.m_signal_bip125_rbf=选项[“可替换”].get_bool（）；
        }
        if（options.exists（“conf_target”））
            if（options.exists（“feerate”））
                throw jsonrpcerror（rpc_无效的_参数，“不能同时指定conf_target和feerate”）；
            }
            coincontrol.m_confirm_target=parseconfirmtarget（选项[“conf_target”]）；
        }
        if（options.exists（“评估模式”））
            if（options.exists（“feerate”））
                throw jsonrpcerror（rpc_无效的_参数，“不能同时指定评估模式和feerate”）；
            }
            如果（！）feemodefromstring（选项[“Estimate_mode”].get_str（），coinControl.m_fee_mode））
                throw jsonrpcerror（rpc_invalid_参数，“invalid estimate_mode参数”）；
            }
        }
      }
    }

    如果（tx.vout.size（）=0）
        throw jsonrpcerror（rpc_invalid_参数，“tx必须至少有一个输出”）；

    如果（改变位置！=-1&（change_position<0（unsigned int）change_position>tx.vout.size（））
        throw jsonrpcerror（rpc_无效的_参数，“changeposition越界”）；

    for（unsigned int idx=0；idx<subtractfeefromoutputs.size（）；idx++）
        int pos=减去feefromoutputs[idx].get_int（）；
        如果（setSubtractFerefromoutputs.count（pos））。
            throw jsonrpcerror（rpc_invalid_参数，strprintf（“无效参数，重复位置：%d”，pos））；
        如果（POS＜0）
            throw jsonrpcerror（rpc_invalid_参数，strprintf（“无效参数，负位置：%d”，pos））；
        如果（pos>=int（tx.vout.size（））
            throw jsonrpcerror（rpc_invalid_参数，strprintf（“无效参数，位置太大：%d”，pos））；
        设置子牵引力输出。插入（位置）；
    }

    std：：字符串strfailreason；

    如果（！）pWallet->FundTransaction（Tx、Fee_Out、Change_Position、StrFailReason、LockunPents、SetSubtractFeefromoutPuts、CoinControl））；
        throw jsonrpcerror（rpc_wallet_错误，strfailreason）；
    }
}

静态单值Fundrawtransaction（const jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    if（request.fhelp request.params.size（）<1 request.params.size（）>3）
        throw std:：runtime_错误（
            rpchelpman“资金转移”，
                \n向事务添加输入，直到事务的值足够满足其输出值为止。\n
                这不会修改现有的输入，并且最多将一个更改输出添加到输出中。\n
                “除非指定\”SubtractFeefromOutputs“，否则不会修改现有输出。\n”
                “请注意，由于已添加输入/输出，因此在完成后可能需要放弃已签名的输入。\n”
                添加的输入将不会被签名，请使用signrawtransaction进行签名。\n
                “请注意，所有现有输入的前一个输出事务必须在钱包中。\n”
                “请注意，所选的所有输入必须是标准格式，p2sh脚本必须是\n”
                “在钱包中使用importaddress或addmultisigaddress（计算费用）。\n”
                “通过检查listunspent输出中的\”solvable“字段，可以查看是否是这种情况。\n”
                “目前只支持Pubkey、Multisig和p2sh版本，仅用于观看\n”，
                {
                    “hexstring”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "The hex string of the raw transaction"},

                    /*选项“，rpcarg:：type:：obj，/*opt*/true，/*default_val*/”null“，”对于向后兼容：传入true而不是对象将导致\“includeWatching\”：true”，
                        {
                            “changeaddress”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "pool address", "The bitcoin address to receive the change"},

                            /*changeposition“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”random“，更改输出的索引”，
                            “更改_类型”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "set by -changetype", "The output type to use. Only valid if changeAddress is not specified. Options are \"legacy\", \"p2sh-segwit\", and \"bech32\"."},

                            /*includewatching“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”false“，”还可以选择仅监视的输入”，
                            “lockunspents”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "false", "Lock selected unspent outputs"},

                            /*eerate“，rpcarg:：type:：amount，/*opt*/true，/*default_val*/”未设置：使wallet确定费用“，”以“+货币\单位+”/kb”设置特定费率，
                            “减feefromoutputs”，rpcarg:：type:：arr，/*opt*/ true, /* default_val */ "empty array", "A json array of integers.\n"

                            "                              The fee will be equally deducted from the amount of each specified output.\n"
                            "                              Those recipients will receive less bitcoins than you enter in their corresponding amount field.\n"
                            "                              If no outputs are specified here, the sender pays the fee.",
                                {
                                    /*out_index“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”“，“添加更改输出之前基于零的输出索引。”，
                                }
                            }
                            “可替换”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "fallback to wallet's default", "Marks this transaction as BIP125 replaceable.\n"

                            "                              Allows this transaction to be replaced by a transaction with higher fees"},
                            /*onf_target“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”回退到wallet的默认值“，”确认目标（以块为单位）”，
                            “估算模式”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "UNSET", "The fee estimate mode, must be one of:\n"

                            "         \"UNSET\"\n"
                            "         \"ECONOMICAL\"\n"
                            "         \"CONSERVATIVE\""},
                        },
                        "options"},
                    /*switness“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”取决于启发式测试，“事务十六进制是否是序列化的见证事务\n”
                            “如果iswitness不存在，则在解码时将使用启发式测试”，
                }
                toSTRIN（）+
                            “\NESRES:\N”
                            “{n”
                            ““hex\”：“value\”，（string）生成的原始事务（十六进制编码字符串）\n”
                            “费用\”：n，（数字）以“+货币\单位+”表示的费用结果交易支付\n”
                            “changepos”：n（数字）添加的更改输出的位置，或-1\n”
                            “}\n”
                            “\n实例：\n”
                            \n创建没有输入的事务\n
                            +helpExamplecli（“createrawtransaction”，“\”[]\“\”\”\“我的地址\\”：0.01 \“”）+
                            \n添加足够的无符号输入以满足输出值\n
                            +helpExamplecli（“fundrawtransaction”，“\”rawtransactionhex\”“）+
                            \n签名事务\n
                            +helpExamplecli（“signrawtransaction”，“fundedtransactionhex\”“）+
                            \n发送事务\n
                            +helpExamplecli（“sendrawtransaction”，“signedTransactionHex\”）
                            ；

    rpctypecheck（request.params，uniValue:：vstr，uniValueType（），uniValue:：vBool）；

    //从参数分析十六进制字符串
    CmutableTransaction发送；
    bool try_witness=request.params[2].isNull（）？true:request.params[2].获取bool（）；
    bool try_no_witness=request.params[2].isNull（）？真的！request.params[2].获取bool（）；
    如果（！）decodehextx（tx，request.params[0].get_str（），try_no_witness，try_witness））
        throw jsonrpcerror（rpc_反序列化_错误，“tx解码失败”）；
    }

    保险费；
    int改变位置；
    FundTransaction（Pwallet，TX，Fee，Change_Position，Request.Params[1]）；

    单值结果（单值：：vobj）；
    结果.pushkv（“hex”，encodehextx（tx））；
    结果.pushkv（“费用”，金额（费用）中的值）；
    结果.pushkv（“changepos”，更改位置）；

    返回结果；
}

带钱包的单值签名交易（const jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    if（request.fhelp request.params.size（）<1 request.params.size（）>3）
        throw std:：runtime_错误（
            rpchelpman“带钱包的交易标志”，
                为原始事务设计输入（序列化，十六进制编码）。\n
                “第二个可选参数（可能为空）是以前事务输出的数组，该数组\n”
                “此事务依赖于但可能还不在区块链中。”+
                    帮助要求密码（pwallet）+\n“，
                {
                    “hexstring”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The transaction hex string"},

                    /*revtxs“，rpcarg:：type:：arr，/*opt*/true，/*default_val*/”null“，”以前依赖事务输出的JSON数组”，
                        {
                            “”，rpcarg:：type:：obj，/*opt*/ false, /* default_val */ "", "",

                                {
                                    /*xid“，rpcarg:：type:：str_hex，/*opt*/false，/*默认值_val*/”“，“事务ID”，
                                    “vout”，rpcarg:：type:：num，/*opt*/ false, /* default_val */ "", "The output number"},

                                    /*scriptpubkey“，rpcarg:：type:：str_hex，/*opt*/false，/*默认值”_val*/“”，“脚本键”，
                                    “redeemscript”，rpcarg:：type:：str_hex，/*opt*/ true, /* default_val */ "omitted", "(required for P2SH or P2WSH)"},

                                    /*mount“，rpcarg：：type：：amount，/*opt*/false，/*默认值“/”，“花费的金额”，
                                }
                            }
                        }
                    }
                    “sighashType”，rpcarg:：type:：str，/*可选*/ true, /* default_val */ "ALL", "The signature hash type. Must be one of\n"

            "       \"ALL\"\n"
            "       \"NONE\"\n"
            "       \"SINGLE\"\n"
            "       \"ALL|ANYONECANPAY\"\n"
            "       \"NONE|ANYONECANPAY\"\n"
            "       \"SINGLE|ANYONECANPAY\""},
                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"hex\" : \"value\",                  (string) The hex-encoded raw transaction with signature(s)\n"
            "  \"complete\" : true|false,          (boolean) If the transaction has a complete set of signatures\n"
            "  \"errors\" : [                      (json array of objects) Script verification errors (if there are any)\n"
            "    {\n"
            "      \"txid\" : \"hash\",              (string) The hash of the referenced, previous transaction\n"
            "      \"vout\" : n,                   (numeric) The index of the output to spent and used as input\n"
            "      \"scriptSig\" : \"hex\",          (string) The hex-encoded signature script\n"
            "      \"sequence\" : n,               (numeric) Script sequence number\n"
            "      \"error\" : \"text\"              (string) Verification or signing error related to the input\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("signrawtransactionwithwallet", "\"myhex\"")
            + HelpExampleRpc("signrawtransactionwithwallet", "\"myhex\"")
        );

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VARR, UniValue::VSTR}, true);

    CMutableTransaction mtx;
    if (!DecodeHexTx(mtx, request.params[0].get_str(), true)) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

//签署交易
    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);
    EnsureWalletIsUnlocked(pwallet);

    return SignTransaction(pwallet->chain(), mtx, request.params[1], pwallet, false, request.params[2]);
}

static UniValue bumpfee(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();


    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"bumpfee",
                "\nBumps the fee of an opt-in-RBF transaction T, replacing it with a new transaction B.\n"
                "An opt-in RBF transaction with the given txid must be in the wallet.\n"
                "The command will pay the additional fee by decreasing (or perhaps removing) its change output.\n"
                "If the change output is not big enough to cover the increased fee, the command will currently fail\n"
                "instead of adding new inputs to compensate. (A future implementation could improve this.)\n"
                "The command will fail if the wallet or mempool contains a transaction that spends one of T's outputs.\n"
                "By default, the new fee will be calculated automatically using estimatesmartfee.\n"
                "The user can specify a confirmation target for estimatesmartfee.\n"
                "Alternatively, the user can specify totalFee, or use RPC settxfee to set a higher fee rate.\n"
                "At a minimum, the new fee rate must be high enough to pay an additional new relay fee (incrementalfee\n"
                "returned by getnetworkinfo) to enter the node's mempool.\n",
                {
                    /*xid“，rpcarg:：type:：str_hex，/*opt*/false，/*默认值_val*/”，“要缓冲的txid”，
                    “选项”，rpcarg:：type:：obj，/*opt*/ true, /* default_val */ "null", "",

                        {
                            /*onfTarget“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”回退到钱包的默认值“，”确认目标（以块为单位）”，
                            “totalfee”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "fallback to 'confTarget'", "Total fee (NOT feerate) to pay, in satoshis.\n"

            "                         In rare cases, the actual fee paid might be slightly higher than the specified\n"
            "                         totalFee if the tx change output has to be removed because it is too close to\n"
            "                         the dust threshold."},
                            /*eplaceable“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”true“，”新事务是否仍应为\n“
            “标记为BIP-125可替换。如果为真，则事务中的序列号将\n“
            “保持原样不变。如果为false，则在\n“
            “小于0xfffffffe的原始事务将增加到0xfffffe\n”
            “因此，新事务不会显式地由BIP-125替换（尽管它可能\n”
            在实践中仍然是可替换的，例如，如果它有未经确认的祖先，而这些祖先\n
            “可替换。”，
                            “估算模式”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "UNSET", "The fee estimate mode, must be one of:\n"

            "         \"UNSET\"\n"
            "         \"ECONOMICAL\"\n"
            "         \"CONSERVATIVE\""},
                        },
                        "options"},
                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"txid\":    \"value\",   (string)  The id of the new transaction\n"
            "  \"origfee\":  n,         (numeric) Fee of the replaced transaction\n"
            "  \"fee\":      n,         (numeric) Fee of the new transaction\n"
            "  \"errors\":  [ str... ] (json array of strings) Errors encountered during processing (may be empty)\n"
            "}\n"
            "\nExamples:\n"
            "\nBump the fee, get the new transaction\'s txid\n" +
            HelpExampleCli("bumpfee", "<txid>"));
    }

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VOBJ});
    uint256 hash(ParseHashV(request.params[0], "txid"));

//可选参数
    CAmount totalFee = 0;
    CCoinControl coin_control;
    coin_control.m_signal_bip125_rbf = true;
    if (!request.params[1].isNull()) {
        UniValue options = request.params[1];
        RPCTypeCheckObj(options,
            {
                {"confTarget", UniValueType(UniValue::VNUM)},
                {"totalFee", UniValueType(UniValue::VNUM)},
                {"replaceable", UniValueType(UniValue::VBOOL)},
                {"estimate_mode", UniValueType(UniValue::VSTR)},
            },
            true, true);

        if (options.exists("confTarget") && options.exists("totalFee")) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "confTarget and totalFee options should not both be set. Please provide either a confirmation target for fee estimation or an explicit total fee for the transaction.");
} else if (options.exists("confTarget")) { //TODO:将其别名为conf_target
            coin_control.m_confirm_target = ParseConfirmTarget(options["confTarget"]);
        } else if (options.exists("totalFee")) {
            totalFee = options["totalFee"].get_int64();
            if (totalFee <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid totalFee %s (must be greater than 0)", FormatMoney(totalFee)));
            }
        }

        if (options.exists("replaceable")) {
            coin_control.m_signal_bip125_rbf = options["replaceable"].get_bool();
        }
        if (options.exists("estimate_mode")) {
            if (!FeeModeFromString(options["estimate_mode"].get_str(), coin_control.m_fee_mode)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid estimate_mode parameter");
            }
        }
    }

//确保结果至少在最新块之前有效
//在此之前，用户可能已经从另一个rpc命令获得了
    pwallet->BlockUntilSyncedToCurrentChain();

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);
    EnsureWalletIsUnlocked(pwallet);


    std::vector<std::string> errors;
    CAmount old_fee;
    CAmount new_fee;
    CMutableTransaction mtx;
    feebumper::Result res = feebumper::CreateTransaction(pwallet, hash, coin_control, totalFee, errors, old_fee, new_fee, mtx);
    if (res != feebumper::Result::OK) {
        switch(res) {
            case feebumper::Result::INVALID_ADDRESS_OR_KEY:
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, errors[0]);
                break;
            case feebumper::Result::INVALID_REQUEST:
                throw JSONRPCError(RPC_INVALID_REQUEST, errors[0]);
                break;
            case feebumper::Result::INVALID_PARAMETER:
                throw JSONRPCError(RPC_INVALID_PARAMETER, errors[0]);
                break;
            case feebumper::Result::WALLET_ERROR:
                throw JSONRPCError(RPC_WALLET_ERROR, errors[0]);
                break;
            default:
                throw JSONRPCError(RPC_MISC_ERROR, errors[0]);
                break;
        }
    }

//签署缓冲交易
    if (!feebumper::SignTransaction(pwallet, mtx)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Can't sign transaction.");
    }
//提交缓冲事务
    uint256 txid;
    if (feebumper::CommitTransaction(pwallet, hash, std::move(mtx), errors, txid) != feebumper::Result::OK) {
        throw JSONRPCError(RPC_WALLET_ERROR, errors[0]);
    }
    UniValue result(UniValue::VOBJ);
    result.pushKV("txid", txid.GetHex());
    result.pushKV("origfee", ValueFromAmount(old_fee));
    result.pushKV("fee", ValueFromAmount(new_fee));
    UniValue result_errors(UniValue::VARR);
    for (const std::string& error : errors) {
        result_errors.push_back(error);
    }
    result.pushKV("errors", result_errors);

    return result;
}

UniValue generate(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();


    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"generate",
                "\nMine up to nblocks blocks immediately (before the RPC call returns) to an address in the wallet.\n",
                {
                    /*blocks“，rpcarg:：type:：num，/*opt*/false，/*default_val*/”，“立即生成多少块。”，
                    “maxtries”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "1000000", "How many iterations to try."},

                }}
                .ToString() +
            "\nResult:\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks\n"
            + HelpExampleCli("generate", "11")
        );
    }

    if (!IsDeprecatedRPCEnabled("generate")) {
        throw JSONRPCError(RPC_METHOD_DEPRECATED, "The wallet generate rpc method is deprecated and will be fully removed in v0.19. "
            "To use generate in v0.18, restart bitcoind with -deprecatedrpc=generate.\n"
            "Clients should transition to using the node rpc method generatetoaddress\n");
    }

    int num_generate = request.params[0].get_int();
    uint64_t max_tries = 1000000;
    if (!request.params[1].isNull()) {
        max_tries = request.params[1].get_int();
    }

    std::shared_ptr<CReserveScript> coinbase_script;
    pwallet->GetScriptForMining(coinbase_script);

//如果密钥池已用完，则根本不会返回任何脚本。抓住这个。
    if (!coinbase_script) {
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    }

//如果未提供脚本，则引发错误
    if (coinbase_script->reserveScript.empty()) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "No coinbase script available");
    }

    return generateBlocks(coinbase_script, num_generate, max_tries, true);
}

UniValue rescanblockchain(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"rescanblockchain",
                "\nRescan the local blockchain for wallet related transactions.\n",
                {
                    /*start_height“，rpcarg:：type:：num，/*opt*/true，/*默认值_val*/”0“，重新扫描应开始的块高度”，
                    “停止高度”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "tip height", "the last block height that should be scanned"},

                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"start_height\"     (numeric) The block height where the rescan has started. If omitted, rescan started from the genesis block.\n"
            "  \"stop_height\"      (numeric) The height of the last rescanned block. If omitted, rescan stopped at the chain tip.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("rescanblockchain", "100000 120000")
            + HelpExampleRpc("rescanblockchain", "100000, 120000")
            );
    }

    WalletRescanReserver reserver(pwallet);
    if (!reserver.reserve()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet is currently rescanning. Abort existing rescan or wait.");
    }

    CBlockIndex *pindexStart = nullptr;
    CBlockIndex *pindexStop = nullptr;
    CBlockIndex *pChainTip = nullptr;
    {
        auto locked_chain = pwallet->chain().lock();
        pindexStart = chainActive.Genesis();
        pChainTip = chainActive.Tip();

        if (!request.params[0].isNull()) {
            pindexStart = chainActive[request.params[0].get_int()];
            if (!pindexStart) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid start_height");
            }
        }

        if (!request.params[1].isNull()) {
            pindexStop = chainActive[request.params[1].get_int()];
            if (!pindexStop) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid stop_height");
            }
            else if (pindexStop->nHeight < pindexStart->nHeight) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "stop_height must be greater than start_height");
            }
        }
    }

//我们无法在非修剪块之外重新扫描，停止并抛出错误
    if (fPruneMode) {
        auto locked_chain = pwallet->chain().lock();
        CBlockIndex *block = pindexStop ? pindexStop : pChainTip;
        while (block && block->nHeight >= pindexStart->nHeight) {
            if (!(block->nStatus & BLOCK_HAVE_DATA)) {
                throw JSONRPCError(RPC_MISC_ERROR, "Can't rescan beyond pruned data. Use RPC call getblockchaininfo to determine your pruned height.");
            }
            block = block->pprev;
        }
    }

    const CBlockIndex *failed_block, *stopBlock;
    CWallet::ScanResult result =
        pwallet->ScanForWalletTransactions(pindexStart, pindexStop, reserver, failed_block, stopBlock, true);
    switch (result) {
    case CWallet::ScanResult::SUCCESS:
break; //由scanforwallettransactions设置的停止块
    case CWallet::ScanResult::FAILURE:
        throw JSONRPCError(RPC_MISC_ERROR, "Rescan failed. Potentially corrupted data files.");
    case CWallet::ScanResult::USER_ABORT:
        throw JSONRPCError(RPC_MISC_ERROR, "Rescan aborted.");
//没有默认情况，因此编译器可以警告丢失的情况
    }
    UniValue response(UniValue::VOBJ);
    response.pushKV("start_height", pindexStart->nHeight);
    response.pushKV("stop_height", stopBlock->nHeight);
    return response;
}

class DescribeWalletAddressVisitor : public boost::static_visitor<UniValue>
{
public:
    CWallet * const pwallet;

    void ProcessSubScript(const CScript& subscript, UniValue& obj, bool include_addresses = false) const
    {
//始终存在：脚本类型和redeemscript
        std::vector<std::vector<unsigned char>> solutions_data;
        txnouttype which_type = Solver(subscript, solutions_data);
        obj.pushKV("script", GetTxnOutputType(which_type));
        obj.pushKV("hex", HexStr(subscript.begin(), subscript.end()));

        CTxDestination embedded;
        UniValue a(UniValue::VARR);
        if (ExtractDestination(subscript, embedded)) {
//只有当脚本与地址对应时。
            UniValue subobj(UniValue::VOBJ);
            UniValue detail = DescribeAddress(embedded);
            subobj.pushKVs(detail);
            UniValue wallet_detail = boost::apply_visitor(*this, embedded);
            subobj.pushKVs(wallet_detail);
            subobj.pushKV("address", EncodeDestination(embedded));
            subobj.pushKV("scriptPubKey", HexStr(subscript.begin(), subscript.end()));
//始终在顶层报告pubkey，以便“getNewAddress（）['pubkey']`始终有效。
            if (subobj.exists("pubkey")) obj.pushKV("pubkey", subobj["pubkey"]);
            obj.pushKV("embedded", std::move(subobj));
            if (include_addresses) a.push_back(EncodeDestination(embedded));
        } else if (which_type == TX_MULTISIG) {
//还报告有关多搜索脚本（没有相应地址）的一些信息。
//TODO:抽象出此逻辑和ExtractDestinations之间的公共功能。
            obj.pushKV("sigsrequired", solutions_data[0][0]);
            UniValue pubkeys(UniValue::VARR);
            for (size_t i = 1; i < solutions_data.size() - 1; ++i) {
                CPubKey key(solutions_data[i].begin(), solutions_data[i].end());
                if (include_addresses) a.push_back(EncodeDestination(key.GetID()));
                pubkeys.push_back(HexStr(key.begin(), key.end()));
            }
            obj.pushKV("pubkeys", std::move(pubkeys));
        }

//“地址”字段令人困惑，因为它引用使用其p2pkh地址的公钥。
//因此，只有在需要向后兼容时才添加“地址”字段。新应用程序
//可以对p2sh或p2wsh包装的地址使用'embedded'->'address'字段，对
//检查多组参与者。
        if (include_addresses) obj.pushKV("addresses", std::move(a));
    }

    explicit DescribeWalletAddressVisitor(CWallet* _pwallet) : pwallet(_pwallet) {}

    UniValue operator()(const CNoDestination& dest) const { return UniValue(UniValue::VOBJ); }

    UniValue operator()(const CKeyID& keyID) const
    {
        UniValue obj(UniValue::VOBJ);
        CPubKey vchPubKey;
        if (pwallet && pwallet->GetPubKey(keyID, vchPubKey)) {
            obj.pushKV("pubkey", HexStr(vchPubKey));
            obj.pushKV("iscompressed", vchPubKey.IsCompressed());
        }
        return obj;
    }

    UniValue operator()(const CScriptID& scriptID) const
    {
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        if (pwallet && pwallet->GetCScript(scriptID, subscript)) {
            ProcessSubScript(subscript, obj, IsDeprecatedRPCEnabled("validateaddress"));
        }
        return obj;
    }

    UniValue operator()(const WitnessV0KeyHash& id) const
    {
        UniValue obj(UniValue::VOBJ);
        CPubKey pubkey;
        if (pwallet && pwallet->GetPubKey(CKeyID(id), pubkey)) {
            obj.pushKV("pubkey", HexStr(pubkey));
        }
        return obj;
    }

    UniValue operator()(const WitnessV0ScriptHash& id) const
    {
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        CRIPEMD160 hasher;
        uint160 hash;
        hasher.Write(id.begin(), 32).Finalize(hash.begin());
        if (pwallet && pwallet->GetCScript(CScriptID(hash), subscript)) {
            ProcessSubScript(subscript, obj);
        }
        return obj;
    }

    UniValue operator()(const WitnessUnknown& id) const { return UniValue(UniValue::VOBJ); }
};

static UniValue DescribeWalletAddress(CWallet* pwallet, const CTxDestination& dest)
{
    UniValue ret(UniValue::VOBJ);
    UniValue detail = DescribeAddress(dest);
    ret.pushKVs(detail);
    ret.pushKVs(boost::apply_visitor(DescribeWalletAddressVisitor(pwallet), dest));
    return ret;
}

/*将caddressbookdata转换为json记录。*/
static UniValue AddressBookDataToJSON(const CAddressBookData& data, const bool verbose)
{
    UniValue ret(UniValue::VOBJ);
    if (verbose) {
        ret.pushKV("name", data.name);
    }
    ret.pushKV("purpose", data.purpose);
    return ret;
}

UniValue getaddressinfo(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"getaddressinfo",
                "\nReturn information about the given bitcoin address. Some information requires the address\n"
                "to be in the wallet.\n",
                {
                    /*地址“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”，“要获取信息的比特币地址。”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            “\”地址\“：\”地址\“，（字符串）比特币地址已验证\n”
            “scriptpubkey\”：“hex\”，（string）由地址生成的十六进制编码的scriptpubkey \n”
            “IsMine”：真假，（布尔值），如果地址是您的或不是您的地址\n”
            “IsWatchOnly”：如果地址是WatchOnly，则为true false，（布尔值）。\n”
            “solvable”：真假，（布尔值）我们是否知道如何使用发送到此地址的硬币，忽略可能缺少私钥的情况\n”
            ““desc\”：\“desc\”，（字符串，可选）用于消费发送到此地址的硬币的描述符（仅当可解决时）。\n”
            “IsScript\”：true false，（布尔值），如果键是脚本\n”
            如果地址用于更改输出，则为“ischange”：true false，（布尔值）。\n
            如果地址是见证地址，则为“is witness”：true false，（布尔值）。\n
            “见证版本”：版本（数字，可选）见证程序的版本号\n”
            “见证程序”：“hex”（字符串，可选）见证程序的十六进制值\n”
            “\”script\“：\”type\“（string，可选）输出脚本类型。只有当“isscript”为真且已知redeemscript时。可能的类型：非标准、pubkey、pubkeyhash、scripthash、multisig、nulldata、witness_v0_keyhash、witness_v0_scripthash、witness_unknown \n”
            ““hex\”：“hex\”，（string，可选）p2sh地址的redeemscript \n”
            “pubkeys”（字符串，可选）与已知redeemscript关联的pubkeys数组（仅当“script”为“multisig”）\n”
            “\n”
            “pubkey \ \ \n”
            “……\n”
            “\n”
            “SIGSREQUIRED\”：xxxxx（数字，可选）花费multisig输出所需的签名数（仅当\”script\“是\”multisig\“）n”
            “pubkey\”：\“public key hex\”，（string，可选）原始公钥的十六进制值，用于单个密钥地址（可能嵌入到p2sh或p2wsh中）\n”
            “\”Embedded\”：…，（对象，可选）有关p2sh或p2wsh中嵌入地址的信息（如果相关和已知）。它包括嵌入地址的所有getAddressInfo输出字段，不包括元数据（\“Timestamp\”、\“HdKeyPath\”、\“HdSeedID\”）和与钱包的关系（\“IsMine\”、\“IsWatchOnly\”）。
            “is compressed\”：true false，（布尔值，可选），如果pubkey被压缩\n”
            “label\”：“label\”（string）与地址关联的标签，“\”\“是默认标签\n”
            “timestamp”：timestamp，（数字，可选）键的创建时间（如果从epoch开始可用，以秒为单位）（1970年1月1日，格林威治标准时间）\n”
            “hd keypath”：“keypath”（字符串，可选）如果密钥是hd并且可用，则为hd keypath \n”
            “hdseeid\”：\“<hash160>\”（string，可选）hd seed的hash160\n”
            hdseeid的“hdmasterkeyid\”：\“<hash160>\”（字符串，可选）别名保持向后兼容性。将在v0.18中删除。\n“
            “标签”（对象）与地址关联的标签数组。\n”
            “\n”
            “（标签数据的JSON对象\n”
            “name\”：“label name\”（string）标签\n
            “用途\”：“字符串\”（string）地址用途（发送地址为“发送”，接收地址为“接收”）\n”
            “}，…\n”
            “\n”
            “}\n”
            “\n实例：\n”
            +helpExamplecli（“getaddressinfo”，“1pssgefhdnknxieyfrd1wceahr9hrqddwc\”）
            +helpExampleRpc（“getAddressInfo”，“1pssGefhDnkXieyFrd1wceahr9hrqddwc\”）
        ；
    }

    锁（pwallet->cs_wallet）；

    单值ret（单值：：vobj）；
    ctxdestination dest=decodedestination（request.params[0].get_str（））；

    //确保目的地有效
    如果（！）isvaliddestination（目的地））
        throw jsonrpcerror（rpc_invalid_address_or_key，“invalid address”）；
    }

    std:：string currentaddress=encodedestination（dest）；
    ret.pushkv（“地址”，当前地址）；

    cscript scriptPubkey=getscriptForDestination（目标）；
    ret.pushkv（“scriptpubkey”，hexstr（scriptpubkey.begin（），scriptpubkey.end（））；

    isminetype mine=ismine（*pwallet，dest）；
    ret.pushkv（“ismine”，bool（mine&ismineou spenable））；
    bool solvable=issolvable（*pwallet，scriptpubkey）；
    ret.pushkv（“可解”，可解）；
    如果（可解）{
       ret.pushkv（“desc”，InferDescriptor（scriptPubKey，*pWallet）->ToString（））；
    }
    ret.pushkv（“iswatchonly”，bool（mine&ismine_watch_only））；
    单值详细信息=描述的walletaddress（pwallet，dest）；
    ret.pushkvs（细节）；
    if（pwallet->mapaddressbook.count（dest））
        ret.pushkv（“标签”，pwallet->mapaddressbook[dest].name）；
    }
    ret.pushkv（“ischange”，pwallet->ischange（scriptpubkey））；
    const ckeymetadata*meta=nullptr；
    ckeyid key_id=getkeywordestination（*pwallet，dest）；
    如果（！）key_id.isNull（））
        auto it=pwallet->mapkeymetadata.find（key_id）；
        如果（它）！=pwallet->mapkeymetadata.end（））
            meta=&it->second；
        }
    }
    如果（！）元）{
        auto it=pwallet->m_script_metadata.find（cscriptid（scriptpubkey））；
        如果（它）！=pwallet->m_script_metadata.end（））
            meta=&it->second；
        }
    }
    如果（元）{
        ret.pushkv（“时间戳”，meta->ncreateTime）；
        如果（！）meta->hdkeypath.empty（））
            ret.pushkv（“hdkeypath”，meta->hdkeypath）；
            ret.pushkv（“hdseeid”，meta->hd_seed_id.gethex（））；
            ret.pushkv（“hdmasterkeyid”，meta->hd_seed_id.gethex（））；
        }
    }

    //当前一个地址只能关联一个标签，返回一个数组
    //因此如果我们允许多个标签与
    /地址。
    单值标签（单值：：varr）；
    std:：map<ctxdestination，caddressbookdata>：：迭代器mi=pwallet->mapaddressbook.find（dest）；
    如果（MI）！=pwallet->mapAddressBook.end（））
        labels.push_back（addressbookdatatojson（mi->second，true））；
    }
    ret.pushkv（“标签”，std:：move（labels））；

    返回RET；
}

静态单值GetAddressByLabel（const-jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    如果（request.fhelp request.params.size（）！= 1）
        throw std:：runtime_错误（
            rpchelpman“获取地址字节”，
                “\n返回分配给指定标签的地址列表。\n”，
                {
                    “label”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The label."},

                }}
                .ToString() +
            "\nResult:\n"
            "{ (json object with addresses as keys)\n"
            "  \"address\": { (json object with information about address)\n"
            "    \"purpose\": \"string\" (string)  Purpose of address (\"send\" for sending address, \"receive\" for receiving address)\n"
            "  },...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressesbylabel", "\"tabby\"")
            + HelpExampleRpc("getaddressesbylabel", "\"tabby\"")
        );

    LOCK(pwallet->cs_wallet);

    std::string label = LabelFromValue(request.params[0]);

//查找具有给定标签的所有地址
    UniValue ret(UniValue::VOBJ);
    for (const std::pair<const CTxDestination, CAddressBookData>& item : pwallet->mapAddressBook) {
        if (item.second.name == label) {
            ret.pushKV(EncodeDestination(item.first), AddressBookDataToJSON(item.second, false));
        }
    }

    if (ret.empty()) {
        throw JSONRPCError(RPC_WALLET_INVALID_LABEL_NAME, std::string("No addresses with label " + label));
    }

    return ret;
}

static UniValue listlabels(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            RPCHelpMan{"listlabels",
                "\nReturns the list of all labels, or labels that are assigned to addresses with a specific purpose.\n",
                {
                    /*urlese“，rpcarg:：type:：str，/*opt*/true，/*默认值val*/”null“，”列出标签的地址目的（“send”，“receive”）。空字符串与不提供此参数相同。“，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “[（字符串的JSON数组\n”
            ““label\”，（string）标签名称\n”
            “……n”
            “\n”
            “\n实例：\n”
            \n列出所有标签\n
            +helpExamplecli（“listLabels”，“）+
            \n列出具有接收地址的标签\n
            +helpExamplecli（“listLabels”，“receive”）+
            \n列出具有发送地址的标签\n
            +helpExamplecli（“listLabels”，“send”）+
            “作为JSON-RPC调用\n”
            +helpExampleRPC（“listLabels”，“receive”）。
        ；

    锁（pwallet->cs_wallet）；

    std：：字符串用途；
    如果（！）request.params[0].isNull（））
        用途=请求。参数[0].get_str（）；
    }

    //添加到按标签名称排序的集合中，然后插入到单值数组中
    std:：set<std:：string>label_set；
    for（const std:：pair<const ctxdestination，caddressbookdata>&entry:pwallet->mapaddressbook）
        if（purpose.empty（）entry.second.purpose==目的）
            标签“set.insert”（entry.second.name）；
        }
    }

    单值ret（单值：：varr）；
    for（const std:：string&name:label_set）
        ret.推回（名称）；
    }

    返回RET；
}

单值sethdseed（const jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    if（request.fhelp request.params.size（）>2）
        throw std:：runtime_错误（
            rpchelpman“sethdseed”，
                \n设置或生成新的HD钱包种子。非高清钱包将不会升级为高清钱包。已经存在的钱包\n“
                “HD将具有新的HD种子集，因此添加到密钥池的新密钥将从此新种子派生。\n”
                “请注意，在设置HD Wallet种子后，您需要对钱包进行新的备份。”+
                    帮助要求密码（pwallet）+\n“，
                {
                    “newkeypool”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "true", "Whether to flush old unused addresses, including change addresses, from the keypool and regenerate it.\n"

            "                             If true, the next address from getnewaddress and change address from getrawchangeaddress will be from this new seed.\n"
            "                             If false, addresses (including change addresses if the wallet already had HD Chain Split enabled) from the existing\n"
            "                             keypool will be used until it has been depleted."},
                    /*eed“，rpcarg:：type:：str，/*opt*/true，/*default_val*/”random seed“，”要用作新HD种子的WIF私钥。\n”
            “可以使用dumpwallet命令检索种子值。它是标记为hdseed=1“的私钥，
                }
                toSTRIN（）+
            “\n实例：\n”
            +helpexamplecli（“sethdseed”，“”）
            +helpExamplecli（“sethdseed”，“false”）。
            +helpExamplecli（“sethdseed”，“true\”wifkey\“”）
            +helpExampleRpc（“sethdseed”，“true”，“wifkey”）。
            ；
    }

    if（isitialBlockDownload（））
        throw jsonrpcerror（rpc_client_in_initial_download，“cannot set a new hd seed while still in initial block download”）；
    }

    自动锁定的_chain=pwallet->chain（）.lock（）；
    锁（pwallet->cs_wallet）；

    //不要对非高清钱包做任何操作
    如果（！）pwallet->ishdenabled（））
        throw jsonrpcerror（rpc_wallet_error，“无法在非HD钱包上设置HD种子。”从升级钱包开始，以将非高清钱包升级为高清”）；
    }

    确保已解锁（PWallet）；

    bool flush_key_pool=true；
    如果（！）request.params[0].isNull（））
        flush_key_pool=request.params[0].get_bool（）；
    }

    cpubkey master_pub_键；
    if（request.params[1].isNull（））
        master_pub_key=pwallet->generatenewseed（）；
    }否则{
        ckey key=decodeSecret（request.params[1].get_str（））；
        如果（！）key.isvalid（））
            throw jsonrpcerror（rpc_invalid_address_or_key，“invalid private key”）；
        }

        if（have key（*pwallet，key））
            throw jsonrpcerror（rpc_-invalid_-address_或_-key，“已经有了这个密钥（作为HD种子或作为松散的私钥）”）；
        }

        master_pub_key=pwallet->derivenewseed（key）；
    }

    pwallet->sethdseed（主菜单键）；
    if（flush_key_pool）pwallet->newkeypool（）；

    返回nullunivalue；
}

void addkeypathtomap（const cwallet*pwallet，const ckeyid&keyid，std:：map<cpubkey，keyorigininfo>&hd_keypaths）
{
    cpubkey-vchSubkey；
    如果（！）pwallet->getpubkey（keyid，vchpubkey））
        返回；
    }
    钥匙原始信息；
    如果（！）pwallet->getkeyorigin（keyid，info））
        throw jsonrpcerror（rpc_internal_error，“internal keypath is broken”）；
    }
    hd_keypaths.emplace（vchSubkey，std:：move（info））；
}

bool fillpsbt（const cwallet*pwallet，partiallysignedtransaction&psbtx，int sighash_类型，bool符号，bool bip32derivals）
{
    锁（pwallet->cs_wallet）；
    //获取所有以前的事务
    布尔完成=真；
    for（unsigned int i=0；i<psbtx.tx->vin.size（）；++i）
        const ctxin&txin=psbtx.tx->vin[i]；
        psbtinput&input=psbtx.inputs.at（i）；

        if（psbtinputsigned（input））
            继续；
        }

        //验证输入是否正常。这将检查我们是否最多有一个UXTO、证人或非证人。
        如果（！）input.issane（））
            throw jsonrpcerror（rpc“反序列化”错误，“psbt输入不正常”）；
        }

        //如果我们没有utxo，从钱包里拿出来。
        如果（！）input.non_-witness_-utxo&&input.witness_-utxo.isNull（））
            const uint256&txshash=txin.prevout.hash；
            const auto it=pwallet->mapwallet.find（txshash）；
            如果（它）！=pwallet->mapwallet.end（））
                const cwallettx&wtx=it->second；
                //我们只需要非证人_utxo，它是证人_utxo的超集。
                //如果可以，签名代码将切换到较小的见证_utxo。
                input.non_-witness_-utxo=wtx.tx；
            }
        }

        //获取sighash类型
        如果（sign&&input.sighash_type>0&&input.sighash_type！=叹息类型）
            throw jsonrpcerror（rpc“反序列化”错误，“PSBT中指定的sighash和sighash不匹配”）；
        }

        完成&=signPSBTinput（hidingSigningProvider（pWallet，！签字！bip32衍生物），psbtx，i，sighash_型；
    }

    //为输出填充bip32键路径和redeemscripts，以便硬件钱包可以识别更改
    for（unsigned int i=0；i<psbtx.tx->vout.size（）；++i）
        const ctxout&out=psbtx.tx->vout.at（i）；
        psbtOutput&psbt_out=psbtx.outputs.at（i）；

        //用输出信息填充SignatureData
        签名数据；
        psbt-out.fillsignaturedata（sigdata）；

        mutableTransactionSignatureCreator创建者（psbtx.tx.get_ptr（），0，out.nvalue，1）；
        产品签名（HidingSigningProvider（pWallet，true，！bip32derivals），creator，out.scriptpubkey，sigdata）；
        从SignatureData（SIGData）输出PSBT；
    }
    返回完成；
}

单值WalletProcessPSBT（const jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    if（request.fhelp request.params.size（）<1 request.params.size（）>4）
        throw std:：runtime_错误（
            rpchelpman“墙处理psbt”，
                \n用钱包中的输入信息更新PSBT，然后对输入进行签名\n
                “我们可以签约。”+
                    帮助要求密码（pwallet）+\n“，
                {
                    “psbt”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The transaction base64 string"},

                    /*ign“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”true“，”更新“”时也要签署事务，
                    “sighashType”，rpcarg:：type:：str，/*可选*/ true, /* default_val */ "ALL", "The signature hash type to sign with if not specified by the PSBT. Must be one of\n"

            "       \"ALL\"\n"
            "       \"NONE\"\n"
            "       \"SINGLE\"\n"
            "       \"ALL|ANYONECANPAY\"\n"
            "       \"NONE|ANYONECANPAY\"\n"
            "       \"SINGLE|ANYONECANPAY\""},
                    /*ip32derivals“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”false“，”如果为true，则包括公钥的bip 32派生路径（如果已知的话）”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            ““psbt\”：“value\”，（string）base64编码的部分签名事务\n”
            ““完成”：如果事务有一组完整的签名，则为true false，（布尔值）。\n”
            “\n”
            “}\n”

            “\n实例：\n”
            +helpExamplecli（“walletprocesspsbt”，“psbt”）。
        ；

    rpctypecheck（request.params，univalue:：vstr，univalue:：vbool，univalue:：vstr）；

    //取消对事务的序列化
    部分已签名的事务PSBTX；
    std：：字符串错误；
    如果（！）decodepsbt（psbtx，request.params[0].get_str（），error））
        throw jsonrpcerror（rpc_反序列化_错误，strprintf（“tx解码失败%s”，错误））；
    }

    //获取sighash类型
    int nhashtype=parseSighAshString（请求.params[2]）；

    //用我们的数据填充事务并签名
    bool sign=request.params[1].isNull（）？true:request.params[1].获取bool（）；
    bool bip32derivals=request.params[3].isNull（）？false:request.params[3].获取bool（）；
    bool complete=fillpsbt（pwallet、psbtx、nhashtype、sign、bip32derivals）；

    单值结果（单值：：vobj）；
    cdatastream sstx（ser_网络，协议版本）；
    SSTX＜PSBTX；
    结果.pushkv（“psbt”，encodebase64（sstx.str（））；
    结果.pushkv（“完成”，完成）；

    返回结果；
}

单值WalletCreateFundedPSBT（const jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；

    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    if（request.fhelp request.params.size（）<2 request.params.size（）>5）
        throw std:：runtime_错误（
            rpchelpman“WalletCreateFundedPSBT”，
                \n以部分签名的事务格式创建事务并为其提供资金。如果提供的输入不够，将添加输入\n“
                “实现创建者和更新者角色。\n”，
                {
                    “输入”，rpcarg:：type:：arr，/*opt*/ false, /* default_val */ "", "A json array of json objects",

                        {
                            /*，rpcarg:：type:：obj，/*opt*/false，/*默认值“/”，“”，
                                {
                                    “txid”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "The transaction id"},

                                    /*out“，rpcarg:：type:：num，/*opt*/false，/*default_val*/”，“输出编号”，
                                    “序列”，rpcarg:：type:：num，/*opt*/ false, /* default_val */ "", "The sequence number"},

                                },
                            },
                        },
                        },
                    /*utputs“，rpcarg:：type:：arr，/*opt*/false，/*default_val*/”“，”带有输出（键值对）的JSON数组，其中没有任何键重复。\n”
                            也就是说，每个地址只能出现一次，并且只能有一个“数据”对象。\n
                            由于兼容性的原因，直接保存键值对的字典也是\n
                            “接受为第二个参数。”，
                        {
                            “”，rpcarg:：type:：obj，/*opt*/ true, /* default_val */ "", "",

                                {
                                    /*地址“，rpcarg:：type:：amount，/*opt*/false，/*default_val*/”“，”键-值对。键（字符串）是比特币地址，值（浮点数或字符串）是以“+货币单位+”“”表示的金额，
                                }
                                }
                            “”，rpcarg:：type:：obj，/*opt*/ true, /* default_val */ "", "",

                                {
                                    /*ata“，rpcarg:：type:：str_hex，/*opt*/false，/*默认值“/”，“一个键值对。键必须为“data”，值为十六进制编码数据“，
                                }
                            }
                        }
                    }
                    “锁定时间”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "0", "Raw locktime. Non-0 value also locktime-activates inputs\n"

                            "                             Allows this transaction to be replaced by a transaction with higher fees. If provided, it is an error if explicit sequence numbers are incompatible."},
                    /*选项“，rpcarg:：type:：obj，/*opt*/true，/*default_val*/”null“，”，
                        {
                            “changeaddress”，rpcarg:：type:：str_hex，/*opt*/ true, /* default_val */ "pool address", "The bitcoin address to receive the change"},

                            /*changeposition“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”random“，更改输出的索引”，
                            “更改_类型”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "set by -changetype", "The output type to use. Only valid if changeAddress is not specified. Options are \"legacy\", \"p2sh-segwit\", and \"bech32\"."},

                            /*includewatching“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”false“，”还可以选择仅监视的输入”，
                            “lockunspents”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "false", "Lock selected unspent outputs"},

                            /*eerate“，rpcarg:：type:：amount，/*opt*/true，/*default_val*/”未设置：使wallet确定费用“，”以“+货币\单位+”/kb”设置特定费率，
                            “减feefromoutputs”，rpcarg:：type:：arr，/*opt*/ true, /* default_val */ "empty array", "A json array of integers.\n"

                            "                              The fee will be equally deducted from the amount of each specified output.\n"
                            "                              Those recipients will receive less bitcoins than you enter in their corresponding amount field.\n"
                            "                              If no outputs are specified here, the sender pays the fee.",
                                {
                                    /*out_index“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”“，“添加更改输出之前基于零的输出索引。”，
                                }
                            }
                            “可替换”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "false", "Marks this transaction as BIP125 replaceable.\n"

                            "                              Allows this transaction to be replaced by a transaction with higher fees"},
                            /*onf_target“，rpcarg:：type:：num，/*opt*/true，/*default_val*/“回退到钱包的确认目标”，“确认目标（以块为单位）”，
                            “估算模式”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "UNSET", "The fee estimate mode, must be one of:\n"

                            "         \"UNSET\"\n"
                            "         \"ECONOMICAL\"\n"
                            "         \"CONSERVATIVE\""},
                        },
                        "options"},
                    /*ip32derivals“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”false“，”如果为true，则包括公钥的bip 32派生路径（如果已知的话）”，
                }
                toSTRIN（）+
                            “\NESRES:\N”
                            “{n”
                            “psbt\”：\“value\”，（string）生成的原始事务（base64编码字符串）\n”
                            “费用\”：n，（数字）以“+货币\单位+”表示的费用结果交易支付\n”
                            “changepos”：n（数字）添加的更改输出的位置，或-1\n”
                            “}\n”
                            “\n实例：\n”
                            \n创建没有输入的事务\n
                            +helpExamplecli（“walletcreatefundedpsbt”，“[\ \ \”txid \ \ \ \\“：\ \ \”myid \ \\“，\ \ \”vout \\“：0]\”“\”[\ \ \\”data \\“：\ \”00010203 \\“\”“\”“\”）
                            ；

    rpctypecheck（request.params，
        UnValue:：
        univalueType（），//arr或obj，稍后检查
        UnValue:VNUM，
        UnValue:：
        UnValue: VBOL
        }，真的
    ；

    保险费；
    int改变位置；
    cmutableTransaction rawtx=constructTransaction（request.params[0]，request.params[1]，request.params[2]，request.params[3][可替换]）；
    FundTransaction（pWallet、Rawtx、Fee、Change_Position、Request.Params[3]）；

    //生成空白PSBT
    部分信号传输PSBTX（RAWTX）；

    //用out数据填充事务，但不签名
    bool bip32derivals=request.params[4].isNull（）？false:request.params[4].获取bool（）；
    fillpsbt（pwallet，psbtx，1，false，bip32derivals）；

    //序列化psbt
    cdatastream sstx（ser_网络，协议版本）；
    SSTX＜PSBTX；

    单值结果（单值：：vobj）；
    结果.pushkv（“psbt”，encodebase64（sstx.str（））；
    结果.pushkv（“费用”，金额（费用）中的值）；
    结果.pushkv（“changepos”，更改位置）；
    返回结果；
}

univalue abortescan（const jsonrpcrequest&request）；//在rpcdump.cpp中
univalue dumpprivkey（const jsonrpcrequest&request）；//在rpcdump.cpp中
univalue importprivkey（const jsonrpcrequest&request）；
单值导入地址（const jsonrpcrequest&request）；
单值importpubkey（const jsonrpcrequest&request）；
单值dumpwallet（const jsonrpcrequest&request）；
单值导入钱包（const jsonrpcrequest&request）；
单值进口修剪基金（const jsonrpcrequest&request）；
单值删除修剪基金（const jsonrpcrequest&request）；
单值importmulti（const jsonrpcrequest&request）；

//关闭clang格式
静态const crpccommand命令[]
//类别名称actor（function）argnames
    ///————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    “生成”，“生成”，&generate，“nblocks”，“maxtries”，
    “隐藏”，“重新发送墙传递”，&resendwallettransactions，，
    “rawtransactions”，“fundrawtransaction”，&fundrawtransaction，“hexstring”，“options”，“is witness”，
    “wallet”，“abandontransaction”，&abandontransaction，“txid”，
    “wallet”，“abortescan”，&abortescan，，
    “wallet”，“addmultisigaddress”，&addmultisigaddress，“nRequired”，“keys”，“label”，“address_type”，
    “钱包”，“备份钱包”，&backupwallet，“目的地”，
    “wallet”、“bumpfee”、&bumpfee、“txid”、“options”，
    “wallet”、“createwallet”、&createwallet、“walletname”、“disable_private_keys”，
    “wallet”，“dumpprivkey”，&dumpprivkey，“address”，
    “wallet”，“dumpwallet”，&dumpwallet，“文件名”，
    “wallet”，“encryptwallet”，&encryptwallet，“passphrase”，
    “wallet”、“getaddressesbylabel”、&getaddressesbylabel、“label”，
    “wallet”，“getaddressinfo”，&getaddressinfo，“address”，
    “wallet”，“getbalance”，&getbalance，“dummy”，“minconf”，“include_watchonly”，
    “wallet”，“getnewaddress”，&getnewaddress，“label”，“address”，
    “wallet”，“getrawchangeaddress”，&getrawchangeaddress，“address”，
    “wallet”，“getreceivedbyaddress”，&getreceivedbyaddress，“address”，“minconf”，
    “wallet”，“getreceivedbylabel”，&getreceivedbylabel，“label”，“minconf”，
    “wallet”，“getTransaction”，&getTransaction，“txid”，“include_watchonly”，
    “wallet”，“getunconfirmedbalance”，&getunconfirmedbalance，，
    “wallet”，“getwalletinfo”，&getwalletinfo，，
    “wallet”、“importaddress”、&importaddress、“address”、“label”、“rescan”、“p2sh”，
    “wallet”，“importmulti”，&importmulti，“requests”，“options”，
    “wallet”、“importprivkey”、&importprivkey、“privkey”、“label”、“rescan”，
    “wallet”、“importedprunedfunds”、“importedprunedfunds”、“rawtransaction”、“txouttop”，
    “wallet”，“importpubkey”，&importpubkey，“pubkey”，“label”，“rescan”，
    “wallet”，“importwallet”，&importwallet，“文件名”，
    “wallet”、“keypoolrefill”、&keypoolrefill、“newsize”，
    “wallet”，“listaddressgroupings”，&listaddressgroupings，，
    “Wallet”、“ListLabels”、&ListLabels、“用途”，
    “wallet”、“listlockunspent”和listlockunspent，，
    “wallet”，“lisreceivedbyaddress”，&lisreceivedbyaddress，“minconf”，“include_empty”，“include_watchonly”，“address_filter”，
    “wallet”，“lisreceivedbylabel”，&lisreceivedbylabel，“minconf”，“include_empty”，“include_watchonly”，
    “wallet”，“listsinceblock”，&listsinceblock，“blockhash”，“target_confirmations”，“include_watchonly”，“include_removed”，
    “Wallet”、“ListTransactions”、&ListTransactions、“Label Dummy”、“Count”、“Skip”、“Include WatchOnly”，
    “wallet”、“listunspent”、&listunspent、“minconf”、“maxconf”、“addresses”、“include_unsafe”、“query_options”，
    “wallet”、“listwalletdir”、&listwalletdir，，
    “wallet”、“listwallets”&listwallets，，
    “wallet”，“loadwallet”，&loadwallet，“文件名”，
    “wallet”、“lockunspent”、&lockunspent、“unlock”、“transactions”，
    “wallet”，“removeprunedfunds”，&removeprunedfunds，“txid”，
    “Wallet”、“RescanBlockchain”、&RescanBlockchain、“Start_Height”、“Stop_Height”，
    “wallet”、“sendmany”、&sendmany、“dummy”、“amount”、“minconf”、“comment”、“subtract feefrom”、“replacement”、“conf_target”、“estimate_mode”，
    “wallet”，“sendtoAddress”，&sendtoAddress，“address”，“amount”，“comment”，“comment_to”，“subtract feefromount”，“replacement”，“conf_target”，“estimate_mode”，
    “wallet”，“sethdseed”，&sethdseed，“newkeypool”，“seed”，
    “wallet”，“setlabel”，&setlabel，“address”，“label”，
    “wallet”、“settxfee”、&settxfee、“amount”，
    “wallet”、“signmessage”、&signmessage、“address”、“message”，
    “wallet”、“signrawtransactionwithwallet”、&signrawtransactionwithwallet、“hexstring”、“prevtxs”、“sigashtype”，
    “Wallet”、“UnloadWallet”、&UnloadWallet、“Wallet”，
    “wallet”，“walletcreatefundedpsbt”，&walletcreatefundedpsbt，“inputs”，“outputs”，“locktime”，“options”，“bip32derivals”，
    “钱包”，“钱包锁”，&walletlock，，
    “wallet”，“wallet passphrase”，&wallet passphrase，“passphrase”，“timeout”，
    “wallet”，“walletpassphrasechange”，&walletpassphrasechange，“oldpassphrase”，“newpassphrase”，
    “wallet”，“walletprocesspsbt”，&walletprocesspsbt，“psbt”，“sign”，“sighashtype”，“bip32derivals”，
}；
//打开Clang格式

无效的注册墙命令（crpctable&t）
{
    for（unsigned int vcidx=0；vcidx<arraylen（commands）；vcidx++）
        t.appendcommand（命令[vcidx].name，&commands[vcidx]）；
}
