
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

#include <consensus/validation.h>
#include <interfaces/chain.h>
#include <wallet/coincontrol.h>
#include <wallet/feebumper.h>
#include <wallet/fees.h>
#include <wallet/wallet.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <policy/rbf.h>
#include <validation.h> //用于mempool访问
#include <txmempool.h>
#include <util/moneystr.h>
#include <util/system.h>
#include <net.h>

//！检查交易是否在wallet或mempool中有后代，或
//！挖掘的，或与挖掘的事务冲突的。返回feebumper:：result。
static feebumper::Result PreconditionChecks(interfaces::Chain::Lock& locked_chain, const CWallet* wallet, const CWalletTx& wtx, std::vector<std::string>& errors) EXCLUSIVE_LOCKS_REQUIRED(wallet->cs_wallet)
{
    if (wallet->HasWalletSpend(wtx.GetHash())) {
        errors.push_back("Transaction has descendants in the wallet");
        return feebumper::Result::INVALID_PARAMETER;
    }

    {
        LOCK(mempool.cs);
        auto it_mp = mempool.mapTx.find(wtx.GetHash());
        if (it_mp != mempool.mapTx.end() && it_mp->GetCountWithDescendants() > 1) {
            errors.push_back("Transaction has descendants in the mempool");
            return feebumper::Result::INVALID_PARAMETER;
        }
    }

    if (wtx.GetDepthInMainChain(locked_chain) != 0) {
        errors.push_back("Transaction has been mined, or is conflicted with a mined transaction");
        return feebumper::Result::WALLET_ERROR;
    }

    if (!SignalsOptInRBF(*wtx.tx)) {
        errors.push_back("Transaction is not BIP 125 replaceable");
        return feebumper::Result::WALLET_ERROR;
    }

    if (wtx.mapValue.count("replaced_by_txid")) {
        errors.push_back(strprintf("Cannot bump transaction %s which was already bumped by transaction %s", wtx.GetHash().ToString(), wtx.mapValue.at("replaced_by_txid")));
        return feebumper::Result::WALLET_ERROR;
    }

//检查原始Tx是否完全由我们的输入组成
//如果没有，我们不能取消费用，因为钱包无法知道其他投入的价值（因此费用）
    if (!wallet->IsAllFromMe(*wtx.tx, ISMINE_SPENDABLE)) {
        errors.push_back("Transaction contains inputs that don't belong to this wallet");
        return feebumper::Result::WALLET_ERROR;
    }


    return feebumper::Result::OK;
}

namespace feebumper {

bool TransactionCanBeBumped(const CWallet* wallet, const uint256& txid)
{
    auto locked_chain = wallet->chain().lock();
    LOCK(wallet->cs_wallet);
    const CWalletTx* wtx = wallet->GetWalletTx(txid);
    if (wtx == nullptr) return false;

    std::vector<std::string> errors_dummy;
    feebumper::Result res = PreconditionChecks(*locked_chain, wallet, *wtx, errors_dummy);
    return res == feebumper::Result::OK;
}

Result CreateTransaction(const CWallet* wallet, const uint256& txid, const CCoinControl& coin_control, CAmount total_fee, std::vector<std::string>& errors,
                         CAmount& old_fee, CAmount& new_fee, CMutableTransaction& mtx)
{
    auto locked_chain = wallet->chain().lock();
    LOCK(wallet->cs_wallet);
    errors.clear();
    auto it = wallet->mapWallet.find(txid);
    if (it == wallet->mapWallet.end()) {
        errors.push_back("Invalid or non-wallet transaction id");
        return Result::INVALID_ADDRESS_OR_KEY;
    }
    const CWalletTx& wtx = it->second;

    Result result = PreconditionChecks(*locked_chain, wallet, wtx, errors);
    if (result != Result::OK) {
        return result;
    }

//找出哪些输出发生了变化
//如果没有更改输出或多个更改输出，则失败
    int nOutput = -1;
    for (size_t i = 0; i < wtx.tx->vout.size(); ++i) {
        if (wallet->IsChange(wtx.tx->vout[i])) {
            if (nOutput != -1) {
                errors.push_back("Transaction has multiple change outputs");
                return Result::WALLET_ERROR;
            }
            nOutput = i;
        }
    }
    if (nOutput == -1) {
        errors.push_back("Transaction does not have a change output");
        return Result::WALLET_ERROR;
    }

//计算新事务的预期大小。
    int64_t txSize = GetVirtualTransactionSize(*(wtx.tx));
    const int64_t maxNewTxSize = CalculateMaximumSignedTxSize(*wtx.tx, wallet);
    if (maxNewTxSize < 0) {
        errors.push_back("Transaction contains inputs that cannot be signed");
        return Result::INVALID_ADDRESS_OR_KEY;
    }

//计算旧费用和费率
    old_fee = wtx.GetDebit(ISMINE_SPENDABLE) - wtx.tx->GetValueOut();
    CFeeRate nOldFeeRate(old_fee, txSize);
    CFeeRate nNewFeeRate;
//钱包使用保守的钱包\增量\中继费价值
//防止增量中继网络范围策略变化的未来证据
//我们的节点可能不知道的费用。
    CFeeRate walletIncrementalRelayFee = CFeeRate(WALLET_INCREMENTAL_RELAY_FEE);
    if (::incrementalRelayFee > walletIncrementalRelayFee) {
        walletIncrementalRelayFee = ::incrementalRelayFee;
    }

    if (total_fee > 0) {
        CAmount minTotalFee = nOldFeeRate.GetFee(maxNewTxSize) + ::incrementalRelayFee.GetFee(maxNewTxSize);
        if (total_fee < minTotalFee) {
            errors.push_back(strprintf("Insufficient totalFee, must be at least %s (oldFee %s + incrementalFee %s)",
                                                                FormatMoney(minTotalFee), FormatMoney(nOldFeeRate.GetFee(maxNewTxSize)), FormatMoney(::incrementalRelayFee.GetFee(maxNewTxSize))));
            return Result::INVALID_PARAMETER;
        }
        CAmount requiredFee = GetRequiredFee(*wallet, maxNewTxSize);
        if (total_fee < requiredFee) {
            errors.push_back(strprintf("Insufficient totalFee (cannot be less than required fee %s)",
                                                                FormatMoney(requiredFee)));
            return Result::INVALID_PARAMETER;
        }
        new_fee = total_fee;
        nNewFeeRate = CFeeRate(total_fee, maxNewTxSize);
    } else {
        /*_fee=getminimumFee（*wallet，maxnewtxsize，coin_control，mempool，：：feeestimator，nullptr/*feecalculation*/）；
        nnewfeereate=cfeerate（新费用，最大新x尺寸）；

        //新费率必须至少为旧费率+最小增量中继费率
        //walletincrementalrelayfee.getfeeperk（）应该是精确的，因为它已初始化
        //单位为（每KB收费）。
        //但是，noldfeereate是根据tx费用/规模计算的值，因此
        //在结果中添加1个Satoshi，因为它可能已被取整。
        if（nnewfeereate.getfeeperk（）<noldfeereate.getfeeperk（）+1+walletincrementalrelayfee.getfeeperk（））
            nnewfeereate=cfeerate（noldfeereate.getfeeperk（）+1+walletincrementalrelayfee.getfeeperk（））；
            新费用=nnewfeereate.getfee（maxnewtxsize）；
        }
    }

    //检查在所有情况下，新费用不会违反maxtxfee
     如果（新费用>最大费用）
         错误。向后推（strprintf（“指定或计算的费用%s太高（不能高于maxtxfee%s）”，
                               格式化货币（新的_费），格式化货币（最大的txtfee））；
         返回结果：钱包出错；
     }

    //检查费率是否高于mempool的最低费用
    /（如果我们知道新的Tx将不会被Mempool接受，那么不必增加费用）
    //如果用户设置totalfee或paytxfee过低、fallbackfee过低或
    //在少数情况下，由于费用估算仅为
    //早些时候。在这种情况下，我们向用户报告一个错误，用户可以使用总费用进行调整。
    cfeerate minmempoolfeereate=mempool.getminfee（gargs.getarg（“-max mempool”，默认的最大内存池大小）*1000000）；
    if（nnewfeereate.getfeeperk（）<minmempoolfeereate.getfeeperk（））
        错误。向后推（strprintf（
            “新费率（%s）低于进入mempool的最低费率（%s--”
            “要添加事务，totalfee值应至少为%s或settxfee值应至少为%s”，
            格式货币（nnewfeereate.getfeeperk（）），
            格式货币（minmempoolferate.getfeeperk（）），
            格式货币（minmempoolferate.getfee（maxnewtxsize）），
            格式货币（minmempoolferate.getfeeperk（））；
        返回结果：钱包出错；
    }

    //现在修改输出以增加费用。
    //如果输出不足以支付费用，则失败。
    camount ndelta=新费用-旧费用；
    断言（ndelta>0）；
    mtx=cmutabletransaction*wtx.tx
    ctxout*poutput=&（mtx.vout[noutput]）；
    if（poutput->nvalue<ndelta）
        错误。推回（“更改输出太小，无法提高费用”）；
        返回结果：钱包出错；
    }

    //如果输出变成灰尘，则丢弃它（将灰尘转换为费用）
    poutput->nvalue-=ndelta；
    如果（poutput->nvalue<=getdustThreshold（*poutput，getDiscardRate（*wallet，：：feeestimator）））；
        wallet->walletlogprintf（“颠簸费和弃灰输出”）；
        新的费用+=poutput->nvalue；
        mtx.vout.erase（mtx.vout.begin（）+noutput）；
    }

    //如果需要，将新的Tx标记为不可替换。
    如果（！）coin_control.m_signal_bip125_rbf.get_value_or（wallet->m_signal_rbf））
        用于（自动和输入：mtx.vin）
            如果（input.nsequence<0xfffffe），input.nsequence=0xfffffffe；
        }
    }


    返回结果：OK；
}

bool signtransaction（cwallet*钱包，cmutabletransaction&mtx）
    自动锁定的_chain=wallet->chain（）.lock（）；
    锁（钱包->cs_钱包）；
    归还钱包->签名交易（MTX）；
}

结果提交事务（cwallet*wallet，const uint256&txid，cmutabletransaction&mtx，std:：vector<std:：string>&errors，uint256&bumped_Txid）
{
    自动锁定的_chain=wallet->chain（）.lock（）；
    锁（钱包->cs_钱包）；
    如果（！）errors.empty（））
        返回结果：其他错误；
    }
    auto it=txid.isNull（）？wallet->mapwallet.end（）：wallet->mapwallet.find（txid）；
    if（it==wallet->mapwallet.end（））
        错误。推后（“无效或非钱包交易ID”）；
        返回结果：其他错误；
    }
    cwallettx&oldwtx=it->second；

    //确保事务仍没有子代，并且在此期间未被挖掘
    结果结果=预处理检查（*锁定的_链、钱包、oldwtx、错误）；
    如果（结果）！=结果：OK）{
        返回结果；
    }

    //提交/广播Tx
    ctransactionref tx=makeTransactionRef（std:：move（mtx））；
    mapvalue_t mapvalue=oldwtx.mapvalue；
    mapvalue[“replaces_txid”]=oldwtx.gethash（）.toString（）；

    Creservekey ReserveKey（钱包）；
    cvalidationState状态；
    如果（！）wallet->commitTransaction（tx，std:：move（mapvalue），oldwtx.vorderform，reservekey，g_connman.get（），state））；
        //注意：commitTransaction从不返回false，因此不应发生这种情况。
        errors.push_back（strprintf（“事务被拒绝：%s”，formatStateMessage（state）））；
        返回结果：钱包出错；
    }

    bumped_txid=tx->gethash（）；
    if（state.isinvalid（））
        //如果mempool拒绝事务，则可能发生这种情况。报告
        //在“错误”响应中发生了什么。
        error s.push_back（strprintf（“错误：事务被拒绝：%s”，formatStateMessage（state）））；
    }

    //将原始Tx标记为已缓冲
    如果（！）wallet->markreplaced（oldwtx.gethash（），bumped_txid））
        //TODO:查看json-rpc是否具有返回响应的标准方法
        //还有一个异常。最好是返回有关
        //wtxpumped到调用方，即使标记原始事务
        //由于某种原因，替换不成功。
        errors.push_back（“创建新的bumpfee交易，但无法将原始交易标记为已替换”）；
    }
    返回结果：OK；
}

//命名空间feebumper
