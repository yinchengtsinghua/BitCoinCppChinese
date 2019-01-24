
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

#include <wallet/db.h>

#include <addrman.h>
#include <hash.h>
#include <protocol.h>
#include <util/strencodings.h>
#include <wallet/walletutil.h>

#include <stdint.h>

#ifndef WIN32
#include <sys/stat.h>
#endif

#include <boost/thread.hpp>

namespace {

//！确保数据库在环境中具有唯一的fileid。如果它
//！不，抛出一个错误。多个BDB缓存无法正常工作
//！开放式数据库具有相同的文件ID（写入一个数据库的值可能显示
//！向上读取其他数据库）。
//！
//！默认情况下，BerkeleyDB生成唯一的文件ID
//！（https://docs.oracle.com/cd/e17275_01/html/programmer_-reference/program_-copy.html）
//！所以比特币不应该用相同的文件ID创建不同的数据库，但是
//！如果用户手动复制数据库文件，则会触发此错误。
void CheckUniqueFileid(const BerkeleyEnvironment& env, const std::string& filename, Db& db, WalletDatabaseFileId& fileid)
{
    if (env.IsMock()) return;

    int ret = db.get_mpf()->get_fileid(fileid.value);
    if (ret != 0) {
        throw std::runtime_error(strprintf("BerkeleyBatch: Can't open database %s (get_fileid failed with %d)", filename, ret));
    }

    for (const auto& item : env.m_fileids) {
        if (fileid == item.second && &fileid != &item.second) {
            throw std::runtime_error(strprintf("BerkeleyBatch: Can't open database %s (duplicates fileid %s from %s)", filename,
                HexStr(std::begin(item.second.value), std::end(item.second.value)), item.first));
        }
    }
}

CCriticalSection cs_db;
std::map<std::string, BerkeleyEnvironment> g_dbenvs GUARDED_BY(cs_db); //！<map from directory name to open db environment.
} //命名空间

bool WalletDatabaseFileId::operator==(const WalletDatabaseFileId& rhs) const
{
    return memcmp(value, &rhs.value, sizeof(value)) == 0;
}

static void SplitWalletPath(const fs::path& wallet_path, fs::path& env_directory, std::string& database_filename)
{
    if (fs::is_regular_file(wallet_path)) {
//向后兼容的特殊情况：如果钱包路径指向
//现有文件，将其视为父级中BDB数据文件的路径
//还包含bdb日志文件的目录。
        env_directory = wallet_path.parent_path();
        database_filename = wallet_path.filename().string();
    } else {
//普通情况：将钱包路径解释为包含
//数据和日志文件。
        env_directory = wallet_path;
        database_filename = "wallet.dat";
    }
}

bool IsWalletLoaded(const fs::path& wallet_path)
{
    fs::path env_directory;
    std::string database_filename;
    SplitWalletPath(wallet_path, env_directory, database_filename);
    LOCK(cs_db);
    auto env = g_dbenvs.find(env_directory.string());
    if (env == g_dbenvs.end()) return false;
    return env->second.IsDatabaseLoaded(database_filename);
}

BerkeleyEnvironment* GetWalletEnv(const fs::path& wallet_path, std::string& database_filename)
{
    fs::path env_directory;
    SplitWalletPath(wallet_path, env_directory, database_filename);
    LOCK(cs_db);
//注意：可以在
//如果密钥已经存在，则使用emplace函数。这有点低效，
//但这不是一个大问题，因为地图将来会被修改保存。
//无论如何，指针而不是对象。
    return &g_dbenvs.emplace(std::piecewise_construct, std::forward_as_tuple(env_directory.string()), std::forward_as_tuple(env_directory)).first->second;
}

//
//贝克莱间歇式
//

void BerkeleyEnvironment::Close()
{
    if (!fDbEnvInit)
        return;

    fDbEnvInit = false;

    for (auto& db : m_databases) {
        auto count = mapFileUseCount.find(db.first);
        assert(count == mapFileUseCount.end() || count->second == 0);
        BerkeleyDatabase& database = db.second.get();
        if (database.m_db) {
            database.m_db->close(0);
            database.m_db.reset();
        }
    }

    int ret = dbenv->close(0);
    if (ret != 0)
        LogPrintf("BerkeleyEnvironment::Close: Error %d closing database environment: %s\n", ret, DbEnv::strerror(ret));
    if (!fMockDb)
        DbEnv((u_int32_t)0).remove(strPath.c_str(), 0);
}

void BerkeleyEnvironment::Reset()
{
    dbenv.reset(new DbEnv(DB_CXX_NO_EXCEPTIONS));
    fDbEnvInit = false;
    fMockDb = false;
}

BerkeleyEnvironment::BerkeleyEnvironment(const fs::path& dir_path) : strPath(dir_path.string())
{
    Reset();
}

BerkeleyEnvironment::~BerkeleyEnvironment()
{
    Close();
}

bool BerkeleyEnvironment::Open(bool retry)
{
    if (fDbEnvInit)
        return true;

    boost::this_thread::interruption_point();

    fs::path pathIn = strPath;
    TryCreateDirectories(pathIn);
    if (!LockDirectory(pathIn, ".walletlock")) {
        LogPrintf("Cannot obtain a lock on wallet directory %s. Another instance of bitcoin may be using it.\n", strPath);
        return false;
    }

    fs::path pathLogDir = pathIn / "database";
    TryCreateDirectories(pathLogDir);
    fs::path pathErrorFile = pathIn / "db.log";
    LogPrintf("BerkeleyEnvironment::Open: LogDir=%s ErrorFile=%s\n", pathLogDir.string(), pathErrorFile.string());

    unsigned int nEnvFlags = 0;
    if (gArgs.GetBoolArg("-privdb", DEFAULT_WALLET_PRIVDB))
        nEnvFlags |= DB_PRIVATE;

    dbenv->set_lg_dir(pathLogDir.string().c_str());
dbenv->set_cachesize(0, 0x100000, 1); //1百万英镑就够钱包了
    dbenv->set_lg_bsize(0x10000);
    dbenv->set_lg_max(1048576);
    dbenv->set_lk_max_locks(40000);
    dbenv->set_lk_max_objects(40000);
dbenv->set_errfile(fsbridge::fopen(pathErrorFile, "a")); //调试
    dbenv->set_flags(DB_AUTO_COMMIT, 1);
    dbenv->set_flags(DB_TXN_WRITE_NOSYNC, 1);
    dbenv->log_set_config(DB_LOG_AUTO_REMOVE, 1);
    int ret = dbenv->open(strPath.c_str(),
                         DB_CREATE |
                             DB_INIT_LOCK |
                             DB_INIT_LOG |
                             DB_INIT_MPOOL |
                             DB_INIT_TXN |
                             DB_THREAD |
                             DB_RECOVER |
                             nEnvFlags,
                         S_IRUSR | S_IWUSR);
    if (ret != 0) {
        LogPrintf("BerkeleyEnvironment::Open: Error %d opening database environment: %s\n", ret, DbEnv::strerror(ret));
        int ret2 = dbenv->close(0);
        if (ret2 != 0) {
            LogPrintf("BerkeleyEnvironment::Open: Error %d closing failed database environment: %s\n", ret2, DbEnv::strerror(ret2));
        }
        Reset();
        if (retry) {
//尝试将数据库env移出
            fs::path pathDatabaseBak = pathIn / strprintf("database.%d.bak", GetTime());
            try {
                fs::rename(pathLogDir, pathDatabaseBak);
                LogPrintf("Moved old %s to %s. Retrying.\n", pathLogDir.string(), pathDatabaseBak.string());
            } catch (const fs::filesystem_error&) {
//失败是可以的（好吧，不是真的，但并不比我们开始时更糟糕）
            }
//再打开一次
            /*（！打开（错误/*重试*/）
                //如果仍然失败，可能意味着我们甚至无法创建数据库env
                返回错误；
            }
        }否则{
            返回错误；
        }
    }

    fdbenvinit=真；
    fmockdb=假；
    回归真实；
}

void Berkeleyenvironment:：makeMock（）。
{
    如果（FDBEVENIT）
        throw std:：runtime_error（“berkeleyenvironment:：makemock:already initialized”）；

    boost:：this_thread:：interrupt_point（）；

    logprint（bclog:：db，“Berkeleyenvironment:：makeMock \n”）；

    dbenv->设置缓存大小（1，0，1）；
    dbenv->set_lg_bsize（10485760*4）；
    dbenv->set_lg_max（10485760）；
    dbenv->set_lk_max_locks（10000）；
    dbenv->set_lk_max_objects（10000）；
    dbenv->set_flags（db_auto_commit，1）；
    dbenv->log_set_config（内存中的db_log_，1）；
    int ret=dbenv->open（nullptr，
                         DB创建
                             dBi-伊尼特洛克
                             dBi－InITyLogo
                             dBi－InITyMcPoin
                             dBi- iNITH-TXN
                             dB线程
                             B
                         s_irusr_s_iwusr）；
    如果（RET＞0）
        throw std:：runtime_error（strprintf（“berkeleyenvironment:：makemock:错误%d，打开数据库环境。”，ret））；

    fdbenvinit=真；
    FMOCKDB＝真；
}

Berkeleyenvironment:：verifyResult Berkeleyenvironment:：verify（const std:：string&strfile，recoverfunc_type recoverfunc，std:：string&out_backup_filename）
{
    锁（CSYDB）；
    断言（mapfileusecount.count（strfile）=0）；

    db db（dbenv.get（），0）；
    int result=db.verify（strfile.c_str（），nullptr，nullptr，0）；
    如果（结果==0）
        返回verifyresult:：verifyok；
    else if（recoverfunc==nullptr）
        返回verifyresult:：recover_fail；

    //尝试恢复：
    bool frecovered=（*recoverfunc）（fs:：path（strpath）/strfile，out_backup_filename）；
    退货（Frecovered？verifyresult:：recover_确定：verifyresult:：recover_失败）；
}

berkeleybatch:：safedbt:：safedbt（）。
{
    m_dbt.设置_标志（db_dbt_malloc）；
}

berkeleybatch:：safedbt:：safedbt（void*数据，大小，大小）
    ：m_-dbt（数据、大小）
{
}

Berkeleybatch:：SafedBT:：~SafedBT（）。
{
    如果（m_dbt.get_data（）！= null pTr）{
        //清除内存，例如，如果它是私钥
        内存清理（m_dbt.get_data（），m_dbt.get_size（））；
        //在db_dbt_malloc下，数据由dbt进行malloc，但必须是
        //由调用方释放。
        //https://docs.oracle.com/cd/e17275_01/html/api_reference/c/dbt.html
        if（m_dbt.get_flags（）&db_dbt_malloc）
            free（m_dbt.get_data（））；
        }
    }
}

const void*berkeleybatch:：safedbt:：get_data（）const
{
    返回m_dbt.get_data（）；
}

uInt32_t Berkeleybatch:：safedbt:：get_size（）常量
{
    返回m_dbt.get_size（）；
}

berkeleybatch:：safedbt:：operator dbt*（）
{
    返回和MyDBT；
}

bool berkeleybatch:：recover（const fs:：path&file_path，void*callbackdatain，bool（*recoverk回调）（void*callbackdata，cdatastream sskey，cdatastream ssvalue），std:：string&newfilename）
{
    std：：字符串文件名；
    Berkeleyenvironment*env=getwalletenv（文件路径，文件名）；

    //恢复过程：
    //将wallet文件移动到walletfilename.timestamp.bak
    //使用faggressive=true调用salvage
    //获取尽可能多的数据。
    //将回收的数据重写为新的钱包文件
    //设置-rescan，以便所有缺少的事务
    发现。
    int64_t now=gettime（）；
    newfilename=strprintf（“%s.%d.bak”，文件名，现在）；

    int result=env->dbenv->dbrename（nullptr，filename.c_str（），nullptr，
                                       newFileName.c_str（），db_auto_commit）；
    如果（结果==0）
        logprintf（“已将%s重命名为%s\n”，文件名，新文件名）；
    其他的
    {
        logprintf（“未能将%s重命名为%s\n”，filename，newfilename）；
        返回错误；
    }

    std:：vector<berkeleyenvironment:：keyvalpair>salvageddata；
    bool fsaccess=env->salvage（newfilename，true，salvageddata）；
    if（salvageddata.empty（））
    {
        logprintf（“挽救（攻击性）在%s中找不到记录。\n”，newfilename）；
        返回错误；
    }
    logprintf（“salvage（aggressive）找到了%u条记录\n”，salvageddata.size（））；

    std:：unique_ptr<db>pdbcopy=makeunique<db>（env->db env.get（），0）；
    int ret=pdbcopy->open（nullptr，//txn指针
                            filename.c_str（），//文件名
                            “主”，//逻辑数据库名称
                            db_btree，//数据库类型
                            数据库创建，//标志
                            0）；
    如果（RET＞0）{
        logprintf（“无法创建数据库文件%s\n”，文件名）；
        pdbcopy->close（0）；
        返回错误；
    }

    dbtxn*ptxn=env->txnbegin（）；
    用于（Berkeleyenvironment:：KeyValPair&Row:SalvagedData）
    {
        if（恢复kvCallback）
        {
            cdatastream sskey（row.first，ser_disk，client_version）；
            cdatastream ssvalue（row.second，ser_disk，client_version）；
            如果（！）（*recoverk回调）（callbackdatain、sskey、ssvalue）
                继续；
        }
        dbt datkey（&row.first[0]，row.first.size（））；
        dbt datvalue（&row.second[0]，row.second.size（））；
        int ret2=pdbcopy->put（ptxn，&datkey，&datvalue，db_Nooverwrite）；
        如果（RIT2> 0）
            fsaccess=假；
    }
    ptxn->提交（0）；
    pdbcopy->close（0）；

    返回fsaccess；
}

bool berkeleybatch:：verifyenvironment（const fs:：path&file_path，std:：string&errorstr）
{
    std：：字符串walletfile；
    Berkeleyenvironment*env=getwalletenv（文件路径，walletfile）；
    fs:：path walletdir=env->directory（）；

    logprintf（“使用BerkeleyDB版本%s\n”，dbenv:：version（nullptr，nullptr，nullptr））；
    logprintf（“使用钱包%s\n”，walletfile）；

    //Wallet文件必须是一个没有目录的普通文件名
    如果（walletfile！=fs:：basename（walletfile）+fs:：extension（walletfile））。
    {
        errorstr=strprintf（（“wallet%s驻留在wallet目录%s”之外），walletfile，walletdir.string（））；
        返回错误；
    }

    如果（！）env->open（真/*重试*/)) {

        errorStr = strprintf(_("Error initializing wallet database environment %s!"), walletDir);
        return false;
    }

    return true;
}

bool BerkeleyBatch::VerifyDatabaseFile(const fs::path& file_path, std::string& warningStr, std::string& errorStr, BerkeleyEnvironment::recoverFunc_type recoverFunc)
{
    std::string walletFile;
    BerkeleyEnvironment* env = GetWalletEnv(file_path, walletFile);
    fs::path walletDir = env->Directory();

    if (fs::exists(walletDir / walletFile))
    {
        std::string backup_filename;
        BerkeleyEnvironment::VerifyResult r = env->Verify(walletFile, recoverFunc, backup_filename);
        if (r == BerkeleyEnvironment::VerifyResult::RECOVER_OK)
        {
            warningStr = strprintf(_("Warning: Wallet file corrupt, data salvaged!"
                                     " Original %s saved as %s in %s; if"
                                     " your balance or transactions are incorrect you should"
                                     " restore from a backup."),
                                   walletFile, backup_filename, walletDir);
        }
        if (r == BerkeleyEnvironment::VerifyResult::RECOVER_FAIL)
        {
            errorStr = strprintf(_("%s corrupt, salvage failed"), walletFile);
            return false;
        }
    }
//如果文件不存在，也返回true
    return true;
}

/*头的结尾，键/值数据的开头*/
static const char *HEADER_END = "HEADER=END";
/*键/值数据结束*/
static const char *DATA_END = "DATA=END";

bool BerkeleyEnvironment::Salvage(const std::string& strFile, bool fAggressive, std::vector<BerkeleyEnvironment::KeyValPair>& vResult)
{
    LOCK(cs_db);
    assert(mapFileUseCount.count(strFile) == 0);

    u_int32_t flags = DB_SALVAGE;
    if (fAggressive)
        flags |= DB_AGGRESSIVE;

    std::stringstream strDump;

    Db db(dbenv.get(), 0);
    int result = db.verify(strFile.c_str(), nullptr, &strDump, flags);
    if (result == DB_VERIFY_BAD) {
        LogPrintf("BerkeleyEnvironment::Salvage: Database salvage found errors, all data may not be recoverable.\n");
        if (!fAggressive) {
            LogPrintf("BerkeleyEnvironment::Salvage: Rerun with aggressive mode to ignore errors and continue.\n");
            return false;
        }
    }
    if (result != 0 && result != DB_VERIFY_BAD) {
        LogPrintf("BerkeleyEnvironment::Salvage: Database salvage failed with result %d.\n", result);
        return false;
    }

//bdb dump的格式为ascii行：
//标题行…
//标题=结束
//十六进制键
//十六进制值
//…重复的
//数据=结束

    std::string strLine;
    while (!strDump.eof() && strLine != HEADER_END)
getline(strDump, strLine); //跳过标题

    std::string keyHex, valueHex;
    while (!strDump.eof() && keyHex != DATA_END) {
        getline(strDump, keyHex);
        if (keyHex != DATA_END) {
            if (strDump.eof())
                break;
            getline(strDump, valueHex);
            if (valueHex == DATA_END) {
                LogPrintf("BerkeleyEnvironment::Salvage: WARNING: Number of keys in data does not match number of values.\n");
                break;
            }
            vResult.push_back(make_pair(ParseHex(keyHex), ParseHex(valueHex)));
        }
    }

    if (keyHex != DATA_END) {
        LogPrintf("BerkeleyEnvironment::Salvage: WARNING: Unexpected end of file while reading salvage output.\n");
        return false;
    }

    return (result == 0);
}


void BerkeleyEnvironment::CheckpointLSN(const std::string& strFile)
{
    dbenv->txn_checkpoint(0, 0, 0);
    if (fMockDb)
        return;
    dbenv->lsn_reset(strFile.c_str(), 0);
}


BerkeleyBatch::BerkeleyBatch(BerkeleyDatabase& database, const char* pszMode, bool fFlushOnCloseIn) : pdb(nullptr), activeTxn(nullptr)
{
    fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));
    fFlushOnClose = fFlushOnCloseIn;
    env = database.env;
    if (database.IsDummy()) {
        return;
    }
    const std::string &strFilename = database.strFile;

    bool fCreate = strchr(pszMode, 'c') != nullptr;
    unsigned int nFlags = DB_THREAD;
    if (fCreate)
        nFlags |= DB_CREATE;

    {
        LOCK(cs_db);
        /*（！env->open（错误/*重试*/）
            throw std:：runtime_error（“Berkeleybatch:未能打开数据库环境”）；

        pdb=database.m_db.get（）；
        如果（pdb==nullptr）
            国际直拨电话；
            std:：unique_ptr<db>pdb_temp=makeunique<db>（env->db env.get（），0）；

            bool fmockdb=env->ismock（）；
            如果（fMOCKDB）{
                dbmPoolFile*mpf=pdb_temp->get_mpf（）；
                ret=mpf->set_flags（db_mpool_nofile，1）；
                如果（RET）！= 0）{
                    throw std:：runtime_error（strprintf（“berkeleybatch:未能为数据库%s配置无临时文件备份”，strfilename））；
                }
            }

            ret=pdb_temp->open（nullptr，//txn指针
                            FMOCKDB？nullptr:strFileName.c_str（），//文件名
                            FMOCKDB？strFileName.c_str（）：“main”，//logical db name
                            db_btree，//数据库类型
                            nflags，//标志
                            0）；

            如果（RET）！= 0）{
                throw s t d:：runtime_error（strprintf（“berkeleybatch:错误%d，无法打开数据库%s”，ret，strfilename））；
            }

            //在包含bdb的环境上调用checkuniquefileid
            //避免在不同数据时发生的BDB数据一致性错误
            //同一环境中的文件具有相同的文件ID。
            / /
            //同时对所有其他g_dbenv调用checkuniquefileid以防止
            //通过另一个数据文件打开同一个数据文件的比特币
            //当文件通过等效的
            //符号链接或硬链接或绑定安装的明显不同
            //路径。将来，我们会更轻松地检查
            //可以改为执行设备ID，这将允许打开
            //同时备份钱包的不同副本。也许甚至
            //更理想的情况是，访问数据库的独占锁可以
            //实现，因此根本不需要进行相等性检查。（新的）
            //bdb版本对此有一个set_lk_exclusive方法
            //目的，但我们使用的旧版本没有。）
            对于（const auto&env:g_dbenvs）
                checkuniquefileid（env.second，strfilename，*pdb_temp，this->env->m_fileid[strfilename]）；
            }

            pdb=pdb_temp.release（）；
            数据库.m_db.reset（pdb）；

            如果（F创建&！存在（std:：string（“版本”））
                bool ftmp=氟利昂；
                freadonly=假；
                writeversion（客户端版本）；
                氟利昂=ftmp；
            }
        }
        ++env->mapfileusecount[strfilename]；
        strfile=strfilename；
    }
}

void Berkeleybatch:：flush（）。
{
    如果（ActueTxn）
        返回；

    //将数据库活动从内存池刷新到磁盘日志
    无符号整数nminutes=0；
    如果（FRADADON）
        nmin＝1；

    env->dbenv->txn_检查点（分钟？gargs.getarg（“-dblogsize”，默认“钱包”dblogsize）*1024:0，分钟，0）；
}

void BerkeleyDatabase:：IncrementUpdateCounter（）。
{
    +nupdatecounter；
}

void BerkeleyBatch:：Close（）。
{
    如果（！）PDB）
        返回；
    如果（ActueTxn）
        activetxn->abort（）；
    activetxn=nullptr；
    PDB＝NulLPTR；

    如果（fflushonclose）
        FrHuSE（）；

    {
        锁（CSYDB）；
        --env->mapfileusecount[strfile]；
    }
    env->m_db_in_use.notify_all（）；
}

void berkeleyenvironment:：closedb（const std:：string&strfile）
{
    {
        锁（CSYDB）；
        auto-it=m_databases.find（strfile）；
        断言（它）！=m_databases.end（））；
        berkeleydatabase&database=it->second.get（）；
        如果（database.m_db）
            //关闭数据库句柄
            database.m_db->close（0）；
            database.m_db.reset（）；
        }
    }
}

无效的Berkeleyenvironment:：ReloadBenv（）。
{
    //确保没有使用数据库
    断言锁定未保持（cs_db）；
    std:：unique_lock<ccriticalsection>lock（cs_db）；
    m_db_in_use.wait（lock，[this]（）
        用于（auto&count:mapfileusecount）
            如果（count.second>0），则返回false；
        }
        回归真实；
    （}）；

    std:：vector<std:：string>文件名；
    用于（auto-it:m_数据库）
        文件名。向后推（it.first）；
    }
    //关闭单个数据库
    for（const std:：string&filename:filename）
        closedb（文件名）；
    }
    //重置环境
    flush（true）；//这将刷新并关闭环境
    重新设置（）；
    打开（真）；
}

bool berkeleybatch:：rewrite（berkeleydatabase&database，const char*pszskip）
{
    if（database.isdummy（））
        回归真实；
    }
    Berkeleyenvironment*env=database.env；
    const std:：string&strfile=database.strfile；
    当（真）{
        {
            锁（CSYDB）；
            如果（！）env->mapfileusecount.count（strfile）env->mapfileusecount[strfile]==0）
                //将日志数据刷新到DAT文件
                env->closedb（strfile）；
                env->checkpointlsn（strfile）；
                env->mapfileusecount.erase（strfile）；

                bool fsaccess=真；
                logprintf（“berkeleybatch:：rewrite:重写%s…\n”，strfile）；
                std:：string strfileres=strfile+“.rewrite”；
                //用额外的包围DB的使用
                    Berkeleybatch数据库（数据库，“R”）；
                    std:：unique_ptr<db>pdbcopy=makeunique<db>（env->db env.get（），0）；

                    int ret=pdbcopy->open（nullptr，//txn指针
                                            strfileres.c_str（），//文件名
                                            “主”，//逻辑数据库名称
                                            db_btree，//数据库类型
                                            数据库创建，//标志
                                            0）；
                    如果（RET＞0）{
                        logprintf（“berkeleybatch:：rewrite:无法创建数据库文件%s\n”，strfileres）；
                        fsaccess=假；
                    }

                    dbc*pcursor=db.getcursor（）；
                    如果（光标）
                        同时（fsaccess）
                            cdatastream sskey（ser_disk，client_version）；
                            cdatastream ssvalue（ser_disk，client_version）；
                            int ret1=db.readatcursor（pcursor、sskey、ssvalue）；
                            如果（ret1==db_notfound）
                                pcursor->close（）；
                                断裂；
                            否则，如果（ret1！= 0）{
                                pcursor->close（）；
                                fsaccess=假；
                                断裂；
                            }
                            如果（PSZSKIP & &
                                strncmp（sskey.data（），pszskip，std:：min（sskey.size（），strlen（pszskip）））=0）
                                继续；
                            if（strncmp（sskey.data（），“\x07version”，8）==0）
                                //更新版本：
                                ssvalue.clear（）；
                                ssvalue<<客户端版本；
                            }
                            dbt datkey（sskey.data（），sskey.size（））；
                            dbt datvalue（ssvalue.data（），ssvalue.size（））；
                            int ret2=pdbcopy->put（nullptr，&datkey，&datvalue，db_Nooverwrite）；
                            如果（RIT2> 0）
                                fsaccess=假；
                        }
                    如果（f成功）{
                        关闭（）；
                        env->closedb（strfile）；
                        如果（pdbcopy->close（0））
                            fsaccess=假；
                    }否则{
                        pdbcopy->close（0）；
                    }
                }
                如果（f成功）{
                    db dba（env->db env.get（），0）；
                    if（dba.remove（strfile.c_str（），nullptr，0））
                        fsaccess=假；
                    db dbb（env->db env.get（），0）；
                    if（dbb.rename（strfileres.c_str（），nullptr，strfile.c_str（），0））
                        fsaccess=假；
                }
                如果（！）F成功）
                    logprintf（“Berkeleybatch:：rewrite:未能重写数据库文件%s\n”，strfileres）；
                返回fsaccess；
            }
        }
        毫秒睡眠（100）；
    }
}


void Berkeleyenvironment:：flush（bool fshutdown）
{
    int64_t instart=getTimeMillis（）；
    //将日志数据刷新到所有未使用文件的实际数据文件中
    logprint（bclog:：db，“Berkeleyenvironment:：flush:[%s]flush（%s）%s\n”，strPath，fshutdown？对：“错”，fdbenvinit？：“数据库未启动”）；
    如果（！）德本维尼）
        返回；
    {
        锁（CSYDB）；
        std:：map<std:：string，int>：：迭代器mi=mapfileuseCount.begin（）；
        （MI）！=mapfileusecount.end（））
            std:：string strfile=（*mi）.first；
            int nrefcount=（*mi）.second；
            logprint（bclog:：db，“Berkeleyenvironment:：flush:刷新%s（refcount=%d）…\n”，strfile，n refcount）；
            如果（nrefcount==0）
                //将日志数据移动到DAT文件
                closedb（strfile）；
                logprint（bclog:：db，“Berkeleyenvironment:：flush:%s checkpoint\n”，strfile）；
                dbenv->txn_检查点（0，0，0）；
                logprint（bclog:：db，“Berkeleyenvironment:：flush:%s detach\n”，strfile）；
                如果（！）FMOCKDB）
                    dbenv->lsn_reset（strfile.c_str（），0）；
                logprint（bclog:：db，“Berkeleyenvironment:：flush:%s closed\n”，strfile）；
                mapfileusecount.erase（mi++）；
            }其他
                Mi++；
        }
        logprint（bclog:：db，“Berkeleyenvironment:：flush:flush（%s）%s花了%15dms\n”，fshutdown？对：“错”，fdbenvinit？：“数据库未启动”，getTimeMillis（）-instart）；
        如果（关闭）
            ch**Listp；
            if（mapfileusecount.empty（））
                dbenv->log_archive（&listp，db_arch_remove）；
                关闭（）；
                如果（！）FMOCKDB）{
                    fs:：remove_all（fs:：path（strpath）/“数据库”）；
                }
            }
        }
    }
}

bool berkeleybatch:：periodicflush（berkeleydatabase&database）
{
    if（database.isdummy（））
        回归真实；
    }
    bool ret=假；
    Berkeleyenvironment*env=database.env；
    const std:：string&strfile=database.strfile；
    尝试锁定（cs-db，lock db）；
    如果（洛克德）
    {
        //如果正在使用任何数据库，则不执行此操作
        int nrefcount=0；
        std:：map<std:：string，int>：：迭代器mit=env->mapfileusecount.begin（）；
        （麻省理工学院）=env->mapfileusecount.end（））
        {
            nrefcount+=（*mit）.second；
            麻省理工学院+；
        }

        如果（nrefcount==0）
        {
            boost:：this_thread:：interrupt_point（）；
            std:：map<std:：string，int>：：迭代器mi=env->mapfileusecount.find（strfile）；
            如果（MI）！=env->mapfileusecount.end（））
            {
                logprint（bclog:：db，“刷新%s\n”，strfile）；
                int64_t instart=getTimeMillis（）；

                //刷新钱包文件，使其独立
                env->closedb（strfile）；
                env->checkpointlsn（strfile）；

                env->mapfileusecount.erase（mi++）；
                logprint（bclog:：db，“刷新了%s%dms\n”，strfile，getTimeMillis（）-instart）；
                RET＝真；
            }
        }
    }

    返回RET；
}

bool berkeleydatabase:：rewrite（const char*pszskip）
{
    返回berkeleybatch:：rewrite（*this，pszskip）；
}

bool berkeleydatabase:：backup（const std:：string&strdest）
{
    if（isSummy（））
        返回错误；
    }
    虽然（真）
    {
        {
            锁（CSYDB）；
            如果（！）env->mapfileusecount.count（strfile）env->mapfileusecount[strfile]==0）
            {
                //将日志数据刷新到DAT文件
                env->closedb（strfile）；
                env->checkpointlsn（strfile）；
                env->mapfileusecount.erase（strfile）；

                //复制钱包文件
                fs:：path pathsrc=env->directory（）/strfile；
                fs：：路径路径目标（strdest）；
                if（fs:：is_directory（pathdest））。
                    pathdest/=strfile；

                尝试{
                    if（fs:：equivalent（pathsrc，pathdest））
                        logprintf（“无法备份到钱包源文件%s\n”，pathdest.string（））；
                        返回错误；
                    }

                    fs:：copy_file（pathsrc，pathdest，fs:：copy_option:：overwrite_，如果存在）；
                    logprintf（“已将%s复制到%s\n”，strfile，pathdest.string（））；
                    回归真实；
                catch（const fs:：filesystem_error&e）
                    logprintf（“将%s复制到%s-%s\n”时出错，strfile，pathdest.string（），fsbridge:：get_filesystem_error_message（e））；
                    返回错误；
                }
            }
        }
        毫秒睡眠（100）；
    }
}

void berkeleydatabase:：flush（bool shutdown）
{
    如果（！）ISMUMMY（））{
        env->冲洗（关闭）；
        如果（关闭）{
            锁（CSYDB）；
            g_dbenvs.erase（env->directory（）.string（））；
            nulpPTR；
        }否则{
            //TODO:为了避免g摼dbenvs.erase在
            //在同一数据库中打开多个数据库时，第一次关闭数据库
            //环境，应将原始数据库“env”指针替换为共享或弱指针
            //指针，或者分离数据库和环境关闭，以便
            //可以在数据库之后关闭环境。
            env->m_fileid.erase（strfile）；
        }
    }
}

void BerkeleyDatabase:：ReloadBenv（）。
{
    如果（！）ISMUMMY（））{
        env->reloadbenv（）；
    }
}
