
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

#ifndef BITCOIN_WALLET_WALLETDB_H
#define BITCOIN_WALLET_WALLETDB_H

#include <amount.h>
#include <primitives/transaction.h>
#include <wallet/db.h>
#include <key.h>

#include <list>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

/*
 *钱包数据库类概述：
 *
 *-walletbatch是wallet数据库的抽象修饰符对象，它封装了一个数据库。
 *批量更新以及对数据库执行操作的方法。它应该对数据库实现不可知。
 *
 *以下类是特定于实现的：
 *-Berkeleyenvironment是一个数据库存在的环境。
 *-BerkeleyDatabase表示钱包数据库。
 *-Berkeleybatch是一个低级的数据库批处理更新。
 **/


static const bool DEFAULT_FLUSHWALLET = true;

struct CBlockLocator;
class CKeyPool;
class CMasterKey;
class CScript;
class CWallet;
class CWalletTx;
class uint160;
class uint256;

/*后端不可知数据库类型。*/
using WalletDatabase = BerkeleyDatabase;

/*钱包数据库的错误状态*/
enum class DBErrors
{
    LOAD_OK,
    CORRUPT,
    NONCRITICAL_ERROR,
    TOO_NEW,
    LOAD_FAIL,
    NEED_REWRITE
};

/*简单的高清链数据模型*/
class CHDChain
{
public:
    uint32_t nExternalChainCounter;
    uint32_t nInternalChainCounter;
CKeyID seed_id; //！<种子HASH160

    static const int VERSION_HD_BASE        = 1;
    static const int VERSION_HD_CHAIN_SPLIT = 2;
    static const int CURRENT_VERSION        = VERSION_HD_CHAIN_SPLIT;
    int nVersion;

    CHDChain() { SetNull(); }
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(this->nVersion);
        READWRITE(nExternalChainCounter);
        READWRITE(seed_id);
        if (this->nVersion >= VERSION_HD_CHAIN_SPLIT)
            READWRITE(nInternalChainCounter);
    }

    void SetNull()
    {
        nVersion = CHDChain::CURRENT_VERSION;
        nExternalChainCounter = 0;
        nInternalChainCounter = 0;
        seed_id.SetNull();
    }
};

class CKeyMetadata
{
public:
    static const int VERSION_BASIC=1;
    static const int VERSION_WITH_HDDATA=10;
    static const int CURRENT_VERSION=VERSION_WITH_HDDATA;
    int nVersion;
int64_t nCreateTime; //0意味着未知
std::string hdKeypath; //可选HD/BIP32键路径
CKeyID hd_seed_id; //用于派生此密钥的HD种子的ID

    CKeyMetadata()
    {
        SetNull();
    }
    explicit CKeyMetadata(int64_t nCreateTime_)
    {
        SetNull();
        nCreateTime = nCreateTime_;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(nCreateTime);
        if (this->nVersion >= VERSION_WITH_HDDATA)
        {
            READWRITE(hdKeypath);
            READWRITE(hd_seed_id);
        }
    }

    void SetNull()
    {
        nVersion = CKeyMetadata::CURRENT_VERSION;
        nCreateTime = 0;
        hdKeypath.clear();
        hd_seed_id.SetNull();
    }
};

/*访问钱包数据库。
 *这表示在
 *数据库。它将在对象超出范围时提交。
 *可选（默认情况下为开），它也将刷新到磁盘。
 **/

class WalletBatch
{
private:
    template <typename K, typename T>
    bool WriteIC(const K& key, const T& value, bool fOverwrite = true)
    {
        if (!m_batch.Write(key, value, fOverwrite)) {
            return false;
        }
        m_database.IncrementUpdateCounter();
        return true;
    }

    template <typename K>
    bool EraseIC(const K& key)
    {
        if (!m_batch.Erase(key)) {
            return false;
        }
        m_database.IncrementUpdateCounter();
        return true;
    }

public:
    explicit WalletBatch(WalletDatabase& database, const char* pszMode = "r+", bool _fFlushOnClose = true) :
        m_batch(database, pszMode, _fFlushOnClose),
        m_database(database)
    {
    }
    WalletBatch(const WalletBatch&) = delete;
    WalletBatch& operator=(const WalletBatch&) = delete;

    bool WriteName(const std::string& strAddress, const std::string& strName);
    bool EraseName(const std::string& strAddress);

    bool WritePurpose(const std::string& strAddress, const std::string& purpose);
    bool ErasePurpose(const std::string& strAddress);

    bool WriteTx(const CWalletTx& wtx);
    bool EraseTx(uint256 hash);

    bool WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata &keyMeta);
    bool WriteCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, const CKeyMetadata &keyMeta);
    bool WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey);

    bool WriteCScript(const uint160& hash, const CScript& redeemScript);

    bool WriteWatchOnly(const CScript &script, const CKeyMetadata &keymeta);
    bool EraseWatchOnly(const CScript &script);

    bool WriteBestBlock(const CBlockLocator& locator);
    bool ReadBestBlock(CBlockLocator& locator);

    bool WriteOrderPosNext(int64_t nOrderPosNext);

    bool ReadPool(int64_t nPool, CKeyPool& keypool);
    bool WritePool(int64_t nPool, const CKeyPool& keypool);
    bool ErasePool(int64_t nPool);

    bool WriteMinVersion(int nVersion);

///将目标数据键、值元组写入数据库
    bool WriteDestData(const std::string &address, const std::string &key, const std::string &value);
///从钱包数据库中删除目标数据元组
    bool EraseDestData(const std::string &address, const std::string &key);

    DBErrors LoadWallet(CWallet* pwallet);
    DBErrors FindWalletTx(std::vector<uint256>& vTxHash, std::vector<CWalletTx>& vWtx);
    DBErrors ZapWalletTx(std::vector<CWalletTx>& vWtx);
    DBErrors ZapSelectTx(std::vector<uint256>& vHashIn, std::vector<uint256>& vHashOut);
    /*尽量（非常小心！）恢复钱包数据库（带有可能的密钥类型筛选器）*/
    static bool Recover(const fs::path& wallet_path, void *callbackDataIn, bool (*recoverKVcallback)(void* callbackData, CDataStream ssKey, CDataStream ssValue), std::string& out_backup_filename);
    /*恢复方便函数以绕过密钥筛选器回调，在验证失败时调用，恢复所有内容*/
    static bool Recover(const fs::path& wallet_path, std::string& out_backup_filename);
    /*recover filter（用作回调），只允许密钥（加密密钥）作为kv/密钥类型通过*/
    static bool RecoverKeysOnlyFilter(void *callbackData, CDataStream ssKey, CDataStream ssValue);
    /*函数确定某个Kv/密钥类型是否为密钥（加密密钥）类型*/
    static bool IsKeyType(const std::string& strType);
    /*验证数据库环境*/
    static bool VerifyEnvironment(const fs::path& wallet_path, std::string& errorStr);
    /*验证数据库文件*/
    static bool VerifyDatabaseFile(const fs::path& wallet_path, std::string& warningStr, std::string& errorStr);

//！编写hdchain模型（外部链子索引计数器）
    bool WriteHDChain(const CHDChain& chain);

    bool WriteWalletFlags(const uint64_t flags);
//！开始新事务
    bool TxnBegin();
//！提交当前事务
    bool TxnCommit();
//！中止当前事务
    bool TxnAbort();
//！阅读钱包版本
    bool ReadVersion(int& nVersion);
//！写钱包版本
    bool WriteVersion(int nVersion);
private:
    BerkeleyBatch m_batch;
    WalletDatabase& m_database;
};

//！压缩bdb状态，使wallet.dat独立（如果有更改）
void MaybeCompactWalletDB();

#endif //比特币钱包
