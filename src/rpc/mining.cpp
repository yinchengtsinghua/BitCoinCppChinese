
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
#include <chainparams.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <key_io.h>
#include <miner.h>
#include <net.h>
#include <policy/fees.h>
#include <pow.h>
#include <rpc/blockchain.h>
#include <rpc/mining.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <shutdown.h>
#include <txmempool.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <validation.h>
#include <validationinterface.h>
#include <versionbitsinfo.h>
#include <warnings.h>

#include <memory>
#include <stdint.h>

unsigned int ParseConfirmTarget(const UniValue& value)
{
    int target = value.get_int();
    unsigned int max_target = ::feeEstimator.HighestTargetTracked(FeeEstimateHorizon::LONG_HALFLIFE);
    if (target < 1 || (unsigned int)target > max_target) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid conf_target, must be between %u - %u", 1, max_target));
    }
    return (unsigned int)target;
}

/*
 *根据上一个“查找”块返回平均每秒网络哈希，
 *如果“查找”是非正的，则从上一个难度更改。
 *如果“高度”为非负值，则在找到给定块时计算估计值。
 **/

static UniValue GetNetworkHashPS(int lookup, int height) {
    CBlockIndex *pb = chainActive.Tip();

    if (height >= 0 && height < chainActive.Height())
        pb = chainActive[height];

    if (pb == nullptr || !pb->nHeight)
        return 0;

//如果查找为-1，则使用自上次困难更改以来的块。
    if (lookup <= 0)
        lookup = pb->nHeight % Params().GetConsensus().DifficultyAdjustmentInterval() + 1;

//如果查找大于链，则将其设置为链长度。
    if (lookup > pb->nHeight)
        lookup = pb->nHeight;

    CBlockIndex *pb0 = pb;
    int64_t minTime = pb0->GetBlockTime();
    int64_t maxTime = minTime;
    for (int i = 0; i < lookup; i++) {
        pb0 = pb0->pprev;
        int64_t time = pb0->GetBlockTime();
        minTime = std::min(time, minTime);
        maxTime = std::max(time, maxTime);
    }

//在mintime==maxtime的情况下，我们不希望出现被零除的异常。
    if (minTime == maxTime)
        return 0;

    arith_uint256 workDiff = pb->nChainWork - pb0->nChainWork;
    int64_t timeDiff = maxTime - minTime;

    return workDiff.getdouble() / timeDiff;
}

static UniValue getnetworkhashps(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            RPCHelpMan{"getnetworkhashps",
                "\nReturns the estimated network hashes per second based on the last n blocks.\n"
                "Pass in [blocks] to override # of blocks, -1 specifies since last difficulty change.\n"
                "Pass in [height] to estimate the network speed at the time when a certain block was found.\n",
                {
                    /*“块”，rpcarg:：type:：num，/*opt*/true，/*default_val*/“120”，“自上一个难度更改后的块数，或-1。”，
                    “高度”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "-1", "To estimate at the time of the given height."},

                }}
                .ToString() +
            "\nResult:\n"
            "x             (numeric) Hashes per second estimated\n"
            "\nExamples:\n"
            + HelpExampleCli("getnetworkhashps", "")
            + HelpExampleRpc("getnetworkhashps", "")
       );

    LOCK(cs_main);
    return GetNetworkHashPS(!request.params[0].isNull() ? request.params[0].get_int() : 120, !request.params[1].isNull() ? request.params[1].get_int() : -1);
}

UniValue generateBlocks(std::shared_ptr<CReserveScript> coinbaseScript, int nGenerate, uint64_t nMaxTries, bool keepScript)
{
    static const int nInnerLoopCount = 0x10000;
    int nHeightEnd = 0;
    int nHeight = 0;

{   //不要保持CSU主锁
        LOCK(cs_main);
        nHeight = chainActive.Height();
        nHeightEnd = nHeight+nGenerate;
    }
    unsigned int nExtraNonce = 0;
    UniValue blockHashes(UniValue::VARR);
    while (nHeight < nHeightEnd && !ShutdownRequested())
    {
        std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(Params()).CreateNewBlock(coinbaseScript->reserveScript));
        if (!pblocktemplate.get())
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't create new block");
        CBlock *pblock = &pblocktemplate->block;
        {
            LOCK(cs_main);
            IncrementExtraNonce(pblock, chainActive.Tip(), nExtraNonce);
        }
        while (nMaxTries > 0 && pblock->nNonce < nInnerLoopCount && !CheckProofOfWork(pblock->GetHash(), pblock->nBits, Params().GetConsensus())) {
            ++pblock->nNonce;
            --nMaxTries;
        }
        if (nMaxTries == 0) {
            break;
        }
        if (pblock->nNonce == nInnerLoopCount) {
            continue;
        }
        std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
        if (!ProcessNewBlock(Params(), shared_pblock, true, nullptr))
            throw JSONRPCError(RPC_INTERNAL_ERROR, "ProcessNewBlock, block not accepted");
        ++nHeight;
        blockHashes.push_back(pblock->GetHash().GetHex());

//将脚本标记为重要，因为如果脚本来自钱包，它至少用于一个coinbase输出
        if (keepScript)
        {
            coinbaseScript->KeepScript();
        }
    }
    return blockHashes;
}

static UniValue generatetoaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw std::runtime_error(
            RPCHelpMan{"generatetoaddress",
                "\nMine blocks immediately to a specified address (before the RPC call returns)\n",
                {
                    /*blocks“，rpcarg:：type:：num，/*opt*/false，/*default_val*/”，“立即生成多少块。”，
                    “地址”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The address to send the newly generated bitcoin to."},

                    /*axtries“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”1000000“，要尝试多少次迭代。”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “[blockhashes]（array）生成的块的哈希\n”
            “\n实例：\n”
            \n向myaddress生成11个块\n
            +helpexamplecli（“generatetoaddress”，“11\”myaddress\“”）
            +“如果您运行的是比特币核心钱包，您可以获得一个新地址，以将新生成的比特币发送到以下地址：\n”
            +helpexamplecli（“getnewaddress”，“”）
        ；

    int ngenerate=request.params[0].get_int（）；
    uint64_t n最大值=1000000；
    如果（！）request.params[2].isNull（））
        nmaxtries=request.params[2].get_int（）；
    }

    ctxDestination destination=decodeDestination（request.params[1].get_str（））；
    如果（！）ISvalidDestination（目的地））
        throw jsonrpcerror（rpc_invalid_address_or_key，“错误：无效地址”）；
    }

    std:：shared_ptr<creservescript>coinbasescript=std:：make_shared<creservescript>（）；
    CoinBaseScript->ReserveScript=GetScriptForDestination（目标）；

    返回generateblocks（coinbasescript、ngenerate、nmaxtries、false）；
}

静态单值getmininginfo（const jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 0）
        throw std:：runtime_错误（
            rpchelpman“获取最小信息”，
                “”，返回包含挖掘相关信息的JSON对象。“”
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            “块\”：n n n，（数字）当前块\n”
            “当前块权重”：n n n，（数字）最后一个块权重\n”
            ““currentblocktx\”：n n n，（数字）最后一个块事务\n”
            “难度\”：xxx.xxxxx（数字）当前难度\n”
            ““networkhashps\”：n n n，（数字）每秒网络哈希数\n”
            “pooledtx”：n（数字）内存池的大小\n”
            ““chain\”：“XXXX\”，（string）bip70中定义的当前网络名称（main，test，regtest）\n”
            “警告\”“：\”“…\”（字符串）任何网络和区块链警告\n”
            “}\n”
            “\n实例：\n”
            +helpexamplecli（“getmininginfo”，“”）
            +helpexamplerpc（“getmininginfo”，“”）
        ；


    锁（CSKEMAN）；

    单值对象（单值：：vobj）；
    obj.pushkv（“块”，（int）chainactive.height（））；
    obj.pushkv（“当前块权重”，（uint64_t）nlastblockweight）；
    对象pushkv（“currentblocktx”，（uint64_t）nlastblocktx）；
    obj.pushkv（“难度”，（double）getDifficulty（chainActive.tip（））；
    obj.pushkv（“networkhashps”，getnetworkhashps（request））；
    obj.pushkv（“pooledtx”，（uint64_t）mempool.size（））；
    obj.pushkv（“链”，params（）.networkidstring（））；
    obj.pushkv（“警告”，getwarnings（“statusbar”））；
    返回对象；
}


//注意：与使用BTC值的wallet rpc不同，挖掘rpc使用satoshi值时遵循gbt（bip 22）
静态单值优先级转换（const-jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 3）
        throw std:：runtime_错误（
            rpchelpman“优先级转移”，
                “以更高（或更低）优先级接受挖掘块中的事务\n”，
                {
                    “txid”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "The transaction id."},

                    /*ummy“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”null“，”与以前的API兼容。必须为零或空。\n“
            “已弃用。为了向前兼容，请使用命名参数并省略此参数。“，
                    “费用_delta”，rpcarg:：type:：num，/*可选*/ false, /* default_val */ "", "The fee value (in satoshis) to add (or subtract, if negative).\n"

            "                  Note, that this value is not a fee rate. It is a value to modify absolute fee of the TX.\n"
            "                  The fee is not actually paid, only the algorithm for selecting transactions into a block\n"
            "                  considers the transaction as it would have paid a higher (or lower) fee."},
                }}
                .ToString() +
            "\nResult:\n"
            "true              (boolean) Returns true\n"
            "\nExamples:\n"
            + HelpExampleCli("prioritisetransaction", "\"txid\" 0.0 10000")
            + HelpExampleRpc("prioritisetransaction", "\"txid\", 0.0, 10000")
        );

    LOCK(cs_main);

    uint256 hash(ParseHashV(request.params[0], "txid"));
    CAmount nAmount = request.params[2].get_int64();

    if (!(request.params[1].isNull() || request.params[1].get_real() == 0)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Priority is no longer supported, dummy argument to prioritisetransaction must be 0.");
    }

    mempool.PrioritiseTransaction(hash, nAmount);
    return true;
}


//注意：假设结果是决定性的；如果结果不确定，则必须由调用方处理。
static UniValue BIP22ValidationResult(const CValidationState& state)
{
    if (state.IsValid())
        return NullUniValue;

    if (state.IsError())
        throw JSONRPCError(RPC_VERIFY_ERROR, FormatStateMessage(state));
    if (state.IsInvalid())
    {
        std::string strRejectReason = state.GetRejectReason();
        if (strRejectReason.empty())
            return "rejected";
        return strRejectReason;
    }
//应该是不可能的
    return "valid?";
}

static std::string gbt_vb_name(const Consensus::DeploymentPos pos) {
    const struct VBDeploymentInfo& vbinfo = VersionBitsDeploymentInfo[pos];
    std::string s = vbinfo.name;
    if (!vbinfo.gbt_force) {
        s.insert(s.begin(), '!');
    }
    return s;
}

static UniValue getblocktemplate(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            RPCHelpMan{"getblocktemplate",
                "\nIf the request parameters include a 'mode' key, that is used to explicitly select between the default 'template' request or a 'proposal'.\n"
                "It returns data needed to construct a block to work on.\n"
                "For full specification, see BIPs 22, 23, 9, and 145:\n"
"    https://github.com/bitcoin/bips/blob/master/bip-0022.mediawiki\n“
"    https://github.com/bitcoin/bips/blob/master/bip-0023.mediawiki\n“
"    https://github.com/bitcoin/bips/blob/master/bip-0009.mediawiki getblocktemplate_changes\n“
"    https://github.com/bitcoin/bips/blob/master/bip-0145.mediawiki\n“，
                {
                    /*emplate_-request“，rpcarg:：type:：obj，/*opt*/false，/*default_-val*/”“，“以下规范中的JSON对象”，
                        {
                            “模式”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "", "This must be set to \"template\", \"proposal\" (see BIP 23), or omitted"},

                            /*功能”，rpcarg:：type:：arr，/*opt*/true，/*default_val*/”“，“字符串列表”，
                                {
                                    “支持”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "", "client side supported feature, 'longpoll', 'coinbasetxn', 'coinbasevalue', 'proposal', 'serverlist', 'workid'"},

                                },
                                },
                            /*ules“，rpcarg:：type:：arr，/*opt*/false，/*default_val*/”“，“字符串列表”，
                                {
                                    “支持”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "", "client side supported softfork deployment"},

                                },
                                },
                        },
                        "\"template_request\""},
                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"version\" : n,                    (numeric) The preferred block version\n"
            "  \"rules\" : [ \"rulename\", ... ],    (array of strings) specific block rules that are to be enforced\n"
            "  \"vbavailable\" : {                 (json object) set of pending, supported versionbit (BIP 9) softfork deployments\n"
            "      \"rulename\" : bitnumber          (numeric) identifies the bit number as indicating acceptance and readiness for the named softfork rule\n"
            "      ,...\n"
            "  },\n"
            "  \"vbrequired\" : n,                 (numeric) bit mask of versionbits the server requires set in submissions\n"
            "  \"previousblockhash\" : \"xxxx\",     (string) The hash of current highest block\n"
            "  \"transactions\" : [                (array) contents of non-coinbase transactions that should be included in the next block\n"
            "      {\n"
            "         \"data\" : \"xxxx\",             (string) transaction data encoded in hexadecimal (byte-for-byte)\n"
            "         \"txid\" : \"xxxx\",             (string) transaction id encoded in little-endian hexadecimal\n"
            "         \"hash\" : \"xxxx\",             (string) hash encoded in little-endian hexadecimal (including witness data)\n"
            "         \"depends\" : [                (array) array of numbers \n"
            "             n                          (numeric) transactions before this one (by 1-based index in 'transactions' list) that must be present in the final block if this one is\n"
            "             ,...\n"
            "         ],\n"
            "         \"fee\": n,                    (numeric) difference in value between transaction inputs and outputs (in satoshis); for coinbase transactions, this is a negative Number of the total collected block fees (ie, not including the block subsidy); if key is not present, fee is unknown and clients MUST NOT assume there isn't one\n"
            "         \"sigops\" : n,                (numeric) total SigOps cost, as counted for purposes of block limits; if key is not present, sigop cost is unknown and clients MUST NOT assume it is zero\n"
            "         \"weight\" : n,                (numeric) total transaction weight, as counted for purposes of block limits\n"
            "      }\n"
            "      ,...\n"
            "  ],\n"
            "  \"coinbaseaux\" : {                 (json object) data that should be included in the coinbase's scriptSig content\n"
            "      \"flags\" : \"xx\"                  (string) key name is to be ignored, and value included in scriptSig\n"
            "  },\n"
            "  \"coinbasevalue\" : n,              (numeric) maximum allowable input to coinbase transaction, including the generation award and transaction fees (in satoshis)\n"
            "  \"coinbasetxn\" : { ... },          (json object) information for coinbase transaction\n"
            "  \"target\" : \"xxxx\",                (string) The hash target\n"
            "  \"mintime\" : xxx,                  (numeric) The minimum timestamp appropriate for next block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"mutable\" : [                     (array of string) list of ways the block template may be changed \n"
            "     \"value\"                          (string) A way the block template may be changed, e.g. 'time', 'transactions', 'prevblock'\n"
            "     ,...\n"
            "  ],\n"
            "  \"noncerange\" : \"00000000ffffffff\",(string) A range of valid nonces\n"
            "  \"sigoplimit\" : n,                 (numeric) limit of sigops in blocks\n"
            "  \"sizelimit\" : n,                  (numeric) limit of block size\n"
            "  \"weightlimit\" : n,                (numeric) limit of block weight\n"
            "  \"curtime\" : ttt,                  (numeric) current timestamp in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"bits\" : \"xxxxxxxx\",              (string) compressed target of next block\n"
            "  \"height\" : n                      (numeric) The height of the next block\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("getblocktemplate", "{\"rules\": [\"segwit\"]}")
            + HelpExampleRpc("getblocktemplate", "{\"rules\": [\"segwit\"]}")
         );

    LOCK(cs_main);

    std::string strMode = "template";
    UniValue lpval = NullUniValue;
    std::set<std::string> setClientRules;
    int64_t nMaxVersionPreVB = -1;
    if (!request.params[0].isNull())
    {
        const UniValue& oparam = request.params[0].get_obj();
        const UniValue& modeval = find_value(oparam, "mode");
        if (modeval.isStr())
            strMode = modeval.get_str();
        else if (modeval.isNull())
        {
            /*什么也不做*/
        }
        else
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode");
        lpval = find_value(oparam, "longpollid");

        if (strMode == "proposal")
        {
            const UniValue& dataval = find_value(oparam, "data");
            if (!dataval.isStr())
                throw JSONRPCError(RPC_TYPE_ERROR, "Missing data String key for proposal");

            CBlock block;
            if (!DecodeHexBlk(block, dataval.get_str()))
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block decode failed");

            uint256 hash = block.GetHash();
            const CBlockIndex* pindex = LookupBlockIndex(hash);
            if (pindex) {
                if (pindex->IsValid(BLOCK_VALID_SCRIPTS))
                    return "duplicate";
                if (pindex->nStatus & BLOCK_FAILED_MASK)
                    return "duplicate-invalid";
                return "duplicate-inconclusive";
            }

            CBlockIndex* const pindexPrev = chainActive.Tip();
//testblockvalidity仅支持基于当前提示构建的块
            if (block.hashPrevBlock != pindexPrev->GetBlockHash())
                return "inconclusive-not-best-prevblk";
            CValidationState state;
            TestBlockValidity(state, Params(), block, pindexPrev, false, true);
            return BIP22ValidationResult(state);
        }

        const UniValue& aClientRules = find_value(oparam, "rules");
        if (aClientRules.isArray()) {
            for (unsigned int i = 0; i < aClientRules.size(); ++i) {
                const UniValue& v = aClientRules[i];
                setClientRules.insert(v.get_str());
            }
        } else {
//注意：如果支持versionBits，则不读取此内容很重要
            const UniValue& uvMaxVersion = find_value(oparam, "maxversion");
            if (uvMaxVersion.isNum()) {
                nMaxVersionPreVB = uvMaxVersion.get_int64();
            }
        }
    }

    if (strMode != "template")
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode");

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    if (g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) == 0)
        throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "Bitcoin is not connected!");

    if (IsInitialBlockDownload())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Bitcoin is downloading blocks...");

    static unsigned int nTransactionsUpdatedLast;

    if (!lpval.isNull())
    {
//等待响应，直到最佳块更改或一分钟过去，并且有更多事务
        uint256 hashWatchedChain;
        std::chrono::steady_clock::time_point checktxtime;
        unsigned int nTransactionsUpdatedLastLP;

        if (lpval.isStr())
        {
//格式：<hashbestchain><ntransactionsupdatedlast>
            std::string lpstr = lpval.get_str();

            hashWatchedChain = ParseHashV(lpstr.substr(0, 64), "longpollid");
            nTransactionsUpdatedLastLP = atoi64(lpstr.substr(64));
        }
        else
        {
//注意：spec没有为非字符串longpollid指定行为，但是这使得测试更加容易。
            hashWatchedChain = chainActive.Tip()->GetBlockHash();
            nTransactionsUpdatedLastLP = nTransactionsUpdatedLast;
        }

//等待时释放钱包和主锁
        LEAVE_CRITICAL_SECTION(cs_main);
        {
            checktxtime = std::chrono::steady_clock::now() + std::chrono::minutes(1);

            WAIT_LOCK(g_best_block_mutex, lock);
            while (g_best_block == hashWatchedChain && IsRPCRunning())
            {
                if (g_best_block_cv.wait_until(lock, checktxtime) == std::cv_status::timeout)
                {
//超时：检查事务以进行更新
                    if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLastLP)
                        break;
                    checktxtime += std::chrono::seconds(10);
                }
            }
        }
        ENTER_CRITICAL_SECTION(cs_main);

        if (!IsRPCRunning())
            throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "Shutting down");
//TODO:可能需要重新检查连接/IBD并（如果有问题）立即发送过期模板以停止矿工？
    }

    const struct VBDeploymentInfo& segwit_info = VersionBitsDeploymentInfo[Consensus::DEPLOYMENT_SEGWIT];
//必须使用规则中设置的“segwit”调用gbt
    if (setClientRules.count(segwit_info.name) != 1) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "getblocktemplate must be called with the segwit rule set (call with {\"rules\": [\"segwit\"]})");
    }

//更新块
    static CBlockIndex* pindexPrev;
    static int64_t nStart;
    static std::unique_ptr<CBlockTemplate> pblocktemplate;
    if (pindexPrev != chainActive.Tip() ||
        (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 5))
    {
//清除pindexprev，以便将来的调用生成新的块，尽管从现在开始出现任何故障
        pindexPrev = nullptr;

//存储创建新锁之前使用的pindexBest，以避免比赛
        nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
        CBlockIndex* pindexPrevNew = chainActive.Tip();
        nStart = GetTime();

//创建新块
        CScript scriptDummy = CScript() << OP_TRUE;
        pblocktemplate = BlockAssembler(Params()).CreateNewBlock(scriptDummy);
        if (!pblocktemplate)
            throw JSONRPCError(RPC_OUT_OF_MEMORY, "Out of memory");

//只有在知道CreateNewBlock成功后才需要更新
        pindexPrev = pindexPrevNew;
    }
    assert(pindexPrev);
CBlock* pblock = &pblocktemplate->block; //方便的指针
    const Consensus::Params& consensusParams = Params().GetConsensus();

//更新NTIME
    UpdateTime(pblock, consensusParams, pindexPrev);
    pblock->nNonce = 0;

//注：如果在某一点上我们支持Segwit激活后的Segwit前矿工，则需要考虑Segwit支持。
    const bool fPreSegWit = (ThresholdState::ACTIVE != VersionBitsState(pindexPrev, consensusParams, Consensus::DEPLOYMENT_SEGWIT, versionbitscache));

    UniValue aCaps(UniValue::VARR); aCaps.push_back("proposal");

    UniValue transactions(UniValue::VARR);
    std::map<uint256, int64_t> setTxIndex;
    int i = 0;
    for (const auto& it : pblock->vtx) {
        const CTransaction& tx = *it;
        uint256 txHash = tx.GetHash();
        setTxIndex[txHash] = i++;

        if (tx.IsCoinBase())
            continue;

        UniValue entry(UniValue::VOBJ);

        entry.pushKV("data", EncodeHexTx(tx));
        entry.pushKV("txid", txHash.GetHex());
        entry.pushKV("hash", tx.GetWitnessHash().GetHex());

        UniValue deps(UniValue::VARR);
        for (const CTxIn &in : tx.vin)
        {
            if (setTxIndex.count(in.prevout.hash))
                deps.push_back(setTxIndex[in.prevout.hash]);
        }
        entry.pushKV("depends", deps);

        int index_in_template = i - 1;
        entry.pushKV("fee", pblocktemplate->vTxFees[index_in_template]);
        int64_t nTxSigOps = pblocktemplate->vTxSigOpsCost[index_in_template];
        if (fPreSegWit) {
            assert(nTxSigOps % WITNESS_SCALE_FACTOR == 0);
            nTxSigOps /= WITNESS_SCALE_FACTOR;
        }
        entry.pushKV("sigops", nTxSigOps);
        entry.pushKV("weight", GetTransactionWeight(tx));

        transactions.push_back(entry);
    }

    UniValue aux(UniValue::VOBJ);
    aux.pushKV("flags", HexStr(COINBASE_FLAGS.begin(), COINBASE_FLAGS.end()));

    arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);

    UniValue aMutable(UniValue::VARR);
    aMutable.push_back("time");
    aMutable.push_back("transactions");
    aMutable.push_back("prevblock");

    UniValue result(UniValue::VOBJ);
    result.pushKV("capabilities", aCaps);

    UniValue aRules(UniValue::VARR);
    UniValue vbavailable(UniValue::VOBJ);
    for (int j = 0; j < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++j) {
        Consensus::DeploymentPos pos = Consensus::DeploymentPos(j);
        ThresholdState state = VersionBitsState(pindexPrev, consensusParams, pos, versionbitscache);
        switch (state) {
            case ThresholdState::DEFINED:
            case ThresholdState::FAILED:
//完全不接触GBT
                break;
            case ThresholdState::LOCKED_IN:
//确保位在块版本中设置
                pblock->nVersion |= VersionBitsMask(consensusParams, pos);
//通过获取vbavailable集…
            case ThresholdState::STARTED:
            {
                const struct VBDeploymentInfo& vbinfo = VersionBitsDeploymentInfo[pos];
                vbavailable.pushKV(gbt_vb_name(pos), consensusParams.vDeployments[pos].bit);
                if (setClientRules.find(vbinfo.name) == setClientRules.end()) {
                    if (!vbinfo.gbt_force) {
//如果客户端不支持此功能，请不要在[默认]版本中指明它。
                        pblock->nVersion &= ~VersionBitsMask(consensusParams, pos);
                    }
                }
                break;
            }
            case ThresholdState::ACTIVE:
            {
//仅添加到规则
                const struct VBDeploymentInfo& vbinfo = VersionBitsDeploymentInfo[pos];
                aRules.push_back(gbt_vb_name(pos));
                if (setClientRules.find(vbinfo.name) == setClientRules.end()) {
//客户端不支持；请确保继续操作安全
                    if (!vbinfo.gbt_force) {
//如果我们在这里做了除抛出异常以外的任何事情，请确保不会将版本/强制发送到旧客户端
                        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Support for '%s' rule requires explicit client support", vbinfo.name));
                    }
                }
                break;
            }
        }
    }
    result.pushKV("version", pblock->nVersion);
    result.pushKV("rules", aRules);
    result.pushKV("vbavailable", vbavailable);
    result.pushKV("vbrequired", int(0));

    if (nMaxVersionPreVB >= 2) {
//如果客户机支持vb，那么nmaxversionprevb是-1，所以我们不会得到这里。
//因为bip 34改变了生成事务的序列化方式，所以我们只能使用version/force返回到v2块
//这是安全的[否则-]无条件的，因为如果非强制部署被激活，我们会抛出上面的异常
//请注意，在第一个bip9非强制部署（例如，可能是segwit）被激活后，这可能也可以完全删除。
        aMutable.push_back("version/force");
    }

    result.pushKV("previousblockhash", pblock->hashPrevBlock.GetHex());
    result.pushKV("transactions", transactions);
    result.pushKV("coinbaseaux", aux);
    result.pushKV("coinbasevalue", (int64_t)pblock->vtx[0]->vout[0].nValue);
    result.pushKV("longpollid", chainActive.Tip()->GetBlockHash().GetHex() + i64tostr(nTransactionsUpdatedLast));
    result.pushKV("target", hashTarget.GetHex());
    result.pushKV("mintime", (int64_t)pindexPrev->GetMedianTimePast()+1);
    result.pushKV("mutable", aMutable);
    result.pushKV("noncerange", "00000000ffffffff");
    int64_t nSigOpLimit = MAX_BLOCK_SIGOPS_COST;
    int64_t nSizeLimit = MAX_BLOCK_SERIALIZED_SIZE;
    if (fPreSegWit) {
        assert(nSigOpLimit % WITNESS_SCALE_FACTOR == 0);
        nSigOpLimit /= WITNESS_SCALE_FACTOR;
        assert(nSizeLimit % WITNESS_SCALE_FACTOR == 0);
        nSizeLimit /= WITNESS_SCALE_FACTOR;
    }
    result.pushKV("sigoplimit", nSigOpLimit);
    result.pushKV("sizelimit", nSizeLimit);
    if (!fPreSegWit) {
        result.pushKV("weightlimit", (int64_t)MAX_BLOCK_WEIGHT);
    }
    result.pushKV("curtime", pblock->GetBlockTime());
    result.pushKV("bits", strprintf("%08x", pblock->nBits));
    result.pushKV("height", (int64_t)(pindexPrev->nHeight+1));

    if (!pblocktemplate->vchCoinbaseCommitment.empty()) {
        result.pushKV("default_witness_commitment", HexStr(pblocktemplate->vchCoinbaseCommitment.begin(), pblocktemplate->vchCoinbaseCommitment.end()));
    }

    return result;
}

class submitblock_StateCatcher : public CValidationInterface
{
public:
    uint256 hash;
    bool found;
    CValidationState state;

    explicit submitblock_StateCatcher(const uint256 &hashIn) : hash(hashIn), found(false), state() {}

protected:
    void BlockChecked(const CBlock& block, const CValidationState& stateIn) override {
        if (block.GetHash() != hash)
            return;
        found = true;
        state = stateIn;
    }
};

static UniValue submitblock(const JSONRPCRequest& request)
{
//我们允许2个符合BIP22的论点。参数2被忽略。
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"submitblock",
                "\nAttempts to submit new block to network.\n"
"See https://完整规格见en.bitcoin.it/wiki/bip0022。\n“，
                {
                    /*exdata“，rpcarg:：type:：str_hex，/*opt*/false，/*默认值_val*/”，“要提交的十六进制编码块数据”，
                    “dummy”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "ignored", "dummy value, for compatibility with BIP22. This value is ignored."},

                }}
                .ToString() +
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("submitblock", "\"mydata\"")
            + HelpExampleRpc("submitblock", "\"mydata\"")
        );
    }

    std::shared_ptr<CBlock> blockptr = std::make_shared<CBlock>();
    CBlock& block = *blockptr;
    if (!DecodeHexBlk(block, request.params[0].get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block decode failed");
    }

    if (block.vtx.empty() || !block.vtx[0]->IsCoinBase()) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block does not start with a coinbase");
    }

    uint256 hash = block.GetHash();
    {
        LOCK(cs_main);
        const CBlockIndex* pindex = LookupBlockIndex(hash);
        if (pindex) {
            if (pindex->IsValid(BLOCK_VALID_SCRIPTS)) {
                return "duplicate";
            }
            if (pindex->nStatus & BLOCK_FAILED_MASK) {
                return "duplicate-invalid";
            }
        }
    }

    {
        LOCK(cs_main);
        const CBlockIndex* pindex = LookupBlockIndex(block.hashPrevBlock);
        if (pindex) {
            UpdateUncommittedBlockStructures(block, pindex, Params().GetConsensus());
        }
    }

    bool new_block;
    submitblock_StateCatcher sc(block.GetHash());
    RegisterValidationInterface(&sc);
    /*L accepted=processNewBlock（params（），blockptr，/*fforceProcessing*/true，/*fnewBlock*/&new_block）；
    注销Interface（&sc）；
    如果（！）新的块和接受的）
        返回“副本”；
    }
    如果（！）SC.发现）{
        返回“无结论”；
    }
    返回bip22验证结果（sc.state）；
}

静态单值提交者（const-jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 1）{
        throw std:：runtime_错误（
            rpchelpman“提交人”，
                \n将给定的hexdata定义为头，并将其作为候选链提示提交（如果有效）。
                \n头无效时的第n行。\n“，
                {
                    “hexdata”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "the hex-encoded block header data"},

                }}
                .ToString() +
            "\nResult:\n"
            "None"
            "\nExamples:\n" +
            HelpExampleCli("submitheader", "\"aabbcc\"") +
            HelpExampleRpc("submitheader", "\"aabbcc\""));
    }

    CBlockHeader h;
    if (!DecodeHexBlockHeader(h, request.params[0].get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block header decode failed");
    }
    {
        LOCK(cs_main);
        if (!LookupBlockIndex(h.hashPrevBlock)) {
            throw JSONRPCError(RPC_VERIFY_ERROR, "Must submit previous header (" + h.hashPrevBlock.GetHex() + ") first");
        }
    }

    CValidationState state;
    /*cessNewBlockHeaders（h，state，params（），/*ppindex*/nullptr，/*第一个无效*/nullptr）；
    if（state.isvalid（））返回nullunivalue；
    if（state.isError（））
        throw jsonrpcerror（rpc_verify_error，formatStateMessage（state））；
    }
    throw jsonrpcerror（rpc_verify_error，state.getRejectReason（））；
}

静态单值估计artfee（const jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）<1 request.params.size（）>2）
        throw std:：runtime_错误（
            rpchelpman“估算艺术品费”，
                \n估计事务开始所需的每千字节大约费用\n
                “如果可能，在conf_目标块内确认并返回块数\n”
                “评估对其有效。使用定义的虚拟事务大小\n“
                “在BIP 141中（证人数据打折）。\n”，
                {
                    “conf_target”，rpcarg:：type：：num，/*opt*/ false, /* default_val */ "", "Confirmation target in blocks (1 - 1008)"},

                    /*stimate_模式“，rpcarg:：type:：str，/*opt*/true，/*default_val*/”conservative“，”费用估算模式。\n”
            “是否返回更保守的估计值，该估计值也满足\n”
            “更悠久的历史。保守估计可能会返回一个\n“
            “频率越高，就越可能满足所需的要求\n”
            目标，但对短期内
            “现行收费市场。必须是下列之一：
            “取消设置\ \ \n”
            “经济型\ \ \ \n”
            “保守派”
                }
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            “fee rate”：x.x，（数字，可选）估计费率，单位为”+货币单位+“/kb\n”
            “\”错误\“：[字符串…]（json字符串数组，可选）处理过程中遇到错误\n“
            “\”块\“：n（数字）找到估计的块编号\n”
            “}\n”
            “\n”
            “请求目标将夹在2和最高目标之间\n”
            “费用估计可以根据运行时间返回。\n”
            “如果没有足够的事务和块，则返回错误\n”
            “已观察到对任意数量的块进行估计。\n”
            “\n实例：\n”
            +helpExamplecli（“EstimateMartfee”，“6”）。
            ；

    rpctypecheck（request.params，univalue:：vnum，univalue:：vstr）；
    rpctypecheckArgument（request.params[0]，univalue:：vnum）；
    unsigned int conf_target=parseConfirmTarget（request.params[0]）；
    布尔保守=真；
    如果（！）request.params[1].isNull（））
        feeestimatemode费用模式；
        如果（！）feemodefromString（request.params[1].get_str（），fee_mode））
            throw jsonrpcerror（rpc_invalid_参数，“invalid estimate_mode参数”）；
        }
        如果（fee_mode==feeestimatemode:：economical）conservative=false；
    }

    单值结果（单值：：vobj）；
    单值错误（单值：：varr）；
    feecalculation feecalc；
    cfeerate feerate=：feeestimator.estimatesmartfee（conf_target，&feecalc，conservative）；
    如果（狂热）！=cfeerate（0））
        result.pushkv（“feerate”，valuefromamount（feerate.getfeeperk（））；
    }否则{
        错误。推回（“数据不足或找不到反馈”）；
        结果.pushkv（“错误”，错误）；
    }
    结果.pushkv（“块”，feecalc.returnedtarget）；
    返回结果；
}

静态单值估计量rawfee（const jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）<1 request.params.size（）>2）
        throw std:：runtime_错误（
            rpchelpman“估计值”，
                警告：此接口不稳定，可能会消失或更改！“N”
                \n警告：这是一个高级API调用，与特定的
                “费用估算的实施。可以用它调用的参数\n“
                “如果内部实现更改，则返回的结果将更改。\n”
                \n估计事务开始所需的每千字节大约费用\n
                “如果可能，在conf_目标块内确认。使用虚拟事务大小作为\n“
                “在BIP 141中定义（证人数据折扣）。\n”，
                {
                    “conf_target”，rpcarg:：type：：num，/*opt*/ false, /* default_val */ "", "Confirmation target in blocks (1 - 1008)"},

                    /*hreshold“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”0.95“，给定时间范围内的事务比例，必须是\n”
            “在conf-tu目标内确认，以便将这些feerate视为足够高并继续检查\n”
            “下桶。”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            “短\”：（json对象，可选）短时间范围的估计值\n”
            “fee rate”：x.x，（数字，可选）估计费率，单位为”+货币单位+“/kb\n”
            ““衰减”：x.x，（数值）确认数据的历史移动平均值的指数衰减（每个块）。\n”
            ““缩放”：x，（数字）此时间范围内确认目标的分辨率\n”
            “通过”：（json对象，可选）有关成功满足阈值的最低频率范围的信息\n”
            “start range\”：x.x，（数字）起始范围\n”
            “结束范围\”：x.x，（数字）结束范围\n”
            ““WithinTarget\”：x.x，（数字）在目标范围内确认的时间范围内历史范围内的Tx数\n”
            “totalconfirmed\”：x.x，（数字）在任何点上确认的时间范围内历史范围内的Tx数\n”
            “in mempool\”：x.x，（数字）在至少未确认目标块的有效范围内，mempool中的当前tx数目\n”
            ““left mempool\”：x.x，（数字）在目标后未确认的有限范围内历史范围内的Tx数\n”
            “}，\n”
            “\”失败\“：”…，（json对象，可选）有关无法满足阈值的最高频率范围的信息\n“
            “\”错误\“：[字符串…]（json字符串数组，可选）处理过程中遇到错误\n“
            “}，\n”
            “\”中\“：”…，（json对象，可选）中期时间范围的估计值\n“
            “长”：…（JSON对象）长时间范围的估计值\n“
            “}\n”
            “\n”
            “跟踪到确认目标的块的任何范围都会返回结果。\n”
            “\n实例：\n”
            +helpexamplecli（“Estimaterawfee”，“6 0.9”）。
            ；

    rpctypecheck（request.params，univalue:：vnum，univalue:：vnum，true）；
    rpctypecheckArgument（request.params[0]，univalue:：vnum）；
    unsigned int conf_target=parseConfirmTarget（request.params[0]）；
    双阈值=0.95；
    如果（！）request.params[1].isNull（））
        threshold=request.params[1].get_real（）；
    }
    如果（阈值<0阈值>1）
        throw jsonrpcerror（rpc_invalid_参数，“invalid threshold”）；
    }

    单值结果（单值：：vobj）；

    for（const feestimatehorizon地平线：feestimatehorizon:：short_hallife，feestimatehorizon:：med_hallife，feestimatehorizon:：long_hallife）
        酸盐温度；
        估计结果存储桶；

        //仅输出跟踪目标的层位的结果
        如果（conf_target>：feeestimator.highestTargetTracked（Horizon））继续；

        feerate=：：feeestimator.estimaterawfee（conf_target、threshold、horizon和buckets）；
        单值视界结果（单值：：vobj）；
        单值错误（单值：：varr）；
        单值passbucket（单值：：vobj）；
        passbucket.pushkv（“开始范围”，圆形（bucket.pass.start））；
        passbucket.pushkv（“endrange”，圆形（bucket.pass.end））；
        passbucket.pushkv（“WithinTarget”，圆形（bucket.pass.WithinTarget*100.0）/100.0）；
        passbucket.pushkv（“totalconfirmed”，圆形（bucket.pass.totalconfirmed*100.0）/100.0）；
        passbucket.pushkv（“inmempool”，圆形（bucket.pass.inmempool*100.0）/100.0）；
        passbucket.pushkv（“leftmempool”，圆形（buckets.pass.leftmempool*100.0）/100.0）；
        单值故障桶（单值：：vobj）；
        failbucket.pushkv（“startrange”，圆形（bucket.fail.start））；
        failbucket.pushkv（“endrange”，圆形（bucket.fail.end））；
        failbucket.pushkv（“withInTarget”，圆形（bucket.fail.withInTarget*100.0）/100.0）；
        failbucket.pushkv（“totalconfirmed”，圆形（bucket.fail.totalconfirmed*100.0）/100.0）；
        failbucket.pushkv（“inmempool”，圆形（bucket.fail.inmempool*100.0）/100.0）；
        failbucket.pushkv（“leftmempool”，圆形（bucket.fail.leftmempool*100.0）/100.0）；

        //cfeerate（0）用于将错误指示为EstimaterawFee的返回值
        如果（狂热）！=cfeerate（0））
            Horizon？result.pushkv（“feerate”，valuefromamount（feerate.getfeeperk（））；
            水平线结果pushkv（“衰减”，buckets.decay）；
            Horizon_result.pushkv（“scale”，（int）buckets.scale）；
            水平线结果pushkv（“通过”，passbucket）；
            //bucket.fail.start==-1表示所有bucket都通过，没有fail bucket输出
            如果（buckets.fail.start！=-1）地平线_result.pushkv（“失败”，failbucket）；
        }否则{
            //仅输出出错时仍有意义的信息
            水平线结果pushkv（“衰减”，buckets.decay）；
            Horizon_result.pushkv（“scale”，（int）buckets.scale）；
            Horizon_result.pushkv（“失败”，failbucket）；
            错误。推回（“数据不足或找不到符合阈值的数据”）；
            Horizon_result.pushkv（“错误”，错误）；
        }
        结果.pushkv（爆炸地平（地平线），地平结果）；
    }
    返回结果；
}

//关闭clang格式
静态const crpccommand命令[]
//类别名称actor（function）argnames
  ///————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    “mining”，“getnetworkhashps”，&getnetworkhashps，“nblocks”，“height”，
    “采矿”，“GetMiningInfo”，&GetMiningInfo，，
    “采矿”，“优先权转让”，&prioritivestraction，“TXID”，“虚拟”，“费用”
    “挖掘”，“GetBlockTemplate”，&GetBlockTemplate，“模板请求”，
    “挖掘”、“SubmitBlock”、&SubmitBlock、“HexData”、“虚拟”，
    “挖掘”，“提交者”，提交者，“HexData”，


    “生成”，“生成地址”，&generatetoaddress，“nblocks”，“地址”，“maxtries”，

    “util”，“Estimatemartfee”，&Estimatemartfee，“conf_target”，“Estimate_mode”，

    “hidden”，“estimatorawfee”，&estimatorawfee，“conf_target”，“threshold”，
}；
//打开Clang格式

空寄存器miningrpccommands（crpctable&t）
{
    for（unsigned int vcidx=0；vcidx<arraylen（commands）；vcidx++）
        t.appendcommand（命令[vcidx].name，&commands[vcidx]）；
}
