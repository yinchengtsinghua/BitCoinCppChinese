
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
/***************************************************************
 *版权所有（c）2013-2015 Pieter Wuille*
 *根据麻省理工学院软件许可证分发，请参阅随附的*
 *文件复制或http://www.opensource.org/licenses/mit license.php.*
 ***************************************************************/


#ifndef SECP256K1_TESTRAND_IMPL_H
#define SECP256K1_TESTRAND_IMPL_H

#include <stdint.h>
#include <string.h>

#include "testrand.h"
#include "hash.h"

static secp256k1_rfc6979_hmac_sha256_t secp256k1_test_rng;
static uint32_t secp256k1_test_rng_precomputed[8];
static int secp256k1_test_rng_precomputed_used = 8;
static uint64_t secp256k1_test_rng_integer;
static int secp256k1_test_rng_integer_bits_left = 0;

SECP256K1_INLINE static void secp256k1_rand_seed(const unsigned char *seed16) {
    secp256k1_rfc6979_hmac_sha256_initialize(&secp256k1_test_rng, seed16, 16);
}

SECP256K1_INLINE static uint32_t secp256k1_rand32(void) {
    if (secp256k1_test_rng_precomputed_used == 8) {
        secp256k1_rfc6979_hmac_sha256_generate(&secp256k1_test_rng, (unsigned char*)(&secp256k1_test_rng_precomputed[0]), sizeof(secp256k1_test_rng_precomputed));
        secp256k1_test_rng_precomputed_used = 0;
    }
    return secp256k1_test_rng_precomputed[secp256k1_test_rng_precomputed_used++];
}

static uint32_t secp256k1_rand_bits(int bits) {
    uint32_t ret;
    if (secp256k1_test_rng_integer_bits_left < bits) {
        secp256k1_test_rng_integer |= (((uint64_t)secp256k1_rand32()) << secp256k1_test_rng_integer_bits_left);
        secp256k1_test_rng_integer_bits_left += 32;
    }
    ret = secp256k1_test_rng_integer;
    secp256k1_test_rng_integer >>= bits;
    secp256k1_test_rng_integer_bits_left -= bits;
    ret &= ((~((uint32_t)0)) >> (32 - bits));
    return ret;
}

static uint32_t secp256k1_rand_int(uint32_t range) {
    /*我们需要一个介于0和range-1之间（包括0和range-1）的统一整数。
     *b是最小的数字，范围<=2**b。
     *这里实施的两种机制：
     *-生成B位数字，直到找到一个低于范围的数字，然后返回该数字。
     *-找到范围的最大倍数m<=2**（b+a），生成b+a
     *位数字，直到找到低于m的位，并返回模范围。
     *第二种机制在每次迭代中消耗更多的熵位，
     *但可能需要较少的迭代，因为m接近2**（b+a），然后
     *范围为2**b。当
     *使用第一种机制，否则使用数字A。
     **/

    static const int addbits[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0};
    uint32_t trange, mult;
    int bits = 0;
    if (range <= 1) {
        return 0;
    }
    trange = range - 1;
    while (trange > 0) {
        trange >>= 1;
        bits++;
    }
    if (addbits[bits]) {
        bits = bits + addbits[bits];
        mult = ((~((uint32_t)0)) >> (32 - bits)) / range;
        trange = range * mult;
    } else {
        trange = range;
        mult = 1;
    }
    while(1) {
        uint32_t x = secp256k1_rand_bits(bits);
        if (x < trange) {
            return (mult == 1) ? x : (x % range);
        }
    }
}

static void secp256k1_rand256(unsigned char *b32) {
    secp256k1_rfc6979_hmac_sha256_generate(&secp256k1_test_rng, b32, 32);
}

static void secp256k1_rand_bytes_test(unsigned char *bytes, size_t len) {
    size_t bits = 0;
    memset(bytes, 0, len);
    while (bits < len * 8) {
        int now;
        uint32_t val;
        now = 1 + (secp256k1_rand_bits(6) * secp256k1_rand_bits(5) + 16) / 31;
        val = secp256k1_rand_bits(1);
        while (now > 0 && bits < len * 8) {
            bytes[bits / 8] |= val << (bits % 8);
            now--;
            bits++;
        }
    }
}

static void secp256k1_rand256_test(unsigned char *b32) {
    secp256k1_rand_bytes_test(b32, 32);
}

/*dif/*secp256k1_testrand_impl_h*/
