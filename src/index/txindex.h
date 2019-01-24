
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

#ifndef BITCOIN_INDEX_TXINDEX_H
#define BITCOIN_INDEX_TXINDEX_H

#include <chain.h>
#include <index/base.h>
#include <txdb.h>

/*
 *txindex用于按哈希查找区块链中包含的交易。
 *索引被写入一个LEVELDB数据库，并记录文件系统
 *每个事务的位置（按事务哈希）。
 **/

class TxIndex final : public BaseIndex
{
protected:
    class DB;

private:
    const std::unique_ptr<DB> m_db;

protected:
///override基类init从旧数据库迁移。
    bool Init() override;

    bool WriteBlock(const CBlock& block, const CBlockIndex* pindex) override;

    BaseIndex::DB& GetDB() const override;

    const char* GetName() const override { return "txindex"; }

public:
///构造可供查询的索引。
    explicit TxIndex(size_t n_cache_size, bool f_memory = false, bool f_wipe = false);

//声明析构函数是因为此类包含对不完整类型的唯一指针。
    virtual ~TxIndex() override;

///按哈希查找事务。
///
///@param[in]tx_散列要返回的事务的散列。
///@param[out]block_hash事务所在块的哈希。
///@param[out]发送事务本身。
///@如果找到事务，返回true，否则返回false
    bool FindTx(const uint256& tx_hash, uint256& block_hash, CTransactionRef& tx) const;
};

///GetTransaction中使用的全局事务索引。可能为空。
extern std::unique_ptr<TxIndex> g_txindex;

#endif //比特币指数
