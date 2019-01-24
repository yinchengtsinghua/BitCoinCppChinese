
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2015-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_TEST_TEST_BITCOIN_H
#define BITCOIN_TEST_TEST_BITCOIN_H

#include <chainparamsbase.h>
#include <fs.h>
#include <key.h>
#include <pubkey.h>
#include <random.h>
#include <scheduler.h>
#include <txdb.h>
#include <txmempool.h>

#include <memory>
#include <type_traits>

#include <boost/thread.hpp>

//为枚举类类型启用boost-check-equal
template <typename T>
std::ostream& operator<<(typename std::enable_if<std::is_enum<T>::value, std::ostream>::type& stream, const T& e)
{
    return stream << static_cast<typename std::underlying_type<T>::type>(e);
}

/*
 *此全局和使用它的帮助程序不是线程安全的。
 *
 *如果需要线程安全，则可以将全局设置为线程本地（给定
 *我们支持的所有架构都支持线程本地）或
 *每个线程实例可以用于多线程测试。
 **/

extern FastRandomContext g_insecure_rand_ctx;

static inline void SeedInsecureRand(bool deterministic = false)
{
    g_insecure_rand_ctx = FastRandomContext(deterministic);
}

static inline uint32_t InsecureRand32() { return g_insecure_rand_ctx.rand32(); }
static inline uint256 InsecureRand256() { return g_insecure_rand_ctx.rand256(); }
static inline uint64_t InsecureRandBits(int bits) { return g_insecure_rand_ctx.randbits(bits); }
static inline uint64_t InsecureRandRange(uint64_t range) { return g_insecure_rand_ctx.randrange(range); }
static inline bool InsecureRandBool() { return g_insecure_rand_ctx.randbool(); }

static constexpr CAmount CENT{1000000};

/*基本测试设置。
 *这只是配置日志和链参数。
 **/

struct BasicTestingSetup {
    ECCVerifyHandle globalVerifyHandle;

    explicit BasicTestingSetup(const std::string& chainName = CBaseChainParams::MAIN);
    ~BasicTestingSetup();

    fs::path SetDataDir(const std::string& name);

private:
    const fs::path m_path_root;
};

/*配置完整环境的测试设置。
 *包括数据目录、硬币数据库、脚本检查线程设置。
 **/

class CConnman;
class CNode;
struct CConnmanTest {
    static void AddNode(CNode& node);
    static void ClearNodes();
};

class PeerLogicValidation;
struct TestingSetup : public BasicTestingSetup {
    boost::thread_group threadGroup;
    CConnman* connman;
    CScheduler scheduler;
    std::unique_ptr<PeerLogicValidation> peerLogic;

    explicit TestingSetup(const std::string& chainName = CBaseChainParams::MAIN);
    ~TestingSetup();
};

class CBlock;
struct CMutableTransaction;
class CScript;

//
//预创建一个
//100块重新测试模式块链
//
struct TestChain100Setup : public TestingSetup {
    TestChain100Setup();

//创建一个新的块，其中只包含给定的事务，coinbase支付给
//scriptPubKey，并尝试将其添加到当前链。
    CBlock CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns,
                                 const CScript& scriptPubKey);

    ~TestChain100Setup();

std::vector<CTransactionRef> m_coinbase_txns; //为了方便起见，CoinBase交易
CKey coinbaseKey; //花费CoinBase交易所需的私钥/公钥
};

class CTxMemPoolEntry;

struct TestMemPoolEntryHelper
{
//默认值
    CAmount nFee;
    int64_t nTime;
    unsigned int nHeight;
    bool spendsCoinbase;
    unsigned int sigOpCost;
    LockPoints lp;

    TestMemPoolEntryHelper() :
        nFee(0), nTime(0), nHeight(1),
        spendsCoinbase(false), sigOpCost(4) { }

    CTxMemPoolEntry FromTx(const CMutableTransaction& tx);
    CTxMemPoolEntry FromTx(const CTransactionRef& tx);

//更改默认值
    TestMemPoolEntryHelper &Fee(CAmount _fee) { nFee = _fee; return *this; }
    TestMemPoolEntryHelper &Time(int64_t _time) { nTime = _time; return *this; }
    TestMemPoolEntryHelper &Height(unsigned int _height) { nHeight = _height; return *this; }
    TestMemPoolEntryHelper &SpendsCoinbase(bool _flag) { spendsCoinbase = _flag; return *this; }
    TestMemPoolEntryHelper &SigOpsCost(unsigned int _sigopsCost) { sigOpCost = _sigopsCost; return *this; }
};

CBlock getBlock13b8a();

//在此处定义隐式转换，以便uint256可直接用于boost_check_*
std::ostream& operator<<(std::ostream& os, const uint256& num);

#endif
