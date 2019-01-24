
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

#ifndef BITCOIN_WALLET_FEES_H
#define BITCOIN_WALLET_FEES_H

#include <amount.h>

class CBlockPolicyEstimator;
class CCoinControl;
class CFeeRate;
class CTxMemPool;
class CWallet;
struct FeeCalculation;

/*
 *返回此尺寸所需的最低绝对费用
 *根据所需费率
 **/

CAmount GetRequiredFee(const CWallet& wallet, unsigned int nTxBytes);

/*
 *考虑到用户设置参数，估计最低费用
 *以及所需费用
 **/

CAmount GetMinimumFee(const CWallet& wallet, unsigned int nTxBytes, const CCoinControl& coin_control, const CTxMemPool& pool, const CBlockPolicyEstimator& estimator, FeeCalculation* feeCalc);

/*
 *考虑到
 *最小中继速率和用户设置的最小事务速率
 **/

CFeeRate GetRequiredFeeRate(const CWallet& wallet);

/*
 *考虑用户设置参数，估算最低费率
 *以及所需费用
 **/

CFeeRate GetMinimumFeeRate(const CWallet& wallet, const CCoinControl& coin_control, const CTxMemPool& pool, const CBlockPolicyEstimator& estimator, FeeCalculation* feeCalc);

/*
 *返回丢弃更改的最大频率。
 **/

CFeeRate GetDiscardRate(const CWallet& wallet, const CBlockPolicyEstimator& estimator);

#endif //比特币钱包手续费
