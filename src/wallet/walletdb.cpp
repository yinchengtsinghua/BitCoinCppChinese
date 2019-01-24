
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

#include <wallet/walletdb.h>

#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <fs.h>
#include <key_io.h>
#include <protocol.h>
#include <serialize.h>
#include <sync.h>
#include <util/system.h>
#include <util/time.h>
#include <wallet/wallet.h>

#include <atomic>
#include <string>

#include <boost/thread.hpp>

//
//墙纸批料
//

bool WalletBatch::WriteName(const std::string& strAddress, const std::string& strName)
{
    return WriteIC(std::make_pair(std::string("name"), strAddress), strName);
}

bool WalletBatch::EraseName(const std::string& strAddress)
{
//这只能用于发送地址，而不能用于接收地址，
//如果收件地址不更改退货，则收件地址必须始终具有通讯簿条目。
    return EraseIC(std::make_pair(std::string("name"), strAddress));
}

bool WalletBatch::WritePurpose(const std::string& strAddress, const std::string& strPurpose)
{
    return WriteIC(std::make_pair(std::string("purpose"), strAddress), strPurpose);
}

bool WalletBatch::ErasePurpose(const std::string& strAddress)
{
    return EraseIC(std::make_pair(std::string("purpose"), strAddress));
}

bool WalletBatch::WriteTx(const CWalletTx& wtx)
{
    return WriteIC(std::make_pair(std::string("tx"), wtx.GetHash()), wtx);
}

bool WalletBatch::EraseTx(uint256 hash)
{
    return EraseIC(std::make_pair(std::string("tx"), hash));
}

bool WalletBatch::WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata& keyMeta)
{
    if (!WriteIC(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta, false)) {
        return false;
    }

//hash pubkey/privkey加速钱包加载
    std::vector<unsigned char> vchKey;
    vchKey.reserve(vchPubKey.size() + vchPrivKey.size());
    vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
    vchKey.insert(vchKey.end(), vchPrivKey.begin(), vchPrivKey.end());

    return WriteIC(std::make_pair(std::string("key"), vchPubKey), std::make_pair(vchPrivKey, Hash(vchKey.begin(), vchKey.end())), false);
}

bool WalletBatch::WriteCryptedKey(const CPubKey& vchPubKey,
                                const std::vector<unsigned char>& vchCryptedSecret,
                                const CKeyMetadata &keyMeta)
{
    if (!WriteIC(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta)) {
        return false;
    }

    if (!WriteIC(std::make_pair(std::string("ckey"), vchPubKey), vchCryptedSecret, false)) {
        return false;
    }
    EraseIC(std::make_pair(std::string("key"), vchPubKey));
    EraseIC(std::make_pair(std::string("wkey"), vchPubKey));
    return true;
}

bool WalletBatch::WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey)
{
    return WriteIC(std::make_pair(std::string("mkey"), nID), kMasterKey, true);
}

bool WalletBatch::WriteCScript(const uint160& hash, const CScript& redeemScript)
{
    return WriteIC(std::make_pair(std::string("cscript"), hash), redeemScript, false);
}

bool WalletBatch::WriteWatchOnly(const CScript &dest, const CKeyMetadata& keyMeta)
{
    if (!WriteIC(std::make_pair(std::string("watchmeta"), dest), keyMeta)) {
        return false;
    }
    return WriteIC(std::make_pair(std::string("watchs"), dest), '1');
}

bool WalletBatch::EraseWatchOnly(const CScript &dest)
{
    if (!EraseIC(std::make_pair(std::string("watchmeta"), dest))) {
        return false;
    }
    return EraseIC(std::make_pair(std::string("watchs"), dest));
}

bool WalletBatch::WriteBestBlock(const CBlockLocator& locator)
{
WriteIC(std::string("bestblock"), CBlockLocator()); //编写空块定位器，以便需要merkle分支的版本自动重新扫描
    return WriteIC(std::string("bestblock_nomerkle"), locator);
}

bool WalletBatch::ReadBestBlock(CBlockLocator& locator)
{
    if (m_batch.Read(std::string("bestblock"), locator) && !locator.vHave.empty()) return true;
    return m_batch.Read(std::string("bestblock_nomerkle"), locator);
}

bool WalletBatch::WriteOrderPosNext(int64_t nOrderPosNext)
{
    return WriteIC(std::string("orderposnext"), nOrderPosNext);
}

bool WalletBatch::ReadPool(int64_t nPool, CKeyPool& keypool)
{
    return m_batch.Read(std::make_pair(std::string("pool"), nPool), keypool);
}

bool WalletBatch::WritePool(int64_t nPool, const CKeyPool& keypool)
{
    return WriteIC(std::make_pair(std::string("pool"), nPool), keypool);
}

bool WalletBatch::ErasePool(int64_t nPool)
{
    return EraseIC(std::make_pair(std::string("pool"), nPool));
}

bool WalletBatch::WriteMinVersion(int nVersion)
{
    return WriteIC(std::string("minversion"), nVersion);
}

class CWalletScanState {
public:
    unsigned int nKeys{0};
    unsigned int nCKeys{0};
    unsigned int nWatchKeys{0};
    unsigned int nKeyMeta{0};
    unsigned int m_unknown_records{0};
    bool fIsEncrypted{false};
    bool fAnyUnordered{false};
    int nFileVersion{0};
    std::vector<uint256> vWalletUpgrade;

    CWalletScanState() {
    }
};

static bool
ReadKeyValue(CWallet* pwallet, CDataStream& ssKey, CDataStream& ssValue,
             CWalletScanState &wss, std::string& strType, std::string& strErr) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet)
{
    try {
//非序列化
//利用成对序列化的事实
//只是两个项目一个接一个序列化
        ssKey >> strType;
        if (strType == "name")
        {
            std::string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->mapAddressBook[DecodeDestination(strAddress)].name;
        }
        else if (strType == "purpose")
        {
            std::string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->mapAddressBook[DecodeDestination(strAddress)].purpose;
        }
        else if (strType == "tx")
        {
            uint256 hash;
            ssKey >> hash;
            /*allettx wtx（nullptr/*pwallet*/，makeTransactionRef（））；
            StValux>
            cvalidationState状态；
            如果（！）（checktransaction（*wtx.tx，state）&&（wtx.gethash（）==hash）&&state.isvalid（））
                返回错误；

            //撤消31600中的序列化更改
            如果（31404<=wtx.ftimeReceiveDistXTime和wtx.ftimeReceiveDistXTime<=31703）
            {
                如果（！）ssvalue.empty（））
                {
                    炭纤维蛋白原；
                    烧焦；
                    std：：字符串未使用的_字符串；
                    ssvalue>>ftmp>>funuused>>未使用的_字符串；
                    strerr=strprintf（“loadwallet（）升级tx ver=%d%d%s”，
                                       wtx.ftimeReceiveDistXTime、ftmp、hash.toString（））；
                    wtx.ftimeReceiveDistXTime=ftmp；
                }
                其他的
                {
                    strerr=strprintf（“loadwallet（）正在修复tx ver=%d%s”，wtx.ftimeReceiveDistXTime，hash.toString（））；
                    wtx.ftimeReceiveDistXTime=0；
                }
                wss.vwalletupgrade.push_back（哈希）；
            }

            如果（wtx.norderpos=-1）
                wss.fanyunordered=真；

            pwallet->loadtowallet（wtx）；
        }
        else if（strType=“watchs”）。
        {
            wss.nwatcheys++；
            脚本脚本；
            sskey>>脚本；
            查尔菲斯；
            ssvalue>>fyes；
            如果（fyes='1'）
                pwallet->loadWatchOnly（脚本）；
        }
        else if（strtype==“key”strtype==“wkey”）
        {
            cpubkey-vchSubkey；
            sskey>>vchSubkey；
            如果（！）vchSubkey.isvalid（））
            {
                strerr=“读取钱包数据库时出错：cpubkey损坏”；
                返回错误；
            }
            密钥；
            CPrivKey pkey；
            uTIN 256散列；

            if（strType==“键”）
            {
                nKix++；
                ssvalue>>pkey；
            }否则{
                C所有钥匙开关；
                ssvalue>>wkey；
                pkey=wkey.vchprivkey；
            }

            //旧钱包将密钥存储为“key”[pubkey]=>[privkey]
            /…对于有很多钥匙的钱包来说，这是很慢的，因为公钥是从私钥派生而来的。
            //使用EC操作作为校验和。
            //新钱包将密钥存储为“key”[pubkey]=>[privkey][hash（pubkey，privkey）]，这比
            //保持向后兼容。
            尝试
            {
                ssvalue>>哈希；
            }
            catch（…）{}

            bool fskipcheck=false；

            如果（！）哈希.ISNULL（）
            {
                //hash pubkey/privkey加速钱包加载
                std:：vector<unsigned char>vchkey；
                vchkey.reserve（vchSubkey.size（）+pkey.size（））；
                vchkey.insert（vchkey.end（），vchSubkey.begin（），vchSubkey.end（））；
                vchkey.insert（vchkey.end（），pkey.begin（），pkey.end（））；

                如果（hash（vchkey.begin（），vchkey.end（））！=哈希）
                {
                    strerr=“读取钱包数据库时出错：cpubkey/cprivkey损坏”；
                    返回错误；
                }

                fskipcheck=真；
            }

            如果（！）key.load（pkey、vchsubkey、fskipcheck）
            {
                strerr=“读取钱包数据库时出错：cprivkey损坏”；
                返回错误；
            }
            如果（！）pwallet->loadkey（key，vchsubkey）
            {
                strerr=“读取钱包数据库时出错：加载密钥失败”；
                返回错误；
            }
        }
        else if（strType=“mkey”）。
        {
            无符号int nid；
            SSKEY > NID；
            C主销K主销；
            ssvalue>>kmasterkey；
            如果（pwallet->mapmasterkeys.count（nid）！= 0）
            {
                strerr=strprintf（“读取钱包数据库时出错：重复的cmasterkey id%u”，nid）；
                返回错误；
            }
            pwallet->mapmasterkeys[nid]=kmasterkey；
            如果（pwallet->nmasterkeymaxid<nid）
                pwallet->nmasterkeymaxid=nid；
        }
        else if（strType=“ckey”）。
        {
            cpubkey-vchSubkey；
            sskey>>vchSubkey；
            如果（！）vchSubkey.isvalid（））
            {
                strerr=“读取钱包数据库时出错：cpubkey损坏”；
                返回错误；
            }
            std:：vector<unsigned char>vchprivkey；
            ssvalue>>vchprivkey；
            NCKEXK++；

            如果（！）pwallet->loadcryptedkey（vchSubkey、vchDrivekey）
            {
                strerr=“读取钱包数据库时出错：LoadCryptedKey失败”；
                返回错误；
            }
            wss.fisencrypted=true；
        }
        else if（strType=“keymeta”）。
        {
            cpubkey-vchSubkey；
            sskey>>vchSubkey；
            ckeymetadata键元；
            ssvalue>>键元；
            NKEYMETA+ +；
            pwallet->loadKeyMetadata（vchSubkey.getID（），keymeta）；
        }
        else if（strtype==“watchmeta”）
        {
            脚本脚本；
            sskey>>脚本；
            ckeymetadata键元；
            ssvalue>>键元；
            NKEYMETA+ +；
            pwallet->loadscriptmetadata（cscriptid（script），keymeta）；
        }
        else if（strType==“默认键”）
        {
            //我们不需要或不需要默认的键，但是如果有一组，
            //我们要确保它是有效的，以便能够检测到损坏
            cpubkey-vchSubkey；
            ssvalue>>vchSubkey；
            如果（！）vchSubkey.isvalid（））
                strerr=“读取钱包数据库时出错：默认密钥损坏”；
                返回错误；
            }
        }
        else if（strtype==“池”）
        {
            n64指数；
            sskey>>nindex；
            ckeyPool密钥池；
            ssvalue>>键池；

            pwallet->loadkeypool（nindex，keypool）；
        }
        else if（strtype==“版本”）
        {
            ssvalue>>wss.nfileversion；
            if（wss.nfileversion==10300）
                wss.nfileversion=300；
        }
        else if（strtype==“cscript”）。
        {
            UTIN 160散列；
            SSKEY>散列；
            脚本脚本；
            ssvalue>>脚本；
            如果（！）pwallet->loadcscript（脚本）
            {
                strerr=“读取钱包数据库时出错：loadcscript失败”；
                返回错误；
            }
        }
        else if（strType==“orderPosNext”）。
        {
            ssvalue>>pwallet->norderposnext；
        }
        else if（strtype==“目标数据”）
        {
            std:：string跨地址，strkey，strvalue；
            sskey>>跨装；
            sskey>>strkey；
            ssvalue>>strvalue；
            pwallet->loadDestData（decodeDestination（straddress），strkey，strvalue）；
        }
        else if（strtype==“hdchain”）
        {
            链链；
            ssvalue>>链条；
            pwallet->sethdchain（链，真）；
        else if（strtype=“flags”）
            UIT64 64旗；
            ssvalue>>标志；
            如果（！）pwallet->setwalletflags（flags，true））
                strerr=“读取钱包数据库时出错：发现未知的不可容忍钱包标志”；
                返回错误；
            }
        否则，如果（strtype！=“最佳块”&&strType！=“最佳块”&&
                斯特里特！=“minVersion”&&strType！=“AcEnter”）{
            wss.m_未知_记录+
        }
    抓住（…）
    {
        返回错误；
    }
    回归真实；
}

bool walletbatch:：iskeytype（const std:：string&strtype）
{
    返回（strtype==“key”strtype==“wkey”
            strtype==“mkey”strtype==“ckey”）；
}

dberrors walletbatch:：loadwallet（cwallet*pwallet）
{
    cwalletchanstate-wss；
    bool fnoncriticalErrors=false；
    dberrors result=dberrors：：加载_OK；

    锁（pwallet->cs_wallet）；
    尝试{
        int nminversion=0；
        if（m_batch.read（（std:：string）“minversion”，nminversion））。
        {
            if（nminversion>feature_latest）
                返回dberrors:：too_new；
            pwallet->loadminversion（nminversion）；
        }

        //获取光标
        dbc*pcursor=m_batch.getcursor（）；
        如果（！）光标）
        {
            pwallet->walletlogprintf（“获取wallet数据库光标时出错\n”）；
            返回dberrors:：corrupt；
        }

        虽然（真）
        {
            //读取下一条记录
            cdatastream sskey（ser_disk，client_version）；
            cdatastream ssvalue（ser_disk，client_version）；
            int ret=m_batch.readatcursor（pcursor、sskey、ssvalue）；
            如果（ret==db_notfound）
                断裂；
            否则如果（RET）！= 0）
            {
                pwallet->walletlogprintf（“从wallet数据库读取下一条记录时出错”）；
                返回dberrors:：corrupt；
            }

            //尝试容忍单个损坏的记录：
            std：：字符串strtype，strerr；
            如果（！）readkeyvalue（pwallet、sskey、ssvalue、wss、strtype、strrr）
            {
                //丢失密钥被视为灾难性错误，其他任何错误
                //我们假设用户可以：
                if（iskeytype（strtype）strtype==“defaultkey”）
                    结果=dberrors:：corrupt；
                else if（strtype=“flags”）
                    //只有存在未知标志时，才能读取钱包标志
                    结果=dberrors：：太新；
                }否则{
                    //别管其他错误，如果我们试图修复它们，可能会使情况更糟。
                    fnonCriticalErrors=真；/…但是一定要警告用户有什么问题。
                    如果（strType=“tx”）
                        //如果存在错误的事务记录，则重新扫描：
                        gargs.softsetboolarg（“-rescan”，真）；
                }
            }
            如果（！）StRel.Enter（）
                pwallet->walletlogprintf（“%s\n”，strerr）；
        }
        pcursor->close（）；
    }
    catch（const boost:：thread_interrupted&）
        投掷；
    }
    渔获量（…）
        结果=dberrors:：corrupt；
    }

    if（fnonCriticalErrors&&result==dbErrors:：Load_OK）
        结果=dberrors：：非关键_错误；

    //任何钱包损坏：跳过任何重写或
    //升级，我们不想让情况更糟。
    如果（结果）！=dberrors：：加载_OK）
        返回结果；

    pwallet->walletlogprintf（“nfileversion=%d\n”，wss.nfileversion）；

    pwallet->walletlogprintf（“密钥：%u明文，u加密，u w/元数据，共%u个）。未知钱包记录：%u\n“，
           wss.nkeys、wss.nckeys、wss.nkeymeta、wss.nkeys+wss.nckeys、wss.m_未知记录）；

    //只有当所有键都有元数据时，ntimefirstkey才是可靠的
    如果（（wss.nkeys+wss.nckeys+wss.nwatcheys）！= WSS.NKEYMETA
        pwallet->updatetimefirstkey（1）；

    for（const uint256&hash:wss.vwalletupgrade）
        writetx（pwallet->mapwallet.at（hash））；

    //重写0.4.0和0.5.0rc版本的加密钱包：
    if（wss.fisencrypted&&（wss.nfileversion==40000 wss.nfileversion==50000））
        返回dberrors：：需要重写；

    if（wss.nfileversion<client_version）//更新
        writeversion（客户端版本）；

    if（wss.fanyun订购）
        结果=pWallet->ReorderTransactions（）；

    返回结果；
}

dberrors walletbatch:：findwallettx（std:：vector<uint256>&vtxhash，std:：vector<cwallettx>&vwtx）
{
    dberrors result=dberrors：：加载_OK；

    尝试{
        int nminversion=0；
        if（m_batch.read（（std:：string）“minversion”，nminversion））。
        {
            if（nminversion>feature_latest）
                返回dberrors:：too_new；
        }

        //获取光标
        dbc*pcursor=m_batch.getcursor（）；
        如果（！）光标）
        {
            logprintf（“获取钱包数据库光标时出错\n”）；
            返回dberrors:：corrupt；
        }

        虽然（真）
        {
            //读取下一条记录
            cdatastream sskey（ser_disk，client_version）；
            cdatastream ssvalue（ser_disk，client_version）；
            int ret=m_batch.readatcursor（pcursor、sskey、ssvalue）；
            如果（ret==db_notfound）
                断裂；
            否则如果（RET）！= 0）
            {
                logprintf（“从wallet数据库读取下一条记录时出错\n”）；
                返回dberrors:：corrupt；
            }

            std：：字符串strtype；
            sskey>>strtype；
            if（strType=“tx”）
                uTIN 256散列；
                SSKEY>散列；

                cwallettx wtx（nullptr/*pwallet）*/, MakeTransactionRef());

                ssValue >> wtx;

                vTxHash.push_back(hash);
                vWtx.push_back(wtx);
            }
        }
        pcursor->close();
    }
    catch (const boost::thread_interrupted&) {
        throw;
    }
    catch (...) {
        result = DBErrors::CORRUPT;
    }

    return result;
}

DBErrors WalletBatch::ZapSelectTx(std::vector<uint256>& vTxHashIn, std::vector<uint256>& vTxHashOut)
{
//创建钱包TXS和哈希列表
    std::vector<uint256> vTxHash;
    std::vector<CWalletTx> vWtx;
    DBErrors err = FindWalletTx(vTxHash, vWtx);
    if (err != DBErrors::LOAD_OK) {
        return err;
    }

    std::sort(vTxHash.begin(), vTxHash.end());
    std::sort(vTxHashIn.begin(), vTxHashIn.end());

//删除每个匹配的钱包tx
    bool delerror = false;
    std::vector<uint256>::iterator it = vTxHashIn.begin();
    for (const uint256& hash : vTxHash) {
        while (it < vTxHashIn.end() && (*it) < hash) {
            it++;
        }
        if (it == vTxHashIn.end()) {
            break;
        }
        else if ((*it) == hash) {
            if(!EraseTx(hash)) {
                LogPrint(BCLog::DB, "Transaction was found for deletion but returned database error: %s\n", hash.GetHex());
                delerror = true;
            }
            vTxHashOut.push_back(hash);
        }
    }

    if (delerror) {
        return DBErrors::CORRUPT;
    }
    return DBErrors::LOAD_OK;
}

DBErrors WalletBatch::ZapWalletTx(std::vector<CWalletTx>& vWtx)
{
//创建钱包txs列表
    std::vector<uint256> vTxHash;
    DBErrors err = FindWalletTx(vTxHash, vWtx);
    if (err != DBErrors::LOAD_OK)
        return err;

//删除每个钱包tx
    for (const uint256& hash : vTxHash) {
        if (!EraseTx(hash))
            return DBErrors::CORRUPT;
    }

    return DBErrors::LOAD_OK;
}

void MaybeCompactWalletDB()
{
    static std::atomic<bool> fOneThread(false);
    if (fOneThread.exchange(true)) {
        return;
    }
    if (!gArgs.GetBoolArg("-flushwallet", DEFAULT_FLUSHWALLET)) {
        return;
    }

    for (const std::shared_ptr<CWallet>& pwallet : GetWallets()) {
        WalletDatabase& dbh = pwallet->GetDBHandle();

        unsigned int nUpdateCounter = dbh.nUpdateCounter;

        if (dbh.nLastSeen != nUpdateCounter) {
            dbh.nLastSeen = nUpdateCounter;
            dbh.nLastWalletUpdate = GetTime();
        }

        if (dbh.nLastFlushed != nUpdateCounter && GetTime() - dbh.nLastWalletUpdate >= 2) {
            if (BerkeleyBatch::PeriodicFlush(dbh)) {
                dbh.nLastFlushed = nUpdateCounter;
            }
        }
    }

    fOneThread = false;
}

//
//尽量（非常小心！）如果有问题，请恢复钱包文件。
//
bool WalletBatch::Recover(const fs::path& wallet_path, void *callbackDataIn, bool (*recoverKVcallback)(void* callbackData, CDataStream ssKey, CDataStream ssValue), std::string& out_backup_filename)
{
    return BerkeleyBatch::Recover(wallet_path, callbackDataIn, recoverKVcallback, out_backup_filename);
}

bool WalletBatch::Recover(const fs::path& wallet_path, std::string& out_backup_filename)
{
//不使用密钥筛选器回调进行恢复
//恢复所有记录类型的结果
    return WalletBatch::Recover(wallet_path, nullptr, nullptr, out_backup_filename);
}

bool WalletBatch::RecoverKeysOnlyFilter(void *callbackData, CDataStream ssKey, CDataStream ssValue)
{
    CWallet *dummyWallet = reinterpret_cast<CWallet*>(callbackData);
    CWalletScanState dummyWss;
    std::string strType, strErr;
    bool fReadOK;
    {
//在loadKeyMetadata（）中是必需的：
        LOCK(dummyWallet->cs_wallet);
        fReadOK = ReadKeyValue(dummyWallet, ssKey, ssValue,
                               dummyWss, strType, strErr);
    }
    if (!IsKeyType(strType) && strType != "hdchain")
        return false;
    if (!fReadOK)
    {
        LogPrintf("WARNING: WalletBatch::Recover skipping %s: %s\n", strType, strErr);
        return false;
    }

    return true;
}

bool WalletBatch::VerifyEnvironment(const fs::path& wallet_path, std::string& errorStr)
{
    return BerkeleyBatch::VerifyEnvironment(wallet_path, errorStr);
}

bool WalletBatch::VerifyDatabaseFile(const fs::path& wallet_path, std::string& warningStr, std::string& errorStr)
{
    return BerkeleyBatch::VerifyDatabaseFile(wallet_path, warningStr, errorStr, WalletBatch::Recover);
}

bool WalletBatch::WriteDestData(const std::string &address, const std::string &key, const std::string &value)
{
    return WriteIC(std::make_pair(std::string("destdata"), std::make_pair(address, key)), value);
}

bool WalletBatch::EraseDestData(const std::string &address, const std::string &key)
{
    return EraseIC(std::make_pair(std::string("destdata"), std::make_pair(address, key)));
}


bool WalletBatch::WriteHDChain(const CHDChain& chain)
{
    return WriteIC(std::string("hdchain"), chain);
}

bool WalletBatch::WriteWalletFlags(const uint64_t flags)
{
    return WriteIC(std::string("flags"), flags);
}

bool WalletBatch::TxnBegin()
{
    return m_batch.TxnBegin();
}

bool WalletBatch::TxnCommit()
{
    return m_batch.TxnCommit();
}

bool WalletBatch::TxnAbort()
{
    return m_batch.TxnAbort();
}

bool WalletBatch::ReadVersion(int& nVersion)
{
    return m_batch.ReadVersion(nVersion);
}

bool WalletBatch::WriteVersion(int nVersion)
{
    return m_batch.WriteVersion(nVersion);
}
