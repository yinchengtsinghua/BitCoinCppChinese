
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2012 Pieter Wuille
//版权所有（c）2012-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_ADDRMAN_H
#define BITCOIN_ADDRMAN_H

#include <netaddress.h>
#include <protocol.h>
#include <random.h>
#include <sync.h>
#include <timedata.h>
#include <util/system.h>

#include <map>
#include <set>
#include <stdint.h>
#include <vector>

/*
 *球童的扩展统计
 **/

class CAddrInfo : public CAddress
{
public:
//！我们的最后一次尝试（仅限记忆）
    int64_t nLastTry{0};

//！上次计数的尝试（仅内存）
    int64_t nLastCountAttempt{0};

private:
//！关于这个地址的知识最初来自哪里
    CNetAddr source;

//！我们上次成功连接
    int64_t nLastSuccess{0};

//！自上次成功尝试后的连接尝试
    int nAttempts{0};

//！新集合中的引用计数（仅内存）
    int nRefCount{0};

//！试过了吗？（仅内存）
    bool fInTried{false};

//！Vrandom中的位置
    int nRandomPos{-1};

    friend class CAddrMan;

public:

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(CAddress, *this);
        READWRITE(source);
        READWRITE(nLastSuccess);
        READWRITE(nAttempts);
    }

    CAddrInfo(const CAddress &addrIn, const CNetAddr &addrSource) : CAddress(addrIn), source(addrSource)
    {
    }

    CAddrInfo() : CAddress(), source()
    {
    }

//！计算此项所属的“已尝试”存储桶
    int GetTriedBucket(const uint256 &nKey) const;

//！计算此项所属的“新”存储桶，给定一个源
    int GetNewBucket(const uint256 &nKey, const CNetAddr& src) const;

//！使用其默认源计算此条目所属的“新”存储桶
    int GetNewBucket(const uint256 &nKey) const
    {
        return GetNewBucket(nKey, source);
    }

//！计算存储此条目的存储桶的位置。
    int GetBucketPosition(const uint256 &nKey, bool fNew, int nBucket) const;

//！确定此项的统计信息是否足够糟糕，以便可以将其删除
    bool IsTerrible(int64_t nNow = GetAdjustedTime()) const;

//！计算在选择要连接的节点时应提供此项的相对机会
    double GetChance(int64_t nNow = GetAdjustedTime()) const;
};

/*随机地址管理器
 *
 ＊设计目标：
 **将地址表保存在内存中，并将整个表异步转储到peers.dat。
 **确保没有（本地化的）攻击者可以用其节点/地址填充整个表。
 *
 *为此目的：
 **地址被组织成存储桶。
 **尚未尝试的地址将进入1024个“新”存储桶。
 **根据信息源的地址范围（IPv4为/16），随机选择64个存储桶。
 **实际存储桶是根据地址本身所在的范围从其中一个存储桶中选择的。
 **一个地址最多可以出现在8个不同的存储桶中，以增加对以下地址的选择机会：
 *常见。增加这种多重性的机会以指数形式减少。
 **将新地址添加到一个完整的存储桶中时，随机选择一个条目（带有最近较少看到的偏好
 *个）先从中删除。
 **已知可访问的节点地址进入256个“已尝试”存储桶。
 **每个地址范围随机选择这些存储桶中的8个。
 **实际存储桶是根据完整地址从其中一个存储桶中选择的。
 **在将新的好地址添加到一个完整的存储桶时，随机选择一个条目（最近偏向较少）
 *尝试过的）被从里面取出，回到“新”桶里。
 **存储桶选择基于加密散列，使用随机生成的256位密钥，不应
 *对手可以观察到。
 **为了提高性能，保留了几个索引。定义debug-addrman将引入频繁的（昂贵的）
 *对整个数据结构进行一致性检查。
 **/


//！已尝试地址的存储桶总数
#define ADDRMAN_TRIED_BUCKET_COUNT_LOG2 8

//！新地址的存储桶总数
#define ADDRMAN_NEW_BUCKET_COUNT_LOG2 10

//！新地址和尝试地址的存储桶中允许的最大条目数
#define ADDRMAN_BUCKET_SIZE_LOG2 6

//！在一个组中有多少个具有尝试的地址的存储桶条目（对于IPv4为/16）被分布
#define ADDRMAN_TRIED_BUCKETS_PER_GROUP 8

//！分布了来自单个组的具有新地址的存储桶条目数
#define ADDRMAN_NEW_BUCKETS_PER_SOURCE_GROUP 64

//！对于具有新地址的条目，可能出现单个地址的存储桶数
#define ADDRMAN_NEW_BUCKETS_PER_ADDRESS 8

//！地址最大可能有多旧
#define ADDRMAN_HORIZON_DAYS 30

//！在尝试了多少次失败后，我们放弃了一个新节点
#define ADDRMAN_RETRIES 3

//！允许连续失败的次数…
#define ADDRMAN_MAX_FAILURES 10

//！…至少在这么多天内
#define ADDRMAN_MIN_FAIL_DAYS 7

//！在允许将地址从“尝试”中移出之前，成功连接的时间应该是多久？
#define ADDRMAN_REPLACEMENT_HOURS 4

//！在getaddr调用中要返回的最大节点百分比
#define ADDRMAN_GETADDR_MAX_PCT 23

//！在getaddr调用中要返回的最大节点数
#define ADDRMAN_GETADDR_MAX 2500

//！便利性
#define ADDRMAN_TRIED_BUCKET_COUNT (1 << ADDRMAN_TRIED_BUCKET_COUNT_LOG2)
#define ADDRMAN_NEW_BUCKET_COUNT (1 << ADDRMAN_NEW_BUCKET_COUNT_LOG2)
#define ADDRMAN_BUCKET_SIZE (1 << ADDRMAN_BUCKET_SIZE_LOG2)

//！要存储的尝试的addr冲突的最大数目
#define ADDRMAN_SET_TRIED_COLLISION_SIZE 10

/*
 *随机（IP）地址管理器
 **/

class CAddrMan
{
protected:
//！保护内部数据结构的关键部分
    mutable CCriticalSection cs;

private:
//！最后使用的NID
    int nIdCount GUARDED_BY(cs);

//！包含所有NID信息的表
    std::map<int, CAddrInfo> mapInfo GUARDED_BY(cs);

//！根据网络地址查找NID
    std::map<CNetAddr, int> mapAddr GUARDED_BY(cs);

//！所有NID的随机有序向量
    std::vector<int> vRandom GUARDED_BY(cs);

//“已尝试”条目数
    int nTried GUARDED_BY(cs);

//！“已尝试”存储桶列表
    int vvTried[ADDRMAN_TRIED_BUCKET_COUNT][ADDRMAN_BUCKET_SIZE] GUARDED_BY(cs);

//！（唯一）“新”条目数
    int nNew GUARDED_BY(cs);

//！“新”桶列表
    int vvNew[ADDRMAN_NEW_BUCKET_COUNT][ADDRMAN_BUCKET_SIZE] GUARDED_BY(cs);

//！上次调用good时（仅限内存）
    int64_t nLastGood GUARDED_BY(cs);

//！保留插入到与现有项冲突的已尝试表中的加法器。驱逐前的测试用于解决这些冲突。
    std::set<int> m_tried_collisions;

protected:
//！随机选择存储桶的密钥
    uint256 nKey;

//！内环随机数的来源
    FastRandomContext insecure_rand;

//！查找条目。
    CAddrInfo* Find(const CNetAddr& addr, int *pnId = nullptr) EXCLUSIVE_LOCKS_REQUIRED(cs);

//！找到一个条目，必要时创建它。
//！如果需要，将更新找到的节点的时间和服务。
    CAddrInfo* Create(const CAddress &addr, const CNetAddr &addrSource, int *pnId = nullptr) EXCLUSIVE_LOCKS_REQUIRED(cs);

//！交换vrandom中的两个元素。
    void SwapRandom(unsigned int nRandomPos1, unsigned int nRandomPos2) EXCLUSIVE_LOCKS_REQUIRED(cs);

//！将条目从“新”表移动到“已尝试”表
    void MakeTried(CAddrInfo& info, int nId) EXCLUSIVE_LOCKS_REQUIRED(cs);

//！删除条目。它不能处于尝试状态，并且具有refcount 0。
    void Delete(int nId) EXCLUSIVE_LOCKS_REQUIRED(cs);

//！清除“新”表中的位置。这是实际删除条目的唯一位置。
    void ClearNew(int nUBucket, int nUBucketPos) EXCLUSIVE_LOCKS_REQUIRED(cs);

//！标记一个条目“好”，可能将其从“新”移动到“尝试”。
    void Good_(const CService &addr, bool test_before_evict, int64_t time) EXCLUSIVE_LOCKS_REQUIRED(cs);

//！在“新”表中添加一个条目。
    bool Add_(const CAddress &addr, const CNetAddr& source, int64_t nTimePenalty) EXCLUSIVE_LOCKS_REQUIRED(cs);

//！将条目标记为“尝试连接”。
    void Attempt_(const CService &addr, bool fCountFailure, int64_t nTime) EXCLUSIVE_LOCKS_REQUIRED(cs);

//！选择要连接的地址，如果newOnly设置为true，则仅从新表中选择。
    CAddrInfo Select_(bool newOnly) EXCLUSIVE_LOCKS_REQUIRED(cs);

//！查看是否测试过任何要收回的已尝试表条目，如果是，请解决冲突。
    void ResolveCollisions_() EXCLUSIVE_LOCKS_REQUIRED(cs);

//！返回一个随机的被逐出的尝试过的表地址。
    CAddrInfo SelectTriedCollision_() EXCLUSIVE_LOCKS_REQUIRED(cs);

#ifdef DEBUG_ADDRMAN
//！执行一致性检查。返回错误代码或零。
    int Check_() EXCLUSIVE_LOCKS_REQUIRED(cs);
#endif

//！一次选择多个地址。
    void GetAddr_(std::vector<CAddress> &vAddr) EXCLUSIVE_LOCKS_REQUIRED(cs);

//！将条目标记为当前连接到。
    void Connected_(const CService &addr, int64_t nTime) EXCLUSIVE_LOCKS_REQUIRED(cs);

//！更新条目的服务位。
    void SetServices_(const CService &addr, ServiceFlags nServices) EXCLUSIVE_LOCKS_REQUIRED(cs);

public:
    /*
     *序列化格式：
     **版本字节（当前为1）
     **0x20+nkey（序列化为矢量，以便向后兼容）
     *NNEW
     ***
     **新桶数xor 2**30
     **VVNew中的所有新addrinfos
     **vvtry中的所有中心addrinfos
     **每个桶：
     **元素数量
     **对于每个元素：索引
     *
     *2**30与存储桶数量相关，以使addrman反序列化程序v0检测到它。
     *不兼容。这是必要的，因为它没有检查版本号
     *反序列化。
     *
     *请注意，vvtry、mapaddr和vvector从未显式编码；
     *而是从其他信息中重建。
     *
     *vvnew已序列化，但仅在addrman_unknown_bucket_count未更改时使用，
     *否则，它也会被重建。
     *
     *此格式更复杂，但明显更小（最多1.5 mib），并且支持
     *在不破坏磁盘结构的情况下更改addrman参数。
     *
     *由于序列化和反序列化代码具有
     *几乎没有共同点。
     **/

    template<typename Stream>
    void Serialize(Stream &s) const
    {
        LOCK(cs);

        unsigned char nVersion = 1;
        s << nVersion;
        s << ((unsigned char)32);
        s << nKey;
        s << nNew;
        s << nTried;

        int nUBuckets = ADDRMAN_NEW_BUCKET_COUNT ^ (1 << 30);
        s << nUBuckets;
        std::map<int, int> mapUnkIds;
        int nIds = 0;
        for (const auto& entry : mapInfo) {
            mapUnkIds[entry.first] = nIds;
            const CAddrInfo &info = entry.second;
            if (info.nRefCount) {
assert(nIds != nNew); //这意味着新来的错了，噢
                s << info;
                nIds++;
            }
        }
        nIds = 0;
        for (const auto& entry : mapInfo) {
            const CAddrInfo &info = entry.second;
            if (info.fInTried) {
assert(nIds != nTried); //这意味着NTRIED错了，噢
                s << info;
                nIds++;
            }
        }
        for (int bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; bucket++) {
            int nSize = 0;
            for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
                if (vvNew[bucket][i] != -1)
                    nSize++;
            }
            s << nSize;
            for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
                if (vvNew[bucket][i] != -1) {
                    int nIndex = mapUnkIds[vvNew[bucket][i]];
                    s << nIndex;
                }
            }
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s)
    {
        LOCK(cs);

        Clear();

        unsigned char nVersion;
        s >> nVersion;
        unsigned char nKeySize;
        s >> nKeySize;
        if (nKeySize != 32) throw std::ios_base::failure("Incorrect keysize in addrman deserialization");
        s >> nKey;
        s >> nNew;
        s >> nTried;
        int nUBuckets = 0;
        s >> nUBuckets;
        if (nVersion != 0) {
            nUBuckets ^= (1 << 30);
        }

        if (nNew > ADDRMAN_NEW_BUCKET_COUNT * ADDRMAN_BUCKET_SIZE) {
            throw std::ios_base::failure("Corrupt CAddrMan serialization, nNew exceeds limit.");
        }

        if (nTried > ADDRMAN_TRIED_BUCKET_COUNT * ADDRMAN_BUCKET_SIZE) {
            throw std::ios_base::failure("Corrupt CAddrMan serialization, nTried exceeds limit.");
        }

//反序列化新表中的条目。
        for (int n = 0; n < nNew; n++) {
            CAddrInfo &info = mapInfo[n];
            s >> info;
            mapAddr[info] = n;
            info.nRandomPos = vRandom.size();
            vRandom.push_back(n);
            if (nVersion != 1 || nUBuckets != ADDRMAN_NEW_BUCKET_COUNT) {
//如果无法使用新的表数据（版本未知或存储桶计数错误），请执行以下操作：
//立即尝试根据它们的主源地址给它们一个引用。
                int nUBucket = info.GetNewBucket(nKey);
                int nUBucketPos = info.GetBucketPosition(nKey, true, nUBucket);
                if (vvNew[nUBucket][nUBucketPos] == -1) {
                    vvNew[nUBucket][nUBucketPos] = n;
                    info.nRefCount++;
                }
            }
        }
        nIdCount = nNew;

//反序列化已尝试表中的条目。
        int nLost = 0;
        for (int n = 0; n < nTried; n++) {
            CAddrInfo info;
            s >> info;
            int nKBucket = info.GetTriedBucket(nKey);
            int nKBucketPos = info.GetBucketPosition(nKey, false, nKBucket);
            if (vvTried[nKBucket][nKBucketPos] == -1) {
                info.nRandomPos = vRandom.size();
                info.fInTried = true;
                vRandom.push_back(nIdCount);
                mapInfo[nIdCount] = info;
                mapAddr[info] = nIdCount;
                vvTried[nKBucket][nKBucketPos] = nIdCount;
                nIdCount++;
            } else {
                nLost++;
            }
        }
        nTried -= nLost;

//反序列化新表中的位置（如果可能）。
        for (int bucket = 0; bucket < nUBuckets; bucket++) {
            int nSize = 0;
            s >> nSize;
            for (int n = 0; n < nSize; n++) {
                int nIndex = 0;
                s >> nIndex;
                if (nIndex >= 0 && nIndex < nNew) {
                    CAddrInfo &info = mapInfo[nIndex];
                    int nUBucketPos = info.GetBucketPosition(nKey, true, bucket);
                    if (nVersion == 1 && nUBuckets == ADDRMAN_NEW_BUCKET_COUNT && vvNew[bucket][nUBucketPos] == -1 && info.nRefCount < ADDRMAN_NEW_BUCKETS_PER_ADDRESS) {
                        info.nRefCount++;
                        vvNew[bucket][nUBucketPos] = nIndex;
                    }
                }
            }
        }

//使用refcount 0删除新条目（由于冲突）。
        int nLostUnk = 0;
        for (std::map<int, CAddrInfo>::const_iterator it = mapInfo.begin(); it != mapInfo.end(); ) {
            if (it->second.fInTried == false && it->second.nRefCount == 0) {
                std::map<int, CAddrInfo>::const_iterator itCopy = it++;
                Delete(itCopy->first);
                nLostUnk++;
            } else {
                it++;
            }
        }
        if (nLost + nLostUnk > 0) {
            LogPrint(BCLog::ADDRMAN, "addrman lost %i new and %i tried addresses due to collisions\n", nLostUnk, nLost);
        }

        Check();
    }

    void Clear()
    {
        LOCK(cs);
        std::vector<int>().swap(vRandom);
        nKey = insecure_rand.rand256();
        for (size_t bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; bucket++) {
            for (size_t entry = 0; entry < ADDRMAN_BUCKET_SIZE; entry++) {
                vvNew[bucket][entry] = -1;
            }
        }
        for (size_t bucket = 0; bucket < ADDRMAN_TRIED_BUCKET_COUNT; bucket++) {
            for (size_t entry = 0; entry < ADDRMAN_BUCKET_SIZE; entry++) {
                vvTried[bucket][entry] = -1;
            }
        }

        nIdCount = 0;
        nTried = 0;
        nNew = 0;
nLastGood = 1; //最初是在1，所以“从不”是严格的更糟。
        mapInfo.clear();
        mapAddr.clear();
    }

    CAddrMan()
    {
        Clear();
    }

    ~CAddrMan()
    {
        nKey.SetNull();
    }

//！返回所有表中的（唯一）地址数。
    size_t size() const
    {
LOCK(cs); //TODO:将此缓存在原子中以避免此开销
        return vRandom.size();
    }

//！一致性检查
    void Check()
    {
#ifdef DEBUG_ADDRMAN
        {
            LOCK(cs);
            int err;
            if ((err=Check_()))
                LogPrintf("ADDRMAN CONSISTENCY CHECK FAILED!!! err=%i\n", err);
        }
#endif
    }

//！添加一个地址。
    bool Add(const CAddress &addr, const CNetAddr& source, int64_t nTimePenalty = 0)
    {
        LOCK(cs);
        bool fRet = false;
        Check();
        fRet |= Add_(addr, source, nTimePenalty);
        Check();
        if (fRet) {
            LogPrint(BCLog::ADDRMAN, "Added %s from %s: %i tried, %i new\n", addr.ToStringIPPort(), source.ToString(), nTried, nNew);
        }
        return fRet;
    }

//！添加多个地址。
    bool Add(const std::vector<CAddress> &vAddr, const CNetAddr& source, int64_t nTimePenalty = 0)
    {
        LOCK(cs);
        int nAdd = 0;
        Check();
        for (std::vector<CAddress>::const_iterator it = vAddr.begin(); it != vAddr.end(); it++)
            nAdd += Add_(*it, source, nTimePenalty) ? 1 : 0;
        Check();
        if (nAdd) {
            LogPrint(BCLog::ADDRMAN, "Added %i addresses from %s: %i tried, %i new\n", nAdd, source.ToString(), nTried, nNew);
        }
        return nAdd > 0;
    }

//！将条目标记为可访问。
    void Good(const CService &addr, bool test_before_evict = true, int64_t nTime = GetAdjustedTime())
    {
        LOCK(cs);
        Check();
        Good_(addr, test_before_evict, nTime);
        Check();
    }

//！将条目标记为试图连接到的。
    void Attempt(const CService &addr, bool fCountFailure, int64_t nTime = GetAdjustedTime())
    {
        LOCK(cs);
        Check();
        Attempt_(addr, fCountFailure, nTime);
        Check();
    }

//！查看是否测试过任何要收回的已尝试表条目，如果是，请解决冲突。
    void ResolveCollisions()
    {
        LOCK(cs);
        Check();
        ResolveCollisions_();
        Check();
    }

//！随机选择另一个地址试图退出的尝试中的地址。
    CAddrInfo SelectTriedCollision()
    {
        CAddrInfo ret;
        {
            LOCK(cs);
            Check();
            ret = SelectTriedCollision_();
            Check();
        }
        return ret;
    }

    /*
     *选择要连接的地址。
     **/

    CAddrInfo Select(bool newOnly = false)
    {
        CAddrInfo addrRet;
        {
            LOCK(cs);
            Check();
            addrRet = Select_(newOnly);
            Check();
        }
        return addrRet;
    }

//！返回一组随机选择的地址。
    std::vector<CAddress> GetAddr()
    {
        Check();
        std::vector<CAddress> vAddr;
        {
            LOCK(cs);
            GetAddr_(vAddr);
        }
        Check();
        return vAddr;
    }

//！将条目标记为当前连接到。
    void Connected(const CService &addr, int64_t nTime = GetAdjustedTime())
    {
        LOCK(cs);
        Check();
        Connected_(addr, nTime);
        Check();
    }

    void SetServices(const CService &addr, ServiceFlags nServices)
    {
        LOCK(cs);
        Check();
        SetServices_(addr, nServices);
        Check();
    }

};

#endif //比特币
