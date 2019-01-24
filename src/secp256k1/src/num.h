
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


#ifndef SECP256K1_NUM_H
#define SECP256K1_NUM_H

#ifndef USE_NUM_NONE

#if defined HAVE_CONFIG_H
#include "libsecp256k1-config.h"
#endif

#if defined(USE_NUM_GMP)
#include "num_gmp.h"
#else
#error "Please select num implementation"
#endif

/*抄一个号码。*/
static void secp256k1_num_copy(secp256k1_num *r, const secp256k1_num *a);

/*将数字的绝对值转换为二进制的big-endian字符串。
 *必须有足够的地方。*/

static void secp256k1_num_get_bin(unsigned char *r, unsigned int rlen, const secp256k1_num *a);

/*将数字设置为二进制big endian字符串的值。*/
static void secp256k1_num_set_bin(secp256k1_num *r, const unsigned char *a, unsigned int alen);

/*计算模逆。输入必须小于模数。*/
static void secp256k1_num_mod_inverse(secp256k1_num *r, const secp256k1_num *a, const secp256k1_num *m);

/*计算雅可比符号（a b）。B必须是正数和奇数。*/
static int secp256k1_num_jacobi(const secp256k1_num *a, const secp256k1_num *b);

/*比较两个数字的绝对值。*/
static int secp256k1_num_cmp(const secp256k1_num *a, const secp256k1_num *b);

/*测试两个数字是否相等（包括符号）。*/
static int secp256k1_num_eq(const secp256k1_num *a, const secp256k1_num *b);

/*加上两个（有符号的）数字。*/
static void secp256k1_num_add(secp256k1_num *r, const secp256k1_num *a, const secp256k1_num *b);

/*减去两个（有符号的）数字。*/
static void secp256k1_num_sub(secp256k1_num *r, const secp256k1_num *a, const secp256k1_num *b);

/*把两个（有符号的）数字相乘。*/
static void secp256k1_num_mul(secp256k1_num *r, const secp256k1_num *a, const secp256k1_num *b);

/*用余数替换一个数。忽略模M的符号。结果是一个介于0和m-1之间的数字，
    即使r为负。*/

static void secp256k1_num_mod(secp256k1_num *r, const secp256k1_num *m);

/*将通过的数字按位右移。*/
static void secp256k1_num_shift(secp256k1_num *r, int bits);

/*检查数字是否为零。*/
static int secp256k1_num_is_zero(const secp256k1_num *a);

/*检查数字是否为1。*/
static int secp256k1_num_is_one(const secp256k1_num *a);

/*检查数字是否严格为负数。*/
static int secp256k1_num_is_neg(const secp256k1_num *a);

/*更改数字的符号。*/
static void secp256k1_num_negate(secp256k1_num *r);

#endif

/*dif/*secp256k1_num_h*/
