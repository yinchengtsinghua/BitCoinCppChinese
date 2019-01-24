
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

#ifndef BITCOIN_TXDB_H
#define BITCOIN_TXDB_H

#include <coins.h>
#include <dbwrapper.h>
#include <chain.h>
#include <primitives/block.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class CBlockIndex;
class CCoinsViewDBCursor;
class uint256;

//！如果至少还有这么多可用空间，就不需要定期刷新。
static constexpr int MAX_BLOCK_COINSDB_USAGE = 10;
//！-dbcache默认值（mib）
static const int64_t nDefaultDbCache = 450;
//！-dbbatchsize默认值（字节）
static const int64_t nDefaultDbBatchSize = 16 << 20;
//！最大值-dbcache（mib）
static const int64_t nMaxDbCache = sizeof(void*) > 4 ? 16384 : 1024;
//！最小-dbcache（mib）
static const int64_t nMinDbCache = 4;
//！分配给块树数据库特定缓存的最大内存，如果没有-txindex（mib）
static const int64_t nMaxBlockDBCache = 2;
//！分配给块树数据库特定缓存的最大内存，if-txindex（mib）
//与utxo数据库不同，对于txindex场景，leveldb缓存使
//一个有意义的区别：https://github.com/bitcoin/bitcoin/pull/8273 issuecomment-22960191
static const int64_t nMaxTxIndexCache = 1024;
//！分配给硬币数据库特定缓存（MIB）的最大内存
static const int64_t nMaxCoinsDBCache = 8;

/*coin数据库支持的ccoinsview（chainstate/）*/
class CCoinsViewDB final : public CCoinsView
{
protected:
    CDBWrapper db;
public:
    explicit CCoinsViewDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
    bool HaveCoin(const COutPoint &outpoint) const override;
    uint256 GetBestBlock() const override;
    std::vector<uint256> GetHeadBlocks() const override;
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) override;
    CCoinsViewCursor *Cursor() const override;

//！尝试从旧的数据库格式更新。返回是否发生错误。
    bool Upgrade();
    size_t EstimateSize() const override;
};

/*CCoinsViewCursor的专用化，以在CCoinsViewDB上迭代*/
class CCoinsViewDBCursor: public CCoinsViewCursor
{
public:
    ~CCoinsViewDBCursor() {}

    bool GetKey(COutPoint &key) const override;
    bool GetValue(Coin &coin) const override;
    unsigned int GetValueSize() const override;

    bool Valid() const override;
    void Next() override;

private:
    CCoinsViewDBCursor(CDBIterator* pcursorIn, const uint256 &hashBlockIn):
        CCoinsViewCursor(hashBlockIn), pcursor(pcursorIn) {}
    std::unique_ptr<CDBIterator> pcursor;
    std::pair<char, COutPoint> keyTmp;

    friend class CCoinsViewDB;
};

/*访问块数据库（blocks/index/）*/
class CBlockTreeDB : public CDBWrapper
{
public:
    explicit CBlockTreeDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo);
    bool ReadBlockFileInfo(int nFile, CBlockFileInfo &info);
    bool ReadLastBlockFile(int &nFile);
    bool WriteReindexing(bool fReindexing);
    void ReadReindexing(bool &fReindexing);
    bool WriteFlag(const std::string &name, bool fValue);
    bool ReadFlag(const std::string &name, bool &fValue);
    bool LoadBlockIndexGuts(const Consensus::Params& consensusParams, std::function<CBlockIndex*(const uint256&)> insertBlockIndex);
};

#endif //BiCONIIN TXBDYH
