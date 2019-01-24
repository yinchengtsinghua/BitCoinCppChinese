
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
/***************************************************************
 *版权所有（c）2014 Pieter Wuille*
 *根据麻省理工学院软件许可证分发，请参阅随附的*
 *文件复制或http://www.opensource.org/licenses/mit license.php.*
 ***************************************************************/


#ifndef SECP256K1_HASH_H
#define SECP256K1_HASH_H

#include <stdlib.h>
#include <stdint.h>

typedef struct {
    uint32_t s[8];
    /*t32_t buf[16]；/*在big endian中*/
    大小字节；
secp256k1_sha256_t；

静态void secp256k1_sha256_initialize（secp256k1_sha256_t*hash）；
静态void secp256k1_sha256_write（secp256k1_sha256_t*hash，const unsigned char*data，size_t size）；
静态void secp256k1_sha256_finalize（secp256k1_sha256_t*hash，unsigned char*out32）；

typedef结构
    secp256k1_sha256_t内、外；
secp256k1_hmac_sha256_t；

静态void secp256k1_hmac_sha256_initialize（secp256k1_hmac_sha256_t*hash，const unsigned char*key，size_t size）；
静态void secp256k1_hmac_sha256_write（secp256k1_hmac_sha256_t*hash，const unsigned char*data，size_t size）；
静态void secp256k1_hmac_sha256_finalize（secp256k1_hmac_sha256_t*哈希，无符号char*out32）；

typedef结构
    无符号字符V[32]；
    无符号字符K[32]；
    INT重试；
secp256k1_rfc6979_hmac_sha256_t；

静态void secp256k1_rfc6979_hmac_sha256_initialize（secp256k1_rfc6979_hmac_sha256_t*rng，const unsigned char*key，size_t keylen）；
静态void secp256k1_rfc6979_hmac_sha256_generate（secp256k1_rfc6979_hmac_sha256_t*rng，unsigned char*out，size_t outlen）；
静态无效secp256k1_rfc6979_hmac_sha256_finalize（secp256k1_rfc6979_hmac_sha256_t*rng）；

endif/*secp256k1_hash_h*/

