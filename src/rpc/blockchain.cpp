
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

#include <rpc/blockchain.h>

#include <amount.h>
#include <base58.h>
#include <chain.h>
#include <chainparams.h>
#include <checkpoints.h>
#include <coins.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <hash.h>
#include <index/txindex.h>
#include <key_io.h>
#include <policy/feerate.h>
#include <policy/policy.h>
#include <policy/rbf.h>
#include <primitives/transaction.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <script/descriptor.h>
#include <streams.h>
#include <sync.h>
#include <txdb.h>
#include <txmempool.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <validation.h>
#include <validationinterface.h>
#include <versionbitsinfo.h>
#include <warnings.h>

#include <assert.h>
#include <stdint.h>

#include <univalue.h>

#include <boost/thread/thread.hpp> //boost：：线程：：中断

#include <memory>
#include <mutex>
#include <condition_variable>

struct CUpdatedBlock
{
    uint256 hash;
    int height;
};

static Mutex cs_blockchange;
static std::condition_variable cond_blockchange;
static CUpdatedBlock latestblock;

/*计算给定块索引的难度。
 **/

double GetDifficulty(const CBlockIndex* blockindex)
{
    assert(blockindex);

    int nShift = (blockindex->nBits >> 24) & 0xff;
    double dDiff =
        (double)0x0000ffff / (double)(blockindex->nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

static int ComputeNextBlockAndDepth(const CBlockIndex* tip, const CBlockIndex* blockindex, const CBlockIndex*& next)
{
    next = tip->GetAncestor(blockindex->nHeight + 1);
    if (next && next->pprev == blockindex) {
        return tip->nHeight - blockindex->nHeight + 1;
    }
    next = nullptr;
    return blockindex == tip ? 1 : -1;
}

UniValue blockheaderToJSON(const CBlockIndex* tip, const CBlockIndex* blockindex)
{
    UniValue result(UniValue::VOBJ);
    result.pushKV("hash", blockindex->GetBlockHash().GetHex());
    const CBlockIndex* pnext;
    int confirmations = ComputeNextBlockAndDepth(tip, blockindex, pnext);
    result.pushKV("confirmations", confirmations);
    result.pushKV("height", blockindex->nHeight);
    result.pushKV("version", blockindex->nVersion);
    result.pushKV("versionHex", strprintf("%08x", blockindex->nVersion));
    result.pushKV("merkleroot", blockindex->hashMerkleRoot.GetHex());
    result.pushKV("time", (int64_t)blockindex->nTime);
    result.pushKV("mediantime", (int64_t)blockindex->GetMedianTimePast());
    result.pushKV("nonce", (uint64_t)blockindex->nNonce);
    result.pushKV("bits", strprintf("%08x", blockindex->nBits));
    result.pushKV("difficulty", GetDifficulty(blockindex));
    result.pushKV("chainwork", blockindex->nChainWork.GetHex());
    result.pushKV("nTx", (uint64_t)blockindex->nTx);

    if (blockindex->pprev)
        result.pushKV("previousblockhash", blockindex->pprev->GetBlockHash().GetHex());
    if (pnext)
        result.pushKV("nextblockhash", pnext->GetBlockHash().GetHex());
    return result;
}

UniValue blockToJSON(const CBlock& block, const CBlockIndex* tip, const CBlockIndex* blockindex, bool txDetails)
{
    UniValue result(UniValue::VOBJ);
    result.pushKV("hash", blockindex->GetBlockHash().GetHex());
    const CBlockIndex* pnext;
    int confirmations = ComputeNextBlockAndDepth(tip, blockindex, pnext);
    result.pushKV("confirmations", confirmations);
    result.pushKV("strippedsize", (int)::GetSerializeSize(block, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS));
    result.pushKV("size", (int)::GetSerializeSize(block, PROTOCOL_VERSION));
    result.pushKV("weight", (int)::GetBlockWeight(block));
    result.pushKV("height", blockindex->nHeight);
    result.pushKV("version", block.nVersion);
    result.pushKV("versionHex", strprintf("%08x", block.nVersion));
    result.pushKV("merkleroot", block.hashMerkleRoot.GetHex());
    UniValue txs(UniValue::VARR);
    for(const auto& tx : block.vtx)
    {
        if(txDetails)
        {
            UniValue objTx(UniValue::VOBJ);
            TxToUniv(*tx, uint256(), objTx, true, RPCSerializationFlags());
            txs.push_back(objTx);
        }
        else
            txs.push_back(tx->GetHash().GetHex());
    }
    result.pushKV("tx", txs);
    result.pushKV("time", block.GetBlockTime());
    result.pushKV("mediantime", (int64_t)blockindex->GetMedianTimePast());
    result.pushKV("nonce", (uint64_t)block.nNonce);
    result.pushKV("bits", strprintf("%08x", block.nBits));
    result.pushKV("difficulty", GetDifficulty(blockindex));
    result.pushKV("chainwork", blockindex->nChainWork.GetHex());
    result.pushKV("nTx", (uint64_t)blockindex->nTx);

    if (blockindex->pprev)
        result.pushKV("previousblockhash", blockindex->pprev->GetBlockHash().GetHex());
    if (pnext)
        result.pushKV("nextblockhash", pnext->GetBlockHash().GetHex());
    return result;
}

static UniValue getblockcount(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            RPCHelpMan{"getblockcount",
                "\nReturns the number of blocks in the longest blockchain.\n", {}}
                .ToString() +
            "\nResult:\n"
            "n    (numeric) The current block count\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockcount", "")
            + HelpExampleRpc("getblockcount", "")
        );

    LOCK(cs_main);
    return chainActive.Height();
}

static UniValue getbestblockhash(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            RPCHelpMan{"getbestblockhash",
                "\nReturns the hash of the best (tip) block in the longest blockchain.\n", {}}
                .ToString() +
            "\nResult:\n"
            "\"hex\"      (string) the block hash, hex-encoded\n"
            "\nExamples:\n"
            + HelpExampleCli("getbestblockhash", "")
            + HelpExampleRpc("getbestblockhash", "")
        );

    LOCK(cs_main);
    return chainActive.Tip()->GetBlockHash().GetHex();
}

void RPCNotifyBlockChange(bool ibd, const CBlockIndex * pindex)
{
    if(pindex) {
        std::lock_guard<std::mutex> lock(cs_blockchange);
        latestblock.hash = pindex->GetBlockHash();
        latestblock.height = pindex->nHeight;
    }
    cond_blockchange.notify_all();
}

static UniValue waitfornewblock(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            RPCHelpMan{"waitfornewblock",
                "\nWaits for a specific new block and returns useful info about it.\n"
                "\nReturns the current block on timeout or exit.\n",
                {
                    /*超时“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”0“，”等待响应的时间（毫秒）。0表示没有超时。“，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “（JSON对象\n”
            “\”哈希\“：（字符串）块哈希\n”
            “高度\”：（int）块高度\n”
            “}\n”
            “\n实例：\n”
            +helpExamplecli（“waitfornewblock”，“1000”）。
            +HelpExampleRPC（“WaitForNewBlock”，“1000”）。
        ；
    int超时=0；
    如果（！）请求.params[0].isNull（））
        timeout=request.params[0].get_int（）；

    CupdatedBlock块；
    {
        等待锁定（cs-blockchange，lock）；
        block=最新block；
        如果（超时）
            cond_blockchange.wait_for（lock，std:：chrono:：millises（timeout），[&block]返回latestblock.height！=block.height latestblock.hash！=block.hash！isrpcrunning（）；））；
        其他的
            Cond_BlockChange.wait（锁定，[&block]返回latestblock.height！=block.height latestblock.hash！=block.hash！isrpcrunning（）；））；
        block=最新block；
    }
    单值ret（单值：：vobj）；
    ret.pushkv（“hash”，block.hash.gethex（））；
    ret.pushkv（“高度”，块高度）；
    返回RET；
}

静态单值WaitForBlock（const jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）<1 request.params.size（）>2）
        throw std:：runtime_错误（
            rpchelpman“等待阻止”，
                \n等待特定的新块并返回有关它的有用信息。\n
                \n在超时或退出时返回当前块。\n“，
                {
                    “blockhash”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "Block hash to wait for."},

                    /*超时“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”0“，”等待响应的时间（毫秒）。0表示没有超时。“，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “（JSON对象\n”
            “\”哈希\“：（字符串）块哈希\n”
            “高度\”：（int）块高度\n”
            “}\n”
            “\n实例：\n”
            +helpExamplecli（“waitForBlock”，“000000000079f8ef3d2c688c244eb7a4570b24c9ed7b4a8c619eb02596f8862”，“1000”）。
            +帮助示例rpc（“WaitForBlock”，“000000000079F8EF3D2C688C2444EB7A4570B24C9ED7B4A8C619EB02596F8862”，“1000”）。
        ；
    int超时=0；

    uint256散列（parsehashv（request.params[0]，“blockhash”））；

    如果（！）请求.params[1].isNull（））
        timeout=request.params[1].get_int（）；

    CupdatedBlock块；
    {
        等待锁定（cs-blockchange，lock）；
        如果（超时）
            cond_blockchange.wait_for（lock，std:：chrono:：millises（timeout），[&hash]返回latestblock.hash==hash！isrpcrunning（）；））；
        其他的
            cond_blockchange.wait（lock，[&hash]返回latestblock.hash==hash！isrpcrunning（）；））；
        block=最新block；
    }

    单值ret（单值：：vobj）；
    ret.pushkv（“hash”，block.hash.gethex（））；
    ret.pushkv（“高度”，块高度）；
    返回RET；
}

静态单值WaitForBlockHeight（const jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）<1 request.params.size（）>2）
        throw std:：runtime_错误（
            rpchelpman“等待锁高度”，
                \n等待（至少）块高度并返回高度和哈希值\n
                “当前提示的内容。\n”
                \n在超时或退出时返回当前块。\n“，
                {
                    “高度”，rpcarg:：type:：num，/*opt*/ false, /* default_val */ "", "Block height to wait for."},

                    /*超时“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”0“，”等待响应的时间（毫秒）。0表示没有超时。“，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “（JSON对象\n”
            “\”哈希\“：（字符串）块哈希\n”
            “高度\”：（int）块高度\n”
            “}\n”
            “\n实例：\n”
            +helpexamplecli（“waitForBlockHeight”，“100”，“1000”）。
            +helpExampleRPC（“waitForBlockHeight”，“100”，“1000”）。
        ；
    int超时=0；

    int height=request.params[0].get_int（）；

    如果（！）请求.params[1].isNull（））
        timeout=request.params[1].get_int（）；

    CupdatedBlock块；
    {
        等待锁定（cs-blockchange，lock）；
        如果（超时）
            cond_blockchange.wait_for（lock，std:：chrono:：millises（timeout），[&height]return latestblock.height>=height！isrpcrunning（）；））；
        其他的
            cond_blockchange.wait（lock，[&height]返回latestblock.height>=height！isrpcrunning（）；））；
        block=最新block；
    }
    单值ret（单值：：vobj）；
    ret.pushkv（“hash”，block.hash.gethex（））；
    ret.pushkv（“高度”，块高度）；
    返回RET；
}

静态单值同步与validationInterfaceQueue（const jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）>0）
        throw std:：runtime_错误（
            rpchelpman“与validationInterfaceQueue同步”，
                “\n等待验证接口队列在我们进入此函数时赶上那里的所有内容。\n”，
                toSTRIN（）+
            “\n实例：\n”
            +helpExamplecli（“与validationInterfaceQueue同步”，“”）
            +helpExampleRpc（“与validationInterfaceQueue同步”，“”）
        ；
    }
    与validationInterfaceQueue（）同步；
    返回nullunivalue；
}

静态单值获取难度（const jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 0）
        throw std:：runtime_错误（
            rpchelpman“获取难度”，
                \n将工作难度的证明作为最小难度的倍数返回。\n“”
                toSTRIN（）+
            “\NESRES:\N”
            “N.n n n（数字）作为最小难度的倍数的工作难度证明。\n”
            “\n实例：\n”
            +helpExamplecli（“GetDifficulty”，“”）
            +helpExampleRpc（“获取难度”，“）
        ；

    锁（CSKEMAN）；
    返回getDifficulty（chainActive.tip（））；
}

静态Std:：String EntryDescriptionString（）
{
    返回“大小”：n，（数字）BIP 141中定义的虚拟事务大小。这与证人事务的实际序列化大小不同，因为证人数据已折扣。\n“
           “费用\”：n，（数字）以“+货币\单位+”表示的交易费用（已弃用）\n”
           “\”ModifiedFee\“：n，（数字）用于挖掘优先级的费用增量交易费（已弃用）\n”
           ““时间”：n，（数字）本地时间事务自1970年1月1日格林威治标准时间以来以秒为单位输入池\n”
           “高度”：n，（数字）当事务输入池时块高度\n”
           ““DescendantCount”：n，（数字）内存池中的后代事务数（包括此事务）\n”
           ““子体大小”：n，（数字）内存池子体（包括此子体）的虚拟事务大小\n”
           “子代费用\”：n，（数字）修改了mempool子代（包括此子代）的费用（见上文）（已弃用）\n”
           “AncestorCount”：n，（数字）内存池中的祖先事务数（包括此事务）\n”
           “AncestorSize”：n，（数字）内存池中祖先的虚拟事务大小（包括此事务大小）\n”
           “Ancestorfees”：n，（数字）修改了mempool祖先（包括此祖先）的费用（见上文）（已弃用）\n”
           ““wtxid”：哈希，（字符串）序列化事务的哈希，包括见证数据\n”
           “费用\”：\n”
           “基数”：n，（数字）以“+货币\单位+”表示的交易费\n”
           “\”已修改\“：n，（数字）以“+货币\单位+”表示的用于挖掘优先级的费用增量的交易费用\n”
           “祖先”：n，（数字）以“+货币\单位+”表示的mempool祖先（包括此祖先）的修改费用（见上文）。\n”
           “\”子体\“：n，（数字）以“+货币\单位+”修改了mempool子体（包括此子体）的费用（见上文）。\n”
           “}\n”
           “依赖于”：[（数组）未确认的事务，用作此事务的输入\n”
           “\”transaction id\“，（string）父事务id\n”
           “……”\n
           “\”Spentby\“：[（数组）未确认的事务支出此事务的输出\n”
           “transaction id\”，（string）子事务id \n”
           “……”\n
           “\”bip125 replacement\”：true false，（布尔值）由于bip125（替换为费用）是否可以替换此事务\n”；
}

静态void entryTojson（univalue&info，const ctxmempoolEntry&e）需要唯一的锁（：：mempool.cs）
{
    断言锁定（mempool.cs）；

    单值费用（单值：：VOBJ）；
    fees.pushkv（“基础”，valuefromamount（e.getfee（））；
    fees.pushkv（“已修改”，valuefromamount（e.getModifiedFee（））；
    fees.pushkv（“祖先”，valuefromamount（e.getmodfeeswith祖先（））；
    fees.pushkv（“后代”，valueFromAmount（e.GetModFeesWithDescendants（））；
    信息：pushkv（“费用”，费用）；

    info.pushkv（“大小”，（int）e.gettxsize（））；
    info.pushkv（“费用”，valuefromamount（e.getfee（））；
    info.pushkv（“modifiedfee”，valuefromamount（e.getmodifiedfee（））；
    info.pushkv（“时间”，e.gettime（））；
    info.pushkv（“高度”，（int）e.getheight（））；
    info.pushkv（“DescendantCount”，e.getCountWithDescendants（））；
    info.pushkv（“DescendantSize”，e.getSizeWithDescendants（））；
    info.pushkv（“DescendantFees”，e.getModFeesWithDescendants（））；
    info.pushkv（“AncestorCount”，e.getCountWithOrights（））；
    info.pushkv（“ancestorsize”，e.getsizewith祖先（））；
    info.pushkv（“ancestorfees”，e.getmodfeeswith祖先（））；
    info.pushkv（“wtxid”，mempool.vtxhash[e.vtxhashesidx].first.toString（））；
    const ctransaction&tx=e.gettx（）；
    std:：set<std:：string>setdepends；
    用于（const ctxin和txin:tx.vin）
    {
        if（mempool.exists（txin.prevout.hash））。
            setDepends.insert（txin.prevout.hash.toString（））；
    }

    单值依赖（单值：：varr）；
    for（const std:：string&dep:setdepends）
    {
        视情况而定。向后推（DEP）；
    }

    信息：pushkv（“取决于”，取决于）；

    花费的单值（单值：：varr）；
    const ctxmempool:：txter&it=mempool.maptx.find（tx.gethash（））；
    const ctxmempool:：setentries&setchildren=mempool.getmempoolchildren（it）；
    对于（ctxmempool:：txter childiter:setchildren）
        speed.push_back（childiter->gettx（）.gethash（）.toString（））；
    }

    信息：pushkv（“spentby”，已用）；

    //添加选择加入RBF状态
    bool rbfstatus=假；
    rbftTransactionState rbfstate=isrbfoptin（tx，mempool）；
    if（rbfstate==rbftTransactionState:：Unknown）
        throw jsonrpcerror（rpc_misc_error，“transaction is not in mempool”）；
    else if（rbfstate==rbfttransactionstate:：replacement_bip125）
        rbfstatus=真；
    }

    信息pushkv（“bip125可替换”，rbfstatus）；
}

单值mempooltokson（bool fverbose）
{
    如果（FVBBOSE）
    {
        锁（mempool.cs）；
        单值o（单值：：vobj）；
        用于（const ctxmempool entry&e:mempool.maptx）
        {
            const uint256&hash=e.gettx（）.gethash（）；
            单值信息（univalue:：vobj）；
            EntryTojson（信息，E）；
            o.pushkv（hash.toString（），信息）；
        }
        返回O；
    }
    其他的
    {
        std:：vector<uint256>vtxid；
        mempool.queryhashes（vtxid）；

        单值A（单值：：varr）；
        for（const uint256&hash:vtxid）
            a.向后推（hash.toString（））；

        返回A；
    }
}

静态单值getrawmupool（const-jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）>1）
        throw std:：runtime_错误（
            rpchelpman“getrawmempool”，
                \n将内存池中的所有事务ID作为字符串事务ID的JSON数组返回。\n
                \nhint:使用getmempoolentry从mempool中提取特定事务。\n“，
                {
                    “verbose”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "false", "True for a json object, false for array of transaction ids"},

                }}
                .ToString() +
            "\nResult: (for verbose = false):\n"
            "[                     (json array of string)\n"
            "  \"transactionid\"     (string) The transaction id\n"
            "  ,...\n"
            "]\n"
            "\nResult: (for verbose = true):\n"
            "{                           (json object)\n"
            "  \"transactionid\" : {       (json object)\n"
            + EntryDescriptionString()
            + "  }, ...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getrawmempool", "true")
            + HelpExampleRpc("getrawmempool", "true")
        );

    bool fVerbose = false;
    if (!request.params[0].isNull())
        fVerbose = request.params[0].get_bool();

    return mempoolToJSON(fVerbose);
}

static UniValue getmempoolancestors(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"getmempoolancestors",
                "\nIf txid is in the mempool, returns all in-mempool ancestors.\n",
                {
                    /*xid“，rpcarg:：type:：str_hex，/*opt*/false，/*default_val*/”，“事务ID（必须在mempool中）”，
                    “verbose”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "false", "True for a json object, false for array of transaction ids"},

                }}
                .ToString() +
            "\nResult (for verbose = false):\n"
            "[                       (json array of strings)\n"
            "  \"transactionid\"           (string) The transaction id of an in-mempool ancestor transaction\n"
            "  ,...\n"
            "]\n"
            "\nResult (for verbose = true):\n"
            "{                           (json object)\n"
            "  \"transactionid\" : {       (json object)\n"
            + EntryDescriptionString()
            + "  }, ...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmempoolancestors", "\"mytxid\"")
            + HelpExampleRpc("getmempoolancestors", "\"mytxid\"")
            );
    }

    bool fVerbose = false;
    if (!request.params[1].isNull())
        fVerbose = request.params[1].get_bool();

    uint256 hash = ParseHashV(request.params[0], "parameter 1");

    LOCK(mempool.cs);

    CTxMemPool::txiter it = mempool.mapTx.find(hash);
    if (it == mempool.mapTx.end()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction not in mempool");
    }

    CTxMemPool::setEntries setAncestors;
    uint64_t noLimit = std::numeric_limits<uint64_t>::max();
    std::string dummy;
    mempool.CalculateMemPoolAncestors(*it, setAncestors, noLimit, noLimit, noLimit, noLimit, dummy, false);

    if (!fVerbose) {
        UniValue o(UniValue::VARR);
        for (CTxMemPool::txiter ancestorIt : setAncestors) {
            o.push_back(ancestorIt->GetTx().GetHash().ToString());
        }

        return o;
    } else {
        UniValue o(UniValue::VOBJ);
        for (CTxMemPool::txiter ancestorIt : setAncestors) {
            const CTxMemPoolEntry &e = *ancestorIt;
            const uint256& _hash = e.GetTx().GetHash();
            UniValue info(UniValue::VOBJ);
            entryToJSON(info, e);
            o.pushKV(_hash.ToString(), info);
        }
        return o;
    }
}

static UniValue getmempooldescendants(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"getmempooldescendants",
                "\nIf txid is in the mempool, returns all in-mempool descendants.\n",
                {
                    /*xid“，rpcarg:：type:：str_hex，/*opt*/false，/*default_val*/”，“事务ID（必须在mempool中）”，
                    “verbose”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "false", "True for a json object, false for array of transaction ids"},

                }}
                .ToString() +
            "\nResult (for verbose = false):\n"
            "[                       (json array of strings)\n"
            "  \"transactionid\"           (string) The transaction id of an in-mempool descendant transaction\n"
            "  ,...\n"
            "]\n"
            "\nResult (for verbose = true):\n"
            "{                           (json object)\n"
            "  \"transactionid\" : {       (json object)\n"
            + EntryDescriptionString()
            + "  }, ...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmempooldescendants", "\"mytxid\"")
            + HelpExampleRpc("getmempooldescendants", "\"mytxid\"")
            );
    }

    bool fVerbose = false;
    if (!request.params[1].isNull())
        fVerbose = request.params[1].get_bool();

    uint256 hash = ParseHashV(request.params[0], "parameter 1");

    LOCK(mempool.cs);

    CTxMemPool::txiter it = mempool.mapTx.find(hash);
    if (it == mempool.mapTx.end()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction not in mempool");
    }

    CTxMemPool::setEntries setDescendants;
    mempool.CalculateDescendants(it, setDescendants);
//ctxmempool:：calculateDescendants将包括给定的Tx
    setDescendants.erase(it);

    if (!fVerbose) {
        UniValue o(UniValue::VARR);
        for (CTxMemPool::txiter descendantIt : setDescendants) {
            o.push_back(descendantIt->GetTx().GetHash().ToString());
        }

        return o;
    } else {
        UniValue o(UniValue::VOBJ);
        for (CTxMemPool::txiter descendantIt : setDescendants) {
            const CTxMemPoolEntry &e = *descendantIt;
            const uint256& _hash = e.GetTx().GetHash();
            UniValue info(UniValue::VOBJ);
            entryToJSON(info, e);
            o.pushKV(_hash.ToString(), info);
        }
        return o;
    }
}

static UniValue getmempoolentry(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"getmempoolentry",
                "\nReturns mempool data for given transaction\n",
                {
                    /*xid“，rpcarg:：type:：str_hex，/*opt*/false，/*default_val*/”，“事务ID（必须在mempool中）”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “（JSON对象\n”
            +EntryDescriptionString（））
            +“}\n”
            “\n实例：\n”
            +helpexamplecli（“getmempoolentry”，“mytxid\”）
            +helpExampleRpc（“getmempoolentry”，“MyTxID\”）
        ；
    }

    uint256 hash=parsehashv（request.params[0]，“参数1”）；

    锁（mempool.cs）；

    ctxmempool:：txter it=mempool.maptx.find（哈希）；
    if（it==mempool.maptx.end（））
        throw jsonrpcerror（rpc_invalid_address_or_key，“transaction not in mempool”）；
    }

    const ctxmempoolEntry&E=*IT；
    单值信息（univalue:：vobj）；
    EntryTojson（信息，E）；
    返回信息；
}

静态单值getblockhash（const jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 1）
        throw std:：runtime_错误（
            rpchelpman“getblockhash”，
                \n返回所提供高度的最佳区块链中区块的哈希值。\n“，
                {
                    “高度”，rpcarg:：type:：num，/*opt*/ false, /* default_val */ "", "The height index"},

                }}
                .ToString() +
            "\nResult:\n"
            "\"hash\"         (string) The block hash\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockhash", "1000")
            + HelpExampleRpc("getblockhash", "1000")
        );

    LOCK(cs_main);

    int nHeight = request.params[0].get_int();
    if (nHeight < 0 || nHeight > chainActive.Height())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");

    CBlockIndex* pblockindex = chainActive[nHeight];
    return pblockindex->GetBlockHash().GetHex();
}

static UniValue getblockheader(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            RPCHelpMan{"getblockheader",
                "\nIf verbose is false, returns a string that is serialized, hex-encoded data for blockheader 'hash'.\n"
                "If verbose is true, returns an Object with information about blockheader <hash>.\n",
                {
                    /*lockhash“，rpcarg:：type:：str_hex，/*opt*/false，/*默认值_val*/”“，“块哈希”，
                    “verbose”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "true", "true for a json object, false for the hex-encoded data"},

                }}
                .ToString() +
            "\nResult (for verbose = true):\n"
            "{\n"
            "  \"hash\" : \"hash\",     (string) the block hash (same as provided)\n"
            "  \"confirmations\" : n,   (numeric) The number of confirmations, or -1 if the block is not on the main chain\n"
            "  \"height\" : n,          (numeric) The block height or index\n"
            "  \"version\" : n,         (numeric) The block version\n"
            "  \"versionHex\" : \"00000000\", (string) The block version formatted in hexadecimal\n"
            "  \"merkleroot\" : \"xxxx\", (string) The merkle root\n"
            "  \"time\" : ttt,          (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"mediantime\" : ttt,    (numeric) The median block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"nonce\" : n,           (numeric) The nonce\n"
            "  \"bits\" : \"1d00ffff\", (string) The bits\n"
            "  \"difficulty\" : x.xxx,  (numeric) The difficulty\n"
            "  \"chainwork\" : \"0000...1f3\"     (string) Expected number of hashes required to produce the current chain (in hex)\n"
            "  \"nTx\" : n,             (numeric) The number of transactions in the block.\n"
            "  \"previousblockhash\" : \"hash\",  (string) The hash of the previous block\n"
            "  \"nextblockhash\" : \"hash\",      (string) The hash of the next block\n"
            "}\n"
            "\nResult (for verbose=false):\n"
            "\"data\"             (string) A string that is serialized, hex-encoded data for block 'hash'.\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockheader", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
            + HelpExampleRpc("getblockheader", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
        );

    uint256 hash(ParseHashV(request.params[0], "hash"));

    bool fVerbose = true;
    if (!request.params[1].isNull())
        fVerbose = request.params[1].get_bool();

    const CBlockIndex* pblockindex;
    const CBlockIndex* tip;
    {
        LOCK(cs_main);
        pblockindex = LookupBlockIndex(hash);
        tip = chainActive.Tip();
    }

    if (!pblockindex) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
    }

    if (!fVerbose)
    {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << pblockindex->GetBlockHeader();
        std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
        return strHex;
    }

    return blockheaderToJSON(tip, pblockindex);
}

static CBlock GetBlockChecked(const CBlockIndex* pblockindex)
{
    CBlock block;
    if (IsBlockPruned(pblockindex)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Block not available (pruned data)");
    }

    if (!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus())) {
//在磁盘上找不到块。这可能是因为我们有街区
//头在索引中，但没有块（例如
//非白名单节点向我们发送一个未请求的长有效链
//块，我们将头添加到索引中，但不接受
//块）。
        throw JSONRPCError(RPC_MISC_ERROR, "Block not found on disk");
    }

    return block;
}

static UniValue getblock(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            RPCHelpMan{"getblock",
                "\nIf verbosity is 0, returns a string that is serialized, hex-encoded data for block 'hash'.\n"
                "If verbosity is 1, returns an Object with information about block <hash>.\n"
                "If verbosity is 2, returns an Object with information about block <hash> and information about each transaction. \n",
                {
                    /*lockhash“，rpcarg:：type:：str_hex，/*opt*/false，/*默认值_val*/”“，“块哈希”，
                    “冗长”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "1", "0 for hex-encoded data, 1 for a json object, and 2 for json object with transaction data"},

                }}
                .ToString() +
            "\nResult (for verbosity = 0):\n"
            "\"data\"             (string) A string that is serialized, hex-encoded data for block 'hash'.\n"
            "\nResult (for verbosity = 1):\n"
            "{\n"
            "  \"hash\" : \"hash\",     (string) the block hash (same as provided)\n"
            "  \"confirmations\" : n,   (numeric) The number of confirmations, or -1 if the block is not on the main chain\n"
            "  \"size\" : n,            (numeric) The block size\n"
            "  \"strippedsize\" : n,    (numeric) The block size excluding witness data\n"
            "  \"weight\" : n           (numeric) The block weight as defined in BIP 141\n"
            "  \"height\" : n,          (numeric) The block height or index\n"
            "  \"version\" : n,         (numeric) The block version\n"
            "  \"versionHex\" : \"00000000\", (string) The block version formatted in hexadecimal\n"
            "  \"merkleroot\" : \"xxxx\", (string) The merkle root\n"
            "  \"tx\" : [               (array of string) The transaction ids\n"
            "     \"transactionid\"     (string) The transaction id\n"
            "     ,...\n"
            "  ],\n"
            "  \"time\" : ttt,          (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"mediantime\" : ttt,    (numeric) The median block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"nonce\" : n,           (numeric) The nonce\n"
            "  \"bits\" : \"1d00ffff\", (string) The bits\n"
            "  \"difficulty\" : x.xxx,  (numeric) The difficulty\n"
            "  \"chainwork\" : \"xxxx\",  (string) Expected number of hashes required to produce the chain up to this block (in hex)\n"
            "  \"nTx\" : n,             (numeric) The number of transactions in the block.\n"
            "  \"previousblockhash\" : \"hash\",  (string) The hash of the previous block\n"
            "  \"nextblockhash\" : \"hash\"       (string) The hash of the next block\n"
            "}\n"
            "\nResult (for verbosity = 2):\n"
            "{\n"
            "  ...,                     Same output as verbosity = 1.\n"
            "  \"tx\" : [               (array of Objects) The transactions in the format of the getrawtransaction RPC. Different from verbosity = 1 \"tx\" result.\n"
            "         ,...\n"
            "  ],\n"
            "  ,...                     Same output as verbosity = 1.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
            + HelpExampleRpc("getblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
        );

    LOCK(cs_main);

    uint256 hash(ParseHashV(request.params[0], "blockhash"));

    int verbosity = 1;
    if (!request.params[1].isNull()) {
        if(request.params[1].isNum())
            verbosity = request.params[1].get_int();
        else
            verbosity = request.params[1].get_bool() ? 1 : 0;
    }

    const CBlockIndex* pblockindex = LookupBlockIndex(hash);
    if (!pblockindex) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
    }

    const CBlock block = GetBlockChecked(pblockindex);

    if (verbosity <= 0)
    {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION | RPCSerializationFlags());
        ssBlock << block;
        std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
        return strHex;
    }

    return blockToJSON(block, chainActive.Tip(), pblockindex, verbosity >= 2);
}

struct CCoinsStats
{
    int nHeight;
    uint256 hashBlock;
    uint64_t nTransactions;
    uint64_t nTransactionOutputs;
    uint64_t nBogoSize;
    uint256 hashSerialized;
    uint64_t nDiskSize;
    CAmount nTotalAmount;

    CCoinsStats() : nHeight(0), nTransactions(0), nTransactionOutputs(0), nBogoSize(0), nDiskSize(0), nTotalAmount(0) {}
};

static void ApplyStats(CCoinsStats &stats, CHashWriter& ss, const uint256& hash, const std::map<uint32_t, Coin>& outputs)
{
    assert(!outputs.empty());
    ss << hash;
    ss << VARINT(outputs.begin()->second.nHeight * 2 + outputs.begin()->second.fCoinBase ? 1u : 0u);
    stats.nTransactions++;
    for (const auto& output : outputs) {
        ss << VARINT(output.first + 1);
        ss << output.second.out.scriptPubKey;
        ss << VARINT(output.second.out.nValue, VarIntMode::NONNEGATIVE_SIGNED);
        stats.nTransactionOutputs++;
        stats.nTotalAmount += output.second.out.nValue;
        /*ts.nbogosize+=32/*txid*/+4/*vout index*/+4/*height+coinbase*/+8/*amount*/+
                           2/*scriptpubkey长度*/ + output.second.out.scriptPubKey.size() /* scriptPubKey */;

    }
    ss << VARINT(0u);
}

//！计算未占用事务输出集的统计信息
static bool GetUTXOStats(CCoinsView *view, CCoinsStats &stats)
{
    std::unique_ptr<CCoinsViewCursor> pcursor(view->Cursor());
    assert(pcursor);

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    stats.hashBlock = pcursor->GetBestBlock();
    {
        LOCK(cs_main);
        stats.nHeight = LookupBlockIndex(stats.hashBlock)->nHeight;
    }
    ss << stats.hashBlock;
    uint256 prevkey;
    std::map<uint32_t, Coin> outputs;
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        COutPoint key;
        Coin coin;
        if (pcursor->GetKey(key) && pcursor->GetValue(coin)) {
            if (!outputs.empty() && key.hash != prevkey) {
                ApplyStats(stats, ss, prevkey, outputs);
                outputs.clear();
            }
            prevkey = key.hash;
            outputs[key.n] = std::move(coin);
        } else {
            return error("%s: unable to read value", __func__);
        }
        pcursor->Next();
    }
    if (!outputs.empty()) {
        ApplyStats(stats, ss, prevkey, outputs);
    }
    stats.hashSerialized = ss.GetHash();
    stats.nDiskSize = view->EstimateSize();
    return true;
}

static UniValue pruneblockchain(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"pruneblockchain", "",
                {
                    /*八“，rpcarg:：type:：num，/*opt*/false，/*default_val*/”，“要修剪到的块高度。可以设置为离散高度，也可以设置为Unix时间戳\n“
            “删除块时间至少比提供的时间戳早2小时的块。”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “N（数字）最后一个修剪块的高度。\n”
            “\n实例：\n”
            +helpExamplecli（“pruneblockchain”，“1000”）。
            +helpexamplerpc（“pruneblockchain”，“1000”）；

    如果（！）FPRONMODED
        throw jsonrpcerror（rpc_misc_error，“无法修剪块，因为节点未处于修剪模式。”）；

    锁（CSKEMAN）；

    int heightparam=request.params[0].get_int（）；
    如果（高度参数<0）
        throw jsonrpcerror（rpc_无效的_参数，“负块高度”）；

    //超过10亿的高度值太高，无法作为块高度，并且
    //太低，无法作为块时间（对应于2001年9月的时间戳）。
    如果（高度参数>100000000）
        //添加一个2小时的缓冲区以包含可能具有旧时间戳的块
        cblockindex*pindex=chainactive.findearliestatleast（heightparam-timestamp_window）；
        如果（！）pQueD）{
            throw jsonrpcerror（rpc_invalid_参数，“找不到至少具有指定时间戳的块。”）；
        }
        heightparam=pindex->nheight；
    }

    无符号整型高度=（无符号整型）高度参数；
    unsigned int chainheight=（unsigned int）chainactive.height（）；
    if（chainheight<params（）.pruneafterheight（））
        throw jsonrpcerror（rpc_misc_error，“区块链太短，无法修剪”）；
    否则，如果（高度>链高度）
        throw jsonrpcerror（rpc_invalid_参数，“区块链短于尝试的修剪高度”）；
    否则，如果（高度>链高-最小_块_到保持）
        logprint（bclog:：rpc，“尝试修剪靠近尖端的块。保留最小块数。\n”）。
        高度=链高-最小\块\保持；
    }

    pruneblockfiles手动（高度）；
    返回uint64_t（高度）；
}

静态单值gettxoutsetinfo（const jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 0）
        throw std:：runtime_错误（
            rpchelpman“gettxoutsetinfo”，
                \n返回有关未暂停事务输出集的统计信息。\n
                “注意，此呼叫可能需要一些时间。\n”，
                {}
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            ““高度”：n，（数字）当前块高度（索引）\n”
            ““bestblock\”：“hex\”，（string）链顶端的块哈希\n”
            ““事务”：n，（数字）具有未暂停输出的事务数\n”
            ““txouts”：n，（数字）未暂停的事务输出数\n”
            ““bogosize”：n，（数字）utxo集大小的无意义度量值\n”
            “哈希\已序列化\u 2\”：\“哈希\”，（string）已序列化的哈希\n”
            “磁盘大小”：n，（数字）磁盘上链状态的估计大小\n”
            “总金额”：x.x x x（数字）总金额\n”
            “}\n”
            “\n实例：\n”
            +helpexamplecli（“gettxoutsetinfo”，“”）
            +helpExampleRPC（“gettXoutSetInfo”，“”）
        ；

    单值ret（单值：：vobj）；

    CCoinstats统计；
    flushstatetodisk（）；
    if（getutxostats（pcoinsdbview.get（），stats））
        ret.pushkv（“高度”，（int64_t）stats.nheight）；
        ret.pushkv（“bestblock”，stats.hashblock.gethex（））；
        ret.pushkv（“事务”，（int64_t）stats.ntransactions）；
        ret.pushkv（“txouts”，（int64_t）stats.ntransactionoutputs）；
        ret.pushkv（“bogosize”，（int64_t）stats.nbogosize）；
        ret.pushkv（“hash_serialized_2”，stats.hash serialized.gethex（））；
        ret.pushkv（“磁盘大小”，stats.ndisksize）；
        ret.pushkv（“总金额”，valuefromamount（stats.ntotalamount））；
    }否则{
        throw jsonrpcerror（rpc_internal_error，“Unable to read utxo set”）；
    }
    返回RET；
}

单值gettxout（const-jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）<2 request.params.size（）>3）
        throw std:：runtime_错误（
            rpchelpman“gettxout”，
                “\n返回有关未暂停事务输出的详细信息。\n”，
                {
                    “txid”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The transaction id"},

                    /*“，rpcarg：：type：：num，/*opt*/false，/*默认值/”“，vout编号”，
                    “include_mempool”，rpcarg:：type:：bool，/*opt*/ true, /* default_val */ "true", "Whether to include the mempool. Note that an unspent output that is spent in the mempool won't appear."},

                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"bestblock\":  \"hash\",    (string) The hash of the block at the tip of the chain\n"
            "  \"confirmations\" : n,       (numeric) The number of confirmations\n"
            "  \"value\" : x.xxx,           (numeric) The transaction value in " + CURRENCY_UNIT + "\n"
            "  \"scriptPubKey\" : {         (json object)\n"
            "     \"asm\" : \"code\",       (string) \n"
            "     \"hex\" : \"hex\",        (string) \n"
            "     \"reqSigs\" : n,          (numeric) Number of required signatures\n"
            "     \"type\" : \"pubkeyhash\", (string) The type, eg pubkeyhash\n"
            "     \"addresses\" : [          (array of string) array of bitcoin addresses\n"
            "        \"address\"     (string) bitcoin address\n"
            "        ,...\n"
            "     ]\n"
            "  },\n"
            "  \"coinbase\" : true|false   (boolean) Coinbase or not\n"
            "}\n"

            "\nExamples:\n"
            "\nGet unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nView the details\n"
            + HelpExampleCli("gettxout", "\"txid\" 1") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("gettxout", "\"txid\", 1")
        );

    LOCK(cs_main);

    UniValue ret(UniValue::VOBJ);

    uint256 hash(ParseHashV(request.params[0], "txid"));
    int n = request.params[1].get_int();
    COutPoint out(hash, n);
    bool fMempool = true;
    if (!request.params[2].isNull())
        fMempool = request.params[2].get_bool();

    Coin coin;
    if (fMempool) {
        LOCK(mempool.cs);
        CCoinsViewMemPool view(pcoinsTip.get(), mempool);
        if (!view.GetCoin(out, coin) || mempool.isSpent(out)) {
            return NullUniValue;
        }
    } else {
        if (!pcoinsTip->GetCoin(out, coin)) {
            return NullUniValue;
        }
    }

    const CBlockIndex* pindex = LookupBlockIndex(pcoinsTip->GetBestBlock());
    ret.pushKV("bestblock", pindex->GetBlockHash().GetHex());
    if (coin.nHeight == MEMPOOL_HEIGHT) {
        ret.pushKV("confirmations", 0);
    } else {
        ret.pushKV("confirmations", (int64_t)(pindex->nHeight - coin.nHeight + 1));
    }
    ret.pushKV("value", ValueFromAmount(coin.out.nValue));
    UniValue o(UniValue::VOBJ);
    ScriptPubKeyToUniv(coin.out.scriptPubKey, o, true);
    ret.pushKV("scriptPubKey", o);
    ret.pushKV("coinbase", (bool)coin.fCoinBase);

    return ret;
}

static UniValue verifychain(const JSONRPCRequest& request)
{
    int nCheckLevel = gArgs.GetArg("-checklevel", DEFAULT_CHECKLEVEL);
    int nCheckDepth = gArgs.GetArg("-checkblocks", DEFAULT_CHECKBLOCKS);
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            RPCHelpMan{"verifychain",
                "\nVerifies blockchain database.\n",
                {
                    /*hecklevel“，rpcarg:：type:：num，/*opt*/true，/*default_val*/strprintf（”%d，range=0-4“，nchecklevel），“块验证的彻底性。”，
                    “nblocks”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ strprintf("%d, 0=all", nCheckDepth), "The number of blocks to check."},

                }}
                .ToString() +
            "\nResult:\n"
            "true|false       (boolean) Verified or not\n"
            "\nExamples:\n"
            + HelpExampleCli("verifychain", "")
            + HelpExampleRpc("verifychain", "")
        );

    LOCK(cs_main);

    if (!request.params[0].isNull())
        nCheckLevel = request.params[0].get_int();
    if (!request.params[1].isNull())
        nCheckDepth = request.params[1].get_int();

    return CVerifyDB().VerifyDB(Params(), pcoinsTip.get(), nCheckLevel, nCheckDepth);
}

/*具有更好反馈的问题优先级的实现*/
static UniValue SoftForkMajorityDesc(int version, const CBlockIndex* pindex, const Consensus::Params& consensusParams)
{
    UniValue rv(UniValue::VOBJ);
    bool activated = false;
    switch(version)
    {
        case 2:
            activated = pindex->nHeight >= consensusParams.BIP34Height;
            break;
        case 3:
            activated = pindex->nHeight >= consensusParams.BIP66Height;
            break;
        case 4:
            activated = pindex->nHeight >= consensusParams.BIP65Height;
            break;
    }
    rv.pushKV("status", activated);
    return rv;
}

static UniValue SoftForkDesc(const std::string &name, int version, const CBlockIndex* pindex, const Consensus::Params& consensusParams)
{
    UniValue rv(UniValue::VOBJ);
    rv.pushKV("id", name);
    rv.pushKV("version", version);
    rv.pushKV("reject", SoftForkMajorityDesc(version, pindex, consensusParams));
    return rv;
}

static UniValue BIP9SoftForkDesc(const Consensus::Params& consensusParams, Consensus::DeploymentPos id)
{
    UniValue rv(UniValue::VOBJ);
    const ThresholdState thresholdState = VersionBitsTipState(consensusParams, id);
    switch (thresholdState) {
    case ThresholdState::DEFINED: rv.pushKV("status", "defined"); break;
    case ThresholdState::STARTED: rv.pushKV("status", "started"); break;
    case ThresholdState::LOCKED_IN: rv.pushKV("status", "locked_in"); break;
    case ThresholdState::ACTIVE: rv.pushKV("status", "active"); break;
    case ThresholdState::FAILED: rv.pushKV("status", "failed"); break;
    }
    if (ThresholdState::STARTED == thresholdState)
    {
        rv.pushKV("bit", consensusParams.vDeployments[id].bit);
    }
    rv.pushKV("startTime", consensusParams.vDeployments[id].nStartTime);
    rv.pushKV("timeout", consensusParams.vDeployments[id].nTimeout);
    rv.pushKV("since", VersionBitsTipStateSinceHeight(consensusParams, id));
    if (ThresholdState::STARTED == thresholdState)
    {
        UniValue statsUV(UniValue::VOBJ);
        BIP9Stats statsStruct = VersionBitsTipStatistics(consensusParams, id);
        statsUV.pushKV("period", statsStruct.period);
        statsUV.pushKV("threshold", statsStruct.threshold);
        statsUV.pushKV("elapsed", statsStruct.elapsed);
        statsUV.pushKV("count", statsStruct.count);
        statsUV.pushKV("possible", statsStruct.possible);
        rv.pushKV("statistics", statsUV);
    }
    return rv;
}

static void BIP9SoftForkDescPushBack(UniValue& bip9_softforks, const Consensus::Params& consensusParams, Consensus::DeploymentPos id)
{
//超时值为0的部署被隐藏。
//超时值为0可确保永远不会激活SoftFork。
//这在合并SoftFork代码而不指定部署计划时使用。
    if (consensusParams.vDeployments[id].nTimeout > 0)
        bip9_softforks.pushKV(VersionBitsDeploymentInfo[id].name, BIP9SoftForkDesc(consensusParams, id));
}

UniValue getblockchaininfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            RPCHelpMan{"getblockchaininfo",
                "Returns an object containing various state info regarding blockchain processing.\n", {}}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"chain\": \"xxxx\",              (string) current network name as defined in BIP70 (main, test, regtest)\n"
            "  \"blocks\": xxxxxx,             (numeric) the current number of blocks processed in the server\n"
            "  \"headers\": xxxxxx,            (numeric) the current number of headers we have validated\n"
            "  \"bestblockhash\": \"...\",       (string) the hash of the currently best block\n"
            "  \"difficulty\": xxxxxx,         (numeric) the current difficulty\n"
            "  \"mediantime\": xxxxxx,         (numeric) median time for the current best block\n"
            "  \"verificationprogress\": xxxx, (numeric) estimate of verification progress [0..1]\n"
            "  \"initialblockdownload\": xxxx, (bool) (debug information) estimate of whether this node is in Initial Block Download mode.\n"
            "  \"chainwork\": \"xxxx\"           (string) total amount of work in active chain, in hexadecimal\n"
            "  \"size_on_disk\": xxxxxx,       (numeric) the estimated size of the block and undo files on disk\n"
            "  \"pruned\": xx,                 (boolean) if the blocks are subject to pruning\n"
            "  \"pruneheight\": xxxxxx,        (numeric) lowest-height complete block stored (only present if pruning is enabled)\n"
            "  \"automatic_pruning\": xx,      (boolean) whether automatic pruning is enabled (only present if pruning is enabled)\n"
            "  \"prune_target_size\": xxxxxx,  (numeric) the target size used by pruning (only present if automatic pruning is enabled)\n"
            "  \"softforks\": [                (array) status of softforks in progress\n"
            "     {\n"
            "        \"id\": \"xxxx\",           (string) name of softfork\n"
            "        \"version\": xx,          (numeric) block version\n"
            "        \"reject\": {             (object) progress toward rejecting pre-softfork blocks\n"
            "           \"status\": xx,        (boolean) true if threshold reached\n"
            "        },\n"
            "     }, ...\n"
            "  ],\n"
            "  \"bip9_softforks\": {           (object) status of BIP9 softforks in progress\n"
            "     \"xxxx\" : {                 (string) name of the softfork\n"
            "        \"status\": \"xxxx\",       (string) one of \"defined\", \"started\", \"locked_in\", \"active\", \"failed\"\n"
            "        \"bit\": xx,              (numeric) the bit (0-28) in the block version field used to signal this softfork (only for \"started\" status)\n"
            "        \"startTime\": xx,        (numeric) the minimum median time past of a block at which the bit gains its meaning\n"
            "        \"timeout\": xx,          (numeric) the median time past of a block at which the deployment is considered failed if not yet locked in\n"
            "        \"since\": xx,            (numeric) height of the first block to which the status applies\n"
            "        \"statistics\": {         (object) numeric statistics about BIP9 signalling for a softfork (only for \"started\" status)\n"
            "           \"period\": xx,        (numeric) the length in blocks of the BIP9 signalling period \n"
            "           \"threshold\": xx,     (numeric) the number of blocks with the version bit set required to activate the feature \n"
            "           \"elapsed\": xx,       (numeric) the number of blocks elapsed since the beginning of the current period \n"
            "           \"count\": xx,         (numeric) the number of blocks with the version bit set in the current period \n"
            "           \"possible\": xx       (boolean) returns false if there are not enough blocks left in this period to pass activation threshold \n"
            "        }\n"
            "     }\n"
            "  }\n"
            "  \"warnings\" : \"...\",           (string) any network and blockchain warnings.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockchaininfo", "")
            + HelpExampleRpc("getblockchaininfo", "")
        );

    LOCK(cs_main);

    const CBlockIndex* tip = chainActive.Tip();
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("chain",                 Params().NetworkIDString());
    obj.pushKV("blocks",                (int)chainActive.Height());
    obj.pushKV("headers",               pindexBestHeader ? pindexBestHeader->nHeight : -1);
    obj.pushKV("bestblockhash",         tip->GetBlockHash().GetHex());
    obj.pushKV("difficulty",            (double)GetDifficulty(tip));
    obj.pushKV("mediantime",            (int64_t)tip->GetMedianTimePast());
    obj.pushKV("verificationprogress",  GuessVerificationProgress(Params().TxData(), tip));
    obj.pushKV("initialblockdownload",  IsInitialBlockDownload());
    obj.pushKV("chainwork",             tip->nChainWork.GetHex());
    obj.pushKV("size_on_disk",          CalculateCurrentUsage());
    obj.pushKV("pruned",                fPruneMode);
    if (fPruneMode) {
        const CBlockIndex* block = tip;
        assert(block);
        while (block->pprev && (block->pprev->nStatus & BLOCK_HAVE_DATA)) {
            block = block->pprev;
        }

        obj.pushKV("pruneheight",        block->nHeight);

//如果为0，则执行将绕过整个if块。
        bool automatic_pruning = (gArgs.GetArg("-prune", 0) != 1);
        obj.pushKV("automatic_pruning",  automatic_pruning);
        if (automatic_pruning) {
            obj.pushKV("prune_target_size",  nPruneTarget);
        }
    }

    const Consensus::Params& consensusParams = Params().GetConsensus();
    UniValue softforks(UniValue::VARR);
    UniValue bip9_softforks(UniValue::VOBJ);
    softforks.push_back(SoftForkDesc("bip34", 2, tip, consensusParams));
    softforks.push_back(SoftForkDesc("bip66", 3, tip, consensusParams));
    softforks.push_back(SoftForkDesc("bip65", 4, tip, consensusParams));
    for (int pos = Consensus::DEPLOYMENT_CSV; pos != Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++pos) {
        BIP9SoftForkDescPushBack(bip9_softforks, consensusParams, static_cast<Consensus::DeploymentPos>(pos));
    }
    obj.pushKV("softforks",             softforks);
    obj.pushKV("bip9_softforks", bip9_softforks);

    obj.pushKV("warnings", GetWarnings("statusbar"));
    return obj;
}

/*用于对getchaintips头排序的比较函数。*/
struct CompareBlocksByHeight
{
    bool operator()(const CBlockIndex* a, const CBlockIndex* b) const
    {
        /*确保高度相同的不相等块不会比较
           相等。使用指针本身进行区分。*/


        if (a->nHeight != b->nHeight)
          return (a->nHeight > b->nHeight);

        return a < b;
    }
};

static UniValue getchaintips(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            RPCHelpMan{"getchaintips",
                "Return information about all known tips in the block tree,"
                " including the main chain as well as orphaned branches.\n",
                {}}
                .ToString() +
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"height\": xxxx,         (numeric) height of the chain tip\n"
            "    \"hash\": \"xxxx\",         (string) block hash of the tip\n"
            "    \"branchlen\": 0          (numeric) zero for main chain\n"
            "    \"status\": \"active\"      (string) \"active\" for the main chain\n"
            "  },\n"
            "  {\n"
            "    \"height\": xxxx,\n"
            "    \"hash\": \"xxxx\",\n"
            "    \"branchlen\": 1          (numeric) length of branch connecting the tip to the main chain\n"
            "    \"status\": \"xxxx\"        (string) status of the chain (active, valid-fork, valid-headers, headers-only, invalid)\n"
            "  }\n"
            "]\n"
            "Possible values for status:\n"
            "1.  \"invalid\"               This branch contains at least one invalid block\n"
            "2.  \"headers-only\"          Not all blocks for this branch are available, but the headers are valid\n"
            "3.  \"valid-headers\"         All blocks are available for this branch, but they were never fully validated\n"
            "4.  \"valid-fork\"            This branch is not part of the active chain, but is fully validated\n"
            "5.  \"active\"                This is the tip of the active main chain, which is certainly valid\n"
            "\nExamples:\n"
            + HelpExampleCli("getchaintips", "")
            + HelpExampleRpc("getchaintips", "")
        );

    LOCK(cs_main);

    /*
     *想法：链提示的集合是chainactive.tip，加上孤立块，它们没有其他孤立的构建。
     ＊算法：
     *-通过一次mapblockindex，选择孤立块，并存储一组孤立块的pprev指针。
     *-遍历孤立块。如果块没有被另一个孤立对象指向，则它是一个链尖。
     *-添加chainactive.tip（）。
     **/

    std::set<const CBlockIndex*, CompareBlocksByHeight> setTips;
    std::set<const CBlockIndex*> setOrphans;
    std::set<const CBlockIndex*> setPrevs;

    for (const std::pair<const uint256, CBlockIndex*>& item : mapBlockIndex)
    {
        if (!chainActive.Contains(item.second)) {
            setOrphans.insert(item.second);
            setPrevs.insert(item.second->pprev);
        }
    }

    for (std::set<const CBlockIndex*>::iterator it = setOrphans.begin(); it != setOrphans.end(); ++it)
    {
        if (setPrevs.erase(*it) == 0) {
            setTips.insert(*it);
        }
    }

//始终报告当前活动的提示。
    setTips.insert(chainActive.Tip());

    /*构造输出数组。*/
    UniValue res(UniValue::VARR);
    for (const CBlockIndex* block : setTips)
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("height", block->nHeight);
        obj.pushKV("hash", block->phashBlock->GetHex());

        const int branchLen = block->nHeight - chainActive.FindFork(block)->nHeight;
        obj.pushKV("branchlen", branchLen);

        std::string status;
        if (chainActive.Contains(block)) {
//此块是当前活动链的一部分。
            status = "active";
        } else if (block->nStatus & BLOCK_FAILED_MASK) {
//此块或其祖先之一无效。
            status = "invalid";
        } else if (!block->HaveTxsDownloaded()) {
//无法连接此块，因为缺少它或它的某个父级的完整块数据。
            status = "headers-only";
        } else if (block->IsValid(BLOCK_VALID_SCRIPTS)) {
//此块已完全验证，但不再是活动链的一部分。它可能曾经是活动块，但被重新组织了。
            status = "valid-fork";
        } else if (block->IsValid(BLOCK_VALID_TREE)) {
//此块的头有效，但尚未验证。它可能从来不是大多数工作链的一部分。
            status = "valid-headers";
        } else {
//没有线索。
            status = "unknown";
        }
        obj.pushKV("status", status);

        res.push_back(obj);
    }

    return res;
}

UniValue mempoolInfoToJSON()
{
    UniValue ret(UniValue::VOBJ);
    ret.pushKV("size", (int64_t) mempool.size());
    ret.pushKV("bytes", (int64_t) mempool.GetTotalTxSize());
    ret.pushKV("usage", (int64_t) mempool.DynamicMemoryUsage());
    size_t maxmempool = gArgs.GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000;
    ret.pushKV("maxmempool", (int64_t) maxmempool);
    ret.pushKV("mempoolminfee", ValueFromAmount(std::max(mempool.GetMinFee(maxmempool), ::minRelayTxFee).GetFeePerK()));
    ret.pushKV("minrelaytxfee", ValueFromAmount(::minRelayTxFee.GetFeePerK()));

    return ret;
}

static UniValue getmempoolinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            RPCHelpMan{"getmempoolinfo",
                "\nReturns details on the active state of the TX memory pool.\n", {}}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"size\": xxxxx,               (numeric) Current tx count\n"
            "  \"bytes\": xxxxx,              (numeric) Sum of all virtual transaction sizes as defined in BIP 141. Differs from actual serialized size because witness data is discounted\n"
            "  \"usage\": xxxxx,              (numeric) Total memory usage for the mempool\n"
            "  \"maxmempool\": xxxxx,         (numeric) Maximum memory usage for the mempool\n"
            "  \"mempoolminfee\": xxxxx       (numeric) Minimum fee rate in " + CURRENCY_UNIT + "/kB for tx to be accepted. Is the maximum of minrelaytxfee and minimum mempool fee\n"
            "  \"minrelaytxfee\": xxxxx       (numeric) Current minimum relay fee for transactions\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmempoolinfo", "")
            + HelpExampleRpc("getmempoolinfo", "")
        );

    return mempoolInfoToJSON();
}

static UniValue preciousblock(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"preciousblock",
                "\nTreats a block as if it were received before others with the same work.\n"
                "\nA later preciousblock call can override the effect of an earlier one.\n"
                "\nThe effects of preciousblock are not retained across restarts.\n",
                {
                    /*lockhash“，rpcarg:：type:：str_hex，/*opt*/false，/*default_val*/”“，“要标记为贵重”的块的哈希，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “\n实例：\n”
            +helpExamplecli（“preciousBlock”，“blockhash\”）
            +helpexamplerpc（“preciousBlock”，“blockhash\”）
        ；

    uint256散列（parsehashv（request.params[0]，“blockhash”））；
    cblockindex*pblockindex；

    {
        锁（CSKEMAN）；
        pblockindex=lookupblockindex（哈希）；
        如果（！）P-阻断蛋白）
            throw jsonrpcerror（rpc_invalid_address_or_key，“找不到块”）；
        }
    }

    cvalidationState状态；
    preciousBlock（状态，params（），pblockIndex）；

    如果（！）state.isvalid（））
        throw jsonrpcerror（rpc_database_error，formatStateMessage（state））；
    }

    返回nullunivalue；
}

静态单值无效块（const-jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 1）
        throw std:：runtime_错误（
            rpchelpman“无效块”，
                \n永久性地将块标记为无效，就像它违反了共识规则一样。\n“，
                {
                    “blockhash”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "the hash of the block to mark as invalid"},

                }}
                .ToString() +
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("invalidateblock", "\"blockhash\"")
            + HelpExampleRpc("invalidateblock", "\"blockhash\"")
        );

    uint256 hash(ParseHashV(request.params[0], "blockhash"));
    CValidationState state;

    {
        LOCK(cs_main);
        CBlockIndex* pblockindex = LookupBlockIndex(hash);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }

        InvalidateBlock(state, Params(), pblockindex);
    }

    if (state.IsValid()) {
        ActivateBestChain(state, Params());
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, FormatStateMessage(state));
    }

    return NullUniValue;
}

static UniValue reconsiderblock(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"reconsiderblock",
                "\nRemoves invalidity status of a block, its ancestors and its descendants, reconsider them for activation.\n"
                "This can be used to undo the effects of invalidateblock.\n",
                {
                    /*lockhash“，rpcarg:：type:：str_hex，/*opt*/false，/*default_val*/”，“要重新考虑的块的哈希”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “\n实例：\n”
            +helpexamplecli（“重新考虑块”，“\”blockhash\”）
            +helpexamplerpc（“重新考虑块”，“\”blockhash\”）
        ；

    uint256散列（parsehashv（request.params[0]，“blockhash”））；

    {
        锁（CSKEMAN）；
        cBlockIndex*pbLockIndex=LookupBlockIndex（哈希）；
        如果（！）P-阻断蛋白）
            throw jsonrpcerror（rpc_invalid_address_or_key，“找不到块”）；
        }

        ResetBlockFailureFlags（pbLockIndex）；
    }

    cvalidationState状态；
    activateBestChain（状态，params（））；

    如果（！）state.isvalid（））
        throw jsonrpcerror（rpc_database_error，formatStateMessage（state））；
    }

    返回nullunivalue；
}

静态单值getchaintxstats（const jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）>2）
        throw std:：runtime_错误（
            rpchelpman“getchaintxstats”，
                \n计算链中事务总数和速率的统计信息。\n“，
                {
                    “nblocks”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "one month", "Size of the window in number of blocks"},

                    /*lockhash“，rpcarg:：type:：str_hex，/*opt*/true，/*default_val*/”chain tip“，”结束窗口的块的哈希。”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            “time”：xxxxx，（数字）窗口中最后一个块的时间戳，采用unix格式。\n”
            ““txcount”：xxxxx，（数字）到该点为止链中的事务总数。\n”
            “window”window“最后一个”block“hash\”：\“…\”，（string）窗口中最后一个块的hash。\n”
            “窗口\块\计数\”：xxxxx，（数字）窗口的大小（以块数计）。\n”
            “\”window_tx_count\”：xxxxx，（数字）窗口中的事务数。仅当“窗口块计数”大于0时返回。\n
            “\”窗口\间隔\“：XXXXX，（数字）窗口中的已用时间（以秒为单位）。仅当“窗口块计数”大于0时返回。\n
            “\”txRate\“：x.x x，（数字）窗口中每秒事务的平均速率。仅当“窗口间隔”大于0时返回。\n
            “}\n”
            “\n实例：\n”
            +helpExamplecli（“getchaintxstats”，“”）
            +helpExampleRPC（“getchaintxstats”，“2016”）。
        ；

    常量cblockindex*pindex；
    int blockcount=30*24*60*60/params（）.getconsensus（）.npowTargetSpacking；//默认为1个月

    if（request.params[1].isNull（））
        锁（CSKEMAN）；
        pindex=chainactive.tip（）；
    }否则{
        uint256散列（parsehashv（request.params[1]，“blockhash”））；
        锁（CSKEMAN）；
        pindex=查找块索引（hash）；
        如果（！）pQueD）{
            throw jsonrpcerror（rpc_invalid_address_or_key，“找不到块”）；
        }
        如果（！）chainactive.contains（pindex））
            throw jsonrpcerror（rpc_invalid_参数，“block is not in main chain”）；
        }
    }

    断言（pCurnor）！= null pTr）；

    if（request.params[0].isNull（））
        blockcount=std:：max（0，std:：min（blockcount，pindex->nheight-1））；
    }否则{
        blockCount=request.params[0].get_int（）；

        if（blockcount<0（blockcount>0&&blockcount>=pindex->nheight））
            throw jsonrpcerror（rpc_invalid_参数，“无效的块计数：应介于0和块的高度-1之间”）；
        }
    }

    const cblockindex*pindexpast=pindex->getancestor（pindex->nheight-blockcount）；
    int ntimediff=pindex->getmediantimepast（）-pindexpast->getmediantimepast（）；
    int ntxdiff=pindex->nchaintx-pindexpass->nchaintx；

    单值ret（单值：：vobj）；
    ret.pushkv（“时间”，（int64_t）pindex->ntime）；
    ret.pushkv（“txcount”，（int64_t）pindex->nchaintx）；
    ret.pushkv（“window_final_block_hash”，pindex->getblockhash（）.gethex（））；
    ret.pushkv（“窗口块计数”，块计数）；
    如果（blockcount>0）
        ret.pushkv（“window_tx_count”，ntxdiff）；
        ret.pushkv（“窗口间隔”，ntimediff）；
        如果（ntimediff>0）
            ret.pushkv（“txtrate”，（（双）ntxdiff）/ntimediff）；
        }
    }

    返回RET；
}

模板<typename t>
静态t calculateTruncatedMedian（std:：vector<t>和scores）
{
    size_t size=scores.size（）；
    如果（尺寸=0）
        返回0；
    }

    std：：sort（scores.begin（），scores.end（））；
    如果（大小%2==0）
        返回（分数[size/2-1]+分数[size/2]）/2；
    }否则{
        返回分数[尺寸/2]；
    }
}

void calculatePercentilesByweight（camount result[num搎getBlockStats_percentiles]，std:：vector<std:：pair<camount，int64_t>>&scores，int64_t total_weight）
{
    if（scores.empty（））
        返回；
    }

    std：：sort（scores.begin（），scores.end（））；

    //第10、25、50、75和90百分位重量单位。
    常量双权重[num_GetBlockStats_Percentiles]=
        总重量/10.0，总重量/4.0，总重量/2.0，（总重量*3.0）/4.0，（总重量*9.0）/10.0
    }；

    Int64_t next_percentile_index=0；
    Int64_t累计_权重=0；
    for（const auto&element:分数）
        累计_weight+=element.second；
        while（next_percentile_index<num_getblockstats_percentiles&&cumulative_weight>=weights[next_percentile_index]）；
            结果[下一个百分位索引]=element.first；
            +下一个_百分位数_指数；
        }
    }

    //用最后一个值填充所有剩余的百分位数。
    for（int64_t i=next_percentile_index；i<num_getblockstats_percentiles；i++）
        结果[i]=scores.back（）.first；
    }
}

模板<typename t>
静态内联bool sethaskeys（const std:：set<t>&set）返回false；
模板<typename t，typename tk，typename…ARG>
静态内联bool sethaskeys（const std:：set<t>&set，const tk&key，const args&…ARGS）
{
    返回（set.count（key）！=0）sethaskeys（set，args…）；
}

//outpoint（utxo索引需要）+nheight+fcoinbase
静态constexpr size_t per_utxo_开销=sizeof（coutpoint）+sizeof（uint32_t）+sizeof（bool）；

静态单值getblockstats（const jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）<1 request.params.size（）>4）
        throw std:：runtime_错误（
            rpchelpman“getblockstats”，
                \n计算给定窗口的每个块的统计信息。所有金额均以Satoshis为单位。\n“
                “修剪对某些高度不起作用。\n”
                “如果没有utxo-size_inc的-txindex、*费用或*feerate状态，它将无法工作。\n”，
                {
                    “hash_or_height”，rpcarg:：type:：num，/*opt*/ false, /* default_val */ "", "The block hash or height of the target block", "", {"", "string or numeric"}},

                    /*tats“，rpcarg:：type:：arr，/*opt*/true，/*default_val*/”all values“，”要绘制的值（请参见下面的结果）”，
                        {
                            “高度”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "", "Selected statistic"},

                            /*输入法“，rpcarg:：type:：str，/*opt*/true，/*默认值“/”，“所选统计”，
                        }
                        “统计”}
                }
                toSTRIN（）+
            “\NESRES:\N”
            “（JSON对象\n”
            “avgfee\”：xxxxx，（数字）块中的平均费用\n”
            “avgfeerate\”：xxxxx，（数字）平均频率（以每虚拟字节的Satoshis为单位）\n”
            “avgtxsize\”：xxxxx，（数字）平均事务大小\n”
            “\”block hash\“：xxxxx，（string）块哈希（检查潜在的重新排序）。\n”
            “feerate percentiles”：[（数字数组）feerates at the 10th、25th、50th、75th和90th percentile weight unity（in satoshis per virtual byte）\n”
            “第10个百分点值”，（数字）第10个百分点值\n”
            “25%”，（数字）25%”，
            “第50个百分点值”“，（数字）第50个百分点值\n”
            “第75个百分点值”“，（数字）第75个百分点值\n”
            “第90个百分点值”“，（数字）第90个百分点值\n”
            “”
            ““高度”：XXXXX，（数字）块的高度\n”
            “\”ins\“：xxxxx，（数字）输入数（不包括coinbase）\n”
            “maxfee”：xxxxx，（数字）块中的最大费用\n”
            “maxfeerate”：xxxxx，（数字）最大feerate（以每个虚拟字节的Satoshis为单位）\n”
            “\”maxtxsize\“：xxxxx，（数字）最大事务大小\n”
            “median fee”：xxxxx，（数字）块中截断的中间费用\n”
            ““median time\”：xxxxx，（数字）过去的块中位数时间\n”
            “mediantxsize”：xxxxx，（数字）截断的中间事务大小\n”
            “minfee”：xxxxx，（数字）块中的最低费用\n”
            “minfeerate\”：xxxxx，（数字）最小feerate（以每个虚拟字节的Satoshis为单位）\n”
            “mintxsize\”：xxxxx，（数字）最小事务大小\n”
            ““Outs”：XXXXX，（数字）输出数\n”
            ““补贴”：XXXXX，（数字）块补贴\n”
            ““swtotal”大小“：xxxxx，（数字）所有segwit事务的总大小\n”
            “swtotal_weight”：xxxxx，（数字）所有segwit事务的总重量除以segwit比例因子（4）\n”
            ““SWTXS\”：xxxxx，（数字）segwit事务数\n”
            ““时间”：XXXXX，（数字）块时间\n”
            “\”合计\“：XXXXX，（数字）所有输出的总金额（不包括CoinBase，因此奖励[即补贴+总费用]）\n”
            “总计大小”：XXXXX，（数字）所有非CoinBase交易的总计大小\n”
            “总权重”：XXXXX，（数字）所有非CoinBase交易的总权重除以Segwit比例因子（4）\n”
            “\”total fee\“：xxxxx，（数字）总费用\n”
            ““txs\”：xxxxx，（数字）事务数（不包括coinbase）\n”
            ““utxo增加”：xxxxx，（数字）未暂停输出数量的增加/减少\n”
            “utxo-size_inc”：xxxxx，（数字）utxo索引的大小增加/减少（不折扣opu-return和类似的值）\n”
            “}\n”
            “\n实例：\n”
            +helpExamplecli（“getblockstats”，“1000'[\”minfeerate\“，\”avgfeerate\“]”）
            +helpExampleRpc（“getBlockstats”，“1000'[\”minfeerate\“，\”avgfeerate\“]”）
        ；
    }

    锁（CSKEMAN）；

    cblockindex*索引；
    if（request.params[0].isnum（））
        const int height=request.params[0].get_int（）；
        const int current_tip=chainactive.height（）；
        如果（高度<0）
            throw jsonrpcerror（rpc_无效的_参数，strprintf（“目标块高度%d为负”，高度））；
        }
        如果（高度>当前_尖）
            throw jsonrpcerror（rpc_无效的_参数，strprintf（“当前提示%d之后的目标块高度%d”，高度，当前_提示））；
        }

        pindex=链激活[高度]；
    }否则{
        const uint256 hash（parsehashv（request.params[0]，“hash_or_height”））；
        pindex=查找块索引（hash）；
        如果（！）pQueD）{
            throw jsonrpcerror（rpc_invalid_address_or_key，“找不到块”）；
        }
        如果（！）chainactive.contains（pindex））
            throw jsonrpcerror（rpc_invalid_参数，strprintf（“block is not in chain%s”，params（）.networkidstring（））；
        }
    }

    断言（pCurnor）！= null pTr）；

    std:：set<std:：string>stats；
    如果（！）request.params[1].isNull（））
        const univalue stats_univalue=request.params[1].get_array（）；
        for（unsigned int i=0；i<stats_univalue.size（）；i++）
            const std:：string stat=stats_univalue[i].get_str（）；
            stats.插入（stat）；
        }
    }

    const cblock block=getblockchecked（pindex）；

    const bool do_all=stats.size（）==0；//如果未选择任何内容，则计算所有内容（默认）
    const bool do_mediantxsize=全部stats.count（“mediantxsize”）！＝0；
    const bool do_medianfee=全部stats.count（“medianfee”）！＝0；
    const bool do feerate_percentiles=所有stats.count（“feerate_percentiles”）！＝0；
    const bool loop_inputs=do_all_do_medianfee_do_erate_u percentiles_
        sethaskeys（stats，“utxo-size_inc”，“totalfee”，“avgfee”，“avgfeerate”，“minfee”，“maxfee”，“minfeerate”，“maxfeerate”）；
    const bool loop_outputs=do_all_loop_inputs_stats.count（“total_out”）；
    const bool do_calculate_size=do_mediantxsize_
        sethaskeys（stats，“total_size”，“avgtxsize”，“mintxsize”，“maxtxsize”，“swtotal_size”）；
    const bool do_calculate_weight=do_all_sethaskeys（stats，“totalou weight”，“avgfeerate”，“swtotalou weight”，“avgfeerate”，“feerateou percentiles”，“minfeerate”，“maxfeerate”）；
    const bool do_calculate_sw=do_all_sethaskeys（stats，“swtxs”，“swtotal_size”，“swtotal_weight”）；

    如果（循环输入&！GX-TXEXT）{
        throw jsonrpcerror（rpc_invalid_参数，“所选的一个或多个状态需要-txindex enabled”）；
    }

    camount maxfee=0；
    camount maxfeerate=0；
    camount minfee=最高奖金；
    camount minfeerate=最高金额；
    camount total_out=0；
    camount totalfee=0；
    Int64输入=0；
    Int64_t maxtxsize=0；
    int64_t mintxsize=max_block_serialized_size；
    Int64输出=0；
    int64_t swtotal_size=0；
    Int64总重量=0；
    Int64_t SWTXS=0；
    Int64总尺寸=0；
    Int64总重量=0；
    int64_t utxo_size_inc=0；
    std:：vector<camount>fee_array；
    std:：vector<std:：pair<camount，int64_t>>feerate_array；
    std:：vector<int64_t>txsize_array；

    用于（const auto&tx:block.vtx）
        输出+=tx->vout.size（）；

        camount tx_total_out=0；
        如果（回路输出）
            for（const ctxout&out:tx->vout）
                tx_total_out+=out.n值；
                utxo_size_inc+=getSerializeSize（out，协议_版本）+每_utxo_开销；
            }
        }

        if（tx->iscoinBase（））
            继续；
        }

        input s+=tx->vin.size（）；//不计算coinbase的假输入
        total_out+=tx_total_out；//不计算coinbase奖励

        Int64_t tx_尺寸=0；
        如果（do_calculate_size）

            tx_size=tx->gettotalize（）；
            如果（do mediantxsize）
                tx size_array.push_back（tx_size）；
            }
            max tx size=std:：max（max tx size，tx_大小）；
            min tx size=std:：min（min tx size，tx_大小）；
            总尺寸+=Tx尺寸；
        }

        Int64_t权重=0；
        如果（do_calculate_weight）
            权重=GetTransactionWeight（*Tx）；
            总重量+=重量；
        }

        if（do_calculate_sw&&tx->hasWitness（））
            +SWTXs；
            swtotal_size+=tx_大小；
            SWtotal_weight+=重量；
        }

        if（回路输入）
            camount tx_total_in=0；
            用于（const ctxin&in:tx->vin）
                ctransactionref-tx-in；
                uint256哈希块；
                如果（！）getTransaction（in.prevout.hash，tx_in，params（）.getconsensus（），hashblock，false））
                    throw jsonrpcerror（rpc_internal_error，std:：string（“意外的内部错误（Tx索引似乎已损坏）”）；
                }

                ctxout prevoutput=tx_in->vout[in.prevout.n]；

                tx_total_in+=上一个输出值；
                utxo-size_inc-=getserializesize（prevoutput，protocol_version）+每个utxo_开销；
            }

            camount txfee=tx_total_in-tx_total_out；
            断言（moneyrange（txfee））；
            如果（do-medianfee）
                fee_array.push_back（txfee）；
            }
            maxfee=std:：max（maxfee，txfee）；
            minfee=std:：min（minfee，txfee）；
            总费用+=txfee；

            //新feerate使用satoshis per virtual byte而不是per serialized byte
            camount feerate=重量？（txfee*见证人×比例系数）/重量：0；
            如果（做百分比）
                feerate_array.emplace_back（std：：make_pair（feerate，weight））；
            }
            max feerate=std:：max（max feerate，feerate）；
            min feerate=std:：min（min feerate，feerate）；
        }
    }

    camount feerate_percentiles[num_getblockstats_percentiles]=0_
    计算百分比重量（feerate_percentiles，feerate_array，total_weight）；

    单值函数（univavalue:：varr）；
    for（int64_t i=0；i<num_getblockstats_percentiles；i++）
        feerates.push_back（feerate_percentiles[i]）；
    }

    univalue ret_all（univalue:：vobj）；
    ret_all.pushkv（“avgfee”，（block.vtx.size（）>1）？totalfee/（block.vtx.size（）-1）：0）；
    ret_all.pushkv（“avgfeereate”，总重量？（totalfee*witness_scale_factor）/total_weight:0）；//单位：Sat/vByte
    ret_all.pushkv（“avgtxsize”，（block.vtx.size（）>1）？总尺寸/（block.vtx.size（）-1）：0）；
    ret_all.pushkv（“blockhash”，pindex->getblockhash（）.gethex（））；
    ret_all.pushkv（“feerate_percentiles”，feerates_res）；
    ret_all.pushkv（“高度”，（int64_t）pindex->nheight）；
    ret_all.pushkv（“ins”，输入）；
    ret_all.pushkv（“maxfee”，maxfee）；
    ret_all.pushkv（“maxfeerate”，maxfeerate）；
    ret_all.pushkv（“maxtxsize”，maxtxsize）；
    ret_all.pushkv（“medianfee”，calculated truncatedmedian（fee_array））；
    ret_all.pushkv（“mediantime”，pindex->getmediantimepast（））；
    ret_all.pushkv（“mediantxsize”，calculatetruncatedmedian（txsize_array））；
    ret_all.pushkv（“minfee”，（minfee==max_money）？0：明费）；
    ret-all.pushkv（“minfeerate”，（minfeerate==max\u money）？0：分钟）；
    ret_all.pushkv（“mintxsize”，mintxsize==max_block_serialized_size？？0：MimtxSead；
    ret_all.pushkv（“输出”，输出）；
    ret_all.pushkv（“补贴”，getBlock补贴（pindex->nheight，params（）.getconsensus（））；
    ret_all.pushkv（“swtotal_size”，swtotal_size）；
    ret_all.pushkv（“swtotal_weight”，swtotal_weight）；
    ret_all.pushkv（“SWTXS”，SWTXS）；
    ret_all.pushkv（“时间”，pindex->getBlockTime（））；
    ret_all.pushkv（“total_out”，total_out）；
    ret_all.pushkv（“总尺寸”，总尺寸）；
    ret_all.pushkv（“总重量”，总重量）；
    ret_all.pushkv（“totalfee”，totalfee）；
    ret_all.pushkv（“txs”，（int64_t）block.vtx.size（））；
    ret_all.pushkv（“utxo_increase”，输出-输入）；
    ret_all.pushkv（“utxo_size_inc”，utxo_size_inc）；

    如果（doyALL）{
        返回回复；
    }

    单值ret（单值：：vobj）；
    for（const std:：string&stat:stats）
        const univalue&value=ret_all[stat]；
        if（value.isNull（））
            throw jsonrpcerror（rpc_invalid_参数，strprintf（“invalid selected statistic%s”，stat））；
        }
        ret.pushkv（stat，value）；
    }
    返回RET；
}

静态单值savemempool（const jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 0）{
        throw std:：runtime_错误（
            rpchelpman“保存内存池”，
                \n将内存池复制到磁盘。在上一个转储完全加载之前，它将失败。\n“”
                toSTRIN（）+
            “\n实例：\n”
            +helpexamplecli（“savemempool”，“”）
            +helpexamplerpc（“savemempool”，“”）
        ；
    }

    如果（！）g_是_mempool_loaded）
        throw jsonrpcerror（rpc“misc”错误，“mempool尚未加载”）；
    }

    如果（！）dumpmempool（））
        throw jsonrpcerror（rpc_misc_error，“Unable to dump mempool to disk”）；
    }

    返回nullunivalue；
}

/ /！搜索给定的pubkey脚本集
bool findscriptpubkey（std:：atomic<int>&scan_progress，const std:：atomic<bool>&shouldaurt，int64_t&count，ccoinsviewcursor*cursor，const std:：set<cscript>&pines，std:：map<coutpoint，coin>out_results）
    扫描进度=0；
    计数＝0；
    while（cursor->valid（））
        关键点；
        硬币硬币；
        如果（！）光标->getkey（key）！cursor->getvalue（coin））返回false；
        如果（++计数%8192==0）
            boost:：this_thread:：interrupt_point（）；
            如果（应该中止）
                //允许通过abort引用中止扫描
                返回错误；
            }
        }
        如果（计数%256==0）
            //每256项更新进度引用
            uint32_t high=0x100**key.hash.begin（）+*（key.hash.begin（）+1）；
            扫描进度=（int）（高*100.0/65536.0+0.5）；
        }
        if（pines.count（coin.out.scriptpubkey））
            输出结果。安放（钥匙、硬币）；
        }
        光标> NEXT（）；
    }
    扫描进度=100；
    回归真实；
}

/**raii对象防止扫描txout集时出现并发问题*/

static std::mutex g_utxosetscan;
static std::atomic<int> g_scan_progress;
static std::atomic<bool> g_scan_in_progress;
static std::atomic<bool> g_should_abort_scan;
class CoinsViewScanReserver
{
private:
    bool m_could_reserve;
public:
    explicit CoinsViewScanReserver() : m_could_reserve(false) {}

    bool reserve() {
        assert (!m_could_reserve);
        std::lock_guard<std::mutex> lock(g_utxosetscan);
        if (g_scan_in_progress) {
            return false;
        }
        g_scan_in_progress = true;
        m_could_reserve = true;
        return true;
    }

    ~CoinsViewScanReserver() {
        if (m_could_reserve) {
            std::lock_guard<std::mutex> lock(g_utxosetscan);
            g_scan_in_progress = false;
        }
    }
};

UniValue scantxoutset(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            RPCHelpMan{"scantxoutset",
                "\nEXPERIMENTAL warning: this call may be removed or changed in future releases.\n"
                "\nScans the unspent transaction output set for entries that match certain output descriptors.\n"
                "Examples of output descriptors are:\n"
                "    addr(<address>)                      Outputs whose scriptPubKey corresponds to the specified address (does not include P2PK)\n"
                "    raw(<hex script>)                    Outputs whose scriptPubKey equals the specified hex scripts\n"
                "    combo(<pubkey>)                      P2PK, P2PKH, P2WPKH, and P2SH-P2WPKH outputs for the given pubkey\n"
                "    pkh(<pubkey>)                        P2PKH outputs for the given pubkey\n"
                "    sh(multi(<n>,<pubkey>,<pubkey>,...)) P2SH-multisig outputs for the given threshold and pubkeys\n"
                "\nIn the above, <pubkey> either refers to a fixed public key in hexadecimal notation, or to an xpub/xprv optionally followed by one\n"
                /*更多路径元素由\“/\”分隔，可选以\“/*\”（未硬化）或\“/*'\”或\“/*H\”（硬化）结尾，以指定所有\n
                “未硬化或硬化的子密钥。\n”
                在后一种情况下，如果与1000不同，则需要在下面指定一个范围。\n
                “有关输出描述符的详细信息，请参阅doc/descriptors.md文件中的文档。\n”，
                {
                    “action”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The action to execute\n"

            "                                      \"start\" for starting a scan\n"
            "                                      \"abort\" for aborting the current scan (returns true when abort was successful)\n"
            "                                      \"status\" for progress report (in %) of the current scan"},
                    /*canObjects“，rpcarg:：type:：arr，/*opt*/false，/*default_val*/”“，“扫描对象数组\n”
            “每个扫描对象都是字符串描述符或对象：”，
                        {
                            “描述符”，rpcarg:：type:：str，/*opt*/ true, /* default_val */ "", "An output descriptor"},

                            /*，rpcarg:：type:：obj，/*opt*/true，/*default_val*/”“，“带有输出描述符和元数据的对象”，
                                {
                                    “desc”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "An output descriptor"},

                                    /*ange“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”1000“，最多可浏览哪个子索引hd链”，
                                }
                            }
                        }
                        “[扫描对象…]”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            “未使用的”：[\n”
            “{n”
            “txid\”：“transaction id\”，（string）事务ID \n”
            “vout”：n，（数字）vout值\n”
            “scriptpubkey\”：\“script\”，（string）脚本键\n”
            “desc\”：“descriptor\”，（string）匹配的scriptpubkey的专用描述符\n”
            “amount”：x.x x x，（数字）未用输出的以“+货币单位+”表示的总金额\n”
            “高度”：n，（数字）未暂停事务输出的高度\n”
            “}\n”
            “，……”
            “\”总金额\“：x.x x x，（数字）以“+货币\单位+”表示的所有未使用输出的总金额\n”
            “\n”
        ；

    rpctypecheck（request.params，univalue:：vstr，univalue:：varr）；

    单值结果（单值：：vobj）；
    if（request.params[0].get_str（）=“状态”）
        coinsviewscanreserver reserver；
        if（reserver.reserve（））
            //没有正在进行的扫描
            返回nullunivalue；
        }
        结果.pushkv（“进度”，g_scan_progress）；
        返回结果；
    else if（request.params[0].get_str（）==“abort”）
        coinsviewscanreserver reserver；
        if（reserver.reserve（））
            //可以保留，这意味着没有运行扫描
            返回错误；
        }
        //设置abort标志
        G_应_中止_扫描=真；
        回归真实；
    else if（request.params[0].get_str（）==“开始”）
        coinsviewscanreserver reserver；
        如果（！）reserver.reserve（））
            throw jsonrpcerror（rpc_invalid_参数，“scan already in progress，use action \”abort \“or \”status \“”）；
        }
        std:：set<cscript>针；
        std:：map<cscript，std:：string>描述符；
        camount total_in=0；

        //循环扫描对象
        for（const univalue&scanObject:request.params[1].get_array（）.getValues（））
            std：：字符串描述字符串；
            int范围=1000；
            if（scanObject.isstr（））
                desc_str=scanObject.get_str（）；
            else if（scanObject.isObject（））
                uni value desc_uni=查找值（scanObject，“desc”）；
                if（desc_uni.isnull（））throw jsonrpcerror（rpc_invalid_参数，“需要在扫描对象中提供描述符”）；
                desc_str=desc_uni.get_str（）；
                uni value range_uni=find_value（scanObject，“range”）；
                如果（！）范围“uni.isNull（））”
                    range=range_uni.get_int（）；
                    if（range<0_range>1000000）throw jsonrpcerror（rpc_invalid_parameter，“range out of range”）；
                }
            }否则{
                throw jsonrpcerror（rpc_invalid_参数，“扫描对象必须是字符串或对象”）；
            }

            FlatsigningProvider提供程序；
            auto desc=解析（desc_str，provider）；
            如果（！）DESC）{
                throw jsonrpcerror（rpc_invalid_address_or_key，strprintf（“无效描述符'%s'”，desc_str））；
            }
            如果（！）desc->isrange（））范围=0；
            对于（int i=0；i<=range；++i）
                std:：vector<cscript>脚本；
                如果（！）desc->expand（i，provider，scripts，provider））
                    throw jsonrpcerror（rpc_invalid_address_or_key，strprintf（“没有私钥，无法派生脚本：”%s“，desc_str））；
                }
                用于（const auto&script:scripts）
                    std：：string推理=inferDescriptor（script，provider）->toString（）；
                    针。安放（脚本）；
                    descriptors.emplace（std:：move（脚本），std:：move（推断））；
                }
            }
        }

        //扫描未暂停的事务输出集的输入
        未使用的单值（单值：：varr）；
        std:：vector<ctxout>input_Txos；
        std:：map<coutpoint，coin>coins；
        G_应_中止_扫描=假；
        G_scan_progress=0；
        Int64计数=0；
        std:：unique_ptr<ccoinsviewcursor>pcursor；
        {
            锁（CSKEMAN）；
            flushstatetodisk（）；
            pcursor=std:：unique_ptr<ccoinsViewCursor>（pcoinsDBView->Cursor（））；
            断言（pcursor）；
        }
        bool res=findscriptpubkey（g_scan_progress，g_应中止_scan，count，pcursor.get（），针，硬币）；
        结果.pushkv（“成功”，res）；
        结果.pushkv（“搜索项”，计数）；

        用于（const auto&it:硬币）
            const coutpoint&outpoint=it.first；
            const coin&coin=it.second；
            const ctxout&txo=coin.out；
            输入txos.push-back（txo）；
            总_in+=txo.n值；

            未使用的单值（单值：：vobj）；
            unpent.pushkv（“txid”，outpoint.hash.gethex（））；
            未使用的pushkv（“vout”，（int32_t）outpoint.n）；
            unpent.pushkv（“scriptpubkey”，hexstr（txo.scriptpubkey.begin（），txo.scriptpubkey.end（））；
            unspent.pushkv（“desc”，描述符[txo.scriptpubkey]）；
            未使用的pushkv（“金额”，valuefromamount（txo.nvalue））；
            未使用的pushkv（“高度”，（int32-t）coin.nheight）；

            松开。向后推（未松开）；
        }
        结果.pushkv（“未使用”，未使用）；
        result.pushkv（“总金额”，valuefromamount（总金额））；
    }否则{
        throw jsonrpcerror（rpc_invalid_参数，“无效命令”）；
    }
    返回结果；
}

//关闭clang格式
静态const crpccommand命令[]
//类别名称actor（function）argnames
  ///————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    “区块链”，“获取区块链信息”，&getblockchaininfo，，
    “区块链”，“getchaintxstats”，&getchaintxstats，“nblocks”，“blockhash”，
    “区块链”，“GetBlockStats”，&GetBlockStats，“hash_or_height”，“stats”，
    “区块链”，“GetBestBlockHash”，&GetBestBlockHash，，
    “区块链”，“GetBlockCount”，&GetBlockCount，，
    “区块链”，“GetBlock”，&GetBlock，“BlockHash”，“详细详细”，
    “区块链”，“GetBlockHash”，&GetBlockHash，“高度”，
    “区块链”，“GetBlockHeader”，&GetBlockHeader，“BlockHash”，“详细”，
    “区块链”，“getchaintips”，&getchaintips，，
    “区块链”、“获取难度”&获取难度，，
    “区块链”，“GetMemPooolancestors”，&GetMemPooolancestors，“txid”，“详细”，
    “区块链”，“getmempooldescendants”，&getmempooldescendants，“txid”，“verbose”，
    “区块链”，“getmempoolentry”，&getmempoolentry，“txid”，
    “区块链”，“getmempoolinfo”，&getmempoolinfo，，
    “区块链”，“getrawmupool”，&getrawmupool，“详细”，
    “区块链”，“getXout”，&getXout，“txid”，“n”，“include，
    “区块链”，“getXoutsetInfo”，&getXoutsetInfo，，
    “区块链”，“pruneblockchain”，&pruneblockchain，“height”，
    “区块链”，“存储内存池”，&savemempool，，
    “区块链”、“verifychain”、&verifychain、“checklevel”、“nblocks”，

    “区块链”，“PreciousBlock”，&PreciousBlock，“BlockHash”，
    “区块链”、“scantStartet”、&scantStartet、“操作”、“scanObjects”，

    /*未在帮助中显示*/

    { "hidden",             "invalidateblock",        &invalidateblock,        {"blockhash"} },
    { "hidden",             "reconsiderblock",        &reconsiderblock,        {"blockhash"} },
    { "hidden",             "waitfornewblock",        &waitfornewblock,        {"timeout"} },
    { "hidden",             "waitforblock",           &waitforblock,           {"blockhash","timeout"} },
    { "hidden",             "waitforblockheight",     &waitforblockheight,     {"height","timeout"} },
    { "hidden",             "syncwithvalidationinterfacequeue", &syncwithvalidationinterfacequeue, {} },
};
//CLAN格式

void RegisterBlockchainRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
