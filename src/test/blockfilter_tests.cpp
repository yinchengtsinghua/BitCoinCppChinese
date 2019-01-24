
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <test/data/blockfilters.json.h>
#include <test/test_bitcoin.h>

#include <blockfilter.h>
#include <core_io.h>
#include <serialize.h>
#include <streams.h>
#include <univalue.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(blockfilter_tests)

BOOST_AUTO_TEST_CASE(gcsfilter_test)
{
    GCSFilter::ElementSet included_elements, excluded_elements;
    for (int i = 0; i < 100; ++i) {
        GCSFilter::Element element1(32);
        element1[0] = i;
        included_elements.insert(std::move(element1));

        GCSFilter::Element element2(32);
        element2[1] = i;
        excluded_elements.insert(std::move(element2));
    }

    GCSFilter filter({0, 0, 10, 1 << 10}, included_elements);
    for (const auto& element : included_elements) {
        BOOST_CHECK(filter.Match(element));

        auto insertion = excluded_elements.insert(element);
        BOOST_CHECK(filter.MatchAny(excluded_elements));
        excluded_elements.erase(insertion.first);
    }
}

BOOST_AUTO_TEST_CASE(gcsfilter_default_constructor)
{
    GCSFilter filter;
    BOOST_CHECK_EQUAL(filter.GetN(), 0);
    BOOST_CHECK_EQUAL(filter.GetEncoded().size(), 1);

    const GCSFilter::Params& params = filter.GetParams();
    BOOST_CHECK_EQUAL(params.m_siphash_k0, 0);
    BOOST_CHECK_EQUAL(params.m_siphash_k1, 0);
    BOOST_CHECK_EQUAL(params.m_P, 0);
    BOOST_CHECK_EQUAL(params.m_M, 1);
}

BOOST_AUTO_TEST_CASE(blockfilter_basic_test)
{
    CScript included_scripts[5], excluded_scripts[3];

//前两个是单个事务的输出。
    included_scripts[0] << std::vector<unsigned char>(0, 65) << OP_CHECKSIG;
    included_scripts[1] << OP_DUP << OP_HASH160 << std::vector<unsigned char>(1, 20) << OP_EQUALVERIFY << OP_CHECKSIG;

//第三个是第二个事务中的输出。
    included_scripts[2] << OP_1 << std::vector<unsigned char>(2, 33) << OP_1 << OP_CHECKMULTISIG;

//最后两个由一个事务花费。
    included_scripts[3] << OP_0 << std::vector<unsigned char>(3, 32);
    included_scripts[4] << OP_4 << OP_ADD << OP_8 << OP_EQUAL;

//返回输出是第二个事务的输出。
    excluded_scripts[0] << OP_RETURN << std::vector<unsigned char>(4, 40);

//此脚本与块完全无关。
    excluded_scripts[1] << std::vector<unsigned char>(5, 33) << OP_CHECKSIG;

    CMutableTransaction tx_1;
    tx_1.vout.emplace_back(100, included_scripts[0]);
    tx_1.vout.emplace_back(200, included_scripts[1]);

    CMutableTransaction tx_2;
    tx_2.vout.emplace_back(300, included_scripts[2]);
    tx_2.vout.emplace_back(0, excluded_scripts[0]);
tx_2.vout.emplace_back(400, excluded_scripts[2]); //脚本是空的

    CBlock block;
    block.vtx.push_back(MakeTransactionRef(tx_1));
    block.vtx.push_back(MakeTransactionRef(tx_2));

    CBlockUndo block_undo;
    block_undo.vtxundo.emplace_back();
    block_undo.vtxundo.back().vprevout.emplace_back(CTxOut(500, included_scripts[3]), 1000, true);
    block_undo.vtxundo.back().vprevout.emplace_back(CTxOut(600, included_scripts[4]), 10000, false);
    block_undo.vtxundo.back().vprevout.emplace_back(CTxOut(700, excluded_scripts[2]), 100000, false);

    BlockFilter block_filter(BlockFilterType::BASIC, block, block_undo);
    const GCSFilter& filter = block_filter.GetFilter();

    for (const CScript& script : included_scripts) {
        BOOST_CHECK(filter.Match(GCSFilter::Element(script.begin(), script.end())));
    }
    for (const CScript& script : excluded_scripts) {
        BOOST_CHECK(!filter.Match(GCSFilter::Element(script.begin(), script.end())));
    }

//测试序列化/非序列化。
    BlockFilter block_filter2;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << block_filter;
    stream >> block_filter2;

    BOOST_CHECK_EQUAL(block_filter.GetFilterType(), block_filter2.GetFilterType());
    BOOST_CHECK_EQUAL(block_filter.GetBlockHash(), block_filter2.GetBlockHash());
    BOOST_CHECK(block_filter.GetEncodedFilter() == block_filter2.GetEncodedFilter());
}

BOOST_AUTO_TEST_CASE(blockfilters_json_test)
{
    UniValue json;
    std::string json_data(json_tests::blockfilters,
                          json_tests::blockfilters + sizeof(json_tests::blockfilters));
    if (!json.read(json_data) || !json.isArray()) {
        BOOST_ERROR("Parse error.");
        return;
    }

    const UniValue& tests = json.get_array();
    for (unsigned int i = 0; i < tests.size(); i++) {
        UniValue test = tests[i];
        std::string strTest = test.write();

        if (test.size() == 1) {
            continue;
        } else if (test.size() < 7) {
            BOOST_ERROR("Bad test: " << strTest);
            continue;
        }

        unsigned int pos = 0;
        /*nt block_height=*/test[pos++]get_int（）；
        uint256块哈希；
        boost_check（parsehashstr（test[pos++]get_str（），block_hash））；

        块块；
        boost_require（decodeHexBLK（block，test[pos++]get_str（））；

        cblockundo块撤消；
        block_undo.vtxundo.emplace_back（）；
        ctxundo&tx_undo=块_undo.vtxundo.back（）；
        const univalue&prev_scripts=test[pos++]get_array（）；
        for（unsigned int ii=0；ii<prev_scripts.size（）；ii++）
            std:：vector<unsigned char>raw_script=parseHex（prev_scripts[ii].get_str（））；
            ctxout txout（0，cscript（raw_script.begin（），raw_script.end（））；
            tx撤消.vprevout.emplace返回（txout，0，false）；
        }

        uint256 prev_filter_header_基本；
        boost_check（parsehashstr（test[pos++]get_str（），prev_filter_header_basic））；
        std:：vector<unsigned char>filter_basic=parseHex（test[pos++].get_str（））；
        uint256过滤器_header_basic；
        boost_check（parsehashstr（test[pos++]get_str（），filter_header_basic））；

        block filter computed_filter_basic（blockfiltertype:：basic，block，block_undo）；
        boost_check（computed_filter_basic.getfilter（）.getencoded（）==filter_basic）；

        uint256 computed_header_basic=computed_filter_basic.computeheader（prev_filter_header_basic）；
        Boost_check（计算的_header_basic==filter_header_basic）；
    }
}

Boost_Auto_测试套件_end（）
