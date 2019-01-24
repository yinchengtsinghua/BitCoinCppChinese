
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


#ifndef SECP256K1_ECMULT_H
#define SECP256K1_ECMULT_H

#include "num.h"
#include "group.h"

typedef struct {
    /*加速计算a*p+b*g：*/
    /*p256k1_ge_存储（*pre_g）[]；/*发生器的奇数倍*/
使用自同态
    secp256k1_ge_存储（*pre_g_128）[]；/*2^128*生成器的奇数倍*/

#endif
} secp256k1_ecmult_context;

static void secp256k1_ecmult_context_init(secp256k1_ecmult_context *ctx);
static void secp256k1_ecmult_context_build(secp256k1_ecmult_context *ctx, const secp256k1_callback *cb);
static void secp256k1_ecmult_context_clone(secp256k1_ecmult_context *dst,
                                           const secp256k1_ecmult_context *src, const secp256k1_callback *cb);
static void secp256k1_ecmult_context_clear(secp256k1_ecmult_context *ctx);
static int secp256k1_ecmult_context_is_built(const secp256k1_ecmult_context *ctx);

/*双乘：r=na*a+ng*g*/
static void secp256k1_ecmult(const secp256k1_ecmult_context *ctx, secp256k1_gej *r, const secp256k1_gej *a, const secp256k1_scalar *na, const secp256k1_scalar *ng);

/*dif/*secp256k1_ecmult_h*/
