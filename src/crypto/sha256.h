
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2014-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_CRYPTO_SHA256_H
#define BITCOIN_CRYPTO_SHA256_H

#include <stdint.h>
#include <stdlib.h>
#include <string>

/*用于SHA-256的哈希类。*/
class CSHA256
{
private:
    uint32_t s[8];
    unsigned char buf[64];
    uint64_t bytes;

public:
    static const size_t OUTPUT_SIZE = 32;

    CSHA256();
    CSHA256& Write(const unsigned char* data, size_t len);
    void Finalize(unsigned char hash[OUTPUT_SIZE]);
    CSHA256& Reset();
};

/*自动检测最佳可用的sha256实现。
 *返回实现的名称。
 **/

std::string SHA256AutoDetect();

/*计算64字节blob的多个double-sha256。
 *输出：指向块的指针*32字节输出缓冲区
 *输入：指向块的指针*64字节输入缓冲区
 *块：要计算的哈希数。
 **/

void SHA256D64(unsigned char* output, const unsigned char* input, size_t blocks);

#endif //比特币加密
