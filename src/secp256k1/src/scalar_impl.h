
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


#ifndef SECP256K1_SCALAR_IMPL_H
#define SECP256K1_SCALAR_IMPL_H

#include "group.h"
#include "scalar.h"

#if defined HAVE_CONFIG_H
#include "libsecp256k1-config.h"
#endif

#if defined(EXHAUSTIVE_TEST_ORDER)
#include "scalar_low_impl.h"
#elif defined(USE_SCALAR_4X64)
#include "scalar_4x64_impl.h"
#elif defined(USE_SCALAR_8X32)
#include "scalar_8x32_impl.h"
#else
#error "Please select scalar implementation"
#endif

#ifndef USE_NUM_NONE
static void secp256k1_scalar_get_num(secp256k1_num *r, const secp256k1_scalar *a) {
    unsigned char c[32];
    secp256k1_scalar_get_b32(c, a);
    secp256k1_num_set_bin(r, c, 32);
}

/*secp256k1曲线顺序，见ecdsa impl.h中的secp256k1_ecdsa_const_order_as_fe*/
static void secp256k1_scalar_order_get_num(secp256k1_num *r) {
#if defined(EXHAUSTIVE_TEST_ORDER)
    static const unsigned char order[32] = {
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,EXHAUSTIVE_TEST_ORDER
    };
#else
    static const unsigned char order[32] = {
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,
        0xBA,0xAE,0xDC,0xE6,0xAF,0x48,0xA0,0x3B,
        0xBF,0xD2,0x5E,0x8C,0xD0,0x36,0x41,0x41
    };
#endif
    secp256k1_num_set_bin(r, order, 32);
}
#endif

static void secp256k1_scalar_inverse(secp256k1_scalar *r, const secp256k1_scalar *x) {
#if defined(EXHAUSTIVE_TEST_ORDER)
    int i;
    *r = 0;
    for (i = 0; i < EXHAUSTIVE_TEST_ORDER; i++)
        if ((i * *x) % EXHAUSTIVE_TEST_ORDER == 1)
            *r = i;
    /*如果这个检验触发器，我们得到了一个不可逆的标量（因此
     *有一个组合组顺序；在详尽的测试中修复它。*/

    VERIFY_CHECK(*r != 0);
}
#else
    secp256k1_scalar *t;
    int i;
    /*首先计算x n为x^（2^n-1），对于一些n的值，
     *对于m的某些值，um为x^m。*/

    secp256k1_scalar x2, x3, x6, x8, x14, x28, x56, x112, x126;
    secp256k1_scalar u2, u5, u9, u11, u13;

    secp256k1_scalar_sqr(&u2, x);
    secp256k1_scalar_mul(&x2, &u2,  x);
    secp256k1_scalar_mul(&u5, &u2, &x2);
    secp256k1_scalar_mul(&x3, &u5,  &u2);
    secp256k1_scalar_mul(&u9, &x3, &u2);
    secp256k1_scalar_mul(&u11, &u9, &u2);
    secp256k1_scalar_mul(&u13, &u11, &u2);

    secp256k1_scalar_sqr(&x6, &u13);
    secp256k1_scalar_sqr(&x6, &x6);
    secp256k1_scalar_mul(&x6, &x6, &u11);

    secp256k1_scalar_sqr(&x8, &x6);
    secp256k1_scalar_sqr(&x8, &x8);
    secp256k1_scalar_mul(&x8, &x8,  &x2);

    secp256k1_scalar_sqr(&x14, &x8);
    for (i = 0; i < 5; i++) {
        secp256k1_scalar_sqr(&x14, &x14);
    }
    secp256k1_scalar_mul(&x14, &x14, &x6);

    secp256k1_scalar_sqr(&x28, &x14);
    for (i = 0; i < 13; i++) {
        secp256k1_scalar_sqr(&x28, &x28);
    }
    secp256k1_scalar_mul(&x28, &x28, &x14);

    secp256k1_scalar_sqr(&x56, &x28);
    for (i = 0; i < 27; i++) {
        secp256k1_scalar_sqr(&x56, &x56);
    }
    secp256k1_scalar_mul(&x56, &x56, &x28);

    secp256k1_scalar_sqr(&x112, &x56);
    for (i = 0; i < 55; i++) {
        secp256k1_scalar_sqr(&x112, &x112);
    }
    secp256k1_scalar_mul(&x112, &x112, &x56);

    secp256k1_scalar_sqr(&x126, &x112);
    for (i = 0; i < 13; i++) {
        secp256k1_scalar_sqr(&x126, &x126);
    }
    secp256k1_scalar_mul(&x126, &x126, &x14);

    /*然后累积最终结果（t从x126开始）。*/
    t = &x126;
    for (i = 0; i < 3; i++) {
        secp256k1_scalar_sqr(t, t);
    }
    /*p256k1_scalar_mul（t，t，&u5）；/*101*/
    对于（i=0；i<4；i++）/*0*/

        secp256k1_scalar_sqr(t, t);
    }
    /*p256k1_scalar_mul（t，t，&x3）；/*111*/
    对于（i=0；i<4；i++）/*0*/

        secp256k1_scalar_sqr(t, t);
    }
    /*p256k1_scalar_mul（t，t，&u5）；/*101*/
    对于（i=0；i<5；i++）/*0*/

        secp256k1_scalar_sqr(t, t);
    }
    /*p256k1_scalar_mul（t，t，&u11）；/*1011*/
    对于（i=0；i<4；i++）
        secp256k1_标量_sqr（t，t）；
    }
    secp256k1_scalar_mul（t，t，&u11）；/*1011*/

    /*（i=0；i<4；i++）/*0*/
        secp256k1_标量_sqr（t，t）；
    }
    secp256k1_scalar_mul（t，t，&x3）；/*111*/

    /*（i=0；i<5；i++）/*00*/
        secp256k1_标量_sqr（t，t）；
    }
    secp256k1_scalar_mul（t，t，&x3）；/*111*/

    /*（i=0；i<6；i++）/*00*/
        secp256k1_标量_sqr（t，t）；
    }
    secp256k1_scalar_mul（t，t，&u13）；/*1101*/

    /*（i=0；i<4；i++）/*0*/
        secp256k1_标量_sqr（t，t）；
    }
    secp256k1_scalar_mul（t，t，&u5）；/*101*/

    for (i = 0; i < 3; i++) {
        secp256k1_scalar_sqr(t, t);
    }
    /*p256k1_scalar_mul（t，t，&x3）；/*111*/
    对于（i=0；i<5；i++）/*0*/

        secp256k1_scalar_sqr(t, t);
    }
    /*p256k1_scalar_mul（t，t，&u9）；/*1001*/
    对于（i=0；i<6；i++）/*000*/

        secp256k1_scalar_sqr(t, t);
    }
    /*p256k1_scalar_mul（t，t，&u5）；/*101*/
    对于（i=0；i<10；i++）/*0000000*/

        secp256k1_scalar_sqr(t, t);
    }
    /*p256k1_scalar_mul（t，t，&x3）；/*111*/
    对于（i=0；i<4；i++）/*0*/

        secp256k1_scalar_sqr(t, t);
    }
    /*p256k1_scalar_mul（t，t，&x3）；/*111*/
    对于（i=0；i<9；i++）/*0*/

        secp256k1_scalar_sqr(t, t);
    }
    /*p256k1_scalar_mul（t，t，&x8）；/*11111111*/
    对于（i=0；i<5；i++）/*0*/

        secp256k1_scalar_sqr(t, t);
    }
    /*p256k1_scalar_mul（t，t，&u9）；/*1001*/
    对于（i=0；i<6；i++）/*00*/

        secp256k1_scalar_sqr(t, t);
    }
    /*p256k1_scalar_mul（t，t，&u11）；/*1011*/
    对于（i=0；i<4；i++）
        secp256k1_标量_sqr（t，t）；
    }
    secp256k1_scalar_mul（t，t，&u13）；/*1101*/

    for (i = 0; i < 5; i++) {
        secp256k1_scalar_sqr(t, t);
    }
    /*p256k1_scalar_mul（t，t，&x2）；/*11*/
    对于（i=0；i<6；i++）/*00*/

        secp256k1_scalar_sqr(t, t);
    }
    /*p256k1_scalar_mul（t，t，&u13）；/*1101*/
    对于（i=0；i<10；i++）/*000000*/

        secp256k1_scalar_sqr(t, t);
    }
    /*p256k1_scalar_mul（t，t，&u13）；/*1101*/
    对于（i=0；i<4；i++）
        secp256k1_标量_sqr（t，t）；
    }
    secp256k1_scalar_mul（t，t，&u9）；/*1001*/

    /*（i=0；i<6；i++）/*00000*/
        secp256k1_标量_sqr（t，t）；
    }
    secp256k1_scalar_mul（t，t，x）；/*1*/

    /*（i=0；i<8；i++）/*00*/
        secp256k1_标量_sqr（t，t）；
    }
    secp256k1_scalar_mul（r，t，&x6）；/*111111*/

}

SECP256K1_INLINE static int secp256k1_scalar_is_even(const secp256k1_scalar *a) {
    return !(a->d[0] & 1);
}
#endif

static void secp256k1_scalar_inverse_var(secp256k1_scalar *r, const secp256k1_scalar *x) {
#if defined(USE_SCALAR_INV_BUILTIN)
    secp256k1_scalar_inverse(r, x);
#elif defined(USE_SCALAR_INV_NUM)
    unsigned char b[32];
    secp256k1_num n, m;
    secp256k1_scalar t = *x;
    secp256k1_scalar_get_b32(b, &t);
    secp256k1_num_set_bin(&n, b, 32);
    secp256k1_scalar_order_get_num(&m);
    secp256k1_num_mod_inverse(&n, &n, &m);
    secp256k1_num_get_bin(b, 32, &n);
    secp256k1_scalar_set_b32(r, b, NULL);
    /*验证反计算是否正确，无GMP代码。*/
    secp256k1_scalar_mul(&t, &t, r);
    CHECK(secp256k1_scalar_is_one(&t));
#else
#error "Please select scalar inverse implementation"
#endif
}

#ifdef USE_ENDOMORPHISM
#if defined(EXHAUSTIVE_TEST_ORDER)
/*
 *找到给定k的k1和k2，这样k1+k2*lambda==k mod n；与
 *完整的情况下，我们不需要让k1和k2变小，我们只想让它们变小。
 *非常重要的一点是要获得详尽测试的完整测试覆盖率。因此我们
 （任意）设置k2=k+5和k1=k-k2*lambda。
 **/

static void secp256k1_scalar_split_lambda(secp256k1_scalar *r1, secp256k1_scalar *r2, const secp256k1_scalar *a) {
    *r2 = (*a + 5) % EXHAUSTIVE_TEST_ORDER;
    *r1 = (*a + (EXHAUSTIVE_TEST_ORDER - *r2) * EXHAUSTIVE_TEST_LAMBDA) % EXHAUSTIVE_TEST_ORDER;
}
#else
/*
 *secp256k1曲线具有自同态，其中lambda*（x，y）=（beta*x，y），其中
 *lambda为0x53,0x63,0xad，0x4c，0xc0,0x5c，0x30,0xe0,0xa5,0x26,0x1c，0x02,0x88,0x12,0x64,0x5a，
 *0×12,0×2e，0×22,0×ea，0×20,0×81,0×66,0×78,0×df，0×2,0×96,0×7c，0×1b，0×23,0×bd，0×72
 *
 *“椭圆曲线密码术指南”（汉克森、梅内泽斯、万斯通）给出了一种算法
 （算法3.74）在给定k的情况下求k1和k2，这样k1+k2*lambda==k mod n，和k1
 *和k2的尺寸较小。
 *它依赖于常数a1、b1、a2、b2。上述lambda值的这些常量是：
 *
 *-a1=0x30,0x86,0xd2,0x21,0xa7,0xd4,0x6b，0xcd，0xe8,0x6c，0x90,0xe4,0x92,0x84,0xeb，0x15_
 *-b1=-0xe4,0x43,0x7e，0xd6,0x01,0x0e，0x88,0x28,0x6f，0x54,0x7f，0xa9,0x0a，0xbf，0xe4,0xc3_
 *-A2=0x01,0x14,0XCA，0x50,0XF7,0XA8,0XE2,0XF3,0XF6,0X57,0XC1,0X10,0X8D，0X9D，0X44,0XCF，0XD8_
 *-b2=0x30,0x86,0xd2,0x21,0xa7,0xd4,0x6b，0xcd，0xe8,0x6c，0x90,0xe4,0x92,0x84,0xeb，0x15_
 *
 *然后该算法计算c1=圆（b1*k/n）和c2=圆（b2*k/n），并给出
 *k1=k-（c1*a1+c2*a2），k2=-（c1*b1+c2*b2）。相反，我们使用模块化算法，
 *将k1计算为k-k2*lambda，避免了常数a1和a2的需要。
 *
 *g1、g2是预先计算的常量，用于用四舍五入乘法替换除法。
 *分解基于自同态的点乘法的标量时。
 *
 *在“椭圆曲线指南”中提到了使用预计算估计的可能性。
 *第3.5节中的“密码学”（汉克森、门尼泽、万通）。
 *
 *本文“公钥的高效软件实现”中描述了派生过程。
 *使用MSP430X微控制器（Gouvea、Oliveira、Lopez）对传感器网络进行加密，
 *第4.3节（这里我们使用更高精度的估计值）：
 *d=a1*b2-b1*a2
 *g1=圆形（（2^272）*b2/d）
 *g2=圆形（（2^272）*b1/d）
 *
 （请注意，这里的“d”也等于曲线顺序，因为找到了[a1，b1]和[a2，b2]。
 *作为输入“order”和“lambda”的扩展欧几里德算法的输出。
 *
 *下面的函数将a拆分为r1和r2，这样r1+lambda*r2==a（mod order）。
 **/


static void secp256k1_scalar_split_lambda(secp256k1_scalar *r1, secp256k1_scalar *r2, const secp256k1_scalar *a) {
    secp256k1_scalar c1, c2;
    static const secp256k1_scalar minus_lambda = SECP256K1_SCALAR_CONST(
        0xAC9C52B3UL, 0x3FA3CF1FUL, 0x5AD9E3FDUL, 0x77ED9BA4UL,
        0xA880B9FCUL, 0x8EC739C2UL, 0xE0CFC810UL, 0xB51283CFUL
    );
    static const secp256k1_scalar minus_b1 = SECP256K1_SCALAR_CONST(
        0x00000000UL, 0x00000000UL, 0x00000000UL, 0x00000000UL,
        0xE4437ED6UL, 0x010E8828UL, 0x6F547FA9UL, 0x0ABFE4C3UL
    );
    static const secp256k1_scalar minus_b2 = SECP256K1_SCALAR_CONST(
        0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFEUL,
        0x8A280AC5UL, 0x0774346DUL, 0xD765CDA8UL, 0x3DB1562CUL
    );
    static const secp256k1_scalar g1 = SECP256K1_SCALAR_CONST(
        0x00000000UL, 0x00000000UL, 0x00000000UL, 0x00003086UL,
        0xD221A7D4UL, 0x6BCDE86CUL, 0x90E49284UL, 0xEB153DABUL
    );
    static const secp256k1_scalar g2 = SECP256K1_SCALAR_CONST(
        0x00000000UL, 0x00000000UL, 0x00000000UL, 0x0000E443UL,
        0x7ED6010EUL, 0x88286F54UL, 0x7FA90ABFUL, 0xE4C42212UL
    );
    VERIFY_CHECK(r1 != a);
    VERIFY_CHECK(r2 != a);
    /*由于移位量是常量，因此这些var调用是常量时间。*/
    secp256k1_scalar_mul_shift_var(&c1, a, &g1, 272);
    secp256k1_scalar_mul_shift_var(&c2, a, &g2, 272);
    secp256k1_scalar_mul(&c1, &c1, &minus_b1);
    secp256k1_scalar_mul(&c2, &c2, &minus_b2);
    secp256k1_scalar_add(r2, &c1, &c2);
    secp256k1_scalar_mul(r1, r2, &minus_lambda);
    secp256k1_scalar_add(r1, r1, a);
}
#endif
#endif

/*dif/*secp256k1_scalar_impl_h*/
