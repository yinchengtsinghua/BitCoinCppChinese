
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

#ifndef BITCOIN_WALLET_DB_H
#define BITCOIN_WALLET_DB_H

#include <clientversion.h>
#include <fs.h>
#include <serialize.h>
#include <streams.h>
#include <sync.h>
#include <util/system.h>
#include <version.h>

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <db_cxx.h>

static const unsigned int DEFAULT_WALLET_DBLOGSIZE = 100;
static const bool DEFAULT_WALLET_PRIVDB = true;

struct WalletDatabaseFileId {
    u_int8_t value[DB_FILE_ID_LEN];
    bool operator==(const WalletDatabaseFileId& rhs) const;
};

class BerkeleyDatabase;

class BerkeleyEnvironment
{
private:
    bool fDbEnvInit;
    bool fMockDb;
//不要更改为fs:：path，因为这样会导致
//由静态初始化的内部指针导致的关闭问题/崩溃。
    std::string strPath;

public:
    std::unique_ptr<DbEnv> dbenv;
    std::map<std::string, int> mapFileUseCount;
    std::map<std::string, std::reference_wrapper<BerkeleyDatabase>> m_databases;
    std::unordered_map<std::string, WalletDatabaseFileId> m_fileids;
    std::condition_variable_any m_db_in_use;

    BerkeleyEnvironment(const fs::path& env_directory);
    ~BerkeleyEnvironment();
    void Reset();

    void MakeMock();
    bool IsMock() const { return fMockDb; }
    bool IsInitialized() const { return fDbEnvInit; }
    bool IsDatabaseLoaded(const std::string& db_filename) const { return m_databases.find(db_filename) != m_databases.end(); }
    fs::path Directory() const { return strPath; }

    /*
     *验证数据库文件strfile是否正常。如果不是，
     *调用回调以尝试恢复。
     *必须在打开strfile之前调用它。
     *如果strfile正常，则返回true。
     **/

    enum class VerifyResult { VERIFY_OK,
                        RECOVER_OK,
                        RECOVER_FAIL };
    typedef bool (*recoverFunc_type)(const fs::path& file_path, std::string& out_backup_filename);
    VerifyResult Verify(const std::string& strFile, recoverFunc_type recoverFunc, std::string& out_backup_filename);
    /*
     *从确认为“坏”的文件中恢复数据。
     *faggressive设置db_aggressive标志（参见Berkeley db->verify（）方法文档）。
     *将二进制键/值对附加到vresult，如果成功，则返回true。
     *注意：将整个数据库读取到内存中，因此不能使用
     *用于大型数据库。
     **/

    typedef std::pair<std::vector<unsigned char>, std::vector<unsigned char> > KeyValPair;
    bool Salvage(const std::string& strFile, bool fAggressive, std::vector<KeyValPair>& vResult);

    bool Open(bool retry);
    void Close();
    void Flush(bool fShutdown);
    void CheckpointLSN(const std::string& strFile);

    void CloseDb(const std::string& strFile);
    void ReloadDbEnv();

    DbTxn* TxnBegin(int flags = DB_TXN_WRITE_NOSYNC)
    {
        DbTxn* ptxn = nullptr;
        int ret = dbenv->txn_begin(nullptr, &ptxn, flags);
        if (!ptxn || ret != 0)
            return nullptr;
        return ptxn;
    }
};

/*返回当前是否加载了钱包数据库。*/
bool IsWalletLoaded(const fs::path& wallet_path);

/*获取给定钱包路径的Berkeleyenvironment和数据库文件名。*/
BerkeleyEnvironment* GetWalletEnv(const fs::path& wallet_path, std::string& database_filename);

/*此类的实例表示一个数据库。
 *对于BerkeleyDB，这只是一个（env，strfile）元组。
 */

class BerkeleyDatabase
{
    friend class BerkeleyBatch;
public:
    /*创建虚拟数据库句柄*/
    BerkeleyDatabase() : nUpdateCounter(0), nLastSeen(0), nLastFlushed(0), nLastWalletUpdate(0), env(nullptr)
    {
    }

    /*创建实际数据库的数据库句柄*/
    BerkeleyDatabase(const fs::path& wallet_path, bool mock = false) :
        nUpdateCounter(0), nLastSeen(0), nLastFlushed(0), nLastWalletUpdate(0)
    {
        env = GetWalletEnv(wallet_path, strFile);
        auto inserted = env->m_databases.emplace(strFile, std::ref(*this));
        assert(inserted.second);
        if (mock) {
            env->Close();
            env->Reset();
            env->MakeMock();
        }
    }

    ~BerkeleyDatabase() {
        if (env) {
            size_t erased = env->m_databases.erase(strFile);
            assert(erased == 1);
        }
    }

    /*返回用于在指定路径访问数据库的对象。*/
    static std::unique_ptr<BerkeleyDatabase> Create(const fs::path& path)
    {
        return MakeUnique<BerkeleyDatabase>(path);
    }

    /*返回用于访问不具有读/写功能的虚拟数据库的对象。*/
    static std::unique_ptr<BerkeleyDatabase> CreateDummy()
    {
        return MakeUnique<BerkeleyDatabase>();
    }

    /*返回用于访问临时内存数据库的对象。*/
    static std::unique_ptr<BerkeleyDatabase> CreateMock()
    {
        /*urn makeunique<berkeleydatabase>（“”，true/*mock*/）；
    }

    /**在磁盘上重写整个数据库，如果不是零，则键pszskip除外。
     **/

    bool Rewrite(const char* pszSkip=nullptr);

    /*将整个数据库备份到一个文件。
     **/

    bool Backup(const std::string& strDest);

    /*确保所有更改都刷新到磁盘。
     **/

    void Flush(bool shutdown);

    void IncrementUpdateCounter();

    void ReloadDbEnv();

    std::atomic<unsigned int> nUpdateCounter;
    unsigned int nLastSeen;
    unsigned int nLastFlushed;
    int64_t nLastWalletUpdate;

    /*数据库指针。这是延迟初始化的，并在刷新期间重置，因此可以为空。*/
    std::unique_ptr<Db> m_db;

private:
    /*特定于BerkeleyDB*/
    BerkeleyEnvironment *env;
    std::string strFile;

    /*返回此数据库句柄是否是用于测试的虚拟句柄。
     *仅在低水平使用时，应用程序最好不要在意
     关于这个。
     **/

    bool IsDummy() { return env == nullptr; }
};

/*提供对伯克利数据库的访问的raii类*/
class BerkeleyBatch
{
    /*自动清除销毁数据的raii类*/
    class SafeDbt final
    {
        Dbt m_dbt;

    public:
//用内部管理的数据构造DBT
        SafeDbt();
//用提供的数据构造DBT
        SafeDbt(void* data, size_t size);
        ~SafeDbt();

//委派到DBT
        const void* get_data() const;
        u_int32_t get_size() const;

//访问基础DBT的转换运算符
        operator Dbt*();
    };

protected:
    Db* pdb;
    std::string strFile;
    DbTxn* activeTxn;
    bool fReadOnly;
    bool fFlushOnClose;
    BerkeleyEnvironment *env;

public:
    explicit BerkeleyBatch(BerkeleyDatabase& database, const char* pszMode = "r+", bool fFlushOnCloseIn=true);
    ~BerkeleyBatch() { Close(); }

    BerkeleyBatch(const BerkeleyBatch&) = delete;
    BerkeleyBatch& operator=(const BerkeleyBatch&) = delete;

    void Flush();
    void Close();
    static bool Recover(const fs::path& file_path, void *callbackDataIn, bool (*recoverKVcallback)(void* callbackData, CDataStream ssKey, CDataStream ssValue), std::string& out_backup_filename);

    /*被动冲洗钱包（尝试锁定）
       理想的定期呼叫*/

    static bool PeriodicFlush(BerkeleyDatabase& database);
    /*验证数据库环境*/
    static bool VerifyEnvironment(const fs::path& file_path, std::string& errorStr);
    /*验证数据库文件*/
    static bool VerifyDatabaseFile(const fs::path& file_path, std::string& warningStr, std::string& errorStr, BerkeleyEnvironment::recoverFunc_type recoverFunc);

    template <typename K, typename T>
    bool Read(const K& key, T& value)
    {
        if (!pdb)
            return false;

//钥匙
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        SafeDbt datKey(ssKey.data(), ssKey.size());

//读
        SafeDbt datValue;
        int ret = pdb->get(activeTxn, datKey, datValue, 0);
        bool success = false;
        if (datValue.get_data() != nullptr) {
//非序列化值
            try {
                CDataStream ssValue((char*)datValue.get_data(), (char*)datValue.get_data() + datValue.get_size(), SER_DISK, CLIENT_VERSION);
                ssValue >> value;
                success = true;
            } catch (const std::exception&) {
//在这种情况下，成功仍然是“错误的”
            }
        }
        return ret == 0 && success;
    }

    template <typename K, typename T>
    bool Write(const K& key, const T& value, bool fOverwrite = true)
    {
        if (!pdb)
            return true;
        if (fReadOnly)
            assert(!"Write called on database in read-only mode");

//钥匙
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        SafeDbt datKey(ssKey.data(), ssKey.size());

//价值
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.reserve(10000);
        ssValue << value;
        SafeDbt datValue(ssValue.data(), ssValue.size());

//写
        int ret = pdb->put(activeTxn, datKey, datValue, (fOverwrite ? 0 : DB_NOOVERWRITE));
        return (ret == 0);
    }

    template <typename K>
    bool Erase(const K& key)
    {
        if (!pdb)
            return false;
        if (fReadOnly)
            assert(!"Erase called on database in read-only mode");

//钥匙
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        SafeDbt datKey(ssKey.data(), ssKey.size());

//擦除
        int ret = pdb->del(activeTxn, datKey, 0);
        return (ret == 0 || ret == DB_NOTFOUND);
    }

    template <typename K>
    bool Exists(const K& key)
    {
        if (!pdb)
            return false;

//钥匙
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        SafeDbt datKey(ssKey.data(), ssKey.size());

//存在
        int ret = pdb->exists(activeTxn, datKey, 0);
        return (ret == 0);
    }

    Dbc* GetCursor()
    {
        if (!pdb)
            return nullptr;
        Dbc* pcursor = nullptr;
        int ret = pdb->cursor(nullptr, &pcursor, 0);
        if (ret != 0)
            return nullptr;
        return pcursor;
    }

    int ReadAtCursor(Dbc* pcursor, CDataStream& ssKey, CDataStream& ssValue)
    {
//光标读取
        SafeDbt datKey;
        SafeDbt datValue;
        int ret = pcursor->get(datKey, datValue, DB_NEXT);
        if (ret != 0)
            return ret;
        else if (datKey.get_data() == nullptr || datValue.get_data() == nullptr)
            return 99999;

//转换为流
        ssKey.SetType(SER_DISK);
        ssKey.clear();
        ssKey.write((char*)datKey.get_data(), datKey.get_size());
        ssValue.SetType(SER_DISK);
        ssValue.clear();
        ssValue.write((char*)datValue.get_data(), datValue.get_size());
        return 0;
    }

    bool TxnBegin()
    {
        if (!pdb || activeTxn)
            return false;
        DbTxn* ptxn = env->TxnBegin();
        if (!ptxn)
            return false;
        activeTxn = ptxn;
        return true;
    }

    bool TxnCommit()
    {
        if (!pdb || !activeTxn)
            return false;
        int ret = activeTxn->commit(0);
        activeTxn = nullptr;
        return (ret == 0);
    }

    bool TxnAbort()
    {
        if (!pdb || !activeTxn)
            return false;
        int ret = activeTxn->abort();
        activeTxn = nullptr;
        return (ret == 0);
    }

    bool ReadVersion(int& nVersion)
    {
        nVersion = 0;
        return Read(std::string("version"), nVersion);
    }

    bool WriteVersion(int nVersion)
    {
        return Write(std::string("version"), nVersion);
    }

    bool static Rewrite(BerkeleyDatabase& database, const char* pszSkip = nullptr);
};

#endif //比特币钱包
