
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

#include <bloom.h>

#include <primitives/transaction.h>
#include <hash.h>
#include <script/script.h>
#include <script/standard.h>
#include <random.h>
#include <streams.h>

#include <math.h>
#include <stdlib.h>


#define LN2SQUARED 0.4804530139182014246671025263266649717305529515945455
#define LN2 0.6931471805599453094172321214581765680755001343602552

CBloomFilter::CBloomFilter(const unsigned int nElements, const double nFPRate, const unsigned int nTweakIn, unsigned char nFlagsIn) :
    /*
     *对于具有给定数量元素和假阳性率的Bloom过滤器，理想尺寸为：
     *-nelements*日志（fp率）/ln（2）^2
     *我们忽略将创建大于协议限制的Bloom筛选器的筛选器参数。
     **/

    vData(std::min((unsigned int)(-1  / LN2SQUARED * nElements * log(nFPRate)), MAX_BLOOM_FILTER_SIZE * 8) / 8),
    /*
     *哈希函数的理想数量是过滤器大小*ln（2）/元素数量
     *同样，我们忽略了过滤器参数，这些参数将创建一个散列函数多于协议限制的bloom过滤器。
     *有关这些公式的解释，请参阅https://en.wikipedia.org/wiki/bloom_filter。
     **/

    isFull(false),
    isEmpty(true),
    nHashFuncs(std::min((unsigned int)(vData.size() * 8 / nElements * LN2), MAX_HASH_FUNCS)),
    nTweak(nTweakIn),
    nFlags(nFlagsIn)
{
}

inline unsigned int CBloomFilter::Hash(unsigned int nHashNum, const std::vector<unsigned char>& vDataToHash) const
{
//选择0xFBA4C795，因为它保证nhashnum值之间的合理位差。
    return MurmurHash3(nHashNum * 0xFBA4C795 + nTweak, vDataToHash) % (vData.size() * 8);
}

void CBloomFilter::insert(const std::vector<unsigned char>& vKey)
{
    if (isFull)
        return;
    for (unsigned int i = 0; i < nHashFuncs; i++)
    {
        unsigned int nIndex = Hash(i, vKey);
//设置vdata的位nindex
        vData[nIndex >> 3] |= (1 << (7 & nIndex));
    }
    isEmpty = false;
}

void CBloomFilter::insert(const COutPoint& outpoint)
{
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << outpoint;
    std::vector<unsigned char> data(stream.begin(), stream.end());
    insert(data);
}

void CBloomFilter::insert(const uint256& hash)
{
    std::vector<unsigned char> data(hash.begin(), hash.end());
    insert(data);
}

bool CBloomFilter::contains(const std::vector<unsigned char>& vKey) const
{
    if (isFull)
        return true;
    if (isEmpty)
        return false;
    for (unsigned int i = 0; i < nHashFuncs; i++)
    {
        unsigned int nIndex = Hash(i, vKey);
//检查vdata的位nindex
        if (!(vData[nIndex >> 3] & (1 << (7 & nIndex))))
            return false;
    }
    return true;
}

bool CBloomFilter::contains(const COutPoint& outpoint) const
{
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << outpoint;
    std::vector<unsigned char> data(stream.begin(), stream.end());
    return contains(data);
}

bool CBloomFilter::contains(const uint256& hash) const
{
    std::vector<unsigned char> data(hash.begin(), hash.end());
    return contains(data);
}

void CBloomFilter::clear()
{
    vData.assign(vData.size(),0);
    isFull = false;
    isEmpty = true;
}

void CBloomFilter::reset(const unsigned int nNewTweak)
{
    clear();
    nTweak = nNewTweak;
}

bool CBloomFilter::IsWithinSizeConstraints() const
{
    return vData.size() <= MAX_BLOOM_FILTER_SIZE && nHashFuncs <= MAX_HASH_FUNCS;
}

bool CBloomFilter::IsRelevantAndUpdate(const CTransaction& tx)
{
    bool fFound = false;
//如果筛选器包含Tx的哈希，则匹配
//当它们出现在一个块中时查找tx
    if (isFull)
        return true;
    if (isEmpty)
        return false;
    const uint256& hash = tx.GetHash();
    if (contains(hash))
        fFound = true;

    for (unsigned int i = 0; i < tx.vout.size(); i++)
    {
        const CTxOut& txout = tx.vout[i];
//如果筛选器在Tx中的任何ScriptPubKey中包含任何任意脚本数据元素，则匹配
//如果匹配，也添加匹配的特定输出。
//这意味着当新的相关Tx出现时，客户机不必自己更新过滤器。
//是为了找到支出交易而发现的，这样可以避免往返和竞争条件。
        CScript::const_iterator pc = txout.scriptPubKey.begin();
        std::vector<unsigned char> data;
        while (pc < txout.scriptPubKey.end())
        {
            opcodetype opcode;
            if (!txout.scriptPubKey.GetOp(pc, opcode, data))
                break;
            if (data.size() != 0 && contains(data))
            {
                fFound = true;
                if ((nFlags & BLOOM_UPDATE_MASK) == BLOOM_UPDATE_ALL)
                    insert(COutPoint(hash, i));
                else if ((nFlags & BLOOM_UPDATE_MASK) == BLOOM_UPDATE_P2PUBKEY_ONLY)
                {
                    std::vector<std::vector<unsigned char> > vSolutions;
                    txnouttype type = Solver(txout.scriptPubKey, vSolutions);
                    if (type == TX_PUBKEY || type == TX_MULTISIG) {
                        insert(COutPoint(hash, i));
                    }
                }
                break;
            }
        }
    }

    if (fFound)
        return true;

    for (const CTxIn& txin : tx.vin)
    {
//如果筛选器包含输出点Tx开销，则匹配
        if (contains(txin.prevout))
            return true;

//如果筛选器在Tx中的任何scriptsig中包含任何任意脚本数据元素，则匹配
        CScript::const_iterator pc = txin.scriptSig.begin();
        std::vector<unsigned char> data;
        while (pc < txin.scriptSig.end())
        {
            opcodetype opcode;
            if (!txin.scriptSig.GetOp(pc, opcode, data))
                break;
            if (data.size() != 0 && contains(data))
                return true;
        }
    }

    return false;
}

void CBloomFilter::UpdateEmptyFull()
{
    bool full = true;
    bool empty = true;
    for (unsigned int i = 0; i < vData.size(); i++)
    {
        full &= vData[i] == 0xff;
        empty &= vData[i] == 0;
    }
    isFull = full;
    isEmpty = empty;
}

CRollingBloomFilter::CRollingBloomFilter(const unsigned int nElements, const double fpRate)
{
    double logFpRate = log(fpRate);
    /*哈希函数的最佳数目是log（fprate）/log（0.5），但是
     *限制在1-50范围内。*/

    nHashFuncs = std::max(1, std::min((int)round(logFpRate / log(0.5)), 50));
    /*在这个滚动的Bloom过滤器中，我们将存储2到3代Nelements/2条目。*/
    nEntriesPerGeneration = (nElements + 1) / 2;
    uint32_t nMaxElements = nEntriesPerGeneration * 3;
    /*最大fprate=pow（1.0-exp（-nhashfuncs*nmaxelements/nfilterbits），nhashfuncs）
     *=>功率（fprate，1.0/nhashfuncs）=1.0-exp（-nhashfuncs*nmaxelements/nfilterbits）
     *=>1.0-功率（fprate，1.0/nhashfuncs）=exp（-nhashfuncs*nmaxelements/nfilterbits）
     *=>日志（1.0-pow（fprate，1.0/nhashfuncs））=-nhashfuncs*nmaxelements/nfilterbits
     *=>nFieldBits=-nHashFuncs*n像素/日志（1.0-功率（fprate，1.0/nHashFuncs））。
     *=>nFieldBits=-nhashfuncs*nmaxelements/log（1.0-exp（logfprate/nhashfuncs））。
     **/

    uint32_t nFilterBits = (uint32_t)ceil(-1.0 * nHashFuncs * nMaxElements / log(1.0 - exp(logFpRate / nHashFuncs)));
    data.clear();
    /*对于每个数据元素，我们需要存储2位。如果两个位都是0，则
     *位被视为未设置。如果位为（01）、（10）或（11），则位为
     *分别按第1、2或3代中的设置处理。
     *这些位存储在单独的整数中：位置P对应位
     *整数数据的（P&63）[（P>>6）*2]和数据的（P>>6）*2+1]。*/

    data.resize(((nFilterBits + 63) / 64) << 1);
    reset();
}

/*类似于cbloomfilter:：hash*/
static inline uint32_t RollingBloomHash(unsigned int nHashNum, uint32_t nTweak, const std::vector<unsigned char>& vDataToHash) {
    return MurmurHash3(nHashNum * 0xFBA4C795 + nTweak, vDataToHash);
}


//x%n的替换。这假设x和n是32位整数，x是均匀随机分布的32位值。
//这应该是一个很好的哈希的情况。
//参见https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reducation/
static inline uint32_t FastMod(uint32_t x, size_t n) {
    return ((uint64_t)x * (uint64_t)n) >> 32;
}

void CRollingBloomFilter::insert(const std::vector<unsigned char>& vKey)
{
    if (nEntriesThisGeneration == nEntriesPerGeneration) {
        nEntriesThisGeneration = 0;
        nGeneration++;
        if (nGeneration == 4) {
            nGeneration = 1;
        }
        uint64_t nGenerationMask1 = 0 - (uint64_t)(nGeneration & 1);
        uint64_t nGenerationMask2 = 0 - (uint64_t)(nGeneration >> 1);
        /*清除使用此生成号的旧条目。*/
        for (uint32_t p = 0; p < data.size(); p += 2) {
            uint64_t p1 = data[p], p2 = data[p + 1];
            uint64_t mask = (p1 ^ nGenerationMask1) | (p2 ^ nGenerationMask2);
            data[p] = p1 & mask;
            data[p + 1] = p2 & mask;
        }
    }
    nEntriesThisGeneration++;

    for (int n = 0; n < nHashFuncs; n++) {
        uint32_t h = RollingBloomHash(n, nTweak, vKey);
        int bit = h & 0x3F;
        /*fastmod与h的高位一起工作，因此忽略h的低位已经用于bit是安全的。*/
        uint32_t pos = FastMod(h, data.size());
        /*pos的最低位被忽略，第一位设置为零，第二位设置为1。*/
        data[pos & ~1] = (data[pos & ~1] & ~(((uint64_t)1) << bit)) | ((uint64_t)(nGeneration & 1)) << bit;
        data[pos | 1] = (data[pos | 1] & ~(((uint64_t)1) << bit)) | ((uint64_t)(nGeneration >> 1)) << bit;
    }
}

void CRollingBloomFilter::insert(const uint256& hash)
{
    std::vector<unsigned char> vData(hash.begin(), hash.end());
    insert(vData);
}

bool CRollingBloomFilter::contains(const std::vector<unsigned char>& vKey) const
{
    for (int n = 0; n < nHashFuncs; n++) {
        uint32_t h = RollingBloomHash(n, nTweak, vKey);
        int bit = h & 0x3F;
        uint32_t pos = FastMod(h, data.size());
        /*如果相关位未在数据[pos&~1]或数据[pos 1]中设置，则过滤器不包含vkey*/
        if (!(((data[pos & ~1] | data[pos | 1]) >> bit) & 1)) {
            return false;
        }
    }
    return true;
}

bool CRollingBloomFilter::contains(const uint256& hash) const
{
    std::vector<unsigned char> vData(hash.begin(), hash.end());
    return contains(vData);
}

void CRollingBloomFilter::reset()
{
    nTweak = GetRand(std::numeric_limits<unsigned int>::max());
    nEntriesThisGeneration = 0;
    nGeneration = 1;
    for (std::vector<uint64_t>::iterator it = data.begin(); it != data.end(); it++) {
        *it = 0;
    }
}
