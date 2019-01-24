
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
#include <key.h>

#include <base58.h>
#include <script/script.h>
#include <uint256.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <test/test_bitcoin.h>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>
#include <rapidcheck/boost_test.h>
#include <rapidcheck/gen/Arbitrary.h>
#include <rapidcheck/Gen.h>

#include <test/gen/crypto_gen.h>

BOOST_FIXTURE_TEST_SUITE(key_properties, BasicTestingSetup)

/*检查CKey唯一性*/
RC_BOOST_PROP(key_uniqueness, (const CKey& key1, const CKey& key2))
{
    RC_ASSERT(!(key1 == key2));
}

/*验证私钥是否生成正确的公钥*/
RC_BOOST_PROP(key_generates_correct_pubkey, (const CKey& key))
{
    CPubKey pubKey = key.GetPubKey();
    RC_ASSERT(key.VerifyPubKey(pubKey));
}

/*使用“set”函数创建一个ckey必须给我们相同的键*/
RC_BOOST_PROP(key_set_symmetry, (const CKey& key))
{
    CKey key1;
    key1.Set(key.begin(), key.end(), key.IsCompressed());
    RC_ASSERT(key1 == key);
}

/*创建一个CKey，对一段数据签名，然后用公钥验证它*/
RC_BOOST_PROP(key_sign_symmetry, (const CKey& key, const uint256& hash))
{
    std::vector<unsigned char> vchSig;
    key.Sign(hash, vchSig, 0);
    const CPubKey& pubKey = key.GetPubKey();
    RC_ASSERT(pubKey.Verify(hash, vchSig));
}
BOOST_AUTO_TEST_SUITE_END()
