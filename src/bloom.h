
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2012-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_BLOOM_H
#define BITCOIN_BLOOM_H

#include <serialize.h>

#include <vector>

class COutPoint;
class CTransaction;
class uint256;

//！FP率<0.1%的20000个项目或10000个项目且<0.0001%
static const unsigned int MAX_BLOOM_FILTER_SIZE = 36000; //字节
static const unsigned int MAX_HASH_FUNCS = 50;

/*
 *前两位nflags控制实际更新的isrelevantandupdate量
 *其余位保留
 **/

enum bloomflags
{
    BLOOM_UPDATE_NONE = 0,
    BLOOM_UPDATE_ALL = 1,
//仅在输出为“支付到发布密钥/支付到多任务”脚本时将输出点添加到筛选器
    BLOOM_UPDATE_P2PUBKEY_ONLY = 2,
    BLOOM_UPDATE_MASK = 3,
};

/*
 *Bloomfilter是SPV客户提供的概率滤波器。
 *这样我们就可以过滤发送给他们的交易。
 *
 *这样可以显著提高交易效率和阻止下载。
 *
 *因为Bloom过滤器是概率的，所以SPV节点可以增加错误-
 *正利率，使我们发送的交易实际上不是它的，
 *允许客户端通过模糊哪些内容来交换更多带宽以获得更多隐私
 *钥匙由它们控制。
 **/

class CBloomFilter
{
private:
    std::vector<unsigned char> vData;
    bool isFull;
    bool isEmpty;
    unsigned int nHashFuncs;
    unsigned int nTweak;
    unsigned char nFlags;

    unsigned int Hash(unsigned int nHashNum, const std::vector<unsigned char>& vDataToHash) const;

public:
    /*
     *创建一个新的Bloom过滤器，当填充给定数量的元素时，它将提供给定的fp速率。
     *请注意，如果给定的参数将导致一个超出协议限制范围的过滤器，
     *创建的过滤器将尽可能接近协议限制内的给定参数。
     *如果净利润非常低或净利润不合理地高，这将适用。
     *ntweak是一个常量，它被添加到传递给散列函数的种子值中。
     *通常应该是一个随机值（并且大部分只暴露在单元测试中）
     *nflags应该是bloom-update-enums（而不是mask）之一。
     **/

    CBloomFilter(const unsigned int nElements, const double nFPRate, const unsigned int nTweak, unsigned char nFlagsIn);
    CBloomFilter() : isFull(true), isEmpty(false), nHashFuncs(0), nTweak(0), nFlags(0) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vData);
        READWRITE(nHashFuncs);
        READWRITE(nTweak);
        READWRITE(nFlags);
    }

    void insert(const std::vector<unsigned char>& vKey);
    void insert(const COutPoint& outpoint);
    void insert(const uint256& hash);

    bool contains(const std::vector<unsigned char>& vKey) const;
    bool contains(const COutPoint& outpoint) const;
    bool contains(const uint256& hash) const;

    void clear();
    void reset(const unsigned int nNewTweak);

//！如果大小小于等于max_bloom_filter_size且哈希函数数小于等于max_hash_funcs，则为true。
//！（捕获一个刚刚反序列化过大的筛选器）
    bool IsWithinSizeConstraints() const;

//！还添加与过滤器匹配的任何输出到过滤器（以匹配其支出txe）
    bool IsRelevantAndUpdate(const CTransaction& tx);

//！检查空的和满的过滤器以避免浪费CPU
    void UpdateEmptyFull();
};

/*
 *RollingBloomFilter是一个概率“跟踪最近插入的内容”集。
 *用要跟踪的项目数和假阳性进行构造。
 *利率。与cbloomfilter不同，默认情况下，ntweak设置为加密的
 *确保随机值安全。同样地，方法不是clear（）。
 *提供reset（），它还更改ntweak以减少
 *误报。
 *
 *如果项目是最后一个n到1.5*n中的一个，则contains（item）将始终返回true。
 *插入（）'ed…但对于未插入的项也可能返回true。
 *
 *假阳性率每因子0.1每元素需要大约1.8字节。
 *（更准确地说：3/（log（256）*log（2））*log（1/fprate）*nelements bytes）
 **/

class CRollingBloomFilter
{
public:
//随机bloom过滤器在创建时调用getrand（）。
//不要创建全局crollingbloomfilter对象，因为它们可能是
//在正确初始化随机数发生器之前构造。
    CRollingBloomFilter(const unsigned int nElements, const double nFPRate);

    void insert(const std::vector<unsigned char>& vKey);
    void insert(const uint256& hash);
    bool contains(const std::vector<unsigned char>& vKey) const;
    bool contains(const uint256& hash) const;

    void reset();

private:
    int nEntriesPerGeneration;
    int nEntriesThisGeneration;
    int nGeneration;
    std::vector<uint64_t> data;
    unsigned int nTweak;
    int nHashFuncs;
};

#endif //比特科尼布鲁姆
