
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2016-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <bench/bench.h>
#include <coins.h>
#include <policy/policy.h>
#include <wallet/crypter.h>

#include <vector>

//Fixme:在test/transaction_tests.cpp中使用setupdummyinputs进行重复数据消除。
//
//帮助程序：创建两个虚拟事务，每个事务使用
//两个输出。第一个有11个和50个硬币输出
//支付给Tx Pubkey，第二个21和22个硬币输出
//支付给一个tx-pubkeyhash。
//
static std::vector<CMutableTransaction>
SetupDummyInputs(CBasicKeyStore& keystoreRet, CCoinsViewCache& coinsRet)
{
    std::vector<CMutableTransaction> dummyTransactions;
    dummyTransactions.resize(2);

//向密钥库添加一些密钥：
    CKey key[4];
    for (int i = 0; i < 4; i++) {
        key[i].MakeNewKey(i % 2);
        keystoreRet.AddKey(key[i]);
    }

//创建一些虚拟输入事务
    dummyTransactions[0].vout.resize(2);
    dummyTransactions[0].vout[0].nValue = 11 * COIN;
    dummyTransactions[0].vout[0].scriptPubKey << ToByteVector(key[0].GetPubKey()) << OP_CHECKSIG;
    dummyTransactions[0].vout[1].nValue = 50 * COIN;
    dummyTransactions[0].vout[1].scriptPubKey << ToByteVector(key[1].GetPubKey()) << OP_CHECKSIG;
    AddCoins(coinsRet, CTransaction(dummyTransactions[0]), 0);

    dummyTransactions[1].vout.resize(2);
    dummyTransactions[1].vout[0].nValue = 21 * COIN;
    dummyTransactions[1].vout[0].scriptPubKey = GetScriptForDestination(key[2].GetPubKey().GetID());
    dummyTransactions[1].vout[1].nValue = 22 * COIN;
    dummyTransactions[1].vout[1].scriptPubKey = GetScriptForDestination(key[3].GetPubKey().GetID());
    AddCoins(coinsRet, CTransaction(dummyTransactions[1]), 0);

    return dummyTransactions;
}

//用于简单访问ccoinsviewcache数据库的微基准。注自
//Laanwj，“但是，复制客户机的实际使用模式很困难，
//很多时候，数据库的微观基准显示完全不同
//特征，例如重新索引计时。但这不是
//每一个基准。”
//（https://github.com/bitcoin/bitcoin/issues/7883_issuecomment-224807484）
static void CCoinsCaching(benchmark::State& state)
{
    CBasicKeyStore keystore;
    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    std::vector<CMutableTransaction> dummyTransactions = SetupDummyInputs(keystore, coins);

    CMutableTransaction t1;
    t1.vin.resize(3);
    t1.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    t1.vin[0].prevout.n = 1;
    t1.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    t1.vin[1].prevout.hash = dummyTransactions[1].GetHash();
    t1.vin[1].prevout.n = 0;
    t1.vin[1].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    t1.vin[2].prevout.hash = dummyTransactions[1].GetHash();
    t1.vin[2].prevout.n = 1;
    t1.vin[2].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    t1.vout.resize(2);
    t1.vout[0].nValue = 90 * COIN;
    t1.vout[0].scriptPubKey << OP_1;

//基准。
    const CTransaction tx_1(t1);
    while (state.KeepRunning()) {
        bool success = AreInputsStandard(tx_1, coins);
        assert(success);
        CAmount value = coins.GetValueIn(tx_1);
        assert(value == (50 + 21 + 22) * COIN);
    }
}

BENCHMARK(CCoinsCaching, 170 * 1000);
