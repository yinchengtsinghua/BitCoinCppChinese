
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


#ifndef SECP256K1_GROUP_IMPL_H
#define SECP256K1_GROUP_IMPL_H

#include "num.h"
#include "field.h"
#include "group.h"

/*这些点可以在SAGE中生成，如下所示：
 *
 * 0。使用以下参数设置工作表。
 *b=4任意曲线将被设置为
 *f=最终字段（0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffc2f）
 *c=椭圆曲线（[f（0），f（b）]）
 *
 * 1。确定所有的小订单。（如果有
 *没有满意的，回去换b。）
 *打印c.order（）。系数（限制=1000）
 *
 * 2。选择一个顺序作为上面步骤中列出的主要因素之一。
 （您也可以用一些乘法得到一个复合顺序，尽管
 *在签名过程中，如果尝试反转标量，测试将崩溃。）我们将
 *随机点并缩放以将其顺序降到所需值。
 *这有可能不起作用，请再试一次。
 ＊阶数＝199
 *p=c.随机点（
 *P=（int（p.order（））/int（order））*P
 *断言（p.order（）==顺序）
 *
 * 3。打印值。您需要使用vim宏或其他
 *将十六进制输出拆分为4字节块。
 *打印“%x%x”%p.xy（）
 **/

#if defined(EXHAUSTIVE_TEST_ORDER)
#  if EXHAUSTIVE_TEST_ORDER == 199
const secp256k1_ge secp256k1_ge_const_g = SECP256K1_GE_CONST(
    0xFA7CC9A7, 0x0737F2DB, 0xA749DD39, 0x2B4FB069,
    0x3B017A7D, 0xA808C2F1, 0xFB12940C, 0x9EA66C18,
    0x78AC123A, 0x5ED8AEF3, 0x8732BC91, 0x1F3A2868,
    0x48DF246C, 0x808DAE72, 0xCFE52572, 0x7F0501ED
);

const int CURVE_B = 4;
#  elif EXHAUSTIVE_TEST_ORDER == 13
const secp256k1_ge secp256k1_ge_const_g = SECP256K1_GE_CONST(
    0xedc60018, 0xa51a786b, 0x2ea91f4d, 0x4c9416c0,
    0x9de54c3b, 0xa1316554, 0x6cf4345c, 0x7277ef15,
    0x54cb1b6b, 0xdc8c1273, 0x087844ea, 0x43f4603e,
    0x0eaf9a43, 0xf6effe55, 0x939f806d, 0x37adf8ac
);
const int CURVE_B = 2;
#  else
#    error No known generator for the specified exhaustive test group order.
#  endif
#else
/*secp256k1的生成器，中定义的值“g”
 *“高效密码术标准”（第2节）2.7.1。
 **/

static const secp256k1_ge secp256k1_ge_const_g = SECP256K1_GE_CONST(
    0x79BE667EUL, 0xF9DCBBACUL, 0x55A06295UL, 0xCE870B07UL,
    0x029BFCDBUL, 0x2DCE28D9UL, 0x59F2815BUL, 0x16F81798UL,
    0x483ADA77UL, 0x26A3C465UL, 0x5DA4FBFCUL, 0x0E1108A8UL,
    0xFD17B448UL, 0xA6855419UL, 0x9C47D08FUL, 0xFB10D4B8UL
);

const int CURVE_B = 7;
#endif

static void secp256k1_ge_set_gej_zinv(secp256k1_ge *r, const secp256k1_gej *a, const secp256k1_fe *zi) {
    secp256k1_fe zi2;
    secp256k1_fe zi3;
    secp256k1_fe_sqr(&zi2, zi);
    secp256k1_fe_mul(&zi3, &zi2, zi);
    secp256k1_fe_mul(&r->x, &a->x, &zi2);
    secp256k1_fe_mul(&r->y, &a->y, &zi3);
    r->infinity = a->infinity;
}

static void secp256k1_ge_set_xy(secp256k1_ge *r, const secp256k1_fe *x, const secp256k1_fe *y) {
    r->infinity = 0;
    r->x = *x;
    r->y = *y;
}

static int secp256k1_ge_is_infinity(const secp256k1_ge *a) {
    return a->infinity;
}

static void secp256k1_ge_neg(secp256k1_ge *r, const secp256k1_ge *a) {
    *r = *a;
    secp256k1_fe_normalize_weak(&r->y);
    secp256k1_fe_negate(&r->y, &r->y, 1);
}

static void secp256k1_ge_set_gej(secp256k1_ge *r, secp256k1_gej *a) {
    secp256k1_fe z2, z3;
    r->infinity = a->infinity;
    secp256k1_fe_inv(&a->z, &a->z);
    secp256k1_fe_sqr(&z2, &a->z);
    secp256k1_fe_mul(&z3, &a->z, &z2);
    secp256k1_fe_mul(&a->x, &a->x, &z2);
    secp256k1_fe_mul(&a->y, &a->y, &z3);
    secp256k1_fe_set_int(&a->z, 1);
    r->x = a->x;
    r->y = a->y;
}

static void secp256k1_ge_set_gej_var(secp256k1_ge *r, secp256k1_gej *a) {
    secp256k1_fe z2, z3;
    r->infinity = a->infinity;
    if (a->infinity) {
        return;
    }
    secp256k1_fe_inv_var(&a->z, &a->z);
    secp256k1_fe_sqr(&z2, &a->z);
    secp256k1_fe_mul(&z3, &a->z, &z2);
    secp256k1_fe_mul(&a->x, &a->x, &z2);
    secp256k1_fe_mul(&a->y, &a->y, &z3);
    secp256k1_fe_set_int(&a->z, 1);
    r->x = a->x;
    r->y = a->y;
}

static void secp256k1_ge_set_all_gej_var(secp256k1_ge *r, const secp256k1_gej *a, size_t len, const secp256k1_callback *cb) {
    secp256k1_fe *az;
    secp256k1_fe *azi;
    size_t i;
    size_t count = 0;
    az = (secp256k1_fe *)checked_malloc(cb, sizeof(secp256k1_fe) * len);
    for (i = 0; i < len; i++) {
        if (!a[i].infinity) {
            az[count++] = a[i].z;
        }
    }

    azi = (secp256k1_fe *)checked_malloc(cb, sizeof(secp256k1_fe) * count);
    secp256k1_fe_inv_all_var(azi, az, count);
    free(az);

    count = 0;
    for (i = 0; i < len; i++) {
        r[i].infinity = a[i].infinity;
        if (!a[i].infinity) {
            secp256k1_ge_set_gej_zinv(&r[i], &a[i], &azi[count++]);
        }
    }
    free(azi);
}

static void secp256k1_ge_set_table_gej_var(secp256k1_ge *r, const secp256k1_gej *a, const secp256k1_fe *zr, size_t len) {
    size_t i = len - 1;
    secp256k1_fe zi;

    if (len > 0) {
        /*计算最后一个z坐标的倒数，并用它来计算最后一个仿射输出。*/
        secp256k1_fe_inv(&zi, &a[i].z);
        secp256k1_ge_set_gej_zinv(&r[i], &a[i], &zi);

        /*向后计算，使用z比缩放x/y值。*/
        while (i > 0) {
            secp256k1_fe_mul(&zi, &zi, &zr[i]);
            i--;
            secp256k1_ge_set_gej_zinv(&r[i], &a[i], &zi);
        }
    }
}

static void secp256k1_ge_globalz_set_table_gej(size_t len, secp256k1_ge *r, secp256k1_fe *globalz, const secp256k1_gej *a, const secp256k1_fe *zr) {
    size_t i = len - 1;
    secp256k1_fe zs;

    if (len > 0) {
        /*最后一点的z给了我们表的“全局z”。*/
        r[i].x = a[i].x;
        r[i].y = a[i].y;
        *globalz = a[i].z;
        r[i].infinity = 0;
        zs = zr[i];

        /*用z-比率来缩放x/y值，这样我们就可以向后工作了。*/
        while (i > 0) {
            if (i != len - 1) {
                secp256k1_fe_mul(&zs, &zs, &zr[i]);
            }
            i--;
            secp256k1_ge_set_gej_zinv(&r[i], &a[i], &zs);
        }
    }
}

static void secp256k1_gej_set_infinity(secp256k1_gej *r) {
    r->infinity = 1;
    secp256k1_fe_clear(&r->x);
    secp256k1_fe_clear(&r->y);
    secp256k1_fe_clear(&r->z);
}

static void secp256k1_gej_clear(secp256k1_gej *r) {
    r->infinity = 0;
    secp256k1_fe_clear(&r->x);
    secp256k1_fe_clear(&r->y);
    secp256k1_fe_clear(&r->z);
}

static void secp256k1_ge_clear(secp256k1_ge *r) {
    r->infinity = 0;
    secp256k1_fe_clear(&r->x);
    secp256k1_fe_clear(&r->y);
}

static int secp256k1_ge_set_xquad(secp256k1_ge *r, const secp256k1_fe *x) {
    secp256k1_fe x2, x3, c;
    r->x = *x;
    secp256k1_fe_sqr(&x2, x);
    secp256k1_fe_mul(&x3, x, &x2);
    r->infinity = 0;
    secp256k1_fe_set_int(&c, CURVE_B);
    secp256k1_fe_add(&c, &x3);
    return secp256k1_fe_sqrt(&r->y, &c);
}

static int secp256k1_ge_set_xo_var(secp256k1_ge *r, const secp256k1_fe *x, int odd) {
    if (!secp256k1_ge_set_xquad(r, x)) {
        return 0;
    }
    secp256k1_fe_normalize_var(&r->y);
    if (secp256k1_fe_is_odd(&r->y) != odd) {
        secp256k1_fe_negate(&r->y, &r->y, 1);
    }
    return 1;

}

static void secp256k1_gej_set_ge(secp256k1_gej *r, const secp256k1_ge *a) {
   r->infinity = a->infinity;
   r->x = a->x;
   r->y = a->y;
   secp256k1_fe_set_int(&r->z, 1);
}

static int secp256k1_gej_eq_x_var(const secp256k1_fe *x, const secp256k1_gej *a) {
    secp256k1_fe r, r2;
    VERIFY_CHECK(!a->infinity);
    secp256k1_fe_sqr(&r, &a->z); secp256k1_fe_mul(&r, &r, x);
    r2 = a->x; secp256k1_fe_normalize_weak(&r2);
    return secp256k1_fe_equal_var(&r, &r2);
}

static void secp256k1_gej_neg(secp256k1_gej *r, const secp256k1_gej *a) {
    r->infinity = a->infinity;
    r->x = a->x;
    r->y = a->y;
    r->z = a->z;
    secp256k1_fe_normalize_weak(&r->y);
    secp256k1_fe_negate(&r->y, &r->y, 1);
}

static int secp256k1_gej_is_infinity(const secp256k1_gej *a) {
    return a->infinity;
}

static int secp256k1_gej_is_valid_var(const secp256k1_gej *a) {
    secp256k1_fe y2, x3, z2, z6;
    if (a->infinity) {
        return 0;
    }
    /*y^ 2＝x^ 3＋7
     *（y/z^3）^2=（x/z^2）^3+7
     *Y^2/Z^6=X^3/Z^6+7
     *y^2=x^3+7*z^6
     **/

    secp256k1_fe_sqr(&y2, &a->y);
    secp256k1_fe_sqr(&x3, &a->x); secp256k1_fe_mul(&x3, &x3, &a->x);
    secp256k1_fe_sqr(&z2, &a->z);
    secp256k1_fe_sqr(&z6, &z2); secp256k1_fe_mul(&z6, &z6, &z2);
    secp256k1_fe_mul_int(&z6, CURVE_B);
    secp256k1_fe_add(&x3, &z6);
    secp256k1_fe_normalize_weak(&x3);
    return secp256k1_fe_equal_var(&y2, &x3);
}

static int secp256k1_ge_is_valid_var(const secp256k1_ge *a) {
    secp256k1_fe y2, x3, c;
    if (a->infinity) {
        return 0;
    }
    /*y^ 2＝x^ 3＋7*/
    secp256k1_fe_sqr(&y2, &a->y);
    secp256k1_fe_sqr(&x3, &a->x); secp256k1_fe_mul(&x3, &x3, &a->x);
    secp256k1_fe_set_int(&c, CURVE_B);
    secp256k1_fe_add(&x3, &c);
    secp256k1_fe_normalize_weak(&x3);
    return secp256k1_fe_equal_var(&y2, &x3);
}

static void secp256k1_gej_double_var(secp256k1_gej *r, const secp256k1_gej *a, secp256k1_fe *rzr) {
    /*操作：3 mul，4 sqr，0 normalize，12 mul int/add/negate。
     *
     *请注意，在
     *https://hyper椭圆形.org/efd/g1p/auto-shortw-jacobian-0.html doubling-dbl-2009-l
     *用一个乘法换一个正方形，但实际上这个过程比较慢，
     *主要是因为它需要更多的规范化。
     **/

    secp256k1_fe t1,t2,t3,t4;
    /*对于secp256k1，当且仅当q为无穷大时，2q为无穷大。这是因为如果2q=无穷大，
     *q必须等于-q，或q.y==-（q.y），或q.y为0。对于y^2=x^3+7上的点
     *y=0，x^3必须是-7 mod p。但是，-7没有cube root mod p。
     *
     *这样说之后，如果这个功能在性扭曲上得到一个点，例如
     *故障攻击，Y可能为0。这种情况发生在y^2=x^3+6，
     *由于-6没有多维数据集根mod p，因此此函数不会设置
     *即使点加倍到无穷大，无穷大标志和结果
     *点是乱七八糟的（z=0，但无穷大=0）。
     **/

    r->infinity = a->infinity;
    if (r->infinity) {
        if (rzr != NULL) {
            secp256k1_fe_set_int(rzr, 1);
        }
        return;
    }

    if (rzr != NULL) {
        *rzr = a->y;
        secp256k1_fe_normalize_weak(rzr);
        secp256k1_fe_mul_int(rzr, 2);
    }

    secp256k1_fe_mul(&r->z, &a->z, &a->y);
    /*p256k1_fe_mul_int（&r->z，2）；/*z'=2*y*z（2）*/
    secp256k1_fe_sqr（&t1，&a->x）；
    secp256k1_fe_mul_int（&t1，3）；/*t1=3*x^2（3）*/

    /*p256k1_fe_sqr（&t2，&t1）；/*t2=9*x^4（1）*/
    secp256k1_fe_sqr（&t3，&a->y）；
    secp256k1_fe_mul_int（&t3，2）；/*t3=2*y^2（2）*/

    secp256k1_fe_sqr(&t4, &t3);
    /*p256k1-fe-mul-int（&t4，2）；/*t4=8*y^4（2）*/
    secp256k1_fe_mul（&t3，&t3，&a->x）；/*t3=2*x*y^2（1）*/

    r->x = t3;
    /*p256k1_fe_mul_int（&r->x，4）；/*x'=8*x*y^2（4）*/
    secp256k1_fe_negate（&r->x，&r->x，4）；/*x'=-8*x*y^2（5）*/

    /*p256k1_fe_add（&r->x，&t2）；/*x'=9*x^4-8*x*y^2（6）*/
    secp256k1_fe_negate（&t2，&t2，1）；/*t2=-9*x^4（2）*/

    /*p256k1_fe_mul_int（&t3，6）；/*t3=12*x*y^2（6）*/
    secp256k1_fe_add（&t3，&t2）；/*t3=12*x*y^2-9*x^4（8）*/

    /*p256k1-fe-mul（&r->y，&t1，&t3）；/*y'=36*x^3*y^2-27*x^6（1）*/
    secp256k1_fe_negate（&t2，&t4，2）；/*t2=-8*y^4（3）*/

    /*p256k1_fe_add（&r->y，&t2）；/*y'=36*x^3*y^2-27*x^6-8*y^4（4）*/
}

静态secp256k1_inline void secp256k1_gej_double_nonzero（secp256k1_gej*r，const secp256k1_gej*a，secp256k1_fe*rzr）
    验证！secp256k1_gej_是_无穷大（a））；
    secp256k1_gej_double_var（r，a，rzr）；
}

静态空隙secp256k1_gej_add_var（secp256k1_gej*r，const secp256k1_gej*a，const secp256k1_gej*b，secp256k1_fe*rzr）
    /*操作：12 mul，4 sqr，2 normalize，12 mul int/add/negate*/

    secp256k1_fe z22, z12, u1, u2, s1, s2, h, i, i2, h2, h3, t;

    if (a->infinity) {
        VERIFY_CHECK(rzr == NULL);
        *r = *b;
        return;
    }

    if (b->infinity) {
        if (rzr != NULL) {
            secp256k1_fe_set_int(rzr, 1);
        }
        *r = *a;
        return;
    }

    r->infinity = 0;
    secp256k1_fe_sqr(&z22, &b->z);
    secp256k1_fe_sqr(&z12, &a->z);
    secp256k1_fe_mul(&u1, &a->x, &z22);
    secp256k1_fe_mul(&u2, &b->x, &z12);
    secp256k1_fe_mul(&s1, &a->y, &z22); secp256k1_fe_mul(&s1, &s1, &b->z);
    secp256k1_fe_mul(&s2, &b->y, &z12); secp256k1_fe_mul(&s2, &s2, &a->z);
    secp256k1_fe_negate(&h, &u1, 1); secp256k1_fe_add(&h, &u2);
    secp256k1_fe_negate(&i, &s1, 1); secp256k1_fe_add(&i, &s2);
    if (secp256k1_fe_normalizes_to_zero_var(&h)) {
        if (secp256k1_fe_normalizes_to_zero_var(&i)) {
            secp256k1_gej_double_var(r, a, rzr);
        } else {
            if (rzr != NULL) {
                secp256k1_fe_set_int(rzr, 0);
            }
            r->infinity = 1;
        }
        return;
    }
    secp256k1_fe_sqr(&i2, &i);
    secp256k1_fe_sqr(&h2, &h);
    secp256k1_fe_mul(&h3, &h, &h2);
    secp256k1_fe_mul(&h, &h, &b->z);
    if (rzr != NULL) {
        *rzr = h;
    }
    secp256k1_fe_mul(&r->z, &a->z, &h);
    secp256k1_fe_mul(&t, &u1, &h2);
    r->x = t; secp256k1_fe_mul_int(&r->x, 2); secp256k1_fe_add(&r->x, &h3); secp256k1_fe_negate(&r->x, &r->x, 3); secp256k1_fe_add(&r->x, &i2);
    secp256k1_fe_negate(&r->y, &r->x, 5); secp256k1_fe_add(&r->y, &t); secp256k1_fe_mul(&r->y, &r->y, &i);
    secp256k1_fe_mul(&h3, &h3, &s1); secp256k1_fe_negate(&h3, &h3, 1);
    secp256k1_fe_add(&r->y, &h3);
}

static void secp256k1_gej_add_ge_var(secp256k1_gej *r, const secp256k1_gej *a, const secp256k1_ge *b, secp256k1_fe *rzr) {
    /*8 mul，3 sqr，4 normalize，12 mul int/add/negate*/
    secp256k1_fe z12, u1, u2, s1, s2, h, i, i2, h2, h3, t;
    if (a->infinity) {
        VERIFY_CHECK(rzr == NULL);
        secp256k1_gej_set_ge(r, b);
        return;
    }
    if (b->infinity) {
        if (rzr != NULL) {
            secp256k1_fe_set_int(rzr, 1);
        }
        *r = *a;
        return;
    }
    r->infinity = 0;

    secp256k1_fe_sqr(&z12, &a->z);
    u1 = a->x; secp256k1_fe_normalize_weak(&u1);
    secp256k1_fe_mul(&u2, &b->x, &z12);
    s1 = a->y; secp256k1_fe_normalize_weak(&s1);
    secp256k1_fe_mul(&s2, &b->y, &z12); secp256k1_fe_mul(&s2, &s2, &a->z);
    secp256k1_fe_negate(&h, &u1, 1); secp256k1_fe_add(&h, &u2);
    secp256k1_fe_negate(&i, &s1, 1); secp256k1_fe_add(&i, &s2);
    if (secp256k1_fe_normalizes_to_zero_var(&h)) {
        if (secp256k1_fe_normalizes_to_zero_var(&i)) {
            secp256k1_gej_double_var(r, a, rzr);
        } else {
            if (rzr != NULL) {
                secp256k1_fe_set_int(rzr, 0);
            }
            r->infinity = 1;
        }
        return;
    }
    secp256k1_fe_sqr(&i2, &i);
    secp256k1_fe_sqr(&h2, &h);
    secp256k1_fe_mul(&h3, &h, &h2);
    if (rzr != NULL) {
        *rzr = h;
    }
    secp256k1_fe_mul(&r->z, &a->z, &h);
    secp256k1_fe_mul(&t, &u1, &h2);
    r->x = t; secp256k1_fe_mul_int(&r->x, 2); secp256k1_fe_add(&r->x, &h3); secp256k1_fe_negate(&r->x, &r->x, 3); secp256k1_fe_add(&r->x, &i2);
    secp256k1_fe_negate(&r->y, &r->x, 5); secp256k1_fe_add(&r->y, &t); secp256k1_fe_mul(&r->y, &r->y, &i);
    secp256k1_fe_mul(&h3, &h3, &s1); secp256k1_fe_negate(&h3, &h3, 1);
    secp256k1_fe_add(&r->y, &h3);
}

static void secp256k1_gej_add_zinv_var(secp256k1_gej *r, const secp256k1_gej *a, const secp256k1_ge *b, const secp256k1_fe *bzinv) {
    /*9 mul，3 sqr，4 normalize，12 mul int/add/negate*/
    secp256k1_fe az, z12, u1, u2, s1, s2, h, i, i2, h2, h3, t;

    if (b->infinity) {
        *r = *a;
        return;
    }
    if (a->infinity) {
        secp256k1_fe bzinv2, bzinv3;
        r->infinity = b->infinity;
        secp256k1_fe_sqr(&bzinv2, bzinv);
        secp256k1_fe_mul(&bzinv3, &bzinv2, bzinv);
        secp256k1_fe_mul(&r->x, &b->x, &bzinv2);
        secp256k1_fe_mul(&r->y, &b->y, &bzinv3);
        secp256k1_fe_set_int(&r->z, 1);
        return;
    }
    r->infinity = 0;

    /*我们需要计算（rx，ry，rz）=（ax，ay，az）+（bx，by，1/bzinv）。由于
     *secp256k1的同构我们可以将两边的z坐标相乘。
     *通过bzinv，得到：（rx，ry，rz*bzinv）=（ax，ay，az*bzinv）+（bx，by，1）。
     *这意味着（Rx、Ry、Rz）可以计算为
     *（ax，ay，az*bzinv）+（bx，by，1），当不将bzinv系数应用于rz时。
     *下面的变量a z保存a的修改后的z坐标，使用
     *用于计算Rx和Ry，但不用于Rz。
     **/

    secp256k1_fe_mul(&az, &a->z, bzinv);

    secp256k1_fe_sqr(&z12, &az);
    u1 = a->x; secp256k1_fe_normalize_weak(&u1);
    secp256k1_fe_mul(&u2, &b->x, &z12);
    s1 = a->y; secp256k1_fe_normalize_weak(&s1);
    secp256k1_fe_mul(&s2, &b->y, &z12); secp256k1_fe_mul(&s2, &s2, &az);
    secp256k1_fe_negate(&h, &u1, 1); secp256k1_fe_add(&h, &u2);
    secp256k1_fe_negate(&i, &s1, 1); secp256k1_fe_add(&i, &s2);
    if (secp256k1_fe_normalizes_to_zero_var(&h)) {
        if (secp256k1_fe_normalizes_to_zero_var(&i)) {
            secp256k1_gej_double_var(r, a, NULL);
        } else {
            r->infinity = 1;
        }
        return;
    }
    secp256k1_fe_sqr(&i2, &i);
    secp256k1_fe_sqr(&h2, &h);
    secp256k1_fe_mul(&h3, &h, &h2);
    r->z = a->z; secp256k1_fe_mul(&r->z, &r->z, &h);
    secp256k1_fe_mul(&t, &u1, &h2);
    r->x = t; secp256k1_fe_mul_int(&r->x, 2); secp256k1_fe_add(&r->x, &h3); secp256k1_fe_negate(&r->x, &r->x, 3); secp256k1_fe_add(&r->x, &i2);
    secp256k1_fe_negate(&r->y, &r->x, 5); secp256k1_fe_add(&r->y, &t); secp256k1_fe_mul(&r->y, &r->y, &i);
    secp256k1_fe_mul(&h3, &h3, &s1); secp256k1_fe_negate(&h3, &h3, 1);
    secp256k1_fe_add(&r->y, &h3);
}


static void secp256k1_gej_add_ge(secp256k1_gej *r, const secp256k1_gej *a, const secp256k1_ge *b) {
    /*操作：7 mul，5 sqr，4 normalize，21 mul_int/add/negate/cmov*/
    static const secp256k1_fe fe_1 = SECP256K1_FE_CONST(0, 0, 0, 0, 0, 0, 0, 1);
    secp256k1_fe zz, u1, u2, s1, s2, t, tt, m, n, q, rr;
    secp256k1_fe m_alt, rr_alt;
    int infinity, degenerate;
    VERIFY_CHECK(!b->infinity);
    VERIFY_CHECK(a->infinity == 0 || a->infinity == 1);

    /*在：
     *Eric Brier和Marc Joye，Weierstrass椭圆曲线和侧通道攻击。
     *在D.Naccache和P.Paillier编辑的《公开密钥密码学》中，计算机科学讲义第2274卷，第335-345页。Springer Verlag，2002年。
     *我们找到一个统一的加/加倍公式的解决方案：
     *lambda=（x1+x2）^2-x1*x2+a）/（y1+y2），对于secp256k1的曲线方程，a=0。
     *x3=lambda^2-（x1+x2）
     *2*Y3=lambda*（x1+x2-2*x3）-（y1+y2）。
     *
     *用Xi i＝Xi/Zi^ 2和Yi＝Yi/Zi^ 3代替i＝1，2，3，给出：
     *u1=x1*z2^2，u2=x2*z1^2
     *s1=y1*z2^3，s2=y2*z1^3
     *Z= Z1*Z2
     ＊t= u1+u2
     *m＝s1+s2
     *q= t*m ^ 2
     *r=t^2-u1*u2
     *x3=4*（r^2-q）
     *y3=4*（r*（3*q-2*r^2）-m^4）
     *Z3＝2×M*Z
     *（注意，本文使用XI= XI/ZI和Yi＝Yi/Zi）。
     *
     *此公式的优点是两种添加量相同。
     *明显的点和加倍。但是，它在
     *任一点为无穷大或y1=-y2的情况。我们处理
     *这些情况有以下几种：
     *
     *如果b是无穷大的，我们只需通过验证支票保释。
     *
     *-如果a是无穷大，我们会检测到它，并且在
     *计算替换结果（这将毫无意义，
     *但我们用b.x:b.y:1计算为常数。
     *
     *-如果a=-b，我们有y1=-y2，这是一个退化的情况。
     *但这里的答案是无穷大的，所以我们只需设置
     *结果的无穷大标志，覆盖计算值
     *甚至不需要CMOV。
     *
     *-如果y1=-y2但x1！=x2，这是由于
     *曲线的属性（具体来说，1具有非平凡的立方体
     *我们领域的根，曲线方程没有x系数）
     *那么答案不是无穷大，也不是由上面给出的。
     *方程式。在这种情况下，我们用cmov代替另一个表达式
     *对于lambda。具体来说（y1-y2）/（x1-x2）。这两个都在哪里
     *lambda的表达式是定义的，它们是相等的，并且可以是
     *相乘（y1+y2）/（y1+y2）得到
     *然后用x^3+7替换y^2（使用曲线方程）。
     *对于所有非零点对（a，b），至少定义一个，
     *所以这涵盖了一切。
     **/


    /*p256k1_fe_sqr（&z z，&a->z）；/*z=z1^2*/
    u1=a->x；secp256k1_fe_normalize_weak（&u1）；/*u1=u1=x1*z2^2（1）*/

    /*p256k1_fe_mul（&u2，&b->x，&zz）；/*u2=u2=x2*z1^2（1）*/
    s1=a->y；secp256k1_fe_normalize_weak（&s1）；/*s1=s1=y1*z2^3（1）*/

    /*p256k1_fe_mul（&s2，&b->y，&zz）；/*s2=y2*z1^2（1）*/
    secp256k1_fe_mul（&s2，&s2，&a->z）；/*s2=s2=y2*z1^3（1）*/

    /*u1；secp256k1_fe_add（&t，&u2）；/*t=t=u1+u2（2）*/
    m=s1；secp256k1_fe_add（&m，&s2）；/*m=m=s1+s2（2）*/

    /*p256k1_fe_sqr（&rr，&t）；/*rr=t^2（1）*/
    secp256k1_fe_negate（&m_alt，&u2，1）；/*m alt=-x2*z1^2*/

    /*p256k1_fe_mul（&tt，&u1，&m_alt）；/*tt=-u1*u2（2）*/
    secp256k1_fe_add（&r r，&t t）；/*r r=r=t^2-u1*u2（3）*/

    /*如果lambda=r/m=0/0，我们就有问题了（除了“琐碎的”
     *如果z=z1z2=0，这是后面的特殊情况）。*/

    degenerate = secp256k1_fe_normalizes_to_zero(&m) &
                 secp256k1_fe_normalizes_to_zero(&rr);
    /*只有当y1=-y2和x1^3==x2^3，但x1！= x2。
     *这意味着x1==beta*x2或beta*x1==x2，其中beta是
     *一个非平凡的立方根。在任何一种情况下，都可以选择
     *lambda的非不确定表达式为（y1-y2）/（x1-x2），
     *所以我们将r/m设置为这个值。*/

    rr_alt = s1;
    /*p256k1_fe_mul_int（&rr_alt，2）；/*rr=y1*z2^3-y2*z1^3（2）*/
    secp256k1_fe_add（&m_alt，&u1）；/*m alt=x1*z2^2-x2*z1^2*/


    secp256k1_fe_cmov(&rr_alt, &rr, !degenerate);
    secp256k1_fe_cmov(&m_alt, &m, !degenerate);
    /*现在，Ralt/Malt=lambda，并且保证不为0/0。
     *从这里开始，Ralt和Malt代表分子。
     *和lambda的分母；r和m表示显式
     *表达式x1^2+x2^2+x1 x2和y1+y2。*/

    /*p256k1_fe_sqr（&n，&m_alt）；/*n=麦芽^2（1）*/
    secp256k1_fe_mul（&q，&n，&t）；/*q=q=t*malt^2（1）*/

    /*这两条线的观测结果是m==malt或m==0，
     *所以m^3*malt是malt^4（通过平方计算），或者
     *零（由CMOV“计算”）。所以成本是一个平方
     *对两次乘法。*/

    secp256k1_fe_sqr(&n, &n);
    /*p256k1_fe_cmov（&n，&m，退化）；/*n=m^3*麦芽（2）*/
    secp256k1_fe_sqr（&t，&rr_alt）；/*t=ralt^2（1）*/

    /*p256k1-fe-mul（&r->z，&a->z，&m-alt）；/*r->z=m alt*z（1）*/
    无穷大=secp256k1_fe_将_标准化为_zero（&r->z）*（1-a->无穷大）；
    secp256k1_fe_mul_int（&r->z，2）；/*r->z=z3=2*malt*z（2）*/

    /*p256k1_fe_negate（&q，&q，1）；/*q=-q（2）*/
    secp256k1_fe_add（&t，&q）；/*t=ralt^2-q（3）*/

    secp256k1_fe_normalize_weak(&t);
    /*x=t；/*r->x=ralt^2-q（1）*/
    secp256k1_fe_mul_int（&t，2）；/*t=2*x3（2）*/

    /*p256k1_fe_add（&t，&q）；/*t=2*x3-q:（4）*/
    secp256k1_fe_mul（&t，&t，&rr_alt）；/*t=ralt*（2*x3-q）（1）*/

    /*p256k1_fe_add（&t，&n）；/*t=ralt*（2*x3-q）+m^3*麦芽（3）*/
    secp256k1_fe_negate（&r->y，&t，3）；/*r->y=ralt*（q-2x3）-m^3*麦芽（4）*/

    secp256k1_fe_normalize_weak(&r->y);
    /*p256k1_fe_mul_int（&r->x，4）；/*r->x=x3=4*（ralt^2-q）*/
    secp256k1_fe_mul_int（&r->y，4）；/*r->y=y3=4*ralt*（q-2x3）-4*m^3*malt（4）*/


    /*如果a->infinity==1，则将r替换为（b->x，b->y，1）。*/
    secp256k1_fe_cmov(&r->x, &b->x, a->infinity);
    secp256k1_fe_cmov(&r->y, &b->y, a->infinity);
    secp256k1_fe_cmov(&r->z, &fe_1, a->infinity);
    r->infinity = infinity;
}

static void secp256k1_gej_rescale(secp256k1_gej *r, const secp256k1_fe *s) {
    /*操作：4个MUL，1个SQR*/
    secp256k1_fe zz;
    VERIFY_CHECK(!secp256k1_fe_is_zero(s));
    secp256k1_fe_sqr(&zz, s);
    /*p256k1_fe_mul（&r->x，&r->x，&zz）；/*r->x*=s^2*/
    secp256k1_fe_mul（&r->y，&r->y，&zz）；
    secp256k1_fe_Mul（&r->y，&r->y，s）；/*r->y*=s^3*/

    /*p256k1_fe_mul（&r->z，&r->z，s）；/*r->z*=s*/
}

静态空隙secp256k1_ge_to_storage（secp256k1_ge_storage*r，const secp256k1_ge*a）
    secp256k1_fe x，y；
    验证！a-无穷大；
    x= a-＞x；
    secp256k1_fe_正火（&x）；
    y＝a-＞y；
    secp256k1_fe_正火（&y）；
    secp256k1_fe_to_storage（&r->x，&x）；
    secp256k1_fe_to_storage（&r->y，&y）；
}

静态空隙secp256k1_ge_从_存储（secp256k1_ge*r，const secp256k1_ge_存储*a）
    secp256k1_fe_从_存储（&r->x，&a->x）；
    secp256k1_fe_从_存储（&r->y，&a->y）；
    r->无穷大=0；
}

静态secp256k1_inline void secp256k1_ge_storage_cmov（secp256k1_ge_storage*r，const secp256k1_ge_storage*a，int flag）
    secp256k1_fe_storage_cmov（&r->x，&a->x，flag）；
    secp256k1_fe_storage_cmov（&r->y，&a->y，flag）；
}

使用自同态
静态空隙secp256k1_ge_mul_lambda（secp256k1_ge*r，const secp256k1_ge*a）
    静态常量secp256k1_fe beta=secp256k1_fe_const（
        0x7ae96a2bul，0x657c071ul，0x6e64479eul，0xac3434e9ul，
        0x9CF04975ul、0x12F58995ul、0xC1396C28ul、0x719501eul
    ；
    ＊r=＊a；
    secp256k1_fe_mul（&r->x，&r->x，&beta）；
}
第二节

静态int secp256k1_gej_has_quad_y_var（const secp256k1_gej*a）
    secp256k1_fe yz；

    如果（A->Infinity）
        返回0；
    }

    /*我们认为1/a->z^3的雅可比符号与
     *a->z，因此a->y/a->z^3是二次余数iff a->y*a->z
       是*/

    secp256k1_fe_mul(&yz, &a->y, &a->z);
    return secp256k1_fe_is_quad_var(&yz);
}

/*dif/*secp256k1_group_impl_h*/
