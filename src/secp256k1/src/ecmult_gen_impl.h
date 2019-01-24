
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
/***************************************************************
 *版权所有（c）2013、2014、2015 Pieter Wuille、Gregory Maxwell*
 *根据麻省理工学院软件许可证分发，请参阅随附的*
 *文件复制或http://www.opensource.org/licenses/mit license.php.*
 ***************************************************************/


#ifndef SECP256K1_ECMULT_GEN_IMPL_H
#define SECP256K1_ECMULT_GEN_IMPL_H

#include "scalar.h"
#include "group.h"
#include "ecmult_gen.h"
#include "hash_impl.h"
#ifdef USE_ECMULT_STATIC_PRECOMPUTATION
#include "ecmult_static_context.h"
#endif
static void secp256k1_ecmult_gen_context_init(secp256k1_ecmult_gen_context *ctx) {
    ctx->prec = NULL;
}

static void secp256k1_ecmult_gen_context_build(secp256k1_ecmult_gen_context *ctx, const secp256k1_callback* cb) {
#ifndef USE_ECMULT_STATIC_PRECOMPUTATION
    secp256k1_ge prec[1024];
    secp256k1_gej gj;
    secp256k1_gej nums_gej;
    int i, j;
#endif

    if (ctx->prec != NULL) {
        return;
    }
#ifndef USE_ECMULT_STATIC_PRECOMPUTATION
    ctx->prec = (secp256k1_ge_storage (*)[64][16])checked_malloc(cb, sizeof(*ctx->prec));

    /*找到发电机*/
    secp256k1_gej_set_ge(&gj, &secp256k1_ge_const_g);

    /*构造一个没有已知对应标量的组元素（在我的袖子里什么都没有）。*/
    {
        static const unsigned char nums_b32[33] = "The scalar for this x is unknown";
        secp256k1_fe nums_x;
        secp256k1_ge nums_ge;
        int r;
        r = secp256k1_fe_set_b32(&nums_x, nums_b32);
        (void)r;
        VERIFY_CHECK(r);
        r = secp256k1_ge_set_xo_var(&nums_ge, &nums_x, 0);
        (void)r;
        VERIFY_CHECK(r);
        secp256k1_gej_set_ge(&nums_gej, &nums_ge);
        /*加G使x中的位均匀分布。*/
        secp256k1_gej_add_ge_var(&nums_gej, &nums_gej, &secp256k1_ge_const_g, NULL);
    }

    /*计算精度*/
    {
        /*p256k1_gej precj[1024]；/*Jacobian版本的prec。*/
        secp256k1_gej gbase；
        secp256k1_gej numsbase；
        gBase=g j；/*16^j*g*/

        /*sbase=nums_gej；/*2^j*nums.*/
        对于（j=0；j<64；j++）
            /*设置precj[j*16..J*16+15]至（NumsBase，NumsBase+GBase，…，NumsBase+15*GBase）。*/

            precj[j*16] = numsbase;
            for (i = 1; i < 16; i++) {
                secp256k1_gej_add_var(&precj[j*16 + i], &precj[j*16 + i - 1], &gbase, NULL);
            }
            /*将gBase乘以16。*/
            for (i = 0; i < 4; i++) {
                secp256k1_gej_double_var(&gbase, &gbase, NULL);
            }
            /*将numbase乘以2。*/
            secp256k1_gej_double_var(&numsbase, &numsbase, NULL);
            if (j == 62) {
                /*在上一次迭代中，numsbase是（1-2^j）*nums。*/
                secp256k1_gej_neg(&numsbase, &numsbase);
                secp256k1_gej_add_var(&numsbase, &numsbase, &nums_gej, NULL);
            }
        }
        secp256k1_ge_set_all_gej_var(prec, precj, 1024, cb);
    }
    for (j = 0; j < 64; j++) {
        for (i = 0; i < 16; i++) {
            secp256k1_ge_to_storage(&(*ctx->prec)[j][i], &prec[j*16 + i]);
        }
    }
#else
    (void)cb;
    ctx->prec = (secp256k1_ge_storage (*)[64][16])secp256k1_ecmult_static_context;
#endif
    secp256k1_ecmult_gen_blind(ctx, NULL);
}

static int secp256k1_ecmult_gen_context_is_built(const secp256k1_ecmult_gen_context* ctx) {
    return ctx->prec != NULL;
}

static void secp256k1_ecmult_gen_context_clone(secp256k1_ecmult_gen_context *dst,
                                               const secp256k1_ecmult_gen_context *src, const secp256k1_callback* cb) {
    if (src->prec == NULL) {
        dst->prec = NULL;
    } else {
#ifndef USE_ECMULT_STATIC_PRECOMPUTATION
        dst->prec = (secp256k1_ge_storage (*)[64][16])checked_malloc(cb, sizeof(*dst->prec));
        memcpy(dst->prec, src->prec, sizeof(*dst->prec));
#else
        (void)cb;
        dst->prec = src->prec;
#endif
        dst->initial = src->initial;
        dst->blind = src->blind;
    }
}

static void secp256k1_ecmult_gen_context_clear(secp256k1_ecmult_gen_context *ctx) {
#ifndef USE_ECMULT_STATIC_PRECOMPUTATION
    free(ctx->prec);
#endif
    secp256k1_scalar_clear(&ctx->blind);
    secp256k1_gej_clear(&ctx->initial);
    ctx->prec = NULL;
}

static void secp256k1_ecmult_gen(const secp256k1_ecmult_gen_context *ctx, secp256k1_gej *r, const secp256k1_scalar *gn) {
    secp256k1_ge add;
    secp256k1_ge_storage adds;
    secp256k1_scalar gnb;
    int bits;
    int i, j;
    memset(&adds, 0, sizeof(adds));
    *r = ctx->initial;
    /*通过计算（n-b）g+b g而不是n g实现的盲标量/点乘法。*/
    secp256k1_scalar_add(&gnb, gn, &ctx->blind);
    add.infinity = 0;
    for (j = 0; j < 64; j++) {
        bits = secp256k1_scalar_get_bits(&gnb, j * 4, 4);
        for (i = 0; i < 16; i++) {
            /*这将使用条件移动来避免数组索引中的任何机密数据。
             *任何使用秘密索引的行为都会导致时间安排
             *侧通道，即使缓存线访问模式是统一的。
             *参见：
             *Daniel J.Bernstein和Peter Schwabe在2013年的会议上提出了“警告之词”。
             （https://crypto绝地.org/peter/data/chesrump-20130822.pdf）和
             *“缓存攻击和对策：AES案例”，RSA 2006，
             *作者：Dag Arne Osvik、Adi Shamir和Eran Tromer
             （http://www.tau.ac.il/~tromer/papers/cache.pdf）
             **/

            secp256k1_ge_storage_cmov(&adds, &(*ctx->prec)[j][i], i == bits);
        }
        secp256k1_ge_from_storage(&add, &adds);
        secp256k1_gej_add_ge(r, r, &add);
    }
    bits = 0;
    secp256k1_ge_clear(&add);
    secp256k1_scalar_clear(&gnb);
}

/*为secp256k1_ecmult_gen设置盲值。*/
static void secp256k1_ecmult_gen_blind(secp256k1_ecmult_gen_context *ctx, const unsigned char *seed32) {
    secp256k1_scalar b;
    secp256k1_gej gb;
    secp256k1_fe s;
    unsigned char nonce32[32];
    secp256k1_rfc6979_hmac_sha256_t rng;
    int retry;
    unsigned char keydata[64] = {0};
    if (seed32 == NULL) {
        /*当seed为空时，重置初始点和盲值。*/
        secp256k1_gej_set_ge(&ctx->initial, &secp256k1_ge_const_g);
        secp256k1_gej_neg(&ctx->initial, &ctx->initial);
        secp256k1_scalar_set_int(&ctx->blind, 1);
    }
    /*前面的盲值（如果不重置）通过将其包含在哈希中向前链接。*/
    secp256k1_scalar_get_b32(nonce32, &ctx->blind);
    /*使用CSPRNG可以实现无故障接口，避免需要大量随机数据，
     *防止种子软弱或敌对。这是一个比
     *直接向调用者询问盲值，并期望他们在失败时重试。
     **/

    memcpy(keydata, nonce32, 32);
    if (seed32 != NULL) {
        memcpy(keydata + 32, seed32, 32);
    }
    secp256k1_rfc6979_hmac_sha256_initialize(&rng, keydata, seed32 ? 64 : 32);
    memset(keydata, 0, sizeof(keydata));
    /*为超出范围的结果重试，以实现一致性。*/
    do {
        secp256k1_rfc6979_hmac_sha256_generate(&rng, nonce32, 32);
        retry = !secp256k1_fe_set_b32(&s, nonce32);
        retry |= secp256k1_fe_is_zero(&s);
    /*hile（retry）；/*此分支true在密码学上不可访问。需要sha256_hmac输出>fp。*/
    /*将投影随机化，以防乘子边信道。*/

    secp256k1_gej_rescale(&ctx->initial, &s);
    secp256k1_fe_clear(&s);
    do {
        secp256k1_rfc6979_hmac_sha256_generate(&rng, nonce32, 32);
        secp256k1_scalar_set_b32(&b, nonce32, &retry);
        /*盲值为0可以工作，但会破坏投影硬化。*/
        retry |= secp256k1_scalar_is_zero(&b);
    /*hile（retry）；/*此分支true在密码学上不可访问。需要sha256_hmac output>order。*/
    secp256k1rufc6979_hmac_sha256_finalize（&rng）；
    内存集（nonce32，0，32）；
    secp256k1_ecmult_gen（CTX，&GB，&B）；
    secp256k1_scalar_negate（&b，&b）；
    CTX->盲＝B；
    ctx->initial=GB；
    secp256k1_scalar_clear（&b）；
    secp256k1_gej_clear（&gb）；
}

endif/*secp256k1_ecmult_gen_impl_h*/

