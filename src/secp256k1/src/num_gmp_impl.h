
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


#ifndef SECP256K1_NUM_REPR_IMPL_H
#define SECP256K1_NUM_REPR_IMPL_H

#include <string.h>
#include <stdlib.h>
#include <gmp.h>

#include "util.h"
#include "num.h"

#ifdef VERIFY
static void secp256k1_num_sanity(const secp256k1_num *a) {
    VERIFY_CHECK(a->limbs == 1 || (a->limbs > 1 && a->data[a->limbs-1] != 0));
}
#else
#define secp256k1_num_sanity(a) do { } while(0)
#endif

static void secp256k1_num_copy(secp256k1_num *r, const secp256k1_num *a) {
    *r = *a;
}

static void secp256k1_num_get_bin(unsigned char *r, unsigned int rlen, const secp256k1_num *a) {
    unsigned char tmp[65];
    int len = 0;
    int shift = 0;
    if (a->limbs>1 || a->data[0] != 0) {
        len = mpn_get_str(tmp, 256, (mp_limb_t*)a->data, a->limbs);
    }
    while (shift < len && tmp[shift] == 0) shift++;
    VERIFY_CHECK(len-shift <= (int)rlen);
    memset(r, 0, rlen - len + shift);
    if (len > shift) {
        memcpy(r + rlen - len + shift, tmp + shift, len - shift);
    }
    memset(tmp, 0, sizeof(tmp));
}

static void secp256k1_num_set_bin(secp256k1_num *r, const unsigned char *a, unsigned int alen) {
    int len;
    VERIFY_CHECK(alen > 0);
    VERIFY_CHECK(alen <= 64);
    len = mpn_set_str(r->data, a, alen, 256);
    if (len == 0) {
        r->data[0] = 0;
        len = 1;
    }
    VERIFY_CHECK(len <= NUM_LIMBS*2);
    r->limbs = len;
    r->neg = 0;
    while (r->limbs > 1 && r->data[r->limbs-1]==0) {
        r->limbs--;
    }
}

static void secp256k1_num_add_abs(secp256k1_num *r, const secp256k1_num *a, const secp256k1_num *b) {
    mp_limb_t c = mpn_add(r->data, a->data, a->limbs, b->data, b->limbs);
    r->limbs = a->limbs;
    if (c != 0) {
        VERIFY_CHECK(r->limbs < 2*NUM_LIMBS);
        r->data[r->limbs++] = c;
    }
}

static void secp256k1_num_sub_abs(secp256k1_num *r, const secp256k1_num *a, const secp256k1_num *b) {
    mp_limb_t c = mpn_sub(r->data, a->data, a->limbs, b->data, b->limbs);
    (void)c;
    VERIFY_CHECK(c == 0);
    r->limbs = a->limbs;
    while (r->limbs > 1 && r->data[r->limbs-1]==0) {
        r->limbs--;
    }
}

static void secp256k1_num_mod(secp256k1_num *r, const secp256k1_num *m) {
    secp256k1_num_sanity(r);
    secp256k1_num_sanity(m);

    if (r->limbs >= m->limbs) {
        mp_limb_t t[2*NUM_LIMBS];
        mpn_tdiv_qr(t, r->data, 0, r->data, r->limbs, m->data, m->limbs);
        memset(t, 0, sizeof(t));
        r->limbs = m->limbs;
        while (r->limbs > 1 && r->data[r->limbs-1]==0) {
            r->limbs--;
        }
    }

    if (r->neg && (r->limbs > 1 || r->data[0] != 0)) {
        secp256k1_num_sub_abs(r, m, r);
        r->neg = 0;
    }
}

static void secp256k1_num_mod_inverse(secp256k1_num *r, const secp256k1_num *a, const secp256k1_num *m) {
    int i;
    mp_limb_t g[NUM_LIMBS+1];
    mp_limb_t u[NUM_LIMBS+1];
    mp_limb_t v[NUM_LIMBS+1];
    mp_size_t sn;
    mp_size_t gn;
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(m);

    /*mpn_gindext计算：（g，s）=gindext（u，v），其中
     **g=gcd（u，v）
     **g=u*s+v*t
     **u的四肢大于等于v，v没有填充物
     *如果我们将u设置为（填充版本）a，v=m：
     *g=a*s+m*t
     *G=A*S M型
     *假设g=1：
     *S=1/A M型
     **/

    VERIFY_CHECK(m->limbs <= NUM_LIMBS);
    VERIFY_CHECK(m->data[m->limbs-1] != 0);
    for (i = 0; i < m->limbs; i++) {
        u[i] = (i < a->limbs) ? a->data[i] : 0;
        v[i] = m->data[i];
    }
    sn = NUM_LIMBS+1;
    gn = mpn_gcdext(g, r->data, &sn, u, m->limbs, v, m->limbs);
    (void)gn;
    VERIFY_CHECK(gn == 1);
    VERIFY_CHECK(g[0] == 1);
    r->neg = a->neg ^ m->neg;
    if (sn < 0) {
        mpn_sub(r->data, m->data, m->limbs, r->data, -sn);
        r->limbs = m->limbs;
        while (r->limbs > 1 && r->data[r->limbs-1]==0) {
            r->limbs--;
        }
    } else {
        r->limbs = sn;
    }
    memset(g, 0, sizeof(g));
    memset(u, 0, sizeof(u));
    memset(v, 0, sizeof(v));
}

static int secp256k1_num_jacobi(const secp256k1_num *a, const secp256k1_num *b) {
    int ret;
    mpz_t ga, gb;
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(b);
    VERIFY_CHECK(!b->neg && (b->limbs > 0) && (b->data[0] & 1));

    mpz_inits(ga, gb, NULL);

    mpz_import(gb, b->limbs, -1, sizeof(mp_limb_t), 0, 0, b->data);
    mpz_import(ga, a->limbs, -1, sizeof(mp_limb_t), 0, 0, a->data);
    if (a->neg) {
        mpz_neg(ga, ga);
    }

    ret = mpz_jacobi(ga, gb);

    mpz_clears(ga, gb, NULL);

    return ret;
}

static int secp256k1_num_is_one(const secp256k1_num *a) {
    return (a->limbs == 1 && a->data[0] == 1);
}

static int secp256k1_num_is_zero(const secp256k1_num *a) {
    return (a->limbs == 1 && a->data[0] == 0);
}

static int secp256k1_num_is_neg(const secp256k1_num *a) {
    return (a->limbs > 1 || a->data[0] != 0) && a->neg;
}

static int secp256k1_num_cmp(const secp256k1_num *a, const secp256k1_num *b) {
    if (a->limbs > b->limbs) {
        return 1;
    }
    if (a->limbs < b->limbs) {
        return -1;
    }
    return mpn_cmp(a->data, b->data, a->limbs);
}

static int secp256k1_num_eq(const secp256k1_num *a, const secp256k1_num *b) {
    if (a->limbs > b->limbs) {
        return 0;
    }
    if (a->limbs < b->limbs) {
        return 0;
    }
    if ((a->neg && !secp256k1_num_is_zero(a)) != (b->neg && !secp256k1_num_is_zero(b))) {
        return 0;
    }
    return mpn_cmp(a->data, b->data, a->limbs) == 0;
}

static void secp256k1_num_subadd(secp256k1_num *r, const secp256k1_num *a, const secp256k1_num *b, int bneg) {
    /*（！（b->neg^b neg^a->neg））/*a和b具有相同的符号*/
        r->neg=a->neg；
        如果（a->四肢>=b->四肢）
            secp256k1_num_add_abs（R，A，B）；
        }否则{
            secp256k1_num_add_abs（r，b，a）；
        }
    }否则{
        如果（secp256k1_num_cmp（a，b）>0）
            r->neg=a->neg；
            secp256k1_num_sub_abs（R，A，B）；
        }否则{
            r->neg=b->neg^b neg；
            secp256k1_num_sub_abs（R，B，A）；
        }
    }
}

静态void secp256k1_num_add（secp256k1_num*r，const secp256k1_num*a，const secp256k1_num*b）
    secp256k1_num_健全性（a）；
    secp256k1_num_健全性（b）；
    secp256k1_num_subdd（r，a，b，0）；
}

静态void secp256k1_num_sub（secp256k1_num*r，const secp256k1_num*a，const secp256k1_num*b）
    secp256k1_num_健全性（a）；
    secp256k1_num_健全性（b）；
    secp256k1_num_subdd（r，a，b，1）；
}

静态void secp256k1_num_mul（secp256k1_num*r，const secp256k1_num*a，const secp256k1_num*b）
    mp_-limb_t t mp[2*num_-limb+1]；
    secp256k1_num_健全性（a）；
    secp256k1_num_健全性（b）；

    验证_check（a->四肢+b->四肢<=2*num_四肢+1）；
    如果（（a->lemb==1&&a->data[0]==0）（b->lemb==1&&b->data[0]==0））；
        r＞肢体＝1；
        R>＞NEG＝0；
        R-＞数据〔0〕＝0；
        返回；
    }
    如果（a->四肢>=b->四肢）
        mpn_mul（tmp，a->数据，a->四肢，b->数据，b->四肢）；
    }否则{
        mpn_mul（tmp，b->数据，b->四肢，a->数据，a->四肢）；
    }
    R->四肢=A->四肢+B->四肢；
    如果（r->lemps>1&&tmp[r->lemps-1]==0）
        R-四肢；
    }
    验证_check（r->四肢<=2*num_四肢）；
    mpn_copyi（r->数据，tmp，r->肢体）；
    r->neg=a->neg^b->neg；
    memset（tmp，0，sizeof（tmp））；
}

静态void secp256k1_num_shift（secp256k1_num*r，int位）
    if（位%gmp_numb_位）
        /*在四肢内移动。*/

        mpn_rshift(r->data, r->data, r->limbs, bits % GMP_NUMB_BITS);
    }
    if (bits >= GMP_NUMB_BITS) {
        int i;
        /*移动四肢。*/
        for (i = 0; i < r->limbs; i++) {
            int index = i + (bits / GMP_NUMB_BITS);
            if (index < r->limbs && index < 2*NUM_LIMBS) {
                r->data[i] = r->data[index];
            } else {
                r->data[i] = 0;
            }
        }
    }
    while (r->limbs>1 && r->data[r->limbs-1]==0) {
        r->limbs--;
    }
}

static void secp256k1_num_negate(secp256k1_num *r) {
    r->neg ^= 1;
}

/*dif/*secp256k1_num_repr_impl_h*/
