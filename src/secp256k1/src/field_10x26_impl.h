
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


#ifndef SECP256K1_FIELD_REPR_IMPL_H
#define SECP256K1_FIELD_REPR_IMPL_H

#include "util.h"
#include "num.h"
#include "field.h"

#ifdef VERIFY
static void secp256k1_fe_verify(const secp256k1_fe *a) {
    const uint32_t *d = a->n;
    int m = a->normalized ? 1 : 2 * a->magnitude, r = 1;
    r &= (d[0] <= 0x3FFFFFFUL * m);
    r &= (d[1] <= 0x3FFFFFFUL * m);
    r &= (d[2] <= 0x3FFFFFFUL * m);
    r &= (d[3] <= 0x3FFFFFFUL * m);
    r &= (d[4] <= 0x3FFFFFFUL * m);
    r &= (d[5] <= 0x3FFFFFFUL * m);
    r &= (d[6] <= 0x3FFFFFFUL * m);
    r &= (d[7] <= 0x3FFFFFFUL * m);
    r &= (d[8] <= 0x3FFFFFFUL * m);
    r &= (d[9] <= 0x03FFFFFUL * m);
    r &= (a->magnitude >= 0);
    r &= (a->magnitude <= 32);
    if (a->normalized) {
        r &= (a->magnitude <= 1);
        if (r && (d[9] == 0x03FFFFFUL)) {
            uint32_t mid = d[8] & d[7] & d[6] & d[5] & d[4] & d[3] & d[2];
            if (mid == 0x3FFFFFFUL) {
                r &= ((d[1] + 0x40UL + ((d[0] + 0x3D1UL) >> 26)) <= 0x3FFFFFFUL);
            }
        }
    }
    VERIFY_CHECK(r == 1);
}
#endif

static void secp256k1_fe_normalize(secp256k1_fe *r) {
    uint32_t t0 = r->n[0], t1 = r->n[1], t2 = r->n[2], t3 = r->n[3], t4 = r->n[4],
             t5 = r->n[5], t6 = r->n[6], t7 = r->n[7], t8 = r->n[8], t9 = r->n[9];

    /*开始时减少t9，这样从第一次通过最多只能有一个进位*/
    uint32_t m;
    uint32_t x = t9 >> 22; t9 &= 0x03FFFFFUL;

    /*第一遍确保震级为1，…*/
    t0 += x * 0x3D1UL; t1 += (x << 6);
    t1 += (t0 >> 26); t0 &= 0x3FFFFFFUL;
    t2 += (t1 >> 26); t1 &= 0x3FFFFFFUL;
    t3 += (t2 >> 26); t2 &= 0x3FFFFFFUL; m = t2;
    t4 += (t3 >> 26); t3 &= 0x3FFFFFFUL; m &= t3;
    t5 += (t4 >> 26); t4 &= 0x3FFFFFFUL; m &= t4;
    t6 += (t5 >> 26); t5 &= 0x3FFFFFFUL; m &= t5;
    t7 += (t6 >> 26); t6 &= 0x3FFFFFFUL; m &= t6;
    t8 += (t7 >> 26); t7 &= 0x3FFFFFFUL; m &= t7;
    t9 += (t8 >> 26); t8 &= 0x3FFFFFFUL; m &= t8;

    /*…除了T9的22位可能的进位（即字段元素的256位）*/
    VERIFY_CHECK(t9 >> 23 == 0);

    /*最多需要进行一次最终减速；检查该值是否大于等于现场特性*/
    x = (t9 >> 22) | ((t9 == 0x03FFFFFUL) & (m == 0x3FFFFFFUL)
        & ((t1 + 0x40UL + ((t0 + 0x3D1UL) >> 26)) > 0x3FFFFFFUL));

    /*应用最终还原（对于恒定时间行为，我们总是这样做）*/
    t0 += x * 0x3D1UL; t1 += (x << 6);
    t1 += (t0 >> 26); t0 &= 0x3FFFFFFUL;
    t2 += (t1 >> 26); t1 &= 0x3FFFFFFUL;
    t3 += (t2 >> 26); t2 &= 0x3FFFFFFUL;
    t4 += (t3 >> 26); t3 &= 0x3FFFFFFUL;
    t5 += (t4 >> 26); t4 &= 0x3FFFFFFUL;
    t6 += (t5 >> 26); t5 &= 0x3FFFFFFUL;
    t7 += (t6 >> 26); t6 &= 0x3FFFFFFUL;
    t8 += (t7 >> 26); t7 &= 0x3FFFFFFUL;
    t9 += (t8 >> 26); t8 &= 0x3FFFFFFUL;

    /*如果t9还没有进位到第22位，那么它应该在任何最终减速之后*/
    VERIFY_CHECK(t9 >> 22 == x);

    /*从最终缩减值中屏蔽2^256的可能倍数*/
    t9 &= 0x03FFFFFUL;

    r->n[0] = t0; r->n[1] = t1; r->n[2] = t2; r->n[3] = t3; r->n[4] = t4;
    r->n[5] = t5; r->n[6] = t6; r->n[7] = t7; r->n[8] = t8; r->n[9] = t9;

#ifdef VERIFY
    r->magnitude = 1;
    r->normalized = 1;
    secp256k1_fe_verify(r);
#endif
}

static void secp256k1_fe_normalize_weak(secp256k1_fe *r) {
    uint32_t t0 = r->n[0], t1 = r->n[1], t2 = r->n[2], t3 = r->n[3], t4 = r->n[4],
             t5 = r->n[5], t6 = r->n[6], t7 = r->n[7], t8 = r->n[8], t9 = r->n[9];

    /*开始时减少t9，这样从第一次通过最多只能有一个进位*/
    uint32_t x = t9 >> 22; t9 &= 0x03FFFFFUL;

    /*第一遍确保震级为1，…*/
    t0 += x * 0x3D1UL; t1 += (x << 6);
    t1 += (t0 >> 26); t0 &= 0x3FFFFFFUL;
    t2 += (t1 >> 26); t1 &= 0x3FFFFFFUL;
    t3 += (t2 >> 26); t2 &= 0x3FFFFFFUL;
    t4 += (t3 >> 26); t3 &= 0x3FFFFFFUL;
    t5 += (t4 >> 26); t4 &= 0x3FFFFFFUL;
    t6 += (t5 >> 26); t5 &= 0x3FFFFFFUL;
    t7 += (t6 >> 26); t6 &= 0x3FFFFFFUL;
    t8 += (t7 >> 26); t7 &= 0x3FFFFFFUL;
    t9 += (t8 >> 26); t8 &= 0x3FFFFFFUL;

    /*…除了T9的22位可能的进位（即字段元素的256位）*/
    VERIFY_CHECK(t9 >> 23 == 0);

    r->n[0] = t0; r->n[1] = t1; r->n[2] = t2; r->n[3] = t3; r->n[4] = t4;
    r->n[5] = t5; r->n[6] = t6; r->n[7] = t7; r->n[8] = t8; r->n[9] = t9;

#ifdef VERIFY
    r->magnitude = 1;
    secp256k1_fe_verify(r);
#endif
}

static void secp256k1_fe_normalize_var(secp256k1_fe *r) {
    uint32_t t0 = r->n[0], t1 = r->n[1], t2 = r->n[2], t3 = r->n[3], t4 = r->n[4],
             t5 = r->n[5], t6 = r->n[6], t7 = r->n[7], t8 = r->n[8], t9 = r->n[9];

    /*开始时减少t9，这样从第一次通过最多只能有一个进位*/
    uint32_t m;
    uint32_t x = t9 >> 22; t9 &= 0x03FFFFFUL;

    /*第一遍确保震级为1，…*/
    t0 += x * 0x3D1UL; t1 += (x << 6);
    t1 += (t0 >> 26); t0 &= 0x3FFFFFFUL;
    t2 += (t1 >> 26); t1 &= 0x3FFFFFFUL;
    t3 += (t2 >> 26); t2 &= 0x3FFFFFFUL; m = t2;
    t4 += (t3 >> 26); t3 &= 0x3FFFFFFUL; m &= t3;
    t5 += (t4 >> 26); t4 &= 0x3FFFFFFUL; m &= t4;
    t6 += (t5 >> 26); t5 &= 0x3FFFFFFUL; m &= t5;
    t7 += (t6 >> 26); t6 &= 0x3FFFFFFUL; m &= t6;
    t8 += (t7 >> 26); t7 &= 0x3FFFFFFUL; m &= t7;
    t9 += (t8 >> 26); t8 &= 0x3FFFFFFUL; m &= t8;

    /*…除了T9的22位可能的进位（即字段元素的256位）*/
    VERIFY_CHECK(t9 >> 23 == 0);

    /*最多需要进行一次最终减速；检查该值是否大于等于现场特性*/
    x = (t9 >> 22) | ((t9 == 0x03FFFFFUL) & (m == 0x3FFFFFFUL)
        & ((t1 + 0x40UL + ((t0 + 0x3D1UL) >> 26)) > 0x3FFFFFFUL));

    if (x) {
        t0 += 0x3D1UL; t1 += (x << 6);
        t1 += (t0 >> 26); t0 &= 0x3FFFFFFUL;
        t2 += (t1 >> 26); t1 &= 0x3FFFFFFUL;
        t3 += (t2 >> 26); t2 &= 0x3FFFFFFUL;
        t4 += (t3 >> 26); t3 &= 0x3FFFFFFUL;
        t5 += (t4 >> 26); t4 &= 0x3FFFFFFUL;
        t6 += (t5 >> 26); t5 &= 0x3FFFFFFUL;
        t7 += (t6 >> 26); t6 &= 0x3FFFFFFUL;
        t8 += (t7 >> 26); t7 &= 0x3FFFFFFUL;
        t9 += (t8 >> 26); t8 &= 0x3FFFFFFUL;

        /*如果t9还没有进位到第22位，那么它应该在任何最终减速之后*/
        VERIFY_CHECK(t9 >> 22 == x);

        /*从最终缩减值中屏蔽2^256的可能倍数*/
        t9 &= 0x03FFFFFUL;
    }

    r->n[0] = t0; r->n[1] = t1; r->n[2] = t2; r->n[3] = t3; r->n[4] = t4;
    r->n[5] = t5; r->n[6] = t6; r->n[7] = t7; r->n[8] = t8; r->n[9] = t9;

#ifdef VERIFY
    r->magnitude = 1;
    r->normalized = 1;
    secp256k1_fe_verify(r);
#endif
}

static int secp256k1_fe_normalizes_to_zero(secp256k1_fe *r) {
    uint32_t t0 = r->n[0], t1 = r->n[1], t2 = r->n[2], t3 = r->n[3], t4 = r->n[4],
             t5 = r->n[5], t6 = r->n[6], t7 = r->n[7], t8 = r->n[8], t9 = r->n[9];

    /*z0跟踪可能的原始值0，z1跟踪可能的原始值p*/
    uint32_t z0, z1;

    /*开始时减少t9，这样从第一次通过最多只能有一个进位*/
    uint32_t x = t9 >> 22; t9 &= 0x03FFFFFUL;

    /*第一遍确保震级为1，…*/
    t0 += x * 0x3D1UL; t1 += (x << 6);
    t1 += (t0 >> 26); t0 &= 0x3FFFFFFUL; z0  = t0; z1  = t0 ^ 0x3D0UL;
    t2 += (t1 >> 26); t1 &= 0x3FFFFFFUL; z0 |= t1; z1 &= t1 ^ 0x40UL;
    t3 += (t2 >> 26); t2 &= 0x3FFFFFFUL; z0 |= t2; z1 &= t2;
    t4 += (t3 >> 26); t3 &= 0x3FFFFFFUL; z0 |= t3; z1 &= t3;
    t5 += (t4 >> 26); t4 &= 0x3FFFFFFUL; z0 |= t4; z1 &= t4;
    t6 += (t5 >> 26); t5 &= 0x3FFFFFFUL; z0 |= t5; z1 &= t5;
    t7 += (t6 >> 26); t6 &= 0x3FFFFFFUL; z0 |= t6; z1 &= t6;
    t8 += (t7 >> 26); t7 &= 0x3FFFFFFUL; z0 |= t7; z1 &= t7;
    t9 += (t8 >> 26); t8 &= 0x3FFFFFFUL; z0 |= t8; z1 &= t8;
                                         z0 |= t9; z1 &= t9 ^ 0x3C00000UL;

    /*…除了T9的22位可能的进位（即字段元素的256位）*/
    VERIFY_CHECK(t9 >> 23 == 0);

    return (z0 == 0) | (z1 == 0x3FFFFFFUL);
}

static int secp256k1_fe_normalizes_to_zero_var(secp256k1_fe *r) {
    uint32_t t0, t1, t2, t3, t4, t5, t6, t7, t8, t9;
    uint32_t z0, z1;
    uint32_t x;

    t0 = r->n[0];
    t9 = r->n[9];

    /*开始时减少t9，这样从第一次通过最多只能有一个进位*/
    x = t9 >> 22;

    /*第一遍确保震级为1，…*/
    t0 += x * 0x3D1UL;

    /*z0跟踪可能的原始值0，z1跟踪可能的原始值p*/
    z0 = t0 & 0x3FFFFFFUL;
    z1 = z0 ^ 0x3D0UL;

    /*快速返回路径应捕获大多数情况*/
    if ((z0 != 0UL) & (z1 != 0x3FFFFFFUL)) {
        return 0;
    }

    t1 = r->n[1];
    t2 = r->n[2];
    t3 = r->n[3];
    t4 = r->n[4];
    t5 = r->n[5];
    t6 = r->n[6];
    t7 = r->n[7];
    t8 = r->n[8];

    t9 &= 0x03FFFFFUL;
    t1 += (x << 6);

    t1 += (t0 >> 26);
    t2 += (t1 >> 26); t1 &= 0x3FFFFFFUL; z0 |= t1; z1 &= t1 ^ 0x40UL;
    t3 += (t2 >> 26); t2 &= 0x3FFFFFFUL; z0 |= t2; z1 &= t2;
    t4 += (t3 >> 26); t3 &= 0x3FFFFFFUL; z0 |= t3; z1 &= t3;
    t5 += (t4 >> 26); t4 &= 0x3FFFFFFUL; z0 |= t4; z1 &= t4;
    t6 += (t5 >> 26); t5 &= 0x3FFFFFFUL; z0 |= t5; z1 &= t5;
    t7 += (t6 >> 26); t6 &= 0x3FFFFFFUL; z0 |= t6; z1 &= t6;
    t8 += (t7 >> 26); t7 &= 0x3FFFFFFUL; z0 |= t7; z1 &= t7;
    t9 += (t8 >> 26); t8 &= 0x3FFFFFFUL; z0 |= t8; z1 &= t8;
                                         z0 |= t9; z1 &= t9 ^ 0x3C00000UL;

    /*…除了T9的22位可能的进位（即字段元素的256位）*/
    VERIFY_CHECK(t9 >> 23 == 0);

    return (z0 == 0) | (z1 == 0x3FFFFFFUL);
}

SECP256K1_INLINE static void secp256k1_fe_set_int(secp256k1_fe *r, int a) {
    r->n[0] = a;
    r->n[1] = r->n[2] = r->n[3] = r->n[4] = r->n[5] = r->n[6] = r->n[7] = r->n[8] = r->n[9] = 0;
#ifdef VERIFY
    r->magnitude = 1;
    r->normalized = 1;
    secp256k1_fe_verify(r);
#endif
}

SECP256K1_INLINE static int secp256k1_fe_is_zero(const secp256k1_fe *a) {
    const uint32_t *t = a->n;
#ifdef VERIFY
    VERIFY_CHECK(a->normalized);
    secp256k1_fe_verify(a);
#endif
    return (t[0] | t[1] | t[2] | t[3] | t[4] | t[5] | t[6] | t[7] | t[8] | t[9]) == 0;
}

SECP256K1_INLINE static int secp256k1_fe_is_odd(const secp256k1_fe *a) {
#ifdef VERIFY
    VERIFY_CHECK(a->normalized);
    secp256k1_fe_verify(a);
#endif
    return a->n[0] & 1;
}

SECP256K1_INLINE static void secp256k1_fe_clear(secp256k1_fe *a) {
    int i;
#ifdef VERIFY
    a->magnitude = 0;
    a->normalized = 1;
#endif
    for (i=0; i<10; i++) {
        a->n[i] = 0;
    }
}

static int secp256k1_fe_cmp_var(const secp256k1_fe *a, const secp256k1_fe *b) {
    int i;
#ifdef VERIFY
    VERIFY_CHECK(a->normalized);
    VERIFY_CHECK(b->normalized);
    secp256k1_fe_verify(a);
    secp256k1_fe_verify(b);
#endif
    for (i = 9; i >= 0; i--) {
        if (a->n[i] > b->n[i]) {
            return 1;
        }
        if (a->n[i] < b->n[i]) {
            return -1;
        }
    }
    return 0;
}

static int secp256k1_fe_set_b32(secp256k1_fe *r, const unsigned char *a) {
    r->n[0] = (uint32_t)a[31] | ((uint32_t)a[30] << 8) | ((uint32_t)a[29] << 16) | ((uint32_t)(a[28] & 0x3) << 24);
    r->n[1] = (uint32_t)((a[28] >> 2) & 0x3f) | ((uint32_t)a[27] << 6) | ((uint32_t)a[26] << 14) | ((uint32_t)(a[25] & 0xf) << 22);
    r->n[2] = (uint32_t)((a[25] >> 4) & 0xf) | ((uint32_t)a[24] << 4) | ((uint32_t)a[23] << 12) | ((uint32_t)(a[22] & 0x3f) << 20);
    r->n[3] = (uint32_t)((a[22] >> 6) & 0x3) | ((uint32_t)a[21] << 2) | ((uint32_t)a[20] << 10) | ((uint32_t)a[19] << 18);
    r->n[4] = (uint32_t)a[18] | ((uint32_t)a[17] << 8) | ((uint32_t)a[16] << 16) | ((uint32_t)(a[15] & 0x3) << 24);
    r->n[5] = (uint32_t)((a[15] >> 2) & 0x3f) | ((uint32_t)a[14] << 6) | ((uint32_t)a[13] << 14) | ((uint32_t)(a[12] & 0xf) << 22);
    r->n[6] = (uint32_t)((a[12] >> 4) & 0xf) | ((uint32_t)a[11] << 4) | ((uint32_t)a[10] << 12) | ((uint32_t)(a[9] & 0x3f) << 20);
    r->n[7] = (uint32_t)((a[9] >> 6) & 0x3) | ((uint32_t)a[8] << 2) | ((uint32_t)a[7] << 10) | ((uint32_t)a[6] << 18);
    r->n[8] = (uint32_t)a[5] | ((uint32_t)a[4] << 8) | ((uint32_t)a[3] << 16) | ((uint32_t)(a[2] & 0x3) << 24);
    r->n[9] = (uint32_t)((a[2] >> 2) & 0x3f) | ((uint32_t)a[1] << 6) | ((uint32_t)a[0] << 14);

    if (r->n[9] == 0x3FFFFFUL && (r->n[8] & r->n[7] & r->n[6] & r->n[5] & r->n[4] & r->n[3] & r->n[2]) == 0x3FFFFFFUL && (r->n[1] + 0x40UL + ((r->n[0] + 0x3D1UL) >> 26)) > 0x3FFFFFFUL) {
        return 0;
    }
#ifdef VERIFY
    r->magnitude = 1;
    r->normalized = 1;
    secp256k1_fe_verify(r);
#endif
    return 1;
}

/*将field元素转换为32字节的big endian值。要求将输入规范化*/
static void secp256k1_fe_get_b32(unsigned char *r, const secp256k1_fe *a) {
#ifdef VERIFY
    VERIFY_CHECK(a->normalized);
    secp256k1_fe_verify(a);
#endif
    r[0] = (a->n[9] >> 14) & 0xff;
    r[1] = (a->n[9] >> 6) & 0xff;
    r[2] = ((a->n[9] & 0x3F) << 2) | ((a->n[8] >> 24) & 0x3);
    r[3] = (a->n[8] >> 16) & 0xff;
    r[4] = (a->n[8] >> 8) & 0xff;
    r[5] = a->n[8] & 0xff;
    r[6] = (a->n[7] >> 18) & 0xff;
    r[7] = (a->n[7] >> 10) & 0xff;
    r[8] = (a->n[7] >> 2) & 0xff;
    r[9] = ((a->n[7] & 0x3) << 6) | ((a->n[6] >> 20) & 0x3f);
    r[10] = (a->n[6] >> 12) & 0xff;
    r[11] = (a->n[6] >> 4) & 0xff;
    r[12] = ((a->n[6] & 0xf) << 4) | ((a->n[5] >> 22) & 0xf);
    r[13] = (a->n[5] >> 14) & 0xff;
    r[14] = (a->n[5] >> 6) & 0xff;
    r[15] = ((a->n[5] & 0x3f) << 2) | ((a->n[4] >> 24) & 0x3);
    r[16] = (a->n[4] >> 16) & 0xff;
    r[17] = (a->n[4] >> 8) & 0xff;
    r[18] = a->n[4] & 0xff;
    r[19] = (a->n[3] >> 18) & 0xff;
    r[20] = (a->n[3] >> 10) & 0xff;
    r[21] = (a->n[3] >> 2) & 0xff;
    r[22] = ((a->n[3] & 0x3) << 6) | ((a->n[2] >> 20) & 0x3f);
    r[23] = (a->n[2] >> 12) & 0xff;
    r[24] = (a->n[2] >> 4) & 0xff;
    r[25] = ((a->n[2] & 0xf) << 4) | ((a->n[1] >> 22) & 0xf);
    r[26] = (a->n[1] >> 14) & 0xff;
    r[27] = (a->n[1] >> 6) & 0xff;
    r[28] = ((a->n[1] & 0x3f) << 2) | ((a->n[0] >> 24) & 0x3);
    r[29] = (a->n[0] >> 16) & 0xff;
    r[30] = (a->n[0] >> 8) & 0xff;
    r[31] = a->n[0] & 0xff;
}

SECP256K1_INLINE static void secp256k1_fe_negate(secp256k1_fe *r, const secp256k1_fe *a, int m) {
#ifdef VERIFY
    VERIFY_CHECK(a->magnitude <= m);
    secp256k1_fe_verify(a);
#endif
    r->n[0] = 0x3FFFC2FUL * 2 * (m + 1) - a->n[0];
    r->n[1] = 0x3FFFFBFUL * 2 * (m + 1) - a->n[1];
    r->n[2] = 0x3FFFFFFUL * 2 * (m + 1) - a->n[2];
    r->n[3] = 0x3FFFFFFUL * 2 * (m + 1) - a->n[3];
    r->n[4] = 0x3FFFFFFUL * 2 * (m + 1) - a->n[4];
    r->n[5] = 0x3FFFFFFUL * 2 * (m + 1) - a->n[5];
    r->n[6] = 0x3FFFFFFUL * 2 * (m + 1) - a->n[6];
    r->n[7] = 0x3FFFFFFUL * 2 * (m + 1) - a->n[7];
    r->n[8] = 0x3FFFFFFUL * 2 * (m + 1) - a->n[8];
    r->n[9] = 0x03FFFFFUL * 2 * (m + 1) - a->n[9];
#ifdef VERIFY
    r->magnitude = m + 1;
    r->normalized = 0;
    secp256k1_fe_verify(r);
#endif
}

SECP256K1_INLINE static void secp256k1_fe_mul_int(secp256k1_fe *r, int a) {
    r->n[0] *= a;
    r->n[1] *= a;
    r->n[2] *= a;
    r->n[3] *= a;
    r->n[4] *= a;
    r->n[5] *= a;
    r->n[6] *= a;
    r->n[7] *= a;
    r->n[8] *= a;
    r->n[9] *= a;
#ifdef VERIFY
    r->magnitude *= a;
    r->normalized = 0;
    secp256k1_fe_verify(r);
#endif
}

SECP256K1_INLINE static void secp256k1_fe_add(secp256k1_fe *r, const secp256k1_fe *a) {
#ifdef VERIFY
    secp256k1_fe_verify(a);
#endif
    r->n[0] += a->n[0];
    r->n[1] += a->n[1];
    r->n[2] += a->n[2];
    r->n[3] += a->n[3];
    r->n[4] += a->n[4];
    r->n[5] += a->n[5];
    r->n[6] += a->n[6];
    r->n[7] += a->n[7];
    r->n[8] += a->n[8];
    r->n[9] += a->n[9];
#ifdef VERIFY
    r->magnitude += a->magnitude;
    r->normalized = 0;
    secp256k1_fe_verify(r);
#endif
}

#if defined(USE_EXTERNAL_ASM)

/*外部汇编程序实现*/
void secp256k1_fe_mul_inner(uint32_t *r, const uint32_t *a, const uint32_t * SECP256K1_RESTRICT b);
void secp256k1_fe_sqr_inner(uint32_t *r, const uint32_t *a);

#else

#ifdef VERIFY
#define VERIFY_BITS(x, n) VERIFY_CHECK(((x) >> (n)) == 0)
#else
#define VERIFY_BITS(x, n) do { } while(0)
#endif

SECP256K1_INLINE static void secp256k1_fe_mul_inner(uint32_t *r, const uint32_t *a, const uint32_t * SECP256K1_RESTRICT b) {
    uint64_t c, d;
    uint64_t u0, u1, u2, u3, u4, u5, u6, u7, u8;
    uint32_t t9, t1, t0, t2, t3, t4, t5, t6, t7;
    const uint32_t M = 0x3FFFFFFUL, R0 = 0x3D10UL, R1 = 0x400UL;

    VERIFY_BITS(a[0], 30);
    VERIFY_BITS(a[1], 30);
    VERIFY_BITS(a[2], 30);
    VERIFY_BITS(a[3], 30);
    VERIFY_BITS(a[4], 30);
    VERIFY_BITS(a[5], 30);
    VERIFY_BITS(a[6], 30);
    VERIFY_BITS(a[7], 30);
    VERIFY_BITS(a[8], 30);
    VERIFY_BITS(a[9], 26);
    VERIFY_BITS(b[0], 30);
    VERIFY_BITS(b[1], 30);
    VERIFY_BITS(b[2], 30);
    VERIFY_BITS(b[3], 30);
    VERIFY_BITS(b[4], 30);
    VERIFY_BITS(b[5], 30);
    VERIFY_BITS(b[6], 30);
    VERIFY_BITS(b[7], 30);
    VERIFY_BITS(b[8], 30);
    VERIFY_BITS(b[9], 26);

    /*…A B C]是…+a<<52+b<<26+c<<0 mod n.
     *px是sum（a[i]*b[x-i]，i=0..x）的简写。
     *注意[X 0 0 0 0 0 0 0 0 0 0 0]=[X*R1 X*R0]。
     **/


    d  = (uint64_t)a[0] * b[9]
       + (uint64_t)a[1] * b[8]
       + (uint64_t)a[2] * b[7]
       + (uint64_t)a[3] * b[6]
       + (uint64_t)a[4] * b[5]
       + (uint64_t)a[5] * b[4]
       + (uint64_t)a[6] * b[3]
       + (uint64_t)a[7] * b[2]
       + (uint64_t)a[8] * b[1]
       + (uint64_t)a[9] * b[0];
    /*验证_位（d，64）；*/
    /*[D 0 0 0 0 0 0 0 0 0 0]=[P9 0 0 0 0 0 0 0 0 0 0 0]*/
    t9 = d & M; d >>= 26;
    VERIFY_BITS(t9, 26);
    VERIFY_BITS(d, 38);
    /*[D t9 0 0 0 0 0 0 0 0 0 0 0]=[P9 0 0 0 0 0 0 0 0 0]*/

    c  = (uint64_t)a[0] * b[0];
    VERIFY_BITS(c, 60);
    /*[D t9 0 0 0 0 0 0 0 0 0 0 C]=[P9 0 0 0 0 0 0 0 0 P]*/
    d += (uint64_t)a[1] * b[9]
       + (uint64_t)a[2] * b[8]
       + (uint64_t)a[3] * b[7]
       + (uint64_t)a[4] * b[6]
       + (uint64_t)a[5] * b[5]
       + (uint64_t)a[6] * b[4]
       + (uint64_t)a[7] * b[3]
       + (uint64_t)a[8] * b[2]
       + (uint64_t)a[9] * b[1];
    VERIFY_BITS(d, 63);
    /*[D t9 0 0 0 0 0 0 0 0 0 0 C]=[P10 p9 0 0 0 0 0 0 0 0 0 p0]*/
    u0 = d & M; d >>= 26; c += u0 * R0;
    VERIFY_BITS(u0, 26);
    VERIFY_BITS(d, 37);
    VERIFY_BITS(c, 61);
    /*[D U0 T9 0 0 0 0 0 0 0 0 0 0 C-U0*R0]=[P10 P9 0 0 0 0 0 0 0 0 0 0 0 p0]*/
    t0 = c & M; c >>= 26; c += u0 * R1;
    VERIFY_BITS(t0, 26);
    VERIFY_BITS(c, 37);
    /*[D U0 t9 0 0 0 0 0 0 0 0 C-U0*R1 t0-U0*R0]=[P10 p9 0 0 0 0 0 0 0 0 0 0 0 0 p0]*/
    /*[D 0 t9 0 0 0 0 0 0 0 C t0]=[P10 p9 0 0 0 0 0 0 0 0 0 p0]*/

    c += (uint64_t)a[0] * b[1]
       + (uint64_t)a[1] * b[0];
    VERIFY_BITS(c, 62);
    /*[D 0 t9 0 0 0 0 0 0 0 C t0]=[p10 p9 0 0 0 0 0 0 0 0 p1 p0]*/
    d += (uint64_t)a[2] * b[9]
       + (uint64_t)a[3] * b[8]
       + (uint64_t)a[4] * b[7]
       + (uint64_t)a[5] * b[6]
       + (uint64_t)a[6] * b[5]
       + (uint64_t)a[7] * b[4]
       + (uint64_t)a[8] * b[3]
       + (uint64_t)a[9] * b[2];
    VERIFY_BITS(d, 63);
    /*[D 0 t9 0 0 0 0 0 0 0 C t0]=[p11 p10 p9 0 0 0 0 0 0 0 0 p1 p0]*/
    u1 = d & M; d >>= 26; c += u1 * R0;
    VERIFY_BITS(u1, 26);
    VERIFY_BITS(d, 37);
    VERIFY_BITS(c, 63);
    /*[D U1 0 t9 0 0 0 0 0 0 0 0 C-U1*R0 t0]=[p11 p10 p9 0 0 0 0 0 0 0 0 p1 p0]*/
    t1 = c & M; c >>= 26; c += u1 * R1;
    VERIFY_BITS(t1, 26);
    VERIFY_BITS(c, 38);
    /*[D U1 0 T9 0 0 0 0 0 0 0 C-U1*R1 T1-U1*R0 t0]=[P11 P10 P9 0 0 0 0 0 0 0 0 0 P1 p0]*/
    /*[D 0 t9 0 0 0 0 0 0 C T1 t0]=[p11 p10 p9 0 0 0 0 0 0 0 p1 p0]*/

    c += (uint64_t)a[0] * b[2]
       + (uint64_t)a[1] * b[1]
       + (uint64_t)a[2] * b[0];
    VERIFY_BITS(c, 62);
    /*[D 0 t9 0 0 0 0 0 0 C T1 t0]=[p11 p10 p9 0 0 0 0 0 0 p2 p1 p0]*/
    d += (uint64_t)a[3] * b[9]
       + (uint64_t)a[4] * b[8]
       + (uint64_t)a[5] * b[7]
       + (uint64_t)a[6] * b[6]
       + (uint64_t)a[7] * b[5]
       + (uint64_t)a[8] * b[4]
       + (uint64_t)a[9] * b[3];
    VERIFY_BITS(d, 63);
    /*[D 0 t9 0 0 0 0 0 0 C T1 t0]=[P12 p11 p10 p9 0 0 0 0 0 p2 p1 p0]*/
    u2 = d & M; d >>= 26; c += u2 * R0;
    VERIFY_BITS(u2, 26);
    VERIFY_BITS(d, 37);
    VERIFY_BITS(c, 63);
    /*[D u2 0 0 t9 0 0 0 0 0 0 0 C-u2*r0 T1 t0]=[p12 p11 p10 p9 0 0 0 0 0 p2 p1 p0]*/
    t2 = c & M; c >>= 26; c += u2 * R1;
    VERIFY_BITS(t2, 26);
    VERIFY_BITS(c, 38);
    /*[D u2 0 0 t9 0 0 0 0 0 0 C-u2*R1 t2-u2*R0 T1 t0]=[p12 p11 p10 p9 0 0 0 0 0 p2 p1 p0]*/
    /*[D 0 0 0 t9 0 0 0 0 0 C t2 t1 t0]=[P12 p11 p10 p9 0 0 0 0 0 p2 p1 p0]*/

    c += (uint64_t)a[0] * b[3]
       + (uint64_t)a[1] * b[2]
       + (uint64_t)a[2] * b[1]
       + (uint64_t)a[3] * b[0];
    VERIFY_BITS(c, 63);
    /*[D 0 0 0 t9 0 0 0 0 0 C t2 t1 t0]=[P12 p11 p10 p9 0 0 0 0 0 p3 p2 p1 p0]*/
    d += (uint64_t)a[4] * b[9]
       + (uint64_t)a[5] * b[8]
       + (uint64_t)a[6] * b[7]
       + (uint64_t)a[7] * b[6]
       + (uint64_t)a[8] * b[5]
       + (uint64_t)a[9] * b[4];
    VERIFY_BITS(d, 63);
    /*[D 0 0 0 t9 0 0 0 0 0 C t2 t1 t0]=[P13 p12 p11 p10 p9 0 0 0 0 0 p3 p2 p1 p0]*/
    u3 = d & M; d >>= 26; c += u3 * R0;
    VERIFY_BITS(u3, 26);
    VERIFY_BITS(d, 37);
    /*验证_位（c，64）；*/
    /*[D U3 0 0 0 0 T9 0 0 0 0 0 0 C-U3*R0 T2 T1 T0]=[P13 P12 P11 P10 P9 0 0 0 0 P3 P2 P1 P0]*/
    t3 = c & M; c >>= 26; c += u3 * R1;
    VERIFY_BITS(t3, 26);
    VERIFY_BITS(c, 39);
    /*[D U3 0 0 0 0 T9 0 0 0 0 C-U3*R1 T3-U3*R0 T2 T1 T0]=[P13 P12 P11 P10 P9 0 0 0 0 0 P3 P2 P1 P0]*/
    /*[D 0 0 0 0 t9 0 0 0 0 C t3 t2 t1 t0]=[P13 p12 p11 p10 p9 0 0 0 0 p3 p2 p1 p0]*/

    c += (uint64_t)a[0] * b[4]
       + (uint64_t)a[1] * b[3]
       + (uint64_t)a[2] * b[2]
       + (uint64_t)a[3] * b[1]
       + (uint64_t)a[4] * b[0];
    VERIFY_BITS(c, 63);
    /*[D 0 0 0 0 t9 0 0 0 0 C t3 t2 t1 t0]=[P13 p12 p11 p10 p9 0 0 0 p4 p3 p2 p1 p0]*/
    d += (uint64_t)a[5] * b[9]
       + (uint64_t)a[6] * b[8]
       + (uint64_t)a[7] * b[7]
       + (uint64_t)a[8] * b[6]
       + (uint64_t)a[9] * b[5];
    VERIFY_BITS(d, 62);
    /*[D 0 0 0 0 t9 0 0 0 0 C t3 t2 t1 t0]=[P14 p13 p12 p11 p10 p9 0 0 0 p4 p3 p2 p1 p0]*/
    u4 = d & M; d >>= 26; c += u4 * R0;
    VERIFY_BITS(u4, 26);
    VERIFY_BITS(d, 36);
    /*验证_位（c，64）；*/
    /*[D U4 0 0 0 0 0 T9 0 0 0 0 C-U4*R0 T3 T2 T1 T0]=[P14 P13 P12 P11 P10 P9 0 0 0 P4 P3 P2 P1 P0]*/
    t4 = c & M; c >>= 26; c += u4 * R1;
    VERIFY_BITS(t4, 26);
    VERIFY_BITS(c, 39);
    /*[D U4 0 0 0 0 0 T9 0 0 0 C-U4*R1 T4-U4*R0 T3 T2 T1 T0]=[P14 P13 P12 P11 P10 P9 0 0 0 P4 P3 P2 P1 P0]*/
    /*[D 0 0 0 0 0 t9 0 0 C t4 t3 t2 t1 t0]=[p14 p13 p12 p11 p10 p9 0 0 0 p4 p3 p2 p1 p0]*/

    c += (uint64_t)a[0] * b[5]
       + (uint64_t)a[1] * b[4]
       + (uint64_t)a[2] * b[3]
       + (uint64_t)a[3] * b[2]
       + (uint64_t)a[4] * b[1]
       + (uint64_t)a[5] * b[0];
    VERIFY_BITS(c, 63);
    /*[D 0 0 0 0 0 t9 0 0 C t4 t3 t2 t1 t0]=[P14 p13 p12 p11 p10 p9 0 0 p5 p4 p3 p2 p1 p0]*/
    d += (uint64_t)a[6] * b[9]
       + (uint64_t)a[7] * b[8]
       + (uint64_t)a[8] * b[7]
       + (uint64_t)a[9] * b[6];
    VERIFY_BITS(d, 62);
    /*[D 0 0 0 0 0 t9 0 0 C t4 t3 t2 t1 t0]=[P15 p14 p13 p12 p11 p10 p9 0 0 p5 p4 p3 p2 p1 p0]*/
    u5 = d & M; d >>= 26; c += u5 * R0;
    VERIFY_BITS(u5, 26);
    VERIFY_BITS(d, 36);
    /*验证_位（c，64）；*/
    /*[D U5 0 0 0 0 0 0 T9 0 0 0 C-U5*R0 T4 T3 T2 T1 T0]=[P15 P14 P13 P12 P11 P10 P9 0 0 P5 P4 P3 P2 P1 P0]*/
    t5 = c & M; c >>= 26; c += u5 * R1;
    VERIFY_BITS(t5, 26);
    VERIFY_BITS(c, 39);
    /*[D U5 0 0 0 0 0 T9 0 C-U5*R1 T5-U5*R0 T4 T3 T2 T1 T0]=[P15 P14 P13 P12 P11 P10 P9 0 0 P5 P4 P3 P2 P1 P0]*/
    /*[D 0 0 0 0 0 0 t9 0 C t5 t4 t3 t2 t1 t0]=[P15 p14 p13 p12 p11 p10 p9 0 0 p5 p4 p3 p2 p1 p0]*/

    c += (uint64_t)a[0] * b[6]
       + (uint64_t)a[1] * b[5]
       + (uint64_t)a[2] * b[4]
       + (uint64_t)a[3] * b[3]
       + (uint64_t)a[4] * b[2]
       + (uint64_t)a[5] * b[1]
       + (uint64_t)a[6] * b[0];
    VERIFY_BITS(c, 63);
    /*[D 0 0 0 0 0 0 t9 0 C t5 t4 t3 t2 t1 t0]=[P15 p14 p13 p12 p11 p10 p9 0 p6 p5 p4 p3 p2 p1 p0]*/
    d += (uint64_t)a[7] * b[9]
       + (uint64_t)a[8] * b[8]
       + (uint64_t)a[9] * b[7];
    VERIFY_BITS(d, 61);
    /*[D 0 0 0 0 0 0 t9 0 C t5 t4 t3 t2 t1 t0]=[P16 p15 p14 p13 p12 p11 p10 p9 0 p6 p5 p4 p3 p2 p1 p0]*/
    u6 = d & M; d >>= 26; c += u6 * R0;
    VERIFY_BITS(u6, 26);
    VERIFY_BITS(d, 35);
    /*验证_位（c，64）；*/
    /*[D U6 0 0 0 0 0 0 T9 0 C-U6*R0 T5 T4 T3 T2 T1 T0]=[P16 P15 P14 P13 P12 P11 P10 P9 0 P6 P5 P4 P3 P2 P1 P0]*/
    t6 = c & M; c >>= 26; c += u6 * R1;
    VERIFY_BITS(t6, 26);
    VERIFY_BITS(c, 39);
    /*[D U6 0 0 0 0 0 0 T9 0 C-U6*R1 T6-U6*R0 T5 T4 T3 T2 T1 T0]=[P16 P15 P14 P13 P12 P11 P10 P9 0 P6 P5 P4 P3 P1 P0]*/
    /*[D 0 0 0 0 0 0 0 t9 0 c t6 t5 t4 t3 t2 t1 t0]=[P16 p15 p14 p13 p12 p11 p10 p9 0 p6 p5 p4 p3 p2 p1 p0]*/

    c += (uint64_t)a[0] * b[7]
       + (uint64_t)a[1] * b[6]
       + (uint64_t)a[2] * b[5]
       + (uint64_t)a[3] * b[4]
       + (uint64_t)a[4] * b[3]
       + (uint64_t)a[5] * b[2]
       + (uint64_t)a[6] * b[1]
       + (uint64_t)a[7] * b[0];
    /*验证_位（c，64）；*/
    VERIFY_CHECK(c <= 0x8000007C00000007ULL);
    /*[D 0 0 0 0 0 0 0 t9 0 c t6 t5 t4 t3 t2 t1 t0]=[P16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p4 p3 p2 p1 p0]*/
    d += (uint64_t)a[8] * b[9]
       + (uint64_t)a[9] * b[8];
    VERIFY_BITS(d, 58);
    /*[D 0 0 0 0 0 0 0 t9 0 c t6 t5 t4 t3 t2 t1 t0]=[P17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p4 p3 p2 p1 p0]*/
    u7 = d & M; d >>= 26; c += u7 * R0;
    VERIFY_BITS(u7, 26);
    VERIFY_BITS(d, 32);
    /*验证_位（c，64）；*/
    VERIFY_CHECK(c <= 0x800001703FFFC2F7ULL);
    /*[D U7 0 0 0 0 0 0 0 T9 0 C-U7*R0 T6 t5 t4 t3 t2 t1 t0]=[P17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p4 p3 p2 p1 p0]*/
    t7 = c & M; c >>= 26; c += u7 * R1;
    VERIFY_BITS(t7, 26);
    VERIFY_BITS(c, 38);
    /*[D U7 0 0 0 0 0 0 0 T9 C-U7*R1 T7-U7*R0 T6 t5 t4 t3 t2 t1 t0]=[P17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p4 p3 p2 p1 p0]*/
    /*[D 0 0 0 0 0 0 0 0 0 T9 C T7 T6 t5 t4 t3 t2 t1 t0]=[P17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p4 p3 p2 p1 p0]*/

    c += (uint64_t)a[0] * b[8]
       + (uint64_t)a[1] * b[7]
       + (uint64_t)a[2] * b[6]
       + (uint64_t)a[3] * b[5]
       + (uint64_t)a[4] * b[4]
       + (uint64_t)a[5] * b[3]
       + (uint64_t)a[6] * b[2]
       + (uint64_t)a[7] * b[1]
       + (uint64_t)a[8] * b[0];
    /*验证_位（c，64）；*/
    VERIFY_CHECK(c <= 0x9000007B80000008ULL);
    /*[D 0 0 0 0 0 0 0 0 T9 C T7 T6 t5 t4 t3 t2 t1 t0]=[P17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p4 p3 p2 p1 p0]*/
    d += (uint64_t)a[9] * b[9];
    VERIFY_BITS(d, 57);
    /*[D 0 0 0 0 0 0 0 0 T9 C T7 T6 t5 t4 t3 t2 t1 t0]=[P18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p7 p6 p4 p3 p2 p1 p0]*/
    u8 = d & M; d >>= 26; c += u8 * R0;
    VERIFY_BITS(u8, 26);
    VERIFY_BITS(d, 31);
    /*验证_位（c，64）；*/
    VERIFY_CHECK(c <= 0x9000016FBFFFC2F8ULL);
    /*[D U8 0 0 0 0 0 0 0 0 T9 C-U8*R0 T7 T6 t5 t4 t3 t2 t1 t0]=[P18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p7 p6 p4 p3 p2 p1 p0]*/

    r[3] = t3;
    VERIFY_BITS(r[3], 26);
    /*[D U8 0 0 0 0 0 0 0 0 T9 C-U8*R0 T7 T6 t5 t4 r3 t2 t1 t0]=[P18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p7 p6 p4 p3 p2 p1 p0]*/
    r[4] = t4;
    VERIFY_BITS(r[4], 26);
    /*[D U8 0 0 0 0 0 0 0 0 T9 C-U8*R0 T7 T6 t5 r4 r3 t2 t1 t0]=[P18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p7 p6 p4 p3 p2 p1 p0]*/
    r[5] = t5;
    VERIFY_BITS(r[5], 26);
    /*[D U8 0 0 0 0 0 0 0 0 T9 C-U8*R0 T7 T6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P2 P1 P0]*/
    r[6] = t6;
    VERIFY_BITS(r[6], 26);
    /*[D U8 0 0 0 0 0 0 0 0 T9 C-U8*R0 T7 R6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P2 P1 P0]*/
    r[7] = t7;
    VERIFY_BITS(r[7], 26);
    /*[D U8 0 0 0 0 0 0 0 0 T9 C-U8*R0 R7 R6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P2 P1 P0]*/

    r[8] = c & M; c >>= 26; c += u8 * R1;
    VERIFY_BITS(r[8], 26);
    VERIFY_BITS(c, 39);
    /*[D U8 0 0 0 0 0 0 0 0 T9+C-U8*R1 R8-U8*R0 R7 R6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P1 P0]*/
    /*[D 0 0 0 0 0 0 0 0 0 t9+C r8 r7 r6 r5 r4 r3 t2 t1 t0]=[P18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p7 p6 p5 p4 p2 p1 p0]*/
    c   += d * R0 + t9;
    VERIFY_BITS(c, 45);
    /*[D 0 0 0 0 0 0 0 0 0 0 C-D*R0 R8 R7 R6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P5 P4 P3 P2 P1 P0]*/
    r[9] = c & (M >> 4); c >>= 22; c += d * (R1 << 4);
    VERIFY_BITS(r[9], 22);
    VERIFY_BITS(c, 46);
    /*[D 0 0 0 0 0 0 0 0 0 R9+（（C-D*R1<<4）<<22）-D*R0 R8 R7 R6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P1 P0]*/
    /*[D 0 0 0 0 0 0 0-D*R1 R9+（C<22）-D*R0 R8 R7 R6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P1 P0]*/
    /*[R9+（C<22）R8 R7 R6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P2 P1 P0]*/

    d    = c * (R0 >> 4) + t0;
    VERIFY_BITS(d, 56);
    /*[R9+（C<<22）R8 R7 R6 R5 R4 R3 T2 T1 D-C*R0>>4]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P5 P4 P3 P2 P1 P0]*/
    r[0] = d & M; d >>= 26;
    VERIFY_BITS(r[0], 26);
    VERIFY_BITS(d, 30);
    /*[R9+（C<<22）R8 R7 R6 R5 R4 R3 T2 T1+D R0-C*R0>>4]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P2 P1 P0]*/
    d   += c * (R1 >> 4) + t1;
    VERIFY_BITS(d, 53);
    VERIFY_CHECK(d <= 0x10000003FFFFBFULL);
    /*[R9+（C<22）R8 R7 R6 R5 R4 R3 T2 D-C*R1>>4 R0-C*R0>>4]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P5 P4 P3 P2 P1 P0]*/
    /*[r9 r8 r7 r6 r5 r4 r3 t2 d r0]=[p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p4 p3 p2 p1 p0]*/
    r[1] = d & M; d >>= 26;
    VERIFY_BITS(r[1], 26);
    VERIFY_BITS(d, 27);
    VERIFY_CHECK(d <= 0x4000000ULL);
    /*[r9 r8 r7 r6 r5 r4 r3 t2+d r1 r0]=[p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p4 p3 p2 p1 p0]*/
    d   += t2;
    VERIFY_BITS(d, 27);
    /*[R9 r8 r7 r6 r5 r4 r3 d r1 r0]=[P18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p4 p3 p2 p1 p0]*/
    r[2] = d;
    VERIFY_BITS(r[2], 27);
    /*[r9 r8 r7 r6 r5 r4 r3 r2 r1 r0]=[p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p4 p3 p2 p1 p0]*/
}

SECP256K1_INLINE static void secp256k1_fe_sqr_inner(uint32_t *r, const uint32_t *a) {
    uint64_t c, d;
    uint64_t u0, u1, u2, u3, u4, u5, u6, u7, u8;
    uint32_t t9, t0, t1, t2, t3, t4, t5, t6, t7;
    const uint32_t M = 0x3FFFFFFUL, R0 = 0x3D10UL, R1 = 0x400UL;

    VERIFY_BITS(a[0], 30);
    VERIFY_BITS(a[1], 30);
    VERIFY_BITS(a[2], 30);
    VERIFY_BITS(a[3], 30);
    VERIFY_BITS(a[4], 30);
    VERIFY_BITS(a[5], 30);
    VERIFY_BITS(a[6], 30);
    VERIFY_BITS(a[7], 30);
    VERIFY_BITS(a[8], 30);
    VERIFY_BITS(a[9], 26);

    /*…A B C]是…+a<<52+b<<26+c<<0 mod n.
     *px是sum（a[i]*a[x-i]，i=0..x）的简写。
     *注意[X 0 0 0 0 0 0 0 0 0 0 0]=[X*R1 X*R0]。
     **/


    d  = (uint64_t)(a[0]*2) * a[9]
       + (uint64_t)(a[1]*2) * a[8]
       + (uint64_t)(a[2]*2) * a[7]
       + (uint64_t)(a[3]*2) * a[6]
       + (uint64_t)(a[4]*2) * a[5];
    /*验证_位（d，64）；*/
    /*[D 0 0 0 0 0 0 0 0 0 0]=[P9 0 0 0 0 0 0 0 0 0 0 0]*/
    t9 = d & M; d >>= 26;
    VERIFY_BITS(t9, 26);
    VERIFY_BITS(d, 38);
    /*[D t9 0 0 0 0 0 0 0 0 0 0 0]=[P9 0 0 0 0 0 0 0 0 0]*/

    c  = (uint64_t)a[0] * a[0];
    VERIFY_BITS(c, 60);
    /*[D t9 0 0 0 0 0 0 0 0 0 0 C]=[P9 0 0 0 0 0 0 0 0 P]*/
    d += (uint64_t)(a[1]*2) * a[9]
       + (uint64_t)(a[2]*2) * a[8]
       + (uint64_t)(a[3]*2) * a[7]
       + (uint64_t)(a[4]*2) * a[6]
       + (uint64_t)a[5] * a[5];
    VERIFY_BITS(d, 63);
    /*[D t9 0 0 0 0 0 0 0 0 0 0 C]=[P10 p9 0 0 0 0 0 0 0 0 0 p0]*/
    u0 = d & M; d >>= 26; c += u0 * R0;
    VERIFY_BITS(u0, 26);
    VERIFY_BITS(d, 37);
    VERIFY_BITS(c, 61);
    /*[D U0 T9 0 0 0 0 0 0 0 0 0 0 C-U0*R0]=[P10 P9 0 0 0 0 0 0 0 0 0 0 0 p0]*/
    t0 = c & M; c >>= 26; c += u0 * R1;
    VERIFY_BITS(t0, 26);
    VERIFY_BITS(c, 37);
    /*[D U0 t9 0 0 0 0 0 0 0 0 C-U0*R1 t0-U0*R0]=[P10 p9 0 0 0 0 0 0 0 0 0 0 0 0 p0]*/
    /*[D 0 t9 0 0 0 0 0 0 0 C t0]=[P10 p9 0 0 0 0 0 0 0 0 0 p0]*/

    c += (uint64_t)(a[0]*2) * a[1];
    VERIFY_BITS(c, 62);
    /*[D 0 t9 0 0 0 0 0 0 0 C t0]=[p10 p9 0 0 0 0 0 0 0 0 p1 p0]*/
    d += (uint64_t)(a[2]*2) * a[9]
       + (uint64_t)(a[3]*2) * a[8]
       + (uint64_t)(a[4]*2) * a[7]
       + (uint64_t)(a[5]*2) * a[6];
    VERIFY_BITS(d, 63);
    /*[D 0 t9 0 0 0 0 0 0 0 C t0]=[p11 p10 p9 0 0 0 0 0 0 0 0 p1 p0]*/
    u1 = d & M; d >>= 26; c += u1 * R0;
    VERIFY_BITS(u1, 26);
    VERIFY_BITS(d, 37);
    VERIFY_BITS(c, 63);
    /*[D U1 0 t9 0 0 0 0 0 0 0 0 C-U1*R0 t0]=[p11 p10 p9 0 0 0 0 0 0 0 0 p1 p0]*/
    t1 = c & M; c >>= 26; c += u1 * R1;
    VERIFY_BITS(t1, 26);
    VERIFY_BITS(c, 38);
    /*[D U1 0 T9 0 0 0 0 0 0 0 C-U1*R1 T1-U1*R0 t0]=[P11 P10 P9 0 0 0 0 0 0 0 0 0 P1 p0]*/
    /*[D 0 t9 0 0 0 0 0 0 C T1 t0]=[p11 p10 p9 0 0 0 0 0 0 0 p1 p0]*/

    c += (uint64_t)(a[0]*2) * a[2]
       + (uint64_t)a[1] * a[1];
    VERIFY_BITS(c, 62);
    /*[D 0 t9 0 0 0 0 0 0 C T1 t0]=[p11 p10 p9 0 0 0 0 0 0 p2 p1 p0]*/
    d += (uint64_t)(a[3]*2) * a[9]
       + (uint64_t)(a[4]*2) * a[8]
       + (uint64_t)(a[5]*2) * a[7]
       + (uint64_t)a[6] * a[6];
    VERIFY_BITS(d, 63);
    /*[D 0 t9 0 0 0 0 0 0 C T1 t0]=[P12 p11 p10 p9 0 0 0 0 0 p2 p1 p0]*/
    u2 = d & M; d >>= 26; c += u2 * R0;
    VERIFY_BITS(u2, 26);
    VERIFY_BITS(d, 37);
    VERIFY_BITS(c, 63);
    /*[D u2 0 0 t9 0 0 0 0 0 0 0 C-u2*r0 T1 t0]=[p12 p11 p10 p9 0 0 0 0 0 p2 p1 p0]*/
    t2 = c & M; c >>= 26; c += u2 * R1;
    VERIFY_BITS(t2, 26);
    VERIFY_BITS(c, 38);
    /*[D u2 0 0 t9 0 0 0 0 0 0 C-u2*R1 t2-u2*R0 T1 t0]=[p12 p11 p10 p9 0 0 0 0 0 p2 p1 p0]*/
    /*[D 0 0 0 t9 0 0 0 0 0 C t2 t1 t0]=[P12 p11 p10 p9 0 0 0 0 0 p2 p1 p0]*/

    c += (uint64_t)(a[0]*2) * a[3]
       + (uint64_t)(a[1]*2) * a[2];
    VERIFY_BITS(c, 63);
    /*[D 0 0 0 t9 0 0 0 0 0 C t2 t1 t0]=[P12 p11 p10 p9 0 0 0 0 0 p3 p2 p1 p0]*/
    d += (uint64_t)(a[4]*2) * a[9]
       + (uint64_t)(a[5]*2) * a[8]
       + (uint64_t)(a[6]*2) * a[7];
    VERIFY_BITS(d, 63);
    /*[D 0 0 0 t9 0 0 0 0 0 C t2 t1 t0]=[P13 p12 p11 p10 p9 0 0 0 0 0 p3 p2 p1 p0]*/
    u3 = d & M; d >>= 26; c += u3 * R0;
    VERIFY_BITS(u3, 26);
    VERIFY_BITS(d, 37);
    /*验证_位（c，64）；*/
    /*[D U3 0 0 0 0 T9 0 0 0 0 0 0 C-U3*R0 T2 T1 T0]=[P13 P12 P11 P10 P9 0 0 0 0 P3 P2 P1 P0]*/
    t3 = c & M; c >>= 26; c += u3 * R1;
    VERIFY_BITS(t3, 26);
    VERIFY_BITS(c, 39);
    /*[D U3 0 0 0 0 T9 0 0 0 0 C-U3*R1 T3-U3*R0 T2 T1 T0]=[P13 P12 P11 P10 P9 0 0 0 0 0 P3 P2 P1 P0]*/
    /*[D 0 0 0 0 t9 0 0 0 0 C t3 t2 t1 t0]=[P13 p12 p11 p10 p9 0 0 0 0 p3 p2 p1 p0]*/

    c += (uint64_t)(a[0]*2) * a[4]
       + (uint64_t)(a[1]*2) * a[3]
       + (uint64_t)a[2] * a[2];
    VERIFY_BITS(c, 63);
    /*[D 0 0 0 0 t9 0 0 0 0 C t3 t2 t1 t0]=[P13 p12 p11 p10 p9 0 0 0 p4 p3 p2 p1 p0]*/
    d += (uint64_t)(a[5]*2) * a[9]
       + (uint64_t)(a[6]*2) * a[8]
       + (uint64_t)a[7] * a[7];
    VERIFY_BITS(d, 62);
    /*[D 0 0 0 0 t9 0 0 0 0 C t3 t2 t1 t0]=[P14 p13 p12 p11 p10 p9 0 0 0 p4 p3 p2 p1 p0]*/
    u4 = d & M; d >>= 26; c += u4 * R0;
    VERIFY_BITS(u4, 26);
    VERIFY_BITS(d, 36);
    /*验证_位（c，64）；*/
    /*[D U4 0 0 0 0 0 T9 0 0 0 0 C-U4*R0 T3 T2 T1 T0]=[P14 P13 P12 P11 P10 P9 0 0 0 P4 P3 P2 P1 P0]*/
    t4 = c & M; c >>= 26; c += u4 * R1;
    VERIFY_BITS(t4, 26);
    VERIFY_BITS(c, 39);
    /*[D U4 0 0 0 0 0 T9 0 0 0 C-U4*R1 T4-U4*R0 T3 T2 T1 T0]=[P14 P13 P12 P11 P10 P9 0 0 0 P4 P3 P2 P1 P0]*/
    /*[D 0 0 0 0 0 t9 0 0 C t4 t3 t2 t1 t0]=[p14 p13 p12 p11 p10 p9 0 0 0 p4 p3 p2 p1 p0]*/

    c += (uint64_t)(a[0]*2) * a[5]
       + (uint64_t)(a[1]*2) * a[4]
       + (uint64_t)(a[2]*2) * a[3];
    VERIFY_BITS(c, 63);
    /*[D 0 0 0 0 0 t9 0 0 C t4 t3 t2 t1 t0]=[P14 p13 p12 p11 p10 p9 0 0 p5 p4 p3 p2 p1 p0]*/
    d += (uint64_t)(a[6]*2) * a[9]
       + (uint64_t)(a[7]*2) * a[8];
    VERIFY_BITS(d, 62);
    /*[D 0 0 0 0 0 t9 0 0 C t4 t3 t2 t1 t0]=[P15 p14 p13 p12 p11 p10 p9 0 0 p5 p4 p3 p2 p1 p0]*/
    u5 = d & M; d >>= 26; c += u5 * R0;
    VERIFY_BITS(u5, 26);
    VERIFY_BITS(d, 36);
    /*验证_位（c，64）；*/
    /*[D U5 0 0 0 0 0 0 T9 0 0 0 C-U5*R0 T4 T3 T2 T1 T0]=[P15 P14 P13 P12 P11 P10 P9 0 0 P5 P4 P3 P2 P1 P0]*/
    t5 = c & M; c >>= 26; c += u5 * R1;
    VERIFY_BITS(t5, 26);
    VERIFY_BITS(c, 39);
    /*[D U5 0 0 0 0 0 T9 0 C-U5*R1 T5-U5*R0 T4 T3 T2 T1 T0]=[P15 P14 P13 P12 P11 P10 P9 0 0 P5 P4 P3 P2 P1 P0]*/
    /*[D 0 0 0 0 0 0 t9 0 C t5 t4 t3 t2 t1 t0]=[P15 p14 p13 p12 p11 p10 p9 0 0 p5 p4 p3 p2 p1 p0]*/

    c += (uint64_t)(a[0]*2) * a[6]
       + (uint64_t)(a[1]*2) * a[5]
       + (uint64_t)(a[2]*2) * a[4]
       + (uint64_t)a[3] * a[3];
    VERIFY_BITS(c, 63);
    /*[D 0 0 0 0 0 0 t9 0 C t5 t4 t3 t2 t1 t0]=[P15 p14 p13 p12 p11 p10 p9 0 p6 p5 p4 p3 p2 p1 p0]*/
    d += (uint64_t)(a[7]*2) * a[9]
       + (uint64_t)a[8] * a[8];
    VERIFY_BITS(d, 61);
    /*[D 0 0 0 0 0 0 t9 0 C t5 t4 t3 t2 t1 t0]=[P16 p15 p14 p13 p12 p11 p10 p9 0 p6 p5 p4 p3 p2 p1 p0]*/
    u6 = d & M; d >>= 26; c += u6 * R0;
    VERIFY_BITS(u6, 26);
    VERIFY_BITS(d, 35);
    /*验证_位（c，64）；*/
    /*[D U6 0 0 0 0 0 0 T9 0 C-U6*R0 T5 T4 T3 T2 T1 T0]=[P16 P15 P14 P13 P12 P11 P10 P9 0 P6 P5 P4 P3 P2 P1 P0]*/
    t6 = c & M; c >>= 26; c += u6 * R1;
    VERIFY_BITS(t6, 26);
    VERIFY_BITS(c, 39);
    /*[D U6 0 0 0 0 0 0 T9 0 C-U6*R1 T6-U6*R0 T5 T4 T3 T2 T1 T0]=[P16 P15 P14 P13 P12 P11 P10 P9 0 P6 P5 P4 P3 P1 P0]*/
    /*[D 0 0 0 0 0 0 0 t9 0 c t6 t5 t4 t3 t2 t1 t0]=[P16 p15 p14 p13 p12 p11 p10 p9 0 p6 p5 p4 p3 p2 p1 p0]*/

    c += (uint64_t)(a[0]*2) * a[7]
       + (uint64_t)(a[1]*2) * a[6]
       + (uint64_t)(a[2]*2) * a[5]
       + (uint64_t)(a[3]*2) * a[4];
    /*验证_位（c，64）；*/
    VERIFY_CHECK(c <= 0x8000007C00000007ULL);
    /*[D 0 0 0 0 0 0 0 t9 0 c t6 t5 t4 t3 t2 t1 t0]=[P16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p4 p3 p2 p1 p0]*/
    d += (uint64_t)(a[8]*2) * a[9];
    VERIFY_BITS(d, 58);
    /*[D 0 0 0 0 0 0 0 t9 0 c t6 t5 t4 t3 t2 t1 t0]=[P17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p4 p3 p2 p1 p0]*/
    u7 = d & M; d >>= 26; c += u7 * R0;
    VERIFY_BITS(u7, 26);
    VERIFY_BITS(d, 32);
    /*验证_位（c，64）；*/
    VERIFY_CHECK(c <= 0x800001703FFFC2F7ULL);
    /*[D U7 0 0 0 0 0 0 0 T9 0 C-U7*R0 T6 t5 t4 t3 t2 t1 t0]=[P17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p4 p3 p2 p1 p0]*/
    t7 = c & M; c >>= 26; c += u7 * R1;
    VERIFY_BITS(t7, 26);
    VERIFY_BITS(c, 38);
    /*[D U7 0 0 0 0 0 0 0 T9 C-U7*R1 T7-U7*R0 T6 t5 t4 t3 t2 t1 t0]=[P17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p4 p3 p2 p1 p0]*/
    /*[D 0 0 0 0 0 0 0 0 0 T9 C T7 T6 t5 t4 t3 t2 t1 t0]=[P17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p4 p3 p2 p1 p0]*/

    c += (uint64_t)(a[0]*2) * a[8]
       + (uint64_t)(a[1]*2) * a[7]
       + (uint64_t)(a[2]*2) * a[6]
       + (uint64_t)(a[3]*2) * a[5]
       + (uint64_t)a[4] * a[4];
    /*验证_位（c，64）；*/
    VERIFY_CHECK(c <= 0x9000007B80000008ULL);
    /*[D 0 0 0 0 0 0 0 0 T9 C T7 T6 t5 t4 t3 t2 t1 t0]=[P17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p4 p3 p2 p1 p0]*/
    d += (uint64_t)a[9] * a[9];
    VERIFY_BITS(d, 57);
    /*[D 0 0 0 0 0 0 0 0 T9 C T7 T6 t5 t4 t3 t2 t1 t0]=[P18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p7 p6 p4 p3 p2 p1 p0]*/
    u8 = d & M; d >>= 26; c += u8 * R0;
    VERIFY_BITS(u8, 26);
    VERIFY_BITS(d, 31);
    /*验证_位（c，64）；*/
    VERIFY_CHECK(c <= 0x9000016FBFFFC2F8ULL);
    /*[D U8 0 0 0 0 0 0 0 0 T9 C-U8*R0 T7 T6 t5 t4 t3 t2 t1 t0]=[P18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p7 p6 p4 p3 p2 p1 p0]*/

    r[3] = t3;
    VERIFY_BITS(r[3], 26);
    /*[D U8 0 0 0 0 0 0 0 0 T9 C-U8*R0 T7 T6 t5 t4 r3 t2 t1 t0]=[P18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p7 p6 p4 p3 p2 p1 p0]*/
    r[4] = t4;
    VERIFY_BITS(r[4], 26);
    /*[D U8 0 0 0 0 0 0 0 0 T9 C-U8*R0 T7 T6 t5 r4 r3 t2 t1 t0]=[P18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p7 p6 p4 p3 p2 p1 p0]*/
    r[5] = t5;
    VERIFY_BITS(r[5], 26);
    /*[D U8 0 0 0 0 0 0 0 0 T9 C-U8*R0 T7 T6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P2 P1 P0]*/
    r[6] = t6;
    VERIFY_BITS(r[6], 26);
    /*[D U8 0 0 0 0 0 0 0 0 T9 C-U8*R0 T7 R6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P2 P1 P0]*/
    r[7] = t7;
    VERIFY_BITS(r[7], 26);
    /*[D U8 0 0 0 0 0 0 0 0 T9 C-U8*R0 R7 R6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P2 P1 P0]*/

    r[8] = c & M; c >>= 26; c += u8 * R1;
    VERIFY_BITS(r[8], 26);
    VERIFY_BITS(c, 39);
    /*[D U8 0 0 0 0 0 0 0 0 T9+C-U8*R1 R8-U8*R0 R7 R6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P1 P0]*/
    /*[D 0 0 0 0 0 0 0 0 0 t9+C r8 r7 r6 r5 r4 r3 t2 t1 t0]=[P18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p7 p6 p5 p4 p2 p1 p0]*/
    c   += d * R0 + t9;
    VERIFY_BITS(c, 45);
    /*[D 0 0 0 0 0 0 0 0 0 0 C-D*R0 R8 R7 R6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P5 P4 P3 P2 P1 P0]*/
    r[9] = c & (M >> 4); c >>= 22; c += d * (R1 << 4);
    VERIFY_BITS(r[9], 22);
    VERIFY_BITS(c, 46);
    /*[D 0 0 0 0 0 0 0 0 0 R9+（（C-D*R1<<4）<<22）-D*R0 R8 R7 R6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P1 P0]*/
    /*[D 0 0 0 0 0 0 0-D*R1 R9+（C<22）-D*R0 R8 R7 R6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P1 P0]*/
    /*[R9+（C<22）R8 R7 R6 R5 R4 R3 T2 T1 T0]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P2 P1 P0]*/

    d    = c * (R0 >> 4) + t0;
    VERIFY_BITS(d, 56);
    /*[R9+（C<<22）R8 R7 R6 R5 R4 R3 T2 T1 D-C*R0>>4]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P5 P4 P3 P2 P1 P0]*/
    r[0] = d & M; d >>= 26;
    VERIFY_BITS(r[0], 26);
    VERIFY_BITS(d, 30);
    /*[R9+（C<<22）R8 R7 R6 R5 R4 R3 T2 T1+D R0-C*R0>>4]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P6 P5 P4 P3 P2 P1 P0]*/
    d   += c * (R1 >> 4) + t1;
    VERIFY_BITS(d, 53);
    VERIFY_CHECK(d <= 0x10000003FFFFBFULL);
    /*[R9+（C<22）R8 R7 R6 R5 R4 R3 T2 D-C*R1>>4 R0-C*R0>>4]=[P18 P17 P16 P15 P14 P13 P12 P11 P10 P9 P7 P5 P4 P3 P2 P1 P0]*/
    /*[r9 r8 r7 r6 r5 r4 r3 t2 d r0]=[p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p4 p3 p2 p1 p0]*/
    r[1] = d & M; d >>= 26;
    VERIFY_BITS(r[1], 26);
    VERIFY_BITS(d, 27);
    VERIFY_CHECK(d <= 0x4000000ULL);
    /*[r9 r8 r7 r6 r5 r4 r3 t2+d r1 r0]=[p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p4 p3 p2 p1 p0]*/
    d   += t2;
    VERIFY_BITS(d, 27);
    /*[R9 r8 r7 r6 r5 r4 r3 d r1 r0]=[P18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p4 p3 p2 p1 p0]*/
    r[2] = d;
    VERIFY_BITS(r[2], 27);
    /*[r9 r8 r7 r6 r5 r4 r3 r2 r1 r0]=[p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p4 p3 p2 p1 p0]*/
}
#endif

static void secp256k1_fe_mul(secp256k1_fe *r, const secp256k1_fe *a, const secp256k1_fe * SECP256K1_RESTRICT b) {
#ifdef VERIFY
    VERIFY_CHECK(a->magnitude <= 8);
    VERIFY_CHECK(b->magnitude <= 8);
    secp256k1_fe_verify(a);
    secp256k1_fe_verify(b);
    VERIFY_CHECK(r != b);
#endif
    secp256k1_fe_mul_inner(r->n, a->n, b->n);
#ifdef VERIFY
    r->magnitude = 1;
    r->normalized = 0;
    secp256k1_fe_verify(r);
#endif
}

static void secp256k1_fe_sqr(secp256k1_fe *r, const secp256k1_fe *a) {
#ifdef VERIFY
    VERIFY_CHECK(a->magnitude <= 8);
    secp256k1_fe_verify(a);
#endif
    secp256k1_fe_sqr_inner(r->n, a->n);
#ifdef VERIFY
    r->magnitude = 1;
    r->normalized = 0;
    secp256k1_fe_verify(r);
#endif
}

static SECP256K1_INLINE void secp256k1_fe_cmov(secp256k1_fe *r, const secp256k1_fe *a, int flag) {
    uint32_t mask0, mask1;
    mask0 = flag + ~((uint32_t)0);
    mask1 = ~mask0;
    r->n[0] = (r->n[0] & mask0) | (a->n[0] & mask1);
    r->n[1] = (r->n[1] & mask0) | (a->n[1] & mask1);
    r->n[2] = (r->n[2] & mask0) | (a->n[2] & mask1);
    r->n[3] = (r->n[3] & mask0) | (a->n[3] & mask1);
    r->n[4] = (r->n[4] & mask0) | (a->n[4] & mask1);
    r->n[5] = (r->n[5] & mask0) | (a->n[5] & mask1);
    r->n[6] = (r->n[6] & mask0) | (a->n[6] & mask1);
    r->n[7] = (r->n[7] & mask0) | (a->n[7] & mask1);
    r->n[8] = (r->n[8] & mask0) | (a->n[8] & mask1);
    r->n[9] = (r->n[9] & mask0) | (a->n[9] & mask1);
#ifdef VERIFY
    if (a->magnitude > r->magnitude) {
        r->magnitude = a->magnitude;
    }
    r->normalized &= a->normalized;
#endif
}

static SECP256K1_INLINE void secp256k1_fe_storage_cmov(secp256k1_fe_storage *r, const secp256k1_fe_storage *a, int flag) {
    uint32_t mask0, mask1;
    mask0 = flag + ~((uint32_t)0);
    mask1 = ~mask0;
    r->n[0] = (r->n[0] & mask0) | (a->n[0] & mask1);
    r->n[1] = (r->n[1] & mask0) | (a->n[1] & mask1);
    r->n[2] = (r->n[2] & mask0) | (a->n[2] & mask1);
    r->n[3] = (r->n[3] & mask0) | (a->n[3] & mask1);
    r->n[4] = (r->n[4] & mask0) | (a->n[4] & mask1);
    r->n[5] = (r->n[5] & mask0) | (a->n[5] & mask1);
    r->n[6] = (r->n[6] & mask0) | (a->n[6] & mask1);
    r->n[7] = (r->n[7] & mask0) | (a->n[7] & mask1);
}

static void secp256k1_fe_to_storage(secp256k1_fe_storage *r, const secp256k1_fe *a) {
#ifdef VERIFY
    VERIFY_CHECK(a->normalized);
#endif
    r->n[0] = a->n[0] | a->n[1] << 26;
    r->n[1] = a->n[1] >> 6 | a->n[2] << 20;
    r->n[2] = a->n[2] >> 12 | a->n[3] << 14;
    r->n[3] = a->n[3] >> 18 | a->n[4] << 8;
    r->n[4] = a->n[4] >> 24 | a->n[5] << 2 | a->n[6] << 28;
    r->n[5] = a->n[6] >> 4 | a->n[7] << 22;
    r->n[6] = a->n[7] >> 10 | a->n[8] << 16;
    r->n[7] = a->n[8] >> 16 | a->n[9] << 10;
}

static SECP256K1_INLINE void secp256k1_fe_from_storage(secp256k1_fe *r, const secp256k1_fe_storage *a) {
    r->n[0] = a->n[0] & 0x3FFFFFFUL;
    r->n[1] = a->n[0] >> 26 | ((a->n[1] << 6) & 0x3FFFFFFUL);
    r->n[2] = a->n[1] >> 20 | ((a->n[2] << 12) & 0x3FFFFFFUL);
    r->n[3] = a->n[2] >> 14 | ((a->n[3] << 18) & 0x3FFFFFFUL);
    r->n[4] = a->n[3] >> 8 | ((a->n[4] << 24) & 0x3FFFFFFUL);
    r->n[5] = (a->n[4] >> 2) & 0x3FFFFFFUL;
    r->n[6] = a->n[4] >> 28 | ((a->n[5] << 4) & 0x3FFFFFFUL);
    r->n[7] = a->n[5] >> 22 | ((a->n[6] << 10) & 0x3FFFFFFUL);
    r->n[8] = a->n[6] >> 16 | ((a->n[7] << 16) & 0x3FFFFFFUL);
    r->n[9] = a->n[7] >> 10;
#ifdef VERIFY
    r->magnitude = 1;
    r->normalized = 1;
#endif
}

/*dif/*secp256k1_field_repr_impl_h*/
