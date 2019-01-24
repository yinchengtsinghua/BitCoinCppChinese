
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_WALLET_CRYPTER_H
#define BITCOIN_WALLET_CRYPTER_H

#include <keystore.h>
#include <serialize.h>
#include <support/allocators/secure.h>

#include <atomic>

const unsigned int WALLET_CRYPTO_KEY_SIZE = 32;
const unsigned int WALLET_CRYPTO_SALT_SIZE = 8;
const unsigned int WALLET_CRYPTO_IV_SIZE = 16;

/*
 *私钥加密是基于cmasterkey进行的，
 *包含一个salt和随机加密密钥。
 *
 *CMasterKeys使用AES-256-CBC使用密钥加密。
 *使用推导法推导
 *（0==evp_sha512（））和派生迭代次数。
 *vchotherderivation参数用于替代算法
 *可能需要更多参数（如scrypt）。
 *
 *然后使用AES-256-CBC对钱包私钥进行加密。
 *公钥的双sha256作为IV，并且
 *主密钥作为加密密钥（见keystore.[ch]）。
 **/


/*钱包加密主密钥*/
class CMasterKey
{
public:
    std::vector<unsigned char> vchCryptedKey;
    std::vector<unsigned char> vchSalt;
//！0=evp_sha512（）。
//！1＝SCRYPT（）
    unsigned int nDerivationMethod;
    unsigned int nDeriveIterations;
//！使用此函数可以获得更多参数来进行键派生，
//！如要加密的各种参数
    std::vector<unsigned char> vchOtherDerivationParameters;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vchCryptedKey);
        READWRITE(vchSalt);
        READWRITE(nDerivationMethod);
        READWRITE(nDeriveIterations);
        READWRITE(vchOtherDerivationParameters);
    }

    CMasterKey()
    {
//在1.86 GHz奔腾M上，25000发子弹的发射时间不足0.1秒。
//也就是说，比我们需要支持的最低硬件低一点
        nDeriveIterations = 25000;
        nDerivationMethod = 0;
        vchOtherDerivationParameters = std::vector<unsigned char>(0);
    }
};

typedef std::vector<unsigned char, secure_allocator<unsigned char> > CKeyingMaterial;

namespace wallet_crypto_tests
{
    class TestCrypter;
}

/*带密钥信息的加密/解密上下文*/
class CCrypter
{
friend class wallet_crypto_tests::TestCrypter; //用于测试访问chkey/chiv
private:
    std::vector<unsigned char, secure_allocator<unsigned char>> vchKey;
    std::vector<unsigned char, secure_allocator<unsigned char>> vchIV;
    bool fKeySet;

    int BytesToKeySHA512AES(const std::vector<unsigned char>& chSalt, const SecureString& strKeyData, int count, unsigned char *key,unsigned char *iv) const;

public:
    bool SetKeyFromPassphrase(const SecureString &strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod);
    bool Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char> &vchCiphertext) const;
    bool Decrypt(const std::vector<unsigned char>& vchCiphertext, CKeyingMaterial& vchPlaintext) const;
    bool SetKey(const CKeyingMaterial& chNewKey, const std::vector<unsigned char>& chNewIV);

    void CleanKey()
    {
        memory_cleanse(vchKey.data(), vchKey.size());
        memory_cleanse(vchIV.data(), vchIV.size());
        fKeySet = false;
    }

    CCrypter()
    {
        fKeySet = false;
        vchKey.resize(WALLET_CRYPTO_KEY_SIZE);
        vchIV.resize(WALLET_CRYPTO_IV_SIZE);
    }

    ~CCrypter()
    {
        CleanKey();
    }
};

/*密钥库，用于加密私钥。
 *它来自基本密钥存储，如果没有激活的加密，则使用基本密钥存储。
 **/

class CCryptoKeyStore : public CBasicKeyStore
{
private:

    CKeyingMaterial vMasterKey GUARDED_BY(cs_KeyStore);

//！如果fusecrypto为真，则mapkeys必须为空
//！如果fusecrypto为false，则vmasterkey必须为空
    std::atomic<bool> fUseCrypto;

//！跟踪解锁之前是否进行过彻底检查
    bool fDecryptionThoroughlyChecked;

protected:
    using CryptedKeyMap = std::map<CKeyID, std::pair<CPubKey, std::vector<unsigned char>>>;

    bool SetCrypted();

//！将加密以前未加密的密钥
    bool EncryptKeys(CKeyingMaterial& vMasterKeyIn);

    bool Unlock(const CKeyingMaterial& vMasterKeyIn);
    CryptedKeyMap mapCryptedKeys GUARDED_BY(cs_KeyStore);

public:
    CCryptoKeyStore() : fUseCrypto(false), fDecryptionThoroughlyChecked(false)
    {
    }

    bool IsCrypted() const { return fUseCrypto; }
    bool IsLocked() const;
    bool Lock();

    virtual bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey) override;
    bool HaveKey(const CKeyID &address) const override;
    bool GetKey(const CKeyID &address, CKey& keyOut) const override;
    bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const override;
    std::set<CKeyID> GetKeys() const override;

    /*
     *钱包状态（加密、锁定）已更改。
     *注意：在没有锁的情况下调用。
     **/

    boost::signals2::signal<void (CCryptoKeyStore* wallet)> NotifyStatusChanged;
};

#endif //比特币钱包密码器
