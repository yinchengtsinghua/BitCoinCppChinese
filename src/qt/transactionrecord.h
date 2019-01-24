
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

#ifndef BITCOIN_QT_TRANSACTIONRECORD_H
#define BITCOIN_QT_TRANSACTIONRECORD_H

#include <amount.h>
#include <uint256.h>

#include <QList>
#include <QString>

namespace interfaces {
class Node;
class Wallet;
struct WalletTx;
struct WalletTxStatus;
}

/*交易状态的用户界面模型。事务状态是事务中随时间变化的部分。
 **/

class TransactionStatus
{
public:
    TransactionStatus():
        countsForBalance(false), sortKey(""),
        matures_in(0), status(Unconfirmed), depth(0), open_for(0), cur_num_blocks(-1)
    { }

    enum Status {
        /*已确认，/**<有6个或更多确认（正常Tx）或完全成熟（已开采Tx）**/
        ///正常（发送/接收）事务
        openuntildate，/**<事务尚未结束，正在等待日期*/

        /*nuntilblock，/**<事务尚未结束，正在等待块*/
        未确认，/**<尚未开采成区块*/

        /*确认，/**<已确认，但等待建议的确认数量**/
        冲突，/**<与其他事务或内存池冲突*/

        /*恩多奈德，/**<从钱包里被抛弃了**/
        ///生成（挖掘）的事务
        不成熟，/**<开采，但等待成熟*/

        /*已接受/**<已开采但未接受*/
    }；

    ///交易计入可用余额
    布尔伯爵；
    ///根据状态排序键
    std：：字符串排序键；

    /**@name生成（挖掘）事务
       @*/

    int matures_in;
    /*@*/

    /*@name报告的状态
       @*/

    Status status;
    qint64 depth;
    /*t64 open_for；/**<timestamp if status==openUntildate，否则为数字
                      之前需要开采的附加块
                      定稿*/

    /*@*/

    /*当前块数（了解缓存状态是否仍然有效）*/
    int cur_num_blocks;

    bool needsUpdate;
};

/*事务的UI模型。核心事务可以由多个UI事务表示，如果它有
    多个输出。
 **/

class TransactionRecord
{
public:
    enum Type
    {
        Other,
        Generated,
        SendToAddress,
        SendToOther,
        RecvWithAddress,
        RecvFromOther,
        SendToSelf
    };

    /*建议接受交易的确认数*/
    static const int RecommendedNumConfirmations = 6;

    TransactionRecord():
            hash(), time(0), type(Other), address(""), debit(0), credit(0), idx(0)
    {
    }

    TransactionRecord(uint256 _hash, qint64 _time):
            hash(_hash), time(_time), type(Other), address(""), debit(0),
            credit(0), idx(0)
    {
    }

    TransactionRecord(uint256 _hash, qint64 _time,
                Type _type, const std::string &_address,
                const CAmount& _debit, const CAmount& _credit):
            hash(_hash), time(_time), type(_type), address(_address), debit(_debit), credit(_credit),
            idx(0)
    {
    }

    /*将cwallet事务分解为模型事务记录。
     **/

    static bool showTransaction();
    static QList<TransactionRecord> decomposeTransaction(const interfaces::WalletTx& wtx);

    /*@name不可变事务属性
      @*/

    uint256 hash;
    qint64 time;
    Type type;
    std::string address;
    CAmount debit;
    CAmount credit;
    /*@*/

    /*子事务索引，用于排序键*/
    int idx;

    /*状态：可以随区块链更新而更改*/
    TransactionStatus status;

    /*交易是否以仅监视地址发送/接收*/
    bool involvesWatchAddress;

    /*返回此事务（部分）的唯一标识符*/
    QString getTxHash() const;

    /*返回子事务的输出索引*/
    int getOutputIndex() const;

    /*从Core Wallet TX更新状态。
     **/

    void updateStatus(const interfaces::WalletTxStatus& wtx, int numBlocks, int64_t block_time);

    /*返回是否需要状态更新。
     **/

    bool statusUpdateNeeded(int numBlocks) const;
};

#endif //比特币交易记录
