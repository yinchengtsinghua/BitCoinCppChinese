
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
/***************************************************************
 *版权所有（c）2013、2014 Pieter Wuille*
 *根据麻省理工学院软件许可证分发，请参阅随附的*
 *文件复制或http://www.opensource.org/licenses/mit license.php.*
 ***************************************************************/


#ifndef SECP256K1_TESTRAND_H
#define SECP256K1_TESTRAND_H

#if defined HAVE_CONFIG_H
#include "libsecp256k1-config.h"
#endif

/*仅用于测试基础设施的非加密RNG。*/

/*为测试设定伪随机数生成器。*/
SECP256K1_INLINE static void secp256k1_rand_seed(const unsigned char *seed16);

/*在[0..2**32-1]范围内生成一个伪随机数。*/
static uint32_t secp256k1_rand32(void);

/*在[0..2**位-1]范围内生成一个伪随机数。位必须是1或
 *更多。*/

static uint32_t secp256k1_rand_bits(int bits);

/*在范围[0..range-1]中生成伪随机数。*/
static uint32_t secp256k1_rand_int(uint32_t range);

/*生成一个伪随机32字节数组。*/
static void secp256k1_rand256(unsigned char *b32);

/*生成一个具有零位和一位长序列的伪随机32字节数组。*/
static void secp256k1_rand256_test(unsigned char *b32);

/*用零位和一位的长序列生成伪随机字节。*/
static void secp256k1_rand_bytes_test(unsigned char *bytes, size_t len);

/*dif/*secp256k1_testrand_h*/
