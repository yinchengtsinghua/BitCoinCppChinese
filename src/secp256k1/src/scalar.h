
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


#ifndef SECP256K1_SCALAR_H
#define SECP256K1_SCALAR_H

#include "num.h"

#if defined HAVE_CONFIG_H
#include "libsecp256k1-config.h"
#endif

#if defined(EXHAUSTIVE_TEST_ORDER)
#include "scalar_low.h"
#elif defined(USE_SCALAR_4X64)
#include "scalar_4x64.h"
#elif defined(USE_SCALAR_8X32)
#include "scalar_8x32.h"
#else
#error "Please select scalar implementation"
#endif

/*清除标量以防止敏感数据泄漏。*/
static void secp256k1_scalar_clear(secp256k1_scalar *r);

/*从标量访问位。所有请求的位必须属于同一个32位分支。*/
static unsigned int secp256k1_scalar_get_bits(const secp256k1_scalar *a, unsigned int offset, unsigned int count);

/*从标量访问位。不是固定时间。*/
static unsigned int secp256k1_scalar_get_bits_var(const secp256k1_scalar *a, unsigned int offset, unsigned int count);

/*从big endian字节数组中设置一个标量。*/
static void secp256k1_scalar_set_b32(secp256k1_scalar *r, const unsigned char *bin, int *overflow);

/*将标量设置为无符号整数。*/
static void secp256k1_scalar_set_int(secp256k1_scalar *r, unsigned int v);

/*将标量转换为字节数组。*/
static void secp256k1_scalar_get_b32(unsigned char *bin, const secp256k1_scalar* a);

/*将两个标量加在一起（对组顺序进行模运算）。返回是否溢出。*/
static int secp256k1_scalar_add(secp256k1_scalar *r, const secp256k1_scalar *a, const secp256k1_scalar *b);

/*有条件地将2的幂加到一个标量上。不允许结果溢出。*/
static void secp256k1_scalar_cadd_bit(secp256k1_scalar *r, unsigned int bit, int flag);

/*将两个标量相乘（与组顺序成模）。*/
static void secp256k1_scalar_mul(secp256k1_scalar *r, const secp256k1_scalar *a, const secp256k1_scalar *b);

/*严格地将标量右移0到16之间的某个值，返回
 *移位的低位*/

static int secp256k1_scalar_shr_int(secp256k1_scalar *r, int n);

/*计算一个标量的平方（按组顺序模化）。*/
static void secp256k1_scalar_sqr(secp256k1_scalar *r, const secp256k1_scalar *a);

/*计算一个标量的倒数（按组顺序取模）。*/
static void secp256k1_scalar_inverse(secp256k1_scalar *r, const secp256k1_scalar *a);

/*计算一个标量的倒数（模化为组顺序），不需要恒定的时间保证。*/
static void secp256k1_scalar_inverse_var(secp256k1_scalar *r, const secp256k1_scalar *a);

/*计算一个标量的补码（按组顺序取模）。*/
static void secp256k1_scalar_negate(secp256k1_scalar *r, const secp256k1_scalar *a);

/*检查标量是否等于零。*/
static int secp256k1_scalar_is_zero(const secp256k1_scalar *a);

/*检查标量是否等于1。*/
static int secp256k1_scalar_is_one(const secp256k1_scalar *a);

/*检查作为非负整数的标量是否为偶数。*/
static int secp256k1_scalar_is_even(const secp256k1_scalar *a);

/*检查标量是否高于组顺序除以2。*/
static int secp256k1_scalar_is_high(const secp256k1_scalar *a);

/*在恒定时间内有条件地求反一个数。
 *如果数字为负数，则返回-1，否则返回1。*/

static int secp256k1_scalar_cond_negate(secp256k1_scalar *a, int flag);

#ifndef USE_NUM_NONE
/*将标量转换为数字。*/
static void secp256k1_scalar_get_num(secp256k1_num *r, const secp256k1_scalar *a);

/*以数字形式获取组的顺序。*/
static void secp256k1_scalar_order_get_num(secp256k1_num *r);
#endif

/*比较两个标量。*/
static int secp256k1_scalar_eq(const secp256k1_scalar *a, const secp256k1_scalar *b);

#ifdef USE_ENDOMORPHISM
/*求r1和r2，使r1+r2*2^128=a。*/
static void secp256k1_scalar_split_128(secp256k1_scalar *r1, secp256k1_scalar *r2, const secp256k1_scalar *a);
/*查找r1和r2，使r1+r2*lambda=a，并且r1和r2的最大长度为128位（请参见secp256k1_gej_mul_lambda）。*/
static void secp256k1_scalar_split_lambda(secp256k1_scalar *r1, secp256k1_scalar *r2, const secp256k1_scalar *a);
#endif

/*乘以A和B（不取模量！），除以2**移位，然后四舍五入到最接近的整数。移位必须至少为256。*/
static void secp256k1_scalar_mul_shift_var(secp256k1_scalar *r, const secp256k1_scalar *a, const secp256k1_scalar *b, unsigned int shift);

/*dif/*secp256k1_scalar_h*/
