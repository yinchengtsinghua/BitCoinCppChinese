
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

#include <index/txindex.h>
#include <shutdown.h>
#include <ui_interface.h>
#include <util/system.h>
#include <validation.h>

#include <boost/thread.hpp>

constexpr char DB_BEST_BLOCK = 'B';
constexpr char DB_TXINDEX = 't';
constexpr char DB_TXINDEX_BLOCK = 'T';

std::unique_ptr<TxIndex> g_txindex;

struct CDiskTxPos : public CDiskBlockPos
{
unsigned int nTxOffset; //后集箱

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(CDiskBlockPos, *this);
        READWRITE(VARINT(nTxOffset));
    }

    CDiskTxPos(const CDiskBlockPos &blockIn, unsigned int nTxOffsetIn) : CDiskBlockPos(blockIn.nFile, blockIn.nPos), nTxOffset(nTxOffsetIn) {
    }

    CDiskTxPos() {
        SetNull();
    }

    void SetNull() {
        CDiskBlockPos::SetNull();
        nTxOffset = 0;
    }
};

/*
 *访问txindex数据库（indexes/txindex/）
 *
 *数据库存储数据库同步到的链的块定位器。
 *以便txindex可以有效地确定它最后停止的点。
 *使用定位器而不是链尖的简单哈希，因为块
 *和块索引项在该数据库之后才能刷新到磁盘
 *更新。
 **/

class TxIndex::DB : public BaseIndex::DB
{
public:
    explicit DB(size_t n_cache_size, bool f_memory = false, bool f_wipe = false);

///使用给定哈希读取事务数据的磁盘位置。如果
///TRANSACTION哈希未编入索引。
    bool ReadTxPos(const uint256& txid, CDiskTxPos& pos) const;

///将一批事务位置写入数据库。
    bool WriteTxs(const std::vector<std::pair<uint256, CDiskTxPos>>& v_pos);

///migrate txtindex data from the block tree db，which it may be for older nodes that have not
///已升级到新数据库。
    bool MigrateData(CBlockTreeDB& block_tree_db, const CBlockLocator& best_locator);
};

TxIndex::DB::DB(size_t n_cache_size, bool f_memory, bool f_wipe) :
    BaseIndex::DB(GetDataDir() / "indexes" / "txindex", n_cache_size, f_memory, f_wipe)
{}

bool TxIndex::DB::ReadTxPos(const uint256 &txid, CDiskTxPos& pos) const
{
    return Read(std::make_pair(DB_TXINDEX, txid), pos);
}

bool TxIndex::DB::WriteTxs(const std::vector<std::pair<uint256, CDiskTxPos>>& v_pos)
{
    CDBBatch batch(*this);
    for (const auto& tuple : v_pos) {
        batch.Write(std::make_pair(DB_TXINDEX, tuple.first), tuple.second);
    }
    return WriteBatch(batch);
}

/*
 *安全地将数据从旧的txindex数据库传输到新数据库，并压缩
 *更新了密钥范围。migratedata在内部使用。
 **/

static void WriteTxIndexMigrationBatches(CDBWrapper& newdb, CDBWrapper& olddb,
                                         CDBBatch& batch_newdb, CDBBatch& batch_olddb,
                                         const std::pair<unsigned char, uint256>& begin_key,
                                         const std::pair<unsigned char, uint256>& end_key)
{
//在从旧数据库删除之前，将新数据库更改同步到磁盘。
    /*db.writebatch（批处理newdb，/*fsync=*/true）；
    olddb.writebatch（批处理olddb）；
    olddb.compactrange（开始键、结束键）；

    batch_newdb.clear（）；
    批处理loddb.clear（）；
}

bool txindex:：db:：migratedata（cblocktreedb&block_tree_db，const cblocklocator&best_locator）
{
    //TxIndex的先前实现始终与块索引同步
    //并用布尔数据库标志指示存在。如果设置了标志，
    //这意味着以前版本的txindex有效，并且与
    //链尖。迁移的第一步是取消标记和
    //将链哈希写入单独的键db_txindex_块。之后，
    //索引项被批量复制到新数据库中。最后，
    //从旧数据库中删除db_txindex_块，块散列为
    //写入新数据库。
    / /
    //取消设置布尔标志可确保如果将节点降级为
    //在以前的版本中，不会看到已损坏的、部分迁移的索引
    //—将看到txindex被禁用。当节点升级时
    //同样，迁移将从停止的位置开始，并同步到块
    //使用hash db_txindex_块。
    bool f_legacy_flag=false；
    block_tree_db.readflag（“txindex”，f_legacy_flag）；
    如果（F_Legacy_Flag）
        如果（！）block_tree_db.write（db_txindex_block，best_locator））
            返回错误（“%s:无法写入块指示器”，
        }
        如果（！）block_tree_db.writeflag（“txindex”，false））
            返回错误（“%s:无法写入块索引db标志”，
        }
    }

    CBlocklocator定位器；
    如果（！）block_tree_db.read（db_txindex_block，locator））
        回归真实；
    }

    Int64计数=0；
    logprintf（“正在升级txtindex数据库…[0% %] n“”；
    uiinterface.showprogress（（“升级txindex数据库”），0，true）；
    int报告完成=0；
    const size_t batch_size=1<<24；//16 mib

    CDB批次新数据库（*this）；
    cdbbatch batch_olddb（block_tree_db）；

    std:：pair<unsigned char，uint256>key；
    std:：pair<unsigned char，uint256>begin_key_db_txindex，uint256（）
    std:：pair<unsigned char，uint256>prev_key=begin_key；

    bool interrupted=false；
    std:：unique_ptr<cdbiterator>cursor（block_tree_db.newiterator（））；
    for（cursor->seek（begin_key）；cursor->valid（）；cursor->next（））
        boost:：this_thread:：interrupt_point（）；
        if（shutdownrequested（））
            中断=真；
            断裂；
        }

        如果（！）cursor->getkey（key））
            返回错误（“%s:无法从有效光标中获取密钥”，
        }
        如果（首先）！= dBuxtxBead）{
            断裂；
        }

        //每10%记录一次进度。
        如果（++计数%256==0）
            //由于txid是均匀随机的，并且以递增的顺序遍历，所以高16位
            //的哈希值可用于估计当前进度。
            const uint256&txid=key.second；
            uint32_t高_nibble=
                （static_cast<uint32_t>（*（txid.begin（）+0））<<8）+
                （static_cast<uint32_t>（*（txid.begin（）+1））<<0）；
            int百分比_done=（int）（高_nibble*100.0/65536.0+0.5）；

            uiinterface.showprogress（“升级txtindex数据库”），完成百分比，真的；
            如果（报告完成<完成百分比）
                logprintf（“正在升级txtindex数据库…[%d%%]\n“，完成百分比”；
                报告完成=完成百分比/10；
            }
        }

        cdisktxpos值；
        如果（！）cursor->getvalue（value））
            返回错误（“%s:无法分析txindex记录”，“func”，
        }
        批量写入（key，value）；
        批量删除（键）；

        如果（batch_newdb.sizeEstimate（）>batch_size batch_olddb.sizeEstimate（）>batch_size）
            //注意：迭代时可以删除当前db光标指向的键
            //因为leveldb迭代器保证提供
            //基础数据，如轻量级快照。
            WritetxIndexMigrationBatches（*此，块树数据库，
                                         批处理newdb，批处理olddb，
                                         密钥；密钥；
            PrimyKEY＝KEY；
        }
    }

    //如果这些最终的DB批完成迁移，则写入最佳块
    //散列标记到新数据库并从旧数据库中删除。这个信号
    //前者在区块链中完全赶上了这一点，并且
    //所有TxIndex项都已从后者中删除。
    如果（！）中断）{
        批处理lddb.erase（db_txindex_块）；
        批量写入（db-best-block，locator）；
    }

    WritetxIndexMigrationBatches（*此，块树数据库，
                                 批处理newdb，批处理olddb，
                                 开始键，键）；

    如果（中断）
        logprintf（“[已取消]……………………………………………………………………………………………………………………………………………
        返回错误；
    }

    uiinterface.showprogress（“”，100，false）；

    logprintf（“[完成]）.\n”）；
    回归真实；
}

txindex：：txindex（大小缓存大小、bool f_内存、bool f_擦除）
    ：m_-db（makeunique<txindex:：db>（n_-cache_-size，f_-memory，f_-wipe））。
{}

txIndex:：~txIndex（）

bool txindex:：init（）。
{
    锁（CSKEMAN）；

    //尝试将txindex从旧数据库迁移到新数据库。即使
    //chain_tip为空，节点可能正在重新索引，我们仍然希望
    //删除旧数据库中的TxIndex记录。
    如果（！）m_db->migratedata（*pblocktree，chainActive.getLocator（））
        返回错误；
    }

    返回baseIndex:：init（）；
}

bool txindex:：writeblock（const cblock&block，const cblockindex*pindex）
{
    //排除Genesis块事务，因为输出不可使用。
    if（pindex->nheight==0）返回true；

    cdisktxpos pos（pindex->getblockpos（），getsizeofcompactsize（block.vtx.size（））；
    std:：vector<std:：pair<uint256，cdisktxpos>>vpos；
    vpos.reserve（block.vtx.size（））；
    用于（const auto&tx:block.vtx）
        vpos.emplace_back（tx->gethash（），pos）；
        pos.ntxoffset+=：：getSerializeSize（*tx，客户机版本）；
    }
    返回m_db->writetxs（vpos）；
}

baseindex:：db&txindex:：getdb（）const返回*m_db；

bool txindex:：findtx（const uint256&tx_hash，uint256&block_hash，ctransactionref&tx）const
{
    CDisktxpos邮箱；
    如果（！）m_db->readtxpos（tx_hash，postx））
        返回错误；
    }

    cautofile文件（openblockfile（postx，true），ser_磁盘，客户机_版本）；
    if（file.isNull（））
        返回错误（“%s:openblockfile failed”，
    }
    cBlockHeader标题；
    尝试{
        文件>页眉；
        if（fseek（file.get（），postx.ntxoffset，seek_cur））
            返回错误（“%s:fseek（…）失败”，“func”）；
        }
        文件>
    catch（const std:：exception&e）
        返回错误（“%s:反序列化或I/O错误-%s”，uufunc_uuuuu，e.what（））；
    }
    如果（tx->gethash（）！= txyHASH）{
        返回错误（“%s:txid不匹配”，
    }
    block_hash=header.gethash（）；
    回归真实；
}
