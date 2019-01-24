
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <bench/bench.h>
#include <chainparams.h>
#include <coins.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <crypto/sha256.h>
#include <miner.h>
#include <policy/policy.h>
#include <pow.h>
#include <scheduler.h>
#include <txdb.h>
#include <txmempool.h>
#include <util/time.h>
#include <validation.h>
#include <validationinterface.h>

#include <boost/thread.hpp>

#include <list>
#include <vector>

static std::shared_ptr<CBlock> PrepareBlock(const CScript& coinbase_scriptPubKey)
{
    auto block = std::make_shared<CBlock>(
        BlockAssembler{Params()}
            .CreateNewBlock(coinbase_scriptPubKey)
            ->block);

    block->nTime = ::chainActive.Tip()->GetMedianTimePast() + 1;
    block->hashMerkleRoot = BlockMerkleRoot(*block);

    return block;
}


static CTxIn MineBlock(const CScript& coinbase_scriptPubKey)
{
    auto block = PrepareBlock(coinbase_scriptPubKey);

    while (!CheckProofOfWork(block->GetHash(), block->nBits, Params().GetConsensus())) {
        ++block->nNonce;
        assert(block->nNonce);
    }

    bool processed{ProcessNewBlock(Params(), block, true, nullptr)};
    assert(processed);

    return CTxIn{block->vtx[0]->GetHash(), 0};
}


static void AssembleBlock(benchmark::State& state)
{
    const std::vector<unsigned char> op_true{OP_TRUE};
    CScriptWitness witness;
    witness.stack.push_back(op_true);

    uint256 witness_program;
    CSHA256().Write(&op_true[0], op_true.size()).Finalize(witness_program.begin());

    const CScript SCRIPT_PUB{CScript(OP_0) << std::vector<unsigned char>{witness_program.begin(), witness_program.end()}};

//切换到REGTEST以便更快地挖掘
//Segwit也是活跃的，因此我们可以包括见证交易
    SelectParams(CBaseChainParams::REGTEST);

    InitScriptExecutionCache();

    boost::thread_group thread_group;
    CScheduler scheduler;
    {
        ::pblocktree.reset(new CBlockTreeDB(1 << 20, true));
        ::pcoinsdbview.reset(new CCoinsViewDB(1 << 23, true));
        ::pcoinsTip.reset(new CCoinsViewCache(pcoinsdbview.get()));

        const CChainParams& chainparams = Params();
        thread_group.create_thread(std::bind(&CScheduler::serviceQueue, &scheduler));
        GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);
        LoadGenesisBlock(chainparams);
        CValidationState state;
        ActivateBestChain(state, chainparams);
        assert(::chainActive.Tip() != nullptr);
        const bool witness_enabled{IsWitnessEnabled(::chainActive.Tip(), chainparams.GetConsensus())};
        assert(witness_enabled);
    }

//收集一些零散的交易，这些交易花费了我们所开采区块的货币基础。
    constexpr size_t NUM_BLOCKS{200};
    std::array<CTransactionRef, NUM_BLOCKS - COINBASE_MATURITY + 1> txs;
    for (size_t b{0}; b < NUM_BLOCKS; ++b) {
        CMutableTransaction tx;
        tx.vin.push_back(MineBlock(SCRIPT_PUB));
        tx.vin.back().scriptWitness = witness;
        tx.vout.emplace_back(1337, SCRIPT_PUB);
        if (NUM_BLOCKS - b >= COINBASE_MATURITY)
            txs.at(b) = MakeTransactionRef(tx);
    }
    {
LOCK(::cs_main); //必须用于：：AcceptToMoryPool。

        for (const auto& txr : txs) {
            CValidationState state;
            /*l ret：：acceptToMoryPool（：：mempool，state，txr，nullptr/*pfMissingInputs*/，nullptr/*pltxnReplaced*/，false/*绕过限制*/，/*nabsurdfee*/0）；
            断言（RET）；
        }
    }

    while（state.keeprunning（））
        PrepareBlock（脚本\u pub）；
    }

    thread_group.interrupt_all（）；
    thread_group.join_all（）；
    getmainsignals（）.flushbackgroundcallbacks（）；
    getMainsignals（）.UnregisterBackgroundSignalScheduler（）；
}

基准（汇编块，700）；
