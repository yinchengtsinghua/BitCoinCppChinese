
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

#include <wallet/fees.h>

#include <policy/policy.h>
#include <txmempool.h>
#include <util/system.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>


CAmount GetRequiredFee(const CWallet& wallet, unsigned int nTxBytes)
{
    return GetRequiredFeeRate(wallet).GetFee(nTxBytes);
}


CAmount GetMinimumFee(const CWallet& wallet, unsigned int nTxBytes, const CCoinControl& coin_control, const CTxMemPool& pool, const CBlockPolicyEstimator& estimator, FeeCalculation* feeCalc)
{
    CAmount fee_needed = GetMinimumFeeRate(wallet, coin_control, pool, estimator, feeCalc).GetFee(nTxBytes);
//始终遵守最大限度
    if (fee_needed > maxTxFee) {
        fee_needed = maxTxFee;
        if (feeCalc) feeCalc->reason = FeeReason::MAXTXFEE;
    }
    return fee_needed;
}

CFeeRate GetRequiredFeeRate(const CWallet& wallet)
{
    return std::max(wallet.m_min_fee, ::minRelayTxFee);
}

CFeeRate GetMinimumFeeRate(const CWallet& wallet, const CCoinControl& coin_control, const CTxMemPool& pool, const CBlockPolicyEstimator& estimator, FeeCalculation* feeCalc)
{
    /*如何计算费用的用户控制使用以下参数优先级：
       1。硬币控制
       2。硬币控制M确认目标
       三。m_pay_tx_fee（钱包的用户设置成员变量）
       4。m_confirm_target（钱包的用户设置成员变量）
       使用设置的第一个参数。
    **/

    CFeeRate feerate_needed;
if (coin_control.m_feerate) { //1。
        feerate_needed = *(coin_control.m_feerate);
        if (feeCalc) feeCalc->reason = FeeReason::PAYTXFEE;
//允许覆盖自动最小/最大检查硬币控制实例
        if (coin_control.fOverrideFeeRate) return feerate_needed;
    }
else if (!coin_control.m_confirm_target && wallet.m_pay_tx_fee != CFeeRate(0)) { //三。TODO:删除钱包成员m_pay_tx_fee的魔法值0
        feerate_needed = wallet.m_pay_tx_fee;
        if (feeCalc) feeCalc->reason = FeeReason::PAYTXFEE;
    }
else { //2。或4。
//我们将使用智能费用估算
        unsigned int target = coin_control.m_confirm_target ? *coin_control.m_confirm_target : wallet.m_confirm_target;
//默认情况下，估计值是经济的，如果我们发出选择加入RBF的信号
        bool conservative_estimate = !coin_control.m_signal_bip125_rbf.get_value_or(wallet.m_signal_rbf);
//允许覆盖CoinControl实例上的默认费用估算模式
        if (coin_control.m_fee_mode == FeeEstimateMode::CONSERVATIVE) conservative_estimate = true;
        else if (coin_control.m_fee_mode == FeeEstimateMode::ECONOMICAL) conservative_estimate = false;

        feerate_needed = estimator.estimateSmartFee(target, feeCalc, conservative_estimate);
        if (feerate_needed == CFeeRate(0)) {
//如果我们没有足够的数据来进行估算，请使用回退费用
            feerate_needed = wallet.m_fallback_fee;
            if (feeCalc) feeCalc->reason = FeeReason::FALLBACK;

//如果禁用回退费用，则直接返回（feerate 0==disabled）
            if (wallet.m_fallback_fee == CFeeRate(0)) return feerate_needed;
        }
//使用智能费用估算时遵守Mempool最低费用
        CFeeRate min_mempool_feerate = pool.GetMinFee(gArgs.GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000);
        if (feerate_needed < min_mempool_feerate) {
            feerate_needed = min_mempool_feerate;
            if (feeCalc) feeCalc->reason = FeeReason::MEMPOOL_MIN;
        }
    }

//阻止用户支付低于所需费率的费用
    CFeeRate required_feerate = GetRequiredFeeRate(wallet);
    if (required_feerate > feerate_needed) {
        feerate_needed = required_feerate;
        if (feeCalc) feeCalc->reason = FeeReason::REQUIRED;
    }
    return feerate_needed;
}

CFeeRate GetDiscardRate(const CWallet& wallet, const CBlockPolicyEstimator& estimator)
{
    unsigned int highest_target = estimator.HighestTargetTracked(FeeEstimateHorizon::LONG_HALFLIFE);
    /*erate discard_rate=estimator.estimatesmartfee（最高_目标，nullptr/*feecalculation*/，false/*conservative*/）；
    //如果我们得到有效的费用估算，不要让丢弃率大于最长可能的费用估算
    丢弃率=（丢弃率==cfeerate（0））？wallet.m_丢弃率：std:：min（丢弃率，wallet.m_丢弃率）；
    //丢弃率必须至少为DustRelayFee
    丢弃率=std：：max（丢弃率，：：dustRelayFee）；
    退货报废率；
}
