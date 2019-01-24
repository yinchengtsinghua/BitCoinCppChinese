
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

#include <chain.h>
#include <coins.h>
#include <compat/byteswap.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <index/txindex.h>
#include <init.h>
#include <key_io.h>
#include <keystore.h>
#include <merkleblock.h>
#include <net.h>
#include <policy/policy.h>
#include <policy/rbf.h>
#include <primitives/transaction.h>
#include <rpc/rawtransaction.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/sign.h>
#include <script/standard.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <validation.h>
#include <validationinterface.h>

#include <future>
#include <stdint.h>

#include <univalue.h>


static void TxToJSON(const CTransaction& tx, const uint256 hashBlock, UniValue& entry)
{
//调用bitcoin common中的txtouniv（）来解码事务十六进制。
//
//区块链上下文信息（确认和阻止时间）不是
//可以用比特币编码，所以我们在这里查询它们并推送
//返回的单值中的数据。
    TxToUniv(tx, uint256(), entry, true, RPCSerializationFlags());

    if (!hashBlock.IsNull()) {
        LOCK(cs_main);

        entry.pushKV("blockhash", hashBlock.GetHex());
        CBlockIndex* pindex = LookupBlockIndex(hashBlock);
        if (pindex) {
            if (chainActive.Contains(pindex)) {
                entry.pushKV("confirmations", 1 + chainActive.Height() - pindex->nHeight);
                entry.pushKV("time", pindex->GetBlockTime());
                entry.pushKV("blocktime", pindex->GetBlockTime());
            }
            else
                entry.pushKV("confirmations", 0);
        }
    }
}

static UniValue getrawtransaction(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw std::runtime_error(
            RPCHelpMan{"getrawtransaction",
                "\nNOTE: By default this function only works for mempool transactions. If the -txindex option is\n"
                "enabled, it also works for blockchain transactions. If the block which contains the transaction\n"
                "is known, its hash can be provided even for nodes without -txindex. Note that if a blockhash is\n"
                "provided, only that block will be searched and if the transaction is in the mempool or other\n"
                "blocks, or if this node does not have the given block available, the transaction will not be found.\n"
            "DEPRECATED: for now, it also works for transactions with unspent outputs.\n"

            "\nReturn the raw transaction data.\n"
            "\nIf verbose is 'true', returns an Object with information about 'txid'.\n"
            "If verbose is 'false' or omitted, returns a string that is serialized, hex-encoded data for 'txid'.\n"
                ,
                {
                    /*xid“，rpcarg:：type:：str_hex，/*opt*/false，/*默认值_val*/”“，“事务ID”，
                    “verbose”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "false", "If false, return a string, otherwise return a json object"},

                    /*lockhash“，rpcarg:：type:：str_hex，/*opt*/true，/*default_val*/”null“，”要在其中查找事务的块“，
                }
                toSTRIN（）+
            \n结果（如果verbose未设置或设置为false）：\n
            “data”（字符串）“txid”的序列化十六进制编码数据\n”

            \n结果（如果verbose设置为true）：\n
            “{n”
            “在活动链中\”：b，（bool）指定的块是否在活动链中（仅与显式\”blockhash“参数一起出现）\n”
            ““hex\”：“data\”，（string）“txid”的序列化十六进制编码数据\n”
            “txid\”：\“id\”，（string）事务ID（与提供的相同）\n”
            “\”哈希\“：\”ID\“，（字符串）事务哈希（与见证事务的txid不同）\n”
            ““大小”：n，（数字）序列化事务大小\n”
            “vsize”：n，（数字）虚拟事务大小（与见证事务的大小不同）\n”
            “weight”：n，（数字）事务的权重（在vsize*4-3和vsize*4之间）\n”
            “版本\”：n，（数字）版本\n”
            ““锁定时间”：ttt，（数字）锁定时间\n”
            ““VIN”：[（JSON对象数组）\n”
            “{n”
            ““txid\”：\“id\”，（string）事务ID \n”
            ““vout”：n，（数字\n”
            “scriptsig”：（json对象）脚本\n”
            “\”asm\“：\”asm\“，（string）asm\n”
            “十六进制”：“十六进制”（字符串）十六进制\n”
            “}，\n”
            ““序列号”：n（数字）脚本序列号\n”
            “txinwitness\”：[\”hex\“，…]（字符串数组）十六进制编码的见证数据（如果有）\n”
            “}\n”
            “……\n”
            “”
            “\”vout\“：[（JSON对象数组\n”
            “{n”
            “值\”：x.x x x，（数字）以“+货币\单位+”表示的值\n”
            “n”：n，（数字）索引\n”
            “scriptpubkey\”：（json对象）\n”
            “\”asm\“：\”asm\“，（string）asm \n”
            “十六进制”：“十六进制”，（string）十六进制\n”
            “reqsigs”：n，（数字）所需的sigs \n”
            “type\”：“pubkeyhash\”，（string）类型，例如'pubkeyhash'\n”
            “\”地址\“：[（字符串的json数组\n”
            “地址\”（string）比特币地址\n”
            “，…\n”
            “\n”
            “}\n”
            “}\n”
            “……\n”
            “”
            “\”block hash\“：\”hash\“，（string）块哈希\n”
            “确认\”：n，（数字）确认\n”
            ““block time\”：ttt（数字）自epoch（1970年1月1日，格林威治标准时间）以来的阻止时间（秒）。\n”
            “时间”：ttt，（数字）与\“blocktime \”相同\n”
            “}\n”

            “\n实例：\n”
            +helpExamplecli（“getrawtransaction”，“mytxid\”）
            +helpExamplecli（“getrawtransaction”，“\”mytxid\“true”）。
            +helpExampleRpc（“GetRawTransaction”，“MyTxID”，“真”）。
            +helpExamplecli（“getrawtransaction”，“mytxid\”false\“myblockhash\”）
            +helpExamplecli（“getrawtransaction”，“mytxid\”true\“myblockhash\”）
        ；

    bool in_active_chain=真；
    uint256 hash=parsehashv（request.params[0]，“参数1”）；
    cblockindex*blockindex=nullptr；

    if（hash==params（）.genesBlock（）.hashmerkleroot）
        //Genesis块CoinBase事务的特殊异常
        throw jsonrpcerror（rpc_invalid_address_or_key，“Genesis块coinbase不被视为普通事务，无法检索”）；
    }

    //接受bool（true）或num（>=1）以指示详细输出。
    bool fverbose=假；
    如果（！）request.params[1].isNull（））
        fverbose=request.params[1].isnum（）？（request.params[1].获取int（）！=0）：请求.params[1].获取bool（）；
    }

    如果（！）request.params[2].isNull（））
        锁（CSKEMAN）；

        uint256 blockhash=parsehashv（request.params[2]，“参数3”）；
        blockIndex=查找blockIndex（blockHash）；
        如果（！）阻滞剂）
            throw jsonrpcerror（rpc_invalid_address_or_key，“找不到块哈希”）；
        }
        in_active_chain=chain active.contains（块索引）；
    }

    bool f_txindex_ready=false；
    如果（g_TxIndex&&！阻滞剂）
        f_TxIndex_ready=g_TxIndex->blockUntilsynchedTocurrentchain（）；
    }

    ctransactionref发送；
    uint256 hash_块；
    如果（！）getTransaction（hash，tx，params（）.getconsensus（），hash_block，true，blockindex））
        std：：字符串errmsg；
        如果（blockindex）
            如果（！）（blockindex->nstatus&block_have_data））
                throw jsonrpcerror（rpc_misc_error，“block not available”）；
            }
            errmsg=“在提供的块中找不到此类事务”；
        否则，（如果）！GX-TXEXT）{
            errmsg=“无此类mempool事务。使用-txindex启用区块链交易查询”；
        否则，（如果）！准备就绪）
            errmsg=“无此类mempool事务。区块链交易仍在被索引的过程中”；
        }否则{
            errmsg=“无此类mempool或区块链交易”；
        }
        抛出jsonrpcerror（rpc_invalid_address_or_key，errmsg+）。对钱包交易使用getTransaction。“）；
    }

    如果（！）FvbOSE）{
        返回encodeHextx（*tx，rpcserializationFlags（））；
    }

    单值结果（单值：：vobj）；
    if（blockindex）result.pushkv（“in_active_chain”，“in_active_chain”）；
    txtojson（*tx，hash_块，结果）；
    返回结果；
}

静态单值gettXoutProof（const-jsonrpcrequest&request）
{
    如果（request.fhelp（request.params.size（）！=1&&request.params.size（）！= 2）
        throw std:：runtime_错误（
            rpchelpman“gettXoutProof”，
                \n返回一个十六进制编码的证明\“txid \”包含在块中。\n
                注意：默认情况下，此功能有时只起作用。这是当有一个\n“
                “此事务的utxo中未占用的输出。为了使它始终有效，请\n“
                您需要使用-txindex命令行选项或\n维护事务索引
                “指定手动包含事务的块（通过blockhash）。\n”，
                {
                    “txids”，rpcarg:：type:：arr，/*opt*/ false, /* default_val */ "", "A json array of txids to filter",

                        {
                            /*xid“，rpcarg:：type:：str_hex，/*opt*/false，/*默认值_val*/”“，“事务哈希”，
                        }
                        }
                    “blockhash”，rpcarg:：type:：str_hex，/*opt*/ true, /* default_val */ "null", "If specified, looks for txid in the block with this hash"},

                }}
                .ToString() +
            "\nResult:\n"
            "\"data\"           (string) A string that is a serialized, hex-encoded data for the proof.\n"
        );

    std::set<uint256> setTxids;
    uint256 oneTxid;
    UniValue txids = request.params[0].get_array();
    for (unsigned int idx = 0; idx < txids.size(); idx++) {
        const UniValue& txid = txids[idx];
        uint256 hash(ParseHashV(txid, "txid"));
        if (setTxids.count(hash))
            throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated txid: ")+txid.get_str());
       setTxids.insert(hash);
       oneTxid = hash;
    }

    CBlockIndex* pblockindex = nullptr;
    uint256 hashBlock;
    if (!request.params[1].isNull()) {
        LOCK(cs_main);
        hashBlock = ParseHashV(request.params[1], "blockhash");
        pblockindex = LookupBlockIndex(hashBlock);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
    } else {
        LOCK(cs_main);

//循环浏览txid并尝试找到它们所在的块。找到块后退出循环。
        for (const auto& tx : setTxids) {
            const Coin& coin = AccessByTxid(*pcoinsTip, tx);
            if (!coin.IsSpent()) {
                pblockindex = chainActive[coin.nHeight];
                break;
            }
        }
    }


//如果我们需要查询，并且在获取cs_main之前，允许txindex进行跟踪。
    if (g_txindex && !pblockindex) {
        g_txindex->BlockUntilSyncedToCurrentChain();
    }

    LOCK(cs_main);

    if (pblockindex == nullptr)
    {
        CTransactionRef tx;
        if (!GetTransaction(oneTxid, tx, Params().GetConsensus(), hashBlock, false) || hashBlock.IsNull())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction not yet in block");
        pblockindex = LookupBlockIndex(hashBlock);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Transaction index corrupt");
        }
    }

    CBlock block;
    if(!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus()))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't read block from disk");

    unsigned int ntxFound = 0;
    for (const auto& tx : block.vtx)
        if (setTxids.count(tx->GetHash()))
            ntxFound++;
    if (ntxFound != setTxids.size())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Not all transactions found in specified or retrieved block");

    CDataStream ssMB(SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS);
    CMerkleBlock mb(block, setTxids);
    ssMB << mb;
    std::string strHex = HexStr(ssMB.begin(), ssMB.end());
    return strHex;
}

static UniValue verifytxoutproof(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"verifytxoutproof",
                "\nVerifies that a proof points to a transaction in a block, returning the transaction it commits to\n"
                "and throwing an RPC error if the block is not in our best chain\n",
                {
                    /*roof“，rpcarg:：type:：str_hex，/*opt*/false，/*default_val*/”，“gettxoutproof生成的十六进制编码证明”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “[\”txid\“]（数组、字符串）证明提交的txid，或者如果证明无法验证，则为空数组。\n”
        ；

    cdatastream ssmb（parsehexv（request.params[0]，“proof”），ser_network，protocol_version_serialize_transaction_no_witness）；
    梅克布洛克；
    SSMB>>梅克布洛克；

    单值资源（单值：：varr）；

    std:：vector<uint256>vmatch；
    std:：vector<unsigned int>vindex；
    如果（merkleblock.txn.extract匹配（vmatch，vindex）！=merkleblock.header.hashmerkleroot）
        收益率；

    锁（CSKEMAN）；

    const cblockindex*pindex=lookupblockindex（merkleblock.header.gethash（））；
    如果（！）pQueY*！chainActive.contains（pindex）pindex->ntx==0）
        throw jsonrpcerror（rpc_invalid_address_or_key，“在链中找不到块”）；
    }

    //检查证明是否有效，只有这样才添加结果
    if（pindex->ntx==merkleblock.txn.getNumTransactions（））
        for（const uint256&hash:vmatch）
            res.push_back（hash.gethex（））；
        }
    }

    收益率；
}

CMutableTransaction构造事务（const univalue&inputs-in、const univalue&outputs-in、const univalue&locktime、const univalue&rbf）
{
    if（inputs_in.isNull（）outputs_in.isNull（））
        throw jsonrpcerror（rpc_invalid_参数，“无效参数，参数1和2必须非空”）；

    单值输入=inputs_in.get_array（）；
    const bool outputs_is_obj=输出_in.isObject（）；
    单值输出=输出\是对象？outputs_in.get_obj（）：输出_in.get_array（）；

    可修改事务rawtx；

    如果（！）locktime.isNull（））
        int64_t nlocktime=locktime.get_int64（）；
        if（nlocktime<0 nlocktime>locktime_max）
            throw jsonrpcerror（rpc_invalid_参数，“无效参数，锁定时间超出范围”）；
        rawtx.nlocktime=nlocktime；
    }

    bool rbfoptin=rbf.istree（）；

    for（unsigned int idx=0；idx<inputs.size（）；idx++）
        const univalue&input=输入[idx]；
        const univalue&o=input.get_obj（）；

        uint256 txid=pareshasho（o，“txid”）；

        const univalue&vout_v=查找_value（o，“vout”）；
        如果（！）Vutyv.ISNUME（）
            throw jsonrpcerror（rpc_invalid_参数，“无效参数，缺少vout键”）；
        int noutput=vout_v.get_int（）；
        如果（输出<0）
            throw jsonrpcerror（rpc_invalid_参数，“invalid参数，vout必须为正数”）；

        uint32序列；
        如果（RBFoppin）{
            nsequence=max_bip125_rbf_sequence；/*ctxin：：sequence_final-2*/

        } else if (rawTx.nLockTime) {
            nSequence = CTxIn::SEQUENCE_FINAL - 1;
        } else {
            nSequence = CTxIn::SEQUENCE_FINAL;
        }

//如果传入参数对象，则设置序列号
        const UniValue& sequenceObj = find_value(o, "sequence");
        if (sequenceObj.isNum()) {
            int64_t seqNr64 = sequenceObj.get_int64();
            if (seqNr64 < 0 || seqNr64 > CTxIn::SEQUENCE_FINAL) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, sequence number is out of range");
            } else {
                nSequence = (uint32_t)seqNr64;
            }
        }

        CTxIn in(COutPoint(txid, nOutput), CScript(), nSequence);

        rawTx.vin.push_back(in);
    }

    if (!outputs_is_obj) {
//将键值对数组转换为dict
        UniValue outputs_dict = UniValue(UniValue::VOBJ);
        for (size_t i = 0; i < outputs.size(); ++i) {
            const UniValue& output = outputs[i];
            if (!output.isObject()) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, key-value pair not an object as expected");
            }
            if (output.size() != 1) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, key-value pair must contain exactly one key");
            }
            outputs_dict.pushKVs(output);
        }
        outputs = std::move(outputs_dict);
    }

//重复检查
    std::set<CTxDestination> destinations;
    bool has_data{false};

    for (const std::string& name_ : outputs.getKeys()) {
        if (name_ == "data") {
            if (has_data) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, duplicate key: data");
            }
            has_data = true;
            std::vector<unsigned char> data = ParseHexV(outputs[name_].getValStr(), "Data");

            CTxOut out(0, CScript() << OP_RETURN << data);
            rawTx.vout.push_back(out);
        } else {
            CTxDestination destination = DecodeDestination(name_);
            if (!IsValidDestination(destination)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: ") + name_);
            }

            if (!destinations.insert(destination).second) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ") + name_);
            }

            CScript scriptPubKey = GetScriptForDestination(destination);
            CAmount nAmount = AmountFromValue(outputs[name_]);

            CTxOut out(nAmount, scriptPubKey);
            rawTx.vout.push_back(out);
        }
    }

    if (!rbf.isNull() && rawTx.vin.size() > 0 && rbfOptIn != SignalsOptInRBF(rawTx)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter combination: Sequence number(s) contradict replaceable option");
    }

    return rawTx;
}

static UniValue createrawtransaction(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 4) {
        throw std::runtime_error(
            RPCHelpMan{"createrawtransaction",
                "\nCreate a transaction spending the given inputs and creating new outputs.\n"
                "Outputs can be addresses or data.\n"
                "Returns hex-encoded raw transaction.\n"
                "Note that the transaction's inputs are not signed, and\n"
                "it is not stored in the wallet or transmitted to the network.\n",
                {
                    /*nputs“，rpcarg:：type:：arr，/*opt*/false，/*default_val*/”“，“JSON对象的JSON数组”，
                        {
                            “”，rpcarg:：type:：obj，/*opt*/ true, /* default_val */ "", "",

                                {
                                    /*xid“，rpcarg:：type:：str_hex，/*opt*/false，/*默认值_val*/”“，“事务ID”，
                                    “vout”，rpcarg:：type:：num，/*opt*/ false, /* default_val */ "", "The output number"},

                                    /*equence“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”取决于'replacement'和'locktime'参数的值，“序列号”，
                                }
                                }
                        }
                        }
                    “输出”，rpcarg:：type:：arr，/*opt*/ false, /* default_val */ "", "a json array with outputs (key-value pairs), where none of the keys are duplicated.\n"

                            "That is, each address can only appear once and there can only be one 'data' object.\n"
                            "For compatibility reasons, a dictionary, which holds the key-value pairs directly, is also\n"
                            "                             accepted as second parameter.",
                        {
                            /*，rpcarg：：type：：obj，/*opt*/true，/*默认值“/”，“”，
                                {
                                    “地址”，rpcarg:：type:：amount，/*opt*/ false, /* default_val */ "", "A key-value pair. The key (string) is the bitcoin address, the value (float or string) is the amount in " + CURRENCY_UNIT},

                                },
                                },
                            /*，rpcarg：：type：：obj，/*opt*/true，/*默认值“/”，“”，
                                {
                                    “数据”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "A key-value pair. The key must be \"data\", the value is hex-encoded data"},

                                },
                                },
                        },
                        },
                    /*ocktime“，rpcarg：：type:：num，/*opt*/true，/*默认值_val*/”0“，”原始锁定时间。非0值也会激活锁定时间输入“，
                    “可替换”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "false", "Marks this transaction as BIP125-replaceable.\n"

            "                             Allows this transaction to be replaced by a transaction with higher fees. If provided, it is an error if explicit sequence numbers are incompatible."},
                }}
                .ToString() +
            "\nResult:\n"
            "\"transaction\"              (string) hex string of the transaction\n"

            "\nExamples:\n"
            + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"[{\\\"address\\\":0.01}]\"")
            + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"[{\\\"data\\\":\\\"00010203\\\"}]\"")
            + HelpExampleRpc("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"[{\\\"address\\\":0.01}]\"")
            + HelpExampleRpc("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"[{\\\"data\\\":\\\"00010203\\\"}]\"")
        );
    }

    RPCTypeCheck(request.params, {
        UniValue::VARR,
UniValueType(), //ARR或OBJ，稍后检查
        UniValue::VNUM,
        UniValue::VBOOL
        }, true
    );

    CMutableTransaction rawTx = ConstructTransaction(request.params[0], request.params[1], request.params[2], request.params[3]);

    return EncodeHexTx(rawTx);
}

static UniValue decoderawtransaction(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            RPCHelpMan{"decoderawtransaction",
                "\nReturn a JSON object representing the serialized, hex-encoded transaction.\n",
                {
                    /*exstring“，rpcarg:：type:：str_hex，/*opt*/false，/*默认值_val*/”“，“事务十六进制字符串”，
                    “iswitness”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "depends on heuristic tests", "Whether the transaction hex is a serialized witness transaction\n"

            "                         If iswitness is not present, heuristic tests will be used in decoding"},
                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"txid\" : \"id\",        (string) The transaction id\n"
            "  \"hash\" : \"id\",        (string) The transaction hash (differs from txid for witness transactions)\n"
            "  \"size\" : n,             (numeric) The transaction size\n"
            "  \"vsize\" : n,            (numeric) The virtual transaction size (differs from size for witness transactions)\n"
            "  \"weight\" : n,           (numeric) The transaction's weight (between vsize*4 - 3 and vsize*4)\n"
            "  \"version\" : n,          (numeric) The version\n"
            "  \"locktime\" : ttt,       (numeric) The lock time\n"
            "  \"vin\" : [               (array of json objects)\n"
            "     {\n"
            "       \"txid\": \"id\",    (string) The transaction id\n"
            "       \"vout\": n,         (numeric) The output number\n"
            "       \"scriptSig\": {     (json object) The script\n"
            "         \"asm\": \"asm\",  (string) asm\n"
            "         \"hex\": \"hex\"   (string) hex\n"
            "       },\n"
            "       \"txinwitness\": [\"hex\", ...] (array of string) hex-encoded witness data (if any)\n"
            "       \"sequence\": n     (numeric) The script sequence number\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"vout\" : [             (array of json objects)\n"
            "     {\n"
            "       \"value\" : x.xxx,            (numeric) The value in " + CURRENCY_UNIT + "\n"
            "       \"n\" : n,                    (numeric) index\n"
            "       \"scriptPubKey\" : {          (json object)\n"
            "         \"asm\" : \"asm\",          (string) the asm\n"
            "         \"hex\" : \"hex\",          (string) the hex\n"
            "         \"reqSigs\" : n,            (numeric) The required sigs\n"
            "         \"type\" : \"pubkeyhash\",  (string) The type, eg 'pubkeyhash'\n"
            "         \"addresses\" : [           (json array of string)\n"
            "           \"12tvKAXCxZjSmdNbao16dKXC8tRWfcF5oc\"   (string) bitcoin address\n"
            "           ,...\n"
            "         ]\n"
            "       }\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("decoderawtransaction", "\"hexstring\"")
            + HelpExampleRpc("decoderawtransaction", "\"hexstring\"")
        );

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VBOOL});

    CMutableTransaction mtx;

    bool try_witness = request.params[1].isNull() ? true : request.params[1].get_bool();
    bool try_no_witness = request.params[1].isNull() ? true : !request.params[1].get_bool();

    if (!DecodeHexTx(mtx, request.params[0].get_str(), try_no_witness, try_witness)) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    UniValue result(UniValue::VOBJ);
    TxToUniv(CTransaction(std::move(mtx)), uint256(), result, false);

    return result;
}

static UniValue decodescript(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"decodescript",
                "\nDecode a hex-encoded script.\n",
                {
                    /*exstring“，rpcarg:：type:：str_hex，/*opt*/false，/*默认值_val*/”“，“十六进制编码脚本”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            “\”asm\“：\”asm\“，（string）脚本公钥\n”
            “十六进制”：“十六进制”，（字符串）十六进制编码的公钥\n”
            ““type\”：“type\”，（string）输出类型\n”
            “reqsigs”：n，（数字）所需签名\n”
            “\”地址\“：[（字符串的json数组\n”
            “地址\”（string）比特币地址\n”
            “……\n”
            “”
            包装此兑换脚本的p2sh脚本的“p2sh”、“address”（string）地址（如果脚本已经是p2sh，则不返回）。\n
            “}\n”
            “\n实例：\n”
            +helpexamplecli（“解码脚本”，“\”hexstring\”）
            +helpexamplerpc（“解码脚本”，“\”hexstring\”）
        ；

    rpctypecheck（request.params，univalue:：vstr）；

    单值r（单值：：vobj）；
    脚本脚本；
    if（request.params[0].get_str（）.size（）>0）
        std:：vector<unsigned char>scriptData（parseHexv（request.params[0]，“argument”））；
        script=cscript（scriptData.begin（），scriptData.end（））；
    }否则{
        //空脚本有效
    }
    scriptpubkeytouniv（script，r，false）；

    单值类型；
    type=find_value（r，“type”）；

    如果（type.isstr（）&&type.get_str（）！=“脚本哈希”）；
        //p2sh不能包装在p2sh中。如果此脚本已经是p2sh，
        //不要返回p2sh的p2sh地址。
        r.pushkv（“p2sh”，编码目的地（cscriptid（script）））；
        //如果此脚本
        //是见证程序，不要返回segwit程序的地址。
        if（type.get_str（）=“pubkey”type.get_str（）=“pubkeyhash”type.get_str（）=“multisig”type.get_str（）=“nonstandard”）
            std:：vector<std:：vector<unsigned char>>solutions_data；
            txnoutype，其类型=解算器（脚本、解算器数据）；
            //未压缩的pubkey不能与segwit checksigs一起使用。
            //如果脚本包含未压缩的pubkey，则跳过segwit程序的编码。
            if（（which_type==tx_pubkey）（which_type==tx_multisig））
                for（const auto&solution:解决方案数据）
                    如果（（solution.size（）！= 1）& &！cpubkey（solution）.iscompressed（））
                        返回R；
                    }
                }
            }
            单值sr（单值：：vobj）；
            脚本Segwitscr；
            if（哪个_type==tx_pubkey）
                segwitscr=getscriptfdestination（witnessv0keyhash（hash160（solutions_data[0].begin（），solutions_data[0].end（））；
            else if（which_type==tx_pubkeyhash）
                segwitscr=getscriptfdestination（witnessv0keyhash（解决方案数据[0]）；
            }否则{
                //不适合p2wpkh的脚本被编码为p2wsh。
                //更新的segwit程序版本应在可用时考虑。
                segwitscr=getscriptfdestination（witnessv0scriptHash（script））；
            }
            脚本pubkeytouniv（segwitscr，sr，true）；
            sr.pushkv（“p2sh segwit”，编码目标（cscriptid（segwitscr））；
            r.pushkv（“segwit”，sr）；
        }
    }

    返回R；
}

/**将用于脚本验证或签名错误的JSON对象推送到verrorsret。*/

static void TxInErrorToJSON(const CTxIn& txin, UniValue& vErrorsRet, const std::string& strMessage)
{
    UniValue entry(UniValue::VOBJ);
    entry.pushKV("txid", txin.prevout.hash.ToString());
    entry.pushKV("vout", (uint64_t)txin.prevout.n);
    UniValue witness(UniValue::VARR);
    for (unsigned int i = 0; i < txin.scriptWitness.stack.size(); i++) {
        witness.push_back(HexStr(txin.scriptWitness.stack[i].begin(), txin.scriptWitness.stack[i].end()));
    }
    entry.pushKV("witness", witness);
    entry.pushKV("scriptSig", HexStr(txin.scriptSig.begin(), txin.scriptSig.end()));
    entry.pushKV("sequence", (uint64_t)txin.nSequence);
    entry.pushKV("error", strMessage);
    vErrorsRet.push_back(entry);
}

static UniValue combinerawtransaction(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"combinerawtransaction",
                "\nCombine multiple partially signed transactions into one transaction.\n"
                "The combined transaction may be another partially signed transaction or a \n"
                "fully signed transaction.",
                {
                    /*xs“，rpcarg:：type:：arr，/*opt*/false，/*default_val*/”“，“部分签名事务的十六进制字符串的JSON数组”，
                        {
                            “hexstring”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "A transaction hash"},

                        },
                        },
                }}
                .ToString() +
            "\nResult:\n"
            "\"hex\"            (string) The hex-encoded raw transaction with signature(s)\n"

            "\nExamples:\n"
            + HelpExampleCli("combinerawtransaction", "[\"myhex1\", \"myhex2\", \"myhex3\"]")
        );


    UniValue txs = request.params[0].get_array();
    std::vector<CMutableTransaction> txVariants(txs.size());

    for (unsigned int idx = 0; idx < txs.size(); idx++) {
        if (!DecodeHexTx(txVariants[idx], txs[idx].get_str(), true)) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, strprintf("TX decode failed for tx %d", idx));
        }
    }

    if (txVariants.empty()) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Missing transactions");
    }

//mergedtx将以所有签名结束；它
//开始作为rawtx的克隆：
    CMutableTransaction mergedTx(txVariants[0]);

//获取以前的事务（输入）：
    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    {
        LOCK(cs_main);
        LOCK(mempool.cs);
        CCoinsViewCache &viewChain = *pcoinsTip;
        CCoinsViewMemPool viewMempool(&viewChain, mempool);
view.SetBackend(viewMempool); //暂时将缓存后端切换到db+mempool视图

        for (const CTxIn& txin : mergedTx.vin) {
view.AccessCoin(txin.prevout); //将条目从视图链加载到视图中；可能会失败。
        }

view.SetBackend(viewDummy); //切换回以避免锁定内存池太久
    }

//将ctransaction用于
//避免重新刷新的事务。
    const CTransaction txConst(mergedTx);
//签署我们能签署的：
    for (unsigned int i = 0; i < mergedTx.vin.size(); i++) {
        CTxIn& txin = mergedTx.vin[i];
        const Coin& coin = view.AccessCoin(txin.prevout);
        if (coin.IsSpent()) {
            throw JSONRPCError(RPC_VERIFY_ERROR, "Input not found or already spent");
        }
        SignatureData sigdata;

//…合并其他签名：
        for (const CMutableTransaction& txv : txVariants) {
            if (txv.vin.size() > i) {
                sigdata.MergeSignatureData(DataFromTransaction(txv, i, coin.out));
            }
        }
        ProduceSignature(DUMMY_SIGNING_PROVIDER, MutableTransactionSignatureCreator(&mergedTx, i, coin.out.nValue, 1), coin.out.scriptPubKey, sigdata);

        UpdateInput(txin, sigdata);
    }

    return EncodeHexTx(mergedTx);
}

UniValue SignTransaction(interfaces::Chain& chain, CMutableTransaction& mtx, const UniValue& prevTxsUnival, CBasicKeyStore *keystore, bool is_temp_keystore, const UniValue& hashType)
{
//获取以前的事务（输入）：
    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    {
        LOCK2(cs_main, mempool.cs);
        CCoinsViewCache &viewChain = *pcoinsTip;
        CCoinsViewMemPool viewMempool(&viewChain, mempool);
view.SetBackend(viewMempool); //暂时将缓存后端切换到db+mempool视图

        for (const CTxIn& txin : mtx.vin) {
view.AccessCoin(txin.prevout); //将条目从视图链加载到视图中；可能会失败。
        }

view.SetBackend(viewDummy); //切换回以避免锁定内存池太久
    }

//添加在RPC调用中给定的先前txout:
    if (!prevTxsUnival.isNull()) {
        UniValue prevTxs = prevTxsUnival.get_array();
        for (unsigned int idx = 0; idx < prevTxs.size(); ++idx) {
            const UniValue& p = prevTxs[idx];
            if (!p.isObject()) {
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "expected object with {\"txid'\",\"vout\",\"scriptPubKey\"}");
            }

            UniValue prevOut = p.get_obj();

            RPCTypeCheckObj(prevOut,
                {
                    {"txid", UniValueType(UniValue::VSTR)},
                    {"vout", UniValueType(UniValue::VNUM)},
                    {"scriptPubKey", UniValueType(UniValue::VSTR)},
                });

            uint256 txid = ParseHashO(prevOut, "txid");

            int nOut = find_value(prevOut, "vout").get_int();
            if (nOut < 0) {
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "vout must be positive");
            }

            COutPoint out(txid, nOut);
            std::vector<unsigned char> pkData(ParseHexO(prevOut, "scriptPubKey"));
            CScript scriptPubKey(pkData.begin(), pkData.end());

            {
                const Coin& coin = view.AccessCoin(out);
                if (!coin.IsSpent() && coin.out.scriptPubKey != scriptPubKey) {
                    std::string err("Previous output scriptPubKey mismatch:\n");
                    err = err + ScriptToAsmStr(coin.out.scriptPubKey) + "\nvs:\n"+
                        ScriptToAsmStr(scriptPubKey);
                    throw JSONRPCError(RPC_DESERIALIZATION_ERROR, err);
                }
                Coin newcoin;
                newcoin.out.scriptPubKey = scriptPubKey;
                newcoin.out.nValue = MAX_MONEY;
                if (prevOut.exists("amount")) {
                    newcoin.out.nValue = AmountFromValue(find_value(prevOut, "amount"));
                }
                newcoin.nHeight = 1;
                view.AddCoin(out, std::move(newcoin), true);
            }

//如果提供了redeemscript和私钥，请将redeemscript添加到密钥库中，以便对其进行签名。
            if (is_temp_keystore && (scriptPubKey.IsPayToScriptHash() || scriptPubKey.IsPayToWitnessScriptHash())) {
                RPCTypeCheckObj(prevOut,
                    {
                        {"redeemScript", UniValueType(UniValue::VSTR)},
                    });
                UniValue v = find_value(prevOut, "redeemScript");
                if (!v.isNull()) {
                    std::vector<unsigned char> rsData(ParseHexV(v, "redeemScript"));
                    CScript redeemScript(rsData.begin(), rsData.end());
                    keystore->AddCScript(redeemScript);
//自动添加脚本的p2wsh封装版本（以处理p2sh-p2wsh）。
                    keystore->AddCScript(GetScriptForWitness(redeemScript));
                }
            }
        }
    }

    int nHashType = ParseSighashString(hashType);

    bool fHashSingle = ((nHashType & ~SIGHASH_ANYONECANPAY) == SIGHASH_SINGLE);

//脚本验证错误
    UniValue vErrors(UniValue::VARR);

//将ctransaction用于
//避免重新刷新的事务。
    const CTransaction txConst(mtx);
//签署我们能签署的：
    for (unsigned int i = 0; i < mtx.vin.size(); i++) {
        CTxIn& txin = mtx.vin[i];
        const Coin& coin = view.AccessCoin(txin.prevout);
        if (coin.IsSpent()) {
            TxInErrorToJSON(txin, vErrors, "Input not found or already spent");
            continue;
        }
        const CScript& prevPubKey = coin.out.scriptPubKey;
        const CAmount& amount = coin.out.nValue;

        SignatureData sigdata = DataFromTransaction(mtx, i, coin.out);
//只有在有相应输出的情况下，才能在SIGHASH_SINGLE上签名：
        if (!fHashSingle || (i < mtx.vout.size())) {
            ProduceSignature(*keystore, MutableTransactionSignatureCreator(&mtx, i, amount, nHashType), prevPubKey, sigdata);
        }

        UpdateInput(txin, sigdata);

//必须为有效的segwit签名指定金额
        if (amount == MAX_MONEY && !txin.scriptWitness.IsNull()) {
            throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Missing amount for %s", coin.out.ToString()));
        }

        ScriptError serror = SCRIPT_ERR_OK;
        if (!VerifyScript(txin.scriptSig, prevPubKey, &txin.scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, TransactionSignatureChecker(&txConst, i, amount), &serror)) {
            if (serror == SCRIPT_ERR_INVALID_STACK_OPERATION) {
//无法对输入进行签名，验证失败（可能尝试部分签名）。
                TxInErrorToJSON(txin, vErrors, "Unable to sign input, invalid stack size (possibly missing key)");
            } else {
                TxInErrorToJSON(txin, vErrors, ScriptErrorString(serror));
            }
        }
    }
    bool fComplete = vErrors.empty();

    UniValue result(UniValue::VOBJ);
    result.pushKV("hex", EncodeHexTx(mtx));
    result.pushKV("complete", fComplete);
    if (!vErrors.empty()) {
        result.pushKV("errors", vErrors);
    }

    return result;
}

static UniValue signrawtransactionwithkey(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 4)
        throw std::runtime_error(
            RPCHelpMan{"signrawtransactionwithkey",
                "\nSign inputs for raw transaction (serialized, hex-encoded).\n"
                "The second argument is an array of base58-encoded private\n"
                "keys that will be the only keys used to sign the transaction.\n"
                "The third optional argument (may be null) is an array of previous transaction outputs that\n"
                "this transaction depends on but may not yet be in the block chain.\n",
                {
                    /*exstring“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，“事务十六进制字符串”，
                    “私钥”，rpcarg:：type:：arr，/*opt*/ false, /* default_val */ "", "A json array of base58-encoded private keys for signing",

                        {
                            /*rivatekey“，rpcarg:：type:：str_hex，/*opt*/false，/*default_val*/”“，“private key in base58 encoding”，
                        }
                        }
                    “prevtxs”，rpcarg:：type:：arr，/*opt*/ true, /* default_val */ "null", "A json array of previous dependent transaction outputs",

                        {
                            /*，rpcarg：：type：：obj，/*opt*/true，/*默认值“/”，“”，
                                {
                                    “txid”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "The transaction id"},

                                    /*out“，rpcarg:：type:：num，/*opt*/false，/*default_val*/”，“输出编号”，
                                    “scriptpubkey”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "script key"},

                                    /*edeemscript“，rpcarg:：type:：str_hex，/*opt*/true，/*default_val*/“省略”，（p2sh或p2wsh需要）兑换脚本”，
                                    “金额”，rpcarg:：type:：amount，/*opt*/ false, /* default_val */ "", "The amount spent"},

                                },
                                },
                        },
                        },
                    /*ighashtype“，rpcarg:：type:：str，/*opt*/true，/*default_val*/”all“，”签名散列类型。必须是下列之一：
            “全部\ \ \ \n”
            “无\ \ \ \n”
            “单个\ \ \”n
            “全部\任何一个可以支付\ \ \n”
            “无任何一个可以支付\ \ \ \n”
            “单任意一个支付\ \ \ \n”
                    }
                }
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            ““hex\”：“value\”，（string）带有签名的十六进制编码的原始事务\n”
            ““完成”：如果事务有一组完整的签名，则为true false，（布尔值）。\n”
            “错误\”：[（JSON对象数组）脚本验证错误（如果有）。\n”
            “{n”
            “txid\”：“hash\”，（string）引用的上一个事务的哈希\n”
            ““vout”：n，（数字）要花费并用作输入的输出的索引\n”
            “scriptsig\”：“hex\”，（string）十六进制编码的签名脚本\n”
            ““序列号”：n，（数字）脚本序列号\n”
            “错误\”：“文本\”（字符串）与输入相关的验证或签名错误\n”
            “}\n”
            “……\n”
            “\n”
            “}\n”

            “\n实例：\n”
            +helpExamplecli（“signrawtransactionwithkey”，“myhex\”）
            +helpExampleRPC（“signrawtransactionwithkey”，“myhex\”）
        ；

    rpctypecheck（request.params，univalue:：vstr，univalue:：varr，univalue:：varr，univalue:：vstr，true）；

    可变交易MTX；
    如果（！）decodeHextx（mtx，request.params[0].get_str（），true））
        throw jsonrpcerror（rpc_反序列化_错误，“tx解码失败”）；
    }

    cbasickeystore密钥库；
    const univalue&keys=request.params[1].get_array（）；
    for（unsigned int idx=0；idx<keys.size（）；+idx）
        单值k=键[idx]；
        ckey key=decodeSecret（k.get_str（））；
        如果（！）key.isvalid（））
            throw jsonrpcerror（rpc_invalid_address_or_key，“invalid private key”）；
        }
        keystore.addkey（key）；
    }

    返回signtransaction（*g_rpc_interfaces->chain，mtx，request.params[2]，&keystore，true，request.params[3]）；
}

单值签名事务（const jsonrpcrequest&request）
{
    //应该在v0.19中完全删除此方法，以及
    //crpccommand表和rpc/client.cpp。
    throw jsonrpcerror（rpc_方法_已弃用，“signrawtransaction已在v0.18中删除。\n”
        “客户应过渡到使用signrawtransactionwithkey和signrawtransactionwithwallet”）；
}

静态单值sendrawtransaction（const jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）<1 request.params.size（）>2）
        throw std:：runtime_错误（
            rpchelpman“sendrawtransaction”，
                \n将原始事务（序列化，十六进制编码）提交到本地节点和网络。\n
                “\n另请参阅createrawtransaction和signrawtransactionwithkey调用。\n”，
                {
                    “hexstring”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "The hex string of the raw transaction"},

                    /*allow high fees“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”false“，”allow high fees”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “十六进制”（string）十六进制事务哈希\n”
            “\n实例：\n”
            \n创建事务\n
            +helpExamplecli（“createrawtransaction”，“[\ \ \”txid \ \ \”，“mytxid \ \ \ \ \”，“vout \ \ \ \”，：0]\“\”\“\ \\”myaddress \ \ \\“：0.01 \”）+
            签署事务，然后返回十六进制\n
            +helpExamplecli（“signrawtransactionwithwallet”，“myhex\”“）+
            \n发送事务（带签名的十六进制\n
            +helpExamplecli（“sendrawtransaction”，“\”signedhex\“”）+
            “作为JSON-RPC调用\n”
            +helpExampleRpc（“sendrawtransaction”，“signedhex”）。
        ；

    std:：promise<void>promise；

    rpctypecheck（request.params，univalue:：vstr，univalue:：vbool）；

    //从参数分析十六进制字符串
    可变交易MTX；
    如果（！）解码hextx（mtx，request.params[0].get_str（））
        throw jsonrpcerror（rpc_反序列化_错误，“tx解码失败”）；
    ctransactionref tx（makeTransactionRef（std:：move（mtx）））；
    const uint256&hashtx=tx->gethash（）；

    camount nmaxrawtxfee=maxtxfee；
    如果（！）request.params[1].isNull（）&&request.params[1].获取bool（））
        nmaxrawtxfee=0；

    //cs_主范围
    锁（CSKEMAN）；
    ccoinsviewcache&view=*pcoinstip；
    bool fhavechain=假；
    对于（尺寸=0；！fhavechain&o<tx->vout.size（）；o++）
        const coin&existingcoin=view.accesscoin（coutpoint（hashtx，o））；
        FAVELCK =！现存的coin.isspent（）；
    }
    bool fhavemempool=mempool.exists（hashtx）；
    如果（！）fhavemempool和&！FHVEVER链）
        //推到本地节点并与钱包同步
        cvalidationState状态；
        Bool fMissingInputs（输出轴）；
        如果（！）acceptToMoryPool（内存池、状态、std:：move（tx）和fMissingInputs，
                                nullptr/*pltxnreplaced（空指针/*pltxnreplaced）*/, false /* bypass_limits */, nMaxRawTxFee)) {

            if (state.IsInvalid()) {
                throw JSONRPCError(RPC_TRANSACTION_REJECTED, FormatStateMessage(state));
            } else {
                if (fMissingInputs) {
                    throw JSONRPCError(RPC_TRANSACTION_ERROR, "Missing inputs");
                }
                throw JSONRPCError(RPC_TRANSACTION_ERROR, FormatStateMessage(state));
            }
        } else {
//如果钱包已启用，请确保钱包已被告知
//返回前的新交易。这阻止了比赛
//用户可以使用事务调用sendrawtransaction
//到/从他们的钱包，立即呼叫一些钱包RPC，并
//由于回调尚未处理而导致的过时结果。
            CallFunctionInValidationInterfaceQueue([&promise] {
                promise.set_value();
            });
        }
    } else if (fHaveChain) {
        throw JSONRPCError(RPC_TRANSACTION_ALREADY_IN_CHAIN, "transaction already in block chain");
    } else {
//确保我们不会永远阻止如果重新发送
//一个事务已经在mempool中。
        promise.set_value();
    }

} //塞斯曼

    promise.get_future().wait();

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    CInv inv(MSG_TX, hashTx);
    g_connman->ForEachNode([&inv](CNode* pnode)
    {
        pnode->PushInventory(inv);
    });

    return hashTx.GetHex();
}

static UniValue testmempoolaccept(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"testmempoolaccept",
                "\nReturns result of mempool acceptance tests indicating if raw transaction (serialized, hex-encoded) would be accepted by mempool.\n"
                "\nThis checks if the transaction violates the consensus or policy rules.\n"
                "\nSee sendrawtransaction call.\n",
                {
                    /*awtxs“，rpcarg:：type:：arr，/*opt*/false，/*default_val*/”“，“原始事务的十六进制字符串数组。\n”
            “目前长度必须为1。”，
                        {
                            “rawtx”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", ""},

                        },
                        },
                    /*allow high fees“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”false“，”allow high fees”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “[（array）输入数组中每个原始事务的mempool接受测试的结果。\n”
            “现在长度正好是一个。\n”
            “{n”
            “txid”（string）十六进制事务哈希\n”
            “允许\”（布尔值），如果mempool允许插入此tx \n”
            “拒绝原因\”（string）拒绝字符串（仅当'allowed'为false时存在）\n”
            “}\n”
            “\n”
            “\n实例：\n”
            \n创建事务\n
            +helpExamplecli（“createrawtransaction”，“[\ \ \”txid \ \ \”，“mytxid \ \ \ \ \”，“vout \ \ \ \”，：0]\“\”\“\ \\”myaddress \ \ \\“：0.01 \”）+
            签署事务，然后返回十六进制\n
            +helpExamplecli（“signrawtransactionwithwallet”，“myhex\”“）+
            \n测试接受事务（签名十六进制\n
            +helpExamplecli（“testmempoolaccept”，“[\”signedhex \“]”），+
            “作为JSON-RPC调用\n”
            +helpExampleRPC（“testmempoolaccept”，“[\”signedhex \“]”）
            ；
    }

    rpctypecheck（request.params，univalue:：varr，univalue:：vbool）；
    if（request.params[0].get_array（）.size（）！= 1）{
        throw jsonrpcerror（rpc_invalid_参数，“array目前只能包含一个原始事务”）；
    }

    可变交易MTX；
    如果（！）decodeHextx（mtx，request.params[0].get_array（）[0].get_str（））
        throw jsonrpcerror（rpc_反序列化_错误，“tx解码失败”）；
    }
    ctransactionref tx（makeTransactionRef（std:：move（mtx）））；
    const uint256&tx_hash=tx->gethash（）；

    camount max_raw_tx_fee=：：max tx fee；
    如果（！）request.params[1].isNull（）&&request.params[1].获取bool（））
        最大原始费=0；
    }

    单值结果（单值：：varr）；
    单值结果_0（单值：：vobj）；
    结果_0.pushkv（“txid”，tx_hash.gethex（））；

    cvalidationState状态；
    bool缺少输入；
    布尔测试接受
    {
        锁（CSKEMAN）；
        test_accept_res=acceptToMoryPool（内存池、状态、std:：move（tx）和丢失的_输入，
            nullptr/*pltxnreplaced（空指针/*pltxnreplaced）*/, false /* bypass_limits */, max_raw_tx_fee, /* test_accept */ true);

    }
    result_0.pushKV("allowed", test_accept_res);
    if (!test_accept_res) {
        if (state.IsInvalid()) {
            result_0.pushKV("reject-reason", strprintf("%i: %s", state.GetRejectCode(), state.GetRejectReason()));
        } else if (missing_inputs) {
            result_0.pushKV("reject-reason", "missing-inputs");
        } else {
            result_0.pushKV("reject-reason", state.GetRejectReason());
        }
    }

    result.push_back(std::move(result_0));
    return result;
}

static std::string WriteHDKeypath(std::vector<uint32_t>& keypath)
{
    std::string keypath_str = "m";
    for (uint32_t num : keypath) {
        keypath_str += "/";
        bool hardened = false;
        if (num & 0x80000000) {
            hardened = true;
            num &= ~0x80000000;
        }

        keypath_str += std::to_string(num);
        if (hardened) {
            keypath_str += "'";
        }
    }
    return keypath_str;
}

UniValue decodepsbt(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"decodepsbt",
                "\nReturn a JSON object representing the serialized, base64-encoded partially signed Bitcoin transaction.\n",
                {
                    /*sbt“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，“psbt base64字符串”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            ““tx\”：（json对象）解码的网络序列化未签名事务。\n”
            “……布局与decoderawtransaction的输出相同。\n“
            “}，\n”
            “未知\”：（json对象）未知的全局字段\n”
            “key”：“value”（键值对）未知的键值对\n”
            “……n”
            “}，\n”
            “\”输入\“：[（JSON对象数组\n”
            “{n”
            “非见证UTXO”：（JSON对象，可选）为非见证UTXO解码网络事务\n”
            “……n”
            “}，\n”
            “见证”utxo\“：（json对象，可选）见证utxos的事务输出\n”
            “amount\”：x.x x x，（数字）以“+货币\单位+”表示的值。\n”
            “scriptpubkey\”：（json对象）\n”
            “\”asm\“：\”asm\“，（string）asm \n”
            “十六进制”：“十六进制”，（string）十六进制\n”
            “type\”：“pubkeyhash\”，（string）类型，例如'pubkeyhash'\n”
            “\”地址\“：\”地址\“（字符串）比特币地址，如果有\n”
            “}\n”
            “}，\n”
            “部分签名”：（JSON对象，可选）\n”
            “\”pubkey\“：\”signature\“，（string）与之对应的公钥和签名。\n”
            “，…\n”
            “}\n”
            “\”sighash\“：\”type\“，（string，可选）要使用的sighash类型\n”
            “\”兑换脚本\“：（JSON对象，可选）\n”
            “\”asm\“：\”asm\“，（string）asm \n”
            “十六进制”：“十六进制”，（string）十六进制\n”
            “type\”：“pubkeyhash\”，（string）类型，例如'pubkeyhash'\n”
            “}\n”
            “\”见证脚本\“：（JSON对象，可选）\n”
            “\”asm\“：\”asm\“，（string）asm \n”
            “十六进制”：“十六进制”，（string）十六进制\n”
            “type\”：“pubkeyhash\”，（string）类型，例如'pubkeyhash'\n”
            “}\n”
            “\”bip32_derivals\“：（json对象，可选）\n”
            “pubkey”：（json对象，可选）以派生路径为值的公钥。\n”
            “\”主\指纹\“：\”指纹\“（字符串）主密钥的指纹\n”
            “\”path\“：\”path\“，（string）路径\n”
            “}\n”
            “，…\n”
            “}\n”
            “\”final_scriptsig\“：（json对象，可选）\n”
            “\”asm\“：\”asm\“，（string）asm \n”
            “十六进制”：“十六进制”，（string）十六进制\n”
            “}\n”
            “最终\”脚本见证\“：[\”十六进制\“，…”（字符串数组）十六进制编码的见证数据（如果有）\n”
            “未知\”：（json对象）未知的全局字段\n”
            “key”：“value”（键值对）未知的键值对\n”
            “…\n”
            “}，\n”
            “}\n”
            “……\n”
            “\n”
            ““输出”：[（JSON对象数组）\n”
            “{n”
            “\”兑换脚本\“：（JSON对象，可选）\n”
            “\”asm\“：\”asm\“，（string）asm \n”
            “十六进制”：“十六进制”，（string）十六进制\n”
            “type\”：“pubkeyhash\”，（string）类型，例如'pubkeyhash'\n”
            “}\n”
            “\”见证脚本\“：（JSON对象，可选）\n”
            “\”asm\“：\”asm\“，（string）asm \n”
            “十六进制”：“十六进制”，（string）十六进制\n”
            “type\”：“pubkeyhash\”，（string）类型，例如'pubkeyhash'\n”
            “}\n”
            ““bip32”派生\“：[（json对象数组，可选）\n”
            “{n”
            “\”pubkey\“：\”pubkey\“，（string）此路径对应的公钥\n”
            “\”主\指纹\“：\”指纹\“（字符串）主密钥的指纹\n”
            “\”path\“：\”path\“，（string）路径\n”
            “}\n”
            “}\n”
            “，…\n”
            “”
            “未知\”：（json对象）未知的全局字段\n”
            “key”：“value”（键值对）未知的键值对\n”
            “…\n”
            “}，\n”
            “}\n”
            “……\n”
            “\n”
            “fee”：fee（数字，可选）如果psbt中的所有utxos插槽都已填满，则支付的事务费。\n”
            “}\n”

            “\n实例：\n”
            +helpexamplecli（“decodepsbt”，“psbt\”）
    ；

    rpctypecheck（request.params，univalue:：vstr）；

    //取消对事务的序列化
    部分已签名的事务PSBTX；
    std：：字符串错误；
    如果（！）decodepsbt（psbtx，request.params[0].get_str（），error））
        throw jsonrpcerror（rpc_反序列化_错误，strprintf（“tx解码失败%s”，错误））；
    }

    单值结果（单值：：vobj）；

    //添加解码后的Tx
    单值tx_univ（单值：：vobj）；
    txtouniv（ctransaction（*psbtx.tx），uint256（），tx_univ，false）；
    结果.pushkv（“tx”，tx_univ）；

    /未知数据
    单值未知（单值：：vobj）；
    for（自动输入：psbtx.未知）
        unknowns.pushkv（hexstr（entry.first），hexstr（entry.second））；
    }
    结果.pushkv（“未知”，未知）；

    //输入
    camount total_in=0；
    bool have_all_utxos=true；
    单值输入（单值：：varr）；
    for（unsigned int i=0；i<psbtx.inputs.size（）；++i）
        const psbtinput&input=psbtx.inputs[i]；
        单值输入（单值：：vobj）；
        / UTXOs
        如果（！）input.witness_utxo.isNull（））
            const ctxout&txout=input.witness_utxo；

            单值输出（单值：：vobj）；

            out.pushkv（“金额”，valuefromamount（txout.nvalue））；
            总_in+=txout.n值；

            单值o（单值：：vobj）；
            scripttouniv（txout.scriptpubkey，o，true）；
            out.pushkv（“scriptpubkey”，o）；
            in.pushkv（“见证人”，out）；
        else if（input.non_-witness_-utxo）
            单值非智能（单值：：vobj）；
            txtouniv（*input.non_-wit见证_Txo，uint256（），non_-wit，false）；
            in.pushkv（“非证人”，非证人）；
            总输入值-=输入值。非见证输入值->vout[psbtx.tx->vin[i].prevout.n].nvalue；
        }否则{
            将所有的输出设为假；
        }

        /部分SIGs
        如果（！）input.partial_sigs.empty（））
            单值部分符号（单值：：vobj）；
            for（const auto&sig:input.partial_sigs）
                部分_sigs.pushkv（hexstr（sig.second.first），hexstr（sig.second.second））；
            }
            in.pushkv（“部分签名”，部分签名）；
        }

        /风景
        如果（input.sighash_type>0）
            in.pushkv（“sighash”，sighashtosttr（（unsigned char）input.sighash_type））；
        }

        //赎回脚本和见证脚本
        如果（！）input.reduce_script.empty（））
            单值r（单值：：vobj）；
            scripttouniv（input.reduce_script，r，false）；
            in.pushkv（“赎回脚本”，r）；
        }
        如果（！）input.witness_script.empty（））
            单值r（单值：：vobj）；
            scripttouniv（input.witness_script，r，false）；
            in.pushkv（“证人脚本”，R）；
        }

        //键槽
        如果（！）input.hd_keypaths.empty（））
            单值密钥路径（单值：：varr）；
            用于（自动输入：input.hd_keypaths）
                单值键路径（单值：：vobj）；
                keypath.pushkv（“pubkey”，hexstr（entry.first））；

                keypath.pushkv（“master_fingerprint”，strprintf（“%08X”，readbe32（entry.second.fingerprint））；
                keypath.pushkv（“路径”，writehdkeypath（entry.second.path））；
                键路径。向后推（键路径）；
            }
            in.pushkv（“bip32_derivals”，keypaths）；
        }

        //最终的scriptsig和scriptwitness
        如果（！）input.final_script_sig.empty（））
            单值脚本sig（univalue:：vobj）；
            script sig.pushkv（“asm”，scriptToAsmstr（input.final_script_sig，true））；
            script sig.pushkv（“hex”，hexstr（input.final_script_sig））；
            in.pushkv（“final_scriptsig”，scriptsig）；
        }
        如果（！）input.final_script_witness.isNull（））
            单值txinwitness（单值：：varr）；
            for（const auto&item:input.final_script_witness.stack）
                txinwitness.push_back（hexstr（item.begin（），item.end（））；
            }
            in.pushkv（“最终脚本见证”，txinwitness）；
        }

        /未知数据
        if（input.unknown.size（）>0）
            单值未知（单值：：vobj）；
            for（自动输入：输入。未知）
                unknowns.pushkv（hexstr（entry.first），hexstr（entry.second））；
            }
            in.pushkv（“未知”，未知）；
        }

        输入。向后推（in）；
    }
    结果.pushkv（“输入”，输入）；

    //输出
    camount输出_值=0；
    单值输出（单值：：varr）；
    for（unsigned int i=0；i<psbtx.outputs.size（）；++i）
        const psbtoutput&output=psbtx.outputs[i]；
        单值输出（单值：：vobj）；
        //赎回脚本和见证脚本
        如果（！）output.reduce_script.empty（））
            单值r（单值：：vobj）；
            scripttouniv（output.reduce_script，r，false）；
            out.pushkv（“赎回脚本”，r）；
        }
        如果（！）output.witness_script.empty（））
            单值r（单值：：vobj）；
            scripttouniv（output.witness_script，r，false）；
            out.pushkv（“证人脚本”，r）；
        }

        //键槽
        如果（！）output.hd_keypaths.empty（））
            单值密钥路径（单值：：varr）；
            用于（自动输入：output.hd_keypaths）
                单值键路径（单值：：vobj）；
                keypath.pushkv（“pubkey”，hexstr（entry.first））；
                keypath.pushkv（“master_fingerprint”，strprintf（“%08X”，readbe32（entry.second.fingerprint））；
                keypath.pushkv（“路径”，writehdkeypath（entry.second.path））；
                键路径。向后推（键路径）；
            }
            out.pushkv（“bip32_derivals”，关键路径）；
        }

        /未知数据
        if（output.unknown.size（）>0）
            单值未知（单值：：vobj）；
            for（自动输入：输出。未知）
                unknowns.pushkv（hexstr（entry.first），hexstr（entry.second））；
            }
            out.pushkv（“未知”，未知）；
        }

        输出。向后（向外）推；

        //费用计算
        输出值+=psbtx.tx->vout[i].nvalue；
    }
    结果.pushkv（“输出”，输出）；
    如果（所有的都有）
        结果.pushkv（“费用”，金额值（总输入输出值））；
    }

    返回结果；
}

univalue combinepsbt（const jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 1）
        throw std:：runtime_错误（
            rpchelpman“combinepsbt”，
                \n将多个部分签名的比特币交易组合成一个交易。\n
                “实现合并器角色。\n”，
                {
                    “txs”，rpcarg:：type:：arr，/*opt*/ false, /* default_val */ "", "A json array of base64 strings of partially signed transactions",

                        {
                            /*sbt“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，“psbt的base64字符串”，
                        }
                        }
                }
                toSTRIN（）+
            “\NESRES:\N”
            “psbt”（字符串）base64编码的部分签名事务\n”
            “\n实例：\n”
            +helpexamplecli（“combinepsbt”，“[\”mybase64_1\“，\”mybase64_2\“，\”mybase64_3\“]”）
        ；

    rpctypecheck（request.params，univalue:：varr，true）；

    //取消对事务的序列化
    std:：vector<partiallysignedtransaction>psbtxs；
    univalue txs=request.params[0].get_array（）；
    for（unsigned int i=0；i<txs.size（）；++i）
        部分已签名的事务PSBTX；
        std：：字符串错误；
        如果（！）decodepsbt（psbtx，txs[i].get_str（），error））
            throw jsonrpcerror（rpc_反序列化_错误，strprintf（“tx解码失败%s”，错误））；
        }
        psbtxs.推回（psbtx）；
    }

    partiallySignedTransaction合并后的_psbt（psbtxs[0]）；//复制第一个

    /合并
    for（auto it=std:：next（psbtxs.begin（））；it！=psbtxs.end（）；++it）
        如果（*）！=合并后的_
            throw jsonrpcerror（rpc_invalid_参数，“psbts不引用相同的事务”）；
        }
        合并后的合并（*it）；
    }
    如果（！）合并后的psbt.issane（））
        throw jsonrpcerror（rpc_无效的_参数，“合并的psbt不一致”）；
    }

    单值结果（单值：：vobj）；
    cdatastream sstx（ser_网络，协议版本）；
    sstx<<合并后的psbt；
    返回encodebase64（（无符号字符*）sstx.data（），sstx.size（））；
}

univalue finishepsbt（const jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）<1 request.params.size（）>2）
        throw std:：runtime_错误（
            rpchelpman“Finaliseppt”，
                “最终确定PSBT的输入。如果事务完全签名，它将生成一个\n“
                “可以使用sendrawtransaction广播的网络序列化事务。否则，PSBT将是\n“
                “已创建，其中为完成的输入填充了最终的\u scriptsig和最终的\u scriptswitness字段。\n”
                “实现终结器和提取程序角色。\n”，
                {
                    “psbt”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "A base64 string of a PSBT"},

                    /*xtract“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”true“，”如果为true，则事务完成，\n”
            “提取并返回正常网络序列化中的完整事务，而不是PSBT。”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            “\”psbt\“：\”value\“，（string）未提取的base64编码的部分签名事务\n”
            ““hex\”：“value\”，（string）提取十六进制编码的网络事务\n”
            ““完成”：如果事务有一组完整的签名，则为true false，（布尔值）。\n”
            “\n”
            “}\n”

            “\n实例：\n”
            +helpexamplecli（“finalisepsbt”，“psbt”）。
        ；

    rpctypecheck（request.params，univalue:：vstr，univalue:：vbool，true）；

    //取消对事务的序列化
    部分已签名的事务PSBTX；
    std：：字符串错误；
    如果（！）decodepsbt（psbtx，request.params[0].get_str（），error））
        throw jsonrpcerror（rpc_反序列化_错误，strprintf（“tx解码失败%s”，错误））；
    }

    //最后确定输入签名--以防我们有部分签名加起来是完整的
    //签名，但尚未组合它们（例如，因为创建此签名的合并器
    //PartiallySignedTransaction不理解它们），这会将它们合并为最终结果
    //脚本。
    布尔完成=真；
    for（unsigned int i=0；i<psbtx.tx->vin.size（）；++i）
        完成&=signpsbtinput（虚拟签名提供者、psbtx、i、sighash_all）；
    }

    单值结果（单值：：vobj）；
    cdatastream sstx（ser_网络，协议版本）；
    bool extract=request.params[1].isNull（）（！request.params[1].isNull（）&&request.params[1].获取bool（））；
    如果（完成并提取）
        CMutableTransaction MTX（*psbtx.tx）；
        for（unsigned int i=0；i<mtx.vin.size（）；++i）
            mtx.vin[i].script sig=psbtx.inputs[i].final_script_sig；
            mtx.vin[i].script witness=psbtx.inputs[i].最终脚本见证；
        }
        SSTX＜MTX；
        结果.pushkv（“hex”，hex str（sstx.str（））；
    }否则{
        SSTX＜PSBTX；
        结果.pushkv（“psbt”，encodebase64（sstx.str（））；
    }
    结果.pushkv（“完成”，完成）；

    返回结果；
}

univalue createpsbt（const jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）<2 request.params.size（）>4）
        throw std:：runtime_错误（
            rpchelpman“创建psbt”，
                以部分签名的事务格式创建事务。\n
                “实现创建者角色。\n”，
                {
                    “输入”，rpcarg:：type:：arr，/*opt*/ false, /* default_val */ "", "A json array of json objects",

                        {
                            /*，rpcarg:：type:：obj，/*opt*/false，/*默认值“/”，“”，
                                {
                                    “txid”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "The transaction id"},

                                    /*out“，rpcarg:：type:：num，/*opt*/false，/*default_val*/”，“输出编号”，
                                    “序列”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "depends on the value of the 'replaceable' and 'locktime' arguments", "The sequence number"},

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
                                    /*地址“，rpcarg:：type:：amount，/*opt*/false，/*default_val*/”“，”键-值对。键（字符串）是比特币地址，值（浮点数或字符串）是以“+货币单位”表示的金额，
                                }
                                }
                            “”，rpcarg:：type:：obj，/*opt*/ true, /* default_val */ "", "",

                                {
                                    /*ata“，rpcarg:：type:：str_hex，/*opt*/false，/*默认值“/”，“一个键值对。键必须为“data”，值为十六进制编码数据“，
                                }
                                }
                        }
                        }
                    “锁定时间”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "0", "Raw locktime. Non-0 value also locktime-activates inputs"},

                    /*eplaceable“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”false“，”将此事务标记为bip125可替换。\n”
                            “允许将此交易替换为收费较高的交易。如果提供，如果显式序列号不兼容，则为错误。“，
                }
                toSTRIN（）+
                            “\NESRES:\N”
                            “psbt”（string）生成的原始事务（base64编码字符串）\n”
                            “\n实例：\n”
                            +helpExamplecli（“createpsbt”，“[\ \ \”txid \ \ \ \”，“myid \ \ \ \”，“vout \ \ \”，“0]\”“[\ \ \ \”data \ \ \”，“00010203 \”，“]\”“）
                            ；


    rpctypecheck（request.params，
        UnValue:：
        univalueType（），//arr或obj，稍后检查
        UnValue:VNUM，
        单值：：vbool，
        }，真的
    ；

    cmutableTransaction rawtx=constructTransaction（request.params[0]，request.params[1]，request.params[2]，request.params[3]）；

    //生成空白PSBT
    部分已签名的事务PSBTX；
    psbtx.tx=rawtx；
    for（unsigned int i=0；i<rawtx.vin.size（）；++i）
        psbtx.inputs.push_back（psbtinput（））；
    }
    for（unsigned int i=0；i<rawtx.vout.size（）；++i）
        psbtx.outputs.push_back（psbtoutput（））；
    }

    //序列化psbt
    cdatastream sstx（ser_网络，协议版本）；
    SSTX＜PSBTX；

    返回encodebase64（（无符号字符*）sstx.data（），sstx.size（））；
}

单值converttopsbt（const jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）<1 request.params.size（）>3）
        throw std:：runtime_错误（
            rpchelpman“converttopsbt”，
                \n将网络序列化事务转换为PSBT。这只能用于createrawtransaction和fundrawtransaction \n“
                “应将createpsbt和walletcreatefundedpsbt用于新应用程序。\n”，
                {
                    “hexstring”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "The hex string of a raw transaction"},

                    /*ermitsigdata“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”false“，”如果为true，则将丢弃输入中的任何签名并进行转换。\n”
                            “将继续。如果为false，则存在任何签名时，RPC将失败。“，
                    “iswitness”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "depends on heuristic tests", "Whether the transaction hex is a serialized witness transaction.\n"

                            "                              If iswitness is not present, heuristic tests will be used in decoding. If true, only witness deserializaion\n"
                            "                              will be tried. If false, only non-witness deserialization will be tried. Only has an effect if\n"
                            "                              permitsigdata is true."},
                }}
                .ToString() +
                            "\nResult:\n"
                            "  \"psbt\"        (string)  The resulting raw transaction (base64-encoded string)\n"
                            "\nExamples:\n"
                            "\nCreate a transaction\n"
                            + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"[{\\\"data\\\":\\\"00010203\\\"}]\"") +
                            "\nConvert the transaction to a PSBT\n"
                            + HelpExampleCli("converttopsbt", "\"rawtransaction\"")
                            );


    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VBOOL, UniValue::VBOOL}, true);

//从参数分析十六进制字符串
    CMutableTransaction tx;
    bool permitsigdata = request.params[1].isNull() ? false : request.params[1].get_bool();
    bool witness_specified = !request.params[2].isNull();
    bool iswitness = witness_specified ? request.params[2].get_bool() : false;
    bool try_witness = permitsigdata ? (witness_specified ? iswitness : true) : false;
    bool try_no_witness = permitsigdata ? (witness_specified ? !iswitness : true) : true;
    if (!DecodeHexTx(tx, request.params[0].get_str(), try_no_witness, try_witness)) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

//从输入中删除所有脚本签名和脚本见证人
    for (CTxIn& input : tx.vin) {
        if ((!input.scriptSig.empty() || !input.scriptWitness.IsNull()) && !permitsigdata) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Inputs must not have scriptSigs and scriptWitnesses");
        }
        input.scriptSig.clear();
        input.scriptWitness.SetNull();
    }

//制作空白PSBT
    PartiallySignedTransaction psbtx;
    psbtx.tx = tx;
    for (unsigned int i = 0; i < tx.vin.size(); ++i) {
        psbtx.inputs.push_back(PSBTInput());
    }
    for (unsigned int i = 0; i < tx.vout.size(); ++i) {
        psbtx.outputs.push_back(PSBTOutput());
    }

//序列化PSBT
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << psbtx;

    return EncodeBase64((unsigned char*)ssTx.data(), ssTx.size());
}

//Clang格式关闭
static const CRPCCommand commands[] =
{ //类别名称actor（function）argnames
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    { "rawtransactions",    "getrawtransaction",            &getrawtransaction,         {"txid","verbose","blockhash"} },
    { "rawtransactions",    "createrawtransaction",         &createrawtransaction,      {"inputs","outputs","locktime","replaceable"} },
    { "rawtransactions",    "decoderawtransaction",         &decoderawtransaction,      {"hexstring","iswitness"} },
    { "rawtransactions",    "decodescript",                 &decodescript,              {"hexstring"} },
    { "rawtransactions",    "sendrawtransaction",           &sendrawtransaction,        {"hexstring","allowhighfees"} },
    { "rawtransactions",    "combinerawtransaction",        &combinerawtransaction,     {"txs"} },
    { "hidden",             "signrawtransaction",           &signrawtransaction,        {"hexstring","prevtxs","privkeys","sighashtype"} },
    { "rawtransactions",    "signrawtransactionwithkey",    &signrawtransactionwithkey, {"hexstring","privkeys","prevtxs","sighashtype"} },
    { "rawtransactions",    "testmempoolaccept",            &testmempoolaccept,         {"rawtxs","allowhighfees"} },
    { "rawtransactions",    "decodepsbt",                   &decodepsbt,                {"psbt"} },
    { "rawtransactions",    "combinepsbt",                  &combinepsbt,               {"txs"} },
    { "rawtransactions",    "finalizepsbt",                 &finalizepsbt,              {"psbt", "extract"} },
    { "rawtransactions",    "createpsbt",                   &createpsbt,                {"inputs","outputs","locktime","replaceable"} },
    { "rawtransactions",    "converttopsbt",                &converttopsbt,             {"hexstring","permitsigdata","iswitness"} },

    { "blockchain",         "gettxoutproof",                &gettxoutproof,             {"txids", "blockhash"} },
    { "blockchain",         "verifytxoutproof",             &verifytxoutproof,          {"proof"} },
};
//CLAN格式

void RegisterRawTransactionRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
