
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2012-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <bench/bench.h>
#include <interfaces/chain.h>
#include <wallet/coinselection.h>
#include <wallet/wallet.h>

#include <set>

static void addCoin(const CAmount& nValue, const CWallet& wallet, std::vector<std::unique_ptr<CWalletTx>>& wtxs)
{
    static int nextLockTime = 0;
    CMutableTransaction tx;
tx.nLockTime = nextLockTime++; //所以所有的事务都有不同的哈希值
    tx.vout.resize(1);
    tx.vout[0].nValue = nValue;
    wtxs.push_back(MakeUnique<CWalletTx>(&wallet, MakeTransactionRef(std::move(tx))));
}

//钱包硬币选择的简单基准。注意，这可能是必要的
//建立更复杂的场景以获得有意义的
//性能测量。从Laanwj，“钱包硬币的选择可能是
//最困难的是，因为您需要更广泛的场景选择，只需测试
//同样的一次又一次并不太有用。生成随机数据是没有用的
//或者用于测量。”
//（https://github.com/bitcoin/bitcoin/issues/7883_issuecomment-224807484）
static void CoinSelection(benchmark::State& state)
{
    auto chain = interfaces::MakeChain();
    const CWallet wallet(*chain, WalletLocation(), WalletDatabase::CreateDummy());
    std::vector<std::unique_ptr<CWalletTx>> wtxs;
    LOCK(wallet.cs_wallet);

//添加硬币。
    for (int i = 0; i < 1000; ++i) {
        addCoin(1000 * COIN, wallet, wtxs);
    }
    addCoin(3 * COIN, wallet, wtxs);

//创建群组
    std::vector<OutputGroup> groups;
    for (const auto& wtx : wtxs) {
        /*tput输出（wtx.get（），0/*iin*/，6*24/*ndepthin*/，true/*spenable*/，true/*solvable*/，true/*safe*/）；
        groups.emplace_back（output.getInputCoin（），6，false，0，0）；
    }

    const coineligibility过滤器标准（1，6，0）；
    const coin selection params coin_selection_params（真，34，148，cfeerate（0），0）；
    while（state.keeprunning（））
        std:：set<cinputcoin>setcoinsret；
        camount nvalueret；
        BoOL BNBULY；
        bool success=wallet.selectcoinsminconf（1003*硬币，过滤_标准，组，setcoinsret，nvalueret，硬币_选择_参数，使用bnb_）；
        断言（成功）；
        断言（nvalueret==1003*硬币）；
        断言（setCoinsret.size（）==2）；
    }
}

typedef std:：set<cinputcoin>coinset；
静态自动测试链=接口：：makechain（）；
static const cwallet testwallet（*testchain，walletlocation（），walletdatabase:：createDummy（））；
std:：vector<std:：unique_ptr<cwallettx>>wtxn；

//从src/wallet/test/coinselector_tests.cpp复制
静态void添加硬币（const camount&nvalue，int ninput，std:：vector<outputgroup>&set）
{
    CmutableTransaction发送；
    tx.vout.resize（输入+1）；
    tx.vout[ninput].nvalue=nvalue；
    std:：unique_ptr<cwallettx>wtx=makeunique<cwallettx>（&testwallet，makeTransactionRef（std:：move（tx）））；
    设置.emplace_back（coutput（wtx.get（），ninput，0，true，true，true）.getinputcoin（），0，true，0，0）；
    wtxn.emplace_back（std:：move（wtx））；
}
//从src/wallet/test/coinselector_tests.cpp复制
静态camount make_hard_case（int utxos，std:：vector<outputgroup>&utxo_pool）
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

静态void bnbexhaustion（benchmark:：state&state）
{
    //设置
    std:：vector<outputgroup>utxo_pool；
    Coinset选择；
    camount值_ret=0；
    camount not_input_fees=0；

    while（state.keeprunning（））
        //基准
        camount target=make_hard_case（17，utxo_pool）；
        selectcoinsbnb（utxo_pool，target，0，selection，value_ret，not_input_fees）；//应该耗尽

        /清理
        utxo_pool.clear（）；
        selection.clear（）；
    }
}

基准（CoinSelection，650）；
基准（bnbexhaustion，650）；
