
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_WALLET_COINCONTROL_H
#define BITCOIN_WALLET_COINCONTROL_H

#include <policy/feerate.h>
#include <policy/fees.h>
#include <primitives/transaction.h>
#include <wallet/wallet.h>

#include <boost/optional.hpp>

/*硬币控制功能。*/
class CCoinControl
{
public:
//！自定义更改目标，如果未设置，则生成地址
    CTxDestination destChange;
//！如果设置了，则重写默认更改类型；如果设置了DestChange，则忽略默认更改类型
    boost::optional<OutputType> m_change_type;
//！如果为false，则允许未选择的输入，但需要使用所有选定的输入
    bool fAllowOtherInputs;
//！包括可解决的仅监视地址
    bool fAllowWatchOnly;
//！覆盖对费用的自动最小/最大检查，如果为真，则必须设置频率
    bool fOverrideFeeRate;
//！覆盖钱包的M_Pay_Tx_费用（如果设置）
    boost::optional<CFeeRate> m_feerate;
//！如果设置，则覆盖默认确认目标
    boost::optional<unsigned int> m_confirm_target;
//！如果设置，则覆盖钱包的M_信号
    boost::optional<bool> m_signal_bip125_rbf;
//！避免部分使用发送到给定地址的资金
    bool m_avoid_partial_spends;
//！费用估算模式，用于控制估算的参数martfee
    FeeEstimateMode m_fee_mode;

    CCoinControl()
    {
        SetNull();
    }

    void SetNull();

    bool HasSelected() const
    {
        return (setSelected.size() > 0);
    }

    bool IsSelected(const COutPoint& output) const
    {
        return (setSelected.count(output) > 0);
    }

    void Select(const COutPoint& output)
    {
        setSelected.insert(output);
    }

    void UnSelect(const COutPoint& output)
    {
        setSelected.erase(output);
    }

    void UnSelectAll()
    {
        setSelected.clear();
    }

    void ListSelected(std::vector<COutPoint>& vOutpoints) const
    {
        vOutpoints.assign(setSelected.begin(), setSelected.end());
    }

private:
    std::set<COutPoint> setSelected;
};

#endif //比特币钱包
