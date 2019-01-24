
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

//注意：此文件旨在由最终用户自定义，并且仅包括本地节点策略逻辑

#include <policy/policy.h>

#include <consensus/validation.h>
#include <validation.h>
#include <coins.h>
#include <tinyformat.h>
#include <util/system.h>
#include <util/strencodings.h>


CAmount GetDustThreshold(const CTxOut& txout, const CFeeRate& dustRelayFeeIn)
{
//“粉尘”是指粉尘释放量，
//单位为每千字节饱和。
//如果你支付的费用超过产出的价值
//花点什么，我们就认为是灰尘。
//一个典型的可消费的非segwit txout是34字节大，将
//需要至少148字节的ctxin来花费：
//所以灰尘是一种可消费的物质
//182*粉尘排放量每1000个（在Satoshis）。
//546 Satoshis，默认速率为3000 Sat/kb。
//一个典型的可消费segwit txout是31字节大，并且将
//需要至少67字节的ctxin来花费：
//所以灰尘是一种可消费的物质
//98*粉尘排放量每1000个（在Satoshis）。
//294 Satoshis，默认速率为3000 Sat/kb。
    if (txout.scriptPubKey.IsUnspendable())
        return 0;

    size_t nSize = GetSerializeSize(txout);
    int witnessversion = 0;
    std::vector<unsigned char> witnessprogram;

    if (txout.scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
//求和事务输入部分的大小
//75%的Segwit折扣适用于脚本大小。
        nSize += (32 + 4 + 1 + (107 / WITNESS_SCALE_FACTOR) + 4);
    } else {
nSize += (32 + 4 + 1 + 107 + 4); //上述148条
    }

    return dustRelayFeeIn.GetFee(nSize);
}

bool IsDust(const CTxOut& txout, const CFeeRate& dustRelayFeeIn)
{
    return (txout.nValue < GetDustThreshold(txout, dustRelayFeeIn));
}

bool IsStandard(const CScript& scriptPubKey, txnouttype& whichType)
{
    std::vector<std::vector<unsigned char> > vSolutions;
    whichType = Solver(scriptPubKey, vSolutions);

    if (whichType == TX_NONSTANDARD || whichType == TX_WITNESS_UNKNOWN) {
        return false;
    } else if (whichType == TX_MULTISIG) {
        unsigned char m = vSolutions.front()[0];
        unsigned char n = vSolutions.back()[0];
//标准支持最多3个多功能TXN中的X个
        if (n < 1 || n > 3)
            return false;
        if (m < 1 || m > n)
            return false;
    } else if (whichType == TX_NULL_DATA &&
               (!fAcceptDatacarrier || scriptPubKey.size() > nMaxDatacarrierBytes)) {
          return false;
    }

    return true;
}

bool IsStandardTx(const CTransaction& tx, std::string& reason)
{
    if (tx.nVersion > CTransaction::MAX_STANDARD_VERSION || tx.nVersion < 1) {
        reason = "version";
        return false;
    }

//具有大量输入的超大事务可能会使网络付出代价
//几乎和处理费用一样多，因为
//计算签名散列为o（ninputs*txsize）。限制交易
//为了最大限度地减少CPU耗尽攻击，标准的Tx重量。
    unsigned int sz = GetTransactionWeight(tx);
    if (sz > MAX_STANDARD_TX_WEIGHT) {
        reason = "tx-size";
        return false;
    }

    for (const CTxIn& txin : tx.vin)
    {
//最大的“标准”txin是一个15/15的p2sh多图像压缩
//键（记住redeemscript大小的520字节限制）。那作品
//输出到A（15*（33+1））+3=513字节的redeemscript，513+1+15*（73+1）+3=1627
//scriptsig的字节数，我们将一些次要的字节数四舍五入到1650字节。
//未来校对。这也足够花20分之20的时间
//检查multisig scriptpubkey，尽管这样的scriptpubkey不是
//考虑的标准。
        if (txin.scriptSig.size() > 1650) {
            reason = "scriptsig-size";
            return false;
        }
        if (!txin.scriptSig.IsPushOnly()) {
            reason = "scriptsig-not-pushonly";
            return false;
        }
    }

    unsigned int nDataOut = 0;
    txnouttype whichType;
    for (const CTxOut& txout : tx.vout) {
        if (!::IsStandard(txout.scriptPubKey, whichType)) {
            reason = "scriptpubkey";
            return false;
        }

        if (whichType == TX_NULL_DATA)
            nDataOut++;
        else if ((whichType == TX_MULTISIG) && (!fIsBareMultisigStd)) {
            reason = "bare-multisig";
            return false;
        } else if (IsDust(txout, ::dustRelayFee)) {
            reason = "dust";
            return false;
        }
    }

//只允许一个操作返回txout
    if (nDataOut > 1) {
        reason = "multi-op-return";
        return false;
    }

    return true;
}

/*
 *检查事务输入以减轻两个
 *潜在的拒绝服务攻击：
 *
 * 1。在脚本中插入额外的数据，
 *不由scriptpubkey（或p2sh脚本）使用
 * 2。p2sh脚本价格昂贵
 *检查信号/检查多信号操作
 *
 *为什么要麻烦？为了避免拒绝服务攻击；攻击者
 *可以提交标准哈希…对等交易，
 *将被接受为块。救赎
 *脚本可以是任何东西；攻击者可以使用
 *检查赎回脚本的成本很高，例如：
 *dup checksig放置…重复100次…OPY1
 **/

bool AreInputsStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs)
{
    if (tx.IsCoinBase())
return true; //CoinBase通常不使用VIN

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const CTxOut& prev = mapInputs.AccessCoin(tx.vin[i].prevout).out;

        std::vector<std::vector<unsigned char> > vSolutions;
        txnouttype whichType = Solver(prev.scriptPubKey, vSolutions);
        if (whichType == TX_NONSTANDARD) {
            return false;
        } else if (whichType == TX_SCRIPTHASH) {
            std::vector<std::vector<unsigned char> > stack;
//将scriptsig转换为堆栈，这样我们可以检查redeemscript
            if (!EvalScript(stack, tx.vin[i].scriptSig, SCRIPT_VERIFY_NONE, BaseSignatureChecker(), SigVersion::BASE))
                return false;
            if (stack.empty())
                return false;
            CScript subscript(stack.back().begin(), stack.back().end());
            if (subscript.GetSigOpCount(true) > MAX_P2SH_SIGOPS) {
                return false;
            }
        }
    }

    return true;
}

bool IsWitnessStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs)
{
    if (tx.IsCoinBase())
return true; //忽略了coinbase

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
//我们不关心此输入的见证是否为空，因为它不能膨胀。
//如果脚本在没有见证的情况下无效，那么在验证过程中，它迟早会被捕获。
        if (tx.vin[i].scriptWitness.IsNull())
            continue;

        const CTxOut &prev = mapInputs.AccessCoin(tx.vin[i].prevout).out;

//获取与此输入对应的scriptPubKey:
        CScript prevScript = prev.scriptPubKey;

        if (prevScript.IsPayToScriptHash()) {
            std::vector <std::vector<unsigned char> > stack;
//如果scriptpubkey是p2sh，我们尝试通过转换scriptsig随意提取redeemscript
//成堆。我们不检查ispushonly，也不比较哈希，因为稍后将进行这些操作。
//如果检查在这个阶段失败，我们知道这个txid肯定是一个坏的。
            if (!EvalScript(stack, tx.vin[i].scriptSig, SCRIPT_VERIFY_NONE, BaseSignatureChecker(), SigVersion::BASE))
                return false;
            if (stack.empty())
                return false;
            prevScript = CScript(stack.back().begin(), stack.back().end());
        }

        int witnessversion = 0;
        std::vector<unsigned char> witnessprogram;

//非证人程序不得与任何证人关联。
        if (!prevScript.IsWitnessProgram(witnessversion, witnessprogram))
            return false;

//检查P2WSH标准限值
        if (witnessversion == 0 && witnessprogram.size() == WITNESS_V0_SCRIPTHASH_SIZE) {
            if (tx.vin[i].scriptWitness.stack.back().size() > MAX_STANDARD_P2WSH_SCRIPT_SIZE)
                return false;
            size_t sizeWitnessStack = tx.vin[i].scriptWitness.stack.size() - 1;
            if (sizeWitnessStack > MAX_STANDARD_P2WSH_STACK_ITEMS)
                return false;
            for (unsigned int j = 0; j < sizeWitnessStack; j++) {
                if (tx.vin[i].scriptWitness.stack[j].size() > MAX_STANDARD_P2WSH_STACK_ITEM_SIZE)
                    return false;
            }
        }
    }
    return true;
}

CFeeRate incrementalRelayFee = CFeeRate(DEFAULT_INCREMENTAL_RELAY_FEE);
CFeeRate dustRelayFee = CFeeRate(DUST_RELAY_TX_FEE);
unsigned int nBytesPerSigOp = DEFAULT_BYTES_PER_SIGOP;

int64_t GetVirtualTransactionSize(int64_t nWeight, int64_t nSigOpCost)
{
    return (std::max(nWeight, nSigOpCost * nBytesPerSigOp) + WITNESS_SCALE_FACTOR - 1) / WITNESS_SCALE_FACTOR;
}

int64_t GetVirtualTransactionSize(const CTransaction& tx, int64_t nSigOpCost)
{
    return GetVirtualTransactionSize(GetTransactionWeight(tx), nSigOpCost);
}

int64_t GetVirtualTransactionInputSize(const CTxIn& txin, int64_t nSigOpCost)
{
    return GetVirtualTransactionSize(GetTransactionInputWeight(txin), nSigOpCost);
}
