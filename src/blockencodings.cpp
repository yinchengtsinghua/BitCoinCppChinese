
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2016-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <blockencodings.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <chainparams.h>
#include <crypto/sha256.h>
#include <crypto/siphash.h>
#include <random.h>
#include <streams.h>
#include <txmempool.h>
#include <validation.h>
#include <util/system.h>

#include <unordered_map>

CBlockHeaderAndShortTxIDs::CBlockHeaderAndShortTxIDs(const CBlock& block, bool fUseWTXID) :
        nonce(GetRand(std::numeric_limits<uint64_t>::max())),
        shorttxids(block.vtx.size() - 1), prefilledtxn(1), header(block) {
    FillShortTxIDSelector();
//TODO:在接受块之前使用我们的mempool来预测填充的内容不仅仅是coinbase
    prefilledtxn[0] = {0, block.vtx[0]};
    for (size_t i = 1; i < block.vtx.size(); i++) {
        const CTransaction& tx = *block.vtx[i];
        shorttxids[i - 1] = GetShortID(fUseWTXID ? tx.GetWitnessHash() : tx.GetHash());
    }
}

void CBlockHeaderAndShortTxIDs::FillShortTxIDSelector() const {
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << header << nonce;
    CSHA256 hasher;
    hasher.Write((unsigned char*)&(*stream.begin()), stream.end() - stream.begin());
    uint256 shorttxidhash;
    hasher.Finalize(shorttxidhash.begin());
    shorttxidk0 = shorttxidhash.GetUint64(0);
    shorttxidk1 = shorttxidhash.GetUint64(1);
}

uint64_t CBlockHeaderAndShortTxIDs::GetShortID(const uint256& txhash) const {
    static_assert(SHORTTXIDS_LENGTH == 6, "shorttxids calculation assumes 6-byte shorttxids");
    return SipHashUint256(shorttxidk0, shorttxidk1, txhash) & 0xffffffffffffL;
}



ReadStatus PartiallyDownloadedBlock::InitData(const CBlockHeaderAndShortTxIDs& cmpctblock, const std::vector<std::pair<uint256, CTransactionRef>>& extra_txn) {
    if (cmpctblock.header.IsNull() || (cmpctblock.shorttxids.empty() && cmpctblock.prefilledtxn.empty()))
        return READ_STATUS_INVALID;
    if (cmpctblock.shorttxids.size() + cmpctblock.prefilledtxn.size() > MAX_BLOCK_WEIGHT / MIN_SERIALIZABLE_TRANSACTION_WEIGHT)
        return READ_STATUS_INVALID;

    assert(header.IsNull() && txn_available.empty());
    header = cmpctblock.header;
    txn_available.resize(cmpctblock.BlockTxCount());

    int32_t lastprefilledindex = -1;
    for (size_t i = 0; i < cmpctblock.prefilledtxn.size(); i++) {
        if (cmpctblock.prefilledtxn[i].tx->IsNull())
            return READ_STATUS_INVALID;

lastprefilledindex += cmpctblock.prefilledtxn[i].index + 1; //索引是uint16，因此不能溢出此处
        if (lastprefilledindex > std::numeric_limits<uint16_t>::max())
            return READ_STATUS_INVALID;
        if ((uint32_t)lastprefilledindex > cmpctblock.shorttxids.size() + i) {
//如果我们在一个大于shortTxID完整列表的索引处插入一个Tx
//加上我们插入的预填充txn的数目，然后我们就得到了
//没有预先填充的txn或shorttxid！
            return READ_STATUS_INVALID;
        }
        txn_available[lastprefilledindex] = cmpctblock.prefilledtxn[i].tx;
    }
    prefilled_count = cmpctblock.prefilledtxn.size();

//计算txid->positions的地图并检查mempool以查看我们有什么（或没有）
//因为格式良好的cmpctblock消息将具有（相对）统一的分布
//对于较短的ID，任何高度不均匀的元素分布都可以安全地视为
//读取“状态”失败。
    std::unordered_map<uint64_t, uint16_t> shorttxids(cmpctblock.shorttxids.size());
    uint16_t index_offset = 0;
    for (size_t i = 0; i < cmpctblock.shorttxids.size(); i++) {
        while (txn_available[i + index_offset])
            index_offset++;
        shorttxids[cmpctblock.shorttxids[i]] = i + index_offset;
//为了确定一个存储桶中条目数超过n的可能性，
//我们使用一个桶中元素的数量是
//二项式分布（n=shortTxID的数量s，p=
//1/桶数），最坏情况下，桶数为
//等于s（由于std:：unordered_映射的默认加载因子为1.0），
//任何一个桶超过n个元素的机会最多是
//bucket*（任何给定bucket高于n个元素的概率）。
//因此：p（max_elements_per_bucket>n）<=s*（1-cdf（二项式（n=s，p=1/s），n））。
//如果我们假设块高达16000，那么每个桶允许12个元素
//每100万个块传输（每个对等端和连接）仅失败一次。
        if (shorttxids.bucket_size(shorttxids.bucket(cmpctblock.shorttxids[i])) > 12)
            return READ_STATUS_FAILED;
    }
//TODO:在shortid冲突的情况下，我们应该请求两个事务
//它相撞了。返回到完整的块请求是多余的。
    if (shorttxids.size() != cmpctblock.shorttxids.size())
return READ_STATUS_FAILED; //短ID冲突

    std::vector<bool> have_txn(txn_available.size());
    {
    LOCK(pool->cs);
    const std::vector<std::pair<uint256, CTxMemPool::txiter> >& vTxHashes = pool->vTxHashes;
    for (size_t i = 0; i < vTxHashes.size(); i++) {
        uint64_t shortid = cmpctblock.GetShortID(vTxHashes[i].first);
        std::unordered_map<uint64_t, uint16_t>::iterator idit = shorttxids.find(shortid);
        if (idit != shorttxids.end()) {
            if (!have_txn[idit->second]) {
                txn_available[idit->second] = vTxHashes[i].second->GetSharedTx();
                have_txn[idit->second]  = true;
                mempool_count++;
            } else {
//如果我们找到两个匹配短ID的mempool txn，就请求它。
//这应该是非常罕见的，额外的带宽不重要，
//但是吃一顿因填充块故障引起的往返旅行会很烦人。
                if (txn_available[idit->second]) {
                    txn_available[idit->second].reset();
                    mempool_count--;
                }
            }
        }
//虽然理想情况下我们会继续扫描两个txn匹配的shortid案例，
//早退的表现太好了，不值得放弃。
//额外的风险。
        if (mempool_count == shorttxids.size())
            break;
    }
    }

    for (size_t i = 0; i < extra_txn.size(); i++) {
        uint64_t shortid = cmpctblock.GetShortID(extra_txn[i].first);
        std::unordered_map<uint64_t, uint16_t>::iterator idit = shorttxids.find(shortid);
        if (idit != shorttxids.end()) {
            if (!have_txn[idit->second]) {
                txn_available[idit->second] = extra_txn[i].second;
                have_txn[idit->second]  = true;
                mempool_count++;
                extra_count++;
            } else {
//如果我们找到两个匹配短ID的mempool/extra txn，只要
//请求它。
//这应该是非常罕见的，额外的带宽不重要，
//但是吃一顿因填充块故障引起的往返旅行会很烦人。
//注意，我们不希望在额外的内存池和内存池之间进行复制
//触发这个案例，所以我们首先比较证人哈希
                if (txn_available[idit->second] &&
                        txn_available[idit->second]->GetWitnessHash() != extra_txn[i].second->GetWitnessHash()) {
                    txn_available[idit->second].reset();
                    mempool_count--;
                    extra_count--;
                }
            }
        }
//虽然理想情况下我们会继续扫描两个txn匹配的shortid案例，
//早退的表现太好了，不值得放弃。
//额外的风险。
        if (mempool_count == shorttxids.size())
            break;
    }

    LogPrint(BCLog::CMPCTBLOCK, "Initialized PartiallyDownloadedBlock for block %s using a cmpctblock of size %lu\n", cmpctblock.header.GetHash().ToString(), GetSerializeSize(cmpctblock, PROTOCOL_VERSION));

    return READ_STATUS_OK;
}

bool PartiallyDownloadedBlock::IsTxAvailable(size_t index) const {
    assert(!header.IsNull());
    assert(index < txn_available.size());
    return txn_available[index] != nullptr;
}

ReadStatus PartiallyDownloadedBlock::FillBlock(CBlock& block, const std::vector<CTransactionRef>& vtx_missing) {
    assert(!header.IsNull());
    uint256 hash = header.GetHash();
    block = header;
    block.vtx.resize(txn_available.size());

    size_t tx_missing_offset = 0;
    for (size_t i = 0; i < txn_available.size(); i++) {
        if (!txn_available[i]) {
            if (vtx_missing.size() <= tx_missing_offset)
                return READ_STATUS_INVALID;
            block.vtx[i] = vtx_missing[tx_missing_offset++];
        } else
            block.vtx[i] = std::move(txn_available[i]);
    }

//确保我们不能再次调用FillBlock。
    header.SetNull();
    txn_available.clear();

    if (vtx_missing.size() != tx_missing_offset)
        return READ_STATUS_INVALID;

    CValidationState state;
    if (!CheckBlock(block, state, Params().GetConsensus())) {
//托多：我们真的想在这里手动检查Merkle树，
//但那很贵，而且checkblock缓存一个块的
//“已检查状态”（在cblock中？）.cblock应该能够
//检查它自己的merkle根目录并缓存该检查。
        if (state.CorruptionPossible())
return READ_STATUS_FAILED; //可能发生短ID冲突
        return READ_STATUS_CHECKBLOCK_FAILED;
    }

    LogPrint(BCLog::CMPCTBLOCK, "Successfully reconstructed block %s with %lu txn prefilled, %lu txn from mempool (incl at least %lu from extra pool) and %lu txn requested\n", hash.ToString(), prefilled_count, mempool_count, extra_count, vtx_missing.size());
    if (vtx_missing.size() < 5) {
        for (const auto& tx : vtx_missing) {
            LogPrint(BCLog::CMPCTBLOCK, "Reconstructed block %s required tx %s\n", hash.ToString(), tx->GetHash().ToString());
        }
    }

    return READ_STATUS_OK;
}
