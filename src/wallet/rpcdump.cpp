
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

#include <chain.h>
#include <core_io.h>
#include <interfaces/chain.h>
#include <key_io.h>
#include <merkleblock.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <script/script.h>
#include <script/standard.h>
#include <sync.h>
#include <util/system.h>
#include <util/time.h>
#include <validation.h>
#include <wallet/wallet.h>

#include <wallet/rpcwallet.h>

#include <stdint.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <univalue.h>


int64_t static DecodeDumpTime(const std::string &str) {
    static const boost::posix_time::ptime epoch = boost::posix_time::from_time_t(0);
    static const std::locale loc(std::locale::classic(),
        new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%SZ"));
    std::istringstream iss(str);
    iss.imbue(loc);
    boost::posix_time::ptime ptime(boost::date_time::not_a_date_time);
    iss >> ptime;
    if (ptime.is_not_a_date_time())
        return 0;
    return (ptime - epoch).total_seconds();
}

std::string static EncodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (const unsigned char c : str) {
        if (c <= 32 || c >= 128 || c == '%') {
            ret << '%' << HexStr(&c, &c + 1);
        } else {
            ret << c;
        }
    }
    return ret.str();
}

static std::string DecodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (unsigned int pos = 0; pos < str.length(); pos++) {
        unsigned char c = str[pos];
        if (c == '%' && pos+2 < str.length()) {
            c = (((str[pos+1]>>6)*9+((str[pos+1]-'0')&15)) << 4) |
                ((str[pos+2]>>6)*9+((str[pos+2]-'0')&15));
            pos += 2;
        }
        ret << c;
    }
    return ret.str();
}

static bool GetWalletAddressesForKey(CWallet * const pwallet, const CKeyID &keyid, std::string &strAddr, std::string &strLabel)
{
    bool fLabelFound = false;
    CKey key;
    pwallet->GetKey(keyid, key);
    for (const auto& dest : GetAllDestinationsForKey(key.GetPubKey())) {
        if (pwallet->mapAddressBook.count(dest)) {
            if (!strAddr.empty()) {
                strAddr += ",";
            }
            strAddr += EncodeDestination(dest);
            strLabel = EncodeDumpString(pwallet->mapAddressBook[dest].name);
            fLabelFound = true;
        }
    }
    if (!fLabelFound) {
        strAddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), pwallet->m_default_address_type));
    }
    return fLabelFound;
}

static const int64_t TIMESTAMP_MIN = 0;

static void RescanWallet(CWallet& wallet, const WalletRescanReserver& reserver, int64_t time_begin = TIMESTAMP_MIN, bool update = true)
{
    int64_t scanned_time = wallet.RescanFromTime(time_begin, reserver, update);
    if (wallet.IsAbortingRescan()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Rescan aborted by user.");
    } else if (scanned_time > time_begin) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Rescan was unable to fully rescan the blockchain. Some transactions may be missing.");
    }
}

UniValue importprivkey(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw std::runtime_error(
            RPCHelpMan{"importprivkey",
                "\nAdds a private key (as returned by dumpprivkey) to your wallet. Requires a new wallet backup.\n"
                "Hint: use importmulti to import more than one private key.\n",
                {
                    /*rivkey“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，“私钥（请参阅dumpprivkey）”，
                    “label”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "current label if address exists, otherwise \"\"", "An optional label"},

                    /*escan“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”true“，”重新扫描钱包进行交易“，
                }
                toSTRIN（）+
            注意：如果重新扫描为真，则此调用可能需要一个多小时才能完成。在此期间，其他RPC调用\n
            可能报告导入的密钥存在，但相关事务仍然丢失，导致暂时不正确/虚假的余额和未暂停的输出，直到重新扫描完成。\n
            “\n实例：\n”
            “执行私钥\n”
            +helpexamplecli（“dumpprivkey”，“\”myaddress\”“）+
            \n用重新扫描导入私钥\n
            +helpExamplecli（“importprivkey”，“\”mykey\”“）+
            \n使用标签导入而不重新扫描\n
            +helpExamplecli（“importprivkey”，“mykey”，“testing”，“false”）+
            \n导入时使用默认的空白标签，不重新扫描\n
            +helpExamplecli（“importprivkey”，“\”mykey\“\”\“false”）+
            “作为JSON-RPC调用\n”
            +helpExampleRpc（“importprivkey”，“mykey”，“testing”，“false”）。
        ；


    墙式储罐（PWallet）；
    布尔·弗雷斯坎=真；
    {
        自动锁定的_chain=pwallet->chain（）.lock（）；
        锁（pwallet->cs_wallet）；

        确保已解锁（PWallet）；

        std:：string strsecret=request.params[0].get_str（）；
        std:：string strlabel=“”；
        如果（！）请求.params[1].isNull（））
            strLabel=request.params[1].get_str（）；

        //导入后是否重新扫描
        如果（！）请求.params[2].isNull（））
            frescan=request.params[2].获取bool（）；

        如果（frescan和fprunemode）
            throw jsonrpcerror（rpc_wallet_error，“rescan is disabled in pruned mode”）；

        如果（壁画&&！reserver.reserve（））
            throw jsonrpcerror（rpc_wallet_error），wallet当前正在重新扫描。中止现有的重新扫描或等待。“）；
        }

        ckey key=decodeSecret（strSecret）；
        如果（！）key.isvalid（））throw jsonrpcerror（rpc_invalid_address_or_key，“invalid private key encoding”）；

        cpubkey pubkey=key.getpubkey（）；
        断言（key.verifypubkey（pubkey））；
        ckeyID vchaddress=pubkey.getid（）；
        {
            pwallet->markdirty（）；

            //我们不知道使用哪个对应的地址；
            //标记所有新地址，如果
            //传递了标签。
            for（const auto&dest:getAllDestinationsForMarkey（pubkey））
                如果（！）request.params[1].isNull（）pWallet->mapAddressBook.Count（dest）==0）
                    pwallet->setaddressbook（dest，strlabel，“receive”）；
                }
            }

            //不要在已经有键的情况下抛出错误
            如果（pwallet->havekey（vchaddress））
                返回nullunivalue；
            }

            //每次导入密钥时，都需要扫描整个链
            pwallet->updatetimefirstkey（1）；
            pwallet->mapkeymetadata[vchaddress].ncreateTime=1；

            如果（！）pwallet->addkeypubkey（key，pubkey））
                throw jsonrpcerror（rpc_wallet_error，“向wallet添加密钥时出错”）；
            }
            pwallet->learnallrelatedscripts（pubkey）；
        }
    }
    如果（弗雷斯坎）{
        rescanwallet（*pwallet，reserver）；
    }

    返回nullunivalue；
}

单值异常扫描（const jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；
    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    if（request.fhelp request.params.size（）>0）
        throw std:：runtime_错误（
            rpchelpman“AbortRescan”，
                \n停止由rpc调用触发的当前钱包重新扫描，例如由importprivkey调用触发的钱包重新扫描。\n“，
                toSTRIN（）+
            “\n实例：\n”
            “导入私钥\n”
            +helpExamplecli（“importprivkey”，“\”mykey\”“）+
            \n导入正在运行的钱包重新扫描\n
            +helpExamplecli（“AbortRescan”，“）+
            “作为JSON-RPC调用\n”
            +helpExampleRPC（“AbortRescan”，“”）
        ；

    如果（！）pwallet->isscanning（）pwallet->isabortingRescan（））返回false；
    pWallet->AbortRescan（）；
    回归真实；
}

静态void importaddress（cwallet*，const ctxdestination&dest，const std:：string&strlabel）；
静态void importscript（cwallet*const pwallet，const cscript&script，const std:：string&strlabel，bool isredeemscript）需要专用锁（pwallet->cs_wallet）
{
    如果（！）isredeemscript&：：ismine（*pwallet，script）==ismine
        throw jsonrpcerror（rpc_wallet_error，“钱包已经包含此地址或脚本的私钥”）；
    }

    pwallet->markdirty（）；

    如果（！）pWallet->HaveWatchOnly（脚本）&&！pwallet->addwatchonly（脚本，0/*ncreateTime*/)) {

        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding address to wallet");
    }

    if (isRedeemScript) {
        const CScriptID id(script);
        if (!pwallet->HaveCScript(id) && !pwallet->AddCScript(script)) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Error adding p2sh redeemScript to wallet");
        }
        ImportAddress(pwallet, id, strLabel);
    } else {
        CTxDestination destination;
        if (ExtractDestination(script, destination)) {
            pwallet->SetAddressBook(destination, strLabel, "receive");
        }
    }
}

static void ImportAddress(CWallet* const pwallet, const CTxDestination& dest, const std::string& strLabel) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet)
{
    CScript script = GetScriptForDestination(dest);
    ImportScript(pwallet, script, strLabel, false);
//添加到通讯簿或更新标签
    if (IsValidDestination(dest))
        pwallet->SetAddressBook(dest, strLabel, "receive");
}

UniValue importaddress(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 4)
        throw std::runtime_error(
            RPCHelpMan{"importaddress",
                "\nAdds an address or script (in hex) that can be watched as if it were in your wallet but cannot be used to spend. Requires a new wallet backup.\n",
                {
                    /*地址“，rpcarg:：type:：str，/*opt*/false，/*默认值“/”，“比特币地址（或十六进制编码脚本）”，
                    “label”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "\"\"", "An optional label"},

                    /*escan“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”true“，”重新扫描钱包进行交易“，
                    “p2sh”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "false", "Add the P2SH version of the script as well"},

                }}
                .ToString() +
            "\nNote: This call can take over an hour to complete if rescan is true, during that time, other rpc calls\n"
            "may report that the imported address exists but related transactions are still missing, leading to temporarily incorrect/bogus balances and unspent outputs until rescan completes.\n"
            "If you have the full public key, you should call importpubkey instead of this.\n"
            "\nNote: If you import a non-standard raw script in hex form, outputs sending to it will be treated\n"
            "as change, and not show up in many RPCs.\n"
            "\nExamples:\n"
            "\nImport an address with rescan\n"
            + HelpExampleCli("importaddress", "\"myaddress\"") +
            "\nImport using a label without rescan\n"
            + HelpExampleCli("importaddress", "\"myaddress\" \"testing\" false") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("importaddress", "\"myaddress\", \"testing\", false")
        );


    std::string strLabel;
    if (!request.params[1].isNull())
        strLabel = request.params[1].get_str();

//导入后是否重新扫描
    bool fRescan = true;
    if (!request.params[2].isNull())
        fRescan = request.params[2].get_bool();

    if (fRescan && fPruneMode)
        throw JSONRPCError(RPC_WALLET_ERROR, "Rescan is disabled in pruned mode");

    WalletRescanReserver reserver(pwallet);
    if (fRescan && !reserver.reserve()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet is currently rescanning. Abort existing rescan or wait.");
    }

//是否也导入p2sh版本
    bool fP2SH = false;
    if (!request.params[3].isNull())
        fP2SH = request.params[3].get_bool();

    {
        auto locked_chain = pwallet->chain().lock();
        LOCK(pwallet->cs_wallet);

        CTxDestination dest = DecodeDestination(request.params[0].get_str());
        if (IsValidDestination(dest)) {
            if (fP2SH) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Cannot use the p2sh flag with an address - use a script instead");
            }
            ImportAddress(pwallet, dest, strLabel);
        } else if (IsHex(request.params[0].get_str())) {
            std::vector<unsigned char> data(ParseHex(request.params[0].get_str()));
            ImportScript(pwallet, CScript(data.begin(), data.end()), strLabel, fP2SH);
        } else {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Bitcoin address or script");
        }
    }
    if (fRescan)
    {
        RescanWallet(*pwallet, reserver);
        pwallet->ReacceptWalletTransactions();
    }

    return NullUniValue;
}

UniValue importprunedfunds(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            RPCHelpMan{"importprunedfunds",
                "\nImports funds without rescan. Corresponding address or script must previously be included in wallet. Aimed towards pruned wallets. The end-user is responsible to import additional transactions that subsequently spend the imported outputs or rescan after the point in the blockchain the transaction is included.\n",
                {
                    /*awtransaction“，rpcarg:：type:：str_hex，/*opt*/false，/*default_val*/”“，“一个十六进制的原始交易为钱包中已有的地址提供资金”，
                    “txOutpRoof”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "The hex output from gettxoutproof that contains the transaction"},

                }}
                .ToString()
        );

    CMutableTransaction tx;
    if (!DecodeHexTx(tx, request.params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    uint256 hashTx = tx.GetHash();
    CWalletTx wtx(pwallet, MakeTransactionRef(std::move(tx)));

    CDataStream ssMB(ParseHexV(request.params[1], "proof"), SER_NETWORK, PROTOCOL_VERSION);
    CMerkleBlock merkleBlock;
    ssMB >> merkleBlock;

//搜索部分Merkle树以证明我们的交易和有效块中的索引
    std::vector<uint256> vMatch;
    std::vector<unsigned int> vIndex;
    unsigned int txnIndex = 0;
    if (merkleBlock.txn.ExtractMatches(vMatch, vIndex) == merkleBlock.header.hashMerkleRoot) {

        auto locked_chain = pwallet->chain().lock();
        const CBlockIndex* pindex = LookupBlockIndex(merkleBlock.header.GetHash());
        if (!pindex || !chainActive.Contains(pindex)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found in chain");
        }

        std::vector<uint256>::const_iterator it;
        if ((it = std::find(vMatch.begin(), vMatch.end(), hashTx))==vMatch.end()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction given doesn't exist in proof");
        }

        txnIndex = vIndex[it - vMatch.begin()];
    }
    else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Something wrong with merkleblock");
    }

    wtx.nIndex = txnIndex;
    wtx.hashBlock = merkleBlock.header.GetHash();

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    if (pwallet->IsMine(*wtx.tx)) {
        pwallet->AddToWallet(wtx, false);
        return NullUniValue;
    }

    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No addresses in wallet correspond to included transaction");
}

UniValue removeprunedfunds(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"removeprunedfunds",
                "\nDeletes the specified transaction from the wallet. Meant for use with pruned wallets and as a companion to importprunedfunds. This will affect wallet balances.\n",
                {
                    /*xid“，rpcarg:：type:：str_hex，/*opt*/false，/*default_val*/”，“要删除的事务的十六进制编码ID”，
                }
                toSTRIN（）+
            “\n实例：\n”
            +helpExamplecli（“removeprunedfunds”，“a8d0c0184dde994a09ec054286f1ce581bebf46a51266eae7628734ea0a5\”“）+
            “作为JSON-RPC调用\n”
            +helpExampleRpc（“removeprunedfunds”，“a8d0c184dde994a09ec054286f1ce581bebf6446a51266eae7628734ea0a5\”）
        ；

    自动锁定的_chain=pwallet->chain（）.lock（）；
    锁（pwallet->cs_wallet）；

    uint256 hash（parsehashv（request.params[0]，“txid”））；
    std:：vector<uint256>vhash；
    vhash.向后推（hash）；
    std:：vector<uint256>vhashout；

    如果（pwallet->zapselectx（vhash，vhashout）！=dberrors：：加载_OK）
        throw jsonrpcerror（rpc_wallet_error，“无法正确删除事务”）；
    }

    if（vhashout.empty（））
        throw jsonrpcerror（rpc_invalid_参数，“钱包中不存在交易”）；
    }

    返回nullunivalue；
}

单值importpubkey（const-jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；
    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    if（request.fhelp request.params.size（）<1 request.params.size（）>3）
        throw std:：runtime_错误（
            rpchelpman“importpubkey”，
                \n添加一个公钥（十六进制），它可以像在钱包中一样被监视，但不能用于消费。需要新的钱包备份。\n“，
                {
                    “pubkey”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The hex-encoded public key"},

                    /*abel“，rpcarg:：type:：str，/*opt*/true，/*默认值\ val*/”\“”，“可选标签”，
                    “rescan”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "true", "Rescan the wallet for transactions"},

                }}
                .ToString() +
            "\nNote: This call can take over an hour to complete if rescan is true, during that time, other rpc calls\n"
            "may report that the imported pubkey exists but related transactions are still missing, leading to temporarily incorrect/bogus balances and unspent outputs until rescan completes.\n"
            "\nExamples:\n"
            "\nImport a public key with rescan\n"
            + HelpExampleCli("importpubkey", "\"mypubkey\"") +
            "\nImport using a label without rescan\n"
            + HelpExampleCli("importpubkey", "\"mypubkey\" \"testing\" false") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("importpubkey", "\"mypubkey\", \"testing\", false")
        );


    std::string strLabel;
    if (!request.params[1].isNull())
        strLabel = request.params[1].get_str();

//导入后是否重新扫描
    bool fRescan = true;
    if (!request.params[2].isNull())
        fRescan = request.params[2].get_bool();

    if (fRescan && fPruneMode)
        throw JSONRPCError(RPC_WALLET_ERROR, "Rescan is disabled in pruned mode");

    WalletRescanReserver reserver(pwallet);
    if (fRescan && !reserver.reserve()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet is currently rescanning. Abort existing rescan or wait.");
    }

    if (!IsHex(request.params[0].get_str()))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Pubkey must be a hex string");
    std::vector<unsigned char> data(ParseHex(request.params[0].get_str()));
    CPubKey pubKey(data.begin(), data.end());
    if (!pubKey.IsFullyValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Pubkey is not a valid public key");

    {
        auto locked_chain = pwallet->chain().lock();
        LOCK(pwallet->cs_wallet);

        for (const auto& dest : GetAllDestinationsForKey(pubKey)) {
            ImportAddress(pwallet, dest, strLabel);
        }
        ImportScript(pwallet, GetScriptForRawPubKey(pubKey), strLabel, false);
        pwallet->LearnAllRelatedScripts(pubKey);
    }
    if (fRescan)
    {
        RescanWallet(*pwallet, reserver);
        pwallet->ReacceptWalletTransactions();
    }

    return NullUniValue;
}


UniValue importwallet(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"importwallet",
                "\nImports keys from a wallet dump file (see dumpwallet). Requires a new wallet backup to include imported keys.\n",
                {
                    /*ilename“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，“钱包文件”，
                }
                toSTRIN（）+
            “\n实例：\n”
            \n打开钱包\n
            +helpExamplecli（“dumpwallet”，“\”测试\“”）+
            “导入钱包\n”
            +helpExamplecli（“导入钱包”，“测试\”）+
            \n使用JSON RPC调用导入\n
            +helpExampleRPC（“导入钱包”，“测试\”）。
        ；

    如果（FPRONMODED）
        throw jsonrpcerror（rpc_wallet_error，“导入钱包在修剪模式下被禁用”）；

    墙式储罐（PWallet）；
    如果（！）reserver.reserve（））
        throw jsonrpcerror（rpc_wallet_error），wallet当前正在重新扫描。中止现有的重新扫描或等待。“）；
    }

    Int64时间开始=0；
    bool fgood=真；
    {
        自动锁定的_chain=pwallet->chain（）.lock（）；
        锁（pwallet->cs_wallet）；

        确保已解锁（PWallet）；

        fsbridge:：ifstream文件；
        file.open（request.params[0].get_str（），std:：ios:：in_std:：ios:：ate）；
        如果（！）文件.is_open（））
            throw jsonrpcerror（rpc_invalid_参数，“cannot open wallet dump file”）；
        }
        ntimebegin=chainactive.tip（）->getBlockTime（）；

        int64_t nfileSize=std:：max（（int64_t）1，（int64_t）file.tellg（））；
        file.seekg（0，file.beg）；

        //使用uiinterface.showprogress而不是pwallet.showprogress，因为pwallet.showprogress有一个与AbortRescan绑定的取消按钮，
        //我们不希望此进度条显示导入进度。uiinterface.showprogress没有取消按钮。
        uiinterface.show progress（strprintf（“%s”+“导入…”），pwallet->getdisplayname（）），0，false）；//在GUI中显示进度对话框
        while（file.good（））
            uiinterface.showprogress（“”，std:：max（1，std:：min（99，（int）（（double）file.tellg（）/（double）filesize）*100）），false）；
            std：：字符串行；
            std:：getline（文件，行）；
            if（line.empty（）line[0]=''）
                继续；

            std:：vector<std:：string>vstr；
            boost:：split（vstr，line，boost:：is_any_of（“”）；
            如果（vstr.size（）<2）
                继续；
            ckey key=decodeSecret（vstr[0]）；
            if（key.isvalid（））
                cpubkey pubkey=key.getpubkey（）；
                断言（key.verifypubkey（pubkey））；
                ckeyid keyid=pubkey.getid（）；
                if（pwallet->havekey（keyid））
                    pwallet->walletlogprintf（“跳过导入%s（密钥已存在），”encodedestination（keyid））；
                    继续；
                }
                int64_t ntime=解码转储时间（vstr[1]）；
                std：：字符串strLabel；
                bool flabel=真；
                for（unsigned int nstr=2；nstr<vstr.size（）；nstr++）
                    if（vstr[nstr].front（）=''）
                        断裂；
                    如果（vstr[nstr]=“change=1”）
                        FLABEL=假；
                    如果（vstr[nstr]=“reserve=1”）
                        FLABEL=假；
                    if（vstr[nstr].substr（0,6）=“label=”）
                        strlabel=decodedumpstring（vstr[nstr].substr（6））；
                        弗拉贝尔=真；
                    }
                }
                pwallet->walletlogprintf（“导入%s…\n”，encodedestination（keyid））；
                如果（！）pwallet->addkeypubkey（key，pubkey））
                    FoGo=假；
                    继续；
                }
                pwallet->mapkeymetadata[keyid].ncreateTime=ntime；
                如果（FLABEL）
                    pwallet->setAddressBook（keyid，strLabel，“接收”）；
                ntimebegin=std:：min（ntimebegin，ntime）；
            else if（ishex（vstr[0]））
               std:：vector<unsigned char>vdata（parseHex（vstr[0]））；
               cscript script=cscript（vdata.begin（），vdata.end（））；
               cscriptid id（脚本）；
               if（pwallet->havecscript（id））
                   pwallet->walletlogprintf（“跳过导入%s（脚本已存在），vstr[0]）；
                   继续；
               }
               如果（！）pwallet->addcscript（script））
                   pwallet->walletlogprintf（“导入脚本%s时出错，vstr[0]）；
                   FoGo=假；
                   继续；
               }
               Int64_t Birth_Time=解码转储时间（vstr[1]）；
               如果（出生时间>0）
                   pwallet->m_script_metadata[id].ncreateTime=出生时间；
                   ntimebegin=std:：min（ntimebegin，出生时间）；
               }
            }
        }
        文件；
        uiinterface.showprogress（“”，100，false）；//在GUI中隐藏进度对话框
        pwallet->updatetimefirstkey（ntimebegin）；
    }
    uiinterface.showprogress（“”，100，false）；//在GUI中隐藏进度对话框
    rescanwallet（*pwallet，reserver，ntimebegin，false/*更新*/);

    pwallet->MarkDirty();

    if (!fGood)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding some keys/scripts to wallet");

    return NullUniValue;
}

UniValue dumpprivkey(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"dumpprivkey",
                "\nReveals the private key corresponding to 'address'.\n"
                "Then the importprivkey can be used with this output\n",
                {
                    /*地址“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，“私钥的比特币地址”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “key”（字符串）私钥\n”
            “\n实例：\n”
            +helpexamplecli（“dumpprivkey”，“myaddress\”）
            +helpexamplecli（“importprivkey”，“mykey\”）
            +helpexamplerpc（“dumpprivkey”，“\”myaddress\”）
        ；

    自动锁定的_chain=pwallet->chain（）.lock（）；
    锁（pwallet->cs_wallet）；

    确保已解锁（PWallet）；

    std:：string straddress=request.params[0].get_str（）；
    ctxdestination dest=解码目的地（跨地址）；
    如果（！）isvaliddestination（目的地））
        throw jsonrpcerror（rpc_invalid_address_or_key，“无效比特币地址”）；
    }
    auto keyid=getkeywordestination（*pwallet，dest）；
    if（keyid.isNull（））
        throw jsonrpcerror（rpc_type_error，“address does not refer to a key”）；
    }
    CKey vchSecret；
    如果（！）pwallet->getkey（keyid，vchsecret））
        throw jsonrpcerror（rpc_wallet_error，“不知道地址”+straddress+“的私钥”）；
    }
    返回encodesecret（vchsecret）；
}


单值dumpwallet（const jsonrpcrequest&request）
{
    std:：shared_ptr<cwallet>const wallet=getwalletforjsonrpcrequest（请求）；
    cwallet*const pwallet=wallet.get（）；
    如果（！）确保所有可用（pwallet，request.fhelp））
        返回nullunivalue；
    }

    如果（request.fhelp request.params.size（）！= 1）
        throw std:：runtime_错误（
            rpchelpman“dumpwallet”，
                \n将所有钱包密钥以可读的格式发送到服务器端文件。这不允许覆盖现有文件。\n“
                导入的脚本包含在转储文件中，但相应的bip173地址等不能由importwallet自动添加。\n
                请注意，如果您的钱包中包含不是从您的HD种子派生的密钥（例如导入的密钥），则这些密钥不包含在\n
                “只备份种子本身，也必须备份（例如，确保备份整个转储文件）。\n”，
                {
                    “文件名”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The filename with path (either absolute or relative to bitcoind)"},

                }}
                .ToString() +
            "\nResult:\n"
            "{                           (json object)\n"
            "  \"filename\" : {        (string) The filename with full absolute path\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("dumpwallet", "\"test\"")
            + HelpExampleRpc("dumpwallet", "\"test\"")
        );

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    fs::path filepath = request.params[0].get_str();
    filepath = fs::absolute(filepath);

    /*防止覆盖任意文件。有报道说
     *用户以这种方式覆盖钱包文件：
     *https://github.com/bitcoin/bitcoin/issues/9934
     *还可以避免其他安全问题。
     **/

    if (fs::exists(filepath)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, filepath.string() + " already exists. If you are sure this is what you want, move it out of the way first");
    }

    fsbridge::ofstream file;
    file.open(filepath);
    if (!file.is_open())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open wallet dump file");

    std::map<CTxDestination, int64_t> mapKeyBirth;
    const std::map<CKeyID, int64_t>& mapKeyPool = pwallet->GetAllReserveKeys();
    pwallet->GetKeyBirthTimes(*locked_chain, mapKeyBirth);

    std::set<CScriptID> scripts = pwallet->GetCScripts();
//TODO:在getKeyBirthTimes（）输出中包含脚本，而不是单独

//排序时间/密钥对
    std::vector<std::pair<int64_t, CKeyID> > vKeyBirth;
    for (const auto& entry : mapKeyBirth) {
if (const CKeyID* keyID = boost::get<CKeyID>(&entry.first)) { //设置与测试
            vKeyBirth.push_back(std::make_pair(entry.second, *keyID));
        }
    }
    mapKeyBirth.clear();
    std::sort(vKeyBirth.begin(), vKeyBirth.end());

//出产量
    file << strprintf("# Wallet dump created by Bitcoin %s\n", CLIENT_BUILD);
    file << strprintf("# * Created on %s\n", FormatISO8601DateTime(GetTime()));
    file << strprintf("# * Best block at time of backup was %i (%s),\n", chainActive.Height(), chainActive.Tip()->GetBlockHash().ToString());
    file << strprintf("#   mined on %s\n", FormatISO8601DateTime(chainActive.Tip()->GetBlockTime()));
    file << "\n";

//如果钱包使用HD，则添加base58检查编码的扩展主控形状。
    CKeyID seed_id = pwallet->GetHDChain().seed_id;
    if (!seed_id.IsNull())
    {
        CKey seed;
        if (pwallet->GetKey(seed_id, seed)) {
            CExtKey masterKey;
            masterKey.SetSeed(seed.begin(), seed.size());

            file << "# extended private masterkey: " << EncodeExtKey(masterKey) << "\n\n";
        }
    }
    for (std::vector<std::pair<int64_t, CKeyID> >::const_iterator it = vKeyBirth.begin(); it != vKeyBirth.end(); it++) {
        const CKeyID &keyid = it->second;
        std::string strTime = FormatISO8601DateTime(it->first);
        std::string strAddr;
        std::string strLabel;
        CKey key;
        if (pwallet->GetKey(keyid, key)) {
            file << strprintf("%s %s ", EncodeSecret(key), strTime);
            if (GetWalletAddressesForKey(pwallet, keyid, strAddr, strLabel)) {
               file << strprintf("label=%s", strLabel);
            } else if (keyid == seed_id) {
                file << "hdseed=1";
            } else if (mapKeyPool.count(keyid)) {
                file << "reserve=1";
            } else if (pwallet->mapKeyMetadata[keyid].hdKeypath == "s") {
                file << "inactivehdseed=1";
            } else {
                file << "change=1";
            }
            file << strprintf(" # addr=%s%s\n", strAddr, (pwallet->mapKeyMetadata[keyid].hdKeypath.size() > 0 ? " hdkeypath="+pwallet->mapKeyMetadata[keyid].hdKeypath : ""));
        }
    }
    file << "\n";
    for (const CScriptID &scriptid : scripts) {
        CScript script;
        std::string create_time = "0";
        std::string address = EncodeDestination(scriptid);
//获取具有元数据的脚本的出生时间
        auto it = pwallet->m_script_metadata.find(scriptid);
        if (it != pwallet->m_script_metadata.end()) {
            create_time = FormatISO8601DateTime(it->second.nCreateTime);
        }
        if(pwallet->GetCScript(scriptid, script)) {
            file << strprintf("%s %s script=1", HexStr(script.begin(), script.end()), create_time);
            file << strprintf(" # addr=%s\n", address);
        }
    }
    file << "\n";
    file << "# End of dump\n";
    file.close();

    UniValue reply(UniValue::VOBJ);
    reply.pushKV("filename", filepath.string());

    return reply;
}

struct ImportData
{
//输入数据
std::unique_ptr<CScript> redeemscript; //！<provided redeemscript；将移动到“import\u scripts”（如果相关）。
std::unique_ptr<CScript> witnessscript; //！<provided witnessscript；将移动到“导入脚本”（如果相关）。

//输出数据
    std::set<CScript> import_scripts;
std::map<CKeyID, bool> used_keys; //！<import these private keys if available（如果可用，则导入这些私钥）（该值指示是否需要该密钥才能解决问题）
};

enum class ScriptContext
{
TOP, //！顶级scriptpubkey
P2SH, //！p2sh修改脚本
WITNESS_V0, //！p2wsh见证脚本
};

//分析提供的scriptpubkey，确定需要哪些键和哪些从importdata结构中提取脚本来使用它，并将它们标记为已使用。
//返回一个错误字符串或空字符串以获得成功。
static std::string RecurseImportData(const CScript& script, ImportData& import_data, const ScriptContext script_ctx)
{
//使用解算器获取脚本类型和已分析的公钥或哈希：
    std::vector<std::vector<unsigned char>> solverdata;
    txnouttype script_type = Solver(script, solverdata);

    switch (script_type) {
    case TX_PUBKEY: {
        CPubKey pubkey(solverdata[0].begin(), solverdata[0].end());
        import_data.used_keys.emplace(pubkey.GetID(), false);
        return "";
    }
    case TX_PUBKEYHASH: {
        CKeyID id = CKeyID(uint160(solverdata[0]));
        import_data.used_keys[id] = true;
        return "";
    }
    case TX_SCRIPTHASH: {
        if (script_ctx == ScriptContext::P2SH) throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Trying to nest P2SH inside another P2SH");
        if (script_ctx == ScriptContext::WITNESS_V0) throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Trying to nest P2SH inside a P2WSH");
        assert(script_ctx == ScriptContext::TOP);
        CScriptID id = CScriptID(uint160(solverdata[0]));
auto subscript = std::move(import_data.redeemscript); //从导入数据中删除redeemscript，以便稍后检查是否有多余的脚本。
        if (!subscript) return "missing redeemscript";
        if (CScriptID(*subscript) != id) return "redeemScript does not match the scriptPubKey";
        import_data.import_scripts.emplace(*subscript);
        return RecurseImportData(*subscript, import_data, ScriptContext::P2SH);
    }
    case TX_MULTISIG: {
        for (size_t i = 1; i + 1< solverdata.size(); ++i) {
            CPubKey pubkey(solverdata[i].begin(), solverdata[i].end());
            import_data.used_keys.emplace(pubkey.GetID(), false);
        }
        return "";
    }
    case TX_WITNESS_V0_SCRIPTHASH: {
        if (script_ctx == ScriptContext::WITNESS_V0) throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Trying to nest P2WSH inside another P2WSH");
        uint256 fullid(solverdata[0]);
        CScriptID id;
        CRIPEMD160().Write(fullid.begin(), fullid.size()).Finalize(id.begin());
auto subscript = std::move(import_data.witnessscript); //从导入数据中删除redeemscript，以便稍后检查是否有多余的脚本。
        if (!subscript) return "missing witnessscript";
        if (CScriptID(*subscript) != id) return "witnessScript does not match the scriptPubKey or redeemScript";
        if (script_ctx == ScriptContext::TOP) {
import_data.import_scripts.emplace(script); //ismine的特殊规则：本机p2wsh需要导入顶级脚本（请参阅script/ismine.cpp）
        }
        import_data.import_scripts.emplace(*subscript);
        return RecurseImportData(*subscript, import_data, ScriptContext::WITNESS_V0);
    }
    case TX_WITNESS_V0_KEYHASH: {
        if (script_ctx == ScriptContext::WITNESS_V0) throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Trying to nest P2WPKH inside P2WSH");
        CKeyID id = CKeyID(uint160(solverdata[0]));
        import_data.used_keys[id] = true;
        if (script_ctx == ScriptContext::TOP) {
import_data.import_scripts.emplace(script); //ismine的特殊规则：本机p2wpkh需要导入顶级脚本（请参阅script/ismine.cpp）
        }
        return "";
    }
    case TX_NULL_DATA:
        return "unspendable script";
    case TX_NONSTANDARD:
    case TX_WITNESS_UNKNOWN:
    default:
        return "unrecognized script";
    }
}

static UniValue ProcessImport(CWallet * const pwallet, const UniValue& data, const int64_t timestamp) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet)
{
    UniValue warnings(UniValue::VARR);
    UniValue result(UniValue::VOBJ);

    try {
//首先确保scriptpubkey有一个带有“地址”字符串的脚本或JSON
        const UniValue& scriptPubKey = data["scriptPubKey"];
        bool isScript = scriptPubKey.getType() == UniValue::VSTR;
        if (!isScript && !(scriptPubKey.getType() == UniValue::VOBJ && scriptPubKey.exists("address"))) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "scriptPubKey must be string with script or JSON with address string");
        }
        const std::string& output = isScript ? scriptPubKey.get_str() : scriptPubKey["address"].get_str();

//可选字段。
        const std::string& strRedeemScript = data.exists("redeemscript") ? data["redeemscript"].get_str() : "";
        const std::string& witness_script_hex = data.exists("witnessscript") ? data["witnessscript"].get_str() : "";
        const UniValue& pubKeys = data.exists("pubkeys") ? data["pubkeys"].get_array() : UniValue();
        const UniValue& keys = data.exists("keys") ? data["keys"].get_array() : UniValue();
        const bool internal = data.exists("internal") ? data["internal"].get_bool() : false;
        const bool watchOnly = data.exists("watchonly") ? data["watchonly"].get_bool() : false;
        const std::string& label = data.exists("label") ? data["label"].get_str() : "";

//为提供的scriptPubKey生成脚本和目标
        CScript script;
        CTxDestination dest;
        if (!isScript) {
            dest = DecodeDestination(output);
            if (!IsValidDestination(dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address \"" + output + "\"");
            }
            script = GetScriptForDestination(dest);
        } else {
            if (!IsHex(output)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid scriptPubKey \"" + output + "\"");
            }
            std::vector<unsigned char> vData(ParseHex(output));
            script = CScript(vData.begin(), vData.end());
            if (!ExtractDestination(script, dest) && !internal) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Internal must be set to true for nonstandard scriptPubKey imports.");
            }
        }

//分析所有参数
        ImportData import_data;
        if (strRedeemScript.size()) {
            if (!IsHex(strRedeemScript)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid redeem script \"" + strRedeemScript + "\": must be hex string");
            }
            auto parsed_redeemscript = ParseHex(strRedeemScript);
            import_data.redeemscript = MakeUnique<CScript>(parsed_redeemscript.begin(), parsed_redeemscript.end());
        }
        if (witness_script_hex.size()) {
            if (!IsHex(witness_script_hex)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid witness script \"" + witness_script_hex + "\": must be hex string");
            }
            auto parsed_witnessscript = ParseHex(witness_script_hex);
            import_data.witnessscript = MakeUnique<CScript>(parsed_witnessscript.begin(), parsed_witnessscript.end());
        }
        std::map<CKeyID, CPubKey> pubkey_map;
        for (size_t i = 0; i < pubKeys.size(); ++i) {
            const auto& str = pubKeys[i].get_str();
            if (!IsHex(str)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Pubkey \"" + str + "\" must be a hex string");
            }
            auto parsed_pubkey = ParseHex(str);
            CPubKey pubkey(parsed_pubkey.begin(), parsed_pubkey.end());
            if (!pubkey.IsFullyValid()) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Pubkey \"" + str + "\" is not a valid public key");
            }
            pubkey_map.emplace(pubkey.GetID(), pubkey);
        }
        std::map<CKeyID, CKey> privkey_map;
        for (size_t i = 0; i < keys.size(); ++i) {
            const auto& str = keys[i].get_str();
            CKey key = DecodeSecret(str);
            if (!key.IsValid()) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key encoding");
            }
            CPubKey pubkey = key.GetPubKey();
            CKeyID id = pubkey.GetID();
            if (pubkey_map.count(id)) {
                pubkey_map.erase(id);
            }
            privkey_map.emplace(id, key);
        }

//内部地址不应具有标签
        if (internal && data.exists("label")) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Internal addresses should not have a label");
        }

//验证和处理输入数据
        bool have_solving_data = import_data.redeemscript || import_data.witnessscript || pubkey_map.size() || privkey_map.size();
        if (have_solving_data) {
//将导入数据中的数据与脚本中的scriptPubKey匹配。
            auto error = RecurseImportData(script, import_data, ScriptContext::TOP);

//验证watchOnly选项是否与私钥的可用性相对应。
            bool spendable = std::all_of(import_data.used_keys.begin(), import_data.used_keys.end(), [&](const std::pair<CKeyID, bool>& used_key){ return privkey_map.count(used_key.first) > 0; });
            if (!watchOnly && !spendable) {
                warnings.push_back("Some private keys are missing, outputs will be considered watchonly. If this is intentional, specify the watchonly flag.");
            }
            if (watchOnly && spendable) {
                warnings.push_back("All private keys are provided, outputs will be considered spendable. If this is intentional, do not specify the watchonly flag.");
            }

//检查是否提供了解决方案所需的所有密钥。
            if (error.empty()) {
                for (const auto& require_key : import_data.used_keys) {
if (!require_key.second) continue; //不是必需的密钥
                    if (pubkey_map.count(require_key.first) == 0 && privkey_map.count(require_key.first) == 0) {
                        error = "some required keys are missing";
                    }
                }
            }

            if (!error.empty()) {
                warnings.push_back("Importing as non-solvable: " + error + ". If this is intentional, don't provide any keys, pubkeys, witnessscript, or redeemscript.");
                import_data = ImportData();
                pubkey_map.clear();
                privkey_map.clear();
                have_solving_data = false;
            } else {
//recurseimportdata（）从导入数据中删除任何相关的redeemscript/witnessscript，因此我们可以使用它来发现是否提供了多余的redeemscript/witnessscript。
                if (import_data.redeemscript) warnings.push_back("Ignoring redeemscript as this is not a P2SH script.");
                if (import_data.witnessscript) warnings.push_back("Ignoring witnessscript as this is not a (P2SH-)P2WSH script.");
                for (auto it = privkey_map.begin(); it != privkey_map.end(); ) {
                    auto oldit = it++;
                    if (import_data.used_keys.count(oldit->first) == 0) {
                        warnings.push_back("Ignoring irrelevant private key.");
                        privkey_map.erase(oldit);
                    }
                }
                for (auto it = pubkey_map.begin(); it != pubkey_map.end(); ) {
                    auto oldit = it++;
                    auto key_data_it = import_data.used_keys.find(oldit->first);
                    if (key_data_it == import_data.used_keys.end() || !key_data_it->second) {
                        warnings.push_back("Ignoring public key \"" + HexStr(oldit->first) + "\" as it doesn't appear inside P2PKH or P2WPKH.");
                        pubkey_map.erase(oldit);
                    }
                }
            }
        }

//检查我们有没有工作要做
        if (::IsMine(*pwallet, script) & ISMINE_SPENDABLE) {
            throw JSONRPCError(RPC_WALLET_ERROR, "The wallet already contains the private key for this address or script");
        }

//一切都好，该进口了
        pwallet->MarkDirty();
        for (const auto& entry : import_data.import_scripts) {
            if (!pwallet->HaveCScript(CScriptID(entry)) && !pwallet->AddCScript(entry)) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Error adding script to wallet");
            }
        }
        for (const auto& entry : privkey_map) {
            const CKey& key = entry.second;
            CPubKey pubkey = key.GetPubKey();
            const CKeyID& id = entry.first;
            assert(key.VerifyPubKey(pubkey));
            pwallet->mapKeyMetadata[id].nCreateTime = timestamp;
//如果钱包中没有私钥，请将其插入。
            if (!pwallet->HaveKey(id) && !pwallet->AddKeyPubKey(key, pubkey)) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Error adding key to wallet");
            }
            pwallet->UpdateTimeFirstKey(timestamp);
        }
        for (const auto& entry : pubkey_map) {
            const CPubKey& pubkey = entry.second;
            const CKeyID& id = entry.first;
            CPubKey temp;
            if (!pwallet->GetPubKey(id, temp) && !pwallet->AddWatchOnly(GetScriptForRawPubKey(pubkey), timestamp)) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Error adding address to wallet");
            }
        }
if (!have_solving_data || !::IsMine(*pwallet, script)) { //始终只对不可解表调用addwatchOnly，以便更新表时间戳
            if (!pwallet->AddWatchOnly(script, timestamp)) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Error adding address to wallet");
            }
        }
        if (!internal) {
            assert(IsValidDestination(dest));
            pwallet->SetAddressBook(dest, label, "receive");
        }

        result.pushKV("success", UniValue(true));
    } catch (const UniValue& e) {
        result.pushKV("success", UniValue(false));
        result.pushKV("error", e);
    } catch (...) {
        result.pushKV("success", UniValue(false));

        result.pushKV("error", JSONRPCError(RPC_MISC_ERROR, "Missing required fields"));
    }
    if (warnings.size()) result.pushKV("warnings", warnings);
    return result;
}

static int64_t GetImportTimestamp(const UniValue& data, int64_t now)
{
    if (data.exists("timestamp")) {
        const UniValue& timestamp = data["timestamp"];
        if (timestamp.isNum()) {
            return timestamp.get_int64();
        } else if (timestamp.isStr() && timestamp.get_str() == "now") {
            return now;
        }
        throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Expected number or \"now\" timestamp value for key. got type %s", uvTypeName(timestamp.type())));
    }
    throw JSONRPCError(RPC_TYPE_ERROR, "Missing required timestamp field for key");
}

UniValue importmulti(const JSONRPCRequest& mainRequest)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(mainRequest);
    CWallet* const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, mainRequest.fHelp)) {
        return NullUniValue;
    }

    if (mainRequest.fHelp || mainRequest.params.size() < 1 || mainRequest.params.size() > 2)
        throw std::runtime_error(
            RPCHelpMan{"importmulti",
                "\nImport addresses/scripts (with private or public keys, redeem script (P2SH)), optionally rescanning the blockchain from the earliest creation time of the imported scripts. Requires a new wallet backup.\n"
                "If an address/script is imported without all of the private keys required to spend from that address, it will be watchonly. The 'watchonly' option must be set to true in this case or a warning will be returned.\n"
                "Conversely, if all the private keys are provided and the address/script is spendable, the watchonly option must be set to false, or a warning will be returned.\n",
                {
                    /*equests“，rpcarg:：type:：arr，/*opt*/false，/*default_val*/”“，“要导入的数据”，
                        {
                            “”，rpcarg:：type:：obj，/*opt*/ false, /* default_val */ "", "",

                                {
                                    /*scriptpubkey“，rpcarg:：type:：str，/*opt*/false，/*默认值“/”，“scriptpubkey的类型（脚本的字符串，地址的json）”，
                                        /*单线描述*/ "", {"\"<script>\" | { \"address\":\"<address>\" }", "string / json"}

                                    },
                                    /*IMESTAMP“，rpcarg:：type:：num，/*opt*/false，/*default_val*/”“，“密钥的创建时间（自epoch以来的秒数）（1970年1月1日，格林威治标准时间），\n”
        或字符串“now”，以替换当前同步的区块链时间。最旧的时间戳\n“
        “密钥将决定丢失钱包交易需要多久才能开始区块链重新扫描。\n”
        可以指定\“现在\”绕过扫描，以查找已知从未使用过的密钥，以及\n
        “可以指定0扫描整个区块链。在最早的键之前最多2小时阻塞\n“
        “将扫描由importmulti调用导入的所有密钥的创建时间。”，
                                        /*单线描述*/ "", {"timestamp | \"now\"", "integer / string"}

                                    },
                                    /*edeemscript“，rpcarg:：type:：str，/*opt*/true，/*default_val*/”省略“，”仅当scriptpubkey是p2sh或p2sh-p2wsh地址/scriptpubkey”，才允许使用，
                                    “见证脚本”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "omitted", "Allowed only if the scriptPubKey is a P2SH-P2WSH or P2WSH address/scriptPubKey"},

                                    /*ubkeys“，rpcarg:：type:：arr，/*opt*/true，/*default_val*/”空数组“，”将pubkeys导入的字符串数组。它们必须出现在p2pkh或p2wpkh脚本中。如果还提供了私钥，则不需要它们（请参阅\“keys\”参数）。”，
                                        {
                                            “pubkey”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", ""},

                                        }
                                    },
                                    /*eys“，rpcarg:：type:：arr，/*opt*/true，/*default_val*/”空数组“，”提供要导入私钥的字符串数组。相应的公钥必须出现在输出或redeemscript中。“，
                                        {
                                            “key”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", ""},

                                        }
                                    },
                                    /*internal“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”false“，”说明是否应将匹配的输出视为非传入付款（也称为更改）”，
                                    “watchonly”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "false", "Stating whether matching outputs should be considered watchonly."},

                                    /*abel“，rpcarg:：type:：str，/*opt*/true，/*default_val*/”“”“”“，要分配给地址的标签，只允许使用internal=false”，
                                }
                            }
                        }
                        “\”请求\“”，
                    “选项”，rpcarg:：type:：obj，/*opt*/ true, /* default_val */ "null", "",

                        {
                            /*escan“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”true“，”说明在所有导入之后是否应重新扫描区块链“，
                        }
                        “选项”“}，
                }
                toSTRIN（）+
            注意：如果重新扫描为真，则此调用可能需要一个多小时才能完成。在此期间，其他RPC调用\n
            可能报告导入的密钥、地址或脚本存在，但相关事务仍然丢失。\n
            “\n示例：\n”+
            helpExamplecli（“importmulti”，“[\”scriptpubkey\“：\”address\“：\”<my address>\“，”\“timestamp\”：1455191478，”
                                          “\”scriptpubkey\“：\”address\“：\”<my 2nd address>\“，”label\“：\”example 2\“，\”timestamp\“：1455191480]'“）+
            helpExamplecli（“importmulti”，“[\”scriptpubkey\“：\”address\“：\”<my address>\“”，“timestamp\”：1455191478]“”“\”rescan\“：false'”）+

            \n响应是一个与具有执行结果的输入大小相同的数组：\n
            “[\”success\“：真，\”success\“：真，\”warnings\“：[\”忽略不相关的私钥\“]，\”success\“：假，\”error\“：\”code\“：-1，\”message\“：\”internal server error\“，…]”


    rpctypecheck（mainrequest.params，univalue:：varr，univalue:：vobj）；

    const univalue&requests=mainrequest.params[0]；

    //默认选项
    布尔·弗雷斯坎=真；

    如果（！）mainRequest.params[1].isNull（））
        const univalue&options=mainrequest.params[1]；

        if（options.exists（“rescan”））
            frescan=options[“rescan”]。
        }
    }

    墙式储罐（PWallet）；
    如果（壁画&&！reserver.reserve（））
        throw jsonrpcerror（rpc_wallet_error），wallet当前正在重新扫描。中止现有的重新扫描或等待。“）；
    }

    Int64_t现在=0；
    bool frunscan=假；
    Int64_t nlowestTimestamp=0；
    单值响应（单值：：varr）；
    {
        自动锁定的_chain=pwallet->chain（）.lock（）；
        锁（pwallet->cs_wallet）；
        确保已解锁（PWallet）；

        //在导入任何键之前验证是否存在所有时间戳。
        现在=chainActive.tip（）？chainActive.tip（）->getMediantimecast（）：0；
        for（const univalue&data:requests.getvalues（））
            getimportTimestamp（数据，现在）；
        }

        const int64最小时间戳=1；

        if（frescan和chainactive.tip（））
            nlowestTimestamp=chainActive.tip（）->getBlockTime（）；
        }否则{
            Frescan=假；
        }

        for（const univalue&data:requests.getvalues（））
            const int64_t timestamp=std:：max（getimporttimestamp（data，now），minimumTimestamp）；
            const univalue result=processimport（pwallet、data、timestamp）；
            响应。推回（结果）；

            如果（！）弗雷斯坎）{
                继续；
            }

            //如果至少有一个请求成功，则允许重新扫描。
            if（result[“success”].get_bool（））
                frunscan=真；
            }

            //获取最低的时间戳。
            if（timestamp<nlowesttimestamp）
                nlowestTimestamp=时间戳；
            }
        }
    }
    if（frescan&&frunscan&&requests.size（））
        Int64_t scannedtime=pWallet->RescanFromTime（nLowestTimestamp，Reserver，true/*更新*/);

        pwallet->ReacceptWalletTransactions();

        if (pwallet->IsAbortingRescan()) {
            throw JSONRPCError(RPC_MISC_ERROR, "Rescan aborted by user.");
        }
        if (scannedTime > nLowestTimestamp) {
            std::vector<UniValue> results = response.getValues();
            response.clear();
            response.setArray();
            size_t i = 0;
            for (const UniValue& request : requests.getValues()) {
//如果密钥创建日期在成功扫描的范围内
//范围，或者如果导入结果已经设置了错误，则让
//结果保持不变。否则替换结果
//带有错误消息。
                if (scannedTime <= GetImportTimestamp(request, now) || results.at(i).exists("error")) {
                    response.push_back(results.at(i));
                } else {
                    UniValue result = UniValue(UniValue::VOBJ);
                    result.pushKV("success", UniValue(false));
                    result.pushKV(
                        "error",
                        JSONRPCError(
                            RPC_MISC_ERROR,
                            strprintf("Rescan failed for key with creation timestamp %d. There was an error reading a "
                                      "block from time %d, which is after or within %d seconds of key creation, and "
                                      "could contain transactions pertaining to the key. As a result, transactions "
                                      "and coins using this key may not appear in the wallet. This error could be "
                                      "caused by pruning or data corruption (see bitcoind log for details) and could "
                                      "be dealt with by downloading and rescanning the relevant blocks (see -reindex "
                                      "and -rescan options).",
                                GetImportTimestamp(request, now), scannedTime - TIMESTAMP_WINDOW - 1, TIMESTAMP_WINDOW)));
                    response.push_back(std::move(result));
                }
                ++i;
            }
        }
    }

    return response;
}
