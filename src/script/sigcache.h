
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

#ifndef BITCOIN_SCRIPT_SIGCACHE_H
#define BITCOIN_SCRIPT_SIGCACHE_H

#include <script/interpreter.h>

#include <vector>

//DoS保护：将缓存大小限制为32MB（64位上超过1000000个条目）
//系统）。由于我们如何计算缓存大小，实际的内存使用量有点小
//更多（约32.25 MB）
static const unsigned int DEFAULT_MAX_SIG_CACHE_SIZE = 32;
//允许的最大SIG缓存大小
static const int64_t MAX_MAX_SIG_CACHE_SIZE = 16384;

class CPubKey;

/*
 *我们正在将一个nonce散列到条目本身中，因此不需要额外的
 *设置哈希计算中的盲法。
 *
 *这可能表现出平台端依赖的行为，但因为这些行为
 *非编码散列（随机），此状态仅在本地使用，是安全的。
 *重要的是本地一致性。
 **/

class SignatureCacheHasher
{
public:
    template <uint8_t hash_select>
    uint32_t operator()(const uint256& key) const
    {
        static_assert(hash_select <8, "SignatureCacheHasher only has 8 hashes available.");
        uint32_t u;
        std::memcpy(&u, key.begin()+4*hash_select, 4);
        return u;
    }
};

class CachingTransactionSignatureChecker : public TransactionSignatureChecker
{
private:
    bool store;

public:
    CachingTransactionSignatureChecker(const CTransaction* txToIn, unsigned int nInIn, const CAmount& amountIn, bool storeIn, PrecomputedTransactionData& txdataIn) : TransactionSignatureChecker(txToIn, nInIn, amountIn, txdataIn), store(storeIn) {}

    bool VerifySignature(const std::vector<unsigned char>& vchSig, const CPubKey& vchPubKey, const uint256& sighash) const override;
};

void InitSignatureCache();

#endif //比特币脚本信号缓存
