
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

#include <addrman.h>

#include <hash.h>
#include <serialize.h>
#include <streams.h>

int CAddrInfo::GetTriedBucket(const uint256& nKey) const
{
    uint64_t hash1 = (CHashWriter(SER_GETHASH, 0) << nKey << GetKey()).GetCheapHash();
    uint64_t hash2 = (CHashWriter(SER_GETHASH, 0) << nKey << GetGroup() << (hash1 % ADDRMAN_TRIED_BUCKETS_PER_GROUP)).GetCheapHash();
    return hash2 % ADDRMAN_TRIED_BUCKET_COUNT;
}

int CAddrInfo::GetNewBucket(const uint256& nKey, const CNetAddr& src) const
{
    std::vector<unsigned char> vchSourceGroupKey = src.GetGroup();
    uint64_t hash1 = (CHashWriter(SER_GETHASH, 0) << nKey << GetGroup() << vchSourceGroupKey).GetCheapHash();
    uint64_t hash2 = (CHashWriter(SER_GETHASH, 0) << nKey << vchSourceGroupKey << (hash1 % ADDRMAN_NEW_BUCKETS_PER_SOURCE_GROUP)).GetCheapHash();
    return hash2 % ADDRMAN_NEW_BUCKET_COUNT;
}

int CAddrInfo::GetBucketPosition(const uint256 &nKey, bool fNew, int nBucket) const
{
    uint64_t hash1 = (CHashWriter(SER_GETHASH, 0) << nKey << (fNew ? 'N' : 'K') << nBucket << GetKey()).GetCheapHash();
    return hash1 % ADDRMAN_BUCKET_SIZE;
}

bool CAddrInfo::IsTerrible(int64_t nNow) const
{
if (nLastTry && nLastTry >= nNow - 60) //不要删除最后一分钟尝试过的东西
        return false;

if (nTime > nNow + 10 * 60) //来了一个飞行的德洛伦人
        return true;

if (nTime == 0 || nNow - nTime > ADDRMAN_HORIZON_DAYS * 24 * 60 * 60) //在最近的历史中看不到
        return true;

if (nLastSuccess == 0 && nAttempts >= ADDRMAN_RETRIES) //尝试了N次却没有成功
        return true;

if (nNow - nLastSuccess > ADDRMAN_MIN_FAIL_DAYS * 24 * 60 * 60 && nAttempts >= ADDRMAN_MAX_FAILURES) //n上周连续失败
        return true;

    return false;
}

double CAddrInfo::GetChance(int64_t nNow) const
{
    double fChance = 1.0;
    int64_t nSinceLastTry = std::max<int64_t>(nNow - nLastTry, 0);

//贬低最近的尝试
    if (nSinceLastTry < 60 * 10)
        fChance *= 0.01;

//每次失败后都会降低66%，但最多1/28，以避免搜索永久性或过度惩罚停机。
    fChance *= pow(0.66, std::min(nAttempts, 8));

    return fChance;
}

CAddrInfo* CAddrMan::Find(const CNetAddr& addr, int* pnId)
{
    std::map<CNetAddr, int>::iterator it = mapAddr.find(addr);
    if (it == mapAddr.end())
        return nullptr;
    if (pnId)
        *pnId = (*it).second;
    std::map<int, CAddrInfo>::iterator it2 = mapInfo.find((*it).second);
    if (it2 != mapInfo.end())
        return &(*it2).second;
    return nullptr;
}

CAddrInfo* CAddrMan::Create(const CAddress& addr, const CNetAddr& addrSource, int* pnId)
{
    int nId = nIdCount++;
    mapInfo[nId] = CAddrInfo(addr, addrSource);
    mapAddr[addr] = nId;
    mapInfo[nId].nRandomPos = vRandom.size();
    vRandom.push_back(nId);
    if (pnId)
        *pnId = nId;
    return &mapInfo[nId];
}

void CAddrMan::SwapRandom(unsigned int nRndPos1, unsigned int nRndPos2)
{
    if (nRndPos1 == nRndPos2)
        return;

    assert(nRndPos1 < vRandom.size() && nRndPos2 < vRandom.size());

    int nId1 = vRandom[nRndPos1];
    int nId2 = vRandom[nRndPos2];

    assert(mapInfo.count(nId1) == 1);
    assert(mapInfo.count(nId2) == 1);

    mapInfo[nId1].nRandomPos = nRndPos2;
    mapInfo[nId2].nRandomPos = nRndPos1;

    vRandom[nRndPos1] = nId2;
    vRandom[nRndPos2] = nId1;
}

void CAddrMan::Delete(int nId)
{
    assert(mapInfo.count(nId) != 0);
    CAddrInfo& info = mapInfo[nId];
    assert(!info.fInTried);
    assert(info.nRefCount == 0);

    SwapRandom(info.nRandomPos, vRandom.size() - 1);
    vRandom.pop_back();
    mapAddr.erase(info);
    mapInfo.erase(nId);
    nNew--;
}

void CAddrMan::ClearNew(int nUBucket, int nUBucketPos)
{
//如果指定的存储桶中有条目，请将其删除。
    if (vvNew[nUBucket][nUBucketPos] != -1) {
        int nIdDelete = vvNew[nUBucket][nUBucketPos];
        CAddrInfo& infoDelete = mapInfo[nIdDelete];
        assert(infoDelete.nRefCount > 0);
        infoDelete.nRefCount--;
        vvNew[nUBucket][nUBucketPos] = -1;
        if (infoDelete.nRefCount == 0) {
            Delete(nIdDelete);
        }
    }
}

void CAddrMan::MakeTried(CAddrInfo& info, int nId)
{
//从所有新bucket中删除条目
    for (int bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; bucket++) {
        int pos = info.GetBucketPosition(nKey, true, bucket);
        if (vvNew[bucket][pos] == nId) {
            vvNew[bucket][pos] = -1;
            info.nRefCount--;
        }
    }
    nNew--;

    assert(info.nRefCount == 0);

//试图把入口移到
    int nKBucket = info.GetTriedBucket(nKey);
    int nKBucketPos = info.GetBucketPosition(nKey, false, nKBucket);

//首先腾出空间添加它（现有的已尝试条目将移动到“新建”，删除其中的任何内容）。
    if (vvTried[nKBucket][nKBucketPos] != -1) {
//查找要驱逐的项目
        int nIdEvict = vvTried[nKBucket][nKBucketPos];
        assert(mapInfo.count(nIdEvict) == 1);
        CAddrInfo& infoOld = mapInfo[nIdEvict];

//从尝试的集合中删除要收回的项。
        infoOld.fInTried = false;
        vvTried[nKBucket][nKBucketPos] = -1;
        nTried--;

//找到它属于哪个新桶
        int nUBucket = infoOld.GetNewBucket(nKey);
        int nUBucketPos = infoOld.GetBucketPosition(nKey, true, nUBucket);
        ClearNew(nUBucket, nUBucketPos);
        assert(vvNew[nUBucket][nUBucketPos] == -1);

//再次将其输入新集合。
        infoOld.nRefCount = 1;
        vvNew[nUBucket][nUBucketPos] = nIdEvict;
        nNew++;
    }
    assert(vvTried[nKBucket][nKBucketPos] == -1);

    vvTried[nKBucket][nKBucketPos] = nId;
    nTried++;
    info.fInTried = true;
}

void CAddrMan::Good_(const CService& addr, bool test_before_evict, int64_t nTime)
{
    int nId;

    nLastGood = nTime;

    CAddrInfo* pinfo = Find(addr, &nId);

//如果找不到，请救出
    if (!pinfo)
        return;

    CAddrInfo& info = *pinfo;

//检查我们是否在谈论完全相同的CSservice（包括相同的端口）
    if (info != addr)
        return;

//更新信息
    info.nLastSuccess = nTime;
    info.nLastTry = nTime;
    info.nAttempts = 0;
//在此不更新时间，以避免泄露有关
//当前连接的对等机。

//如果它已经在试过的集合中，不要做任何其他事情
    if (info.fInTried)
        return;

//找一个桶，它现在在里面
    int nRnd = insecure_rand.randrange(ADDRMAN_NEW_BUCKET_COUNT);
    int nUBucket = -1;
    for (unsigned int n = 0; n < ADDRMAN_NEW_BUCKET_COUNT; n++) {
        int nB = (n + nRnd) % ADDRMAN_NEW_BUCKET_COUNT;
        int nBpos = info.GetBucketPosition(nKey, true, nB);
        if (vvNew[nB][nBpos] == nId) {
            nUBucket = nB;
            break;
        }
    }

//如果没有找到桶，就发生了一些不好的事情；
//托多：也许可以重新添加节点，但现在，只需退出即可。
    if (nUBucket == -1)
        return;

//试图把入口移到
    int tried_bucket = info.GetTriedBucket(nKey);
    int tried_bucket_pos = info.GetBucketPosition(nKey, false, tried_bucket);

//将此地址移入“已尝试”中会逐出另一个条目吗？
    if (test_before_evict && (vvTried[tried_bucket][tried_bucket_pos] != -1)) {
        LogPrint(BCLog::ADDRMAN, "Collision inserting element into tried table, moving %s to m_tried_collisions=%d\n", addr.ToString(), m_tried_collisions.size());
        if (m_tried_collisions.size() < ADDRMAN_SET_TRIED_COLLISION_SIZE) {
            m_tried_collisions.insert(nId);
        }
    } else {
        LogPrint(BCLog::ADDRMAN, "Moving %s to tried\n", addr.ToString());

//将NID移到已尝试的表中
        MakeTried(info, nId);
    }
}

bool CAddrMan::Add_(const CAddress& addr, const CNetAddr& source, int64_t nTimePenalty)
{
    if (!addr.IsRoutable())
        return false;

    bool fNew = false;
    int nId;
    CAddrInfo* pinfo = Find(addr, &nId);

//不要为消息源的自我通知设置惩罚
    if (addr == source) {
        nTimePenalty = 0;
    }

    if (pinfo) {
//定期更新时间
        bool fCurrentlyOnline = (GetAdjustedTime() - addr.nTime < 24 * 60 * 60);
        int64_t nUpdateInterval = (fCurrentlyOnline ? 60 * 60 : 24 * 60 * 60);
        if (addr.nTime && (!pinfo->nTime || pinfo->nTime < addr.nTime - nUpdateInterval - nTimePenalty))
            pinfo->nTime = std::max((int64_t)0, addr.nTime - nTimePenalty);

//添加服务
        pinfo->nServices = ServiceFlags(pinfo->nServices | addr.nServices);

//如果没有新信息，则不更新
        if (!addr.nTime || (pinfo->nTime && addr.nTime <= pinfo->nTime))
            return false;

//如果条目已在“已尝试”表中，则不更新
        if (pinfo->fInTried)
            return false;

//如果达到最大引用计数，则不更新
        if (pinfo->nRefCount == ADDRMAN_NEW_BUCKETS_PER_ADDRESS)
            return false;

//随机测试：以前的nrefcount==n:2^n倍增加难度
        int nFactor = 1;
        for (int n = 0; n < pinfo->nRefCount; n++)
            nFactor *= 2;
        if (nFactor > 1 && (insecure_rand.randrange(nFactor) != 0))
            return false;
    } else {
        pinfo = Create(addr, source, &nId);
        pinfo->nTime = std::max((int64_t)0, (int64_t)pinfo->nTime - nTimePenalty);
        nNew++;
        fNew = true;
    }

    int nUBucket = pinfo->GetNewBucket(nKey, source);
    int nUBucketPos = pinfo->GetBucketPosition(nKey, true, nUBucket);
    if (vvNew[nUBucket][nUBucketPos] != nId) {
        bool fInsert = vvNew[nUBucket][nUBucketPos] == -1;
        if (!fInsert) {
            CAddrInfo& infoExisting = mapInfo[vvNew[nUBucket][nUBucketPos]];
            if (infoExisting.IsTerrible() || (infoExisting.nRefCount > 1 && pinfo->nRefCount == 0)) {
//覆盖现有的新表条目。
                fInsert = true;
            }
        }
        if (fInsert) {
            ClearNew(nUBucket, nUBucketPos);
            pinfo->nRefCount++;
            vvNew[nUBucket][nUBucketPos] = nId;
        } else {
            if (pinfo->nRefCount == 0) {
                Delete(nId);
            }
        }
    }
    return fNew;
}

void CAddrMan::Attempt_(const CService& addr, bool fCountFailure, int64_t nTime)
{
    CAddrInfo* pinfo = Find(addr);

//如果找不到，请救出
    if (!pinfo)
        return;

    CAddrInfo& info = *pinfo;

//检查我们是否在谈论完全相同的CSservice（包括相同的端口）
    if (info != addr)
        return;

//更新信息
    info.nLastTry = nTime;
    if (fCountFailure && info.nLastCountAttempt < nLastGood) {
        info.nLastCountAttempt = nTime;
        info.nAttempts++;
    }
}

CAddrInfo CAddrMan::Select_(bool newOnly)
{
    if (size() == 0)
        return CAddrInfo();

    if (newOnly && nNew == 0)
        return CAddrInfo();

//使用50%的机会在尝试过的表条目和新表条目之间进行选择。
    if (!newOnly &&
       (nTried > 0 && (nNew == 0 || insecure_rand.randbool() == 0))) {
//使用已尝试的节点
        double fChanceFactor = 1.0;
        while (1) {
            int nKBucket = insecure_rand.randrange(ADDRMAN_TRIED_BUCKET_COUNT);
            int nKBucketPos = insecure_rand.randrange(ADDRMAN_BUCKET_SIZE);
            while (vvTried[nKBucket][nKBucketPos] == -1) {
                nKBucket = (nKBucket + insecure_rand.randbits(ADDRMAN_TRIED_BUCKET_COUNT_LOG2)) % ADDRMAN_TRIED_BUCKET_COUNT;
                nKBucketPos = (nKBucketPos + insecure_rand.randbits(ADDRMAN_BUCKET_SIZE_LOG2)) % ADDRMAN_BUCKET_SIZE;
            }
            int nId = vvTried[nKBucket][nKBucketPos];
            assert(mapInfo.count(nId) == 1);
            CAddrInfo& info = mapInfo[nId];
            if (insecure_rand.randbits(30) < fChanceFactor * info.GetChance() * (1 << 30))
                return info;
            fChanceFactor *= 1.2;
        }
    } else {
//使用新节点
        double fChanceFactor = 1.0;
        while (1) {
            int nUBucket = insecure_rand.randrange(ADDRMAN_NEW_BUCKET_COUNT);
            int nUBucketPos = insecure_rand.randrange(ADDRMAN_BUCKET_SIZE);
            while (vvNew[nUBucket][nUBucketPos] == -1) {
                nUBucket = (nUBucket + insecure_rand.randbits(ADDRMAN_NEW_BUCKET_COUNT_LOG2)) % ADDRMAN_NEW_BUCKET_COUNT;
                nUBucketPos = (nUBucketPos + insecure_rand.randbits(ADDRMAN_BUCKET_SIZE_LOG2)) % ADDRMAN_BUCKET_SIZE;
            }
            int nId = vvNew[nUBucket][nUBucketPos];
            assert(mapInfo.count(nId) == 1);
            CAddrInfo& info = mapInfo[nId];
            if (insecure_rand.randbits(30) < fChanceFactor * info.GetChance() * (1 << 30))
                return info;
            fChanceFactor *= 1.2;
        }
    }
}

#ifdef DEBUG_ADDRMAN
int CAddrMan::Check_()
{
    std::set<int> setTried;
    std::map<int, int> mapNew;

    if (vRandom.size() != (size_t)(nTried + nNew))
        return -7;

    for (const auto& entry : mapInfo) {
        int n = entry.first;
        const CAddrInfo& info = entry.second;
        if (info.fInTried) {
            if (!info.nLastSuccess)
                return -1;
            if (info.nRefCount)
                return -2;
            setTried.insert(n);
        } else {
            if (info.nRefCount < 0 || info.nRefCount > ADDRMAN_NEW_BUCKETS_PER_ADDRESS)
                return -3;
            if (!info.nRefCount)
                return -4;
            mapNew[n] = info.nRefCount;
        }
        if (mapAddr[info] != n)
            return -5;
        if (info.nRandomPos < 0 || (size_t)info.nRandomPos >= vRandom.size() || vRandom[info.nRandomPos] != n)
            return -14;
        if (info.nLastTry < 0)
            return -6;
        if (info.nLastSuccess < 0)
            return -8;
    }

    if (setTried.size() != (size_t)nTried)
        return -9;
    if (mapNew.size() != (size_t)nNew)
        return -10;

    for (int n = 0; n < ADDRMAN_TRIED_BUCKET_COUNT; n++) {
        for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
             if (vvTried[n][i] != -1) {
                 if (!setTried.count(vvTried[n][i]))
                     return -11;
                 if (mapInfo[vvTried[n][i]].GetTriedBucket(nKey) != n)
                     return -17;
                 if (mapInfo[vvTried[n][i]].GetBucketPosition(nKey, false, n) != i)
                     return -18;
                 setTried.erase(vvTried[n][i]);
             }
        }
    }

    for (int n = 0; n < ADDRMAN_NEW_BUCKET_COUNT; n++) {
        for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
            if (vvNew[n][i] != -1) {
                if (!mapNew.count(vvNew[n][i]))
                    return -12;
                if (mapInfo[vvNew[n][i]].GetBucketPosition(nKey, true, n) != i)
                    return -19;
                if (--mapNew[vvNew[n][i]] == 0)
                    mapNew.erase(vvNew[n][i]);
            }
        }
    }

    if (setTried.size())
        return -13;
    if (mapNew.size())
        return -15;
    if (nKey.IsNull())
        return -16;

    return 0;
}
#endif

void CAddrMan::GetAddr_(std::vector<CAddress>& vAddr)
{
    unsigned int nNodes = ADDRMAN_GETADDR_MAX_PCT * vRandom.size() / 100;
    if (nNodes > ADDRMAN_GETADDR_MAX)
        nNodes = ADDRMAN_GETADDR_MAX;

//收集随机节点列表，跳过低质量的节点
    for (unsigned int n = 0; n < vRandom.size(); n++) {
        if (vAddr.size() >= nNodes)
            break;

        int nRndPos = insecure_rand.randrange(vRandom.size() - n) + n;
        SwapRandom(n, nRndPos);
        assert(mapInfo.count(vRandom[n]) == 1);

        const CAddrInfo& ai = mapInfo[vRandom[n]];
        if (!ai.IsTerrible())
            vAddr.push_back(ai);
    }
}

void CAddrMan::Connected_(const CService& addr, int64_t nTime)
{
    CAddrInfo* pinfo = Find(addr);

//如果找不到，请救出
    if (!pinfo)
        return;

    CAddrInfo& info = *pinfo;

//检查我们是否在谈论完全相同的CSservice（包括相同的端口）
    if (info != addr)
        return;

//更新信息
    int64_t nUpdateInterval = 20 * 60;
    if (nTime - info.nTime > nUpdateInterval)
        info.nTime = nTime;
}

void CAddrMan::SetServices_(const CService& addr, ServiceFlags nServices)
{
    CAddrInfo* pinfo = Find(addr);

//如果找不到，请救出
    if (!pinfo)
        return;

    CAddrInfo& info = *pinfo;

//检查我们是否在谈论完全相同的CSservice（包括相同的端口）
    if (info != addr)
        return;

//更新信息
    info.nServices = nServices;
}

void CAddrMan::ResolveCollisions_()
{
    for (std::set<int>::iterator it = m_tried_collisions.begin(); it != m_tried_collisions.end();) {
        int id_new = *it;

        bool erase_collision = false;

//如果在mapinfo中找不到id_new，请将其从m_尝试过的碰撞中删除。
        if (mapInfo.count(id_new) != 1) {
            erase_collision = true;
        } else {
            CAddrInfo& info_new = mapInfo[id_new];

//它试图把入口移到bucket中。
            int tried_bucket = info_new.GetTriedBucket(nKey);
            int tried_bucket_pos = info_new.GetBucketPosition(nKey, false, tried_bucket);
if (!info_new.IsValid()) { //ID_New可能不再映射到有效地址
                erase_collision = true;
} else if (vvTried[tried_bucket][tried_bucket_pos] != -1) { //尝试的存储桶中的位置不是空的

//获取正在测试的要收回的地址
                int id_old = vvTried[tried_bucket][tried_bucket_pos];
                CAddrInfo& info_old = mapInfo[id_old];

//已在过去x小时内成功连接
                if (GetAdjustedTime() - info_old.nLastSuccess < ADDRMAN_REPLACEMENT_HOURS*(60*60)) {
                    erase_collision = true;
} else if (GetAdjustedTime() - info_old.nLastTry < ADDRMAN_REPLACEMENT_HOURS*(60*60)) { //尝试连接，但在最近x小时内失败

//给地址至少60秒以成功连接
                    if (GetAdjustedTime() - info_old.nLastTry > 60) {
                        LogPrint(BCLog::ADDRMAN, "Swapping %s for %s in tried table\n", info_new.ToString(), info_old.ToString());

//用新地址替换已尝试表中的现有地址
                        Good_(info_new, false, GetAdjustedTime());
                        erase_collision = true;
                    }
                }
} else { //碰撞不再是真正的碰撞了
                Good_(info_new, false, GetAdjustedTime());
                erase_collision = true;
            }
        }

        if (erase_collision) {
            m_tried_collisions.erase(it++);
        } else {
            it++;
        }
    }
}

CAddrInfo CAddrMan::SelectTriedCollision_()
{
    if (m_tried_collisions.size() == 0) return CAddrInfo();

    std::set<int>::iterator it = m_tried_collisions.begin();

//从m_尝试过的\碰撞中选择一个随机元素
    std::advance(it, insecure_rand.randrange(m_tried_collisions.size()));
    int id_new = *it;

//如果在mapinfo中找不到id_new，请将其从m_尝试过的碰撞中删除。
    if (mapInfo.count(id_new) != 1) {
        m_tried_collisions.erase(it);
        return CAddrInfo();
    }

    CAddrInfo& newInfo = mapInfo[id_new];

//试图把入口移到
    int tried_bucket = newInfo.GetTriedBucket(nKey);
    int tried_bucket_pos = newInfo.GetBucketPosition(nKey, false, tried_bucket);

    int id_old = vvTried[tried_bucket][tried_bucket_pos];

    return mapInfo[id_old];
}
