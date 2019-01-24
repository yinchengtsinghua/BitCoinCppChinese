
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
/***************************************************************
 *版权所有（c）2015 Pieter Wuille，Andrew Poelstra*
 *根据麻省理工学院软件许可证分发，请参阅随附的*
 *文件复制或http://www.opensource.org/licenses/mit license.php.*
 ***************************************************************/


#ifndef SECP256K1_ECMULT_CONST_IMPL_H
#define SECP256K1_ECMULT_CONST_IMPL_H

#include "scalar.h"
#include "group.h"
#include "ecmult_const.h"
#include "ecmult_impl.h"

#ifdef USE_ENDOMORPHISM
    #define WNAF_BITS 128
#else
    #define WNAF_BITS 256
#endif
#define WNAF_SIZE(w) ((WNAF_BITS + (w) - 1) / (w))

/*这类似于“ecmult”表，但时间不变*/
#define ECMULT_CONST_TABLE_GET_GE(r,pre,n,w) do { \
    int m; \
    int abs_n = (n) * (((n) > 0) * 2 - 1); \
    int idx_n = abs_n / 2; \
    secp256k1_fe neg_y; \
    VERIFY_CHECK(((n) & 1) == 1); \
    VERIFY_CHECK((n) >= -((1 << ((w)-1)) - 1)); \
    VERIFY_CHECK((n) <=  ((1 << ((w)-1)) - 1)); \
    VERIFY_SETUP(secp256k1_fe_clear(&(r)->x)); \
    VERIFY_SETUP(secp256k1_fe_clear(&(r)->y)); \
    for (m = 0; m < ECMULT_TABLE_SIZE(w); m++) { \
        /*此循环用于避免数组索引中的机密数据。见
         *有关理由，请参阅ecmult_gen_impl.h中的注释。*/ \

        secp256k1_fe_cmov(&(r)->x, &(pre)[m].x, m == idx_n); \
        secp256k1_fe_cmov(&(r)->y, &(pre)[m].y, m == idx_n); \
    } \
    (r)->infinity = 0; \
    secp256k1_fe_negate(&neg_y, &(r)->y, 1); \
    secp256k1_fe_cmov(&(r)->y, &neg_y, (n) != abs_n); \
} while(0)


/*将数字转换为wnaf符号。
 *该数字由SUM（2 ^ w i*wnaf[i]，i=0..wnaf_大小（w）+1）-返回值表示。
 *有以下保证：
 *-每个wnaf[i]一个介于（1<<w）和（1<<w）之间的奇数
 *-每个wnaf[i]不是零
 *-字数设置始终为wnaf_大小（w）+1
 *
 *改编自“width-w naf方法，内存小，椭圆标量快。
 *乘法安全地抵御侧通道攻击`、Okeya和Tagaki。M. Joye（E.）
 *CT-RSA 2003，LNCS 2612，第328-443页，2003年。柏林海德堡2003年
 *
 *数字参考第335页“算法抗SPA宽度-W奇数标量NAF”的步骤。
 **/

static int secp256k1_wnaf_const(int *wnaf, secp256k1_scalar s, int w) {
    int global_sign;
    int skew = 0;
    int word = 0;

    /*1 2 2*/
    int u_last;
    int u;

    int flip;
    int bit;
    secp256k1_scalar neg_s;
    int not_neg_one;
    /*请注意，我们不能通过将偶数求反为奇数来处理偶数。
     *在其他实现中完成，因为如果我们的scalar被指定为
     *宽度小于256由于性能原因，它们的否定宽度为256。
     *我们将失去任何性能优势。相反，我们使用的技术来自
     *Okeya/Tagaki论文的第4.2节，将添加1（偶数）
     *或2（奇数），返回一个表示
     *这样，让调用者在进行乘法运算后进行补偿。*/


    /*负数将被求反以使其位表示低于最大宽度。*/
    flip = secp256k1_scalar_is_high(&s);
    /*我们把1加上偶数，2加上奇数，注意负数会翻转奇偶性。*/
    bit = flip ^ !secp256k1_scalar_is_even(&s);
    /*我们检查负1，因为加2会导致溢出*/
    secp256k1_scalar_negate(&neg_s, &s);
    not_neg_one = !secp256k1_scalar_is_one(&neg_s);
    secp256k1_scalar_cadd_bit(&s, bit, not_neg_one);
    /*如果是负的，那么flip==1，s.d[0]==0，bit==1，所以调用者需要
     *我们加了两个，然后翻转了它。实际上，对于-1，这些操作是
     *完全相同。我们只是翻转，但由于需要倾斜（从这个意义上说
     *倾斜必须为1或2，绝不为零），而翻转则不是，我们需要更改
     *我们的旗帜宣称我们只是歪斜。*/

    global_sign = secp256k1_scalar_cond_negate(&s, flip);
    global_sign *= not_neg_one * 2 - 1;
    skew = 1 << bit;

    /*四*/
    u_last = secp256k1_scalar_shr_int(&s, w);
    while (word * w < WNAF_BITS) {
        int sign;
        int even;

        /*4.1 4.4*/
        u = secp256k1_scalar_shr_int(&s, w);
        /*四点二*/
        even = ((u & 1) == 0);
        sign = 2 * (u_last > 0) - 1;
        u += sign * even;
        u_last -= sign * even * (1 << w);

        /*4.3，适应全球标志变更*/
        wnaf[word++] = u_last * global_sign;

        u_last = u;
    }
    wnaf[word] = u * global_sign;

    VERIFY_CHECK(secp256k1_scalar_is_zero(&s));
    VERIFY_CHECK(word == WNAF_SIZE(w));
    return skew;
}


static void secp256k1_ecmult_const(secp256k1_gej *r, const secp256k1_ge *a, const secp256k1_scalar *scalar) {
    secp256k1_ge pre_a[ECMULT_TABLE_SIZE(WINDOW_A)];
    secp256k1_ge tmpa;
    secp256k1_fe Z;

    int skew_1;
    int wnaf_1[1 + WNAF_SIZE(WINDOW_A - 1)];
#ifdef USE_ENDOMORPHISM
    secp256k1_ge pre_a_lam[ECMULT_TABLE_SIZE(WINDOW_A)];
    int wnaf_lam[1 + WNAF_SIZE(WINDOW_A - 1)];
    int skew_lam;
    secp256k1_scalar q_1, q_lam;
#endif

    int i;
    secp256k1_scalar sc = *scalar;

    /*为Q构建Wnaf表示。*/
#ifdef USE_ENDOMORPHISM
    /*将q分为q_1和q_lam（其中q=q_1+q_lam*lambda，q_1和q_lam约为128位）*/
    secp256k1_scalar_split_lambda(&q_1, &q_lam, &sc);
    skew_1   = secp256k1_wnaf_const(wnaf_1,   q_1,   WINDOW_A - 1);
    skew_lam = secp256k1_wnaf_const(wnaf_lam, q_lam, WINDOW_A - 1);
#else
    skew_1   = secp256k1_wnaf_const(wnaf_1, sc, WINDOW_A - 1);
#endif

    /*计算a的奇数倍。
     *所有倍数的z'分母相同，存储在同一个z'分母中。
     *在Z中，由于secp256k1的同构性，我们可以假装所有操作
     *z坐标为1，使用仿射加法公式，并更正
     *结果的Z坐标在末尾一次。
     **/

    secp256k1_gej_set_ge(r, a);
    secp256k1_ecmult_odd_multiples_table_globalz_windowa(pre_a, &Z, r);
    for (i = 0; i < ECMULT_TABLE_SIZE(WINDOW_A); i++) {
        secp256k1_fe_normalize_weak(&pre_a[i].y);
    }
#ifdef USE_ENDOMORPHISM
    for (i = 0; i < ECMULT_TABLE_SIZE(WINDOW_A); i++) {
        secp256k1_ge_mul_lambda(&pre_a_lam[i], &pre_a[i]);
    }
#endif

    /*第一个循环迭代（分离出来，这样我们可以直接设置r
     *比从无穷大开始，要翻几倍，然后
     *增加了新的价值）*/

    i = wnaf_1[WNAF_SIZE(WINDOW_A - 1)];
    VERIFY_CHECK(i != 0);
    ECMULT_CONST_TABLE_GET_GE(&tmpa, pre_a, i, WINDOW_A);
    secp256k1_gej_set_ge(r, &tmpa);
#ifdef USE_ENDOMORPHISM
    i = wnaf_lam[WNAF_SIZE(WINDOW_A - 1)];
    VERIFY_CHECK(i != 0);
    ECMULT_CONST_TABLE_GET_GE(&tmpa, pre_a_lam, i, WINDOW_A);
    secp256k1_gej_add_ge(r, r, &tmpa);
#endif
    /*剩余循环迭代*/
    for (i = WNAF_SIZE(WINDOW_A - 1) - 1; i >= 0; i--) {
        int n;
        int j;
        for (j = 0; j < WINDOW_A - 1; ++j) {
            secp256k1_gej_double_nonzero(r, r, NULL);
        }

        n = wnaf_1[i];
        ECMULT_CONST_TABLE_GET_GE(&tmpa, pre_a, n, WINDOW_A);
        VERIFY_CHECK(n != 0);
        secp256k1_gej_add_ge(r, r, &tmpa);
#ifdef USE_ENDOMORPHISM
        n = wnaf_lam[i];
        ECMULT_CONST_TABLE_GET_GE(&tmpa, pre_a_lam, n, WINDOW_A);
        VERIFY_CHECK(n != 0);
        secp256k1_gej_add_ge(r, r, &tmpa);
#endif
    }

    secp256k1_fe_mul(&r->z, &r->z, &Z);

    {
        /*纠正Wnaf歪斜*/
        secp256k1_ge correction = *a;
        secp256k1_ge_storage correction_1_stor;
#ifdef USE_ENDOMORPHISM
        secp256k1_ge_storage correction_lam_stor;
#endif
        secp256k1_ge_storage a2_stor;
        secp256k1_gej tmpj;
        secp256k1_gej_set_ge(&tmpj, &correction);
        secp256k1_gej_double_var(&tmpj, &tmpj, NULL);
        secp256k1_ge_set_gej(&correction, &tmpj);
        secp256k1_ge_to_storage(&correction_1_stor, a);
#ifdef USE_ENDOMORPHISM
        secp256k1_ge_to_storage(&correction_lam_stor, a);
#endif
        secp256k1_ge_to_storage(&a2_stor, &correction);

        /*对于奇数，这是2a（所以替换它），对于偶数a（所以不op）*/
        secp256k1_ge_storage_cmov(&correction_1_stor, &a2_stor, skew_1 == 2);
#ifdef USE_ENDOMORPHISM
        secp256k1_ge_storage_cmov(&correction_lam_stor, &a2_stor, skew_lam == 2);
#endif

        /*应用更正*/
        secp256k1_ge_from_storage(&correction, &correction_1_stor);
        secp256k1_ge_neg(&correction, &correction);
        secp256k1_gej_add_ge(r, r, &correction);

#ifdef USE_ENDOMORPHISM
        secp256k1_ge_from_storage(&correction, &correction_lam_stor);
        secp256k1_ge_neg(&correction, &correction);
        secp256k1_ge_mul_lambda(&correction, &correction);
        secp256k1_gej_add_ge(r, r, &correction);
#endif
    }
}

/*dif/*secp256k1_ecmult_const_impl_h*/
