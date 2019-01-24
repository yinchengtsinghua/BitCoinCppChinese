
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

#ifndef BITCOIN_UNDO_H
#define BITCOIN_UNDO_H

#include <coins.h>
#include <compressor.h>
#include <consensus/consensus.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <version.h>

/*撤消ctxin的信息
 *
 *包含正在使用的prevout的ctxout及其元数据
 *（不管是否为CoinBase，高度）。序列化包含的虚拟值为
 *零。这与希望看到的旧版本兼容
 *那里的交易版本。
 **/

class TxInUndoSerializer
{
    const Coin* txout;

public:
    template<typename Stream>
    void Serialize(Stream &s) const {
        ::Serialize(s, VARINT(txout->nHeight * 2 + (txout->fCoinBase ? 1u : 0u)));
        if (txout->nHeight > 0) {
//需要与旧的撤消格式保持兼容性。
            ::Serialize(s, (unsigned char)0);
        }
        ::Serialize(s, CTxOutCompressor(REF(txout->out)));
    }

    explicit TxInUndoSerializer(const Coin* coin) : txout(coin) {}
};

class TxInUndoDeserializer
{
    Coin* txout;

public:
    template<typename Stream>
    void Unserialize(Stream &s) {
        unsigned int nCode = 0;
        ::Unserialize(s, VARINT(nCode));
        txout->nHeight = nCode / 2;
        txout->fCoinBase = nCode & 1;
        if (txout->nHeight > 0) {
//旧版本存储了上次花费的版本号
//事务的输出。非最终支出用
//身高＝0。
            unsigned int nVersionDummy;
            ::Unserialize(s, VARINT(nVersionDummy));
        }
        ::Unserialize(s, CTxOutCompressor(REF(txout->out)));
    }

    explicit TxInUndoDeserializer(Coin* coin) : txout(coin) {}
};

static const size_t MIN_TRANSACTION_INPUT_WEIGHT = WITNESS_SCALE_FACTOR * ::GetSerializeSize(CTxIn(), PROTOCOL_VERSION);
static const size_t MAX_INPUTS_PER_BLOCK = MAX_BLOCK_WEIGHT / MIN_TRANSACTION_INPUT_WEIGHT;

/*撤消ctransaction的信息*/
class CTxUndo
{
public:
//撤消所有Txin的信息
    std::vector<Coin> vprevout;

    template <typename Stream>
    void Serialize(Stream& s) const {
//TODO:避免重新实现向量序列化程序
        uint64_t count = vprevout.size();
        ::Serialize(s, COMPACTSIZE(REF(count)));
        for (const auto& prevout : vprevout) {
            ::Serialize(s, TxInUndoSerializer(&prevout));
        }
    }

    template <typename Stream>
    void Unserialize(Stream& s) {
//TODO:避免重新实现向量反序列化程序
        uint64_t count = 0;
        ::Unserialize(s, COMPACTSIZE(count));
        if (count > MAX_INPUTS_PER_BLOCK) {
            throw std::ios_base::failure("Too many input undo records");
        }
        vprevout.resize(count);
        for (auto& prevout : vprevout) {
            ::Unserialize(s, TxInUndoDeserializer(&prevout));
        }
    }
};

/*撤消cblock的信息*/
class CBlockUndo
{
public:
std::vector<CTxUndo> vtxundo; //除了硬币库

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vtxundo);
    }
};

#endif //比特科因
