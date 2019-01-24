
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


#ifndef SECP256K1_ECMULT_GEN_H
#define SECP256K1_ECMULT_GEN_H

#include "scalar.h"
#include "group.h"

typedef struct {
    /*对于加速计算a*g：
     *要加强抵御定时攻击，请使用以下机制：
     **将被乘数分成4位的组，称为n_0、n_1、n_2、…、n_63。
     **计算和（n诳i*16^i*g+u诳i，i=0..63），其中：
     **u_i=u*2^i（i=0..62）
     **u_i=u*（1-2^63）（i=63）
     *其中u是一个没有已知对应标量的点。注意总和（u_i，i=0..63）=0。
     *对于每个i和n_i的16个可能值中的每一个，（n_i*16^i*g+u i）是
     *预计算（称为prec（i，n_i））。公式现在变成和（prec（i，n_i），i=0..63）。
     *生成的prec group元素都没有已知的标量，也没有
     *计算a*g时的中间和。
     **/

    /*p256k1_ge_存储（*prec）[64][16]；/*prec[j][i]=16^j*i*g+u_i*/
    secp256k1_标量盲；
    secp256k1_gej首字母；
secp256k1_ecmult_gen_上下文；

静态void secp256k1_ecmult_gen_context_init（secp256k1_ecmult_gen_context*ctx）；
静态void secp256k1_ecmult_gen_context_build（secp256k1_ecmult_gen_context*ctx，const secp256k1_callback*cb）；
静态void secp256k1_ecmult_gen_context_clone（secp256k1_ecmult_gen_context*dst，
                                               const secp256k1_ecmult_gen_context*src，const secp256k1_callback*cb）；
静态无效secp256k1_ecmult_gen_context_clear（secp256k1_ecmult_gen_context*ctx）；
静态int secp256k1_ecmult_gen_context_is_build（const secp256k1_ecmult_gen_context*ctx）；

/**与发电机相乘：r=a*g*/

static void secp256k1_ecmult_gen(const secp256k1_ecmult_gen_context* ctx, secp256k1_gej *r, const secp256k1_scalar *a);

static void secp256k1_ecmult_gen_blind(secp256k1_ecmult_gen_context *ctx, const unsigned char *seed32);

/*dif/*secp256k1_ecmult_gen_h*/
