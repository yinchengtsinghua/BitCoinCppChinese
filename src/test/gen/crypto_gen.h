
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
#ifndef BITCOIN_TEST_GEN_CRYPTO_GEN_H
#define BITCOIN_TEST_GEN_CRYPTO_GEN_H

#include <key.h>
#include <random.h>
#include <uint256.h>
#include <rapidcheck/gen/Arbitrary.h>
#include <rapidcheck/Gen.h>
#include <rapidcheck/gen/Create.h>
#include <rapidcheck/gen/Numeric.h>

/*生成1到15个用于op_checkmultisig的键*/
rc::Gen<std::vector<CKey>> MultisigKeys();

namespace rc
{
/*一个新的凯伊发电机*/
template <>
struct Arbitrary<CKey> {
    static Gen<CKey> arbitrary()
    {
        return rc::gen::map<int>([](int x) {
            CKey key;
            key.MakeNewKey(true);
            return key;
        });
    };
};

/*CPrivkey的发生器*/
template <>
struct Arbitrary<CPrivKey> {
    static Gen<CPrivKey> arbitrary()
    {
        return gen::map(gen::arbitrary<CKey>(), [](const CKey& key) {
            return key.GetPrivKey();
        });
    };
};

/*新CPubkey的发电机*/
template <>
struct Arbitrary<CPubKey> {
    static Gen<CPubKey> arbitrary()
    {
        return gen::map(gen::arbitrary<CKey>(), [](const CKey& key) {
            return key.GetPubKey();
        });
    };
};
/*生成任意uint256*/
template <>
struct Arbitrary<uint256> {
    static Gen<uint256> arbitrary()
    {
        return rc::gen::just(GetRandHash());
    };
};
} //命名空间RC
#endif
