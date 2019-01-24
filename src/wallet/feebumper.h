
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

#ifndef BITCOIN_WALLET_FEEBUMPER_H
#define BITCOIN_WALLET_FEEBUMPER_H

#include <primitives/transaction.h>

class CWallet;
class CWalletTx;
class uint256;
class CCoinControl;
enum class FeeEstimateMode;

namespace feebumper {

enum class Result
{
    OK,
    INVALID_ADDRESS_OR_KEY,
    INVALID_REQUEST,
    INVALID_PARAMETER,
    WALLET_ERROR,
    MISC_ERROR,
};

//！返回是否可以缓冲事务。
bool TransactionCanBeBumped(const CWallet* wallet, const uint256& txid);

//！创建BumpFee交易记录。
Result CreateTransaction(const CWallet* wallet,
                         const uint256& txid,
                         const CCoinControl& coin_control,
                         CAmount total_fee,
                         std::vector<std::string>& errors,
                         CAmount& old_fee,
                         CAmount& new_fee,
                         CMutableTransaction& mtx);

//！签署新交易，
//！@如果找不到tx或它是
//！无法创建签名
bool SignTransaction(CWallet* wallet, CMutableTransaction& mtx);

//！提交bumpfee事务。
//！如果cwallet:：commitTransaction成功，@return success，
//！但如果无法将Tx添加到内存池，则设置错误（稍后再试）
//！或者无法将旧事务标记为已替换。
Result CommitTransaction(CWallet* wallet,
                         const uint256& txid,
                         CMutableTransaction&& mtx,
                         std::vector<std::string>& errors,
                         uint256& bumped_txid);

} //命名空间feebumper

#endif //比特币钱包
