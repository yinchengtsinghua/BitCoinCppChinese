
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

#ifndef BITCOIN_WALLET_WALLET_H
#define BITCOIN_WALLET_WALLET_H

#include <amount.h>
#include <interfaces/chain.h>
#include <outputtype.h>
#include <policy/feerate.h>
#include <streams.h>
#include <tinyformat.h>
#include <ui_interface.h>
#include <util/strencodings.h>
#include <validationinterface.h>
#include <script/ismine.h>
#include <script/sign.h>
#include <util/system.h>
#include <wallet/crypter.h>
#include <wallet/coinselection.h>
#include <wallet/walletdb.h>
#include <wallet/walletutil.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

//！负责读取和验证-wallet参数和验证wallet数据库。
//如果需要，此功能将对钱包执行打捞，只要只有一个钱包
//正在加载（WalletParameterInteraction禁止-SalvageWallet、-ZapAllettXes或-UpgradeWallet和MultiWallet）。
bool VerifyWallets(interfaces::Chain& chain, const std::vector<std::string>& wallet_files);

//！加载钱包数据库。
bool LoadWallets(interfaces::Chain& chain, const std::vector<std::string>& wallet_files);

//！完成钱包的启动。
void StartWallets(CScheduler& scheduler);

//！冲洗所有钱包，准备关机。
void FlushWallets();

//！停止所有钱包。首先要冲洗钱包。
void StopWallets();

//！关闭所有钱包。
void UnloadWallets();

//！明确卸载和删除钱包。
//在发出卸载意图的信号后阻止当前线程，以便
//钱包客户释放钱包。
//注意，当不需要阻塞时，钱包会被隐式卸载。
//共享指针删除程序。
void UnloadWallet(std::shared_ptr<CWallet>&& wallet);

bool AddWallet(const std::shared_ptr<CWallet>& wallet);
bool RemoveWallet(const std::shared_ptr<CWallet>& wallet);
bool HasWallets();
std::vector<std::shared_ptr<CWallet>> GetWallets();
std::shared_ptr<CWallet> GetWallet(const std::string& name);

//！-keypool的默认值
static const unsigned int DEFAULT_KEYPOOL_SIZE = 1000;
//！-支付x费用默认值
constexpr CAmount DEFAULT_PAY_TX_FEE = 0;
//！-回退费默认值
static const CAmount DEFAULT_FALLBACK_FEE = 20000;
//！-报废费默认值
static const CAmount DEFAULT_DISCARD_FEE = 10000;
//！-mintxfee默认值
static const CAmount DEFAULT_TRANSACTION_MINFEE = 1000;
//！BIP 125更换TXS的最小建议增量
static const CAmount WALLET_INCREMENTAL_RELAY_FEE = 5000;
//！-spendzeroconfchange的默认值
static const bool DEFAULT_SPEND_ZEROCONF_CHANGE = true;
//！-walletrejectlongchains的默认值
static const bool DEFAULT_WALLET_REJECT_LONG_CHAINS = false;
//！-avoidPartialPends的默认值
static const bool DEFAULT_AVOIDPARTIALSPENDS = false;
//！-txconfirmtarget默认值
static const unsigned int DEFAULT_TX_CONFIRM_TARGET = 6;
//！-walletrbf默认值
static const bool DEFAULT_WALLET_RBF = false;
static const bool DEFAULT_WALLETBROADCAST = true;
static const bool DEFAULT_DISABLE_WALLET = false;

//！用于输入大小估计的预计算常量*虚拟大小*
static constexpr size_t DUMMY_NESTED_P2WPKH_INPUT_SIZE = 91;

class CBlockIndex;
class CCoinControl;
class COutput;
class CReserveKey;
class CScript;
class CTxMemPool;
class CBlockPolicyEstimator;
class CWalletTx;
struct FeeCalculation;
enum class FeeEstimateMode;

/*（客户端）特定钱包功能的版本号*/
enum WalletFeature
{
FEATURE_BASE = 10500, //最早版本的新钱包支持（仅对getwalletinfo的clientversion输出有用）

FEATURE_WALLETCRYPT = 40000, //钱包加密
FEATURE_COMPRPUBKEY = 60000, //压缩公钥

FEATURE_HD = 130000, //Bip32（高清钱包）后的分层密钥推导

FEATURE_HD_SPLIT = 139900, //带高清链拆分的钱包（更改输出将使用m/0'/1'/k）

FEATURE_NO_DEFAULT_KEY = 159900, //没有写默认密钥的钱包

FEATURE_PRE_SPLIT_KEYPOOL = 169900, //升级到HD Split，可以有预拆分的密钥池

    FEATURE_LATEST = FEATURE_PRE_SPLIT_KEYPOOL
};

//！-addresstype的默认值
constexpr OutputType DEFAULT_ADDRESS_TYPE{OutputType::P2SH_SEGWIT};

//！-changetype的默认值
constexpr OutputType DEFAULT_CHANGE_TYPE{OutputType::CHANGE_AUTO};

enum WalletFlags : uint64_t {
//如果标志未知，上方（>1<<31）的钱包标志将导致无法打开钱包。
//下方的未知钱包标志<=（1<<31）将被接受。

//将强制执行钱包不能包含任何私钥的规则（仅限观看/发布密钥）
    WALLET_FLAG_DISABLE_PRIVATE_KEYS = (1ULL << 32),
};

static constexpr uint64_t g_known_wallet_flags = WALLET_FLAG_DISABLE_PRIVATE_KEYS;

/*密钥池条目*/
class CKeyPool
{
public:
    int64_t nTime;
    CPubKey vchPubKey;
bool fInternal; //用于更改输出
bool m_pre_split; //对于在密钥池拆分升级之前生成的密钥

    CKeyPool();
    CKeyPool(const CPubKey& vchPubKeyIn, bool internalIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(nTime);
        READWRITE(vchPubKey);
        if (ser_action.ForRead()) {
            try {
                READWRITE(fInternal);
            }
            catch (std::ios_base::failure&) {
                /*如果无法读取内部布尔值，则标记为外部地址
                   （这适用于高清链拆分版本之前的任何钱包）*/

                fInternal = false;
            }
            try {
                READWRITE(m_pre_split);
            }
            catch (std::ios_base::failure&) {
                /*如果无法读取m_pre_split布尔值，则标记为postsplit地址
                   （任何升级到高清链拆分的钱包都是如此*/

                m_pre_split = false;
            }
        }
        else {
            READWRITE(fInternal);
            READWRITE(m_pre_split);
        }
    }
};

/*通讯簿数据*/
class CAddressBookData
{
public:
    std::string name;
    std::string purpose;

    CAddressBookData() : purpose("unknown") {}

    typedef std::map<std::string, std::string> StringMap;
    StringMap destdata;
};

struct CRecipient
{
    CScript scriptPubKey;
    CAmount nAmount;
    bool fSubtractFeeFromAmount;
};

typedef std::map<std::string, std::string> mapValue_t;


static inline void ReadOrderPos(int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (!mapValue.count("n"))
    {
nOrderPos = -1; //TODO:在别处计算
        return;
    }
    nOrderPos = atoi64(mapValue["n"].c_str());
}


static inline void WriteOrderPos(const int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (nOrderPos == -1)
        return;
    mapValue["n"] = i64tostr(nOrderPos);
}

struct COutputEntry
{
    CTxDestination destination;
    CAmount amount;
    int vout;
};

/*一种带有Merkle分支的事务，将其链接到块链。*/
class CMerkleTx
{
private:
  /*hashblock中用于指示tx已被放弃的常量*/
    static const uint256 ABANDON_HASH;

public:
    CTransactionRef tx;
    uint256 hashBlock;

    /*nindex=-1表示hashblock（非零）指的是最早的
     *我们知道这一点或任何钱包内的依赖冲突。
     *。较旧的客户端将nindex=-1解释为向后未确认
     *兼容性。
     **/

    int nIndex;

    CMerkleTx()
    {
        SetTx(MakeTransactionRef());
        Init();
    }

    explicit CMerkleTx(CTransactionRef arg)
    {
        SetTx(std::move(arg));
        Init();
    }

    void Init()
    {
        hashBlock = uint256();
        nIndex = -1;
    }

    void SetTx(CTransactionRef arg)
    {
        tx = std::move(arg);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
std::vector<uint256> vMerkleBranch; //与旧版本兼容。
        READWRITE(tx);
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    }

    void SetMerkleBranch(const CBlockIndex* pIndex, int posInBlock);

    /*
     *区块链交易返回深度：
     *<0：与区块链深处的交易冲突
     *0:内存池中，等待包含在块中
     *>=1：主链深处有这么多块
     **/

    int GetDepthInMainChain(interfaces::Chain::Lock& locked_chain) const;
    bool IsInMainChain(interfaces::Chain::Lock& locked_chain) const { return GetDepthInMainChain(locked_chain) > 0; }

    /*
     *@返回此交易到期的块数：
     *0:不是CoinBase交易，或是成熟的CoinBase交易
     *>0:是一个CoinBase事务，在这许多块中到期。
     **/

    int GetBlocksToMaturity(interfaces::Chain::Lock& locked_chain) const;
    bool hashUnset() const { return (hashBlock.IsNull() || hashBlock == ABANDON_HASH); }
    bool isAbandoned() const { return (hashBlock == ABANDON_HASH); }
    void setAbandoned() { hashBlock = ABANDON_HASH; }

    const uint256& GetHash() const { return tx->GetHash(); }
    bool IsCoinBase() const { return tx->IsCoinBase(); }
    bool IsImmatureCoinBase(interfaces::Chain::Lock& locked_chain) const;
};

//获取花费指定输出的边际字节
int CalculateMaximumSignedInputSize(const CTxOut& txout, const CWallet* pwallet, bool use_max_sig = false);

/*
 *一个包含大量附加信息的事务，只有所有者才关心这些信息。
 *它包括将其链接回区块链所需的任何未记录的交易。
 **/

class CWalletTx : public CMerkleTx
{
private:
    const CWallet* pwallet;

public:
    /*
     *带有交易信息的键/值映射。
     *
     *以下键可以通过地图读取和写入，并且
     *在钱包数据库中序列化：
     *
     *“comment”，“to”-为sendtoAddress提供的注释字符串，
     *并发送许多钱包RPC
     *“替换”事务的txid“-txid（作为hexstr）替换为
     *由bumpfee创建的交易的bumpfee
     *“replaced_by_txid”-txid（as hexstr）of transaction created by
     *交易的bumpfee替换为bumpfee
     *“From”，“Message”-可以在用户界面中设置的过时字段
     *2011（在提交4D9B223中删除）
     *
     *以下密钥在钱包数据库中序列化，但不应
     *通过地图阅读或书写（它们将被临时添加和
     *在序列化过程中从映射中删除）：
     *
     *“FromAccount”-序列化的strFromAccount值
     *“n”-序列化norderpos值
     *“Timesmart”-序列化的ntimesmart值
     *“已用”-之前存在的序列化vfstreed值
     *2014（在提交93A18A3中删除）
     **/

    mapValue_t mapValue;
    std::vector<std::pair<std::string, std::string> > vOrderForm;
    unsigned int fTimeReceivedIsTxTime;
unsigned int nTimeReceived; //！<此节点接收的时间
    /*
     *稳定的时间戳，永不更改，并反映事务的顺序
     *已添加到钱包中。时间戳基于
     *作为块的一部分添加的事务，或者
     *如果事务不是块的一部分，则接收事务，时间戳为
     *在这两种情况下都进行了调整，以便时间戳顺序与订单事务匹配。
     *已添加到钱包中。有关更多详细信息，请参见
     *cwallet:：ComputeTimeSmart（）。
     **/

    unsigned int nTimeSmart;
    /*
     *对于钱包创建的交易，From Me标志设置为1。
     *在此比特币节点上，对于创建的交易设置为0
     *通过网络或sendrawtransaction rpc从外部传入。
     **/

    char fFromMe;
int64_t nOrderPos; //！<订单交易列表中的位置
    std::multimap<int64_t, CWalletTx*>::const_iterator m_it_wtxOrdered;

//仅记忆
    mutable bool fDebitCached;
    mutable bool fCreditCached;
    mutable bool fImmatureCreditCached;
    mutable bool fAvailableCreditCached;
    mutable bool fWatchDebitCached;
    mutable bool fWatchCreditCached;
    mutable bool fImmatureWatchCreditCached;
    mutable bool fAvailableWatchCreditCached;
    mutable bool fChangeCached;
    mutable bool fInMempool;
    mutable CAmount nDebitCached;
    mutable CAmount nCreditCached;
    mutable CAmount nImmatureCreditCached;
    mutable CAmount nAvailableCreditCached;
    mutable CAmount nWatchDebitCached;
    mutable CAmount nWatchCreditCached;
    mutable CAmount nImmatureWatchCreditCached;
    mutable CAmount nAvailableWatchCreditCached;
    mutable CAmount nChangeCached;

    CWalletTx(const CWallet* pwalletIn, CTransactionRef arg) : CMerkleTx(std::move(arg))
    {
        Init(pwalletIn);
    }

    void Init(const CWallet* pwalletIn)
    {
        pwallet = pwalletIn;
        mapValue.clear();
        vOrderForm.clear();
        fTimeReceivedIsTxTime = false;
        nTimeReceived = 0;
        nTimeSmart = 0;
        fFromMe = false;
        fDebitCached = false;
        fCreditCached = false;
        fImmatureCreditCached = false;
        fAvailableCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fChangeCached = false;
        fInMempool = false;
        nDebitCached = 0;
        nCreditCached = 0;
        nImmatureCreditCached = 0;
        nAvailableCreditCached = 0;
        nWatchDebitCached = 0;
        nWatchCreditCached = 0;
        nAvailableWatchCreditCached = 0;
        nImmatureWatchCreditCached = 0;
        nChangeCached = 0;
        nOrderPos = -1;
    }

    template<typename Stream>
    void Serialize(Stream& s) const
    {
        char fSpent = false;
        mapValue_t mapValueCopy = mapValue;

        mapValueCopy["fromaccount"] = "";
        WriteOrderPos(nOrderPos, mapValueCopy);
        if (nTimeSmart) {
            mapValueCopy["timesmart"] = strprintf("%u", nTimeSmart);
        }

        s << static_cast<const CMerkleTx&>(*this);
std::vector<CMerkleTx> vUnused; //！<以前是vtxprev
        s << vUnused << mapValueCopy << vOrderForm << fTimeReceivedIsTxTime << nTimeReceived << fFromMe << fSpent;
    }

    template<typename Stream>
    void Unserialize(Stream& s)
    {
        Init(nullptr);
        char fSpent;

        s >> static_cast<CMerkleTx&>(*this);
std::vector<CMerkleTx> vUnused; //！<以前是vtxprev
        s >> vUnused >> mapValue >> vOrderForm >> fTimeReceivedIsTxTime >> nTimeReceived >> fFromMe >> fSpent;

        ReadOrderPos(nOrderPos, mapValue);
        nTimeSmart = mapValue.count("timesmart") ? (unsigned int)atoi64(mapValue["timesmart"]) : 0;

        mapValue.erase("fromaccount");
        mapValue.erase("spent");
        mapValue.erase("n");
        mapValue.erase("timesmart");
    }

//！确保重新计算余额
    void MarkDirty()
    {
        fCreditCached = false;
        fAvailableCreditCached = false;
        fImmatureCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fDebitCached = false;
        fChangeCached = false;
    }

    void BindWallet(CWallet *pwalletIn)
    {
        pwallet = pwalletIn;
        MarkDirty();
    }

//！筛选决定哪些地址将计入借方
    CAmount GetDebit(const isminefilter& filter) const;
    CAmount GetCredit(interfaces::Chain::Lock& locked_chain, const isminefilter& filter) const;
    CAmount GetImmatureCredit(interfaces::Chain::Lock& locked_chain, bool fUseCache=true) const;
//TODO:删除“无线程安全分析”，并将其替换为正确的
//注释“需要专用锁”（cs_-main，pwallet->cs_-wallet）。这个
//临时添加注释“无螺纹安全分析”以避免
//必须解决成员访问不完整类型cwallet的问题。
    CAmount GetAvailableCredit(interfaces::Chain::Lock& locked_chain, bool fUseCache=true, const isminefilter& filter=ISMINE_SPENDABLE) const NO_THREAD_SAFETY_ANALYSIS;
    CAmount GetImmatureWatchOnlyCredit(interfaces::Chain::Lock& locked_chain, const bool fUseCache=true) const;
    CAmount GetChange() const;

//如果花费此事务的指定输出，则获取边际字节
    int GetSpendSize(unsigned int out, bool use_max_sig = false) const
    {
        return CalculateMaximumSignedInputSize(tx->vout[out], pwallet, use_max_sig);
    }

    void GetAmounts(std::list<COutputEntry>& listReceived,
                    std::list<COutputEntry>& listSent, CAmount& nFee, const isminefilter& filter) const;

    bool IsFromMe(const isminefilter& filter) const
    {
        return (GetDebit(filter) > 0);
    }

//如果只有脚本信号不同，则为true
    bool IsEquivalentTo(const CWalletTx& tx) const;

    bool InMempool() const;
    bool IsTrusted(interfaces::Chain::Lock& locked_chain) const;

    int64_t GetTxTime() const;

//只有在FBRoadcastTransactions时才能调用RelaywalletTransaction！
    bool RelayWalletTransaction(interfaces::Chain::Lock& locked_chain, CConnman* connman);

    /*将此事务传递到mempool。如果绝对费用超过荒谬费用，则失败。*/
    bool AcceptToMemoryPool(interfaces::Chain::Lock& locked_chain, const CAmount& nAbsurdFee, CValidationState& state);

//TODO:删除“无线程安全分析”，并将其替换为正确的
//注释“需要专用锁（pwallet->cs_钱包）”。注释
//临时添加了“无线程安全分析”，以避免
//解决成员访问不完整类型cwallet的问题。注释
//我们还有运行时检查“assertlockhold（pwallet->cs_wallet）”。
//就位。
    std::set<uint256> GetConflicts() const NO_THREAD_SAFETY_ANALYSIS;
};

class COutput
{
public:
    const CWalletTx *tx;
    int i;
    int nDepth;

    /*作为事务中的完全签名输入，预先计算出此输出的估计大小。如果无法计算，则可以为-1*/
    int nInputBytes;

    /*是否有私钥来使用此输出*/
    bool fSpendable;

    /*我们是否知道如何使用这个输出，忽略缺少键*/
    bool fSolvable;

    /*在计算输入开销的大小时，是否使用最大大小的72字节签名。仅当允许仅监视输出时才应设置此选项。*/
    bool use_max_sig;

    /*
     *该产出是否被认为是安全消费。未确认的交易
     *考虑从外部密钥和未确认的替换事务
     *不安全，不会用于资助新的支出交易。
     **/

    bool fSafe;

    COutput(const CWalletTx *txIn, int iIn, int nDepthIn, bool fSpendableIn, bool fSolvableIn, bool fSafeIn, bool use_max_sig_in = false)
    {
        tx = txIn; i = iIn; nDepth = nDepthIn; fSpendable = fSpendableIn; fSolvable = fSolvableIn; fSafe = fSafeIn; nInputBytes = -1; use_max_sig = use_max_sig_in;
//如果已知并可由给定钱包签名，则计算9个输入字节
//失败将保留该值-1
        if (fSpendable && tx) {
            nInputBytes = tx->GetSpendSize(i, use_max_sig);
        }
    }

    std::string ToString() const;

    inline CInputCoin GetInputCoin() const
    {
        return CInputCoin(tx->tx, i, nInputBytes);
    }
};

/*包含过期日期以防永远无法使用的私钥。*/
class CWalletKey
{
public:
    CPrivKey vchPrivKey;
    int64_t nTimeCreated;
    int64_t nTimeExpires;
    std::string strComment;
//！TODO:添加一些内容以记录创建它的内容（user、getnewaddress、change）
//！可能应该有一个map<string，string>property map

    explicit CWalletKey(int64_t nExpires=0);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPrivKey);
        READWRITE(nTimeCreated);
        READWRITE(nTimeExpires);
        READWRITE(LIMITED_STRING(strComment, 65536));
    }
};

struct CoinSelectionParams
{
    bool use_bnb = true;
    size_t change_output_size = 0;
    size_t change_spend_size = 0;
    CFeeRate effective_fee = CFeeRate(0);
    size_t tx_noinputs_size = 0;

    CoinSelectionParams(bool use_bnb, size_t change_output_size, size_t change_spend_size, CFeeRate effective_fee, size_t tx_noinputs_size) : use_bnb(use_bnb), change_output_size(change_output_size), change_spend_size(change_spend_size), effective_fee(effective_fee), tx_noinputs_size(tx_noinputs_size) {}
    CoinSelectionParams() {}
};

class WalletRescanReserver; //转发scanforwallettransactions/rescanfromtime的声明
/*
 *cwallet是密钥库的扩展，它还维护一组事务和平衡，
 *并提供创建新交易的能力。
 **/

class CWallet final : public CCryptoKeyStore, public CValidationInterface
{
private:
    std::atomic<bool> fAbortRescan{false};
std::atomic<bool> fScanningWallet{false}; //由WalletRescanReserver控制
    std::mutex mutexScanning;
    friend class WalletRescanReserver;

    WalletBatch *encrypted_batch GUARDED_BY(cs_wallet) = nullptr;

//！当前钱包版本：低于此版本的客户端无法加载钱包
    int nWalletVersion = FEATURE_BASE;

//！最大钱包格式版本：仅内存变量，指定此钱包可以升级到什么版本
    int nWalletMaxVersion GUARDED_BY(cs_wallet) = FEATURE_BASE;

    int64_t nNextResend = 0;
    int64_t nLastResend = 0;
    bool fBroadcastTransactions = false;

    /*
     *用于跟踪已用的输出点，以及
     *检测并报告冲突（双倍支出或
     *突变体被挖掘的突变事务）。
     **/

    typedef std::multimap<COutPoint, uint256> TxSpends;
    TxSpends mapTxSpends GUARDED_BY(cs_wallet);
    void AddToSpends(const COutPoint& outpoint, const uint256& wtxid) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void AddToSpends(const uint256& wtxid) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /*
     *向钱包中添加交易，或更新交易。pindex和posinblock应该
     *当已知事务包含在块中时进行设置。什么时候？
     *pindex==nullptr，则钱包状态不会在addtowallet中更新，但
     *发生通知，缓存余额标记为脏。
     *
     *如果fupdate为true，则将更新现有事务。
     *TODO:此操作的一个例外是在
     *假设考虑的交易的任何进一步通知
     *放弃表示被视为放弃是不安全的。
     *应该通过不同的
     *posinblock信号，或在必要时检查内存池是否存在。
     **/

    bool AddToWalletIfInvolvingMe(const CTransactionRef& tx, const CBlockIndex* pIndex, int posInBlock, bool fUpdate) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /*将交易（及其在钱包中的后代）标记为与特定块冲突。*/
    void MarkConflicted(const uint256& hashBlock, const uint256& hashTx);

    /*将事务的输入标记为脏的，从而强制重新计算输出*/
    void MarkInputsDirty(const CTransactionRef& tx) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    void SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator>) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /*由TransactionAddedToMemoryPool/BlockConnected/Disconnected/ScanForWalletTransactions使用。
     *如果是针对包含在块中的事务，则应使用pindexblock和posinblock调用。*/

    void SyncTransaction(const CTransactionRef& tx, const CBlockIndex *pindex = nullptr, int posInBlock = 0, bool update_tx = true) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /*HD链数据模型（外部链计数器）*/
    CHDChain hdChain;

    /*HD派生新的子密钥（在内部或外部链上）*/
    void DeriveNewChildKey(WalletBatch &batch, CKeyMetadata& metadata, CKey& secret, bool internal = false) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    std::set<int64_t> setInternalKeyPool;
    std::set<int64_t> setExternalKeyPool GUARDED_BY(cs_wallet);
    std::set<int64_t> set_pre_split_keypool;
    int64_t m_max_keypool_index GUARDED_BY(cs_wallet) = 0;
    std::map<CKeyID, int64_t> m_pool_key_to_index;
    std::atomic<uint64_t> m_wallet_flags{0};

    int64_t nTimeFirstKey GUARDED_BY(cs_wallet) = 0;

    /*
     *不接受
     *时间戳，如果
     *Watch密钥以前没有与之关联的时间戳。
     *由于这是继承的虚拟方法，因此尽管
     *标记为私有，但无论如何都标记为私有以鼓励使用
     *只接受时间戳并设置
     *ntimefirstkey更智能，更高效地重新扫描。
     **/

    bool AddWatchOnly(const CScript& dest) override EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /*用于访问链状态的接口。*/
    interfaces::Chain& m_chain;

    /*钱包位置，包括钱包名称（参见钱包位置）。*/
    WalletLocation m_location;

    /*内部数据库句柄。*/
    std::unique_ptr<WalletDatabase> database;

    /*
     *以下用于跟踪钱包后面的距离
     *从链同步，并允许客户阻止我们被赶上。
     *
     *请注意，这是*不是*我们已经处理了多远，我们可能需要重新扫描。
     *查看链中的所有交易，但仅用于跟踪
     *Live BlockConnected回调。
     *
     *受cs_Main保护（请参阅blockuntilsynchedoccurrentchain）
     **/

    const CBlockIndex* m_last_block_processed = nullptr;

public:
    /*
     *主钱包锁。
     *此锁保护由cwallet添加的所有字段。
     **/

    mutable CCriticalSection cs_wallet;

    /*获取此钱包使用的数据库句柄。理想情况下，此函数将
     *不需要。
     **/

    WalletDatabase& GetDBHandle()
    {
        return *database;
    }

    /*
     *选择一组硬币，使Nvalueret>=n目标值，至少
     *所有来自CoinControl的硬币均已选择；切勿选择未确认的硬币。
     *如果他们不是我们的人
     **/

    bool SelectCoins(const std::vector<COutput>& vAvailableCoins, const CAmount& nTargetValue, std::set<CInputCoin>& setCoinsRet, CAmount& nValueRet,
                    const CCoinControl& coin_control, CoinSelectionParams& coin_selection_params, bool& bnb_used) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    const WalletLocation& GetLocation() const { return m_location; }

    /*获取用于登录/调试目的的此钱包的名称。
     **/

    const std::string& GetName() const { return m_location.GetName(); }

    void LoadKeyPool(int64_t nIndex, const CKeyPool &keypool) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void MarkPreSplitKeys() EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

//从键ID映射到键元数据。
    std::map<CKeyID, CKeyMetadata> mapKeyMetadata GUARDED_BY(cs_wallet);

//从脚本ID映射到密钥元数据（仅用于监视密钥）。
    std::map<CScriptID, CKeyMetadata> m_script_metadata GUARDED_BY(cs_wallet);

    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    MasterKeyMap mapMasterKeys;
    unsigned int nMasterKeyMaxID = 0;

    /*用指定的名称和数据库实现构造钱包。*/
    CWallet(interfaces::Chain& chain, const WalletLocation& location, std::unique_ptr<WalletDatabase> database) : m_chain(chain), m_location(location), database(std::move(database))
    {
    }

    ~CWallet()
    {
//此时不应连接插槽。
        assert(NotifyUnload.empty());
        delete encrypted_batch;
        encrypted_batch = nullptr;
    }

    std::map<uint256, CWalletTx> mapWallet GUARDED_BY(cs_wallet);

    typedef std::multimap<int64_t, CWalletTx*> TxItems;
    TxItems wtxOrdered;

    int64_t nOrderPosNext GUARDED_BY(cs_wallet) = 0;
    uint64_t nAccountingEntryNumber = 0;

    std::map<CTxDestination, CAddressBookData> mapAddressBook;

    std::set<COutPoint> setLockedCoins GUARDED_BY(cs_wallet);

    /*用于访问链状态的接口。*/
    interfaces::Chain& chain() const { return m_chain; }

    const CWalletTx* GetWalletTx(const uint256& hash) const;

//！检查是否允许我们升级（或已经支持）到命名功能
    bool CanSupportFeature(enum WalletFeature wf) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet) { AssertLockHeld(cs_wallet); return nWalletMaxVersion >= wf; }

    /*
     *用可用输出向量填充VCoins。
     **/

    void AvailableCoins(interfaces::Chain::Lock& locked_chain, std::vector<COutput>& vCoins, bool fOnlySafe=true, const CCoinControl *coinControl = nullptr, const CAmount& nMinimumAmount = 1, const CAmount& nMaximumAmount = MAX_MONEY, const CAmount& nMinimumSumAmount = MAX_MONEY, const uint64_t nMaximumCount = 0, const int nMinDepth = 0, const int nMaxDepth = 9999999) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /*
     *按无变化输出地址分组的可用硬币和锁定硬币的返回列表。
     **/

    std::map<CTxDestination, std::vector<COutput>> ListCoins(interfaces::Chain::Lock& locked_chain) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /*
     *查找未更改的父输出。
     **/

    const CTxOut& FindNonChangeParentOutput(const CTransaction& tx, int output) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /*
     *洗牌并选择硬币，直到达到目标值，同时避免
     *小变化；此方法对于某些输入是随机的，并且
     *完成设定的硬币和相应的实际目标值为
     *组装
     **/

    bool SelectCoinsMinConf(const CAmount& nTargetValue, const CoinEligibilityFilter& eligibility_filter, std::vector<OutputGroup> groups,
        std::set<CInputCoin>& setCoinsRet, CAmount& nValueRet, const CoinSelectionParams& coin_selection_params, bool& bnb_used) const;

    bool IsSpent(interfaces::Chain::Lock& locked_chain, const uint256& hash, unsigned int n) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    std::vector<OutputGroup> GroupOutputs(const std::vector<COutput>& outputs, bool single_coin) const;

    bool IsLockedCoin(uint256 hash, unsigned int n) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void LockCoin(const COutPoint& output) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void UnlockCoin(const COutPoint& output) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void UnlockAllCoins() EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void ListLockedCoins(std::vector<COutPoint>& vOutpts) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /*
     *重新扫描中止属性
     **/

    void AbortRescan() { fAbortRescan = true; }
    bool IsAbortingRescan() { return fAbortRescan; }
    bool IsScanning() { return fScanningWallet; }

    /*
     *密钥库实现
     *生成新密钥
     **/

    CPubKey GenerateNewKey(WalletBatch& batch, bool internal = false) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
//！向存储区添加密钥，并将其保存到磁盘。
    bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey) override EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    bool AddKeyPubKeyWithDB(WalletBatch &batch,const CKey& key, const CPubKey &pubkey) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
//！向商店添加密钥，但不将其保存到磁盘（由LoadWallet使用）
    bool LoadKey(const CKey& key, const CPubKey &pubkey) { return CCryptoKeyStore::AddKeyPubKey(key, pubkey); }
//！加载元数据（由LoadWallet使用）
    void LoadKeyMetadata(const CKeyID& keyID, const CKeyMetadata &metadata) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void LoadScriptMetadata(const CScriptID& script_id, const CKeyMetadata &metadata) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    bool LoadMinVersion(int nVersion) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet) { AssertLockHeld(cs_wallet); nWalletVersion = nVersion; nWalletMaxVersion = std::max(nWalletMaxVersion, nVersion); return true; }
    void UpdateTimeFirstKey(int64_t nCreateTime) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

//！向存储区添加加密密钥，并将其保存到磁盘。
    bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret) override;
//！向商店添加加密密钥，但不将其保存到磁盘（由LoadWallet使用）
    bool LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    bool AddCScript(const CScript& redeemScript) override;
    bool LoadCScript(const CScript& redeemScript);

//！将目标数据元组添加到存储，并将其保存到磁盘
    bool AddDestData(const CTxDestination &dest, const std::string &key, const std::string &value);
//！删除存储和磁盘上的目标数据元组
    bool EraseDestData(const CTxDestination &dest, const std::string &key);
//！将目标数据元组添加到存储，而不将其保存到磁盘
    void LoadDestData(const CTxDestination &dest, const std::string &key, const std::string &value);
//！在存储中查找目标数据元组，否则返回true
    bool GetDestData(const CTxDestination &dest, const std::string &key, std::string *value) const;
//！获取与前缀匹配的所有目标值。
    std::vector<std::string> GetDestValues(const std::string& prefix) const;

//！将仅监视地址添加到存储，并将其保存到磁盘。
    bool AddWatchOnly(const CScript& dest, int64_t nCreateTime) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    bool RemoveWatchOnly(const CScript &dest) override EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
//！向商店添加仅限手表的地址，但不将其保存到磁盘（由LoadWallet使用）
    bool LoadWatchOnly(const CScript &dest);

//！保存一个时间戳，在该时间点，钱包被安排（外部）重新锁定。调用方必须安排通过lock（）进行实际重新锁定。
    int64_t nRelockTime = 0;

    bool Unlock(const SecureString& strWalletPassphrase);
    bool ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase);
    bool EncryptWallet(const SecureString& strWalletPassphrase);

    void GetKeyBirthTimes(interfaces::Chain::Lock& locked_chain, std::map<CTxDestination, int64_t> &mapKeyBirth) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    unsigned int ComputeTimeSmart(const CWalletTx& wtx) const;

    /*
     *增加下一个交易订单ID
     *@返回下一个交易订单ID
     **/

    int64_t IncOrderPosNext(WalletBatch *batch = nullptr) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    DBErrors ReorderTransactions();

    void MarkDirty();
    bool AddToWallet(const CWalletTx& wtxIn, bool fFlushOnClose=true);
    void LoadToWallet(const CWalletTx& wtxIn) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void TransactionAddedToMempool(const CTransactionRef& tx) override;
    void BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex *pindex, const std::vector<CTransactionRef>& vtxConflicted) override;
    void BlockDisconnected(const std::shared_ptr<const CBlock>& pblock) override;
    int64_t RescanFromTime(int64_t startTime, const WalletRescanReserver& reserver, bool update);

    enum class ScanResult {
        SUCCESS,
        FAILURE,
        USER_ABORT
    };
    ScanResult ScanForWalletTransactions(const CBlockIndex* const pindexStart, const CBlockIndex* const pindexStop, const WalletRescanReserver& reserver, const CBlockIndex*& failed_block, const CBlockIndex*& stop_block, bool fUpdate = false);
    void TransactionRemovedFromMempool(const CTransactionRef &ptx) override;
    void ReacceptWalletTransactions();
    void ResendWalletTransactions(int64_t nBestBlockTime, CConnman* connman) override EXCLUSIVE_LOCKS_REQUIRED(cs_main);
//只有在FBRoadcastTransactions时才能调用ResendWalletTransactionsBefore！
    std::vector<uint256> ResendWalletTransactionsBefore(interfaces::Chain::Lock& locked_chain, int64_t nTime, CConnman* connman);
    CAmount GetBalance(const isminefilter& filter=ISMINE_SPENDABLE, const int min_depth=0) const;
    CAmount GetUnconfirmedBalance() const;
    CAmount GetImmatureBalance() const;
    CAmount GetUnconfirmedWatchOnlyBalance() const;
    CAmount GetImmatureWatchOnlyBalance() const;
    CAmount GetLegacyBalance(const isminefilter& filter, int minDepth) const;
    CAmount GetAvailableBalance(const CCoinControl* coinControl = nullptr) const;

    OutputType TransactionChangeType(OutputType change_type, const std::vector<CRecipient>& vecSend);

    /*
     *在交易中插入额外的输入
     *调用createTransaction（）；
     **/

    bool FundTransaction(CMutableTransaction& tx, CAmount& nFeeRet, int& nChangePosInOut, std::string& strFailReason, bool lockUnspents, const std::set<int>& setSubtractFeeFromOutputs, CCoinControl);
    bool SignTransaction(CMutableTransaction& tx) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /*
     *创建一个新的交易，用一组硬币支付收件人
     *由selectCoins（）选择；还可以在需要时创建更改输出
     *note将nchangeposinout传递为-1将导致设置随机位置
     **/

    bool CreateTransaction(interfaces::Chain::Lock& locked_chain, const std::vector<CRecipient>& vecSend, CTransactionRef& tx, CReserveKey& reservekey, CAmount& nFeeRet, int& nChangePosInOut,
                           std::string& strFailReason, const CCoinControl& coin_control, bool sign = true);
    bool CommitTransaction(CTransactionRef tx, mapValue_t mapValue, std::vector<std::pair<std::string, std::string>> orderForm, CReserveKey& reservekey, CConnman* connman, CValidationState& state);

    bool DummySignTx(CMutableTransaction &txNew, const std::set<CTxOut> &txouts, bool use_max_sig = false) const
    {
        std::vector<CTxOut> v_txouts(txouts.size());
        std::copy(txouts.begin(), txouts.end(), v_txouts.begin());
        return DummySignTx(txNew, v_txouts, use_max_sig);
    }
    bool DummySignTx(CMutableTransaction &txNew, const std::vector<CTxOut> &txouts, bool use_max_sig = false) const;
    bool DummySignInput(CTxIn &tx_in, const CTxOut &txout, bool use_max_sig = false) const;

    CFeeRate m_pay_tx_fee{DEFAULT_PAY_TX_FEE};
    unsigned int m_confirm_target{DEFAULT_TX_CONFIRM_TARGET};
    bool m_spend_zero_conf_change{DEFAULT_SPEND_ZEROCONF_CHANGE};
    bool m_signal_rbf{DEFAULT_WALLET_RBF};
bool m_allow_fallback_fee{true}; //！<将通过chainParams定义
CFeeRate m_min_fee{DEFAULT_TRANSACTION_MINFEE}; //！<用-mintxfee覆盖
    /*
     *如果费用估算没有足够的数据来提供估算，则使用该费用。
     *如果不使用费用估算，则无效。
     *覆盖-回退费
     **/

    CFeeRate m_fallback_fee{DEFAULT_FALLBACK_FEE};
    CFeeRate m_discard_rate{DEFAULT_DISCARD_FEE};
    OutputType m_default_address_type{DEFAULT_ADDRESS_TYPE};
    OutputType m_default_change_type{DEFAULT_CHANGE_TYPE};

    bool NewKeyPool();
    size_t KeypoolCountExternalKeys() EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    bool TopUpKeyPool(unsigned int kpSize = 0);

    /*
     *从密钥池中保留一个密钥，并将nindex设置为其索引
     *
     *@param[out]nindex键池中键的索引
     *@param[out]keypool从中提取密钥的密钥池，可以是
     *预分割池（如果存在），或内部或外部池
     *@param frequeuestedinternal true，如果调用方希望绘制密钥
     *在内部密钥池中，如果首选外部密钥池，则为false。
     *
     *@如果成功则返回true，如果由于空密钥池而失败则返回false
     *@throws std:：runtime_error如果键池读取失败，则键无效，
     *未在钱包中找到，或在内部分类错误
     *或外部密钥池
     **/

    bool ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool, bool fRequestedInternal);
    void KeepKey(int64_t nIndex);
    void ReturnKey(int64_t nIndex, bool fInternal, const CPubKey& pubkey);
    bool GetKeyFromPool(CPubKey &key, bool internal = false);
    int64_t GetOldestKeyPoolTime();
    /*
     *将密钥池中的所有密钥标记为已使用的保留密钥（包括保留密钥）。
     **/

    void MarkReserveKeysAsUsed(int64_t keypool_id) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    const std::map<CKeyID, int64_t>& GetAllReserveKeys() const { return m_pool_key_to_index; }

    std::set<std::set<CTxDestination>> GetAddressGroupings() EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    std::map<CTxDestination, CAmount> GetAddressBalances(interfaces::Chain::Lock& locked_chain);

    std::set<CTxDestination> GetLabelAddresses(const std::string& label) const;

    isminetype IsMine(const CTxIn& txin) const;
    /*
     *如果输入与
     *过滤，否则返回0
     **/

    CAmount GetDebit(const CTxIn& txin, const isminefilter& filter) const;
    isminetype IsMine(const CTxOut& txout) const;
    CAmount GetCredit(const CTxOut& txout, const isminefilter& filter) const;
    bool IsChange(const CTxOut& txout) const;
    bool IsChange(const CScript& script) const;
    CAmount GetChange(const CTxOut& txout) const;
    bool IsMine(const CTransaction& tx) const;
    /*应该改名为isrelevanttome*/
    bool IsFromMe(const CTransaction& tx) const;
    CAmount GetDebit(const CTransaction& tx, const isminefilter& filter) const;
    /*返回所有输入是否与筛选器匹配*/
    bool IsAllFromMe(const CTransaction& tx, const isminefilter& filter) const;
    CAmount GetCredit(const CTransaction& tx, const isminefilter& filter) const;
    CAmount GetChange(const CTransaction& tx) const;
    void ChainStateFlushed(const CBlockLocator& loc) override;

    DBErrors LoadWallet(bool& fFirstRunRet);
    DBErrors ZapWalletTx(std::vector<CWalletTx>& vWtx);
    DBErrors ZapSelectTx(std::vector<uint256>& vHashIn, std::vector<uint256>& vHashOut) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    bool SetAddressBook(const CTxDestination& address, const std::string& strName, const std::string& purpose);

    bool DelAddressBook(const CTxDestination& address);

    const std::string& GetLabelName(const CScript& scriptPubKey) const;

    void GetScriptForMining(std::shared_ptr<CReserveScript> &script);

    unsigned int GetKeyPoolSize() EXCLUSIVE_LOCKS_REQUIRED(cs_wallet)
    {
AssertLockHeld(cs_wallet); //设置Ex，在ternalkeypool中
        return setInternalKeyPool.size() + setExternalKeyPool.size();
    }

//！表示现在使用了特定的钱包功能。这可能会更改nwalltversion和nwalltmaxversion（如果它们较低）
    void SetMinVersion(enum WalletFeature, WalletBatch* batch_in = nullptr, bool fExplicit = false);

//！更改允许升级到的版本（请注意，这并不意味着立即升级到该格式）
    bool SetMaxVersion(int nVersion);

//！获取当前钱包格式（最旧的客户端版本保证理解此钱包）
    int GetVersion() { LOCK(cs_wallet); return nWalletVersion; }

//！获取与给定交易冲突的钱包交易（支出相同的输出）
    std::set<uint256> GetConflicts(const uint256& txid) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

//！检查给定交易的任何输出是否由钱包中的另一个交易花费。
    bool HasWalletSpend(const uint256& txid) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

//！Flush钱包（BitDB Flush）
    void Flush(bool shutdown=false);

    /*钱包要卸了*/
    boost::signals2::signal<void ()> NotifyUnload;

    /*
     *通讯簿条目已更改。
     带着锁的储物柜打电话给了我们。
     **/

    boost::signals2::signal<void (CWallet *wallet, const CTxDestination
            &address, const std::string &label, bool isMine,
            const std::string &purpose,
            ChangeType status)> NotifyAddressBookChanged;

    /*
     *钱包交易已添加、删除或更新。
     带着锁的储物柜打电话给了我们。
     **/

    boost::signals2::signal<void (CWallet *wallet, const uint256 &hashTx,
            ChangeType status)> NotifyTransactionChanged;

    /*显示进度，例如重新扫描*/
    boost::signals2::signal<void (const std::string &title, int nProgress)> ShowProgress;

    /*仅监视添加的地址*/
    boost::signals2::signal<void (bool fHaveWatchOnly)> NotifyWatchonlyChanged;

    /*询问此钱包是否广播交易。*/
    bool GetBroadcastTransactions() const { return fBroadcastTransactions; }
    /*设置此钱包是否广播交易。*/
    void SetBroadcastTransactions(bool broadcast) { fBroadcastTransactions = broadcast; }

    /*返回是否可以放弃交易*/
    bool TransactionCanBeAbandoned(const uint256& hashTx) const;

    /*将交易（及其在钱包子代中）标记为已放弃，以便其输入可以被响应。*/
    bool AbandonTransaction(interfaces::Chain::Lock& locked_chain, const uint256& hashTx);

    /*将一个事务标记为被另一个事务（例如，BIP 125）替换。*/
    bool MarkReplaced(const uint256& originalHash, const uint256& newHash);

//！核实钱包名称，必要时对钱包进行补救。
    static bool Verify(interfaces::Chain& chain, const WalletLocation& location, bool salvage_wallet, std::string& error_string, std::string& warning_string);

    /*初始化钱包，返回新的cwallet实例或空指针以防出错。*/
    static std::shared_ptr<CWallet> CreateWalletFromFile(interfaces::Chain& chain, const WalletLocation& location, uint64_t wallet_creation_flags = 0);

    /*
     *Wallet Post初始化设置
     *让钱包有机会注册重复任务并完成初始化后的任务。
     **/

    void postInitProcess();

    bool BackupWallet(const std::string& strDest);

    /*设置HD链模型（链子索引计数器）*/
    void SetHDChain(const CHDChain& chain, bool memonly);
    const CHDChain& GetHDChain() const { return hdChain; }

    /*如果启用了HD，则返回true*/
    bool IsHDEnabled() const;

    /*生成新的HD种子（将不被激活）*/
    CPubKey GenerateNewSeed();

    /*派生新的HD种子（将不被激活）*/
    CPubKey DeriveNewSeed(const CKey& key);

    /*设置当前HD种子（将重置链子索引计数器）
       根据当前钱包版本设置种子版本（因此
       呼叫方在呼叫前必须确保当前钱包版本正确。
       这个函数。*/

    void SetHDSeed(const CPubKey& key);

    /*
     *在钱包状态为最新/至少/当前
     *输入此函数时的链
     *很明显，在打这个电话的时候拿着CS-MAIN/CS-U钱包可能会导致
     ＊死锁
     **/

    void BlockUntilSyncedToCurrentChain() LOCKS_EXCLUDED(cs_main, cs_wallet);

    /*
     *明确让钱包学习输出到
     *给出密钥。这纯粹是为了使钱包文件与旧的兼容
     *软件，因为cbasickeystore自动为所有人隐式执行此操作。
     *现在键。
     **/

    void LearnRelatedScripts(const CPubKey& key, OutputType);

    /*
     *与LearnRelatedScripts相同，但当输出类型未知时（并且可能
     什么都可以。
     **/

    void LearnAllRelatedScripts(const CPubKey& key);

    /*设置单个钱包标志*/
    void SetWalletFlag(uint64_t flags);

    /*检查是否设置了某个钱包标志*/
    bool IsWalletFlagSet(uint64_t flag);

    /*用给定的uint64覆盖所有标志
       如果存在未知的不可容忍标志，则返回false*/

    bool SetWalletFlags(uint64_t overwriteFlags, bool memOnly);

    /*返回带括号的钱包名称以显示在日志中，如果钱包没有名称，则返回[默认钱包]*/
    const std::string GetDisplayName() const {
        std::string wallet_name = GetName().length() == 0 ? "default wallet" : GetName();
        return strprintf("[%s]", wallet_name);
    };

    /*在日志输出中预先设置钱包名称，以便于在多钱包用例中进行调试。*/
    template<typename... Params>
    void WalletLogPrintf(std::string fmt, Params... parameters) const {
        LogPrintf(("%s " + fmt).c_str(), GetDisplayName(), parameters...);
    };

    /*通过钱包密钥元数据实现密钥来源信息的查找。*/
    bool GetKeyOrigin(const CKeyID& keyid, KeyOriginInfo& info) const override;
};

/*从密钥池分配的密钥。*/
class CReserveKey final : public CReserveScript
{
protected:
    CWallet* pwallet;
    int64_t nIndex{-1};
    CPubKey vchPubKey;
    bool fInternal{false};

public:
    explicit CReserveKey(CWallet* pwalletIn)
    {
        pwallet = pwalletIn;
    }

    CReserveKey(const CReserveKey&) = delete;
    CReserveKey& operator=(const CReserveKey&) = delete;

    ~CReserveKey()
    {
        ReturnKey();
    }

    void ReturnKey();
    bool GetReservedKey(CPubKey &pubkey, bool internal = false);
    void KeepKey();
    void KeepScript() override { KeepKey(); }
};

/*拒绝检查和预订钱包重新扫描*/
class WalletRescanReserver
{
private:
    CWallet* m_wallet;
    bool m_could_reserve;
public:
    explicit WalletRescanReserver(CWallet* w) : m_wallet(w), m_could_reserve(false) {}

    bool reserve()
    {
        assert(!m_could_reserve);
        std::lock_guard<std::mutex> lock(m_wallet->mutexScanning);
        if (m_wallet->fScanningWallet) {
            return false;
        }
        m_wallet->fScanningWallet = true;
        m_could_reserve = true;
        return true;
    }

    bool isReserved() const
    {
        return (m_could_reserve && m_wallet->fScanningWallet);
    }

    ~WalletRescanReserver()
    {
        std::lock_guard<std::mutex> lock(m_wallet->mutexScanning);
        if (m_could_reserve) {
            m_wallet->fScanningWallet = false;
        }
    }
};

//假设所有签名都是最大大小，则计算事务的大小
//使用dummysignaturecreator，它在任何地方插入71个字节的签名。
//注意：这要求所有输入必须在mapwallet中（例如，tx应该
//是我的。
int64_t CalculateMaximumSignedTxSize(const CTransaction &tx, const CWallet *wallet, bool use_max_sig = false) EXCLUSIVE_LOCKS_REQUIRED(wallet->cs_wallet);
int64_t CalculateMaximumSignedTxSize(const CTransaction &tx, const CWallet *wallet, const std::vector<CTxOut>& txouts, bool use_max_sig = false);
#endif //比特币钱包
