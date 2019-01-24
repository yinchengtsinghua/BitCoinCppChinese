
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2017-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <wallet/wallet.h>
#include <wallet/coinselection.h>
#include <wallet/coincontrol.h>
#include <amount.h>
#include <primitives/transaction.h>
#include <random.h>
#include <test/test_bitcoin.h>
#include <wallet/test/wallet_test_fixture.h>

#include <boost/test/unit_test.hpp>
#include <random>

BOOST_FIXTURE_TEST_SUITE(coinselector_tests, WalletTestingSetup)

//多少次运行所有测试，以便有机会捕获只随特定随机随机洗牌显示的错误
#define RUN_TESTS 100

//有些测试由于运气不好而失败了1%。
//我们重复这些测试很多次，只有当测试的所有迭代都失败时才会抱怨。
#define RANDOM_REPEATS 5

std::vector<std::unique_ptr<CWalletTx>> wtxn;

typedef std::set<CInputCoin> CoinSet;

static std::vector<COutput> vCoins;
static auto testChain = interfaces::MakeChain();
static CWallet testWallet(*testChain, WalletLocation(), WalletDatabase::CreateDummy());
static CAmount balance = 0;

CoinEligibilityFilter filter_standard(1, 6, 0);
CoinEligibilityFilter filter_confirmed(1, 1, 0);
CoinEligibilityFilter filter_standard_extra(6, 6, 0);
CoinSelectionParams coin_selection_params(false, 0, 0, CFeeRate(0), 0);

static void add_coin(const CAmount& nValue, int nInput, std::vector<CInputCoin>& set)
{
    CMutableTransaction tx;
    tx.vout.resize(nInput + 1);
    tx.vout[nInput].nValue = nValue;
    set.emplace_back(MakeTransactionRef(tx), nInput);
}

static void add_coin(const CAmount& nValue, int nInput, CoinSet& set)
{
    CMutableTransaction tx;
    tx.vout.resize(nInput + 1);
    tx.vout[nInput].nValue = nValue;
    set.emplace(MakeTransactionRef(tx), nInput);
}

static void add_coin(const CAmount& nValue, int nAge = 6*24, bool fIsFromMe = false, int nInput=0)
{
    balance += nValue;
    static int nextLockTime = 0;
    CMutableTransaction tx;
tx.nLockTime = nextLockTime++;        //所以所有的事务都有不同的哈希值
    tx.vout.resize(nInput + 1);
    tx.vout[nInput].nValue = nValue;
    if (fIsFromMe) {
//isFromme（）返回（getDebit（）>0），如果vin.empty（），则getDebit（）为0，
//因此，停止VIN为空，并缓存一个非零借记以伪造isfromme（）。
        tx.vin.resize(1);
    }
    std::unique_ptr<CWalletTx> wtx = MakeUnique<CWalletTx>(&testWallet, MakeTransactionRef(std::move(tx)));
    if (fIsFromMe)
    {
        wtx->fDebitCached = true;
        wtx->nDebitCached = 1;
    }
    /*tput输出（wtx.get（），ninput，nage，true/*spenable*/，true/*solvable*/，true/*safe*/）；
    vcoins.push_back（输出）；
    testwallet.addtowallet（*wtx.get（））；
    wtxn.emplace_back（std:：move（wtx））；
}

静态空钱包（空）
{
    V硬币；
    WTXN.Car（）；
    余额＝0；
}

静态布尔等价集（coinset A，coinset B）
{
    std:：pair<coinset:：iterator，coinset:：iterator>ret=不匹配（a.begin（），a.end（），b.begin（））；
    返回ret.first==a.end（）&&ret.second==b.end（）；
}

静态camount make_hard_case（int utxos，std:：vector<cinputcoin>&utxo_pool）
{
    utxo_pool.clear（）；
    camount目标=0；
    对于（int i=0；i<utxos；+i）
        目标+=（camount）1<（utxos+i）；
        添加硬币（camount）1<（utxos+i），2*i，utxo-pool；
        添加硬币（（camount）1<（utxos+i））+（（camount）1<（utxos-1-i）），2*i+1，utxo-pool）；
    }
    返回目标；
}

inline std:：vector<outputgroup>和groupcoins（const std:：vector<cinputcoin>和coins）
{
    static std:：vector<outputgroup>static_groups；
    静态_groups.clear（）；
    对于（自动和硬币：硬币）静态组。放置回（硬币，0，真，0，0）；
    返回静态组；
}

inline std:：vector<outputgroup>和groupcoins（const std:：vector<coutput>和coins）
{
    static std:：vector<outputgroup>static_groups；
    静态_groups.clear（）；
    对于（auto&coin:coins）静态_groups.emplace_back（coin.getinputcoin（），coin.ndepth，coin.tx->fdebitcached&&coin.tx->ndebitcached==1/*hack:我们无法找出is_me标志，因此我们使用上面定义的条件；可能将safe设置为false for！第五个字母是“添加硬币”（）*/, 0, 0);

    return static_groups;
}

//支链和绑定硬币选择测试
BOOST_AUTO_TEST_CASE(bnb_search_test)
{

    LOCK(testWallet.cs_wallet);

//安装程序
    std::vector<CInputCoin> utxo_pool;
    CoinSet selection;
    CoinSet actual_selection;
    CAmount value_ret = 0;
    CAmount not_input_fees = 0;

////
//已知结果测试//
////
    BOOST_TEST_MESSAGE("Testing known outcomes");

//空的UTXO池
    BOOST_CHECK(!SelectCoinsBnB(GroupCoins(utxo_pool), 1 * CENT, 0.5 * CENT, selection, value_ret, not_input_fees));
    selection.clear();

//添加UTXOS
    add_coin(1 * CENT, 1, utxo_pool);
    add_coin(2 * CENT, 2, utxo_pool);
    add_coin(3 * CENT, 3, utxo_pool);
    add_coin(4 * CENT, 4, utxo_pool);

//选择1美分
    add_coin(1 * CENT, 1, actual_selection);
    BOOST_CHECK(SelectCoinsBnB(GroupCoins(utxo_pool), 1 * CENT, 0.5 * CENT, selection, value_ret, not_input_fees));
    BOOST_CHECK(equal_sets(selection, actual_selection));
    BOOST_CHECK_EQUAL(value_ret, 1 * CENT);
    actual_selection.clear();
    selection.clear();

//选择2美分
    add_coin(2 * CENT, 2, actual_selection);
    BOOST_CHECK(SelectCoinsBnB(GroupCoins(utxo_pool), 2 * CENT, 0.5 * CENT, selection, value_ret, not_input_fees));
    BOOST_CHECK(equal_sets(selection, actual_selection));
    BOOST_CHECK_EQUAL(value_ret, 2 * CENT);
    actual_selection.clear();
    selection.clear();

//选择5美分
    add_coin(3 * CENT, 3, actual_selection);
    add_coin(2 * CENT, 2, actual_selection);
    BOOST_CHECK(SelectCoinsBnB(GroupCoins(utxo_pool), 5 * CENT, 0.5 * CENT, selection, value_ret, not_input_fees));
    BOOST_CHECK(equal_sets(selection, actual_selection));
    BOOST_CHECK_EQUAL(value_ret, 5 * CENT);
    actual_selection.clear();
    selection.clear();

//选择11美分，不可能
    BOOST_CHECK(!SelectCoinsBnB(GroupCoins(utxo_pool), 11 * CENT, 0.5 * CENT, selection, value_ret, not_input_fees));
    actual_selection.clear();
    selection.clear();

//选择10美分
    add_coin(5 * CENT, 5, utxo_pool);
    add_coin(4 * CENT, 4, actual_selection);
    add_coin(3 * CENT, 3, actual_selection);
    add_coin(2 * CENT, 2, actual_selection);
    add_coin(1 * CENT, 1, actual_selection);
    BOOST_CHECK(SelectCoinsBnB(GroupCoins(utxo_pool), 10 * CENT, 0.5 * CENT, selection, value_ret, not_input_fees));
    BOOST_CHECK(equal_sets(selection, actual_selection));
    BOOST_CHECK_EQUAL(value_ret, 10 * CENT);
    actual_selection.clear();
    selection.clear();

//负有效值
//选择10美分，但不能有1美分，因为太小
    add_coin(5 * CENT, 5, actual_selection);
    add_coin(3 * CENT, 3, actual_selection);
    add_coin(2 * CENT, 2, actual_selection);
    BOOST_CHECK(SelectCoinsBnB(GroupCoins(utxo_pool), 10 * CENT, 5000, selection, value_ret, not_input_fees));
    BOOST_CHECK_EQUAL(value_ret, 10 * CENT);
//Fixme：上述测试是多余的，因为选择了1分，而不是“太小”
//Boost_check（相等_集（选择，实际_选择））；

//选择0.25美分，不可能
    BOOST_CHECK(!SelectCoinsBnB(GroupCoins(utxo_pool), 0.25 * CENT, 0.5 * CENT, selection, value_ret, not_input_fees));
    actual_selection.clear();
    selection.clear();

//迭代疲劳试验
    CAmount target = make_hard_case(17, utxo_pool);
BOOST_CHECK(!SelectCoinsBnB(GroupCoins(utxo_pool), target, 0, selection, value_ret, not_input_fees)); //应该排气
    target = make_hard_case(14, utxo_pool);
BOOST_CHECK(SelectCoinsBnB(GroupCoins(utxo_pool), target, 0, selection, value_ret, not_input_fees)); //不应该排气

//测试相同价值的早期救助优化
    utxo_pool.clear();
    add_coin(7 * CENT, 7, actual_selection);
    add_coin(7 * CENT, 7, actual_selection);
    add_coin(7 * CENT, 7, actual_selection);
    add_coin(7 * CENT, 7, actual_selection);
    add_coin(2 * CENT, 7, actual_selection);
    add_coin(7 * CENT, 7, utxo_pool);
    add_coin(7 * CENT, 7, utxo_pool);
    add_coin(7 * CENT, 7, utxo_pool);
    add_coin(7 * CENT, 7, utxo_pool);
    add_coin(2 * CENT, 7, utxo_pool);
    for (int i = 0; i < 50000; ++i) {
        add_coin(5 * CENT, 7, utxo_pool);
    }
    BOOST_CHECK(SelectCoinsBnB(GroupCoins(utxo_pool), 30 * CENT, 5000, selection, value_ret, not_input_fees));
    BOOST_CHECK_EQUAL(value_ret, 30 * CENT);
    BOOST_CHECK(equal_sets(selection, actual_selection));

////
//行为测试//
////
//选择1美分，池仅大于5美分
    utxo_pool.clear();
    for (int i = 5; i <= 20; ++i) {
        add_coin(i * CENT, i, utxo_pool);
    }
//运行100次，以确保永远找不到解决方案
    for (int i = 0; i < 100; ++i) {
        BOOST_CHECK(!SelectCoinsBnB(GroupCoins(utxo_pool), 1 * CENT, 2 * CENT, selection, value_ret, not_input_fees));
    }

//使用bnb时，确保有效值在selectcoinsminconf中有效。
    CoinSelectionParams coin_selection_params_bnb(true, 0, 0, CFeeRate(3000), 0);
    CoinSet setCoinsRet;
    CAmount nValueRet;
    bool bnb_used;
    empty_wallet();
    add_coin(1);
vCoins.at(0).nInputBytes = 40; //确保它有一个负的有效值。下一个检查应该断言这是否以某种方式通过。否则就会失败
    BOOST_CHECK(!testWallet.SelectCoinsMinConf( 1 * CENT, filter_standard, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params_bnb, bnb_used));

//确保在有预设输入时不使用BNB
    empty_wallet();
    add_coin(5 * CENT);
    add_coin(3 * CENT);
    add_coin(2 * CENT);
    CCoinControl coin_control;
    coin_control.fAllowOtherInputs = true;
    coin_control.Select(COutPoint(vCoins.at(0).tx->GetHash(), vCoins.at(0).i));
    BOOST_CHECK(testWallet.SelectCoins(vCoins, 10 * CENT, setCoinsRet, nValueRet, coin_control, coin_selection_params_bnb, bnb_used));
    BOOST_CHECK(!bnb_used);
    BOOST_CHECK(!coin_selection_params_bnb.use_bnb);
}

BOOST_AUTO_TEST_CASE(knapsack_solver_test)
{
    CoinSet setCoinsRet, setCoinsRet2;
    CAmount nValueRet;
    bool bnb_used;

    LOCK(testWallet.cs_wallet);

//多次测试以允许随机播放顺序的差异
    for (int i = 0; i < RUN_TESTS; i++)
    {
        empty_wallet();

//带着一个空钱包，我们连一分钱都付不起
        BOOST_CHECK(!testWallet.SelectCoinsMinConf( 1 * CENT, filter_standard, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));

add_coin(1*CENT, 4);        //加一个新的1美分硬币

//有了新的1美分硬币，我们还是找不到成熟的1美分
        BOOST_CHECK(!testWallet.SelectCoinsMinConf( 1 * CENT, filter_standard, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));

//但是我们可以找到新的1美分
        BOOST_CHECK( testWallet.SelectCoinsMinConf( 1 * CENT, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
        BOOST_CHECK_EQUAL(nValueRet, 1 * CENT);

add_coin(2*CENT);           //加一枚成熟的2美分硬币

//我们不能制造3美分的成熟硬币
        BOOST_CHECK(!testWallet.SelectCoinsMinConf( 3 * CENT, filter_standard, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));

//我们可以制造3美分的新硬币
        BOOST_CHECK( testWallet.SelectCoinsMinConf( 3 * CENT, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
        BOOST_CHECK_EQUAL(nValueRet, 3 * CENT);

add_coin(5*CENT);           //加一枚成熟的5美分硬币，
add_coin(10*CENT, 3, true); //从我们自己的地址寄来的10美分硬币
add_coin(20*CENT);          //一枚成熟的20美分硬币

//现在我们有了新的：1+10=11（其中10个是自己发送的），成熟的：2+5+20=27。总数＝38

//如果我们不允许新硬币，我们就不能赚38美分：
        BOOST_CHECK(!testWallet.SelectCoinsMinConf(38 * CENT, filter_standard, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
//如果我们不允许新硬币，即使是我们的新硬币，我们也赚不到37美分。
        BOOST_CHECK(!testWallet.SelectCoinsMinConf(38 * CENT, filter_standard_extra, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
//但如果我们接受自己的新硬币，我们可以赚37美分
        BOOST_CHECK( testWallet.SelectCoinsMinConf(37 * CENT, filter_standard, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
        BOOST_CHECK_EQUAL(nValueRet, 37 * CENT);
//如果我们接受所有的新硬币，我们可以赚38美分
        BOOST_CHECK( testWallet.SelectCoinsMinConf(38 * CENT, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
        BOOST_CHECK_EQUAL(nValueRet, 38 * CENT);

//试着从1,2,5,10,20中赚34美分-我们不能完全做到
        BOOST_CHECK( testWallet.SelectCoinsMinConf(34 * CENT, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
BOOST_CHECK_EQUAL(nValueRet, 35 * CENT);       //但35美分是最接近的
BOOST_CHECK_EQUAL(setCoinsRet.size(), 3U);     //最好是20+10+5。1或2不太可能被包括在内（但可能）

//当我们试着制造7美分时，较小的硬币（1，2，5）就足够了。我们应该只看到2+5
        BOOST_CHECK( testWallet.SelectCoinsMinConf( 7 * CENT, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
        BOOST_CHECK_EQUAL(nValueRet, 7 * CENT);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 2U);

//当我们试着制造8美分时，较小的硬币（1，2，5）就足够了。
        BOOST_CHECK( testWallet.SelectCoinsMinConf( 8 * CENT, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
        BOOST_CHECK(nValueRet == 8 * CENT);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 3U);

//当我们试着制造9美分时，小硬币的任何一个子集都不够，我们得到下一个大硬币（10）
        BOOST_CHECK( testWallet.SelectCoinsMinConf( 9 * CENT, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
        BOOST_CHECK_EQUAL(nValueRet, 10 * CENT);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

//现在，清空钱包，重新开始测试在小硬币和下一个大硬币之间进行选择。
        empty_wallet();

        add_coin( 6*CENT);
        add_coin( 7*CENT);
        add_coin( 8*CENT);
        add_coin(20*CENT);
add_coin(30*CENT); //现在我们有6+7+8+20+30=71分

//检查我们有71个而不是72个
        BOOST_CHECK( testWallet.SelectCoinsMinConf(71 * CENT, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
        BOOST_CHECK(!testWallet.SelectCoinsMinConf(72 * CENT, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));

//现在试着挣16美分。最好的小硬币是6+7+8=21；不如下一个大硬币，20
        BOOST_CHECK( testWallet.SelectCoinsMinConf(16 * CENT, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
BOOST_CHECK_EQUAL(nValueRet, 20 * CENT); //我们一个硬币要20元
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

add_coin( 5*CENT); //现在我们有5+6+7+8+20+30=75美分

//现在，如果我们再次尝试制造16美分，较小的硬币可以制造5+6+7=18美分，比下一个最大的硬币更好，20美分
        BOOST_CHECK( testWallet.SelectCoinsMinConf(16 * CENT, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
BOOST_CHECK_EQUAL(nValueRet, 18 * CENT); //我们应该得到三分之十八的硬币
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 3U);

add_coin( 18*CENT); //现在我们有5+6+7+8+18+20+30

//现在，如果我们再次尝试制造16美分，较小的硬币可以制造5+6+7=18美分，与下一个最大的硬币一样，18美分
        BOOST_CHECK( testWallet.SelectCoinsMinConf(16 * CENT, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
BOOST_CHECK_EQUAL(nValueRet, 18 * CENT);  //我们应该得到18合1的硬币
BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U); //因为在平局中，最大的硬币获胜

//现在试着挣11美分。我们应该得到5+6
        BOOST_CHECK( testWallet.SelectCoinsMinConf(11 * CENT, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
        BOOST_CHECK_EQUAL(nValueRet, 11 * CENT);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 2U);

//检查是否使用最小的大硬币
        add_coin( 1*COIN);
        add_coin( 2*COIN);
        add_coin( 3*COIN);
add_coin( 4*COIN); //现在我们有5+6+7+8+18+20+30+100+200+300+400=1094美分
        BOOST_CHECK( testWallet.SelectCoinsMinConf(95 * CENT, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
BOOST_CHECK_EQUAL(nValueRet, 1 * COIN);  //我们应该一个硬币一个BTC
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

        BOOST_CHECK( testWallet.SelectCoinsMinConf(195 * CENT, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
BOOST_CHECK_EQUAL(nValueRet, 2 * COIN);  //我们一个硬币要2个BTC
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

//清空钱包，然后重新开始，现在只剩下零钱的一部分，以测试避免零钱。

        empty_wallet();
        add_coin(MIN_CHANGE * 1 / 10);
        add_coin(MIN_CHANGE * 2 / 10);
        add_coin(MIN_CHANGE * 3 / 10);
        add_coin(MIN_CHANGE * 4 / 10);
        add_coin(MIN_CHANGE * 5 / 10);

//尝试从1.5*分钟的变化中做出1*分钟的变化
//无论发生什么变化，我们都会得到比最小变化小的变化，所以可以期待最小变化
        BOOST_CHECK( testWallet.SelectCoinsMinConf(MIN_CHANGE, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
        BOOST_CHECK_EQUAL(nValueRet, MIN_CHANGE);

//但是如果我们加一个大硬币，就避免了零钱
        add_coin(1111*MIN_CHANGE);

//尝试使1从0.1+0.2+0.3+0.4+0.5+1111=1112.5
        BOOST_CHECK( testWallet.SelectCoinsMinConf(1 * MIN_CHANGE, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
BOOST_CHECK_EQUAL(nValueRet, 1 * MIN_CHANGE); //我们应该得到确切的数额

//如果我们增加更多的小硬币：
        add_coin(MIN_CHANGE * 6 / 10);
        add_coin(MIN_CHANGE * 7 / 10);

//再次尝试进行1.0*分钟的更改
        BOOST_CHECK( testWallet.SelectCoinsMinConf(1 * MIN_CHANGE, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
BOOST_CHECK_EQUAL(nValueRet, 1 * MIN_CHANGE); //我们应该得到确切的数额

//运行“mtgox”测试（请参阅http://blockexplorer.com/tx/29a3efd3ef04f9153d47a990bd7b048a4b2d213daaa5fb8ed670fb85f13bdbcf）
//他们试图把10枚5万币合并成一枚5万币，结果换了5万币。
        empty_wallet();
        for (int j = 0; j < 20; j++)
            add_coin(50000 * COIN);

        BOOST_CHECK( testWallet.SelectCoinsMinConf(500000 * COIN, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
BOOST_CHECK_EQUAL(nValueRet, 500000 * COIN); //我们应该得到确切的数额
BOOST_CHECK_EQUAL(setCoinsRet.size(), 10U); //十枚硬币

//如果小硬币中没有足够的硬币来进行至少1*min的兑换（0.5+0.6+0.7<1.0+1.0）。
//无论如何，我们需要尝试找到一个精确的子集。

//有时会失败，因此我们使用下一个最大的硬币：
        empty_wallet();
        add_coin(MIN_CHANGE * 5 / 10);
        add_coin(MIN_CHANGE * 6 / 10);
        add_coin(MIN_CHANGE * 7 / 10);
        add_coin(1111 * MIN_CHANGE);
        BOOST_CHECK( testWallet.SelectCoinsMinConf(1 * MIN_CHANGE, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
BOOST_CHECK_EQUAL(nValueRet, 1111 * MIN_CHANGE); //我们得到更大的硬币
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);

//但有时是可能的，我们使用一个精确的子集（0.4+0.6=1.0）
        empty_wallet();
        add_coin(MIN_CHANGE * 4 / 10);
        add_coin(MIN_CHANGE * 6 / 10);
        add_coin(MIN_CHANGE * 8 / 10);
        add_coin(1111 * MIN_CHANGE);
        BOOST_CHECK( testWallet.SelectCoinsMinConf(MIN_CHANGE, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
BOOST_CHECK_EQUAL(nValueRet, MIN_CHANGE);   //我们应该得到确切的数额
BOOST_CHECK_EQUAL(setCoinsRet.size(), 2U); //两个硬币0.4+0.6

//避免小变化的测试
        empty_wallet();
        add_coin(MIN_CHANGE * 5 / 100);
        add_coin(MIN_CHANGE * 1);
        add_coin(MIN_CHANGE * 100);

//试图用这三枚硬币制造100.01
        BOOST_CHECK(testWallet.SelectCoinsMinConf(MIN_CHANGE * 10001 / 100, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
BOOST_CHECK_EQUAL(nValueRet, MIN_CHANGE * 10105 / 100); //我们应该得到所有的硬币
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 3U);

//但是，如果我们试图制造99.9，我们应该取两个小硬币中较大的一个，以避免零钱。
        BOOST_CHECK(testWallet.SelectCoinsMinConf(MIN_CHANGE * 9990 / 100, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
        BOOST_CHECK_EQUAL(nValueRet, 101 * MIN_CHANGE);
        BOOST_CHECK_EQUAL(setCoinsRet.size(), 2U);
      }

//多输入测试
      for (CAmount amt=1500; amt < COIN; amt*=10) {
           empty_wallet();
//创建676个输入（=（旧的max_标准_tx_大小=100000）/每个输入148个字节）
           for (uint16_t j = 0; j < 676; j++)
               add_coin(amt);

//我们只创建一次钱包以节省时间，但我们仍然运行硬币选择运行测试时间。
           for (int i = 0; i < RUN_TESTS; i++) {
             BOOST_CHECK(testWallet.SelectCoinsMinConf(2000, filter_confirmed, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));

             if (amt - 2000 < MIN_CHANGE) {
//需要多个输入：
                 uint16_t returnSize = std::ceil((2000.0 + MIN_CHANGE)/amt);
                 CAmount returnValue = amt * returnSize;
                 BOOST_CHECK_EQUAL(nValueRet, returnValue);
                 BOOST_CHECK_EQUAL(setCoinsRet.size(), returnSize);
             } else {
//一个输入就足够了：
                 BOOST_CHECK_EQUAL(nValueRet, amt);
                 BOOST_CHECK_EQUAL(setCoinsRet.size(), 1U);
             }
           }
      }

//测试随机性
      {
          empty_wallet();
          for (int i2 = 0; i2 < 100; i2++)
              add_coin(COIN);

//同样，我们只创建一次钱包以节省时间，但我们仍然运行硬币选择运行测试次数。
          for (int i = 0; i < RUN_TESTS; i++) {
//从100个硬币中挑选50个并不取决于洗牌，
//但这取决于随机近似码的随机性
            BOOST_CHECK(testWallet.SelectCoinsMinConf(50 * COIN, filter_standard, GroupCoins(vCoins), setCoinsRet , nValueRet, coin_selection_params, bnb_used));
            BOOST_CHECK(testWallet.SelectCoinsMinConf(50 * COIN, filter_standard, GroupCoins(vCoins), setCoinsRet2, nValueRet, coin_selection_params, bnb_used));
            BOOST_CHECK(!equal_sets(setCoinsRet, setCoinsRet2));

            int fails = 0;
            for (int j = 0; j < RANDOM_REPEATS; j++)
            {
//从100个相同的硬币中选择1取决于洗牌；此测试将失败1%。
//随机运行测试\重复次，只有在所有测试都失败时才抱怨
                BOOST_CHECK(testWallet.SelectCoinsMinConf(COIN, filter_standard, GroupCoins(vCoins), setCoinsRet , nValueRet, coin_selection_params, bnb_used));
                BOOST_CHECK(testWallet.SelectCoinsMinConf(COIN, filter_standard, GroupCoins(vCoins), setCoinsRet2, nValueRet, coin_selection_params, bnb_used));
                if (equal_sets(setCoinsRet, setCoinsRet2))
                    fails++;
            }
            BOOST_CHECK_NE(fails, RANDOM_REPEATS);
          }

//零钱加75美分。不足以赚90美分，
//然后试着挣90美分。有多个相互竞争的“最小的、更大的”硬币，
//其中一个应该随机挑选
          add_coin(5 * CENT);
          add_coin(10 * CENT);
          add_coin(15 * CENT);
          add_coin(20 * CENT);
          add_coin(25 * CENT);

          for (int i = 0; i < RUN_TESTS; i++) {
            int fails = 0;
            for (int j = 0; j < RANDOM_REPEATS; j++)
            {
//从100个相同的硬币中选择1取决于洗牌；此测试将失败1%。
//随机运行测试\重复次，只有在所有测试都失败时才抱怨
                BOOST_CHECK(testWallet.SelectCoinsMinConf(90*CENT, filter_standard, GroupCoins(vCoins), setCoinsRet , nValueRet, coin_selection_params, bnb_used));
                BOOST_CHECK(testWallet.SelectCoinsMinConf(90*CENT, filter_standard, GroupCoins(vCoins), setCoinsRet2, nValueRet, coin_selection_params, bnb_used));
                if (equal_sets(setCoinsRet, setCoinsRet2))
                    fails++;
            }
            BOOST_CHECK_NE(fails, RANDOM_REPEATS);
          }
      }

    empty_wallet();
}

BOOST_AUTO_TEST_CASE(ApproximateBestSubset)
{
    CoinSet setCoinsRet;
    CAmount nValueRet;
    bool bnb_used;

    LOCK(testWallet.cs_wallet);

    empty_wallet();

//测试v值排序顺序
    for (int i = 0; i < 1000; i++)
        add_coin(1000 * COIN);
    add_coin(3 * COIN);

    BOOST_CHECK(testWallet.SelectCoinsMinConf(1003 * COIN, filter_standard, GroupCoins(vCoins), setCoinsRet, nValueRet, coin_selection_params, bnb_used));
    BOOST_CHECK_EQUAL(nValueRet, 1003 * COIN);
    BOOST_CHECK_EQUAL(setCoinsRet.size(), 2U);

    empty_wallet();
}

//测试在理想条件下，硬币选择器总是能够找到一个能够支付目标值的解决方案
BOOST_AUTO_TEST_CASE(SelectCoins_test)
{
//随机发生器材料
    std::default_random_engine generator;
    std::exponential_distribution<double> distribution (100);
    FastRandomContext rand;

//运行此测试100次
    for (int i = 0; i < 100; ++i)
    {
        empty_wallet();

//制作一个有1000个指数分布随机输入的钱包
        for (int j = 0; j < 1000; ++j)
        {
            add_coin((CAmount)(distribution(generator)*10000000));
        }

//生成100-400范围内的随机费率
        CFeeRate rate(rand.randrange(300) + 100);

//生成介于1000和钱包余额之间的随机目标值
        CAmount target = rand.randrange(balance - 1000) + 1000;

//执行选择
        CoinSelectionParams coin_selection_params_knapsack(false, 34, 148, CFeeRate(0), 0);
        CoinSelectionParams coin_selection_params_bnb(true, 34, 148, CFeeRate(0), 0);
        CoinSet out_set;
        CAmount out_value = 0;
        bool bnb_used = false;
        BOOST_CHECK(testWallet.SelectCoinsMinConf(target, filter_standard, GroupCoins(vCoins), out_set, out_value, coin_selection_params_bnb, bnb_used) ||
                    testWallet.SelectCoinsMinConf(target, filter_standard, GroupCoins(vCoins), out_set, out_value, coin_selection_params_knapsack, bnb_used));
        BOOST_CHECK_GE(out_value, target);
    }
}

BOOST_AUTO_TEST_SUITE_END()
