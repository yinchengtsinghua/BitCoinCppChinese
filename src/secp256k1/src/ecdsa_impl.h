
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



#ifndef SECP256K1_ECDSA_IMPL_H
#define SECP256K1_ECDSA_IMPL_H

#include "scalar.h"
#include "field.h"
#include "group.h"
#include "ecmult.h"
#include "ecmult_gen.h"
#include "ecdsa.h"

/*在“有效密码标准”（sec2）2.7.1中定义为“n”的secp256k1的组顺序
 *sage:xrange（1023，-1，-1）中的t：
 *P=2**256-2**32-t
 *如果p.isou prime（）：
 *打印“%x”%p
 *打破
 *'ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff2f'
 *鼠尾草：A＝0
 *鼠尾草：B＝7
 *Sage:F=菲尼特菲尔德（P）
 *sage:'%x'%（ellipticcurve（[f（a），f（b）]）.order（））
 *'fffffffffffffffffffffffffffffffff ebaedce6af48a03bbfd25e8cd036441'
 **/

static const secp256k1_fe secp256k1_ecdsa_const_order_as_fe = SECP256K1_FE_CONST(
    0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFEUL,
    0xBAAEDCE6UL, 0xAF48A03BUL, 0xBFD25E8CUL, 0xD0364141UL
);

/*字段和顺序之间的差异，在中定义的值“p”和“n”
 *“高效密码术标准”（第2节）2.7.1。
 *鼠尾草：p=0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff2f
 *鼠尾草：A＝0
 *鼠尾草：B＝7
 *Sage:F=菲尼特菲尔德（P）
 *sage:'%x'%（p-ellipticcurve（[f（a），f（b））.order（））
 *'14551231950B75FC4402DA1722FC9BAEE'
 **/

static const secp256k1_fe secp256k1_ecdsa_const_p_minus_order = SECP256K1_FE_CONST(
    0, 0, 0, 1, 0x45512319UL, 0x50B75FC4UL, 0x402DA172UL, 0x2FC9BAEEUL
);

static int secp256k1_der_read_len(const unsigned char **sigp, const unsigned char *sigend) {
    int lenleft, b1;
    size_t ret = 0;
    if (*sigp >= sigend) {
        return -1;
    }
    b1 = *((*sigp)++);
    if (b1 == 0xFF) {
        /*X.690-0207 8.1.3.5.c不得使用值0xff。*/
        return -1;
    }
    if ((b1 & 0x80) == 0) {
        /*X.690-0207 8.1.3.4短格式长度八位字节*/
        return b1;
    }
    if (b1 == 0x80) {
        /*DER中不允许使用不定长度。*/
        return -1;
    }
    /*X.690-207 8.1.3.5长八位字节*/
    lenleft = b1 & 0x7F;
    if (lenleft > sigend - *sigp) {
        return -1;
    }
    if (**sigp == 0) {
        /*不是最短的长度编码。*/
        return -1;
    }
    if ((size_t)lenleft > sizeof(size_t)) {
        /*结果的长度将超过尺寸范围，因此
         *肯定比传递的数组大小长。
         **/

        return -1;
    }
    while (lenleft > 0) {
        ret = (ret << 8) | **sigp;
        if (ret + lenleft > (size_t)(sigend - *sigp)) {
            /*结果超过了传递的数组的长度。*/
            return -1;
        }
        (*sigp)++;
        lenleft--;
    }
    if (ret < 128) {
        /*不是最短的长度编码。*/
        return -1;
    }
    return ret;
}

static int secp256k1_der_parse_integer(secp256k1_scalar *r, const unsigned char **sig, const unsigned char *sigend) {
    int overflow = 0;
    unsigned char ra[32] = {0};
    int rlen;

    if (*sig == sigend || **sig != 0x02) {
        /*不是原始整数（X.690-0207 8.3.1）。*/
        return 0;
    }
    (*sig)++;
    rlen = secp256k1_der_read_len(sig, sigend);
    if (rlen <= 0 || (*sig) + rlen > sigend) {
        /*超出界限或长度不小于1（X.690-0207 8.3.1）。*/
        return 0;
    }
    if (**sig == 0x00 && rlen > 1 && (((*sig)[1]) & 0x80) == 0x00) {
        /*0x00填充过多。*/
        return 0;
    }
    if (**sig == 0xFF && rlen > 1 && (((*sig)[1]) & 0x80) == 0x80) {
        /*0xFF填充过多。*/
        return 0;
    }
    if ((**sig & 0x80) == 0x80) {
        /*否定的。*/
        overflow = 1;
    }
    while (rlen > 0 && **sig == 0) {
        /*跳过前导零字节*/
        rlen--;
        (*sig)++;
    }
    if (rlen > 32) {
        overflow = 1;
    }
    if (!overflow) {
        memcpy(ra + 32 - rlen, *sig, rlen);
        secp256k1_scalar_set_b32(r, ra, &overflow);
    }
    if (overflow) {
        secp256k1_scalar_set_int(r, 0);
    }
    (*sig) += rlen;
    return 1;
}

static int secp256k1_ecdsa_sig_parse(secp256k1_scalar *rr, secp256k1_scalar *rs, const unsigned char *sig, size_t size) {
    const unsigned char *sigend = sig + size;
    int rlen;
    if (sig == sigend || *(sig++) != 0x30) {
        /*编码不是以构造的序列（X.690-0207 8.9.1）开始的。*/
        return 0;
    }
    rlen = secp256k1_der_read_len(&sig, sigend);
    if (rlen < 0 || sig + rlen > sigend) {
        /*元组超出界限*/
        return 0;
    }
    if (sig + rlen != sigend) {
        /*一个接一个的垃圾。*/
        return 0;
    }

    if (!secp256k1_der_parse_integer(rr, &sig, sigend)) {
        return 0;
    }
    if (!secp256k1_der_parse_integer(rs, &sig, sigend)) {
        return 0;
    }

    if (sig != sigend) {
        /*在元组中拖拽垃圾。*/
        return 0;
    }

    return 1;
}

static int secp256k1_ecdsa_sig_serialize(unsigned char *sig, size_t *size, const secp256k1_scalar* ar, const secp256k1_scalar* as) {
    unsigned char r[33] = {0}, s[33] = {0};
    unsigned char *rp = r, *sp = s;
    size_t lenR = 33, lenS = 33;
    secp256k1_scalar_get_b32(&r[1], ar);
    secp256k1_scalar_get_b32(&s[1], as);
    while (lenR > 1 && rp[0] == 0 && rp[1] < 0x80) { lenR--; rp++; }
    while (lenS > 1 && sp[0] == 0 && sp[1] < 0x80) { lenS--; sp++; }
    if (*size < 6+lenS+lenR) {
        *size = 6 + lenS + lenR;
        return 0;
    }
    *size = 6 + lenS + lenR;
    sig[0] = 0x30;
    sig[1] = 4 + lenS + lenR;
    sig[2] = 0x02;
    sig[3] = lenR;
    memcpy(sig+4, rp, lenR);
    sig[4+lenR] = 0x02;
    sig[5+lenR] = lenS;
    memcpy(sig+lenR+6, sp, lenS);
    return 1;
}

static int secp256k1_ecdsa_sig_verify(const secp256k1_ecmult_context *ctx, const secp256k1_scalar *sigr, const secp256k1_scalar *sigs, const secp256k1_ge *pubkey, const secp256k1_scalar *message) {
    unsigned char c[32];
    secp256k1_scalar sn, u1, u2;
#if !defined(EXHAUSTIVE_TEST_ORDER)
    secp256k1_fe xr;
#endif
    secp256k1_gej pubkeyj;
    secp256k1_gej pr;

    if (secp256k1_scalar_is_zero(sigr) || secp256k1_scalar_is_zero(sigs)) {
        return 0;
    }

    secp256k1_scalar_inverse_var(&sn, sigs);
    secp256k1_scalar_mul(&u1, &sn, message);
    secp256k1_scalar_mul(&u2, &sn, sigr);
    secp256k1_gej_set_ge(&pubkeyj, pubkey);
    secp256k1_ecmult(ctx, &pr, &pubkeyj, &u2, &u1);
    if (secp256k1_gej_is_infinity(&pr)) {
        return 0;
    }

#if defined(EXHAUSTIVE_TEST_ORDER)
{
    secp256k1_scalar computed_r;
    secp256k1_ge pr_ge;
    secp256k1_ge_set_gej(&pr_ge, &pr);
    secp256k1_fe_normalize(&pr_ge.x);

    secp256k1_fe_get_b32(c, &pr_ge.x);
    secp256k1_scalar_set_b32(&computed_r, c, NULL);
    return secp256k1_scalar_eq(sigr, &computed_r);
}
#else
    secp256k1_scalar_get_b32(c, sigr);
    secp256k1_fe_set_b32(&xr, c);

    /*我们现在在pr中有重新计算的r点，以及它声称的x坐标（模n）
     *在XR中。简单地说，我们将从pr中提取x坐标（需要一个反转模p）。
     *计算剩余模n，并与xr进行比较。然而：
     *
     *xr==x（pr）模式n
     *<=>存在h.（xr+h*n<p&&xr+h*n==x（pr））
     *[因为2*n>p，h只能是0或1]
     *<=>（xr==x（pr））（xr+n<p&&xr+n==x（pr））
     *[在雅可比坐标系中，x（pr）是pr.x/pr.z^2 mod p]
     *<=>（xr==pr.x/pr.z^2 mod p）（xr+n<p&&xr+n==pr.x/pr.z^2 mod p）
     *[方程两边乘以pr.z^2 mod p]
     *<=>（xr*pr.z^2 mod p==pr.x）（xr+n<p&&（xr+n）*pr.z^2 mod p==pr.x）
     *
     *因此，我们可以避免倒置，但我们必须分别检查这两种情况。
     *secp256k1_gej_eq_x执行（xr*pr.z^2 mod p==pr.x）测试。
     **/

    if (secp256k1_gej_eq_x_var(&xr, &pr)) {
        /*xr*pr.z^2 mod p==pr.x，因此签名有效。*/
        return 1;
    }
    if (secp256k1_fe_cmp_var(&xr, &secp256k1_ecdsa_const_p_minus_order) >= 0) {
        /*xr+n>=p，所以我们可以跳过第二种情况的测试。*/
        return 0;
    }
    secp256k1_fe_add(&xr, &secp256k1_ecdsa_const_order_as_fe);
    if (secp256k1_gej_eq_x_var(&xr, &pr)) {
        /*（xr+n）*pr.z^2 mod p==pr.x，因此签名有效。*/
        return 1;
    }
    return 0;
#endif
}

static int secp256k1_ecdsa_sig_sign(const secp256k1_ecmult_gen_context *ctx, secp256k1_scalar *sigr, secp256k1_scalar *sigs, const secp256k1_scalar *seckey, const secp256k1_scalar *message, const secp256k1_scalar *nonce, int *recid) {
    unsigned char b[32];
    secp256k1_gej rp;
    secp256k1_ge r;
    secp256k1_scalar n;
    int overflow = 0;

    secp256k1_ecmult_gen(ctx, &rp, nonce);
    secp256k1_ge_set_gej(&r, &rp);
    secp256k1_fe_normalize(&r.x);
    secp256k1_fe_normalize(&r.y);
    secp256k1_fe_get_b32(b, &r.x);
    secp256k1_scalar_set_b32(sigr, b, &overflow);
    /*这两个条件应该在呼叫前检查*/
    VERIFY_CHECK(!secp256k1_scalar_is_zero(sigr));
    VERIFY_CHECK(overflow == 0);

    if (recid) {
        /*溢出条件在密码学上是不可访问的，因为命中它需要找到离散的日志。
         *其中p.x>=顺序，只有1/2^127点符合此标准。
         **/

        *recid = (overflow ? 2 : 0) | (secp256k1_fe_is_odd(&r.y) ? 1 : 0);
    }
    secp256k1_scalar_mul(&n, sigr, seckey);
    secp256k1_scalar_add(&n, &n, message);
    secp256k1_scalar_inverse(sigs, nonce);
    secp256k1_scalar_mul(sigs, sigs, &n);
    secp256k1_scalar_clear(&n);
    secp256k1_gej_clear(&rp);
    secp256k1_ge_clear(&r);
    if (secp256k1_scalar_is_zero(sigs)) {
        return 0;
    }
    if (secp256k1_scalar_is_high(sigs)) {
        secp256k1_scalar_negate(sigs, sigs);
        if (recid) {
            *recid ^= 1;
        }
    }
    return 1;
}

/*dif/*secp256k1_ecdsa_impl_h*/
