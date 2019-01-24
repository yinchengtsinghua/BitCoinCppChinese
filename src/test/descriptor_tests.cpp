
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

#include <vector>
#include <string>
#include <script/sign.h>
#include <script/standard.h>
#include <test/test_bitcoin.h>
#include <boost/test/unit_test.hpp>
#include <script/descriptor.h>
#include <util/strencodings.h>

namespace {

void CheckUnparsable(const std::string& prv, const std::string& pub)
{
    FlatSigningProvider keys_priv, keys_pub;
    auto parse_priv = Parse(prv, keys_priv);
    auto parse_pub = Parse(pub, keys_pub);
    BOOST_CHECK(!parse_priv);
    BOOST_CHECK(!parse_pub);
}

constexpr int DEFAULT = 0;
constexpr int RANGE = 1; //应为范围描述符
constexpr int HARDENED = 2; //派生需要访问私钥
constexpr int UNSOLVABLE = 4; //此描述符不应是可解的
constexpr int SIGNABLE = 8; //我们可以用这个描述符签名（当使用实际的bip32派生时，这是不正确的，因为它没有集成在我们的签名代码中）

std::string MaybeUseHInsteadOfApostrophy(std::string ret)
{
    if (InsecureRandBool()) {
        while (true) {
            auto it = ret.find("'");
            if (it != std::string::npos) {
                ret[it] = 'h';
            } else {
                break;
            }
        }
    }
    return ret;
}

const std::set<std::vector<uint32_t>> ONLY_EMPTY{{}};

void Check(const std::string& prv, const std::string& pub, int flags, const std::vector<std::vector<std::string>>& scripts, const std::set<std::vector<uint32_t>>& paths = ONLY_EMPTY)
{
    FlatSigningProvider keys_priv, keys_pub;
    std::set<std::vector<uint32_t>> left_paths = paths;

//检查分析是否成功。
    auto parse_priv = Parse(MaybeUseHInsteadOfApostrophy(prv), keys_priv);
    auto parse_pub = Parse(MaybeUseHInsteadOfApostrophy(pub), keys_pub);
    BOOST_CHECK(parse_priv);
    BOOST_CHECK(parse_pub);

//检查私钥是从私有版本中提取的，而不是从公共版本中提取的。
    BOOST_CHECK(keys_priv.keys.size());
    BOOST_CHECK(!keys_pub.keys.size());

//检查两个版本是否都序列化回公共版本。
    std::string pub1 = parse_priv->ToString();
    std::string pub2 = parse_pub->ToString();
    BOOST_CHECK_EQUAL(pub, pub1);
    BOOST_CHECK_EQUAL(pub, pub2);

//请检查是否可以使用私钥将两者序列化回专用版本，但不能使用私钥。
    std::string prv1;
    BOOST_CHECK(parse_priv->ToPrivateString(keys_priv, prv1));
    BOOST_CHECK_EQUAL(prv, prv1);
    BOOST_CHECK(!parse_priv->ToPrivateString(keys_pub, prv1));
    BOOST_CHECK(parse_pub->ToPrivateString(keys_priv, prv1));
    BOOST_CHECK_EQUAL(prv, prv1);
    BOOST_CHECK(!parse_pub->ToPrivateString(keys_pub, prv1));

//检查isrange on both是否返回预期结果
    BOOST_CHECK_EQUAL(parse_pub->IsRange(), (flags & RANGE) != 0);
    BOOST_CHECK_EQUAL(parse_priv->IsRange(), (flags & RANGE) != 0);

//*对于范围描述符，“scripts”参数是预期结果输出的列表，用于后续
//用于评估描述符的位置（因此“scripts”的第一个元素用于评估
//描述符位于0；第二个位于1；依此类推）。为了验证这一点，我们对描述符进行一次评估，
//“scripts”中的每个元素。
//*对于非范围描述符，我们评估位置0、1和2处的描述符，但预计
//在每种情况下都有相同的结果，即“scripts”的第一个元素。正因为如此，
//在这种情况下，“scripts”必须是一个。
    if (!(flags & RANGE)) assert(scripts.size() == 1);
    size_t max = (flags & RANGE) ? scripts.size() : 3;

//迭代我们将在其中评估描述符的位置。
    for (size_t i = 0; i < max; ++i) {
//调用预期的结果脚本“ref”。
        const auto& ref = scripts[(flags & RANGE) ? i : 0];
//当t=0时，计算“prv”描述符；当t=1时，计算“pub”描述符。
        for (int t = 0; t < 2; ++t) {
//当描述符被强化时，使用对内部私钥的访问进行评估。
            const FlatSigningProvider& key_provider = (flags & HARDENED) ? keys_priv : keys_pub;

//在poisition`i`中计算由't'选择的描述符。
            FlatSigningProvider script_provider, script_provider_cached;
            std::vector<CScript> spks, spks_cached;
            std::vector<unsigned char> cache;
            BOOST_CHECK((t ? parse_priv : parse_pub)->Expand(i, key_provider, spks, script_provider, &cache));

//将输出与预期结果进行比较。
            BOOST_CHECK_EQUAL(spks.size(), ref.size());

//尝试使用缓存数据再次展开，然后进行比较。
            BOOST_CHECK(parse_pub->ExpandFromCache(i, cache, spks_cached, script_provider_cached));
            BOOST_CHECK(spks == spks_cached);
            BOOST_CHECK(script_provider.pubkeys == script_provider_cached.pubkeys);
            BOOST_CHECK(script_provider.scripts == script_provider_cached.scripts);
            BOOST_CHECK(script_provider.origins == script_provider_cached.origins);

//对于每个生成的脚本，验证可解决性，并在可能的情况下，尝试签署一个使用它的事务。
            for (size_t n = 0; n < spks.size(); ++n) {
                BOOST_CHECK_EQUAL(ref[n], HexStr(spks[n].begin(), spks[n].end()));
                BOOST_CHECK_EQUAL(IsSolvable(Merge(key_provider, script_provider), spks[n]), (flags & UNSOLVABLE) == 0);

                if (flags & SIGNABLE) {
                    CMutableTransaction spend;
                    spend.vin.resize(1);
                    spend.vout.resize(1);
                    BOOST_CHECK_MESSAGE(SignSignature(Merge(keys_priv, script_provider), spks[n], spend, 0, 1, SIGHASH_ALL), prv);
                }

                /*从生成的脚本推断描述符，并验证其可解性和往返性。*/
                auto inferred = InferDescriptor(spks[n], script_provider);
                BOOST_CHECK_EQUAL(inferred->IsSolvable(), !(flags & UNSOLVABLE));
                std::vector<CScript> spks_inferred;
                FlatSigningProvider provider_inferred;
                BOOST_CHECK(inferred->Expand(0, provider_inferred, spks_inferred, provider_inferred));
                BOOST_CHECK_EQUAL(spks_inferred.size(), 1);
                BOOST_CHECK(spks_inferred[0] == spks[n]);
                BOOST_CHECK_EQUAL(IsSolvable(provider_inferred, spks_inferred[0]), !(flags & UNSOLVABLE));
                BOOST_CHECK(provider_inferred.origins == script_provider.origins);
            }

//测试观察到的密钥路径是否存在于“paths”变量中（该变量包含预期的、未观察到的路径），
//然后把它从那套里拿出来。
            for (const auto& origin : script_provider.origins) {
                BOOST_CHECK_MESSAGE(paths.count(origin.second.path), "Unexpected key path: " + prv);
                left_paths.erase(origin.second.path);
            }
        }
    }

//确认没有未观察到的预期路径。
    BOOST_CHECK_MESSAGE(left_paths.empty(), "Not all expected key paths found: " + prv);
}

}

BOOST_FIXTURE_TEST_SUITE(descriptor_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(descriptor_test)
{
//基本单键压缩
    Check("combo(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1)", "combo(03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)", SIGNABLE, {{"2103a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bdac","76a9149a1c78a507689f6f54b847ad1cef1e614ee23f1e88ac","00149a1c78a507689f6f54b847ad1cef1e614ee23f1e","a91484ab21b1b2fd065d4504ff693d832434b6108d7b87"}});
    Check("pk(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1)", "pk(03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)", SIGNABLE, {{"2103a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bdac"}});
    Check("pkh([deadbeef/1/2'/3/4']L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1)", "pkh([deadbeef/1/2'/3/4']03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)", SIGNABLE, {{"76a9149a1c78a507689f6f54b847ad1cef1e614ee23f1e88ac"}}, {{1,0x80000002UL,3,0x80000004UL}});
    Check("wpkh(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1)", "wpkh(03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)", SIGNABLE, {{"00149a1c78a507689f6f54b847ad1cef1e614ee23f1e"}});
    Check("sh(wpkh(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1))", "sh(wpkh(03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd))", SIGNABLE, {{"a91484ab21b1b2fd065d4504ff693d832434b6108d7b87"}});

//基本单键未压缩
    Check("combo(5KYZdUEo39z3FPrtuX2QbbwGnNP5zTd7yyr2SC1j299sBCnWjss)", "combo(04a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235)", SIGNABLE, {{"4104a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235ac","76a914b5bd079c4d57cc7fc28ecf8213a6b791625b818388ac"}});
    Check("pk(5KYZdUEo39z3FPrtuX2QbbwGnNP5zTd7yyr2SC1j299sBCnWjss)", "pk(04a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235)", SIGNABLE, {{"4104a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235ac"}});
    Check("pkh(5KYZdUEo39z3FPrtuX2QbbwGnNP5zTd7yyr2SC1j299sBCnWjss)", "pkh(04a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235)", SIGNABLE, {{"76a914b5bd079c4d57cc7fc28ecf8213a6b791625b818388ac"}});
CheckUnparsable("wpkh(5KYZdUEo39z3FPrtuX2QbbwGnNP5zTd7yyr2SC1j299sBCnWjss)", "wpkh(04a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235)"); //见证中没有未压缩的密钥
CheckUnparsable("wsh(pk(5KYZdUEo39z3FPrtuX2QbbwGnNP5zTd7yyr2SC1j299sBCnWjss))", "wsh(pk(04a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235))"); //见证中没有未压缩的密钥
CheckUnparsable("sh(wpkh(5KYZdUEo39z3FPrtuX2QbbwGnNP5zTd7yyr2SC1j299sBCnWjss))", "sh(wpkh(04a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235))"); //见证中没有未压缩的密钥

//一些非常规的单键结构
    Check("sh(pk(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1))", "sh(pk(03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd))", SIGNABLE, {{"a9141857af51a5e516552b3086430fd8ce55f7c1a52487"}});
    Check("sh(pkh(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1))", "sh(pkh(03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd))", SIGNABLE, {{"a9141a31ad23bf49c247dd531a623c2ef57da3c400c587"}});
    Check("wsh(pk(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1))", "wsh(pk(03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd))", SIGNABLE, {{"00202e271faa2325c199d25d22e1ead982e45b64eeb4f31e73dbdf41bd4b5fec23fa"}});
    Check("wsh(pkh(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1))", "wsh(pkh(03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd))", SIGNABLE, {{"0020338e023079b91c58571b20e602d7805fb808c22473cbc391a41b1bd3a192e75b"}});
    Check("sh(wsh(pk(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1)))", "sh(wsh(pk(03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)))", SIGNABLE, {{"a91472d0c5a3bfad8c3e7bd5303a72b94240e80b6f1787"}});
    Check("sh(wsh(pkh(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1)))", "sh(wsh(pkh(03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)))", SIGNABLE, {{"a914b61b92e2ca21bac1e72a3ab859a742982bea960a87"}});

//具有bip32派生的版本
    Check("combo([01234567]xprvA1RpRA33e1JQ7ifknakTFpgNXPmW2YvmhqLQYMmrj4xJXXWYpDPS3xz7iAxn8L39njGVyuoseXzU6rcxFLJ8HFsTjSyQbLYnMpCqE2VbFWc)", "combo([01234567]xpub6ERApfZwUNrhLCkDtcHTcxd75RbzS1ed54G1LkBUHQVHQKqhMkhgbmJbZRkrgZw4koxb5JaHWkY4ALHY2grBGRjaDMzQLcgJvLJuZZvRcEL)", SIGNABLE, {{"2102d2b36900396c9282fa14628566582f206a5dd0bcc8d5e892611806cafb0301f0ac","76a91431a507b815593dfc51ffc7245ae7e5aee304246e88ac","001431a507b815593dfc51ffc7245ae7e5aee304246e","a9142aafb926eb247cb18240a7f4c07983ad1f37922687"}});
    Check("pk(xprv9uPDJpEQgRQfDcW7BkF7eTya6RPxXeJCqCJGHuCJ4GiRVLzkTXBAJMu2qaMWPrS7AANYqdq6vcBcBUdJCVVFceUvJFjaPdGZ2y9WACViL4L/0)", "pk(xpub68NZiKmJWnxxS6aaHmn81bvJeTESw724CRDs6HbuccFQN9Ku14VQrADWgqbhhTHBaohPX4CjNLf9fq9MYo6oDaPPLPxSb7gwQN3ih19Zm4Y/0)", DEFAULT, {{"210379e45b3cf75f9c5f9befd8e9506fb962f6a9d185ac87001ec44a8d3df8d4a9e3ac"}}, {{0}});
    Check("pkh(xprv9s21ZrQH143K31xYSDQpPDxsXRTUcvj2iNHm5NUtrGiGG5e2DtALGdso3pGz6ssrdK4PFmM8NSpSBHNqPqm55Qn3LqFtT2emdEXVYsCzC2U/2147483647'/0)", "pkh(xpub661MyMwAqRbcFW31YEwpkMuc5THy2PSt5bDMsktWQcFF8syAmRUapSCGu8ED9W6oDMSgv6Zz8idoc4a6mr8BDzTJY47LJhkJ8UB7WEGuduB/2147483647'/0)", HARDENED, {{"76a914ebdc90806a9c4356c1c88e42216611e1cb4c1c1788ac"}}, {{0xFFFFFFFFUL,0}});
    /*ck("wpkh([ffffffff/13']xprv9vHkqa6EV4sPZHYqZznhT2NPtPCjKuDKGY38FBWLvgaDx45zo9WQRUT3dKYnjwih2yJD9mkrocEZXo1ex8G81dwSM1fwqWpWkeS3v86pgKt/1/2/*)", "wpkh([ffffffff/13']xpub69H7F5d8KSRgmmdJg2KhpAK8SR3DjMwAdkxj3ZuxV27CprR9LgpeyGmXUbC6wb7ERfvrnKZjXoUmmDznezpbZb7ap6r1D3tgFxHmwMkQTPH/1/2/*)", RANGE, {{"0014326b2249e3a25d5dc60935f044ee835d090ba859"},{"0014af0bd98abc2f2cae66e368a39ffe2d32984fb7“，”00141fa798efd1cbf95cebf912c031b8a4a6e9fb9f27“，，0x8000000 dul，1，2，0，0x8000000 dul，1，2，1，0x8000000 dul，1，2，2）；
    Check("sh(wpkh(xprv9s21ZrQH143K3QTDL4LXw2F7HEK3wJUD2nW2nRk4stbPy6cq3jPPqjiChkVvvNKmPGJxWUtg6LnF5kejMRNNU3TGtRBeJgk33yuGBxrMPHi/10/20/30/40/*'))", "sh(wpkh(xpub661MyMwAqRbcFtXgS5sYJABqqG9YLmC4Q1Rdap9gSE8NqtwybGhePY2gZ29ESFjqJoCu1Rupje8YtGqsefD265TMg7usUDFdp6W1EGMcet8/10/20/30/40/*'))", RANGE | HARDENED, {{"a9149a4d9901d6af519b2a23d4a2f51650fcba87ce7B87“，”A914BED59FC0024FAE941D6E20A3B44A109AE7440129287“，”A91148483AA1116EB9C05C48272BADA4B1DB24AF654387“，，10，20，30，40，0x800000000UL，10，20，30，0x800000001UL，10，20，30，40，0x800000002UL）；
    
    
    

    //multisig结构
    
    
    Check("wsh(multi(2,xprv9s21ZrQH143K31xYSDQpPDxsXRTUcvj2iNHm5NUtrGiGG5e2DtALGdso3pGz6ssrdK4PFmM8NSpSBHNqPqm55Qn3LqFtT2emdEXVYsCzC2U/2147483647'/0,xprv9vHkqa6EV4sPZHYqZznhT2NPtPCjKuDKGY38FBWLvgaDx45zo9WQRUT3dKYnjwih2yJD9mkrocEZXo1ex8G81dwSM1fwqWpWkeS3v86pgKt/1/2/*,xprv9s21ZrQH143K3QTDL4LXw2F7HEK3wJUD2nW2nRk4stbPy6cq3jPPqjiChkVvvNKmPGJxWUtg6LnF5kejMRNNU3TGtRBeJgk33yuGBxrMPHi/10/20/30/40/*'))", "wsh(multi(2,xpub661MyMwAqRbcFW31YEwpkMuc5THy2PSt5bDMsktWQcFF8syAmRUapSCGu8ED9W6oDMSgv6Zz8idoc4a6mr8BDzTJY47LJhkJ8UB7WEGuduB/2147483647'/0,xpub69H7F5d8KSRgmmdJg2KhpAK8SR3DjMwAdkxj3ZuxV27CprR9LgpeyGmXUbC6wb7ERfvrnKZjXoUmmDznezpbZb7ap6r1D3tgFxHmwMkQTPH/1/2/*,xpub661MyMwAqRbcFtXgS5sYJABqqG9YLmC4Q1Rdap9gSE8NqtwybGhePY2gZ29ESFjqJoCu1Rupje8YtGqsefD265TMg7usUDFdp6W1EGMcet8/10/20/30/40/*'))", HARDENED | RANGE, {{"0020b92623201f3bb7c3771d45b2ad1d0351ea8fbf8cfe0a0e570264e1075fa1948f"},{"002036a08bbe4923af41cf4316817c93b8d37e2f635dd25cfff06bd50df6ae7ea203"},{"0020a96e7ab4607ca6b261bfe3245ffda9c746b28d3f59e83d34820ec0e2b36c139c"}}, {{0xFFFFFFFFUL,0}, {1,2,0，1,2,1，1,2,2，10，20，30，40，0x800000001ul，10，20，30，40，0x800000001ul，10，20，30，40，0x800000002ul）；
    检查（“SH（WSH（多（16，KZAZ5CANAYRK5CANAYRKX3FSLQ2BWJPN7U52GZVXMYY78NDHuqRuxuSJY，KWGNZ6YCCQTYVFZMTC63T KTKBBMRLTSJR2NYVVVVPCKKKK7MR，KXOGYHIFWXV66EFYKCCPM7DZ7TQVQUJHVJVVJJVXYIXQ9X，LbundutSZJWNHynQTF14mV2V2V2UZ2NRQ5N5SYTB4F4FKKMQGEE9F（16，16，KZZZZZZZZZZZZZZZZZZZZJA5QSBKCSPSB7REQJEZKEOD，KxDCSST75HFPAW5QKPZHTAYAQC7P9VO3FI2U4DXD1VGMIBOK，L5EDQJFTKcf5uuuwn6uuuwfrabgdqhdhekcziw42alws3kizu，kzf8uwfcc7bytq8go1xvmkdmynyvmxv5pv7rudicvacopb8i，l3nhubokg2w4vsj5jyzc5cbm97oek6yukvfzxxxffzhecjeykwz，kyjho36dwkyhimvmqtq3gv3pqa4xfcpvuggjad7es888we，kwsfyhkruzpqtyn7m33z4m33tz4xt4vv5xv5xrgjdjd5xgjd5xgjd9ljhdeffl9zqgtjmjqxdbkeekrgzx24hxdgncijkkap，kzgpmbwsdlwkac5urmbgcyabd2wgz7pbogyxr8KT7GCA9UTN5A3，KYBXTPYT7YG4Q9TCA3LKVFRPD1YBHMVCJ2ehawxasqeguxedkp，KZJDE9IJPTKP2A2ON6ZBZS7UUAwhwwwwwwwwc3PHNJ8M8，L1XHRXYNRQKYC4QTOQPX6U5QYYR5ZDYVYBSRMCV5PI3JJ9）），，，，，，，，，，，，，，，，（多（1603669B88AFEC803A30D 323E9A117F3EA8E8E8E8E8E8A 17F8E8E8E8E8A28A27802072078020A92929292929292B2003C386519FC9EADF2B5CF124DD8EEA4C4E68D5E154050A9346EA98CE6000362A74E399C39ED5593852A30147F2959B56B827DFA3E464B2CF87DC5E80261345B53DE74A4A4D721EF77C255429961B7E4371AC06168D7E08C542828B8,02DA72E8B46901A65D4374E635538D8F68557DDA3A3A3A3A3A3CF9EA903AF7314C80318C82DD0B53FD3A3932D169E78CFC937C582D5781B2616E201F722280297CCEF1EF99F9F9F9F9D73DE9D747474747474749DDB2323232DDB23232323232DDB2327DDB232323232DDB2327DDB232A3A3A3A3A3A3A3A3A3F1238AFF877AF19E72BA0493361009，02E502CFD5C3F972FE9A3E2A18827820638F96B6F347E54D63DEB839011FD5765D，03E68877100E3EBE81C137074DA939D409C0025F17EB86ADB9427D28fff7AEE9，02C04D3A5274952ACDB76987F3184B346A484A3A3A34424B29E3692CF5AF，02ED06E0F418B53A37EC01D1D7D27290FA15F777771CB69B642A1471C29C84ACD，036D46073CB99CFE90473A3A3A3A49AABC8DE7F871199999F8711999999F8711999F777F28F24B29B29E3692C1DF5AF，02ED06DA44485682A989A4BEBB24,02F5D1F7C9029A80A4E36B9A5497027 EF7F3E73384A4A94Fbf7c4e9164eec8bc，02e41deff1b7cce11cade209a781adcffdabd1b91c0ba0375857a2bf9302419f3，02D76625FFFF956A7FC505AB0256C23E72D832FBAC391bCD3BCE5710A13D0603999EB05487515802DC4544CF10B3666623762be2C38A33975716E2C29C232））“，可签名，“A147FC63E13DC23E13DC25E8888A3CEE3D9A9A9A9A714AC3AAFD96F13AAFD96F14A3A3A3A3A3A3A3A3D9A9AE87“”）；
    检查不可分析（“SH（多（16，kzoaz5canayrkex3fslq2bwjpn7u52gzvxmy78ndmhuqruxujy，kwgnz6yccqtyvvvzmtc63tktkbbmrltsjr2nyvvbwapckn7mr，kxxyhinfxwv66efykccpm7d7tqhvqujujxyivxq9x，l2 bundutsyzwjjwnhynqf14mv2nq5n5n55syTB4f4fqgee9f（16，kzozoazazay5anayrkeyyykx3kx3fslq3fslq2bw2bw2bw2bw5w5qsbkcspsb7reqjezkod，kxdcnsst75hpaw5qkpzhtayacqc7p9vo3ffi2u4dxd1vgmik，l5这是一个很好的QJFTNKCF5UUUUUUUUUUUUUUUUUUUUUUUUUUUUFC7BYTQ8Go1XVV5Pv7V7RudicvCopB8I，L3NHUBW4VJ5J5YZ5cB97OEK6YUkvfZxefDhecjeykmwz，Kyjho36DWKyHi明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明明Zcubghn9ljhdeffl9zqgtjmjqxdbkeekrgzx24hxdgncijkkap，kzgpmbwwsdlwkac5urmbgcyabd2wgz7pbYGYXR8KT7GCA9UTN5A3、KYBXTPYT7YG4Q9TCA3LKVFRPD1YBhmVCJ2ehawxasqeguxedkp、KZJDE9IWJJPTKP2A2ON6ZBGZS7UUAWHWCFGDNeyJ3PHNJ8M8、l1XBHRXYNRQKOY4OQX6UYR5ZDYVYBSRMCV5PI3JJ9）），，，，，，，，，，，，，，，，，（多（1603669B8AFEC803A3032323E9A117F3EA8E8E8E8E88E8E8E8E88E8E8E8E8E8E8A28B2003C386519FC9EADF2B5CF124DD8EEA4C4E68D5E154050A9346EA98CE6000362A74E399C39ED559328A230147f959b56bb827fA3EE464b02CF87DC5E80261345b53de74a4d721eF77C255429961B7E4371AC06168D7E08c542828b8,02DA72E8B46901A65D4374E635538D8F68557DDA3A3A3CF9EA903AF7314C80318C82DD0B53FD3A3932d16E78CFC937C582D5781be6ff16E201F722280297CCEF1EF99F9F9F9F9D73DE9D7474747474749DDB2323232DDB23232323232329DDB232329DDB2327DDB23232329DDB232A3A3A3A3A3F1238AFF877AF19E72BA0493361009,02E502CFD5C3F972FE9A3E2A1882782063F96B6F347E54D63DEB839011FD5765D，03E68877100EE81C137074DA939D409C0025F17EB86ADB9427D28fff7AE9，02C04D3A5274952ACDB76987F3184B346A484A484A3A3A4874624B29E3692CF5AF，02ED06E0F418B53A7B4777D290FA15F5771CB69B642A1471C29C84ACD，036D46073BB9FFE90473DA429BC8DE7F877511978F871192988888F8775118F8F8F8F8F8E9，02C04E9，02C044D3A527495249529DA44485682A989A4BEBB24,02F5D1F7C9029A80A4E36B9A5497027 EF7F3E73384A4A94fbfe7c4e9164eec8bc，02e41deffd1b7cce11cde209a781adcffdab1b91c0ba0375857a2bfd9302419f3，02d76625f7956a7fc505ab02556c23ee72d832f1bac391bcd2d3abc5710a13d060399eb0a5487515802dc14544cf10b366623762bed2ec38a3975716e2c29c232））“）；//p2sh在redeemscript中不适合16个压缩的公钥
    
    
    CheckUnparsable("wsh(multi(2,[aaaaaaaa],xprv9vHkqa6EV4sPZHYqZznhT2NPtPCjKuDKGY38FBWLvgaDx45zo9WQRUT3dKYnjwih2yJD9mkrocEZXo1ex8G81dwSM1fwqWpWkeS3v86pgKt/1/2/*,xprv9s21ZrQH143K3QTDL4LXw2F7HEK3wJUD2nW2nRk4stbPy6cq3jPPqjiChkVvvNKmPGJxWUtg6LnF5kejMRNNU3TGtRBeJgk33yuGBxrMPHi/10/20/30/40/*'))", "wsh(multi(2,[aaaaaaaa],xpub69H7F5d8KSRgmmdJg2KhpAK8SR3DjMwAdkxj3zuxv27cprr9lgpeygmxubc6wb7erfvrnkzjxoummdznezpbzb7ap6r1d3tgfxhmwmkqtph/1/2/*，xpub661Mymwaqrbcftxgs5syjabqqg9ylmc4q1rdap9gse8nqtwybghey2gz29esfjqjocu1rupje8ytgqsfd265tmg7usudfdp6w1egmcet8/10/20/30/40/“）；//没有带有来源的公钥
    
    

    //检查结构的嵌套是否无效
    checkUnparable（“sh（l4rk1ydtcwekvxue6oxd9jcyffnv2cwrpvuplccu2z8trisoy1）”，“sh（03a34b99f2c790c4e36b2c3c35a36db06226e41c692fc82b8b56ac1c540c5bd）”）；//p2sh需要脚本，而不是密钥
    checkUnparable（“sh（combo（l4rk1ydtcwekvxue6oxd9jcyffnv2cwrpvuplccu2z8trisoy1））”，“sh（combo（03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd））”；//old必须是顶级
    checkUnparable（“wsh（l4rk1ydtcwekvxue6oxd9jcyffnv2cwrpvuplccu2z8trisoy1）”，“wsh（03a34b99f2c790c4e36b2c3c35a36db06226e41c692fc82b8b56ac1c540c5bd）”）；//p2wsh需要脚本，而不是密钥
    checkunparable（“wsh（wpkh（l4rk1ydtcwekvxue6oxd9jcyffnv2cwrpvuplccu2z8trisoyy1））”，“wsh（wpkh（03a34b99f2c790c4e36b2b3c2c35a36db06226e41c692c82b8b56ac1c540c5bd））”；//不能将证人嵌入证人内部
    checkunparable（“wsh（sh（pk（l4rk1ydtcwekvxue6oxd9jcyffnv2cwrpvuplccu2z8trisoyyy1））”，“wsh（sh（pk（03a34b99f2c790c4e36b2c3c35a36db06226e41c692c82b56ac1c540c5bd）））”；//不能在p2wsh内嵌入p2sh
    checkunparable（“sh（sh（pk（l4rk1ydtcwekvxue6oxd9jcyffnv2cwrpvuplccu2z8trisoyyy1））”，“sh（sh（pk（03a34b99f2c790c4e36b2b3c2c35a36db06226e41c692c82b8b56ac1c540c5bd）））”；//不能在p2sh中嵌入p2sh
    checkunparable（“wsh（wsh（pk（l4rk1ydtcwekvxue6oxd9jcyffnv2cwrpvuplccu2z8trisoyy1））”，“wsh（wsh（pk（03a34b99f2c790c4e36b2c3c35a36db06226e41c692c82b56ac1c540c5bd）））”；//不能在p2wsh内嵌入p2wsh
}

Boost_Auto_测试套件_end（）
