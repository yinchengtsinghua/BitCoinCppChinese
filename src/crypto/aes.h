
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2015-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。
//
//围绕CTAES的C++包装器，一种固定时间的AES实现

#ifndef BITCOIN_CRYPTO_AES_H
#define BITCOIN_CRYPTO_AES_H

extern "C" {
#include <crypto/ctaes/ctaes.h>
}

static const int AES_BLOCKSIZE = 16;
static const int AES128_KEYSIZE = 16;
static const int AES256_KEYSIZE = 32;

/*AES-128的加密类。*/
class AES128Encrypt
{
private:
    AES128_ctx ctx;

public:
    explicit AES128Encrypt(const unsigned char key[16]);
    ~AES128Encrypt();
    void Encrypt(unsigned char ciphertext[16], const unsigned char plaintext[16]) const;
};

/*AES-128的解密类。*/
class AES128Decrypt
{
private:
    AES128_ctx ctx;

public:
    explicit AES128Decrypt(const unsigned char key[16]);
    ~AES128Decrypt();
    void Decrypt(unsigned char plaintext[16], const unsigned char ciphertext[16]) const;
};

/*AES-256的加密类。*/
class AES256Encrypt
{
private:
    AES256_ctx ctx;

public:
    explicit AES256Encrypt(const unsigned char key[32]);
    ~AES256Encrypt();
    void Encrypt(unsigned char ciphertext[16], const unsigned char plaintext[16]) const;
};

/*AES-256的解密类。*/
class AES256Decrypt
{
private:
    AES256_ctx ctx;

public:
    explicit AES256Decrypt(const unsigned char key[32]);
    ~AES256Decrypt();
    void Decrypt(unsigned char plaintext[16], const unsigned char ciphertext[16]) const;
};

class AES256CBCEncrypt
{
public:
    AES256CBCEncrypt(const unsigned char key[AES256_KEYSIZE], const unsigned char ivIn[AES_BLOCKSIZE], bool padIn);
    ~AES256CBCEncrypt();
    int Encrypt(const unsigned char* data, int size, unsigned char* out) const;

private:
    const AES256Encrypt enc;
    const bool pad;
    unsigned char iv[AES_BLOCKSIZE];
};

class AES256CBCDecrypt
{
public:
    AES256CBCDecrypt(const unsigned char key[AES256_KEYSIZE], const unsigned char ivIn[AES_BLOCKSIZE], bool padIn);
    ~AES256CBCDecrypt();
    int Decrypt(const unsigned char* data, int size, unsigned char* out) const;

private:
    const AES256Decrypt dec;
    const bool pad;
    unsigned char iv[AES_BLOCKSIZE];
};

class AES128CBCEncrypt
{
public:
    AES128CBCEncrypt(const unsigned char key[AES128_KEYSIZE], const unsigned char ivIn[AES_BLOCKSIZE], bool padIn);
    ~AES128CBCEncrypt();
    int Encrypt(const unsigned char* data, int size, unsigned char* out) const;

private:
    const AES128Encrypt enc;
    const bool pad;
    unsigned char iv[AES_BLOCKSIZE];
};

class AES128CBCDecrypt
{
public:
    AES128CBCDecrypt(const unsigned char key[AES128_KEYSIZE], const unsigned char ivIn[AES_BLOCKSIZE], bool padIn);
    ~AES128CBCDecrypt();
    int Decrypt(const unsigned char* data, int size, unsigned char* out) const;

private:
    const AES128Decrypt dec;
    const bool pad;
    unsigned char iv[AES_BLOCKSIZE];
};

#endif //比特币加密
