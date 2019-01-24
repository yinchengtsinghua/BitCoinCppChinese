
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


#ifndef SECP256K1_SCALAR_REPR_IMPL_H
#define SECP256K1_SCALAR_REPR_IMPL_H

/*secp256k1顺序的分支。*/
#define SECP256K1_N_0 ((uint64_t)0xBFD25E8CD0364141ULL)
#define SECP256K1_N_1 ((uint64_t)0xBAAEDCE6AF48A03BULL)
#define SECP256K1_N_2 ((uint64_t)0xFFFFFFFFFFFFFFFEULL)
#define SECP256K1_N_3 ((uint64_t)0xFFFFFFFFFFFFFFFFULL)

/*2^256的四肢减去secp256k1顺序。*/
#define SECP256K1_N_C_0 (~SECP256K1_N_0 + 1)
#define SECP256K1_N_C_1 (~SECP256K1_N_1)
#define SECP256K1_N_C_2 (1)

/*Secp256k1阶的一半分支。*/
#define SECP256K1_N_H_0 ((uint64_t)0xDFE92F46681B20A0ULL)
#define SECP256K1_N_H_1 ((uint64_t)0x5D576E7357A4501DULL)
#define SECP256K1_N_H_2 ((uint64_t)0xFFFFFFFFFFFFFFFFULL)
#define SECP256K1_N_H_3 ((uint64_t)0x7FFFFFFFFFFFFFFFULL)

SECP256K1_INLINE static void secp256k1_scalar_clear(secp256k1_scalar *r) {
    r->d[0] = 0;
    r->d[1] = 0;
    r->d[2] = 0;
    r->d[3] = 0;
}

SECP256K1_INLINE static void secp256k1_scalar_set_int(secp256k1_scalar *r, unsigned int v) {
    r->d[0] = v;
    r->d[1] = 0;
    r->d[2] = 0;
    r->d[3] = 0;
}

SECP256K1_INLINE static unsigned int secp256k1_scalar_get_bits(const secp256k1_scalar *a, unsigned int offset, unsigned int count) {
    VERIFY_CHECK((offset + count - 1) >> 6 == offset >> 6);
    return (a->d[offset >> 6] >> (offset & 0x3F)) & ((((uint64_t)1) << count) - 1);
}

SECP256K1_INLINE static unsigned int secp256k1_scalar_get_bits_var(const secp256k1_scalar *a, unsigned int offset, unsigned int count) {
    VERIFY_CHECK(count < 32);
    VERIFY_CHECK(offset + count <= 256);
    if ((offset + count - 1) >> 6 == offset >> 6) {
        return secp256k1_scalar_get_bits(a, offset, count);
    } else {
        VERIFY_CHECK((offset >> 6) + 1 < 4);
        return ((a->d[offset >> 6] >> (offset & 0x3F)) | (a->d[(offset >> 6) + 1] << (64 - (offset & 0x3F)))) & ((((uint64_t)1) << count) - 1);
    }
}

SECP256K1_INLINE static int secp256k1_scalar_check_overflow(const secp256k1_scalar *a) {
    int yes = 0;
    int no = 0;
    /*=（a->d[3]<secp256k1_n_3）；/*无需进行>检查。*/
    否=（a->d[2]<secp256k1_n_2）；
    是=（a->d[2]>secp256k1_n_2）&~no；
    否=（a->d[1]<secp256k1_n_1）；
    是=（a->d[1]>secp256k1_n_1）&~no；
    是=（a->d[0]>=secp256k1_n_0）&~no；
    返回是的；
}

secp256k1_inline static int secp256k1_scalar_reduce（secp256k1_scalar*r，无符号int溢出）
    uTN128T T；
    验证_check（溢出<=1）；
    t=（uint128_t）r->d[0]+溢出*secp256k1_n_c_0；
    r->d[0]=t&0xffffffffffffffffull；t>>=64；
    t+=（uint128_t）r->d[1]+溢出*secp256k1_n_c_1；
    r->d[1]=t&0xffffffffffffffffull；t>>=64；
    t+=（uint128_t）r->d[2]+溢出*secp256k1_n_c_2；
    r->d[2]=t&0xffffffffffffffffull；t>>=64；
    t+=（uint64_t）r->d[3]；
    r->d[3]=t&0xfffffffffffffffffull；
    返回溢出；
}

静态int secp256k1_scalar_add（secp256k1_scalar*r，const secp256k1_scalar*a，const secp256k1_scalar*b）
    int溢出；
    uint128_t=（uint128_t）a->d[0]+b->d[0]；
    r->d[0]=t&0xffffffffffffffffull；t>>=64；
    t+=（uint128_t）a->d[1]+b->d[1]；
    r->d[1]=t&0xffffffffffffffffull；t>>=64；
    t+=（uint128_t）a->d[2]+b->d[2]；
    r->d[2]=t&0xffffffffffffffffull；t>>=64；
    t+=（uint128_t）a->d[3]+b->d[3]；
    r->d[3]=t&0xffffffffffffffffull；t>>=64；
    overflow=t+secp256k1_scalar_check_overflow（r）；
    验证_check（overflow==0 overflow==1）；
    secp256k1_scalar_reduce（r，溢出）；
    返回溢出；
}

静态void secp256k1_scalar_cadd_bit（secp256k1_scalar*r，unsigned int bit，int flag）
    uTN128T T；
    验证检查（位<256）；
    bit+=（（uint32_t）标志-1）&0x100；/*强制（bit>>6）>3使其成为noop*/

    t = (uint128_t)r->d[0] + (((uint64_t)((bit >> 6) == 0)) << (bit & 0x3F));
    r->d[0] = t & 0xFFFFFFFFFFFFFFFFULL; t >>= 64;
    t += (uint128_t)r->d[1] + (((uint64_t)((bit >> 6) == 1)) << (bit & 0x3F));
    r->d[1] = t & 0xFFFFFFFFFFFFFFFFULL; t >>= 64;
    t += (uint128_t)r->d[2] + (((uint64_t)((bit >> 6) == 2)) << (bit & 0x3F));
    r->d[2] = t & 0xFFFFFFFFFFFFFFFFULL; t >>= 64;
    t += (uint128_t)r->d[3] + (((uint64_t)((bit >> 6) == 3)) << (bit & 0x3F));
    r->d[3] = t & 0xFFFFFFFFFFFFFFFFULL;
#ifdef VERIFY
    VERIFY_CHECK((t >> 64) == 0);
    VERIFY_CHECK(secp256k1_scalar_check_overflow(r) == 0);
#endif
}

static void secp256k1_scalar_set_b32(secp256k1_scalar *r, const unsigned char *b32, int *overflow) {
    int over;
    r->d[0] = (uint64_t)b32[31] | (uint64_t)b32[30] << 8 | (uint64_t)b32[29] << 16 | (uint64_t)b32[28] << 24 | (uint64_t)b32[27] << 32 | (uint64_t)b32[26] << 40 | (uint64_t)b32[25] << 48 | (uint64_t)b32[24] << 56;
    r->d[1] = (uint64_t)b32[23] | (uint64_t)b32[22] << 8 | (uint64_t)b32[21] << 16 | (uint64_t)b32[20] << 24 | (uint64_t)b32[19] << 32 | (uint64_t)b32[18] << 40 | (uint64_t)b32[17] << 48 | (uint64_t)b32[16] << 56;
    r->d[2] = (uint64_t)b32[15] | (uint64_t)b32[14] << 8 | (uint64_t)b32[13] << 16 | (uint64_t)b32[12] << 24 | (uint64_t)b32[11] << 32 | (uint64_t)b32[10] << 40 | (uint64_t)b32[9] << 48 | (uint64_t)b32[8] << 56;
    r->d[3] = (uint64_t)b32[7] | (uint64_t)b32[6] << 8 | (uint64_t)b32[5] << 16 | (uint64_t)b32[4] << 24 | (uint64_t)b32[3] << 32 | (uint64_t)b32[2] << 40 | (uint64_t)b32[1] << 48 | (uint64_t)b32[0] << 56;
    over = secp256k1_scalar_reduce(r, secp256k1_scalar_check_overflow(r));
    if (overflow) {
        *overflow = over;
    }
}

static void secp256k1_scalar_get_b32(unsigned char *bin, const secp256k1_scalar* a) {
    bin[0] = a->d[3] >> 56; bin[1] = a->d[3] >> 48; bin[2] = a->d[3] >> 40; bin[3] = a->d[3] >> 32; bin[4] = a->d[3] >> 24; bin[5] = a->d[3] >> 16; bin[6] = a->d[3] >> 8; bin[7] = a->d[3];
    bin[8] = a->d[2] >> 56; bin[9] = a->d[2] >> 48; bin[10] = a->d[2] >> 40; bin[11] = a->d[2] >> 32; bin[12] = a->d[2] >> 24; bin[13] = a->d[2] >> 16; bin[14] = a->d[2] >> 8; bin[15] = a->d[2];
    bin[16] = a->d[1] >> 56; bin[17] = a->d[1] >> 48; bin[18] = a->d[1] >> 40; bin[19] = a->d[1] >> 32; bin[20] = a->d[1] >> 24; bin[21] = a->d[1] >> 16; bin[22] = a->d[1] >> 8; bin[23] = a->d[1];
    bin[24] = a->d[0] >> 56; bin[25] = a->d[0] >> 48; bin[26] = a->d[0] >> 40; bin[27] = a->d[0] >> 32; bin[28] = a->d[0] >> 24; bin[29] = a->d[0] >> 16; bin[30] = a->d[0] >> 8; bin[31] = a->d[0];
}

SECP256K1_INLINE static int secp256k1_scalar_is_zero(const secp256k1_scalar *a) {
    return (a->d[0] | a->d[1] | a->d[2] | a->d[3]) == 0;
}

static void secp256k1_scalar_negate(secp256k1_scalar *r, const secp256k1_scalar *a) {
    uint64_t nonzero = 0xFFFFFFFFFFFFFFFFULL * (secp256k1_scalar_is_zero(a) == 0);
    uint128_t t = (uint128_t)(~a->d[0]) + SECP256K1_N_0 + 1;
    r->d[0] = t & nonzero; t >>= 64;
    t += (uint128_t)(~a->d[1]) + SECP256K1_N_1;
    r->d[1] = t & nonzero; t >>= 64;
    t += (uint128_t)(~a->d[2]) + SECP256K1_N_2;
    r->d[2] = t & nonzero; t >>= 64;
    t += (uint128_t)(~a->d[3]) + SECP256K1_N_3;
    r->d[3] = t & nonzero;
}

SECP256K1_INLINE static int secp256k1_scalar_is_one(const secp256k1_scalar *a) {
    return ((a->d[0] ^ 1) | a->d[1] | a->d[2] | a->d[3]) == 0;
}

static int secp256k1_scalar_is_high(const secp256k1_scalar *a) {
    int yes = 0;
    int no = 0;
    no |= (a->d[3] < SECP256K1_N_H_3);
    yes |= (a->d[3] > SECP256K1_N_H_3) & ~no;
    /*=（a->d[2]<secp256k1_n_h_2）&~是；/*无需进行>检查。*/
    否=（a->d[1]<secp256k1_n_h_1）&~是；
    是=（a->d[1]>secp256k1_n_h_1）&~no；
    是=（a->d[0]>secp256k1_n_h_0）&~no；
    返回是的；
}

静态int secp256k1_scalar_cond_negate（secp256k1_scalar*r，int flag）
    /*如果我们是flag=0，mask=00…00，这是一个no op；
     *如果我们是flag=1，mask=11…11，这与secp256k1_scalar_negate相同。*/

    uint64_t mask = !flag - 1;
    uint64_t nonzero = (secp256k1_scalar_is_zero(r) != 0) - 1;
    uint128_t t = (uint128_t)(r->d[0] ^ mask) + ((SECP256K1_N_0 + 1) & mask);
    r->d[0] = t & nonzero; t >>= 64;
    t += (uint128_t)(r->d[1] ^ mask) + (SECP256K1_N_1 & mask);
    r->d[1] = t & nonzero; t >>= 64;
    t += (uint128_t)(r->d[2] ^ mask) + (SECP256K1_N_2 & mask);
    r->d[2] = t & nonzero; t >>= 64;
    t += (uint128_t)(r->d[3] ^ mask) + (SECP256K1_N_3 & mask);
    r->d[3] = t & nonzero;
    return 2 * (mask == 0) - 1;
}

/*灵感来源于openssl的crypto/bn/asm/x8664-gcc.c中的宏。*/

/*在（c0，c1，c2）定义的数字上加上a*b。C2不得溢出。*/
#define muladd(a,b) { \
    uint64_t tl, th; \
    { \
        uint128_t t = (uint128_t)a * b; \
        /*=t>>64；/*最多为0xfffffffffffffe*/\
        tl＝t；
    }
    c0+=tl；/*溢出在下一行处理*/ \

    /*+=（C0<Tl）？1:0；/*至多为0xfffffffffffffffffffffffffffffffffffffffffffff*/\
    c1+=th；/*溢出在下一行处理*/ \

    /*+ =（C1＜Th）？1:0；/*从不因合同溢出（在下一行中验证）*/\
    验证_check（（c1>=th）（c2！= 0）；
}

/**在（c0，c1）定义的数字上加上a*b。C1不得溢出。*/

#define muladd_fast(a,b) { \
    uint64_t tl, th; \
    { \
        uint128_t t = (uint128_t)a * b; \
        /*=t>>64；/*最多为0xfffffffffffffe*/\
        tl＝t；
    }
    c0+=tl；/*溢出在下一行处理*/ \

    /*+=（C0<Tl）？1:0；/*至多为0xfffffffffffffffffffffffffffffffffffffffffffff*/\
    c1+=th；/*决不因合同而溢出（在下一行中验证）*/ \

    VERIFY_CHECK(c1 >= th); \
}

/*在（c0，c1，c2）定义的数字上加上2*a*b。C2不得溢出。*/
#define muladd2(a,b) { \
    uint64_t tl, th, th2, tl2; \
    { \
        uint128_t t = (uint128_t)a * b; \
        /*=t>>64；/*最多为0xfffffffffffffe*/\
        tl＝t；
    }
    th2=th+th；/*最多为0xfffffffffffffe（如果th为0x7fffffffffffffffffff）*/ \

    /*+=（th2<th）？1:0；/*从不因合同溢出（已验证下一行）*/\
    验证_check（（th2>=th）（c2！= 0）；
    tl2=tl+tl；/*最多为0xfffffffffffffe（如果tl的最低63位为0x7fffffffffffffffff）*/ \

    /*+=（tl2<tl）？1:0；/*至多为0xfffffffffffffffffffffffffffffffffffffffffffff*/\
    c0+=tl2；/*溢出在下一行处理*/ \

    /*+=（c0<tl2）？1:0；/*第二个溢出在下一行处理*/\
    c2+=（c0<tl2）&（th2==0）；/*合同从不溢出（验证下一行）*/ \

    VERIFY_CHECK((c0 >= tl2) || (th2 != 0) || (c2 != 0)); \
    /*+=th2；/*溢出在下一行处理*/\
    c2+=（c1<th2）？1:0；/*从不因合同溢出（已验证下一行）*/ \

    VERIFY_CHECK((c1 >= th2) || (c2 != 0)); \
}

/*在（c0，c1，c2）定义的数字上加上a。C2不得溢出。*/
#define sumadd(a) { \
    unsigned int over; \
    /*+=（a）；/*溢出在下一行处理*/\
    过量=（c0<（a））？1：0；
    c1+=超过；/*溢出在下一行处理*/ \

    /*+=（c1<以上）？1:0；/*决不因合同而溢出*/\
}

/**在（c0，c1）定义的数字上加上a。c1不能溢出，c2必须为零。*/

#define sumadd_fast(a) { \
    /*+=（a）；/*溢出在下一行处理*/\
    c1+=（c0<（a））？1:0；/*从不因合同溢出（已验证下一行）*/ \

    VERIFY_CHECK((c1 != 0) | (c0 >= (a))); \
    VERIFY_CHECK(c2 == 0); \
}

/*将（c0、c1、c2）的最低64位提取到n中，并左移数字64位。*/
#define extract(n) { \
    (n) = c0; \
    c0 = c1; \
    c1 = c2; \
    c2 = 0; \
}

/*将（c0、c1、c2）的最低64位提取到n中，并左移数字64位。c2必须为零。*/
#define extract_fast(n) { \
    (n) = c0; \
    c0 = c1; \
    c1 = 0; \
    VERIFY_CHECK(c2 == 0); \
}

static void secp256k1_scalar_reduce_512(secp256k1_scalar *r, const uint64_t *l) {
#ifdef USE_ASM_X86_64
    /*将512位减少到385位。*/
    uint64_t m0, m1, m2, m3, m4, m5, m6;
    uint64_t p0, p1, p2, p3, p4;
    uint64_t c;

    __asm__ __volatile__(
    /*预加载。*/
    "movq 32(%%rsi), %%r11\n"
    "movq 40(%%rsi), %%r12\n"
    "movq 48(%%rsi), %%r13\n"
    "movq 56(%%rsi), %%r14\n"
    /*初始化R8、R9、R10*/
    "movq 0(%%rsi), %%r8\n"
    "xorq %%r9, %%r9\n"
    "xorq %%r10, %%r10\n"
    /*（r8，r9）+=n0*c0*/
    "movq %8, %%rax\n"
    "mulq %%r11\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    /*M0提取*/
    "movq %%r8, %q0\n"
    "xorq %%r8, %%r8\n"
    /*（R9，R10）+L1*/
    "addq 8(%%rsi), %%r9\n"
    "adcq $0, %%r10\n"
    /*（R9、R10、R8）+=N1*C0*/
    "movq %8, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /*（R9、R10、R8）+=N0*C1*/
    "movq %9, %%rax\n"
    "mulq %%r11\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /*提取M1*/
    "movq %%r9, %q1\n"
    "xorq %%r9, %%r9\n"
    /*（R10、R8、R9）+=L2*/
    "addq 16(%%rsi), %%r10\n"
    "adcq $0, %%r8\n"
    "adcq $0, %%r9\n"
    /*（R10、R8、R9）+=n2*c0*/
    "movq %8, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /*（R10、R8、R9）+=N1*C1*/
    "movq %9, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /*（R10、R8、R9）+=N0*/
    "addq %%r11, %%r10\n"
    "adcq $0, %%r8\n"
    "adcq $0, %%r9\n"
    /*提取M2*/
    "movq %%r10, %q2\n"
    "xorq %%r10, %%r10\n"
    /*（R8、R9、R10）+=L3*/
    "addq 24(%%rsi), %%r8\n"
    "adcq $0, %%r9\n"
    "adcq $0, %%r10\n"
    /*（r8、r9、r10）+=n3*c0*/
    "movq %8, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /*（r8、r9、r10）+=n2*c1*/
    "movq %9, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /*（r8、r9、r10）+=n1*/
    "addq %%r12, %%r8\n"
    "adcq $0, %%r9\n"
    "adcq $0, %%r10\n"
    /*M3提取液*/
    "movq %%r8, %q3\n"
    "xorq %%r8, %%r8\n"
    /*（R9、R10、R8）+=N3*C1*/
    "movq %9, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /*（R9、R10、R8）+=N2*/
    "addq %%r13, %%r9\n"
    "adcq $0, %%r10\n"
    "adcq $0, %%r8\n"
    /*提取M4*/
    "movq %%r9, %q4\n"
    /*（R10，R8）+N3*/
    "addq %%r14, %%r10\n"
    "adcq $0, %%r8\n"
    /*提取M5*/
    "movq %%r10, %q5\n"
    /*提取M6*/
    "movq %%r8, %q6\n"
    : "=g"(m0), "=g"(m1), "=g"(m2), "=g"(m3), "=g"(m4), "=g"(m5), "=g"(m6)
    : "S"(l), "n"(SECP256K1_N_C_0), "n"(SECP256K1_N_C_1)
    : "rax", "rdx", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "cc");

    /*将385位减少到258位。*/
    __asm__ __volatile__(
    /*预载*/
    "movq %q9, %%r11\n"
    "movq %q10, %%r12\n"
    "movq %q11, %%r13\n"
    /*初始化（R8、R9、R10）*/
    "movq %q5, %%r8\n"
    "xorq %%r9, %%r9\n"
    "xorq %%r10, %%r10\n"
    /*（r8，r9）+=m4*c0*/
    "movq %12, %%rax\n"
    "mulq %%r11\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    /*提取P0*/
    "movq %%r8, %q0\n"
    "xorq %%r8, %%r8\n"
    /*（R9，R10）+M1*/
    "addq %q6, %%r9\n"
    "adcq $0, %%r10\n"
    /*（R9、R10、R8）+=M5*C0*/
    "movq %12, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /*（R9、R10、R8）+=M4*C1*/
    "movq %13, %%rax\n"
    "mulq %%r11\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /*P1萃取液*/
    "movq %%r9, %q1\n"
    "xorq %%r9, %%r9\n"
    /*（R10、R8、R9）+=m2*/
    "addq %q7, %%r10\n"
    "adcq $0, %%r8\n"
    "adcq $0, %%r9\n"
    /*（R10、R8、R9）+=M6*C0*/
    "movq %12, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /*（R10、R8、R9）+=M5*C1*/
    "movq %13, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /*（R10、R8、R9）+=M4*/
    "addq %%r11, %%r10\n"
    "adcq $0, %%r8\n"
    "adcq $0, %%r9\n"
    /*提取P2*/
    "movq %%r10, %q2\n"
    /*（R8，R9）+m3*/
    "addq %q8, %%r8\n"
    "adcq $0, %%r9\n"
    /*（r8，r9）+=m6*c1*/
    "movq %13, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    /*（R8，R9）+M5*/
    "addq %%r12, %%r8\n"
    "adcq $0, %%r9\n"
    /*萃取剂P3*/
    "movq %%r8, %q3\n"
    /*（R9）+= M6*/
    "addq %%r13, %%r9\n"
    /*提取P4*/
    "movq %%r9, %q4\n"
    : "=&g"(p0), "=&g"(p1), "=&g"(p2), "=g"(p3), "=g"(p4)
    : "g"(m0), "g"(m1), "g"(m2), "g"(m3), "g"(m4), "g"(m5), "g"(m6), "n"(SECP256K1_N_C_0), "n"(SECP256K1_N_C_1)
    : "rax", "rdx", "r8", "r9", "r10", "r11", "r12", "r13", "cc");

    /*将258位减为256位。*/
    __asm__ __volatile__(
    /*预载*/
    "movq %q5, %%r10\n"
    /*（rax，rdx）=p4*c0*/
    "movq %7, %%rax\n"
    "mulq %%r10\n"
    /*（RAX，RDX）+= P0*/
    "addq %q1, %%rax\n"
    "adcq $0, %%rdx\n"
    /*提取R0*/
    "movq %%rax, 0(%q6)\n"
    /*移动到（R8，R9）*/
    "movq %%rdx, %%r8\n"
    "xorq %%r9, %%r9\n"
    /*（R8，R9）+P1*/
    "addq %q2, %%r8\n"
    "adcq $0, %%r9\n"
    /*（R8、R9）+=P4*C1*/
    "movq %8, %%rax\n"
    "mulq %%r10\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    /*提取R1*/
    "movq %%r8, 8(%q6)\n"
    "xorq %%r8, %%r8\n"
    /*（R9，R8）+P4*/
    "addq %%r10, %%r9\n"
    "adcq $0, %%r8\n"
    /*（R9，R8）+P2*/
    "addq %q3, %%r9\n"
    "adcq $0, %%r8\n"
    /*提取R2*/
    "movq %%r9, 16(%q6)\n"
    "xorq %%r9, %%r9\n"
    /*（R8，R9）+P3*/
    "addq %q4, %%r8\n"
    "adcq $0, %%r9\n"
    /*提取R3*/
    "movq %%r8, 24(%q6)\n"
    /*提取C*/
    "movq %%r9, %q0\n"
    : "=g"(c)
    : "g"(p0), "g"(p1), "g"(p2), "g"(p3), "g"(p4), "D"(r), "n"(SECP256K1_N_C_0), "n"(SECP256K1_N_C_1)
    : "rax", "rdx", "r8", "r9", "r10", "cc", "memory");
#else
    uint128_t c;
    uint64_t c0, c1, c2;
    uint64_t n0 = l[4], n1 = l[5], n2 = l[6], n3 = l[7];
    uint64_t m0, m1, m2, m3, m4, m5;
    uint32_t m6;
    uint64_t p0, p1, p2, p3;
    uint32_t p4;

    /*将512位减少到385位。*/
    /*m[0..6]=l[0..3]+n[0..3]*secp256k1_n_c.*/
    c0 = l[0]; c1 = 0; c2 = 0;
    muladd_fast(n0, SECP256K1_N_C_0);
    extract_fast(m0);
    sumadd_fast(l[1]);
    muladd(n1, SECP256K1_N_C_0);
    muladd(n0, SECP256K1_N_C_1);
    extract(m1);
    sumadd(l[2]);
    muladd(n2, SECP256K1_N_C_0);
    muladd(n1, SECP256K1_N_C_1);
    sumadd(n0);
    extract(m2);
    sumadd(l[3]);
    muladd(n3, SECP256K1_N_C_0);
    muladd(n2, SECP256K1_N_C_1);
    sumadd(n1);
    extract(m3);
    muladd(n3, SECP256K1_N_C_1);
    sumadd(n2);
    extract(m4);
    sumadd_fast(n3);
    extract_fast(m5);
    VERIFY_CHECK(c0 <= 1);
    m6 = c0;

    /*将385位减少到258位。*/
    /*P[0..4]=M[0..3]+M[4..6]*secp256k1_n_c.*/
    c0 = m0; c1 = 0; c2 = 0;
    muladd_fast(m4, SECP256K1_N_C_0);
    extract_fast(p0);
    sumadd_fast(m1);
    muladd(m5, SECP256K1_N_C_0);
    muladd(m4, SECP256K1_N_C_1);
    extract(p1);
    sumadd(m2);
    muladd(m6, SECP256K1_N_C_0);
    muladd(m5, SECP256K1_N_C_1);
    sumadd(m4);
    extract(p2);
    sumadd_fast(m3);
    muladd_fast(m6, SECP256K1_N_C_1);
    sumadd_fast(m5);
    extract_fast(p3);
    p4 = c0 + m6;
    VERIFY_CHECK(p4 <= 2);

    /*将258位减为256位。*/
    /*R[0..3]=P[0..3]+P[4]*secp256k1_n_c.*/
    c = p0 + (uint128_t)SECP256K1_N_C_0 * p4;
    r->d[0] = c & 0xFFFFFFFFFFFFFFFFULL; c >>= 64;
    c += p1 + (uint128_t)SECP256K1_N_C_1 * p4;
    r->d[1] = c & 0xFFFFFFFFFFFFFFFFULL; c >>= 64;
    c += p2 + (uint128_t)p4;
    r->d[2] = c & 0xFFFFFFFFFFFFFFFFULL; c >>= 64;
    c += p3;
    r->d[3] = c & 0xFFFFFFFFFFFFFFFFULL; c >>= 64;
#endif

    /*最终还原R。*/
    secp256k1_scalar_reduce(r, c + secp256k1_scalar_check_overflow(r));
}

static void secp256k1_scalar_mul_512(uint64_t l[8], const secp256k1_scalar *a, const secp256k1_scalar *b) {
#ifdef USE_ASM_X86_64
    const uint64_t *pb = b->d;
    __asm__ __volatile__(
    /*预载*/
    "movq 0(%%rdi), %%r15\n"
    "movq 8(%%rdi), %%rbx\n"
    "movq 16(%%rdi), %%rcx\n"
    "movq 0(%%rdx), %%r11\n"
    "movq 8(%%rdx), %%r12\n"
    "movq 16(%%rdx), %%r13\n"
    "movq 24(%%rdx), %%r14\n"
    /*（rax，rdx）=a0*b0*/
    "movq %%r15, %%rax\n"
    "mulq %%r11\n"
    /*L0提取*/
    "movq %%rax, 0(%%rsi)\n"
    /*（R8、R9、R10）=（RDX）*/
    "movq %%rdx, %%r8\n"
    "xorq %%r9, %%r9\n"
    "xorq %%r10, %%r10\n"
    /*（R8、R9、R10）+=a0*b1*/
    "movq %%r15, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /*（R8、R9、R10）+=A1*B0*/
    "movq %%rbx, %%rax\n"
    "mulq %%r11\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /*提取L1*/
    "movq %%r8, 8(%%rsi)\n"
    "xorq %%r8, %%r8\n"
    /*（R9、R10、R8）+=a0*b2*/
    "movq %%r15, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /*（R9、R10、R8）+=A1*B1*/
    "movq %%rbx, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /*（R9、R10、R8）+=A2*B0*/
    "movq %%rcx, %%rax\n"
    "mulq %%r11\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /*L2提取*/
    "movq %%r9, 16(%%rsi)\n"
    "xorq %%r9, %%r9\n"
    /*（R10、R8、R9）+=a0*b3*/
    "movq %%r15, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /*预载A3*/
    "movq 24(%%rdi), %%r15\n"
    /*（R10、R8、R9）+=A1*B2*/
    "movq %%rbx, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /*（R10、R8、R9）+=A2*B1*/
    "movq %%rcx, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /*（R10、R8、R9）+=A3*B0*/
    "movq %%r15, %%rax\n"
    "mulq %%r11\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /*萃取液L3*/
    "movq %%r10, 24(%%rsi)\n"
    "xorq %%r10, %%r10\n"
    /*（R8、R9、R10）+=A1*B3*/
    "movq %%rbx, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /*（R8、R9、R10）+=A2*B2*/
    "movq %%rcx, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /*（R8、R9、R10）+=A3*B1*/
    "movq %%r15, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /*L4提取液*/
    "movq %%r8, 32(%%rsi)\n"
    "xorq %%r8, %%r8\n"
    /*（R9、R10、R8）+=A2*B3*/
    "movq %%rcx, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /*（R9、R10、R8）+=A3*B2*/
    "movq %%r15, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /*L5提取液*/
    "movq %%r9, 40(%%rsi)\n"
    /*（R10，R8）+=A3*B3*/
    "movq %%r15, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    /*L6提取液*/
    "movq %%r10, 48(%%rsi)\n"
    /*L7提取*/
    "movq %%r8, 56(%%rsi)\n"
    : "+d"(pb)
    : "S"(l), "D"(a->d)
    : "rax", "rbx", "rcx", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "cc", "memory");
#else
    /*160位累加器。*/
    uint64_t c0 = 0, c1 = 0;
    uint32_t c2 = 0;

    /*L[0..7]=A[0..3]*B[0..3]。*/
    muladd_fast(a->d[0], b->d[0]);
    extract_fast(l[0]);
    muladd(a->d[0], b->d[1]);
    muladd(a->d[1], b->d[0]);
    extract(l[1]);
    muladd(a->d[0], b->d[2]);
    muladd(a->d[1], b->d[1]);
    muladd(a->d[2], b->d[0]);
    extract(l[2]);
    muladd(a->d[0], b->d[3]);
    muladd(a->d[1], b->d[2]);
    muladd(a->d[2], b->d[1]);
    muladd(a->d[3], b->d[0]);
    extract(l[3]);
    muladd(a->d[1], b->d[3]);
    muladd(a->d[2], b->d[2]);
    muladd(a->d[3], b->d[1]);
    extract(l[4]);
    muladd(a->d[2], b->d[3]);
    muladd(a->d[3], b->d[2]);
    extract(l[5]);
    muladd_fast(a->d[3], b->d[3]);
    extract_fast(l[6]);
    VERIFY_CHECK(c1 == 0);
    l[7] = c0;
#endif
}

static void secp256k1_scalar_sqr_512(uint64_t l[8], const secp256k1_scalar *a) {
#ifdef USE_ASM_X86_64
    __asm__ __volatile__(
    /*预载*/
    "movq 0(%%rdi), %%r11\n"
    "movq 8(%%rdi), %%r12\n"
    "movq 16(%%rdi), %%r13\n"
    "movq 24(%%rdi), %%r14\n"
    /*（rax，rdx）=a0*a0*/
    "movq %%r11, %%rax\n"
    "mulq %%r11\n"
    /*L0提取*/
    "movq %%rax, 0(%%rsi)\n"
    /*（R8、R9、R10）=（RDX、0）*/
    "movq %%rdx, %%r8\n"
    "xorq %%r9, %%r9\n"
    "xorq %%r10, %%r10\n"
    /*（R8、R9、R10）+=2*a0*a1*/
    "movq %%r11, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /*提取L1*/
    "movq %%r8, 8(%%rsi)\n"
    "xorq %%r8, %%r8\n"
    /*（R9、R10、R8）+=2*a0*a2*/
    "movq %%r11, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /*（R9、R10、R8）+=A1*A1*/
    "movq %%r12, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /*L2提取*/
    "movq %%r9, 16(%%rsi)\n"
    "xorq %%r9, %%r9\n"
    /*（R10、R8、R9）+=2*a0*a3*/
    "movq %%r11, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /*（R10、R8、R9）+=2*A1*A2*/
    "movq %%r12, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /*萃取液L3*/
    "movq %%r10, 24(%%rsi)\n"
    "xorq %%r10, %%r10\n"
    /*（R8、R9、R10）+=2*A1*A3*/
    "movq %%r12, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /*（R8、R9、R10）+=A2*A2*/
    "movq %%r13, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /*L4提取液*/
    "movq %%r8, 32(%%rsi)\n"
    "xorq %%r8, %%r8\n"
    /*（R9、R10、R8）+=2*A2*A3*/
    "movq %%r13, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /*L5提取液*/
    "movq %%r9, 40(%%rsi)\n"
    /*（R10，R8）+=A3*A3*/
    "movq %%r14, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    /*L6提取液*/
    "movq %%r10, 48(%%rsi)\n"
    /*L7提取*/
    "movq %%r8, 56(%%rsi)\n"
    :
    : "S"(l), "D"(a->d)
    : "rax", "rdx", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "cc", "memory");
#else
    /*160位累加器。*/
    uint64_t c0 = 0, c1 = 0;
    uint32_t c2 = 0;

    /*L[0..7]=A[0..3]*B[0..3]。*/
    muladd_fast(a->d[0], a->d[0]);
    extract_fast(l[0]);
    muladd2(a->d[0], a->d[1]);
    extract(l[1]);
    muladd2(a->d[0], a->d[2]);
    muladd(a->d[1], a->d[1]);
    extract(l[2]);
    muladd2(a->d[0], a->d[3]);
    muladd2(a->d[1], a->d[2]);
    extract(l[3]);
    muladd2(a->d[1], a->d[3]);
    muladd(a->d[2], a->d[2]);
    extract(l[4]);
    muladd2(a->d[2], a->d[3]);
    extract(l[5]);
    muladd_fast(a->d[3], a->d[3]);
    extract_fast(l[6]);
    VERIFY_CHECK(c1 == 0);
    l[7] = c0;
#endif
}

#undef sumadd
#undef sumadd_fast
#undef muladd
#undef muladd_fast
#undef muladd2
#undef extract
#undef extract_fast

static void secp256k1_scalar_mul(secp256k1_scalar *r, const secp256k1_scalar *a, const secp256k1_scalar *b) {
    uint64_t l[8];
    secp256k1_scalar_mul_512(l, a, b);
    secp256k1_scalar_reduce_512(r, l);
}

static int secp256k1_scalar_shr_int(secp256k1_scalar *r, int n) {
    int ret;
    VERIFY_CHECK(n > 0);
    VERIFY_CHECK(n < 16);
    ret = r->d[0] & ((1 << n) - 1);
    r->d[0] = (r->d[0] >> n) + (r->d[1] << (64 - n));
    r->d[1] = (r->d[1] >> n) + (r->d[2] << (64 - n));
    r->d[2] = (r->d[2] >> n) + (r->d[3] << (64 - n));
    r->d[3] = (r->d[3] >> n);
    return ret;
}

static void secp256k1_scalar_sqr(secp256k1_scalar *r, const secp256k1_scalar *a) {
    uint64_t l[8];
    secp256k1_scalar_sqr_512(l, a);
    secp256k1_scalar_reduce_512(r, l);
}

#ifdef USE_ENDOMORPHISM
static void secp256k1_scalar_split_128(secp256k1_scalar *r1, secp256k1_scalar *r2, const secp256k1_scalar *a) {
    r1->d[0] = a->d[0];
    r1->d[1] = a->d[1];
    r1->d[2] = 0;
    r1->d[3] = 0;
    r2->d[0] = a->d[2];
    r2->d[1] = a->d[3];
    r2->d[2] = 0;
    r2->d[3] = 0;
}
#endif

SECP256K1_INLINE static int secp256k1_scalar_eq(const secp256k1_scalar *a, const secp256k1_scalar *b) {
    return ((a->d[0] ^ b->d[0]) | (a->d[1] ^ b->d[1]) | (a->d[2] ^ b->d[2]) | (a->d[3] ^ b->d[3])) == 0;
}

SECP256K1_INLINE static void secp256k1_scalar_mul_shift_var(secp256k1_scalar *r, const secp256k1_scalar *a, const secp256k1_scalar *b, unsigned int shift) {
    uint64_t l[8];
    unsigned int shiftlimbs;
    unsigned int shiftlow;
    unsigned int shifthigh;
    VERIFY_CHECK(shift >= 256);
    secp256k1_scalar_mul_512(l, a, b);
    shiftlimbs = shift >> 6;
    shiftlow = shift & 0x3F;
    shifthigh = 64 - shiftlow;
    r->d[0] = shift < 512 ? (l[0 + shiftlimbs] >> shiftlow | (shift < 448 && shiftlow ? (l[1 + shiftlimbs] << shifthigh) : 0)) : 0;
    r->d[1] = shift < 448 ? (l[1 + shiftlimbs] >> shiftlow | (shift < 384 && shiftlow ? (l[2 + shiftlimbs] << shifthigh) : 0)) : 0;
    r->d[2] = shift < 384 ? (l[2 + shiftlimbs] >> shiftlow | (shift < 320 && shiftlow ? (l[3 + shiftlimbs] << shifthigh) : 0)) : 0;
    r->d[3] = shift < 320 ? (l[3 + shiftlimbs] >> shiftlow) : 0;
    secp256k1_scalar_cadd_bit(r, 0, (l[(shift - 1) >> 6] >> ((shift - 1) & 0x3f)) & 1);
}

/*dif/*secp256k1_scalar_repr_impl_h*/
