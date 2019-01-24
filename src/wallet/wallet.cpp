
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

#include <wallet/wallet.h>

#include <checkpoints.h>
#include <chain.h>
#include <wallet/coincontrol.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <fs.h>
#include <interfaces/chain.h>
#include <key.h>
#include <key_io.h>
#include <keystore.h>
#include <validation.h>
#include <net.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <policy/rbf.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/descriptor.h>
#include <script/script.h>
#include <shutdown.h>
#include <timedata.h>
#include <txmempool.h>
#include <util/moneystr.h>
#include <wallet/fees.h>

#include <algorithm>
#include <assert.h>
#include <future>

#include <boost/algorithm/string/replace.hpp>

static const size_t OUTPUT_GROUP_MAX_ENTRIES = 10;

static CCriticalSection cs_wallets;
static std::vector<std::shared_ptr<CWallet>> vpwallets GUARDED_BY(cs_wallets);

bool AddWallet(const std::shared_ptr<CWallet>& wallet)
{
    LOCK(cs_wallets);
    assert(wallet);
    std::vector<std::shared_ptr<CWallet>>::const_iterator i = std::find(vpwallets.begin(), vpwallets.end(), wallet);
    if (i != vpwallets.end()) return false;
    vpwallets.push_back(wallet);
    return true;
}

bool RemoveWallet(const std::shared_ptr<CWallet>& wallet)
{
    LOCK(cs_wallets);
    assert(wallet);
    std::vector<std::shared_ptr<CWallet>>::iterator i = std::find(vpwallets.begin(), vpwallets.end(), wallet);
    if (i == vpwallets.end()) return false;
    vpwallets.erase(i);
    return true;
}

bool HasWallets()
{
    LOCK(cs_wallets);
    return !vpwallets.empty();
}

std::vector<std::shared_ptr<CWallet>> GetWallets()
{
    LOCK(cs_wallets);
    return vpwallets;
}

std::shared_ptr<CWallet> GetWallet(const std::string& name)
{
    LOCK(cs_wallets);
    for (const std::shared_ptr<CWallet>& wallet : vpwallets) {
        if (wallet->GetName() == name) return wallet;
    }
    return nullptr;
}

static Mutex g_wallet_release_mutex;
static std::condition_variable g_wallet_release_cv;
static std::set<CWallet*> g_unloading_wallet_set;

//共享资源的自定义删除程序。
static void ReleaseWallet(CWallet* wallet)
{
//注销并删除钱包后立即阻止，直到同步当前链接
//使其与当前链状态同步。
    wallet->WalletLogPrintf("Releasing wallet\n");
    wallet->BlockUntilSyncedToCurrentChain();
    wallet->Flush();
    UnregisterValidationInterface(wallet);
    delete wallet;
//钱包现在释放，如果有，通知UnloadWallet。
    {
        LOCK(g_wallet_release_mutex);
        if (g_unloading_wallet_set.erase(wallet) == 0) {
//这个钱包没有叫UnloadWallet，一切都结束了。
            return;
        }
    }
    g_wallet_release_cv.notify_all();
}

void UnloadWallet(std::shared_ptr<CWallet>&& wallet)
{
//标记钱包以便卸货。
    CWallet* pwallet = wallet.get();
    {
        LOCK(g_wallet_release_mutex);
        auto it = g_unloading_wallet_set.insert(pwallet);
        assert(it.second);
    }
//钱包可以使用，因此不可能在这里明确卸载。
//通知卸载意图，以便所有剩余的共享指针
//释放。
    pwallet->NotifyUnload();
//是时候放弃我们的共享资源，等待释放钱包的电话了。
    wallet.reset();
    {
        WAIT_LOCK(g_wallet_release_mutex, lock);
        while (g_unloading_wallet_set.count(pwallet) == 1) {
            g_wallet_release_cv.wait(lock);
        }
    }
}

const uint32_t BIP32_HARDENED_KEY_LIMIT = 0x80000000;

const uint256 CMerkleTx::ABANDON_HASH(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));

/*@defgroup地图钱包
 *
 *@
 **/


std::string COutput::ToString() const
{
    return strprintf("COutput(%s, %d, %d) [%s]", tx->GetHash().ToString(), i, nDepth, FormatMoney(tx->tx->vout[i].nValue));
}

std::vector<CKeyID> GetAffectedKeys(const CScript& spk, const SigningProvider& provider)
{
    std::vector<CScript> dummy;
    FlatSigningProvider out;
    InferDescriptor(spk, provider)->Expand(0, DUMMY_SIGNING_PROVIDER, dummy, out);
    std::vector<CKeyID> ret;
    for (const auto& entry : out.pubkeys) {
        ret.push_back(entry.first);
    }
    return ret;
}

const CWalletTx* CWallet::GetWalletTx(const uint256& hash) const
{
    LOCK(cs_wallet);
    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(hash);
    if (it == mapWallet.end())
        return nullptr;
    return &(it->second);
}

CPubKey CWallet::GenerateNewKey(WalletBatch &batch, bool internal)
{
    assert(!IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS));
AssertLockHeld(cs_wallet); //MAPKEY元数据
bool fCompressed = CanSupportFeature(FEATURE_COMPRPUBKEY); //如果需要0.6.0钱包，默认为压缩公钥

    CKey secret;

//创建新元数据
    int64_t nCreationTime = GetTime();
    CKeyMetadata metadata(nCreationTime);

//如果在钱包创建期间启用了HD，则使用HD密钥派生
    if (IsHDEnabled()) {
        DeriveNewChildKey(batch, metadata, secret, (CanSupportFeature(FEATURE_HD_SPLIT) ? internal : false));
    } else {
        secret.MakeNewKey(fCompressed);
    }

//0.6.0版中引入了压缩公钥
    if (fCompressed) {
        SetMinVersion(FEATURE_COMPRPUBKEY);
    }

    CPubKey pubkey = secret.GetPubKey();
    assert(secret.VerifyPubKey(pubkey));

    mapKeyMetadata[pubkey.GetID()] = metadata;
    UpdateTimeFirstKey(nCreationTime);

    if (!AddKeyPubKeyWithDB(batch, secret, pubkey)) {
        throw std::runtime_error(std::string(__func__) + ": AddKey failed");
    }
    return pubkey;
}

void CWallet::DeriveNewChildKey(WalletBatch &batch, CKeyMetadata& metadata, CKey& secret, bool internal)
{
//目前我们使用的是m/0'/0'/k的固定密钥路径方案
CKey seed;                     //种子（256位）
CExtKey masterKey;             //硬盘主密钥
CExtKey accountKey;            //M／0键
CExtKey chainChildKey;         //键位于m/0'/0'（外部）或m/0'/1'（内部）
CExtKey childKey;              //M/0'/0'/n>处的键

//试着找到种子
    if (!GetKey(hdChain.seed_id, seed))
        throw std::runtime_error(std::string(__func__) + ": seed not found");

    masterKey.SetSeed(seed.begin(), seed.size());

//导出M／0’
//使用强化派生（bip32之后强化大于等于0x8000000的子键）
    masterKey.Derive(accountKey, BIP32_HARDENED_KEY_LIMIT);

//导出m/0'/0'（外部链）或m/0'/1'（内部链）
    assert(internal ? CanSupportFeature(FEATURE_HD_SPLIT) : true);
    accountKey.Derive(chainChildKey, BIP32_HARDENED_KEY_LIMIT+(internal ? 1 : 0));

//在下一个索引处派生子密钥，跳过钱包已知的密钥
    do {
//始终派生硬化键
//childIndex bip32_hardend_key_limit=派生硬化子索引范围内的childIndex
//示例：1_BIP32U硬化U键U限制==0x800000001==2147483649
        if (internal) {
            chainChildKey.Derive(childKey, hdChain.nInternalChainCounter | BIP32_HARDENED_KEY_LIMIT);
            metadata.hdKeypath = "m/0'/1'/" + std::to_string(hdChain.nInternalChainCounter) + "'";
            hdChain.nInternalChainCounter++;
        }
        else {
            chainChildKey.Derive(childKey, hdChain.nExternalChainCounter | BIP32_HARDENED_KEY_LIMIT);
            metadata.hdKeypath = "m/0'/0'/" + std::to_string(hdChain.nExternalChainCounter) + "'";
            hdChain.nExternalChainCounter++;
        }
    } while (HaveKey(childKey.key.GetPubKey().GetID()));
    secret = childKey.key;
    metadata.hd_seed_id = hdChain.seed_id;
//更新数据库中的链模型
    if (!batch.WriteHDChain(hdChain))
        throw std::runtime_error(std::string(__func__) + ": Writing HD chain model failed");
}

bool CWallet::AddKeyPubKeyWithDB(WalletBatch &batch, const CKey& secret, const CPubKey &pubkey)
{
AssertLockHeld(cs_wallet); //MAPKEY元数据

//ccryptokeystore没有钱包数据库的概念，但调用addCryptedKey
//在下面被覆盖。为了避免刷新，数据库句柄是
//通过隧道到达那里。
    bool needsDB = !encrypted_batch;
    if (needsDB) {
        encrypted_batch = &batch;
    }
    if (!CCryptoKeyStore::AddKeyPubKey(secret, pubkey)) {
        if (needsDB) encrypted_batch = nullptr;
        return false;
    }
    if (needsDB) encrypted_batch = nullptr;

//检查我们是否只需要从手表上取下
    CScript script;
    script = GetScriptForDestination(pubkey.GetID());
    if (HaveWatchOnly(script)) {
        RemoveWatchOnly(script);
    }
    script = GetScriptForRawPubKey(pubkey);
    if (HaveWatchOnly(script)) {
        RemoveWatchOnly(script);
    }

    if (!IsCrypted()) {
        return batch.WriteKey(pubkey,
                                                 secret.GetPrivKey(),
                                                 mapKeyMetadata[pubkey.GetID()]);
    }
    return true;
}

bool CWallet::AddKeyPubKey(const CKey& secret, const CPubKey &pubkey)
{
    WalletBatch batch(*database);
    return CWallet::AddKeyPubKeyWithDB(batch, secret, pubkey);
}

bool CWallet::AddCryptedKey(const CPubKey &vchPubKey,
                            const std::vector<unsigned char> &vchCryptedSecret)
{
    if (!CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret))
        return false;
    {
        LOCK(cs_wallet);
        if (encrypted_batch)
            return encrypted_batch->WriteCryptedKey(vchPubKey,
                                                        vchCryptedSecret,
                                                        mapKeyMetadata[vchPubKey.GetID()]);
        else
            return WalletBatch(*database).WriteCryptedKey(vchPubKey,
                                                            vchCryptedSecret,
                                                            mapKeyMetadata[vchPubKey.GetID()]);
    }
}

void CWallet::LoadKeyMetadata(const CKeyID& keyID, const CKeyMetadata &meta)
{
AssertLockHeld(cs_wallet); //MAPKEY元数据
    UpdateTimeFirstKey(meta.nCreateTime);
    mapKeyMetadata[keyID] = meta;
}

void CWallet::LoadScriptMetadata(const CScriptID& script_id, const CKeyMetadata &meta)
{
AssertLockHeld(cs_wallet); //脚本元数据
    UpdateTimeFirstKey(meta.nCreateTime);
    m_script_metadata[script_id] = meta;
}

bool CWallet::LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    return CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret);
}

/*
 *更新钱包第一个密钥创建时间。每当键
 *添加到钱包中，使用最早的密钥创建时间。
 **/

void CWallet::UpdateTimeFirstKey(int64_t nCreateTime)
{
    AssertLockHeld(cs_wallet);
    if (nCreateTime <= 1) {
//无法确定生日信息，因此将钱包生日设置为
//时间的开始。
        nTimeFirstKey = 1;
    } else if (!nTimeFirstKey || nCreateTime < nTimeFirstKey) {
        nTimeFirstKey = nCreateTime;
    }
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    if (!CCryptoKeyStore::AddCScript(redeemScript))
        return false;
    return WalletBatch(*database).WriteCScript(Hash160(redeemScript), redeemScript);
}

bool CWallet::LoadCScript(const CScript& redeemScript)
{
    /*在pull 3843中添加了健全性检查，以避免添加redeemscripts
     *永远无法赎回。然而，旧钱包可能仍然包含
     *这些。不要将它们添加到钱包中并发出警告。*/

    if (redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE)
    {
        std::string strAddr = EncodeDestination(CScriptID(redeemScript));
        WalletLogPrintf("%s: Warning: This wallet contains a redeemScript of size %i which exceeds maximum size %i thus can never be redeemed. Do not use address %s.\n", __func__, redeemScript.size(), MAX_SCRIPT_ELEMENT_SIZE, strAddr);
        return true;
    }

    return CCryptoKeyStore::AddCScript(redeemScript);
}

bool CWallet::AddWatchOnly(const CScript& dest)
{
    if (!CCryptoKeyStore::AddWatchOnly(dest))
        return false;
    const CKeyMetadata& meta = m_script_metadata[CScriptID(dest)];
    UpdateTimeFirstKey(meta.nCreateTime);
    NotifyWatchonlyChanged(true);
    return WalletBatch(*database).WriteWatchOnly(dest, meta);
}

bool CWallet::AddWatchOnly(const CScript& dest, int64_t nCreateTime)
{
    m_script_metadata[CScriptID(dest)].nCreateTime = nCreateTime;
    return AddWatchOnly(dest);
}

bool CWallet::RemoveWatchOnly(const CScript &dest)
{
    AssertLockHeld(cs_wallet);
    if (!CCryptoKeyStore::RemoveWatchOnly(dest))
        return false;
    if (!HaveWatchOnly())
        NotifyWatchonlyChanged(false);
    if (!WalletBatch(*database).EraseWatchOnly(dest))
        return false;

    return true;
}

bool CWallet::LoadWatchOnly(const CScript &dest)
{
    return CCryptoKeyStore::AddWatchOnly(dest);
}

bool CWallet::Unlock(const SecureString& strWalletPassphrase)
{
    CCrypter crypter;
    CKeyingMaterial _vMasterKey;

    {
        LOCK(cs_wallet);
        for (const MasterKeyMap::value_type& pMasterKey : mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, _vMasterKey))
continue; //尝试另一个主密钥
            if (CCryptoKeyStore::Unlock(_vMasterKey))
                return true;
        }
    }
    return false;
}

bool CWallet::ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase)
{
    bool fWasLocked = IsLocked();

    {
        LOCK(cs_wallet);
        Lock();

        CCrypter crypter;
        CKeyingMaterial _vMasterKey;
        for (MasterKeyMap::value_type& pMasterKey : mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strOldWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, _vMasterKey))
                return false;
            if (CCryptoKeyStore::Unlock(_vMasterKey))
            {
                int64_t nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = static_cast<unsigned int>(pMasterKey.second.nDeriveIterations * (100 / ((double)(GetTimeMillis() - nStartTime))));

                nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = (pMasterKey.second.nDeriveIterations + static_cast<unsigned int>(pMasterKey.second.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime)))) / 2;

                if (pMasterKey.second.nDeriveIterations < 25000)
                    pMasterKey.second.nDeriveIterations = 25000;

                WalletLogPrintf("Wallet passphrase changed to an nDeriveIterations of %i\n", pMasterKey.second.nDeriveIterations);

                if (!crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                    return false;
                if (!crypter.Encrypt(_vMasterKey, pMasterKey.second.vchCryptedKey))
                    return false;
                WalletBatch(*database).WriteMasterKey(pMasterKey.first, pMasterKey.second);
                if (fWasLocked)
                    Lock();
                return true;
            }
        }
    }

    return false;
}

void CWallet::ChainStateFlushed(const CBlockLocator& loc)
{
    WalletBatch batch(*database);
    batch.WriteBestBlock(loc);
}

void CWallet::SetMinVersion(enum WalletFeature nVersion, WalletBatch* batch_in, bool fExplicit)
{
LOCK(cs_wallet); //壁纸版
    if (nWalletVersion >= nVersion)
        return;

//在执行显式升级时，如果我们通过了允许的最大版本，则会一直升级。
    if (fExplicit && nVersion > nWalletMaxVersion)
            nVersion = FEATURE_LATEST;

    nWalletVersion = nVersion;

    if (nVersion > nWalletMaxVersion)
        nWalletMaxVersion = nVersion;

    {
        WalletBatch* batch = batch_in ? batch_in : new WalletBatch(*database);
        if (nWalletVersion > 40000)
            batch->WriteMinVersion(nWalletVersion);
        if (!batch_in)
            delete batch;
    }
}

bool CWallet::SetMaxVersion(int nVersion)
{
LOCK(cs_wallet); //nwalletversion，nwalletmax版本
//不能降级到当前版本以下
    if (nWalletVersion > nVersion)
        return false;

    nWalletMaxVersion = nVersion;

    return true;
}

std::set<uint256> CWallet::GetConflicts(const uint256& txid) const
{
    std::set<uint256> result;
    AssertLockHeld(cs_wallet);

    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(txid);
    if (it == mapWallet.end())
        return result;
    const CWalletTx& wtx = it->second;

    std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;

    for (const CTxIn& txin : wtx.tx->vin)
    {
        if (mapTxSpends.count(txin.prevout) <= 1)
continue;  //零花费或一花费不会产生冲突
        range = mapTxSpends.equal_range(txin.prevout);
        for (TxSpends::const_iterator _it = range.first; _it != range.second; ++_it)
            result.insert(_it->second);
    }
    return result;
}

bool CWallet::HasWalletSpend(const uint256& txid) const
{
    AssertLockHeld(cs_wallet);
    auto iter = mapTxSpends.lower_bound(COutPoint(txid, 0));
    return (iter != mapTxSpends.end() && iter->first.hash == txid);
}

void CWallet::Flush(bool shutdown)
{
    database->Flush(shutdown);
}

void CWallet::SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator> range)
{
//我们希望范围内的所有钱包交易都具有与
//最古老的（最小的北欧人）。
//所以：找到最小的诺德波斯：

    int nMinOrderPos = std::numeric_limits<int>::max();
    const CWalletTx* copyFrom = nullptr;
    for (TxSpends::iterator it = range.first; it != range.second; ++it) {
        const CWalletTx* wtx = &mapWallet.at(it->second);
        if (wtx->nOrderPos < nMinOrderPos) {
            nMinOrderPos = wtx->nOrderPos;
            copyFrom = wtx;
        }
    }

    if (!copyFrom) {
        return;
    }

//现在将数据从copyFrom复制到rest：
    for (TxSpends::iterator it = range.first; it != range.second; ++it)
    {
        const uint256& hash = it->second;
        CWalletTx* copyTo = &mapWallet.at(hash);
        if (copyFrom == copyTo) continue;
        assert(copyFrom && "Oldest wallet transaction in range assumed to have been found.");
        if (!copyFrom->IsEquivalentTo(*copyTo)) continue;
        copyTo->mapValue = copyFrom->mapValue;
        copyTo->vOrderForm = copyFrom->vOrderForm;
//未故意复制ftimeReceiveDistXTime
//未故意复制
        copyTo->nTimeSmart = copyFrom->nTimeSmart;
        copyTo->fFromMe = copyFrom->fFromMe;
//Norderpos未故意复制
//未故意复制缓存成员
    }
}

/*
 *如果有任何不冲突的事务，则使用Outpoint
 *花费：
 **/

bool CWallet::IsSpent(interfaces::Chain::Lock& locked_chain, const uint256& hash, unsigned int n) const
{
    const COutPoint outpoint(hash, n);
    std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;
    range = mapTxSpends.equal_range(outpoint);

    for (TxSpends::const_iterator it = range.first; it != range.second; ++it)
    {
        const uint256& wtxid = it->second;
        std::map<uint256, CWalletTx>::const_iterator mit = mapWallet.find(wtxid);
        if (mit != mapWallet.end()) {
            int depth = mit->second.GetDepthInMainChain(locked_chain);
            if (depth > 0  || (depth == 0 && !mit->second.isAbandoned()))
return true; //花
        }
    }
    return false;
}

void CWallet::AddToSpends(const COutPoint& outpoint, const uint256& wtxid)
{
    mapTxSpends.insert(std::make_pair(outpoint, wtxid));

    setLockedCoins.erase(outpoint);

    std::pair<TxSpends::iterator, TxSpends::iterator> range;
    range = mapTxSpends.equal_range(outpoint);
    SyncMetaData(range);
}


void CWallet::AddToSpends(const uint256& wtxid)
{
    auto it = mapWallet.find(wtxid);
    assert(it != mapWallet.end());
    CWalletTx& thisTx = it->second;
if (thisTx.IsCoinBase()) //硬币库什么都不花！
        return;

    for (const CTxIn& txin : thisTx.tx->vin)
        AddToSpends(txin.prevout, wtxid);
}

bool CWallet::EncryptWallet(const SecureString& strWalletPassphrase)
{
    if (IsCrypted())
        return false;

    CKeyingMaterial _vMasterKey;

    _vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
    GetStrongRandBytes(&_vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

    CMasterKey kMasterKey;

    kMasterKey.vchSalt.resize(WALLET_CRYPTO_SALT_SIZE);
    GetStrongRandBytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);

    CCrypter crypter;
    int64_t nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, 25000, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = static_cast<unsigned int>(2500000 / ((double)(GetTimeMillis() - nStartTime)));

    nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = (kMasterKey.nDeriveIterations + static_cast<unsigned int>(kMasterKey.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime)))) / 2;

    if (kMasterKey.nDeriveIterations < 25000)
        kMasterKey.nDeriveIterations = 25000;

    WalletLogPrintf("Encrypting Wallet with an nDeriveIterations of %i\n", kMasterKey.nDeriveIterations);

    if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod))
        return false;
    if (!crypter.Encrypt(_vMasterKey, kMasterKey.vchCryptedKey))
        return false;

    {
        LOCK(cs_wallet);
        mapMasterKeys[++nMasterKeyMaxID] = kMasterKey;
        assert(!encrypted_batch);
        encrypted_batch = new WalletBatch(*database);
        if (!encrypted_batch->TxnBegin()) {
            delete encrypted_batch;
            encrypted_batch = nullptr;
            return false;
        }
        encrypted_batch->WriteMasterKey(nMasterKeyMaxID, kMasterKey);

        if (!EncryptKeys(_vMasterKey))
        {
            encrypted_batch->TxnAbort();
            delete encrypted_batch;
            encrypted_batch = nullptr;
//我们现在可能有一半的密钥在内存中加密，而另一半不是…
//死后让用户重新加载未加密的钱包。
            assert(false);
        }

//0.4.0版中引入了加密
        SetMinVersion(FEATURE_WALLETCRYPT, encrypted_batch, true);

        if (!encrypted_batch->TxnCommit()) {
            delete encrypted_batch;
            encrypted_batch = nullptr;
//我们现在在内存中加密了密钥，但不在磁盘上…
//死亡以避免混淆，并让用户重新加载未加密的钱包。
            assert(false);
        }

        delete encrypted_batch;
        encrypted_batch = nullptr;

        Lock();
        Unlock(strWalletPassphrase);

//如果我们使用HD，请用新的HD种子替换它
        if (IsHDEnabled()) {
            SetHDSeed(GenerateNewSeed());
        }

        NewKeyPool();
        Lock();

//需要完全重写钱包文件；如果没有，BDB可能会保留
//数据库文件中可宽延空间中未加密的私钥的位。
        database->Rewrite();

//BDB似乎有将旧数据写入的坏习惯
//.dat文件中的可宽延空间；如果旧数据
//未加密的私钥。所以：
        database->ReloadDbEnv();

    }
    NotifyStatusChanged(this);

    return true;
}

DBErrors CWallet::ReorderTransactions()
{
    LOCK(cs_wallet);
    WalletBatch batch(*database);

//旧钱包没有明确的交易顺序
//可能是个坏主意

//首先：将所有cwallettx放入按时间排序的多映射中。
    typedef std::multimap<int64_t, CWalletTx*> TxItems;
    TxItems txByTime;

    for (auto& entry : mapWallet)
    {
        CWalletTx* wtx = &entry.second;
        txByTime.insert(std::make_pair(wtx->nTimeReceived, wtx));
    }

    nOrderPosNext = 0;
    std::vector<int64_t> nOrderPosOffsets;
    for (TxItems::iterator it = txByTime.begin(); it != txByTime.end(); ++it)
    {
        CWalletTx *const pwtx = (*it).second;
        int64_t& nOrderPos = pwtx->nOrderPos;

        if (nOrderPos == -1)
        {
            nOrderPos = nOrderPosNext++;
            nOrderPosOffsets.push_back(nOrderPos);

            if (!batch.WriteTx(*pwtx))
                return DBErrors::LOAD_FAIL;
        }
        else
        {
            int64_t nOrderPosOff = 0;
            for (const int64_t& nOffsetStart : nOrderPosOffsets)
            {
                if (nOrderPos >= nOffsetStart)
                    ++nOrderPosOff;
            }
            nOrderPos += nOrderPosOff;
            nOrderPosNext = std::max(nOrderPosNext, nOrderPos + 1);

            if (!nOrderPosOff)
                continue;

//既然我们要更改订单，就把它写回去。
            if (!batch.WriteTx(*pwtx))
                return DBErrors::LOAD_FAIL;
        }
    }
    batch.WriteOrderPosNext(nOrderPosNext);

    return DBErrors::LOAD_OK;
}

int64_t CWallet::IncOrderPosNext(WalletBatch *batch)
{
AssertLockHeld(cs_wallet); //诺德波斯波斯特
    int64_t nRet = nOrderPosNext++;
    if (batch) {
        batch->WriteOrderPosNext(nOrderPosNext);
    } else {
        WalletBatch(*database).WriteOrderPosNext(nOrderPosNext);
    }
    return nRet;
}

void CWallet::MarkDirty()
{
    {
        LOCK(cs_wallet);
        for (std::pair<const uint256, CWalletTx>& item : mapWallet)
            item.second.MarkDirty();
    }
}

bool CWallet::MarkReplaced(const uint256& originalHash, const uint256& newHash)
{
    LOCK(cs_wallet);

    auto mi = mapWallet.find(originalHash);

//如果在现有钱包交易中未调用markreplaced，则会出现错误。
    assert(mi != mapWallet.end());

    CWalletTx& wtx = (*mi).second;

//现在确保我们不会覆盖数据
    assert(wtx.mapValue.count("replaced_by_txid") == 0);

    wtx.mapValue["replaced_by_txid"] = newHash.ToString();

    WalletBatch batch(*database, "r+");

    bool success = true;
    if (!batch.WriteTx(wtx)) {
        WalletLogPrintf("%s: Updating batch tx %s failed\n", __func__, wtx.GetHash().ToString());
        success = false;
    }

    NotifyTransactionChanged(this, originalHash, CT_UPDATED);

    return success;
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn, bool fFlushOnClose)
{
    LOCK(cs_wallet);

    WalletBatch batch(*database, "r+", fFlushOnClose);

    uint256 hash = wtxIn.GetHash();

//仅当不存在时插入，返回插入的Tx或找到的Tx
    std::pair<std::map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(std::make_pair(hash, wtxIn));
    CWalletTx& wtx = (*ret.first).second;
    wtx.BindWallet(this);
    bool fInsertedNew = ret.second;
    if (fInsertedNew) {
        wtx.nTimeReceived = GetAdjustedTime();
        wtx.nOrderPos = IncOrderPosNext(&batch);
        wtx.m_it_wtxOrdered = wtxOrdered.insert(std::make_pair(wtx.nOrderPos, &wtx));
        wtx.nTimeSmart = ComputeTimeSmart(wtx);
        AddToSpends(hash);
    }

    bool fUpdated = false;
    if (!fInsertedNew)
    {
//合并
        if (!wtxIn.hashUnset() && wtxIn.hashBlock != wtx.hashBlock)
        {
            wtx.hashBlock = wtxIn.hashBlock;
            fUpdated = true;
        }
//如果不再放弃，请更新
        if (wtxIn.hashBlock.IsNull() && wtx.isAbandoned())
        {
            wtx.hashBlock = wtxIn.hashBlock;
            fUpdated = true;
        }
        if (wtxIn.nIndex != -1 && (wtxIn.nIndex != wtx.nIndex))
        {
            wtx.nIndex = wtxIn.nIndex;
            fUpdated = true;
        }
        if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe)
        {
            wtx.fFromMe = wtxIn.fFromMe;
            fUpdated = true;
        }
//如果我们有一个证人剥离版本的交易，我们
//看到一个证人的新版本，那么我们必须升级一个pre-segwit
//钱包。将事务的新版本存储到见证服务器，
//因为剥离版本必须无效。
//TODO:存储事务的所有版本，而不是只存储一个。
        if (wtxIn.tx->HasWitness() && !wtx.tx->HasWitness()) {
            wtx.SetTx(wtxIn.tx);
            fUpdated = true;
        }
    }

////调试打印
    WalletLogPrintf("AddToWallet %s  %s%s\n", wtxIn.GetHash().ToString(), (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));

//写入磁盘
    if (fInsertedNew || fUpdated)
        if (!batch.WriteTx(wtx))
            return false;

//中断借方/贷方余额缓存：
    wtx.MarkDirty();

//通知用户界面新的或更新的事务
    NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED);

//当钱包交易进入或更新时通知外部脚本
    std::string strCmd = gArgs.GetArg("-walletnotify", "");

    if (!strCmd.empty())
    {
        boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
        std::thread t(runCommand, strCmd);
t.detach(); //线程自由运行
    }

    return true;
}

void CWallet::LoadToWallet(const CWalletTx& wtxIn)
{
    uint256 hash = wtxIn.GetHash();
    const auto& ins = mapWallet.emplace(hash, wtxIn);
    CWalletTx& wtx = ins.first->second;
    wtx.BindWallet(this);
    /*（/*插入发生了*/ins.second）
        wtx.m_it_wtxordered=wtxordered.insert（std:：make_pair（wtx.norderpos，&wtx））；
    }
    addToPends（哈希）；
    对于（const ctxin&txin:wtx.tx->vin）
        auto it=mapwallet.find（txin.prevout.hash）；
        如果（它）！=mapwallet.end（））
            cwallettx&prevtx=it->second；
            如果（prevtx.nindex=-1&！prevtx.hashuset（））
                markconflicted（prevtx.hashblock，wtx.gethash（））；
            }
        }
    }
}

bool cwallet:：addtowalletifinvolvingme（const ctransactionref&ptx，const cblockindex*pindex，int posinblock，bool fupdate）
{
    const ctransaction&tx=*ptx；
    {
        断言锁定（CS-U钱包）；

        如果（pHealth.）= null pTr）{
            用于（const ctxin&txin:tx.vin）
                std:：pair<txtExpends:：const_迭代器，txtExpends:：const_迭代器>range=maptxtExpends.equal_range（txin.prevout）；
                同时（range.first！=范围.秒）
                    if（range.first->second！=tx.gethash（））
                        walletlogprintf（“事务%s（在块%s中）与wallet事务%s发生冲突（两者都花费了%s:%i）\n”，tx.gethash（）.toString（），pindex->getblockhash（），range.first->second.toString（），range.first->first.hash.toString（），range.first->first.n）；
                        markconflicted（pindex->getBlockHash（），range.first->second）；
                    }
                    范围：第一++；
                }
            }
        }

        bool fexisted=mapwallet.count（tx.gethash（））！＝0；
        如果（不存在&！fupdate）返回false；
        如果（fexisted ismine（tx）isfromme（tx））
        {
            /*检查钱包钥匙池中是否有本应未使用的钥匙
             *出现在新交易中。如果是，请从密钥池中删除这些密钥。
             *当还原不包含
             *最新创建的交易来自较新版本的钱包。
             **/


//循环所有输出
            for (const CTxOut& txout: tx.vout) {
//提取地址并检查它们是否与未使用的密钥池密钥匹配
                for (const auto& keyid : GetAffectedKeys(txout.scriptPubKey, *this)) {
                    std::map<CKeyID, int64_t>::const_iterator mi = m_pool_key_to_index.find(keyid);
                    if (mi != m_pool_key_to_index.end()) {
                        WalletLogPrintf("%s: Detected a used keypool key, mark all keypool key up to this key as used\n", __func__);
                        MarkReserveKeysAsUsed(mi->second);

                        if (!TopUpKeyPool()) {
                            WalletLogPrintf("%s: Topping up keypool failed (locked wallet)\n", __func__);
                        }
                    }
                }
            }

            CWalletTx wtx(this, ptx);

//如果在块中找到事务，则获取Merkle分支
            if (pIndex != nullptr)
                wtx.SetMerkleBranch(pIndex, posInBlock);

            return AddToWallet(wtx, false);
        }
    }
    return false;
}

bool CWallet::TransactionCanBeAbandoned(const uint256& hashTx) const
{
    auto locked_chain = chain().lock();
    LOCK(cs_wallet);
    const CWalletTx* wtx = GetWalletTx(hashTx);
    return wtx && !wtx->isAbandoned() && wtx->GetDepthInMainChain(*locked_chain) == 0 && !wtx->InMempool();
}

void CWallet::MarkInputsDirty(const CTransactionRef& tx)
{
    for (const CTxIn& txin : tx->vin) {
        auto it = mapWallet.find(txin.prevout.hash);
        if (it != mapWallet.end()) {
            it->second.MarkDirty();
        }
    }
}

bool CWallet::AbandonTransaction(interfaces::Chain::Lock& locked_chain, const uint256& hashTx)
{
auto locked_chain_recursive = chain().lock();  //暂时的。在即将进行的锁清理中删除
    LOCK(cs_wallet);

    WalletBatch batch(*database, "r+");

    std::set<uint256> todo;
    std::set<uint256> done;

//如果确认或在mempool中不能标记为已放弃
    auto it = mapWallet.find(hashTx);
    assert(it != mapWallet.end());
    CWalletTx& origtx = it->second;
    if (origtx.GetDepthInMainChain(locked_chain) != 0 || origtx.InMempool()) {
        return false;
    }

    todo.insert(hashTx);

    while (!todo.empty()) {
        uint256 now = *todo.begin();
        todo.erase(now);
        done.insert(now);
        auto it = mapWallet.find(now);
        assert(it != mapWallet.end());
        CWalletTx& wtx = it->second;
        int currentconfirm = wtx.GetDepthInMainChain(locked_chain);
//如果orig-tx没有被阻止，它的任何花费都不能
        assert(currentconfirm <= 0);
//如果（currentconfirm<0）tx和花销已经发生冲突，则无需放弃
        if (currentconfirm == 0 && !wtx.isAbandoned()) {
//如果orig tx不在block/mempool中，则其任何支出都不能在mempool中
            assert(!wtx.InMempool());
            wtx.nIndex = -1;
            wtx.setAbandoned();
            wtx.MarkDirty();
            batch.WriteTx(wtx);
            NotifyTransactionChanged(this, wtx.GetHash(), CT_UPDATED);
//迭代其所有输出，并在钱包中标记花费它们的交易也被放弃。
            TxSpends::const_iterator iter = mapTxSpends.lower_bound(COutPoint(now, 0));
            while (iter != mapTxSpends.end() && iter->first.hash == now) {
                if (!done.count(iter->second)) {
                    todo.insert(iter->second);
                }
                iter++;
            }
//如果交易更改“冲突”状态，则会更改余额。
//可获得的产出。所以强制重新计算
            MarkInputsDirty(wtx.tx);
        }
    }

    return true;
}

void CWallet::MarkConflicted(const uint256& hashBlock, const uint256& hashTx)
{
    auto locked_chain = chain().lock();
    LOCK(cs_wallet);

    int conflictconfirms = 0;
    CBlockIndex* pindex = LookupBlockIndex(hashBlock);
    if (pindex && chainActive.Contains(pindex)) {
        conflictconfirms = -(chainActive.Height() - pindex->nHeight + 1);
    }
//如果无法确定冲突确认数，这意味着
//区块仍然未知或尚未成为主链的一部分，
//例如，在重新索引期间加载钱包时。什么都不做
//案例。
    if (conflictconfirms >= 0)
        return;

//出于性能原因，请勿在此处冲洗钱包。
    WalletBatch batch(*database, "r+", false);

    std::set<uint256> todo;
    std::set<uint256> done;

    todo.insert(hashTx);

    while (!todo.empty()) {
        uint256 now = *todo.begin();
        todo.erase(now);
        done.insert(now);
        auto it = mapWallet.find(now);
        assert(it != mapWallet.end());
        CWalletTx& wtx = it->second;
        int currentconfirm = wtx.GetDepthInMainChain(*locked_chain);
        if (conflictconfirms < currentconfirm) {
//块比当前确认“更冲突”；更新。
//将事务标记为与此块冲突。
            wtx.nIndex = -1;
            wtx.hashBlock = hashBlock;
            wtx.MarkDirty();
            batch.WriteTx(wtx);
//迭代其所有输出，并在钱包中标记花费它们的交易也会产生冲突。
            TxSpends::const_iterator iter = mapTxSpends.lower_bound(COutPoint(now, 0));
            while (iter != mapTxSpends.end() && iter->first.hash == now) {
                 if (!done.count(iter->second)) {
                     todo.insert(iter->second);
                 }
                 iter++;
            }
//如果交易更改“冲突”状态，则会更改余额。
//可获得的产出。所以强制重新计算
            MarkInputsDirty(wtx.tx);
        }
    }
}

void CWallet::SyncTransaction(const CTransactionRef& ptx, const CBlockIndex *pindex, int posInBlock, bool update_tx) {
    if (!AddToWalletIfInvolvingMe(ptx, pindex, posInBlock, update_tx))
return; //不是我们其中一个

//如果交易更改“冲突”状态，则会更改余额。
//可获得的产出。所以强迫他们
//重新计算，同样：
    MarkInputsDirty(ptx);
}

void CWallet::TransactionAddedToMempool(const CTransactionRef& ptx) {
    auto locked_chain = chain().lock();
    LOCK(cs_wallet);
    SyncTransaction(ptx);

    auto it = mapWallet.find(ptx->GetHash());
    if (it != mapWallet.end()) {
        it->second.fInMempool = true;
    }
}

void CWallet::TransactionRemovedFromMempool(const CTransactionRef &ptx) {
    LOCK(cs_wallet);
    auto it = mapWallet.find(ptx->GetHash());
    if (it != mapWallet.end()) {
        it->second.fInMempool = false;
    }
}

void CWallet::BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex *pindex, const std::vector<CTransactionRef>& vtxConflicted) {
    auto locked_chain = chain().lock();
    LOCK(cs_wallet);
//TODO:暂时确保之前通知了mempool删除
//关联交易。这不重要，但被抛弃的
//当我们
//收到另一个通知，并且存在一个竞赛条件，其中
//通知已连接的冲突可能会导致外部进程
//放弃一项交易，然后在不经意间通过
//冲突事务被逐出的通知。

    for (const CTransactionRef& ptx : vtxConflicted) {
        SyncTransaction(ptx);
        TransactionRemovedFromMempool(ptx);
    }
    for (size_t i = 0; i < pblock->vtx.size(); i++) {
        SyncTransaction(pblock->vtx[i], pindex, i);
        TransactionRemovedFromMempool(pblock->vtx[i]);
    }

    m_last_block_processed = pindex;
}

void CWallet::BlockDisconnected(const std::shared_ptr<const CBlock>& pblock) {
    auto locked_chain = chain().lock();
    LOCK(cs_wallet);

    for (const CTransactionRef& ptx : pblock->vtx) {
        SyncTransaction(ptx);
    }
}



void CWallet::BlockUntilSyncedToCurrentChain() {
    AssertLockNotHeld(cs_main);
    AssertLockNotHeld(cs_wallet);

    {
//如果我们知道我们已经赶上了，那就不要排队排尿了。
//chainActive.tip（）..
//我们也可以在这里取CS钱包，然后打电话给M处理过的最后一个块
//用钱夹代替钱夹保护，但只要我们需要
//不管怎样，在这里称它为“受保护的主控系统”比较容易。
        auto locked_chain = chain().lock();
        const CBlockIndex* initialChainTip = chainActive.Tip();

        if (m_last_block_processed && m_last_block_processed->GetAncestor(initialChainTip->nHeight) == initialChainTip) {
            return;
        }
    }

//…否则，在验证接口队列中放入回调并等待
//使队列有足够的空间来执行它（表示我们被捕获了
//至少在我们进入这个函数的时候）。
    SyncWithValidationInterfaceQueue();
}


isminetype CWallet::IsMine(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.tx->vout.size())
                return IsMine(prev.tx->vout[txin.prevout.n]);
        }
    }
    return ISMINE_NO;
}

//注意，此函数不区分0值输入，
//和一个非“是我的”（根据过滤器）输入。
CAmount CWallet::GetDebit(const CTxIn &txin, const isminefilter& filter) const
{
    {
        LOCK(cs_wallet);
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.tx->vout.size())
                if (IsMine(prev.tx->vout[txin.prevout.n]) & filter)
                    return prev.tx->vout[txin.prevout.n].nValue;
        }
    }
    return 0;
}

isminetype CWallet::IsMine(const CTxOut& txout) const
{
    return ::IsMine(*this, txout.scriptPubKey);
}

CAmount CWallet::GetCredit(const CTxOut& txout, const isminefilter& filter) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error(std::string(__func__) + ": value out of range");
    return ((IsMine(txout) & filter) ? txout.nValue : 0);
}

bool CWallet::IsChange(const CTxOut& txout) const
{
    return IsChange(txout.scriptPubKey);
}

bool CWallet::IsChange(const CScript& script) const
{
//TODO:修复对“更改”输出的处理。假设是
//支付给我们的脚本，但不在通讯簿中
//是变化。当我们执行多重签名时，这个假设可能会被打破。
//返回的钱包变回多签名保护地址；
//识别哪些输出是“发送”和哪些是
//需要实现“更改”（可能需要扩展cwallettx以记住
//哪种输出，如果有的话，是变化的）。
    if (::IsMine(*this, script))
    {
        CTxDestination address;
        if (!ExtractDestination(script, address))
            return true;

        LOCK(cs_wallet);
        if (!mapAddressBook.count(address))
            return true;
    }
    return false;
}

CAmount CWallet::GetChange(const CTxOut& txout) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error(std::string(__func__) + ": value out of range");
    return (IsChange(txout) ? txout.nValue : 0);
}

bool CWallet::IsMine(const CTransaction& tx) const
{
    for (const CTxOut& txout : tx.vout)
        if (IsMine(txout))
            return true;
    return false;
}

bool CWallet::IsFromMe(const CTransaction& tx) const
{
    return (GetDebit(tx, ISMINE_ALL) > 0);
}

CAmount CWallet::GetDebit(const CTransaction& tx, const isminefilter& filter) const
{
    CAmount nDebit = 0;
    for (const CTxIn& txin : tx.vin)
    {
        nDebit += GetDebit(txin, filter);
        if (!MoneyRange(nDebit))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nDebit;
}

bool CWallet::IsAllFromMe(const CTransaction& tx, const isminefilter& filter) const
{
    LOCK(cs_wallet);

    for (const CTxIn& txin : tx.vin)
    {
        auto mi = mapWallet.find(txin.prevout.hash);
        if (mi == mapWallet.end())
return false; //任何未知输入都不能来自我们

        const CWalletTx& prev = (*mi).second;

        if (txin.prevout.n >= prev.tx->vout.size())
return false; //输入无效！

        if (!(IsMine(prev.tx->vout[txin.prevout.n]) & filter))
            return false;
    }
    return true;
}

CAmount CWallet::GetCredit(const CTransaction& tx, const isminefilter& filter) const
{
    CAmount nCredit = 0;
    for (const CTxOut& txout : tx.vout)
    {
        nCredit += GetCredit(txout, filter);
        if (!MoneyRange(nCredit))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nCredit;
}

CAmount CWallet::GetChange(const CTransaction& tx) const
{
    CAmount nChange = 0;
    for (const CTxOut& txout : tx.vout)
    {
        nChange += GetChange(txout);
        if (!MoneyRange(nChange))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nChange;
}

CPubKey CWallet::GenerateNewSeed()
{
    assert(!IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS));
    CKey key;
    key.MakeNewKey(true);
    return DeriveNewSeed(key);
}

CPubKey CWallet::DeriveNewSeed(const CKey& key)
{
    int64_t nCreationTime = GetTime();
    CKeyMetadata metadata(nCreationTime);

//计算种子
    CPubKey seed = key.GetPubKey();
    assert(key.VerifyPubKey(seed));

//将hd键路径设置为“s”->seed，将seed引用为自身
    metadata.hdKeypath     = "s";
    metadata.hd_seed_id = seed.GetID();

    {
        LOCK(cs_wallet);

//MEM存储元数据
        mapKeyMetadata[seed.GetID()] = metadata;

//将密钥和元数据写入数据库
        if (!AddKeyPubKey(key, seed))
            throw std::runtime_error(std::string(__func__) + ": AddKeyPubKey failed");
    }

    return seed;
}

void CWallet::SetHDSeed(const CPubKey& seed)
{
    LOCK(cs_wallet);
//将keyID（hash160）与
//数据库中的子索引计数器
//作为hdchain对象
    CHDChain newHdChain;
    newHdChain.nVersion = CanSupportFeature(FEATURE_HD_SPLIT) ? CHDChain::VERSION_HD_CHAIN_SPLIT : CHDChain::VERSION_HD_BASE;
    newHdChain.seed_id = seed.GetID();
    SetHDChain(newHdChain, false);
}

void CWallet::SetHDChain(const CHDChain& chain, bool memonly)
{
    LOCK(cs_wallet);
    if (!memonly && !WalletBatch(*database).WriteHDChain(chain))
        throw std::runtime_error(std::string(__func__) + ": writing chain failed");

    hdChain = chain;
}

bool CWallet::IsHDEnabled() const
{
    return !hdChain.seed_id.IsNull();
}

void CWallet::SetWalletFlag(uint64_t flags)
{
    LOCK(cs_wallet);
    m_wallet_flags |= flags;
    if (!WalletBatch(*database).WriteWalletFlags(m_wallet_flags))
        throw std::runtime_error(std::string(__func__) + ": writing wallet flags failed");
}

bool CWallet::IsWalletFlagSet(uint64_t flag)
{
    return (m_wallet_flags & flag);
}

bool CWallet::SetWalletFlags(uint64_t overwriteFlags, bool memonly)
{
    LOCK(cs_wallet);
    m_wallet_flags = overwriteFlags;
    if (((overwriteFlags & g_known_wallet_flags) >> 32) ^ (overwriteFlags >> 32)) {
//包含未知的不可容忍的钱包标志
        return false;
    }
    if (!memonly && !WalletBatch(*database).WriteWalletFlags(m_wallet_flags)) {
        throw std::runtime_error(std::string(__func__) + ": writing wallet flags failed");
    }

    return true;
}

int64_t CWalletTx::GetTxTime() const
{
    int64_t n = nTimeSmart;
    return n ? n : nTimeReceived;
}

//帮助生成最大大小的低S低R签名（例如71字节）
//或者，如果使用“max”sig为真，则为最大大小的低S签名（例如72字节）
bool CWallet::DummySignInput(CTxIn &tx_in, const CTxOut &txout, bool use_max_sig) const
{
//填写虚拟签名用于费用计算。
    const CScript& scriptPubKey = txout.scriptPubKey;
    SignatureData sigdata;

    if (!ProduceSignature(*this, use_max_sig ? DUMMY_MAXIMUM_SIGNATURE_CREATOR : DUMMY_SIGNATURE_CREATOR, scriptPubKey, sigdata)) {
        return false;
    }
    UpdateInput(tx_in, sigdata);
    return true;
}

//帮助生成一组最大大小的低S低R签名（例如71字节）
bool CWallet::DummySignTx(CMutableTransaction &txNew, const std::vector<CTxOut> &txouts, bool use_max_sig) const
{
//填写虚拟签名用于费用计算。
    int nIn = 0;
    for (const auto& txout : txouts)
    {
        if (!DummySignInput(txNew.vin[nIn], txout, use_max_sig)) {
            return false;
        }

        nIn++;
    }
    return true;
}

int64_t CalculateMaximumSignedTxSize(const CTransaction &tx, const CWallet *wallet, bool use_max_sig)
{
    std::vector<CTxOut> txouts;
//查找输入。我们应该已经检查过了
//IsalFromMe（ismine可消费），所以每个输入都应该已经在我们的
//钱包，带有有效的凭证数组索引，并且能够签名。
    for (const CTxIn& input : tx.vin) {
        const auto mi = wallet->mapWallet.find(input.prevout.hash);
        if (mi == wallet->mapWallet.end()) {
            return -1;
        }
        assert(input.prevout.n < mi->second.tx->vout.size());
        txouts.emplace_back(mi->second.tx->vout[input.prevout.n]);
    }
    return CalculateMaximumSignedTxSize(tx, wallet, txouts, use_max_sig);
}

//txout必须按照tx.vin的顺序
int64_t CalculateMaximumSignedTxSize(const CTransaction &tx, const CWallet *wallet, const std::vector<CTxOut>& txouts, bool use_max_sig)
{
    CMutableTransaction txNew(tx);
    if (!wallet->DummySignTx(txNew, txouts, use_max_sig)) {
//这是不应该发生的，因为伊莎尔弗洛姆（伊莎琳·斯佩布尔）
//意味着我们可以为每个输入签名。
        return -1;
    }
    return GetVirtualTransactionSize(txNew);
}

int CalculateMaximumSignedInputSize(const CTxOut& txout, const CWallet* wallet, bool use_max_sig)
{
    CMutableTransaction txn;
    txn.vin.push_back(CTxIn(COutPoint()));
    if (!wallet->DummySignInput(txn.vin[0], txout, use_max_sig)) {
        return -1;
    }
    return GetVirtualTransactionInputSize(txn.vin[0]);
}

void CWalletTx::GetAmounts(std::list<COutputEntry>& listReceived,
                           std::list<COutputEntry>& listSent, CAmount& nFee, const isminefilter& filter) const
{
    nFee = 0;
    listReceived.clear();
    listSent.clear();

//计算费：
    CAmount nDebit = GetDebit(filter);
if (nDebit > 0) //借方>0表示我们已签署/发送此交易
    {
        CAmount nValueOut = tx->GetValueOut();
        nFee = nDebit - nValueOut;
    }

//发送/接收。
    for (unsigned int i = 0; i < tx->vout.size(); ++i)
    {
        const CTxOut& txout = tx->vout[i];
        isminetype fIsMine = pwallet->IsMine(txout);
//只有在以下情况下才需要处理txout：
//1）从我方借记（发送）
//2）输出给我们（收到）
        if (nDebit > 0)
        {
//不报告“更改”txout
            if (pwallet->IsChange(txout))
                continue;
        }
        else if (!(fIsMine & filter))
            continue;

//无论哪种情况，我们都需要得到目的地地址
        CTxDestination address;

        if (!ExtractDestination(txout.scriptPubKey, address) && !txout.scriptPubKey.IsUnspendable())
        {
            pwallet->WalletLogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                                    this->GetHash().ToString());
            address = CNoDestination();
        }

        COutputEntry output = {address, txout.nValue, (int)i};

//如果我们被交易记入借方，则将输出添加为“已发送”条目。
        if (nDebit > 0)
            listSent.push_back(output);

//如果我们正在接收输出，请将其添加为“已接收”条目。
        if (fIsMine & filter)
            listReceived.push_back(output);
    }

}

/*
 *导入密钥后扫描活动链中的相关事务。这应该
 *在钱包中添加新钥匙时使用最旧的钥匙进行呼叫。
 *创建时间。
 *
 *@返回可以成功扫描的最早时间戳。时间戳
 *如果无法读取相关块，则返回的值将高于StartTime。
 **/

int64_t CWallet::RescanFromTime(int64_t startTime, const WalletRescanReserver& reserver, bool update)
{
//找到起始块。如果ncreateTime大于
//最高的区块链时间戳，在这种情况下不需要
//被扫描。
    CBlockIndex* startBlock = nullptr;
    {
        auto locked_chain = chain().lock();
        startBlock = chainActive.FindEarliestAtLeast(startTime - TIMESTAMP_WINDOW);
        WalletLogPrintf("%s: Rescanning last %i blocks\n", __func__, startBlock ? chainActive.Height() - startBlock->nHeight + 1 : 0);
    }

    if (startBlock) {
        const CBlockIndex *failedBlock, *stop_block;
//TODO:这应该考虑到scanresult:：user_abort失败
        if (ScanResult::FAILURE == ScanForWalletTransactions(startBlock, nullptr, reserver, failedBlock, stop_block, update)) {
            return failedBlock->GetBlockTimeMax() + TIMESTAMP_WINDOW + 1;
        }
    }
    return startTime;
}

/*
 *扫描区块链（从pindexstart开始）中的事务
 *我们之间。如果fupdate为true，则找到已经
 *钱包中的存在将被更新。
 *
 *@param[in]pindexstop如果不是nullptr，扫描将在此块索引处停止。
 *@param[out]failed_block如果返回failure，则返回最新的块
 *无法扫描，否则为nullptr
 *@param[out]stop_块可以扫描的最新块，
 *否则，如果无法扫描任何块，则为nullptr
 *
 *@返回scanresult，表示扫描成功或失败。成功如果
 *扫描成功。如果无法完全重新扫描（由于
 *修剪或腐败）。如果在此之前重新扫描被中止，用户将中止
 *可以完成。
 *
 *@pre-caller需要确保pindexstop（和可选的pindexstart）已打开
 *添加任何要检测的新密钥后的主链
 *的交易。
 **/

CWallet::ScanResult CWallet::ScanForWalletTransactions(const CBlockIndex* const pindexStart, const CBlockIndex* const pindexStop, const WalletRescanReserver& reserver, const CBlockIndex*& failed_block, const CBlockIndex*& stop_block, bool fUpdate)
{
    int64_t nNow = GetTime();
    const CChainParams& chainParams = Params();

    assert(reserver.isReserved());
    if (pindexStop) {
        assert(pindexStop->nHeight >= pindexStart->nHeight);
    }

    const CBlockIndex* pindex = pindexStart;
    failed_block = nullptr;
    stop_block = nullptr;

    if (pindex) WalletLogPrintf("Rescan started from block %d...\n", pindex->nHeight);

    {
        fAbortRescan = false;
ShowProgress(strprintf("%s " + _("Rescanning..."), GetDisplayName()), 0); //在GUI中以对话框或SplashScreen显示重新扫描进度，如果-启动时重新扫描
        CBlockIndex* tip = nullptr;
        double progress_begin;
        double progress_end;
        {
            auto locked_chain = chain().lock();
            progress_begin = GuessVerificationProgress(chainParams.TxData(), pindex);
            if (pindexStop == nullptr) {
                tip = chainActive.Tip();
                progress_end = GuessVerificationProgress(chainParams.TxData(), tip);
            } else {
                progress_end = GuessVerificationProgress(chainParams.TxData(), pindexStop);
            }
        }
        double progress_current = progress_begin;
        while (pindex && !fAbortRescan && !ShutdownRequested()) {
            if (pindex->nHeight % 100 == 0 && progress_end - progress_begin > 0.0) {
                ShowProgress(strprintf("%s " + _("Rescanning..."), GetDisplayName()), std::max(1, std::min(99, (int)((progress_current - progress_begin) / (progress_end - progress_begin) * 100))));
            }
            if (GetTime() >= nNow + 60) {
                nNow = GetTime();
                WalletLogPrintf("Still rescanning. At block %d. Progress=%f\n", pindex->nHeight, progress_current);
            }

            CBlock block;
            if (ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
                auto locked_chain = chain().lock();
                LOCK(cs_wallet);
                if (pindex && !chainActive.Contains(pindex)) {
//如果当前块不再处于活动状态，则中止扫描，以防止
//将事务标记为来自错误块。
                    failed_block = pindex;
                    break;
                }
                for (size_t posInBlock = 0; posInBlock < block.vtx.size(); ++posInBlock) {
                    SyncTransaction(block.vtx[posInBlock], pindex, posInBlock, fUpdate);
                }
//扫描成功，将块记录为最近成功扫描的块
                stop_block = pindex;
            } else {
//无法扫描块，继续扫描，但将此块记录为最近的失败
                failed_block = pindex;
            }
            if (pindex == pindexStop) {
                break;
            }
            {
                auto locked_chain = chain().lock();
                pindex = chainActive.Next(pindex);
                progress_current = GuessVerificationProgress(chainParams.TxData(), pindex);
                if (pindexStop == nullptr && tip != chainActive.Tip()) {
                    tip = chainActive.Tip();
//如果提示已更改，则更新最大进度
                    progress_end = GuessVerificationProgress(chainParams.TxData(), tip);
                }
            }
        }
ShowProgress(strprintf("%s " + _("Rescanning..."), GetDisplayName()), 100); //在GUI中隐藏进度对话框
        if (pindex && fAbortRescan) {
            WalletLogPrintf("Rescan aborted at block %d. Progress=%f\n", pindex->nHeight, progress_current);
            return ScanResult::USER_ABORT;
        } else if (pindex && ShutdownRequested()) {
            WalletLogPrintf("Rescan interrupted by shutdown request at block %d. Progress=%f\n", pindex->nHeight, progress_current);
            return ScanResult::USER_ABORT;
        }
    }
    if (failed_block) {
        return ScanResult::FAILURE;
    } else {
        return ScanResult::SUCCESS;
    }
}

void CWallet::ReacceptWalletTransactions()
{
//如果事务没有被广播，也不要让它们进入本地mempool
    if (!fBroadcastTransactions)
        return;
    auto locked_chain = chain().lock();
    LOCK(cs_wallet);
    std::map<int64_t, CWalletTx*> mapSorted;

//根据初始钱包插入顺序对挂起的钱包交易进行排序
    for (std::pair<const uint256, CWalletTx>& item : mapWallet)
    {
        const uint256& wtxid = item.first;
        CWalletTx& wtx = item.second;
        assert(wtx.GetHash() == wtxid);

        int nDepth = wtx.GetDepthInMainChain(*locked_chain);

        if (!wtx.IsCoinBase() && (nDepth == 0 && !wtx.isAbandoned())) {
            mapSorted.insert(std::make_pair(wtx.nOrderPos, &wtx));
        }
    }

//尝试将钱包交易添加到内存池
    for (const std::pair<const int64_t, CWalletTx*>& item : mapSorted) {
        CWalletTx& wtx = *(item.second);
        CValidationState state;
        wtx.AcceptToMemoryPool(*locked_chain, maxTxFee, state);
    }
}

bool CWalletTx::RelayWalletTransaction(interfaces::Chain::Lock& locked_chain, CConnman* connman)
{
    assert(pwallet->GetBroadcastTransactions());
    if (!IsCoinBase() && !isAbandoned() && GetDepthInMainChain(locked_chain) == 0)
    {
        CValidationState state;
        /*GetDepthinMainchain已捕获已知冲突。*/
        if (InMempool() || AcceptToMemoryPool(locked_chain, maxTxFee, state)) {
            pwallet->WalletLogPrintf("Relaying wtx %s\n", GetHash().ToString());
            if (connman) {
                CInv inv(MSG_TX, GetHash());
                connman->ForEachNode([&inv](CNode* pnode)
                {
                    pnode->PushInventory(inv);
                });
                return true;
            }
        }
    }
    return false;
}

std::set<uint256> CWalletTx::GetConflicts() const
{
    std::set<uint256> result;
    if (pwallet != nullptr)
    {
        uint256 myHash = GetHash();
        result = pwallet->GetConflicts(myHash);
        result.erase(myHash);
    }
    return result;
}

CAmount CWalletTx::GetDebit(const isminefilter& filter) const
{
    if (tx->vin.empty())
        return 0;

    CAmount debit = 0;
    if(filter & ISMINE_SPENDABLE)
    {
        if (fDebitCached)
            debit += nDebitCached;
        else
        {
            nDebitCached = pwallet->GetDebit(*tx, ISMINE_SPENDABLE);
            fDebitCached = true;
            debit += nDebitCached;
        }
    }
    if(filter & ISMINE_WATCH_ONLY)
    {
        if(fWatchDebitCached)
            debit += nWatchDebitCached;
        else
        {
            nWatchDebitCached = pwallet->GetDebit(*tx, ISMINE_WATCH_ONLY);
            fWatchDebitCached = true;
            debit += nWatchDebitCached;
        }
    }
    return debit;
}

CAmount CWalletTx::GetCredit(interfaces::Chain::Lock& locked_chain, const isminefilter& filter) const
{
//必须等到CoinBase安全地深入到链中，才能对其进行估值。
    if (IsImmatureCoinBase(locked_chain))
        return 0;

    CAmount credit = 0;
    if (filter & ISMINE_SPENDABLE)
    {
//GetBalance可以假设Mapwallet中的交易不会更改
        if (fCreditCached)
            credit += nCreditCached;
        else
        {
            nCreditCached = pwallet->GetCredit(*tx, ISMINE_SPENDABLE);
            fCreditCached = true;
            credit += nCreditCached;
        }
    }
    if (filter & ISMINE_WATCH_ONLY)
    {
        if (fWatchCreditCached)
            credit += nWatchCreditCached;
        else
        {
            nWatchCreditCached = pwallet->GetCredit(*tx, ISMINE_WATCH_ONLY);
            fWatchCreditCached = true;
            credit += nWatchCreditCached;
        }
    }
    return credit;
}

CAmount CWalletTx::GetImmatureCredit(interfaces::Chain::Lock& locked_chain, bool fUseCache) const
{
    if (IsImmatureCoinBase(locked_chain) && IsInMainChain(locked_chain)) {
        if (fUseCache && fImmatureCreditCached)
            return nImmatureCreditCached;
        nImmatureCreditCached = pwallet->GetCredit(*tx, ISMINE_SPENDABLE);
        fImmatureCreditCached = true;
        return nImmatureCreditCached;
    }

    return 0;
}

CAmount CWalletTx::GetAvailableCredit(interfaces::Chain::Lock& locked_chain, bool fUseCache, const isminefilter& filter) const
{
    if (pwallet == nullptr)
        return 0;

//必须等到CoinBase安全地深入到链中，才能对其进行估值。
    if (IsImmatureCoinBase(locked_chain))
        return 0;

    CAmount* cache = nullptr;
    bool* cache_used = nullptr;

    if (filter == ISMINE_SPENDABLE) {
        cache = &nAvailableCreditCached;
        cache_used = &fAvailableCreditCached;
    } else if (filter == ISMINE_WATCH_ONLY) {
        cache = &nAvailableWatchCreditCached;
        cache_used = &fAvailableWatchCreditCached;
    }

    if (fUseCache && cache_used && *cache_used) {
        return *cache;
    }

    CAmount nCredit = 0;
    uint256 hashTx = GetHash();
    for (unsigned int i = 0; i < tx->vout.size(); i++)
    {
        if (!pwallet->IsSpent(locked_chain, hashTx, i))
        {
            const CTxOut &txout = tx->vout[i];
            nCredit += pwallet->GetCredit(txout, filter);
            if (!MoneyRange(nCredit))
                throw std::runtime_error(std::string(__func__) + " : value out of range");
        }
    }

    if (cache) {
        *cache = nCredit;
        assert(cache_used);
        *cache_used = true;
    }
    return nCredit;
}

CAmount CWalletTx::GetImmatureWatchOnlyCredit(interfaces::Chain::Lock& locked_chain, const bool fUseCache) const
{
    if (IsImmatureCoinBase(locked_chain) && IsInMainChain(locked_chain)) {
        if (fUseCache && fImmatureWatchCreditCached)
            return nImmatureWatchCreditCached;
        nImmatureWatchCreditCached = pwallet->GetCredit(*tx, ISMINE_WATCH_ONLY);
        fImmatureWatchCreditCached = true;
        return nImmatureWatchCreditCached;
    }

    return 0;
}

CAmount CWalletTx::GetChange() const
{
    if (fChangeCached)
        return nChangeCached;
    nChangeCached = pwallet->GetChange(*tx);
    fChangeCached = true;
    return nChangeCached;
}

bool CWalletTx::InMempool() const
{
    return fInMempool;
}

bool CWalletTx::IsTrusted(interfaces::Chain::Lock& locked_chain) const
{
LockAnnotation lock(::cs_main); //临时，用于下面的checkfinaltx。在即将到来的提交中删除。

//大多数情况下快速回答
    if (!CheckFinalTx(*tx))
        return false;
    int nDepth = GetDepthInMainChain(locked_chain);
    if (nDepth >= 1)
        return true;
    if (nDepth < 0)
        return false;
if (!pwallet->m_spend_zero_conf_change || !IsFromMe(ISMINE_ALL)) //使用WTX的缓存借记
        return false;

//不要相信我们未确认的交易，除非它们在mempool中。
    if (!InMempool())
        return false;

//如果所有输入都来自我们并且在mempool中，则受信任：
    for (const CTxIn& txin : tx->vin)
    {
//我们未发送的交易：不可信
        const CWalletTx* parent = pwallet->GetWalletTx(txin.prevout.hash);
        if (parent == nullptr)
            return false;
        const CTxOut& parentOut = parent->tx->vout[txin.prevout.n];
        if (pwallet->IsMine(parentOut) != ISMINE_SPENDABLE)
            return false;
    }
    return true;
}

bool CWalletTx::IsEquivalentTo(const CWalletTx& _tx) const
{
        CMutableTransaction tx1 {*this->tx};
        CMutableTransaction tx2 {*_tx.tx};
        for (auto& txin : tx1.vin) txin.scriptSig = CScript();
        for (auto& txin : tx2.vin) txin.scriptSig = CScript();
        return CTransaction(tx1) == CTransaction(tx2);
}

std::vector<uint256> CWallet::ResendWalletTransactionsBefore(interfaces::Chain::Lock& locked_chain, int64_t nTime, CConnman* connman)
{
    std::vector<uint256> result;

    LOCK(cs_wallet);

//按时间顺序排序
    std::multimap<unsigned int, CWalletTx*> mapSorted;
    for (std::pair<const uint256, CWalletTx>& item : mapWallet)
    {
        CWalletTx& wtx = item.second;
//如果比NTIME更新，请不要重新广播：
        if (wtx.nTimeReceived > nTime)
            continue;
        mapSorted.insert(std::make_pair(wtx.nTimeReceived, &wtx));
    }
    for (const std::pair<const unsigned int, CWalletTx*>& item : mapSorted)
    {
        CWalletTx& wtx = *item.second;
        if (wtx.RelayWalletTransaction(locked_chain, connman))
            result.push_back(wtx.GetHash());
    }
    return result;
}

void CWallet::ResendWalletTransactions(int64_t nBestBlockTime, CConnman* connman)
{
//偶尔和随机地这样做以避免放弃
//这是我们的交易。
    if (GetTime() < nNextResend || !fBroadcastTransactions)
        return;
    bool fFirst = (nNextResend == 0);
    nNextResend = GetTime() + GetRand(30 * 60);
    if (fFirst)
        return;

//只有在上一次有新街区的情况下才这么做
    if (nBestBlockTime < nLastResend)
        return;
    nLastResend = GetTime();

//重播前5分钟以上未确认的txes
//找到块：
auto locked_chain = chain().assumeLocked();  //暂时的。在即将进行的锁清理中删除
    std::vector<uint256> relayed = ResendWalletTransactionsBefore(*locked_chain, nBestBlockTime-5*60, connman);
    if (!relayed.empty())
        WalletLogPrintf("%s: rebroadcast %u unconfirmed transactions\n", __func__, relayed.size());
}

/*@*///地图钱包结束




/**@defgroup操作
 *
 *@
 **/



CAmount CWallet::GetBalance(const isminefilter& filter, const int min_depth) const
{
    CAmount nTotal = 0;
    {
        auto locked_chain = chain().lock();
        LOCK(cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            if (pcoin->IsTrusted(*locked_chain) && pcoin->GetDepthInMainChain(*locked_chain) >= min_depth) {
                nTotal += pcoin->GetAvailableCredit(*locked_chain, true, filter);
            }
        }
    }

    return nTotal;
}

CAmount CWallet::GetUnconfirmedBalance() const
{
    CAmount nTotal = 0;
    {
        auto locked_chain = chain().lock();
        LOCK(cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            if (!pcoin->IsTrusted(*locked_chain) && pcoin->GetDepthInMainChain(*locked_chain) == 0 && pcoin->InMempool())
                nTotal += pcoin->GetAvailableCredit(*locked_chain);
        }
    }
    return nTotal;
}

CAmount CWallet::GetImmatureBalance() const
{
    CAmount nTotal = 0;
    {
        auto locked_chain = chain().lock();
        LOCK(cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            nTotal += pcoin->GetImmatureCredit(*locked_chain);
        }
    }
    return nTotal;
}

CAmount CWallet::GetUnconfirmedWatchOnlyBalance() const
{
    CAmount nTotal = 0;
    {
        auto locked_chain = chain().lock();
        LOCK(cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            if (!pcoin->IsTrusted(*locked_chain) && pcoin->GetDepthInMainChain(*locked_chain) == 0 && pcoin->InMempool())
                nTotal += pcoin->GetAvailableCredit(*locked_chain, true, ISMINE_WATCH_ONLY);
        }
    }
    return nTotal;
}

CAmount CWallet::GetImmatureWatchOnlyBalance() const
{
    CAmount nTotal = 0;
    {
        auto locked_chain = chain().lock();
        LOCK(cs_wallet);
        for (const auto& entry : mapWallet)
        {
            const CWalletTx* pcoin = &entry.second;
            nTotal += pcoin->GetImmatureWatchOnlyCredit(*locked_chain);
        }
    }
    return nTotal;
}

//以与GetBalance不同的方式计算总余额。最大的
//不同的是，getbalance将支付给
//钱包，而这两个总和已花和未用的txout支付给
//钱包，然后从钱包中减去txins消费的值。这个
//对未确认的交易也有较少的限制。
//可信的。
CAmount CWallet::GetLegacyBalance(const isminefilter& filter, int minDepth) const
{
LockAnnotation lock(::cs_main); //临时，用于下面的checkfinaltx。在即将到来的提交中删除。
    auto locked_chain = chain().lock();
    LOCK(cs_wallet);

    CAmount balance = 0;
    for (const auto& entry : mapWallet) {
        const CWalletTx& wtx = entry.second;
        const int depth = wtx.GetDepthInMainChain(*locked_chain);
        if (depth < 0 || !CheckFinalTx(*wtx.tx) || wtx.IsImmatureCoinBase(*locked_chain)) {
            continue;
        }

//循环通过Tx输出并添加传入付款。对于输出TXS，
//特别处理变更输出，作为借记金额的一部分。
        CAmount debit = wtx.GetDebit(filter);
        const bool outgoing = debit > 0;
        for (const CTxOut& out : wtx.tx->vout) {
            if (outgoing && IsChange(out)) {
                debit -= out.nValue;
            } else if (IsMine(out) & filter && depth >= minDepth) {
                balance += out.nValue;
            }
        }

//对于传出TXS，减去借记金额。
        if (outgoing) {
            balance -= debit;
        }
    }

    return balance;
}

CAmount CWallet::GetAvailableBalance(const CCoinControl* coinControl) const
{
    auto locked_chain = chain().lock();
    LOCK(cs_wallet);

    CAmount balance = 0;
    std::vector<COutput> vCoins;
    AvailableCoins(*locked_chain, vCoins, true, coinControl);
    for (const COutput& out : vCoins) {
        if (out.fSpendable) {
            balance += out.tx->tx->vout[out.i].nValue;
        }
    }
    return balance;
}

void CWallet::AvailableCoins(interfaces::Chain::Lock& locked_chain, std::vector<COutput> &vCoins, bool fOnlySafe, const CCoinControl *coinControl, const CAmount &nMinimumAmount, const CAmount &nMaximumAmount, const CAmount &nMinimumSumAmount, const uint64_t nMaximumCount, const int nMinDepth, const int nMaxDepth) const
{
    AssertLockHeld(cs_main);
    AssertLockHeld(cs_wallet);

    vCoins.clear();
    CAmount nTotal = 0;

    for (const auto& entry : mapWallet)
    {
        const uint256& wtxid = entry.first;
        const CWalletTx* pcoin = &entry.second;

        if (!CheckFinalTx(*pcoin->tx))
            continue;

        if (pcoin->IsImmatureCoinBase(locked_chain))
            continue;

        int nDepth = pcoin->GetDepthInMainChain(locked_chain);
        if (nDepth < 0)
            continue;

//我们不应该考虑那些至少不在我们记忆库里的硬币
//有可能这些通过祖先而产生冲突，我们可能永远无法察觉。
        if (nDepth == 0 && !pcoin->InMempool())
            continue;

        bool safeTx = pcoin->IsTrusted(locked_chain);

//我们不应该考虑换掉交易中的硬币
//其他交易。
//
//示例：有一个事务A被bumpfee替换
//事务B。在这种情况下，我们希望阻止创建
//花费B的输出的交易。
//
//原因：如果交易A最初确认，则交易B
//B'将不再有效，因此用户必须创建
//替换B'的新交易C。但是，在
//一个区块重组、交易B'和交易C都可以接受，
//当用户只想要其中一个的时候。具体来说，有可能
//与事务A和C所在的链保持1个块的重新排序
//被另一条链接受，其中b，b'和c都是
//认可的。
        if (nDepth == 0 && pcoin->mapValue.count("replaces_txid")) {
            safeTx = false;
        }

//同样，我们不应考虑来自交易的硬币
//已被替换。在上面的示例中，我们希望
//创建交易A'花费输出A，因为如果
//交易B最初被确认，与A和
//A’，我们不希望用户创建事务D
//打算替换，但可能导致
//其中a、a和d都可以接受（而不仅仅是b和
//或者只是一个和一个'用户想要的'）。
        if (nDepth == 0 && pcoin->mapValue.count("replaced_by_txid")) {
            safeTx = false;
        }

        if (fOnlySafe && !safeTx) {
            continue;
        }

        if (nDepth < nMinDepth || nDepth > nMaxDepth)
            continue;

        for (unsigned int i = 0; i < pcoin->tx->vout.size(); i++) {
            if (pcoin->tx->vout[i].nValue < nMinimumAmount || pcoin->tx->vout[i].nValue > nMaximumAmount)
                continue;

            if (coinControl && coinControl->HasSelected() && !coinControl->fAllowOtherInputs && !coinControl->IsSelected(COutPoint(entry.first, i)))
                continue;

            if (IsLockedCoin(entry.first, i))
                continue;

            if (IsSpent(locked_chain, wtxid, i))
                continue;

            isminetype mine = IsMine(pcoin->tx->vout[i]);

            if (mine == ISMINE_NO) {
                continue;
            }

            bool solvable = IsSolvable(*this, pcoin->tx->vout[i].scriptPubKey);
            bool spendable = ((mine & ISMINE_SPENDABLE) != ISMINE_NO) || (((mine & ISMINE_WATCH_ONLY) != ISMINE_NO) && (coinControl && coinControl->fAllowWatchOnly && solvable));

            vCoins.push_back(COutput(pcoin, i, nDepth, spendable, solvable, safeTx, (coinControl && coinControl->fAllowWatchOnly)));

//检查所有utxo的总和。
            if (nMinimumSumAmount != MAX_MONEY) {
                nTotal += pcoin->tx->vout[i].nValue;

                if (nTotal >= nMinimumSumAmount) {
                    return;
                }
            }

//检查utxo的最大数目。
            if (nMaximumCount > 0 && vCoins.size() >= nMaximumCount) {
                return;
            }
        }
    }
}

std::map<CTxDestination, std::vector<COutput>> CWallet::ListCoins(interfaces::Chain::Lock& locked_chain) const
{
    AssertLockHeld(cs_main);
    AssertLockHeld(cs_wallet);

    std::map<CTxDestination, std::vector<COutput>> result;
    std::vector<COutput> availableCoins;

    AvailableCoins(locked_chain, availableCoins);

    for (const COutput& coin : availableCoins) {
        CTxDestination address;
        if (coin.fSpendable &&
            ExtractDestination(FindNonChangeParentOutput(*coin.tx->tx, coin.i).scriptPubKey, address)) {
            result[address].emplace_back(std::move(coin));
        }
    }

    std::vector<COutPoint> lockedCoins;
    ListLockedCoins(lockedCoins);
    for (const COutPoint& output : lockedCoins) {
        auto it = mapWallet.find(output.hash);
        if (it != mapWallet.end()) {
            int depth = it->second.GetDepthInMainChain(locked_chain);
            if (depth >= 0 && output.n < it->second.tx->vout.size() &&
                IsMine(it->second.tx->vout[output.n]) == ISMINE_SPENDABLE) {
                CTxDestination address;
                if (ExtractDestination(FindNonChangeParentOutput(*it->second.tx, output.n).scriptPubKey, address)) {
                    result[address].emplace_back(
                        /*->second，output.n，depth，true/*spenable*/，true/*solvable*/，false/*safe*/）；
                }
            }
        }
    }

    返回结果；
}

const ctxout和cwallet：：findnonchangeparentoutput（const ctransaction&tx，int output）const
{
    const ctransaction*ptx=&tx；
    int＝输出；
    同时（ischange（ptx->vout[n]）&&ptx->vin.size（）>0）
        const coutpoint&prevout=ptx->vin[0].预览；
        auto it=mapwallet.find（prevout.hash）；
        if（it==mapwallet.end（）it->second.tx->vout.size（）<=prevout.n_
            ！ismine（it->second.tx->vout[prevout.n]）_
            断裂；
        }
        ptx=it->second.tx.get（）；
        n＝Primult.n；
    }
    返回ptx->vout[n]；
}

bool cwallet:：selectcoinsminconf（const camount&ntargetvalue，const coineligibilityfilter&qualification_filter，std:：vector<outputgroup>组，
                                 std：：set<cinputcoin>&setcoinsret，camount&nvalueret，const coin selection params&coin-selection_-params，bool&bnb-used）const
{
    setCoinsret.clear（）；
    nValueRET＝0；

    std:：vector<outputgroup>utxo_pool；
    如果（硬币选择参数使用bnb）
        //得到长期估计
        feecalculation feecalc；
        C控制温度；
        温度M_确认目标=1008；
        cfeerate long-term feerate=getminimumfeerate（*此，温度，：：mempool，：：feeestimator，&feecalc）；

        //计算变更成本
        camount cost_of_change=getDiscardRate（*this，：：feeestimator）.getFee（coin_selection_params.change_spend_size）+coin_selection_params.effective_fee.getFee（coin_selection_params.change_output_size）；

        //按最小配置规格过滤，并添加到utxo_池中，计算有效值
        用于（输出组和组：组）
            如果（！）group.eligibleforspending（合格性过滤））继续；

            费用＝0；
            group.long_term_fee=0；
            group.effective_值=0；
            对于（auto it=group.m_outputs.begin（）；it！=group.m_outputs.end（）；）
                const cinputcoin&coin=*it；
                camount effective_value=coin.txout.nvalue-（coin.m_input_bytes<0？0:币_选择_参数.有效_费用.获取费用（coin.m _输入_字节））；
                //只包括正有效值的输出（即不包括灰尘）
                如果（有效值>0）
                    group.fee+=coin.m_input_bytes<0？0:币\选择\参数.有效\费.getfee（coin.m \输入\字节）；
                    group.long_term_fee+=coin.m_input_bytes<0？0:long-term-feerate.getfee（coin.m-input-bytes）；
                    group.effective_value+=有效_value；
                    +++；
                }否则{
                    它=组。丢弃（硬币）；
                }
            }
            if（group.effective_value>0）utxo_pool.push_back（group）；
        }
        //计算非输入项的费用
        camount not_input_fees=coin_selection_params.effective_fee.getfee（coin_selection_params.tx_noinputs_size）；
        bnb_used=真；
        返回selectcoinsbnb（utxo_pool、ntargetvalue、cost_change、setcoinsret、nvalueret，而不是input_fees）；
    }否则{
        //按最小配置规格筛选并添加到utxo_池
        对于（const outputgroup&group:groups）
            如果（！）group.eligibleforspending（合格性过滤））继续；
            utxo_pool.push_back（组）；
        }
        bnb_used=false；
        返回背包解算器（nTargetValue，utxo_pool，setCoinsret，nValueret）；
    }
}

bool cwallet:：selectcoins（const std:：vector<coutput>和vavavailablecoins，const camount&ntargetvalue，std:：set<cinputcoin>和setcoinsret，camount&nvalueret，const ccoincontrol&coinou control，coin selection params&coinou selectionou params，bool&bnbouse used）const
{
    std:：vector<coutput>vcoins（vavavailablecoins）；

    //coin control->return all selected outputs（我们希望所有选中的都进入事务处理）
    如果（coinou control.hasselected（）&&！硬币控制（法洛热输入）
    {
        //这里没有使用bnb，所以设置为false。
        bnb_used=false；

        用于（const coutput&out:vcoins）
        {
            如果（！）出来了。
                 继续；
            nvalueret+=out.tx->tx->vout[out.i].nvalue；
            setCoinsret.insert（out.getInputCoin（））；
        }
        返回（nValueReet>=nTargetValue）；
    }

    //从预设输入中计算值并存储它们
    std:：set<cinputcoin>setpresetcoins；
    camount nvaluefrompresetinputs=0；

    std:：vector<coutpoint>vpresetinputs；
    已选择硬币控制列表（vpresetinputs）；
    for（const coutpoint&outpoint:vpresetinputs）
    {
        //现在，如果选择了预设输入，则不要使用bnb。TODO:稍后启用
        bnb_used=false；
        coin_selection_params.use_bnb=false；

        std:：map<uint256，cwallettx>：：const_iterator it=mapwallet.find（outpoint.hash）；
        如果（它）！=mapwallet.end（））
        {
            const cwallettx*pcoin=&it->second；
            //显然输入无效，失败
            if（pcoin->tx->vout.size（）<=outpoint.n）
                返回错误；
            //只计算边缘字节大小
            nvaluefrompresetinputs+=pcoin->tx->vout[outpoint.n].nvalue；
            setpresetcoins.insert（cinputcoin（pcoin->tx，outpoint.n））；
        }其他
            返回false；//TODO:允许非钱包输入
    }

    //从vcoins中删除预设输入
    对于（std:：vector<coutput>：：迭代器it=vcoins.begin（）；it！=vcoins.end（）&&coincontrol.hasselected（）；）
    {
        if（setpresetcoins.count（it->getinputcoin（））
            它=vcoins.erase（它）；
        其他的
            +++；
    }

    //从剩余硬币组成组；注意，预设硬币不会
    //自动包含相关（相同地址）硬币
    if（coin_control.m_避免部分_花费&&vcoins.size（）>输出_group_max_项）
        //如果我们有11个以上的输出都指向同一个目标，则可能导致
        //隐私泄漏，因为它们可能会被确定地排序。我们通过
        //在处理之前显式地改变输出
        随机播放（vcoins.begin（），vcoins.end（），fastRandomContext（））；
    }
    std:：vector<outputgroup>groups=groupoutputs（vcoins，！硬币控制，避免部分花费；

    size_t max_祖先=（size_t）std:：max<int64_t>（1，gargs.getarg（“-limitancestorcount”，default_祖先_限制））；
    size_t max_descendants=（size_t）std:：max<int64_t>（1，gargs.getarg（“-limitDescendantCount”，default_descendant_limit））；
    bool frejectlongchains=gargs.getboolarg（“-wallet reject long chains”，默认的_wallet_reject_long_chains）；

    bool res=nTargetValue<=nValueFromComparistinPuts
        selectcoinsminconf（nTargetValue-nValueFromComparistinPuts，coineligibilityFilter（1，6，0），groups，setcoinsret，nValueret，coin_selection_params，bnb_used）
        selectcoinsminconf（nTargetValue-nValueFromComparistinPuts，coineligibilityFilter（1，1，0），groups，setcoinsret，nValueret，coin_selection_params，bnb_used）
        （M U花费零配置更改并选择CoinsMinConf（nTargetValue-nValueCompareTinPuts，CoinEligibilityFilter（0，1，2），Groups，SetCoinSret，nValueRet，Coin选择参数，bnb_已使用））
        （m花费零配置更改并选择CoinsMinConf（nTargetValue-nValueFromPresetInPuts，CoinEligibilityFilter（0，1，Std:：Min（（大小t）4，Max祖先/3），Std:：Min（（大小t）4，Max后代/3）），Groups，SetCoinsret，NValueret，Coin选择参数，BNB
        （m花费零配置更改并选择CoinsMinConf（nTargetValue-nValueComparistinPuts，CoinEligibilityFilter（0，1，Max祖先/2，Max后代/2），组，setCoinsret，nValueReet，Coin选择参数，bnb使用））
        （m花费零配置更改并选择CoinsMinConf（nTargetValue-nValueFromResetinPuts，CoinEligibilityFilter（0，1，Max祖先-1，Max后代-1），组，setCoinSret，nValueReet，Coin选择参数，bnb使用））
        （M花零配置更改&！frejectLongChains&&selectCoinsMinConf（nTargetValue-nValueRefompresetinPuts，CoineLigibilityFilter（0，1，std:：Numeric_Limits<uint64_t>：max（）），Groups，SetCoinsret，nValueret，Coin_Selection_Params，bnb_used））；

    //因为selectcoinsminconf清除setcoinsret，我们现在将可能的输入添加到coinset
    util：：插入（setcoinsret，setpresetcoins）；

    //将预设输入添加到所选的总值中
    nvalueret+=nvaluefrompresetinputs；

    收益率；
}

bool cwallet:：signTransaction（cmutableTransaction&tx）
{
    断言锁定（cs_wallet）；//mapwallet

    //签署新的Tx
    INTN＝0；
    用于（自动和输入：tx.vin）
        std:：map<uint256，cwallettx>：：const_iterator mi=mapwallet.find（input.prevout.hash）；
        if（mi==mapwallet.end（）input.prevout.n>=mi->second.tx->vout.size（））
            返回错误；
        }
        const cscript&scriptpubkey=mi->second.tx->vout[input.prevout.n].scriptpubkey；
        const camount&amount=mi->second.tx->vout[input.prevout.n].nvalue；
        签名数据；
        如果（！）produceSignature（*this，mutableTransactionSignatureCreator（&tx，nin，amount，sighash_all），scriptPubkey，sigData））；
            返回错误；
        }
        更新输入（输入，信号数据）；
        NIN＋；
    }
    回归真实；
}

bool cwallet:：fundtransaction（cmutabletransaction&tx、camount&nfeeret、int&nchangeposinot、std:：string&strfailreason、bool lockunspents、const std:：set<int>和setsubtractfeefromoutputs、ccoincontrol coincontrol）
{
    std:：vector<crecipient>vecsend；

    //将txout集转换为crecipient向量。
    对于（size_t idx=0；idx<tx.vout.size（）；idx++）
        const ctxout&txout=tx.vout[idx]；
        crecipient recipient=txout.scriptpubkey，txout.nvalue，setSubtractFeefromoutputs.count（idx）==1_
        vecsend.push_back（收件人）；
    }

    coinControl.fallowotherinputs=真；

    用于（const ctxin&txin:tx.vin）
        coincontrol.select（txin.prevout）；
    }

    //获取锁以防止在
    //CreateTransaction调用和LockCoin调用（当LockUnpents为true时）。
    自动锁定的_chain=chain（）.lock（）；
    锁（CS-U钱包）；

    creservekey reservekey（这个）；
    ctransactionref tx_新；
    如果（！）createTransaction（*locked_chain，vecsend，tx_new，reservekey，nfeeret，nchangeposinot，strfailreason，coincontrol，false））；
        返回错误；
    }

    如果（nchangeposinout！= -1）{
        插入（tx.vout.begin（）+nchangeposinout，tx_new->vout[nchangeposinout]）；
        //我们没有正常的创建/提交周期，不想冒险
        //重复使用更改，所以只需从这里的密钥池中删除密钥。
        reservekey.keepkey（）；
    }

    //从新事务复制输出大小；它们可能已经收取了费用
    //从中减去。
    for（unsigned int idx=0；idx<tx.vout.size（）；idx++）
        tx.vout[idx].nvalue=tx_new->vout[idx].nvalue；
    }

    //在保持原始txin脚本sig/顺序的同时添加新txin。
    对于（const ctxin&txin:tx_new->vin）
        如果（！）CoinControl.IsSelected（txin.prevout））
            tx.vin.向后推（txin）；

            如果（锁扣）
                锁币（txin.prevout）；
            }
        }
    }

    回归真实；
}

静态bool iscurrentforantifeesnip（interfaces:：chain:：lock&locked_chain）
{
    if（isitialBlockDownload（））
        返回错误；
    }
    constexpr int64_t max_anti_fee_sniping_tip_age=8*60*60；//秒
    if（chainActive.tip（）->getBlockTime（）<（getTime（）-max_anti_fee_sniping_tip_age））
        返回错误；
    }
    回归真实；
}

/**
 *为新事务返回基于高度的锁定时间（使用
 *当前链提示，除非我们未与当前链同步
 **/

static uint32_t GetLocktimeForNewTransaction(interfaces::Chain::Lock& locked_chain)
{
    uint32_t locktime;
//阻止费用削减。
//
//对于大型矿业公司，最佳区块中交易的价值
//内存池可以超过故意挖掘两个内存池的成本
//块以孤立当前最佳块。通过设置nlocktime使
//只有下一个块可以包含事务，我们不鼓励这样做。
//限制高度和限制块尺寸给矿工的做法
//考虑到减少费用削减的选择来完成这次攻击。
//
//一个简单的思考方法是从钱包的角度来看
//总是希望区块链向前发展。通过设置nlocktime
//我们基本上说我们只想要这个
//事务将出现在下一个块中；我们不希望
//通过允许交易出现在较低的高度来鼓励重组
//比下一块最好的叉链。
//
//当然，补贴足够高，交易量也很低
//够了，削减费用还不是问题，而是通过实施修复
//现在，我们确保不会编写假设
//阻止以后修复的查找时间。
    if (IsCurrentForAntiFeeSniping(locked_chain)) {
        locktime = chainActive.Height();

//其次，偶尔会随机选择一个时间甚至更远的时间，所以
//无论出于何种原因签署后延迟的交易，
//例如，高延迟混合网络和一些联合实现
//更好的隐私。
        if (GetRandInt(10) == 0)
            locktime = std::max(0, (int)locktime - GetRandInt(100));
    } else {
//如果我们的连锁店落后了，我们既不能阻止费用削减，也不能帮助
//高延迟事务的隐私。以避免泄漏
//唯一的“nlocktime指纹”，将nlocktime设置为常量。
        locktime = 0;
    }
    assert(locktime <= (unsigned int)chainActive.Height());
    assert(locktime < LOCKTIME_THRESHOLD);
    return locktime;
}

OutputType CWallet::TransactionChangeType(OutputType change_type, const std::vector<CRecipient>& vecSend)
{
//如果指定了-change type，则始终使用该更改类型。
    if (change_type != OutputType::CHANGE_AUTO) {
        return change_type;
    }

//如果m_default_address_type为legacy，则使用legacy address作为更改（偶数
//如果一些输出是p2wpkh或p2wsh）。
    if (m_default_address_type == OutputType::LEGACY) {
        return OutputType::LEGACY;
    }

//如果任何目的地是p2wpkh或p2wsh，则使用p2wpkh进行更改。
//输出。
    for (const auto& recipient : vecSend) {
//检查目标是否包含见证程序：
        int witnessversion = 0;
        std::vector<unsigned char> witnessprogram;
        if (recipient.scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
            return OutputType::BECH32;
        }
    }

//否则，请使用m_-default_-address_-type进行更改
    return m_default_address_type;
}

bool CWallet::CreateTransaction(interfaces::Chain::Lock& locked_chain, const std::vector<CRecipient>& vecSend, CTransactionRef& tx, CReserveKey& reservekey, CAmount& nFeeRet,
                         int& nChangePosInOut, std::string& strFailReason, const CCoinControl& coin_control, bool sign)
{
    CAmount nValue = 0;
    int nChangePosRequest = nChangePosInOut;
    unsigned int nSubtractFeeFromAmount = 0;
    for (const auto& recipient : vecSend)
    {
        if (nValue < 0 || recipient.nAmount < 0)
        {
            strFailReason = _("Transaction amounts must not be negative");
            return false;
        }
        nValue += recipient.nAmount;

        if (recipient.fSubtractFeeFromAmount)
            nSubtractFeeFromAmount++;
    }
    if (vecSend.empty())
    {
        strFailReason = _("Transaction must have at least one recipient");
        return false;
    }

    CMutableTransaction txNew;

    txNew.nLockTime = GetLocktimeForNewTransaction(locked_chain);

    FeeCalculation feeCalc;
    CAmount nFeeNeeded;
    int nBytes;
    {
        std::set<CInputCoin> setCoins;
        auto locked_chain = chain().lock();
        LOCK(cs_wallet);
        {
            std::vector<COutput> vAvailableCoins;
            AvailableCoins(*locked_chain, vAvailableCoins, true, &coin_control);
CoinSelectionParams coin_selection_params; //硬币选择参数，用假人初始化

//创建将在需要更改时使用的更改脚本
//TODO:传入scriptChange而不是reserveKey，因此
//更改交易并不总是支付到比特币地址
            CScript scriptChange;

//硬币控制：将更改发送到自定义地址
            if (!boost::get<CNoDestination>(&coin_control.destChange)) {
                scriptChange = GetScriptForDestination(coin_control.destChange);
} else { //无硬币控制：将更改发送到新生成的地址
//注意：我们在这里使用了一个新的键来防止它变得明显，哪一面是变化。
//缺点是不重用以前的密钥，如果
//如果备份没有用于更改的新私钥，则会还原备份。
//如果我们重用旧的密钥，就可以添加代码来查找和
//重新发现用我们的密钥写入以恢复的未知事务
//备份后更改。

//从密钥池中保留新密钥对
                if (IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
                    strFailReason = _("Can't generate a change-address key. Private keys are disabled for this wallet.");
                    return false;
                }
                CPubKey vchPubKey;
                bool ret;
                ret = reservekey.GetReservedKey(vchPubKey, true);
                if (!ret)
                {
                    strFailReason = _("Keypool ran out, please call keypoolrefill first");
                    return false;
                }

                const OutputType change_type = TransactionChangeType(coin_control.m_change_type ? *coin_control.m_change_type : m_default_change_type, vecSend);

                LearnRelatedScripts(vchPubKey, change_type);
                scriptChange = GetScriptForDestination(GetDestinationForKey(vchPubKey, change_type));
            }
            CTxOut change_prototype_txout(0, scriptChange);
            coin_selection_params.change_output_size = GetSerializeSize(change_prototype_txout);

            CFeeRate discard_rate = GetDiscardRate(*this, ::feeEstimator);

//获取在硬币选择中使用有效值的费率
            CFeeRate nFeeRateNeeded = GetMinimumFeeRate(*this, coin_control, ::mempool, ::feeEstimator, &feeCalc);

            nFeeRet = 0;
            bool pick_new_inputs = true;
            CAmount nValueIn = 0;

//bnb选择器是唯一使用的选择器，当这是真的。
//这应该只在第一次通过循环时发生。
coin_selection_params.use_bnb = nSubtractFeeFromAmount == 0; //如果我们要从接收者那里减去费用，那么不要使用BNB。
//从没有费用开始循环直到有足够的费用
            while (true)
            {
                nChangePosInOut = nChangePosRequest;
                txNew.vin.clear();
                txNew.vout.clear();
                bool fFirst = true;

                CAmount nValueToSelect = nValue;
                if (nSubtractFeeFromAmount == 0)
                    nValueToSelect += nFeeRet;

//给收款人的凭证
coin_selection_params.tx_noinputs_size = 11; //静态vsize开销+输出vsize。4次转换，4次nlocktime，1次输入计数，1次输出计数，1次见证开销（虚拟，标志，堆栈大小）
                for (const auto& recipient : vecSend)
                {
                    CTxOut txout(recipient.nAmount, recipient.scriptPubKey);

                    if (recipient.fSubtractFeeFromAmount)
                    {
                        assert(nSubtractFeeFromAmount != 0);
txout.nValue -= nFeeRet / nSubtractFeeFromAmount; //从每个选定收件人中平均扣除费用

if (fFirst) //第一个接收者支付输出计数不可除的余数
                        {
                            fFirst = false;
                            txout.nValue -= nFeeRet % nSubtractFeeFromAmount;
                        }
                    }
//包括输出的费用成本。注意这只用于BNB现在
                    coin_selection_params.tx_noinputs_size += ::GetSerializeSize(txout, PROTOCOL_VERSION);

                    if (IsDust(txout, ::dustRelayFee))
                    {
                        if (recipient.fSubtractFeeFromAmount && nFeeRet > 0)
                        {
                            if (txout.nValue < 0)
                                strFailReason = _("The transaction amount is too small to pay the fee");
                            else
                                strFailReason = _("The transaction amount is too small to send after the fee has been deducted");
                        }
                        else
                            strFailReason = _("Transaction amount too small");
                        return false;
                    }
                    txNew.vout.push_back(txout);
                }

//选择要使用的硬币
                bool bnb_used;
                if (pick_new_inputs) {
                    nValueIn = 0;
                    setCoins.clear();
                    int change_spend_size = CalculateMaximumSignedInputSize(change_prototype_txout, this);
//如果钱包不知道如何签名更改输出，假设p2sh-p2wpkh
//作为允许BNB做这件事的下限
                    if (change_spend_size == -1) {
                        coin_selection_params.change_spend_size = DUMMY_NESTED_P2WPKH_INPUT_SIZE;
                    } else {
                        coin_selection_params.change_spend_size = (size_t)change_spend_size;
                    }
                    coin_selection_params.effective_fee = nFeeRateNeeded;
                    if (!SelectCoins(vAvailableCoins, nValueToSelect, setCoins, nValueIn, coin_control, coin_selection_params, bnb_used))
                    {
//如果使用BNB，这是第一次通过。不再是第一次传球，继续用背包回环。
                        if (bnb_used) {
                            coin_selection_params.use_bnb = false;
                            continue;
                        }
                        else {
                            strFailReason = _("Insufficient funds");
                            return false;
                        }
                    }
                } else {
                    bnb_used = false;
                }

                const CAmount nChange = nValueIn - nValueToSelect;
                if (nChange > 0)
                {
//给我们自己填一张担保书
                    CTxOut newTxOut(nChange, scriptChange);

//永远不要产生灰尘；如果我们愿意，只要
//在费用中加上灰尘。
//使用BNB时的更改总是要收费。
                    if (IsDust(newTxOut, discard_rate) || bnb_used)
                    {
                        nChangePosInOut = -1;
                        nFeeRet += nChange;
                    }
                    else
                    {
                        if (nChangePosInOut == -1)
                        {
//在任意位置插入更改txn：
                            nChangePosInOut = GetRandInt(txNew.vout.size()+1);
                        }
                        else if ((unsigned int)nChangePosInOut > txNew.vout.size())
                        {
                            strFailReason = _("Change index out of range");
                            return false;
                        }

                        std::vector<CTxOut>::iterator position = txNew.vout.begin()+nChangePosInOut;
                        txNew.vout.insert(position, newTxOut);
                    }
                } else {
                    nChangePosInOut = -1;
                }

//用于最大尺寸估计的虚拟填充VIN
//
                for (const auto& coin : setCoins) {
                    txNew.vin.push_back(CTxIn(coin.outpoint,CScript()));
                }

                nBytes = CalculateMaximumSignedTxSize(txNew, this, coin_control.fAllowWatchOnly);
                if (nBytes < 0) {
                    strFailReason = _("Signing transaction failed");
                    return false;
                }

                nFeeNeeded = GetMinimumFee(*this, nBytes, coin_control, ::mempool, ::feeEstimator, &feeCalc);
                if (feeCalc.reason == FeeReason::FALLBACK && !m_allow_fallback_fee) {
//最终允许退费
                    strFailReason = _("Fee estimation failed. Fallbackfee is disabled. Wait a few blocks or enable -fallbackfee.");
                    return false;
                }

//如果我们在这里成功了，我们甚至不能在下一个通行证上支付中转费，就放弃吧。
//因为我们必须在允许的最高费用。
                if (nFeeNeeded < ::minRelayTxFee.GetFee(nBytes))
                {
                    strFailReason = _("Transaction too large for fee policy");
                    return false;
                }

                if (nFeeRet >= nFeeNeeded) {
//尽可能将费用减少到所需的数额。这个
//如果硬币
//选择以满足NFeeRequired的结果是
//所需的费用比之前的迭代少。

//如果我们没有零钱和足够多的额外费用，那么
//尝试在不选择的情况下重新构造事务
//新投入。我们现在知道我们只需要较小的费用
//（因为减少了tx大小）因此我们应该添加
//更改输出。只试一次。
                    if (nChangePosInOut == -1 && nSubtractFeeFromAmount == 0 && pick_new_inputs) {
unsigned int tx_size_with_change = nBytes + coin_selection_params.change_output_size + 2; //增加2作为缓冲区，以防输出的增加改变紧凑的尺寸。
                        CAmount fee_needed_with_change = GetMinimumFee(*this, tx_size_with_change, coin_control, ::mempool, ::feeEstimator, nullptr);
                        CAmount minimum_value_for_change = GetDustThreshold(change_prototype_txout, discard_rate);
                        if (nFeeRet >= fee_needed_with_change + minimum_value_for_change) {
                            pick_new_inputs = false;
                            nFeeRet = fee_needed_with_change;
                            continue;
                        }
                    }

//如果我们已经改变输出，只需增加它
                    if (nFeeRet > nFeeNeeded && nChangePosInOut != -1 && nSubtractFeeFromAmount == 0) {
                        CAmount extraFeePaid = nFeeRet - nFeeNeeded;
                        std::vector<CTxOut>::iterator change_position = txNew.vout.begin()+nChangePosInOut;
                        change_position->nValue += extraFeePaid;
                        nFeeRet -= extraFeePaid;
                    }
break; //完成，包括足够的费用。
                }
                else if (!pick_new_inputs) {
//这不应该发生，我们应该有足够的过量
//支付新产品的费用，但仍需满足NFEEneeded
//或者我们应该从收信人那里减去费用，
//nfeeneeded不应更改
                    strFailReason = _("Transaction fee and change calculation failed");
                    return false;
                }

//尽量减少变化以包括必要的费用
                if (nChangePosInOut != -1 && nSubtractFeeFromAmount == 0) {
                    CAmount additionalFeeNeeded = nFeeNeeded - nFeeRet;
                    std::vector<CTxOut>::iterator change_position = txNew.vout.begin()+nChangePosInOut;
//只有在剩余数量仍然足够大的输出时才减少更改。
                    if (change_position->nValue >= MIN_FINAL_CHANGE + additionalFeeNeeded) {
                        change_position->nValue -= additionalFeeNeeded;
                        nFeeRet += additionalFeeNeeded;
break; //完成，能够从更改中增加费用
                    }
                }

//如果从接受者中减去费用，我们现在知道我们
//需要减去，我们没有理由重新选择输入
                if (nSubtractFeeFromAmount > 0) {
                    pick_new_inputs = false;
                }

//包括更多费用，然后再试一次。
                nFeeRet = nFeeNeeded;
                coin_selection_params.use_bnb = false;
                continue;
            }
        }

if (nChangePosInOut == -1) reservekey.ReturnKey(); //如果我们没有零钱，请返回任何保留的密钥

//洗牌选定的硬币并填写最终VIN
        txNew.vin.clear();
        std::vector<CInputCoin> selected_coins(setCoins.begin(), setCoins.end());
        Shuffle(selected_coins.begin(), selected_coins.end(), FastRandomContext());

//注意如何将序列号设置为非最大值，以便
//上面设置的nlocktime实际上有效。
//
//bip125将opt-in rbf定义为any nsequence<maxint-1，所以
//我们使用该范围内的最大可能值（maxint-2）
//为了避免与N序列的其他可能用途相冲突，
//本着“与以前相比可能发生的最小变化”的精神
//行为。”
        const uint32_t nSequence = coin_control.m_signal_bip125_rbf.get_value_or(m_signal_rbf) ? MAX_BIP125_RBF_SEQUENCE : (CTxIn::SEQUENCE_FINAL - 1);
        for (const auto& coin : selected_coins) {
            txNew.vin.push_back(CTxIn(coin.outpoint, CScript(), nSequence));
        }

        if (sign)
        {
            int nIn = 0;
            for (const auto& coin : selected_coins)
            {
                const CScript& scriptPubKey = coin.txout.scriptPubKey;
                SignatureData sigdata;

                if (!ProduceSignature(*this, MutableTransactionSignatureCreator(&txNew, nIn, coin.txout.nValue, SIGHASH_ALL), scriptPubKey, sigdata))
                {
                    strFailReason = _("Signing transaction failed");
                    return false;
                } else {
                    UpdateInput(txNew.vin.at(nIn), sigdata);
                }

                nIn++;
            }
        }

//返回构造的事务数据。
        tx = MakeTransactionRef(std::move(txNew));

//极限尺寸
        if (GetTransactionWeight(*tx) > MAX_STANDARD_TX_WEIGHT)
        {
            strFailReason = _("Transaction too large");
            return false;
        }
    }

    if (gArgs.GetBoolArg("-walletrejectlongchains", DEFAULT_WALLET_REJECT_LONG_CHAINS)) {
//最后，确保此Tx将通过mempool的链限制
        LockPoints lp;
        CTxMemPoolEntry entry(tx, 0, 0, 0, false, 0, lp);
        CTxMemPool::setEntries setAncestors;
        size_t nLimitAncestors = gArgs.GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT);
        size_t nLimitAncestorSize = gArgs.GetArg("-limitancestorsize", DEFAULT_ANCESTOR_SIZE_LIMIT)*1000;
        size_t nLimitDescendants = gArgs.GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT);
        size_t nLimitDescendantSize = gArgs.GetArg("-limitdescendantsize", DEFAULT_DESCENDANT_SIZE_LIMIT)*1000;
        std::string errString;
        LOCK(::mempool.cs);
        if (!::mempool.CalculateMemPoolAncestors(entry, setAncestors, nLimitAncestors, nLimitAncestorSize, nLimitDescendants, nLimitDescendantSize, errString)) {
            strFailReason = _("Transaction has too long of a mempool chain");
            return false;
        }
    }

    WalletLogPrintf("Fee Calculation: Fee:%d Bytes:%u Needed:%d Tgt:%d (requested %d) Reason:\"%s\" Decay %.5f: Estimation: (%g - %g) %.2f%% %.1f/(%.1f %d mem %.1f out) Fail: (%g - %g) %.2f%% %.1f/(%.1f %d mem %.1f out)\n",
              nFeeRet, nBytes, nFeeNeeded, feeCalc.returnedTarget, feeCalc.desiredTarget, StringForFeeReason(feeCalc.reason), feeCalc.est.decay,
              feeCalc.est.pass.start, feeCalc.est.pass.end,
              100 * feeCalc.est.pass.withinTarget / (feeCalc.est.pass.totalConfirmed + feeCalc.est.pass.inMempool + feeCalc.est.pass.leftMempool),
              feeCalc.est.pass.withinTarget, feeCalc.est.pass.totalConfirmed, feeCalc.est.pass.inMempool, feeCalc.est.pass.leftMempool,
              feeCalc.est.fail.start, feeCalc.est.fail.end,
              100 * feeCalc.est.fail.withinTarget / (feeCalc.est.fail.totalConfirmed + feeCalc.est.fail.inMempool + feeCalc.est.fail.leftMempool),
              feeCalc.est.fail.withinTarget, feeCalc.est.fail.totalConfirmed, feeCalc.est.fail.inMempool, feeCalc.est.fail.leftMempool);
    return true;
}

/*
 *在CreateTransaction之后调用，除非要中止
 **/

bool CWallet::CommitTransaction(CTransactionRef tx, mapValue_t mapValue, std::vector<std::pair<std::string, std::string>> orderForm, CReserveKey& reservekey, CConnman* connman, CValidationState& state)
{
    {
        auto locked_chain = chain().lock();
        LOCK(cs_wallet);

        CWalletTx wtxNew(this, std::move(tx));
        wtxNew.mapValue = std::move(mapValue);
        wtxNew.vOrderForm = std::move(orderForm);
        wtxNew.fTimeReceivedIsTxTime = true;
        wtxNew.fFromMe = true;

        /*letlogprintf（“commitTransaction:\n%s”，wtxnew.tx->toString（））；/*继续*/
        {
            //从密钥池中获取密钥对，以便不再使用它
            reservekey.keepkey（）；

            //将tx添加到wallet中，因为如果它有变化，它也是我们的，
            //否则仅用于事务历史记录。
            添加到所有（wtxnew）；

            //通知旧硬币已用完
            用于（const ctxin&txin:wtxnew.tx->vin）
            {
                cwallettx&coin=mapwallet.at（txin.prevout.hash）；
                硬币。宾德钱包（这个）；
                notifyTransactionChanged（this，coin.gethash（），ct_updated）；
            }
        }

        //从mapwallet获取插入的cwallettx，以便
        //已正确缓存finmempool标志
        cwallettx&wtx=mapwallet.at（wtxnew.gethash（））；

        如果（FBRoadcastTransactions）
        {
            /广播
            如果（！）wtx.acceptToMoryPool（*locked_chain，maxtxfee，state））
                walletlogprintf（“commitTransaction（）：无法立即广播事务，%s\n”，formatStateMessage（state））；
                //TODO:如果我们希望失败是长期的或永久的，请从钱包中删除wtx并返回失败。
            }否则{
                Wtx.RelaywalletTransaction（*Locked_Chain，Connman）；
            }
        }
    }
    回归真实；
}

dberrors cwallet:：loadwallet（bool&ffirstrunlet）
{
    自动锁定的_chain=chain（）.lock（）；
    锁（CS-U钱包）；

    ffirstrurret=假；
    dberrors nloadwalletret=walletbatch（*数据库，“cr+”）.loadwallet（this）；
    if（nloadwalletret==dberrors：：需要重写）
    {
        if（数据库->重写（“\x04pool”））
        {
            setInternalKeyPool.clear（）；
            setExternalKeyPool.clear（）；
            m_pool_key_to_index.clear（）；
            //注意：这里不能充值，因为钱包被锁了。
            //下次操作提示用户解锁钱包
            //这需要一个新的键。
        }
    }

    {
        锁（cs_keystore）；
        //如果这些都是空的，这个钱包就在第一次运行中
        ffirstruret=mapkeys.empty（）&&mapcryptedkeys.empty（）&&mapwatchkeys.empty（）&&setwatchonly.empty（）&&mapscripts.empty（）&&！iswalletflagset（钱包标记禁用专用钥匙）；
    }

    如果（nloadwalletret！=dberrors：：加载_OK）
        返回nloadwalletret；

    返回dberrors：：加载\u OK；
}

dberrors cwallet:：zapselectx（std:：vector<uint256>&vhashin，std:：vector<uint256>&vhashout）
{
    断言锁定（cs_wallet）；//mapwallet
    dberrors nzapselectxtret=walletbatch（*数据库，“cr+”）.zapselecttx（vhashin，vhashout）；
    用于（uint256 hash:vhashout）
        const auto&it=mapwallet.find（哈希）；
        wtxordered.erase（it->second.m_it_wtxordered）；
        删除（it）；
    }

    if（nzapselectxtret==dberrors：：需要重写）
    {
        if（数据库->重写（“\x04pool”））
        {
            setInternalKeyPool.clear（）；
            setExternalKeyPool.clear（）；
            m_pool_key_to_index.clear（）；
            //注意：这里不能充值，因为钱包被锁了。
            //下次操作提示用户解锁钱包
            //这需要一个新的键。
        }
    }

    如果（nzapselectxtret！=dberrors：：加载_OK）
        返回nzapselectxtret；

    MARKDIR（）；

    返回dberrors：：加载\u OK；

}

dberrors cwallet:：zapwallettx（std:：vector<cwallettx>和vwtx）
{
    dberrors nzapwallettxret=walletbatch（*数据库，“cr+”）.zapwallettx（vwtx）；
    if（nzapwallettxret==dberrors：：需要重写）
    {
        if（数据库->重写（“\x04pool”））
        {
            锁（CS-U钱包）；
            setInternalKeyPool.clear（）；
            setExternalKeyPool.clear（）；
            m_pool_key_to_index.clear（）；
            //注意：这里不能充值，因为钱包被锁了。
            //下次操作提示用户解锁钱包
            //这需要一个新的键。
        }
    }

    如果（nzapwallettxret！=dberrors：：加载_OK）
        返回nzapwallettxret；

    返回dberrors：：加载\u OK；
}


bool cwallet:：setaddressbook（const ctxdestination&address，const std:：string&strname，const std:：string&strpurpose）
{
    bool fupdated=false；
    {
        lock（cs_wallet）；//地图地址簿
        std:：map<ctxdestination，caddressbookdata>：：迭代器mi=mapaddressbook.find（address）；
        FMI = MI！=mapAddressBook.end（）；
        mapAddressBook[地址]。名称=strName；
        如果（！）strpurpose.empty（））/*仅在请求时更新用途*/

            mapAddressBook[address].purpose = strPurpose;
    }
    NotifyAddressBookChanged(this, address, strName, ::IsMine(*this, address) != ISMINE_NO,
                             strPurpose, (fUpdated ? CT_UPDATED : CT_NEW) );
    if (!strPurpose.empty() && !WalletBatch(*database).WritePurpose(EncodeDestination(address), strPurpose))
        return false;
    return WalletBatch(*database).WriteName(EncodeDestination(address), strName);
}

bool CWallet::DelAddressBook(const CTxDestination& address)
{
    {
LOCK(cs_wallet); //地图册

//删除与地址关联的destdata元组
        std::string strAddress = EncodeDestination(address);
        for (const std::pair<const std::string, std::string> &item : mapAddressBook[address].destdata)
        {
            WalletBatch(*database).EraseDestData(strAddress, item.first);
        }
        mapAddressBook.erase(address);
    }

    NotifyAddressBookChanged(this, address, "", ::IsMine(*this, address) != ISMINE_NO, "", CT_DELETED);

    WalletBatch(*database).ErasePurpose(EncodeDestination(address));
    return WalletBatch(*database).EraseName(EncodeDestination(address));
}

const std::string& CWallet::GetLabelName(const CScript& scriptPubKey) const
{
    CTxDestination address;
    if (ExtractDestination(scriptPubKey, address) && !scriptPubKey.IsUnspendable()) {
        auto mi = mapAddressBook.find(address);
        if (mi != mapAddressBook.end()) {
            return mi->second.name;
        }
    }
//在通讯簿中没有条目的ScriptPubKey是
//与默认标签（“”）关联。
    const static std::string DEFAULT_LABEL_NAME;
    return DEFAULT_LABEL_NAME;
}

/*
 *将旧的密钥池密钥标记为已使用，
 *并生成所有新密钥
 **/

bool CWallet::NewKeyPool()
{
    if (IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
        return false;
    }
    {
        LOCK(cs_wallet);
        WalletBatch batch(*database);

        for (const int64_t nIndex : setInternalKeyPool) {
            batch.ErasePool(nIndex);
        }
        setInternalKeyPool.clear();

        for (const int64_t nIndex : setExternalKeyPool) {
            batch.ErasePool(nIndex);
        }
        setExternalKeyPool.clear();

        for (const int64_t nIndex : set_pre_split_keypool) {
            batch.ErasePool(nIndex);
        }
        set_pre_split_keypool.clear();

        m_pool_key_to_index.clear();

        if (!TopUpKeyPool()) {
            return false;
        }
        WalletLogPrintf("CWallet::NewKeyPool rewrote keypool\n");
    }
    return true;
}

size_t CWallet::KeypoolCountExternalKeys()
{
AssertLockHeld(cs_wallet); //设置外部密钥池
    return setExternalKeyPool.size() + set_pre_split_keypool.size();
}

void CWallet::LoadKeyPool(int64_t nIndex, const CKeyPool &keypool)
{
    AssertLockHeld(cs_wallet);
    if (keypool.m_pre_split) {
        set_pre_split_keypool.insert(nIndex);
    } else if (keypool.fInternal) {
        setInternalKeyPool.insert(nIndex);
    } else {
        setExternalKeyPool.insert(nIndex);
    }
    m_max_keypool_index = std::max(m_max_keypool_index, nIndex);
    m_pool_key_to_index[keypool.vchPubKey.GetID()] = nIndex;

//如果还不存在元数据，请使用池键的
//创建时间。请注意，这可能会被实际覆盖
//稍后为该键存储元数据，这很好。
    CKeyID keyid = keypool.vchPubKey.GetID();
    if (mapKeyMetadata.count(keyid) == 0)
        mapKeyMetadata[keyid] = CKeyMetadata(keypool.nTime);
}

bool CWallet::TopUpKeyPool(unsigned int kpSize)
{
    if (IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
        return false;
    }
    {
        LOCK(cs_wallet);

        if (IsLocked())
            return false;

//自顶向下的密钥池
        unsigned int nTargetSize;
        if (kpSize > 0)
            nTargetSize = kpSize;
        else
            nTargetSize = std::max(gArgs.GetArg("-keypool", DEFAULT_KEYPOOL_SIZE), (int64_t) 0);

//计数可用密钥的数量（内部、外部）
//确保外部和内部密钥的密钥池适合用户选择的目标（-keypool）
        int64_t missingExternal = std::max(std::max((int64_t) nTargetSize, (int64_t) 1) - (int64_t)setExternalKeyPool.size(), (int64_t) 0);
        int64_t missingInternal = std::max(std::max((int64_t) nTargetSize, (int64_t) 1) - (int64_t)setInternalKeyPool.size(), (int64_t) 0);

        if (!IsHDEnabled() || !CanSupportFeature(FEATURE_HD_SPLIT))
        {
//不创建额外的内部键
            missingInternal = 0;
        }
        bool internal = false;
        WalletBatch batch(*database);
        for (int64_t i = missingInternal + missingExternal; i--;)
        {
            if (i < missingInternal) {
                internal = true;
            }

assert(m_max_keypool_index < std::numeric_limits<int64_t>::max()); //你到底是怎么用这么多钥匙的？
            int64_t index = ++m_max_keypool_index;

            CPubKey pubkey(GenerateNewKey(batch, internal));
            if (!batch.WritePool(index, CKeyPool(pubkey, internal))) {
                throw std::runtime_error(std::string(__func__) + ": writing generated key failed");
            }

            if (internal) {
                setInternalKeyPool.insert(index);
            } else {
                setExternalKeyPool.insert(index);
            }
            m_pool_key_to_index[pubkey.GetID()] = index;
        }
        if (missingInternal + missingExternal > 0) {
            WalletLogPrintf("keypool added %d keys (%d internal), size=%u (%u internal)\n", missingInternal + missingExternal, missingInternal, setInternalKeyPool.size() + setExternalKeyPool.size() + set_pre_split_keypool.size(), setInternalKeyPool.size());
        }
    }
    return true;
}

bool CWallet::ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool, bool fRequestedInternal)
{
    nIndex = -1;
    keypool.vchPubKey = CPubKey();
    {
        LOCK(cs_wallet);

        if (!IsLocked())
            TopUpKeyPool();

        bool fReturningInternal = IsHDEnabled() && CanSupportFeature(FEATURE_HD_SPLIT) && fRequestedInternal;
        bool use_split_keypool = set_pre_split_keypool.empty();
        std::set<int64_t>& setKeyPool = use_split_keypool ? (fReturningInternal ? setInternalKeyPool : setExternalKeyPool) : set_pre_split_keypool;

//拿到最旧的钥匙
        if (setKeyPool.empty()) {
            return false;
        }

        WalletBatch batch(*database);

        auto it = setKeyPool.begin();
        nIndex = *it;
        setKeyPool.erase(it);
        if (!batch.ReadPool(nIndex, keypool)) {
            throw std::runtime_error(std::string(__func__) + ": read failed");
        }
        if (!HaveKey(keypool.vchPubKey.GetID())) {
            throw std::runtime_error(std::string(__func__) + ": unknown key in key pool");
        }
//如果密钥是预拆分的密钥池，我们不关心它是什么类型
        if (use_split_keypool && keypool.fInternal != fReturningInternal) {
            throw std::runtime_error(std::string(__func__) + ": keypool entry misclassified");
        }
        if (!keypool.vchPubKey.IsValid()) {
            throw std::runtime_error(std::string(__func__) + ": keypool entry invalid");
        }

        m_pool_key_to_index.erase(keypool.vchPubKey.GetID());
        WalletLogPrintf("keypool reserve %d\n", nIndex);
    }
    return true;
}

void CWallet::KeepKey(int64_t nIndex)
{
//从密钥池中删除
    WalletBatch batch(*database);
    batch.ErasePool(nIndex);
    WalletLogPrintf("keypool keep %d\n", nIndex);
}

void CWallet::ReturnKey(int64_t nIndex, bool fInternal, const CPubKey& pubkey)
{
//返回密钥池
    {
        LOCK(cs_wallet);
        if (fInternal) {
            setInternalKeyPool.insert(nIndex);
        } else if (!set_pre_split_keypool.empty()) {
            set_pre_split_keypool.insert(nIndex);
        } else {
            setExternalKeyPool.insert(nIndex);
        }
        m_pool_key_to_index[pubkey.GetID()] = nIndex;
    }
    WalletLogPrintf("keypool return %d\n", nIndex);
}

bool CWallet::GetKeyFromPool(CPubKey& result, bool internal)
{
    if (IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
        return false;
    }

    CKeyPool keypool;
    {
        LOCK(cs_wallet);
        int64_t nIndex;
        if (!ReserveKeyFromKeyPool(nIndex, keypool, internal)) {
            if (IsLocked()) return false;
            WalletBatch batch(*database);
            result = GenerateNewKey(batch, internal);
            return true;
        }
        KeepKey(nIndex);
        result = keypool.vchPubKey;
    }
    return true;
}

static int64_t GetOldestKeyTimeInPool(const std::set<int64_t>& setKeyPool, WalletBatch& batch) {
    if (setKeyPool.empty()) {
        return GetTime();
    }

    CKeyPool keypool;
    int64_t nIndex = *(setKeyPool.begin());
    if (!batch.ReadPool(nIndex, keypool)) {
        throw std::runtime_error(std::string(__func__) + ": read oldest key in keypool failed");
    }
    assert(keypool.vchPubKey.IsValid());
    return keypool.nTime;
}

int64_t CWallet::GetOldestKeyPoolTime()
{
    LOCK(cs_wallet);

    WalletBatch batch(*database);

//从密钥池加载最旧的密钥，获取时间并返回
    int64_t oldestKey = GetOldestKeyTimeInPool(setExternalKeyPool, batch);
    if (IsHDEnabled() && CanSupportFeature(FEATURE_HD_SPLIT)) {
        oldestKey = std::max(GetOldestKeyTimeInPool(setInternalKeyPool, batch), oldestKey);
        if (!set_pre_split_keypool.empty()) {
            oldestKey = std::max(GetOldestKeyTimeInPool(set_pre_split_keypool, batch), oldestKey);
        }
    }

    return oldestKey;
}

std::map<CTxDestination, CAmount> CWallet::GetAddressBalances(interfaces::Chain::Lock& locked_chain)
{
    std::map<CTxDestination, CAmount> balances;

    {
        LOCK(cs_wallet);
        for (const auto& walletEntry : mapWallet)
        {
            const CWalletTx *pcoin = &walletEntry.second;

            if (!pcoin->IsTrusted(locked_chain))
                continue;

            if (pcoin->IsImmatureCoinBase(locked_chain))
                continue;

            int nDepth = pcoin->GetDepthInMainChain(locked_chain);
            if (nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < pcoin->tx->vout.size(); i++)
            {
                CTxDestination addr;
                if (!IsMine(pcoin->tx->vout[i]))
                    continue;
                if(!ExtractDestination(pcoin->tx->vout[i].scriptPubKey, addr))
                    continue;

                CAmount n = IsSpent(locked_chain, walletEntry.first, i) ? 0 : pcoin->tx->vout[i].nValue;

                if (!balances.count(addr))
                    balances[addr] = 0;
                balances[addr] += n;
            }
        }
    }

    return balances;
}

std::set< std::set<CTxDestination> > CWallet::GetAddressGroupings()
{
AssertLockHeld(cs_wallet); //地图钱包
    std::set< std::set<CTxDestination> > groupings;
    std::set<CTxDestination> grouping;

    for (const auto& walletEntry : mapWallet)
    {
        const CWalletTx *pcoin = &walletEntry.second;

        if (pcoin->tx->vin.size() > 0)
        {
            bool any_mine = false;
//将所有输入地址分组
            for (const CTxIn& txin : pcoin->tx->vin)
            {
                CTxDestination address;
                /*！ismine（txin））/*如果此输入不是我的，则忽略它*/
                    继续；
                如果（！）提取目标（mapwallet.at（txin.prevout.hash）.tx->vout[txin.prevout.n].scriptpubkey，address））
                    继续；
                分组。插入（地址）；
                任何_mine=真；
            }

            //使用输入地址分组更改
            如果（安尼矿）
            {
               for（const ctxout&txout:pcoin->tx->vout）
                   如果（ischange（txout））。
                   {
                       CTX目标txoutaddr；
                       如果（！）提取目标（txout.scriptpubkey，txoutaddr）
                           继续；
                       grouping.insert（txoutaddr）；
                   }
            }
            if（grouping.size（）>0）
            {
                groupings.insert（分组）；
                grouping.clear（）；
            }
        }

        //单独分组加法器
        for（const auto&txout:pcoin->tx->vout）
            如果（ismine（txout））。
            {
                CTX目标地址；
                如果（！）提取目标（txout.scriptpubkey，地址）
                    继续；
                分组。插入（地址）；
                groupings.insert（分组）；
                grouping.clear（）；
            }
    }

    std:：set<std:：set<ctxdestination>>uniquegroupings；//一组指向地址组的指针
    std:：map<ctxdestination，std:：set<ctxdestination>>set map；//将地址映射到包含该地址的唯一组
    for（std:：set<ctxdestination>_grouping:groupings）
    {
        //创建一组新组命中的所有组
        std:：set<std:：set<ctxdestination>>点击次数；
        std:：map<ctxdestination，std:：set<ctxdestination>>：迭代器it；
        for（const ctxdestination&address:_grouping）
            如果（（它=setmap.find（address））！= SETMAP.EN（）
                点击.插入（（*it）.second）；

        //将所有命中组合并为新的单个组并删除旧组
        std:：set<ctxdestination>*merged=new std:：set<ctxdestination>（_grouping）；
        对于（std:：set<ctxdestination>>*hit:hits）
        {
            合并->插入（hit->begin（），hit->end（））；
            唯一分组。擦除（命中）；
            删除命中；
        }
        uniquegroupings.insert（合并）；

        //更新setmap
        for（const ctxdestination&element:*合并）
            setmap[element]=合并；
    }

    std:：set<std:：set<ctxdestination>>ret；
    for（const std:：set<ctxdestination>>*uniquegrouping:uniquegroupings）
    {
        ret.insert（*uniquegrouping）；
        删除唯一分组；
    }

    返回RET；
}

std:：set<ctxdestination>cwallet:：getLabelAddresses（const std:：string&label）const
{
    锁（CS-U钱包）；
    std:：set<ctxdestination>result；
    for（const std:：pair<const ctxdestination，caddressbookdata>&item:mapAddressBook）
    {
        const ctxdestination&address=item.first；
        const std:：string&strname=item.second.name；
        if（strname==label）
            结果。插入（地址）；
    }
    返回结果；
}

bool creservekey:：getreservedkey（cpubkey&pubkey，bool internal）
{
    如果（nindex=-1）
    {
        ckeyPool密钥池；
        如果（！）pWallet->ReserveKeyFromKeyPool（nindex，keypool，internal））
            返回错误；
        }
        vchSubkey=keypool.vchSubkey；
        finternal=密钥池.finternal；
    }
    断言（vchpubkey.isvalid（））；
    pubkey=vchSubkey；
    回归真实；
}

void creservekey:：keepkey（）。
{
    如果（NDeCK）！= -1）
        pwallet->keepkey（nindex）；
    nCe=＝1；
    vchSubkey=cpubkey（）；
}

无效的creservekey:：returnkey（）。
{
    如果（NDeCK）！= -1）{
        pwallet->returnkey（nindex、finternal、vchsubkey）；
    }
    nCe=＝1；
    vchSubkey=cpubkey（）；
}

void cwallet:：markreservekeysasused（int64_t keyspool_id）
{
    断言锁定（CS-U钱包）；
    bool internal=setinternalkeypool.count（keypool_id）；
    如果（！）内部）断言（setexternalkeypool.count（keypool_id）设置_pre_split_keypool.count（keypool_id））；
    std：：set<int64_t>*setkeypool=internal？&setInternalKeyPool:（set_pre_split_keypool.empty（）？&setexternalkeypool:&set_pre_split_keypool）；
    auto it=setkeypool->begin（）；

    walletbatch批次（*database）；
    尽管如此！=std:：end（*setkeypool））
        const int64_t&index=*（it）；
        if（index>keypool_id）break；//set*keypool已排序

        ckeyPool密钥池；
        if（batch.readpool（index，keypool））//todo:这应该是不必要的
            m_pool_key_to_index.erase（key pool.vchpubkey.getid（））；
        }
        学习所有相关脚本（keypool.vchSubkey）；
        批.erasepol（index）；
        walletlogprintf（“keypool index%d removed\n”，index）；
        它=setkeypool->erase（它）；
    }
}

void cwallet:：getscriptforming（std:：shared_ptr<creservescript>&script）
{
    std:：shared_ptr<creservekey>rkey=std:：make_shared<creservekey>（this）；
    CPubKey pubkey；
    如果（！）Rkey->GetReservedkey（pubkey）
        返回；

    脚本= rKEY；
    script->reservescript=cscript（）<<tobytevector（pubkey）<<op_checksig；
}

void cwallet:：lockcoin（const coutpoint&output）
{
    assertLockHolded（cs_wallet）；//setLockedCoins
    setLockedCoins.insert（输出）；
}

void cwallet:：unlockcoin（const coutpoint&output）
{
    assertLockHolded（cs_wallet）；//setLockedCoins
    setLockedCoins.Erase（输出）；
}

void cwallet：：解锁所有硬币（）。
{
    assertLockHolded（cs_wallet）；//setLockedCoins
    setLockedCoins.clear（）；
}

bool cwallet:：isLockedCoin（uint256哈希，无符号int n）常量
{
    assertLockHolded（cs_wallet）；//setLockedCoins
    coutpoint输出（hash，n）；

    返回（setlockedcoins.count（outpt）>0）；
}

void cwallet:：listlockedcoins（std:：vector<coutpoint>和voutpts）常量
{
    assertLockHolded（cs_wallet）；//setLockedCoins
    for（std:：set<coutpoint>：：迭代器it=setLockedCoins.begin（）；
         它！=setLockedCoins.end（）；it++）
        coutpoint输出=（*it）；
        回推（输出）；
    }
}

/**}*/ // end of Actions


void CWallet::GetKeyBirthTimes(interfaces::Chain::Lock& locked_chain, std::map<CTxDestination, int64_t> &mapKeyBirth) const {
AssertLockHeld(cs_wallet); //MAPKEY元数据
    mapKeyBirth.clear();

//获取具有元数据的密钥的出生时间
    for (const auto& entry : mapKeyMetadata) {
        if (entry.second.nCreateTime) {
            mapKeyBirth[entry.first] = entry.second.nCreateTime;
        }
    }

//我们将在其中推断其他关键点的高度的地图
CBlockIndex *pindexMax = chainActive[std::max(0, chainActive.Height() - 144)]; //尖端可以重新组织；使用144块的安全裕度
    std::map<CKeyID, CBlockIndex*> mapKeyFirstBlock;
    for (const CKeyID &keyid : GetKeys()) {
        if (mapKeyBirth.count(keyid) == 0)
            mapKeyFirstBlock[keyid] = pindexMax;
    }

//如果没有这样的钥匙，我们就完了
    if (mapKeyFirstBlock.empty())
        return;

//找到影响这些键的第一个块，如果还有
    for (const auto& entry : mapWallet) {
//重复所有钱包交易…
        const CWalletTx &wtx = entry.second;
        CBlockIndex* pindex = LookupBlockIndex(wtx.hashBlock);
        if (pindex && chainActive.Contains(pindex)) {
//…已经在一个街区里了
            int nHeight = pindex->nHeight;
            for (const CTxOut &txout : wtx.tx->vout) {
//迭代所有输出
                for (const auto &keyid : GetAffectedKeys(txout.scriptPubKey, *this)) {
//…以及所有受影响的钥匙
                    std::map<CKeyID, CBlockIndex*>::iterator rit = mapKeyFirstBlock.find(keyid);
                    if (rit != mapKeyFirstBlock.end() && nHeight < rit->second->nHeight)
                        rit->second = pindex;
                }
            }
        }
    }

//提取这些键的块时间戳
    for (const auto& entry : mapKeyFirstBlock)
mapKeyBirth[entry.first] = entry.second->GetBlockTime() - TIMESTAMP_WINDOW; //封锁时间可以是2小时
}

/*
 *计算添加到钱包的交易的智能时间戳。
 *
 *逻辑：
 *-如果发送事务，则将其时间戳分配给当前时间。
 *-如果接收到块外的事务，则将其时间戳分配给
 *当前时间。
 *-如果接收到具有未来时间戳的块，则分配其所有（尚未分配
 *已知）事务的时间戳到当前时间。
 *-如果接收到具有过去时间戳的块，则在最近已知的
 *事务（我们关心的事务），分配所有事务（还不知道）
 *事务的时间戳与最近已知的时间戳相同
 *交易。
 *-如果接收的块具有过去的时间戳，但在最近已知的
 *事务，将其所有（未知）事务的时间戳分配给
 *阻塞时间。
 *
 *有关详细信息，请参阅cwallettx:：ntimesmart，
 *https://bitcointalk.org/？主题＝54527，或
 *https://github.com/bitcoin/bitcoin/pull/1393。
 **/

unsigned int CWallet::ComputeTimeSmart(const CWalletTx& wtx) const
{
    unsigned int nTimeSmart = wtx.nTimeReceived;
    if (!wtx.hashUnset()) {
        if (const CBlockIndex* pindex = LookupBlockIndex(wtx.hashBlock)) {
            int64_t latestNow = wtx.nTimeReceived;
            int64_t latestEntry = 0;

//在未来不超过5分钟的时间内，容忍钱包中最后一个时间戳的次数
            int64_t latestTolerated = latestNow + 300;
            const TxItems& txOrdered = wtxOrdered;
            for (auto it = txOrdered.rbegin(); it != txOrdered.rend(); ++it) {
                CWalletTx* const pwtx = it->second;
                if (pwtx == &wtx) {
                    continue;
                }
                int64_t nSmartTime;
                nSmartTime = pwtx->nTimeSmart;
                if (!nSmartTime) {
                    nSmartTime = pwtx->nTimeReceived;
                }
                if (nSmartTime <= latestTolerated) {
                    latestEntry = nSmartTime;
                    if (nSmartTime > latestNow) {
                        latestNow = nSmartTime;
                    }
                    break;
                }
            }

            int64_t blocktime = pindex->GetBlockTime();
            nTimeSmart = std::max(latestEntry, std::min(blocktime, latestNow));
        } else {
            WalletLogPrintf("%s: found %s in block %s not in index\n", __func__, wtx.GetHash().ToString(), wtx.hashBlock.ToString());
        }
    }
    return nTimeSmart;
}

bool CWallet::AddDestData(const CTxDestination &dest, const std::string &key, const std::string &value)
{
    if (boost::get<CNoDestination>(&dest))
        return false;

    mapAddressBook[dest].destdata.insert(std::make_pair(key, value));
    return WalletBatch(*database).WriteDestData(EncodeDestination(dest), key, value);
}

bool CWallet::EraseDestData(const CTxDestination &dest, const std::string &key)
{
    if (!mapAddressBook[dest].destdata.erase(key))
        return false;
    return WalletBatch(*database).EraseDestData(EncodeDestination(dest), key);
}

void CWallet::LoadDestData(const CTxDestination &dest, const std::string &key, const std::string &value)
{
    mapAddressBook[dest].destdata.insert(std::make_pair(key, value));
}

bool CWallet::GetDestData(const CTxDestination &dest, const std::string &key, std::string *value) const
{
    std::map<CTxDestination, CAddressBookData>::const_iterator i = mapAddressBook.find(dest);
    if(i != mapAddressBook.end())
    {
        CAddressBookData::StringMap::const_iterator j = i->second.destdata.find(key);
        if(j != i->second.destdata.end())
        {
            if(value)
                *value = j->second;
            return true;
        }
    }
    return false;
}

std::vector<std::string> CWallet::GetDestValues(const std::string& prefix) const
{
    LOCK(cs_wallet);
    std::vector<std::string> values;
    for (const auto& address : mapAddressBook) {
        for (const auto& data : address.second.destdata) {
            if (!data.first.compare(0, prefix.size(), prefix)) {
                values.emplace_back(data.second);
            }
        }
    }
    return values;
}

void CWallet::MarkPreSplitKeys()
{
    WalletBatch batch(*database);
    for (auto it = setExternalKeyPool.begin(); it != setExternalKeyPool.end();) {
        int64_t index = *it;
        CKeyPool keypool;
        if (!batch.ReadPool(index, keypool)) {
            throw std::runtime_error(std::string(__func__) + ": read keypool entry failed");
        }
        keypool.m_pre_split = true;
        if (!batch.WritePool(index, keypool)) {
            throw std::runtime_error(std::string(__func__) + ": writing modified keypool entry failed");
        }
        set_pre_split_keypool.insert(index);
        it = setExternalKeyPool.erase(it);
    }
}

bool CWallet::Verify(interfaces::Chain& chain, const WalletLocation& location, bool salvage_wallet, std::string& error_string, std::string& warning_string)
{
//在钱包路径上做一些检查。它应该是：
//
//1。可以创建目录的路径。
//2。现有目录的路径。
//三。指向目录的符号链接的路径。
//4。为了向后兼容，-walletdir中的数据文件名。
    LOCK(cs_wallets);
    const fs::path& wallet_path = location.GetPath();
    fs::file_type path_type = fs::symlink_status(wallet_path).type();
    if (!(path_type == fs::file_not_found || path_type == fs::directory_file ||
          (path_type == fs::symlink_file && fs::is_directory(wallet_path)) ||
          (path_type == fs::regular_file && fs::path(location.GetName()).filename() == location.GetName()))) {
        error_string = strprintf(
              "Invalid -wallet path '%s'. -wallet path should point to a directory where wallet.dat and "
              "database/log.?????????? files can be stored, a location where such a directory could be created, "
              "or (for backwards compatibility) the name of an existing data file in -walletdir (%s)",
              location.GetName(), GetWalletDir());
        return false;
    }

//确保钱包路径不与现有钱包路径冲突
    if (IsWalletLoaded(wallet_path)) {
        error_string = strprintf("Error loading wallet %s. Duplicate -wallet filename specified.", location.GetName());
        return false;
    }

    try {
        if (!WalletBatch::VerifyEnvironment(wallet_path, error_string)) {
            return false;
        }
    } catch (const fs::filesystem_error& e) {
        error_string = strprintf("Error loading wallet %s. %s", location.GetName(), fsbridge::get_filesystem_error_message(e));
        return false;
    }

    if (salvage_wallet) {
//恢复可读的密钥对：
        CWallet dummyWallet(chain, WalletLocation(), WalletDatabase::CreateDummy());
        std::string backup_filename;
        if (!WalletBatch::Recover(wallet_path, (void *)&dummyWallet, WalletBatch::RecoverKeysOnlyFilter, backup_filename)) {
            return false;
        }
    }

    return WalletBatch::VerifyDatabaseFile(wallet_path, warning_string, error_string);
}

std::shared_ptr<CWallet> CWallet::CreateWalletFromFile(interfaces::Chain& chain, const WalletLocation& location, uint64_t wallet_creation_flags)
{
    const std::string& walletFile = location.GetName();

//需要在-zapwallettxes之后恢复钱包交易元数据
    std::vector<CWalletTx> vWtx;

    if (gArgs.GetBoolArg("-zapwallettxes", false)) {
        uiInterface.InitMessage(_("Zapping all transactions from wallet..."));

        std::unique_ptr<CWallet> tempWallet = MakeUnique<CWallet>(chain, location, WalletDatabase::Create(location.GetPath()));
        DBErrors nZapWalletRet = tempWallet->ZapWalletTx(vWtx);
        if (nZapWalletRet != DBErrors::LOAD_OK) {
            InitError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
            return nullptr;
        }
    }

    uiInterface.InitMessage(_("Loading wallet..."));

    int64_t nStart = GetTimeMillis();
    bool fFirstRun = true;
//TODO:无法使用std:：make_shared，因为我们需要自定义的删除程序，但是
//应该可以使用std：：allocate_shared。
    std::shared_ptr<CWallet> walletInstance(new CWallet(chain, location, WalletDatabase::Create(location.GetPath())), ReleaseWallet);
    DBErrors nLoadWalletRet = walletInstance->LoadWallet(fFirstRun);
    if (nLoadWalletRet != DBErrors::LOAD_OK)
    {
        if (nLoadWalletRet == DBErrors::CORRUPT) {
            InitError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
            return nullptr;
        }
        else if (nLoadWalletRet == DBErrors::NONCRITICAL_ERROR)
        {
            InitWarning(strprintf(_("Error reading %s! All keys read correctly, but transaction data"
                                         " or address book entries might be missing or incorrect."),
                walletFile));
        }
        else if (nLoadWalletRet == DBErrors::TOO_NEW) {
            InitError(strprintf(_("Error loading %s: Wallet requires newer version of %s"), walletFile, _(PACKAGE_NAME)));
            return nullptr;
        }
        else if (nLoadWalletRet == DBErrors::NEED_REWRITE)
        {
            InitError(strprintf(_("Wallet needed to be rewritten: restart %s to complete"), _(PACKAGE_NAME)));
            return nullptr;
        }
        else {
            InitError(strprintf(_("Error loading %s"), walletFile));
            return nullptr;
        }
    }

    int prev_version = walletInstance->nWalletVersion;
    if (gArgs.GetBoolArg("-upgradewallet", fFirstRun))
    {
        int nMaxVersion = gArgs.GetArg("-upgradewallet", 0);
if (nMaxVersion == 0) //升级版钱包，无理由
        {
            walletInstance->WalletLogPrintf("Performing wallet upgrade to %i\n", FEATURE_LATEST);
            nMaxVersion = FEATURE_LATEST;
walletInstance->SetMinVersion(FEATURE_LATEST); //立即永久升级钱包
        }
        else
            walletInstance->WalletLogPrintf("Allowing wallet upgrade up to %i\n", nMaxVersion);
        if (nMaxVersion < walletInstance->GetVersion())
        {
            InitError(_("Cannot downgrade wallet"));
            return nullptr;
        }
        walletInstance->SetMaxVersion(nMaxVersion);
    }

//如果显式升级，则升级到hd
    if (gArgs.GetBoolArg("-upgradewallet", false)) {
        LOCK(walletInstance->cs_wallet);

//不要将版本升级到hd-split和feature-pre-split-keypool之间的任何版本，除非已经支持hd-split
        int max_version = walletInstance->nWalletVersion;
        if (!walletInstance->CanSupportFeature(FEATURE_HD_SPLIT) && max_version >=FEATURE_HD_SPLIT && max_version < FEATURE_PRE_SPLIT_KEYPOOL) {
            InitError(_("Cannot upgrade a non HD split wallet without upgrading to support pre split keypool. Please use -upgradewallet=169900 or -upgradewallet with no version specified."));
            return nullptr;
        }

        bool hd_upgrade = false;
        bool split_upgrade = false;
        if (walletInstance->CanSupportFeature(FEATURE_HD) && !walletInstance->IsHDEnabled()) {
            walletInstance->WalletLogPrintf("Upgrading wallet to HD\n");
            walletInstance->SetMinVersion(FEATURE_HD);

//生成新的主密钥
            CPubKey masterPubKey = walletInstance->GenerateNewSeed();
            walletInstance->SetHDSeed(masterPubKey);
            hd_upgrade = true;
        }
//如有必要，升级到HD链拆分
        if (walletInstance->CanSupportFeature(FEATURE_HD_SPLIT)) {
            walletInstance->WalletLogPrintf("Upgrading wallet to use HD chain split\n");
            walletInstance->SetMinVersion(FEATURE_PRE_SPLIT_KEYPOOL);
            split_upgrade = FEATURE_HD_SPLIT > prev_version;
        }
//将密钥池中当前所有密钥标记为预拆分
        if (split_upgrade) {
            walletInstance->MarkPreSplitKeys();
        }
//如果升级到HD，则重新生成密钥池
        if (hd_upgrade) {
            if (!walletInstance->TopUpKeyPool()) {
                InitError(_("Unable to generate keys"));
                return nullptr;
            }
        }
    }

    if (fFirstRun)
    {
//确保此wallet.dat只能由支持带链拆分的HD的客户打开，不需要默认密钥。
        walletInstance->SetMinVersion(FEATURE_LATEST);

        if ((wallet_creation_flags & WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
//选择性允许设置标志
            walletInstance->SetWalletFlag(WALLET_FLAG_DISABLE_PRIVATE_KEYS);
        } else {
//生成新种子
            CPubKey seed = walletInstance->GenerateNewSeed();
            walletInstance->SetHDSeed(seed);
        }

//加满钥匙池
        if (!walletInstance->IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS) && !walletInstance->TopUpKeyPool()) {
            InitError(_("Unable to generate initial keys"));
            return nullptr;
        }

auto locked_chain = chain.assumeLocked();  //暂时的。在即将进行的锁清理中删除
        walletInstance->ChainStateFlushed(chainActive.GetLocator());
    } else if (wallet_creation_flags & WALLET_FLAG_DISABLE_PRIVATE_KEYS) {
//使创建后无法禁用私钥
        InitError(strprintf(_("Error loading %s: Private keys can only be disabled during creation"), walletFile));
        return NULL;
    } else if (walletInstance->IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
        LOCK(walletInstance->cs_KeyStore);
        if (!walletInstance->mapKeys.empty() || !walletInstance->mapCryptedKeys.empty()) {
            InitWarning(strprintf(_("Warning: Private keys detected in wallet {%s} with disabled private keys"), walletFile));
        }
    }

    if (!gArgs.GetArg("-addresstype", "").empty() && !ParseOutputType(gArgs.GetArg("-addresstype", ""), walletInstance->m_default_address_type)) {
        InitError(strprintf("Unknown address type '%s'", gArgs.GetArg("-addresstype", "")));
        return nullptr;
    }

    if (!gArgs.GetArg("-changetype", "").empty() && !ParseOutputType(gArgs.GetArg("-changetype", ""), walletInstance->m_default_change_type)) {
        InitError(strprintf("Unknown change type '%s'", gArgs.GetArg("-changetype", "")));
        return nullptr;
    }

    if (gArgs.IsArgSet("-mintxfee")) {
        CAmount n = 0;
        if (!ParseMoney(gArgs.GetArg("-mintxfee", ""), n) || 0 == n) {
            InitError(AmountErrMsg("mintxfee", gArgs.GetArg("-mintxfee", "")));
            return nullptr;
        }
        if (n > HIGH_TX_FEE_PER_KB) {
            InitWarning(AmountHighWarn("-mintxfee") + " " +
                        _("This is the minimum transaction fee you pay on every transaction."));
        }
        walletInstance->m_min_fee = CFeeRate(n);
    }

    walletInstance->m_allow_fallback_fee = Params().IsFallbackFeeEnabled();
    if (gArgs.IsArgSet("-fallbackfee")) {
        CAmount nFeePerK = 0;
        if (!ParseMoney(gArgs.GetArg("-fallbackfee", ""), nFeePerK)) {
            InitError(strprintf(_("Invalid amount for -fallbackfee=<amount>: '%s'"), gArgs.GetArg("-fallbackfee", "")));
            return nullptr;
        }
        if (nFeePerK > HIGH_TX_FEE_PER_KB) {
            InitWarning(AmountHighWarn("-fallbackfee") + " " +
                        _("This is the transaction fee you may pay when fee estimates are not available."));
        }
        walletInstance->m_fallback_fee = CFeeRate(nFeePerK);
walletInstance->m_allow_fallback_fee = nFeePerK != 0; //如果值设置为0，则禁用回退费用；如果值非空，则启用回退费用。
    }
    if (gArgs.IsArgSet("-discardfee")) {
        CAmount nFeePerK = 0;
        if (!ParseMoney(gArgs.GetArg("-discardfee", ""), nFeePerK)) {
            InitError(strprintf(_("Invalid amount for -discardfee=<amount>: '%s'"), gArgs.GetArg("-discardfee", "")));
            return nullptr;
        }
        if (nFeePerK > HIGH_TX_FEE_PER_KB) {
            InitWarning(AmountHighWarn("-discardfee") + " " +
                        _("This is the transaction fee you may discard if change is smaller than dust at this level"));
        }
        walletInstance->m_discard_rate = CFeeRate(nFeePerK);
    }
    if (gArgs.IsArgSet("-paytxfee")) {
        CAmount nFeePerK = 0;
        if (!ParseMoney(gArgs.GetArg("-paytxfee", ""), nFeePerK)) {
            InitError(AmountErrMsg("paytxfee", gArgs.GetArg("-paytxfee", "")));
            return nullptr;
        }
        if (nFeePerK > HIGH_TX_FEE_PER_KB) {
            InitWarning(AmountHighWarn("-paytxfee") + " " +
                        _("This is the transaction fee you will pay if you send a transaction."));
        }
        walletInstance->m_pay_tx_fee = CFeeRate(nFeePerK, 1000);
        if (walletInstance->m_pay_tx_fee < ::minRelayTxFee) {
            InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s' (must be at least %s)"),
                gArgs.GetArg("-paytxfee", ""), ::minRelayTxFee.ToString()));
            return nullptr;
        }
    }
    walletInstance->m_confirm_target = gArgs.GetArg("-txconfirmtarget", DEFAULT_TX_CONFIRM_TARGET);
    walletInstance->m_spend_zero_conf_change = gArgs.GetBoolArg("-spendzeroconfchange", DEFAULT_SPEND_ZEROCONF_CHANGE);
    walletInstance->m_signal_rbf = gArgs.GetBoolArg("-walletrbf", DEFAULT_WALLET_RBF);

    walletInstance->WalletLogPrintf("Wallet completed loading in %15dms\n", GetTimeMillis() - nStart);

//尝试加满密钥池。如果钱包被锁上，则不允许操作。
    walletInstance->TopUpKeyPool();

LockAnnotation lock(::cs_main); //临时，用于下面的findWorkingLobalIndex。在即将到来的提交中删除。
    auto locked_chain = chain.lock();
    LOCK(walletInstance->cs_wallet);

    CBlockIndex *pindexRescan = chainActive.Genesis();
    if (!gArgs.GetBoolArg("-rescan", false))
    {
        WalletBatch batch(*walletInstance->database);
        CBlockLocator locator;
        if (batch.ReadBestBlock(locator))
            pindexRescan = FindForkInGlobalIndex(chainActive, locator);
    }

    walletInstance->m_last_block_processed = chainActive.Tip();

    if (chainActive.Tip() && chainActive.Tip() != pindexRescan)
    {
//我们无法在非修剪块之外重新扫描，停止并抛出错误
//如果用户在修剪后的节点内使用旧钱包，则可能会发生这种情况。
//或者，如果他长时间不合格，那么决定重新启用
        if (fPruneMode)
        {
            CBlockIndex *block = chainActive.Tip();
            while (block && block->pprev && (block->pprev->nStatus & BLOCK_HAVE_DATA) && block->pprev->nTx > 0 && pindexRescan != block)
                block = block->pprev;

            if (pindexRescan != block) {
                InitError(_("Prune: last wallet synchronisation goes beyond pruned data. You need to -reindex (download the whole blockchain again in case of pruned node)"));
                return nullptr;
            }
        }

        uiInterface.InitMessage(_("Rescanning..."));
        walletInstance->WalletLogPrintf("Rescanning last %i blocks (from block %i)...\n", chainActive.Height() - pindexRescan->nHeight, pindexRescan->nHeight);

//如果块是以前创建的，则无需读取和扫描块
//我们的钱包生日（根据阻塞时间变化进行调整）
        while (pindexRescan && walletInstance->nTimeFirstKey && (pindexRescan->GetBlockTime() < (walletInstance->nTimeFirstKey - TIMESTAMP_WINDOW))) {
            pindexRescan = chainActive.Next(pindexRescan);
        }

        nStart = GetTimeMillis();
        {
            WalletRescanReserver reserver(walletInstance.get());
            const CBlockIndex *stop_block, *failed_block;
            if (!reserver.reserve() || (ScanResult::SUCCESS != walletInstance->ScanForWalletTransactions(pindexRescan, nullptr, reserver, failed_block, stop_block, true))) {
                InitError(_("Failed to rescan the wallet during initialization"));
                return nullptr;
            }
        }
        walletInstance->WalletLogPrintf("Rescan completed in %15dms\n", GetTimeMillis() - nStart);
        walletInstance->ChainStateFlushed(chainActive.GetLocator());
        walletInstance->database->IncrementUpdateCounter();

//在-zapwallettxes=1之后还原钱包交易元数据
        if (gArgs.GetBoolArg("-zapwallettxes", false) && gArgs.GetArg("-zapwallettxes", "1") != "2")
        {
            WalletBatch batch(*walletInstance->database);

            for (const CWalletTx& wtxOld : vWtx)
            {
                uint256 hash = wtxOld.GetHash();
                std::map<uint256, CWalletTx>::iterator mi = walletInstance->mapWallet.find(hash);
                if (mi != walletInstance->mapWallet.end())
                {
                    const CWalletTx* copyFrom = &wtxOld;
                    CWalletTx* copyTo = &mi->second;
                    copyTo->mapValue = copyFrom->mapValue;
                    copyTo->vOrderForm = copyFrom->vOrderForm;
                    copyTo->nTimeReceived = copyFrom->nTimeReceived;
                    copyTo->nTimeSmart = copyFrom->nTimeSmart;
                    copyTo->fFromMe = copyFrom->fFromMe;
                    copyTo->nOrderPos = copyFrom->nOrderPos;
                    batch.WriteTx(*copyTo);
                }
            }
        }
    }

    uiInterface.LoadWallet(walletInstance);

//在验证接口中注册。在重新扫描之后可以这样做，因为我们仍然保持着CS主。
    RegisterValidationInterface(walletInstance.get());

    walletInstance->SetBroadcastTransactions(gArgs.GetBoolArg("-walletbroadcast", DEFAULT_WALLETBROADCAST));

    {
        walletInstance->WalletLogPrintf("setKeyPool.size() = %u\n",      walletInstance->GetKeyPoolSize());
        walletInstance->WalletLogPrintf("mapWallet.size() = %u\n",       walletInstance->mapWallet.size());
        walletInstance->WalletLogPrintf("mapAddressBook.size() = %u\n",  walletInstance->mapAddressBook.size());
    }

    return walletInstance;
}

void CWallet::postInitProcess()
{
//将不在块中的钱包交易添加到mempool
//在这里执行此操作，因为mempool需要加载genesis块
    ReacceptWalletTransactions();
}

bool CWallet::BackupWallet(const std::string& strDest)
{
    return database->Backup(strDest);
}

CKeyPool::CKeyPool()
{
    nTime = GetTime();
    fInternal = false;
    m_pre_split = false;
}

CKeyPool::CKeyPool(const CPubKey& vchPubKeyIn, bool internalIn)
{
    nTime = GetTime();
    vchPubKey = vchPubKeyIn;
    fInternal = internalIn;
    m_pre_split = false;
}

CWalletKey::CWalletKey(int64_t nExpires)
{
    nTimeCreated = (nExpires ? GetTime() : 0);
    nTimeExpires = nExpires;
}

void CMerkleTx::SetMerkleBranch(const CBlockIndex* pindex, int posInBlock)
{
//更新tx的hashblock
    hashBlock = pindex->GetBlockHash();

//设置事务在块中的位置
    nIndex = posInBlock;
}

int CMerkleTx::GetDepthInMainChain(interfaces::Chain::Lock& locked_chain) const
{
    if (hashUnset())
        return 0;

    AssertLockHeld(cs_main);

//找到它声称的所在的街区
    CBlockIndex* pindex = LookupBlockIndex(hashBlock);
    if (!pindex || !chainActive.Contains(pindex))
        return 0;

    return ((nIndex == -1) ? (-1) : 1) * (chainActive.Height() - pindex->nHeight + 1);
}

int CMerkleTx::GetBlocksToMaturity(interfaces::Chain::Lock& locked_chain) const
{
    if (!IsCoinBase())
        return 0;
    int chain_depth = GetDepthInMainChain(locked_chain);
assert(chain_depth >= 0); //CoinBase TX不应冲突
    return std::max(0, (COINBASE_MATURITY+1) - chain_depth);
}

bool CMerkleTx::IsImmatureCoinBase(interfaces::Chain::Lock& locked_chain) const
{
//注：非Coinbase Tx的GetBlockTourity为0
    return GetBlocksToMaturity(locked_chain) > 0;
}

bool CWalletTx::AcceptToMemoryPool(interfaces::Chain::Lock& locked_chain, const CAmount& nAbsurdFee, CValidationState& state)
{
LockAnnotation lock(::cs_main); //临时，用于下面的AcceptToMoryPool。在即将到来的提交中删除。

//我们必须在这里设置finmempool，而它将由
//进入了mempool回调，如果没有，将会有一场比赛
//用户可以在一个循环中调用sendmony并命中虚假的资金不足错误。
//因为我们认为这个新产生的交易的变化是
//不可用，因为我们还不知道它在mempool中。
    /*l ret=：：acceptToMoryPool（内存池、状态、Tx、nullptr/*pfMissingInputs*/，
                                nullptr/*pltxnreplaced（空指针/*pltxnreplaced）*/, false /* bypass_limits */, nAbsurdFee);

    fInMempool |= ret;
    return ret;
}

void CWallet::LearnRelatedScripts(const CPubKey& key, OutputType type)
{
    if (key.IsCompressed() && (type == OutputType::P2SH_SEGWIT || type == OutputType::BECH32)) {
        CTxDestination witdest = WitnessV0KeyHash(key.GetID());
        CScript witprog = GetScriptForDestination(witdest);
//确保生成的程序是可解决的。
        assert(IsSolvable(*this, witprog));
        AddCScript(witprog);
    }
}

void CWallet::LearnAllRelatedScripts(const CPubKey& key)
{
//outputtype:：p2sh_segwit始终为所有类型添加所有必需的脚本。
    LearnRelatedScripts(key, OutputType::P2SH_SEGWIT);
}

std::vector<OutputGroup> CWallet::GroupOutputs(const std::vector<COutput>& outputs, bool single_coin) const {
    std::vector<OutputGroup> groups;
    std::map<CTxDestination, OutputGroup> gmap;
    CTxDestination dst;
    for (const auto& output : outputs) {
        if (output.fSpendable) {
            CInputCoin input_coin = output.GetInputCoin();

            size_t ancestors, descendants;
            mempool.GetTransactionAncestry(output.tx->GetHash(), ancestors, descendants);
            if (!single_coin && ExtractDestination(output.tx->tx->vout[output.i].scriptPubKey, dst)) {
//限制输出组不超过10个条目，以保护
//防止无意中创建过大的事务
//使用-avoidpartialspends时
                if (gmap[dst].m_outputs.size() >= OUTPUT_GROUP_MAX_ENTRIES) {
                    groups.push_back(gmap[dst]);
                    gmap.erase(dst);
                }
                gmap[dst].Insert(input_coin, output.nDepth, output.tx->IsFromMe(ISMINE_ALL), ancestors, descendants);
            } else {
                groups.emplace_back(input_coin, output.nDepth, output.tx->IsFromMe(ISMINE_ALL), ancestors, descendants);
            }
        }
    }
    if (!single_coin) {
        for (const auto& it : gmap) groups.push_back(it.second);
    }
    return groups;
}

bool CWallet::GetKeyOrigin(const CKeyID& keyID, KeyOriginInfo& info) const
{
    CKeyMetadata meta;
    {
        LOCK(cs_wallet);
        auto it = mapKeyMetadata.find(keyID);
        if (it != mapKeyMetadata.end()) {
            meta = it->second;
        }
    }
    if (!meta.hdKeypath.empty()) {
        if (!ParseHDKeypath(meta.hdKeypath, info.path)) return false;
//获取正确的主密钥ID
        CKey key;
        GetKey(meta.hd_seed_id, key);
        CExtKey masterKey;
        masterKey.SetSeed(key.begin(), key.size());
//计算标识符
        CKeyID masterid = masterKey.key.GetPubKey().GetID();
        std::copy(masterid.begin(), masterid.begin() + 4, info.fingerprint);
} else { //单键发布获取自己的主指纹
        std::copy(keyID.begin(), keyID.begin() + 4, info.fingerprint);
    }
    return true;
}
