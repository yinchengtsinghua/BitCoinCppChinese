
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

#include <crypto/aes.h>
#include <crypto/common.h>

#include <assert.h>
#include <string.h>

extern "C" {
#include <crypto/ctaes/ctaes.c>
}

AES128Encrypt::AES128Encrypt(const unsigned char key[16])
{
    AES128_init(&ctx, key);
}

AES128Encrypt::~AES128Encrypt()
{
    memset(&ctx, 0, sizeof(ctx));
}

void AES128Encrypt::Encrypt(unsigned char ciphertext[16], const unsigned char plaintext[16]) const
{
    AES128_encrypt(&ctx, 1, ciphertext, plaintext);
}

AES128Decrypt::AES128Decrypt(const unsigned char key[16])
{
    AES128_init(&ctx, key);
}

AES128Decrypt::~AES128Decrypt()
{
    memset(&ctx, 0, sizeof(ctx));
}

void AES128Decrypt::Decrypt(unsigned char plaintext[16], const unsigned char ciphertext[16]) const
{
    AES128_decrypt(&ctx, 1, plaintext, ciphertext);
}

AES256Encrypt::AES256Encrypt(const unsigned char key[32])
{
    AES256_init(&ctx, key);
}

AES256Encrypt::~AES256Encrypt()
{
    memset(&ctx, 0, sizeof(ctx));
}

void AES256Encrypt::Encrypt(unsigned char ciphertext[16], const unsigned char plaintext[16]) const
{
    AES256_encrypt(&ctx, 1, ciphertext, plaintext);
}

AES256Decrypt::AES256Decrypt(const unsigned char key[32])
{
    AES256_init(&ctx, key);
}

AES256Decrypt::~AES256Decrypt()
{
    memset(&ctx, 0, sizeof(ctx));
}

void AES256Decrypt::Decrypt(unsigned char plaintext[16], const unsigned char ciphertext[16]) const
{
    AES256_decrypt(&ctx, 1, plaintext, ciphertext);
}


template <typename T>
static int CBCEncrypt(const T& enc, const unsigned char iv[AES_BLOCKSIZE], const unsigned char* data, int size, bool pad, unsigned char* out)
{
    int written = 0;
    int padsize = size % AES_BLOCKSIZE;
    unsigned char mixed[AES_BLOCKSIZE];

    if (!data || !size || !out)
        return 0;

    if (!pad && padsize != 0)
        return 0;

    memcpy(mixed, iv, AES_BLOCKSIZE);

//只写最后一个块
    while (written + AES_BLOCKSIZE <= size) {
        for (int i = 0; i != AES_BLOCKSIZE; i++)
            mixed[i] ^= *data++;
        enc.Encrypt(out + written, mixed);
        memcpy(mixed, out + written, AES_BLOCKSIZE);
        written += AES_BLOCKSIZE;
    }
    if (pad) {
//对于所有剩余的，用剩余的值填充每个字节
//空间。如果没有，按一个完整的块填充。
        for (int i = 0; i != padsize; i++)
            mixed[i] ^= *data++;
        for (int i = padsize; i != AES_BLOCKSIZE; i++)
            mixed[i] ^= AES_BLOCKSIZE - padsize;
        enc.Encrypt(out + written, mixed);
        written += AES_BLOCKSIZE;
    }
    return written;
}

template <typename T>
static int CBCDecrypt(const T& dec, const unsigned char iv[AES_BLOCKSIZE], const unsigned char* data, int size, bool pad, unsigned char* out)
{
    int written = 0;
    bool fail = false;
    const unsigned char* prev = iv;

    if (!data || !size || !out)
        return 0;

    if (size % AES_BLOCKSIZE != 0)
        return 0;

//解密所有数据。将在输出中检查填充。
    while (written != size) {
        dec.Decrypt(out, data + written);
        for (int i = 0; i != AES_BLOCKSIZE; i++)
            *out++ ^= prev[i];
        prev = data + written;
        written += AES_BLOCKSIZE;
    }

//解密填充时，尝试在恒定时间内运行
    if (pad) {
//如果使用，填充大小是最后一个解密字节的值。为了
//它必须介于1和aes_块大小之间才能有效。
        unsigned char padsize = *--out;
        fail = !padsize | (padsize > AES_BLOCKSIZE);

//如果形状不好，就把它当作没有填充物一样对待。
        padsize *= !fail;

//所有填充必须等于最后一个字节，否则格式不正确
        for (int i = AES_BLOCKSIZE; i != 0; i--)
            fail |= ((i > AES_BLOCKSIZE - padsize) & (*out-- != padsize));

        written -= padsize;
    }
    return written * !fail;
}

AES256CBCEncrypt::AES256CBCEncrypt(const unsigned char key[AES256_KEYSIZE], const unsigned char ivIn[AES_BLOCKSIZE], bool padIn)
    : enc(key), pad(padIn)
{
    memcpy(iv, ivIn, AES_BLOCKSIZE);
}

int AES256CBCEncrypt::Encrypt(const unsigned char* data, int size, unsigned char* out) const
{
    return CBCEncrypt(enc, iv, data, size, pad, out);
}

AES256CBCEncrypt::~AES256CBCEncrypt()
{
    memset(iv, 0, sizeof(iv));
}

AES256CBCDecrypt::AES256CBCDecrypt(const unsigned char key[AES256_KEYSIZE], const unsigned char ivIn[AES_BLOCKSIZE], bool padIn)
    : dec(key), pad(padIn)
{
    memcpy(iv, ivIn, AES_BLOCKSIZE);
}


int AES256CBCDecrypt::Decrypt(const unsigned char* data, int size, unsigned char* out) const
{
    return CBCDecrypt(dec, iv, data, size, pad, out);
}

AES256CBCDecrypt::~AES256CBCDecrypt()
{
    memset(iv, 0, sizeof(iv));
}

AES128CBCEncrypt::AES128CBCEncrypt(const unsigned char key[AES128_KEYSIZE], const unsigned char ivIn[AES_BLOCKSIZE], bool padIn)
    : enc(key), pad(padIn)
{
    memcpy(iv, ivIn, AES_BLOCKSIZE);
}

AES128CBCEncrypt::~AES128CBCEncrypt()
{
    memset(iv, 0, AES_BLOCKSIZE);
}

int AES128CBCEncrypt::Encrypt(const unsigned char* data, int size, unsigned char* out) const
{
    return CBCEncrypt(enc, iv, data, size, pad, out);
}

AES128CBCDecrypt::AES128CBCDecrypt(const unsigned char key[AES128_KEYSIZE], const unsigned char ivIn[AES_BLOCKSIZE], bool padIn)
    : dec(key), pad(padIn)
{
    memcpy(iv, ivIn, AES_BLOCKSIZE);
}

AES128CBCDecrypt::~AES128CBCDecrypt()
{
    memset(iv, 0, AES_BLOCKSIZE);
}

int AES128CBCDecrypt::Decrypt(const unsigned char* data, int size, unsigned char* out) const
{
    return CBCDecrypt(dec, iv, data, size, pad, out);
}
