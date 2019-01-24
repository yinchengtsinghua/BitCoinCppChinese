
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

#ifndef BITCOIN_QT_WALLETMODELTRANSACTION_H
#define BITCOIN_QT_WALLETMODELTRANSACTION_H

#include <qt/walletmodel.h>

#include <memory>
#include <amount.h>

#include <QObject>

class SendCoinsRecipient;

namespace interfaces {
class Node;
class PendingWalletTx;
}

/*用于WalletModel事务的数据模型。*/
class WalletModelTransaction
{
public:
    explicit WalletModelTransaction(const QList<SendCoinsRecipient> &recipients);

    QList<SendCoinsRecipient> getRecipients() const;

    std::unique_ptr<interfaces::PendingWalletTx>& getWtx();
    unsigned int getTransactionSize();

    void setTransactionFee(const CAmount& newFee);
    CAmount getTransactionFee() const;

    CAmount getTotalTransactionAmount() const;

void reassignAmounts(int nChangePosRet); //“从金额中减去费用”功能需要

private:
    QList<SendCoinsRecipient> recipients;
    std::unique_ptr<interfaces::PendingWalletTx> wtx;
    CAmount fee;
};

#endif //比特币\u qt\u钱包模型交易\u h
