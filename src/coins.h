
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

#ifndef BITCOIN_COINS_H
#define BITCOIN_COINS_H

#include <primitives/transaction.h>
#include <compressor.h>
#include <core_memusage.h>
#include <crypto/siphash.h>
#include <memusage.h>
#include <serialize.h>
#include <uint256.h>

#include <assert.h>
#include <stdint.h>

#include <unordered_map>

/*
 *一个UTXO条目。
 *
 *序列化格式：
 *-变量（（coinbase？1:0）（高度<<1）
 *-未用过的CTXout（通过CTXout压缩机）
 **/

class Coin
{
public:
//！未占用的事务输出
    CTxOut out;

//！包含交易是否为CoinBase
    unsigned int fCoinBase : 1;

//！活动块链中包含此包含事务的高度
    uint32_t nHeight : 31;

//！根据ctxout和高度/硬币库信息构造硬币。
    Coin(CTxOut&& outIn, int nHeightIn, bool fCoinBaseIn) : out(std::move(outIn)), fCoinBase(fCoinBaseIn), nHeight(nHeightIn) {}
    Coin(const CTxOut& outIn, int nHeightIn, bool fCoinBaseIn) : out(outIn), fCoinBase(fCoinBaseIn),nHeight(nHeightIn) {}

    void Clear() {
        out.SetNull();
        fCoinBase = false;
        nHeight = 0;
    }

//！空构造函数
    Coin() : fCoinBase(false), nHeight(0) { }

    bool IsCoinBase() const {
        return fCoinBase;
    }

    template<typename Stream>
    void Serialize(Stream &s) const {
        assert(!IsSpent());
        uint32_t code = nHeight * 2 + fCoinBase;
        ::Serialize(s, VARINT(code));
        ::Serialize(s, CTxOutCompressor(REF(out)));
    }

    template<typename Stream>
    void Unserialize(Stream &s) {
        uint32_t code = 0;
        ::Unserialize(s, VARINT(code));
        nHeight = code >> 1;
        fCoinBase = code & 1;
        ::Unserialize(s, CTxOutCompressor(out));
    }

    bool IsSpent() const {
        return out.IsNull();
    }

    size_t DynamicMemoryUsage() const {
        return memusage::DynamicUsage(out.scriptPubKey);
    }
};

class SaltedOutpointHasher
{
private:
    /*盐*/
    const uint64_t k0, k1;

public:
    SaltedOutpointHasher();

    /*
     *此*必须*返回大小在32位系统上使用Boost 1.46
     *如果自定义哈希返回
     *uint64_t，导致同步链时出现故障（4634）。
     **/

    size_t operator()(const COutPoint& id) const {
        return SipHashUint256Extra(k0, k1, id.hash, id.n);
    }
};

struct CCoinsCacheEntry
{
Coin coin; //实际缓存的数据。
    unsigned char flags;

    enum Flags {
DIRTY = (1 << 0), //此缓存项可能与父视图中的版本不同。
FRESH = (1 << 1), //父视图没有此项（或已修剪）。
        /*请注意，Fresh是一种性能优化，我们可以使用它
         *如果我们知道我们不需要的话，请删除已完全用完的硬币。
         *刷新对父缓存的更改。总是安全的
         *如果不保证该条件，则不标记为新鲜。
         **/

    };

    CCoinsCacheEntry() : flags(0) {}
    explicit CCoinsCacheEntry(Coin&& coin_) : coin(std::move(coin_)), flags(0) {}
};

typedef std::unordered_map<COutPoint, CCoinsCacheEntry, SaltedOutpointHasher> CCoinsMap;

/*用于在CoinsView状态上迭代的光标*/
class CCoinsViewCursor
{
public:
    CCoinsViewCursor(const uint256 &hashBlockIn): hashBlock(hashBlockIn) {}
    virtual ~CCoinsViewCursor() {}

    virtual bool GetKey(COutPoint &key) const = 0;
    virtual bool GetValue(Coin &coin) const = 0;
    virtual unsigned int GetValueSize() const = 0;

    virtual bool Valid() const = 0;
    virtual void Next() = 0;

//！创建此光标时获取最佳块
    const uint256 &GetBestBlock() const { return hashBlock; }
private:
    uint256 hashBlock;
};

/*打开的txout数据集的抽象视图。*/
class CCoinsView
{
public:
    /*检索给定输出点的硬币（未暂停的事务输出）。
     *只有在找到一枚未用过的硬币时才返回“真”，而这枚硬币是以硬币形式返回的。
     *当返回“假”时，未指明硬币的价值。
     **/

    virtual bool GetCoin(const COutPoint &outpoint, Coin &coin) const;

//！只需检查给定的输出点是否未被占用。
    virtual bool HaveCoin(const COutPoint &outpoint) const;

//！检索此ccoinsview当前表示其状态的块哈希
    virtual uint256 GetBestBlock() const;

//！检索可能只被部分写入的块的范围。
//！如果数据库处于一致状态，则结果为空向量。
//！否则，返回一个由新的和
//！旧的块散列，按这个顺序。
    virtual std::vector<uint256> GetHeadBlocks() const;

//！进行批量修改（多次硬币更改+最佳块更改）。
//！通过的地图硬币可以修改。
    virtual bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock);

//！获取要在整个状态上迭代的光标
    virtual CCoinsViewCursor *Cursor() const;

//！当我们多态地使用ccoinsview时，有一个虚拟的析构函数
    virtual ~CCoinsView() {}

//！估计数据库大小（如果未实现，则为0）
    virtual size_t EstimateSize() const { return 0; }
};


/*由另一个ccoinsview支持的ccoinsview*/
class CCoinsViewBacked : public CCoinsView
{
protected:
    CCoinsView *base;

public:
    CCoinsViewBacked(CCoinsView *viewIn);
    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
    bool HaveCoin(const COutPoint &outpoint) const override;
    uint256 GetBestBlock() const override;
    std::vector<uint256> GetHeadBlocks() const override;
    void SetBackend(CCoinsView &viewIn);
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) override;
    CCoinsViewCursor *Cursor() const override;
    size_t EstimateSize() const override;
};


/*将事务的内存缓存添加到另一个CCoinsView的CCoinsView*/
class CCoinsViewCache : public CCoinsViewBacked
{
protected:
    /*
     *使可变以便我们可以“填充缓存”，即使是从get方法
     *声明为“const”。
     **/

    mutable uint256 hashBlock;
    mutable CCoinsMap cacheCoins;

    /*内部硬币对象的缓存动态内存使用率。*/
    mutable size_t cachedCoinsUsage;

public:
    CCoinsViewCache(CCoinsView *baseIn);

    /*
     *通过删除复制构造函数，我们可以防止在基本缓存之上创建缓存时意外使用它。
     **/

    CCoinsViewCache(const CCoinsViewCache &) = delete;

//标准CCoinsView方法
    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
    bool HaveCoin(const COutPoint &outpoint) const override;
    uint256 GetBestBlock() const override;
    void SetBestBlock(const uint256 &hashBlock);
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) override;
    CCoinsViewCursor* Cursor() const override {
        throw std::logic_error("CCoinsViewCache cursor iteration not supported.");
    }

    /*
     *检查给定的utxo是否已加载到此缓存中。
     *语义与havecoin（）相同，但不调用
     *制作背视图。
     **/

    bool HaveCoinInCache(const COutPoint &outpoint) const;

    /*
     *返回对缓存中硬币的引用，如果未找到则返回修剪后的引用。这是
     *比getcoin更高效。
     *
     *一般来说，不要将返回的引用保留超过一个短范围。
     *而当前的实现允许对内容进行修改
     *在保存引用时，不应依赖此行为。
     快点！为了安全起见，最好不要通过任何其他方式保存返回的引用
     *调用此缓存。
     **/

    const Coin& AccessCoin(const COutPoint &output) const;

    /*
     *加一枚硬币。如果非修剪版本可能
     *已经存在。
     **/

    void AddCoin(const COutPoint& outpoint, Coin&& coin, bool potential_overwrite);

    /*
     *花一枚硬币。传递moveto以获取已删除的数据。
     *如果传递的输出点不存在未暂停的输出，则此调用
     *无效。
     **/

    bool SpendCoin(const COutPoint &outpoint, Coin* moveto = nullptr);

    /*
     *将应用于此缓存的修改推送到其基础。
     *如果在销毁之前未调用此方法，将导致忘记更改。
     *如果返回false，则此缓存（及其支持视图）的状态将未定义。
     **/

    bool Flush();

    /*
     *从缓存中删除具有给定输出点的utxo（如果是）
     *未修改。
     **/

    void Uncache(const COutPoint &outpoint);

//！计算缓存的大小（以事务输出的数量为单位）
    unsigned int GetCacheSize() const;

//！计算缓存大小（字节）
    size_t DynamicMemoryUsage() const;

    /*
     *交易中的比特币数量
     *请注意，轻量级客户机可能除了先前事务的散列值之外，不知道其他任何信息，
     *所以可能无法计算。
     *
     *@param[in]tx事务，我们正在检查其输入总计
     *@返回所有输入值的总和（scriptsigs）
     **/

    CAmount GetValueIn(const CTransaction& tx) const;

//！检查此视图表示的utxo集合中是否存在事务的所有prevout
    bool HaveInputs(const CTransaction& tx) const;

private:
    CCoinsMap::iterator FetchCoin(const COutPoint &outpoint) const;
};

//！实用程序函数，将事务的所有输出添加到缓存中。
//如果check为false，则假定仅可对coinbase事务进行覆盖。
//如果check为true，则可以查询基础视图，以确定添加项是否为
//重写。
//TODO:传入布尔值以将这些可能的覆盖限制为已知
//（BIP34前）病例。
void AddCoins(CCoinsViewCache& cache, const CTransaction& tx, int nHeight, bool check = false);

//！实用程序函数查找具有给定txid的任何未暂停输出。
//这个函数可能非常昂贵，因为在事务的情况下
//在缓存中找不到，它可能导致每个块最多有个输出
//查找数据库，因此应该小心使用。
const Coin& AccessByTxid(const CCoinsViewCache& cache, const uint256& txid);

#endif //比科因斯科因斯赫
