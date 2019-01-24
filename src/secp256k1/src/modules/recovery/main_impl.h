
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


#ifndef SECP256K1_MODULE_RECOVERY_MAIN_H
#define SECP256K1_MODULE_RECOVERY_MAIN_H

#include "include/secp256k1_recovery.h"

static void secp256k1_ecdsa_recoverable_signature_load(const secp256k1_context* ctx, secp256k1_scalar* r, secp256k1_scalar* s, int* recid, const secp256k1_ecdsa_recoverable_signature* sig) {
    (void)ctx;
    if (sizeof(secp256k1_scalar) == 32) {
        /*当secp256k1_标量类型正好为32字节时，使用其
         *secp256k1_ecdsa_签名内的表示，因为转换非常快。
         *请注意，secp256k1_ecdsa_signature_save必须使用相同的表示。*/

        memcpy(r, &sig->data[0], 32);
        memcpy(s, &sig->data[32], 32);
    } else {
        secp256k1_scalar_set_b32(r, &sig->data[0], NULL);
        secp256k1_scalar_set_b32(s, &sig->data[32], NULL);
    }
    *recid = sig->data[64];
}

static void secp256k1_ecdsa_recoverable_signature_save(secp256k1_ecdsa_recoverable_signature* sig, const secp256k1_scalar* r, const secp256k1_scalar* s, int recid) {
    if (sizeof(secp256k1_scalar) == 32) {
        memcpy(&sig->data[0], r, 32);
        memcpy(&sig->data[32], s, 32);
    } else {
        secp256k1_scalar_get_b32(&sig->data[0], r);
        secp256k1_scalar_get_b32(&sig->data[32], s);
    }
    sig->data[64] = recid;
}

int secp256k1_ecdsa_recoverable_signature_parse_compact(const secp256k1_context* ctx, secp256k1_ecdsa_recoverable_signature* sig, const unsigned char *input64, int recid) {
    secp256k1_scalar r, s;
    int ret = 1;
    int overflow = 0;

    (void)ctx;
    ARG_CHECK(sig != NULL);
    ARG_CHECK(input64 != NULL);
    ARG_CHECK(recid >= 0 && recid <= 3);

    secp256k1_scalar_set_b32(&r, &input64[0], &overflow);
    ret &= !overflow;
    secp256k1_scalar_set_b32(&s, &input64[32], &overflow);
    ret &= !overflow;
    if (ret) {
        secp256k1_ecdsa_recoverable_signature_save(sig, &r, &s, recid);
    } else {
        memset(sig, 0, sizeof(*sig));
    }
    return ret;
}

int secp256k1_ecdsa_recoverable_signature_serialize_compact(const secp256k1_context* ctx, unsigned char *output64, int *recid, const secp256k1_ecdsa_recoverable_signature* sig) {
    secp256k1_scalar r, s;

    (void)ctx;
    ARG_CHECK(output64 != NULL);
    ARG_CHECK(sig != NULL);
    ARG_CHECK(recid != NULL);

    secp256k1_ecdsa_recoverable_signature_load(ctx, &r, &s, recid, sig);
    secp256k1_scalar_get_b32(&output64[0], &r);
    secp256k1_scalar_get_b32(&output64[32], &s);
    return 1;
}

int secp256k1_ecdsa_recoverable_signature_convert(const secp256k1_context* ctx, secp256k1_ecdsa_signature* sig, const secp256k1_ecdsa_recoverable_signature* sigin) {
    secp256k1_scalar r, s;
    int recid;

    (void)ctx;
    ARG_CHECK(sig != NULL);
    ARG_CHECK(sigin != NULL);

    secp256k1_ecdsa_recoverable_signature_load(ctx, &r, &s, &recid, sigin);
    secp256k1_ecdsa_signature_save(sig, &r, &s);
    return 1;
}

static int secp256k1_ecdsa_sig_recover(const secp256k1_ecmult_context *ctx, const secp256k1_scalar *sigr, const secp256k1_scalar* sigs, secp256k1_ge *pubkey, const secp256k1_scalar *message, int recid) {
    unsigned char brx[32];
    secp256k1_fe fx;
    secp256k1_ge x;
    secp256k1_gej xj;
    secp256k1_scalar rn, u1, u2;
    secp256k1_gej qj;
    int r;

    if (secp256k1_scalar_is_zero(sigr) || secp256k1_scalar_is_zero(sigs)) {
        return 0;
    }

    secp256k1_scalar_get_b32(brx, sigr);
    r = secp256k1_fe_set_b32(&fx, brx);
    (void)r;
    /*ify_check（r）；/*brx来自标量，因此小于顺序；当然小于p*/
    如果（recid&2）
        if（secp256k1_fe_cmp_var（&fx，&secp256k1_ecdsa_const_p_minus_order）>=0）
            返回0；
        }
        secp256k1_fe_add（&fx，&secp256k1_ecdsa_const_order_as_fe）；
    }
    如果（！）secp256k1_ge_set_xo_var（&x，&fx，recid&1））
        返回0；
    }
    secp256k1_gej_set_ge（&xj，&x）；
    secp256k1_scalar_inverse_var（&rn，sigr）；
    secp256k1_scalar_mul（&u1，&rn，message）；
    secp256k1_scalar_negate（&u1，&u1）；
    secp256k1_scalar_mul（&u2，&rn，sigs）；
    secp256k1_ecmult（CTX、QJ、XJ、U2和U1）；
    secp256k1_ge_set_gej_var（pubkey，&qj）；
    回来！secp256k1_gej_表示无穷大（&qj）；
}

int secp256k1_ecdsa_sign_recoverable（const secp256k1_context*ctx，secp256k1_ecdsa_recoverable_signature*签名，const unsigned char*msg32，const unsigned char*seckey，secp256k1_nonce_function noncefp，const void*noncedata）
    secp256k1_标量r，s；
    secp256k1_scalar sec，non，msg；
    INT累加；
    INT RET＝0；
    int溢出=0；
    确认检查（CTX！= NULL）；
    arg_check（secp256k1_ecmult_gen_context_is_build（&ctx->ecmult_gen_ctx））；
    arg_检查（msg32！= NULL）；
    银联支票（签名！= NULL）；
    arg_检查（seckey！= NULL）；
    if（noncefp==null）
        noncefp=secp256k1_nonce_函数_默认值；
    }

    secp256k1_scalar_set_b32（&sec、seckey和overflow）；
    /*如果密钥无效，则失败。*/

    if (!overflow && !secp256k1_scalar_is_zero(&sec)) {
        unsigned char nonce32[32];
        unsigned int count = 0;
        secp256k1_scalar_set_b32(&msg, msg32, NULL);
        while (1) {
            ret = noncefp(nonce32, msg32, seckey, NULL, (void*)noncedata, count);
            if (!ret) {
                break;
            }
            secp256k1_scalar_set_b32(&non, nonce32, &overflow);
            if (!secp256k1_scalar_is_zero(&non) && !overflow) {
                if (secp256k1_ecdsa_sig_sign(&ctx->ecmult_gen_ctx, &r, &s, &sec, &msg, &non, &recid)) {
                    break;
                }
            }
            count++;
        }
        memset(nonce32, 0, 32);
        secp256k1_scalar_clear(&msg);
        secp256k1_scalar_clear(&non);
        secp256k1_scalar_clear(&sec);
    }
    if (ret) {
        secp256k1_ecdsa_recoverable_signature_save(signature, &r, &s, recid);
    } else {
        memset(signature, 0, sizeof(*signature));
    }
    return ret;
}

int secp256k1_ecdsa_recover(const secp256k1_context* ctx, secp256k1_pubkey *pubkey, const secp256k1_ecdsa_recoverable_signature *signature, const unsigned char *msg32) {
    secp256k1_ge q;
    secp256k1_scalar r, s;
    secp256k1_scalar m;
    int recid;
    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(secp256k1_ecmult_context_is_built(&ctx->ecmult_ctx));
    ARG_CHECK(msg32 != NULL);
    ARG_CHECK(signature != NULL);
    ARG_CHECK(pubkey != NULL);

    secp256k1_ecdsa_recoverable_signature_load(ctx, &r, &s, &recid, signature);
    /*ify_check（recid>=0&&recid<4）；/*应该在parse_compact*中被捕获。
    secp256k1_scalar_set_b32（&m，msg32，空）；
    if（secp256k1_ecdsa_sig_recover（&ctx->ecmult_ctx，&r，&s，&q，&m，recid））
        secp256k1_pubkey_save（pubkey，&q）；
        返回1；
    }否则{
        memset（pubkey，0，sizeof（*pubkey））；
        返回0；
    }
}

endif/*secp256k1_模块_恢复_主_h*/

