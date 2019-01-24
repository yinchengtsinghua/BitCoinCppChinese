
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_BLOCKFILTER_H
#define BITCOIN_BLOCKFILTER_H

#include <stdint.h>
#include <unordered_set>
#include <vector>

#include <primitives/block.h>
#include <serialize.h>
#include <uint256.h>
#include <undo.h>
#include <util/bytevectorhash.h>

/*
 *这实现了bip 158中定义的golomb编码集。这是一个
 *测试集成员的紧凑、概率数据结构。
 **/

class GCSFilter
{
public:
    typedef std::vector<unsigned char> Element;
    typedef std::unordered_set<Element, ByteVectorHash> ElementSet;

    struct Params
    {
        uint64_t m_siphash_k0;
        uint64_t m_siphash_k1;
uint8_t m_P;  //！<golomb-rice编码参数
uint32_t m_M;  //！<反向假阳性率

        Params(uint64_t siphash_k0 = 0, uint64_t siphash_k1 = 0, uint8_t P = 0, uint32_t M = 1)
            : m_siphash_k0(siphash_k0), m_siphash_k1(siphash_k1), m_P(P), m_M(M)
        {}
    };

private:
    Params m_params;
uint32_t m_N;  //！<过滤器中的元素数
uint64_t m_F;  //！<元素哈希范围，f=n*m
    std::vector<unsigned char> m_encoded;

    /*将数据元素散列为范围[0，n*m]内的整数。*/
    uint64_t HashToRange(const Element& element) const;

    std::vector<uint64_t> BuildHashedSet(const ElementSet& elements) const;

    /*用于实现Match和MatchAny的Helper方法*/
    bool MatchInternal(const uint64_t* sorted_element_hashes, size_t size) const;

public:

    /*构造空筛选器。*/
    explicit GCSFilter(const Params& params = Params());

    /*从编码中重建已创建的筛选器。*/
    GCSFilter(const Params& params, std::vector<unsigned char> encoded_filter);

    /*从参数和元素集生成新的筛选器。*/
    GCSFilter(const Params& params, const ElementSet& elements);

    uint32_t GetN() const { return m_N; }
    const Params& GetParams() const { return m_params; }
    const std::vector<unsigned char>& GetEncoded() const { return m_encoded; }

    /*
     *检查元素是否在集合中。假阳性是可能的
     *概率为1/m。
     **/

    bool Match(const Element& element) const;

    /*
     *检查集合中是否有给定元素。假阳性
     *在检查每个元素的概率为1/m的情况下是可能的。这是更多
     *有效地分别检查多个元素的匹配。
     **/

    bool MatchAny(const ElementSet& elements) const;
};

constexpr uint8_t BASIC_FILTER_P = 19;
constexpr uint32_t BASIC_FILTER_M = 784931;

enum BlockFilterType : uint8_t
{
    BASIC = 0,
};

/*
 *完成BIP 157中定义的块过滤器结构。序列化匹配
 *cfilter消息的有效负载。
 **/

class BlockFilter
{
private:
    BlockFilterType m_filter_type;
    uint256 m_block_hash;
    GCSFilter m_filter;

    bool BuildParams(GCSFilter::Params& params) const;

public:

    BlockFilter() = default;

//！从零件重建块过滤器。
    BlockFilter(BlockFilterType filter_type, const uint256& block_hash,
                std::vector<unsigned char> filter);

//！从块构造指定类型的新BlockFilter。
    BlockFilter(BlockFilterType filter_type, const CBlock& block, const CBlockUndo& block_undo);

    BlockFilterType GetFilterType() const { return m_filter_type; }
    const uint256& GetBlockHash() const { return m_block_hash; }
    const GCSFilter& GetFilter() const { return m_filter; }

    const std::vector<unsigned char>& GetEncodedFilter() const
    {
        return m_filter.GetEncoded();
    }

//！计算筛选器哈希。
    uint256 GetHash() const;

//！计算前面的过滤器头。
    uint256 ComputeHeader(const uint256& prev_header) const;

    template <typename Stream>
    void Serialize(Stream& s) const {
        s << m_block_hash
          << static_cast<uint8_t>(m_filter_type)
          << m_filter.GetEncoded();
    }

    template <typename Stream>
    void Unserialize(Stream& s) {
        std::vector<unsigned char> encoded_filter;
        uint8_t filter_type;

        s >> m_block_hash
          >> filter_type
          >> encoded_filter;

        m_filter_type = static_cast<BlockFilterType>(filter_type);

        GCSFilter::Params params;
        if (!BuildParams(params)) {
            throw std::ios_base::failure("unknown filter_type");
        }
        m_filter = GCSFilter(params, std::move(encoded_filter));
    }
};

#endif //比特币阻塞过滤器
