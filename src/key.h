
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//版权所有（c）2017 Zcash开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_KEY_H
#define BITCOIN_KEY_H

#include <pubkey.h>
#include <serialize.h>
#include <support/allocators/secure.h>
#include <uint256.h>

#include <stdexcept>
#include <vector>


/*
 *安全分配程序在allocators.h中定义。
 *cprivkey是一个序列化的私钥，包含所有参数。
 *（专用\密钥\大小字节）
 **/

typedef std::vector<unsigned char, secure_allocator<unsigned char> > CPrivKey;

/*封装的私钥。*/
class CKey
{
public:
    /*
     ＊SECP256K1：
     **/

    static const unsigned int PRIVATE_KEY_SIZE            = 279;
    static const unsigned int COMPRESSED_PRIVATE_KEY_SIZE = 214;
    /*
     *请访问www.keylength.com
     *对于单字节推送，脚本最多支持75个
     **/

    static_assert(
        PRIVATE_KEY_SIZE >= COMPRESSED_PRIVATE_KEY_SIZE,
        "COMPRESSED_PRIVATE_KEY_SIZE is larger than PRIVATE_KEY_SIZE");

private:
//！此私钥是否有效。我们在修改密钥时检查是否正确。
//！数据，因此fvalid应该始终对应于实际状态。
    bool fValid;

//！是否压缩与此私钥对应的公钥。
    bool fCompressed;

//！实际字节数据
    std::vector<unsigned char, secure_allocator<unsigned char> > keydata;

//！检查VCH指向的32字节数组是否为有效的keydata。
    bool static Check(const unsigned char* vch);

public:
//！构造无效的私钥。
    CKey() : fValid(false), fCompressed(false)
    {
//重要提示：VCH的长度必须为32个字节，才能中断序列化。
        keydata.resize(32);
    }

    friend bool operator==(const CKey& a, const CKey& b)
    {
        return a.fCompressed == b.fCompressed &&
            a.size() == b.size() &&
            memcmp(a.keydata.data(), b.keydata.data(), a.size()) == 0;
    }

//！使用begin和end迭代器初始化字节数据。
    template <typename T>
    void Set(const T pbegin, const T pend, bool fCompressedIn)
    {
        if (size_t(pend - pbegin) != keydata.size()) {
            fValid = false;
        } else if (Check(&pbegin[0])) {
            memcpy(keydata.data(), (unsigned char*)&pbegin[0], keydata.size());
            fValid = true;
            fCompressed = fCompressedIn;
        } else {
            fValid = false;
        }
    }

//！简单的只读类矢量接口。
    unsigned int size() const { return (fValid ? keydata.size() : 0); }
    const unsigned char* begin() const { return keydata.data(); }
    const unsigned char* end() const { return keydata.data() + size(); }

//！检查此私钥是否有效。
    bool IsValid() const { return fValid; }

//！检查与此私钥对应的公钥是否被压缩。
    bool IsCompressed() const { return fCompressed; }

//！使用加密prng生成新的私钥。
    void MakeNewKey(bool fCompressed);

    /*
     *将私钥转换为cprivkey（序列化的openssl私钥数据）。
     *这个很贵。
     */

    CPrivKey GetPrivKey() const;

    /*
     *从私钥计算公钥。
     *这个很贵。
     **/

    CPubKey GetPubKey() const;

    /*
     *创建一个DER序列化签名。
     *测试用例参数调整了确定性nonce。
     **/

    bool Sign(const uint256& hash, std::vector<unsigned char>& vchSig, bool grind = true, uint32_t test_case = 0) const;

    /*
     *创建一个压缩签名（65字节），允许重建使用的公钥。
     *格式为一个头字节，后跟序列化程序和s值的两倍32字节。
     *头字节：0x1B=偶数Y的第一个键，0x1C=奇数Y的第一个键，
     *0x1d=第二个Y偶数键，0x1e=第二个Y奇数键，
     *为压缩键添加0x04。
     **/

    bool SignCompact(const uint256& hash, std::vector<unsigned char>& vchSig) const;

//！派生bip32子键。
    bool Derive(CKey& keyChild, ChainCode &ccChild, unsigned int nChild, const ChainCode& cc) const;

    /*
     *彻底验证私钥和公钥是否匹配。
     *这是使用不同的机制来完成的，而不仅仅是重新生成它。
     **/

    bool VerifyPubKey(const CPubKey& vchPubKey) const;

//！加载私钥并检查公钥是否匹配。
    bool Load(const CPrivKey& privkey, const CPubKey& vchPubKey, bool fSkipCheck);
};

struct CExtKey {
    unsigned char nDepth;
    unsigned char vchFingerprint[4];
    unsigned int nChild;
    ChainCode chaincode;
    CKey key;

    friend bool operator==(const CExtKey& a, const CExtKey& b)
    {
        return a.nDepth == b.nDepth &&
            memcmp(&a.vchFingerprint[0], &b.vchFingerprint[0], sizeof(vchFingerprint)) == 0 &&
            a.nChild == b.nChild &&
            a.chaincode == b.chaincode &&
            a.key == b.key;
    }

    void Encode(unsigned char code[BIP32_EXTKEY_SIZE]) const;
    void Decode(const unsigned char code[BIP32_EXTKEY_SIZE]);
    bool Derive(CExtKey& out, unsigned int nChild) const;
    CExtPubKey Neuter() const;
    void SetSeed(const unsigned char* seed, unsigned int nSeedLen);
    template <typename Stream>
    void Serialize(Stream& s) const
    {
        unsigned int len = BIP32_EXTKEY_SIZE;
        ::WriteCompactSize(s, len);
        unsigned char code[BIP32_EXTKEY_SIZE];
        Encode(code);
        s.write((const char *)&code[0], len);
    }
    template <typename Stream>
    void Unserialize(Stream& s)
    {
        unsigned int len = ::ReadCompactSize(s);
        unsigned char code[BIP32_EXTKEY_SIZE];
        if (len != BIP32_EXTKEY_SIZE)
            throw std::runtime_error("Invalid extended key size\n");
        s.read((char *)&code[0], len);
        Decode(code);
    }
};

/*初始化椭圆曲线支持。如果不先呼叫ecc_stop，则不能呼叫两次。*/
void ECC_Start();

/*将椭圆曲线支架取消初始化。如果没有先打电话叫ECC启动，就没有行动。*/
void ECC_Stop();

/*检查所需的EC支持是否在运行时可用。*/
bool ECC_InitSanityCheck();

#endif //比科尼亚基耶赫
