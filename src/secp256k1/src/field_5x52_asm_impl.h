
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
/***************************************************************
 *版权所有（c）2013-2014 Diederik Huys，Pieter Wuille*
 *根据麻省理工学院软件许可证分发，请参阅随附的*
 *文件复制或http://www.opensource.org/licenses/mit license.php.*
 ***************************************************************/


/*
 ＊嫦娥歌：
 *2013年3月，Diederik Huys：原始版本
 *2014年11月，Pieter Wuille：更新为使用Peter Dettman的并行乘法算法
 *-2014年12月，Pieter Wuille：从Yasm转换为GCC内联组件
 **/


#ifndef SECP256K1_FIELD_INNER5X52_IMPL_H
#define SECP256K1_FIELD_INNER5X52_IMPL_H

SECP256K1_INLINE static void secp256k1_fe_mul_inner(uint64_t *r, const uint64_t *a, const uint64_t * SECP256K1_RESTRICT b) {
/*
 *寄存器：rdx:rax=乘法累加器
 *R9:R8=C
 *R15:Rcx=d
 *R10-R14=A0-A4
 *RBX=B
 *RDI=R
 *rsi=a/t？
 **/

  uint64_t tmp1, tmp2, tmp3;
__asm__ __volatile__(
    "movq 0(%%rsi),%%r10\n"
    "movq 8(%%rsi),%%r11\n"
    "movq 16(%%rsi),%%r12\n"
    "movq 24(%%rsi),%%r13\n"
    "movq 32(%%rsi),%%r14\n"

    /*d+a3*b0*/
    "movq 0(%%rbx),%%rax\n"
    "mulq %%r13\n"
    "movq %%rax,%%rcx\n"
    "movq %%rdx,%%r15\n"
    /*d+a2*b1*/
    "movq 8(%%rbx),%%rax\n"
    "mulq %%r12\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*d+a1*b2*/
    "movq 16(%%rbx),%%rax\n"
    "mulq %%r11\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*D＝A0*B3*/
    "movq 24(%%rbx),%%rax\n"
    "mulq %%r10\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*C＝A4*B4*/
    "movq 32(%%rbx),%%rax\n"
    "mulq %%r14\n"
    "movq %%rax,%%r8\n"
    "movq %%rdx,%%r9\n"
    /*D+=（C&M）*R*/
    "movq $0xfffffffffffff,%%rdx\n"
    "andq %%rdx,%%rax\n"
    "movq $0x1000003d10,%%rdx\n"
    "mulq %%rdx\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*C>>=52（%%R8专用）*/
    "shrdq $52,%%r9,%%r8\n"
    /*t3（tmp1）=D&M*/
    "movq %%rcx,%%rsi\n"
    "movq $0xfffffffffffff,%%rdx\n"
    "andq %%rdx,%%rsi\n"
    "movq %%rsi,%q1\n"
    /*d>＞52*/
    "shrdq $52,%%r15,%%rcx\n"
    "xorq %%r15,%%r15\n"
    /*d+a4*b0*/
    "movq 0(%%rbx),%%rax\n"
    "mulq %%r14\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*d+a3*b1*/
    "movq 8(%%rbx),%%rax\n"
    "mulq %%r13\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*d+a2*b2*/
    "movq 16(%%rbx),%%rax\n"
    "mulq %%r12\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*d+a1*b3*/
    "movq 24(%%rbx),%%rax\n"
    "mulq %%r11\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*d+a0*b4*/
    "movq 32(%%rbx),%%rax\n"
    "mulq %%r10\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*D+= C*R*/
    "movq %%r8,%%rax\n"
    "movq $0x1000003d10,%%rdx\n"
    "mulq %%rdx\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*t4=d&m（%%rsi）*/
    "movq %%rcx,%%rsi\n"
    "movq $0xfffffffffffff,%%rdx\n"
    "andq %%rdx,%%rsi\n"
    /*d>＞52*/
    "shrdq $52,%%r15,%%rcx\n"
    "xorq %%r15,%%r15\n"
    /*tx=t4>>48（tmp3）*/
    "movq %%rsi,%%rax\n"
    "shrq $48,%%rax\n"
    "movq %%rax,%q3\n"
    /*t4&=（m>>4）（tmp2）*/
    "movq $0xffffffffffff,%%rax\n"
    "andq %%rax,%%rsi\n"
    "movq %%rsi,%q2\n"
    /*C= A0*B0*/
    "movq 0(%%rbx),%%rax\n"
    "mulq %%r10\n"
    "movq %%rax,%%r8\n"
    "movq %%rdx,%%r9\n"
    /*d+a4*b1*/
    "movq 8(%%rbx),%%rax\n"
    "mulq %%r14\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*d+a3*b2*/
    "movq 16(%%rbx),%%rax\n"
    "mulq %%r13\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*d+a2*b3*/
    "movq 24(%%rbx),%%rax\n"
    "mulq %%r12\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*d+a1*b4*/
    "movq 32(%%rbx),%%rax\n"
    "mulq %%r11\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*U0=D&M（%%rsi）*/
    "movq %%rcx,%%rsi\n"
    "movq $0xfffffffffffff,%%rdx\n"
    "andq %%rdx,%%rsi\n"
    /*d>＞52*/
    "shrdq $52,%%r15,%%rcx\n"
    "xorq %%r15,%%r15\n"
    /*u0=（u0<<4）tx（%%rsi）*/
    "shlq $4,%%rsi\n"
    "movq %q3,%%rax\n"
    "orq %%rax,%%rsi\n"
    /*C+=U0*（R>>4）*/
    "movq $0x1000003d1,%%rax\n"
    "mulq %%rsi\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*R〔0〕＝C & m*/
    "movq %%r8,%%rax\n"
    "movq $0xfffffffffffff,%%rdx\n"
    "andq %%rdx,%%rax\n"
    "movq %%rax,0(%%rdi)\n"
    /*C>＞52*/
    "shrdq $52,%%r9,%%r8\n"
    "xorq %%r9,%%r9\n"
    /*C+= A1*B0*/
    "movq 0(%%rbx),%%rax\n"
    "mulq %%r11\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*C+= A0*B1*/
    "movq 8(%%rbx),%%rax\n"
    "mulq %%r10\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*d+a4*b2*/
    "movq 16(%%rbx),%%rax\n"
    "mulq %%r14\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*d+a3*b3*/
    "movq 24(%%rbx),%%rax\n"
    "mulq %%r13\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*d+a2*b4*/
    "movq 32(%%rbx),%%rax\n"
    "mulq %%r12\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*C+=（D&M）*R*/
    "movq %%rcx,%%rax\n"
    "movq $0xfffffffffffff,%%rdx\n"
    "andq %%rdx,%%rax\n"
    "movq $0x1000003d10,%%rdx\n"
    "mulq %%rdx\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*d>＞52*/
    "shrdq $52,%%r15,%%rcx\n"
    "xorq %%r15,%%r15\n"
    /*R〔1〕＝C & m*/
    "movq %%r8,%%rax\n"
    "movq $0xfffffffffffff,%%rdx\n"
    "andq %%rdx,%%rax\n"
    "movq %%rax,8(%%rdi)\n"
    /*C>＞52*/
    "shrdq $52,%%r9,%%r8\n"
    "xorq %%r9,%%r9\n"
    /*C+= A2*B0*/
    "movq 0(%%rbx),%%rax\n"
    "mulq %%r12\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*C+= A1*B1*/
    "movq 8(%%rbx),%%rax\n"
    "mulq %%r11\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*C+=a0*b2（上次使用的是%%r10=a0）*/
    "movq 16(%%rbx),%%rax\n"
    "mulq %%r10\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*获取t3（%%r10，覆盖a0），t4（%%rsi）*/
    "movq %q2,%%rsi\n"
    "movq %q1,%%r10\n"
    /*d+a4*b3*/
    "movq 24(%%rbx),%%rax\n"
    "mulq %%r14\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*d+a3*b4*/
    "movq 32(%%rbx),%%rax\n"
    "mulq %%r13\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r15\n"
    /*C+=（D&M）*R*/
    "movq %%rcx,%%rax\n"
    "movq $0xfffffffffffff,%%rdx\n"
    "andq %%rdx,%%rax\n"
    "movq $0x1000003d10,%%rdx\n"
    "mulq %%rdx\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*D>>=52（%%rcx仅限）*/
    "shrdq $52,%%r15,%%rcx\n"
    /*R〔2〕＝C & m*/
    "movq %%r8,%%rax\n"
    "movq $0xfffffffffffff,%%rdx\n"
    "andq %%rdx,%%rax\n"
    "movq %%rax,16(%%rdi)\n"
    /*C>＞52*/
    "shrdq $52,%%r9,%%r8\n"
    "xorq %%r9,%%r9\n"
    /*C+= T3*/
    "addq %%r10,%%r8\n"
    /*C+= D*R*/
    "movq %%rcx,%%rax\n"
    "movq $0x1000003d10,%%rdx\n"
    "mulq %%rdx\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*R〔3〕＝C & m*/
    "movq %%r8,%%rax\n"
    "movq $0xfffffffffffff,%%rdx\n"
    "andq %%rdx,%%rax\n"
    "movq %%rax,24(%%rdi)\n"
    /*C>>=52（%%R8专用）*/
    "shrdq $52,%%r9,%%r8\n"
    /*C+=t4（%%r8）*/
    "addq %%rsi,%%r8\n"
    /*R〔4〕＝C*/
    "movq %%r8,32(%%rdi)\n"
: "+S"(a), "=m"(tmp1), "=m"(tmp2), "=m"(tmp3)
: "b"(b), "D"(r)
: "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15", "cc", "memory"
);
}

SECP256K1_INLINE static void secp256k1_fe_sqr_inner(uint64_t *r, const uint64_t *a) {
/*
 *寄存器：rdx:rax=乘法累加器
 *R9:R8=C
 *Rcx:Rbx=d
 *R10-R14=A0-A4
 *R15=m（0xffffffffffffffff）
 *RDI=R
 *rsi=a/t？
 **/

  uint64_t tmp1, tmp2, tmp3;
__asm__ __volatile__(
    "movq 0(%%rsi),%%r10\n"
    "movq 8(%%rsi),%%r11\n"
    "movq 16(%%rsi),%%r12\n"
    "movq 24(%%rsi),%%r13\n"
    "movq 32(%%rsi),%%r14\n"
    "movq $0xfffffffffffff,%%r15\n"

    /*D＝（A0* 2）*A3*/
    "leaq (%%r10,%%r10,1),%%rax\n"
    "mulq %%r13\n"
    "movq %%rax,%%rbx\n"
    "movq %%rdx,%%rcx\n"
    /*D+=（A1*2）*A2*/
    "leaq (%%r11,%%r11,1),%%rax\n"
    "mulq %%r12\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    /*C＝A4*A4*/
    "movq %%r14,%%rax\n"
    "mulq %%r14\n"
    "movq %%rax,%%r8\n"
    "movq %%rdx,%%r9\n"
    /*D+=（C&M）*R*/
    "andq %%r15,%%rax\n"
    "movq $0x1000003d10,%%rdx\n"
    "mulq %%rdx\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    /*C>>=52（%%R8专用）*/
    "shrdq $52,%%r9,%%r8\n"
    /*t3（tmp1）=D&M*/
    "movq %%rbx,%%rsi\n"
    "andq %%r15,%%rsi\n"
    "movq %%rsi,%q1\n"
    /*d>＞52*/
    "shrdq $52,%%rcx,%%rbx\n"
    "xorq %%rcx,%%rcx\n"
    /*A4*＝2*/
    "addq %%r14,%%r14\n"
    /*d+a0*a4*/
    "movq %%r10,%%rax\n"
    "mulq %%r14\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    /*d+=（a1＊2）*a3*/
    "leaq (%%r11,%%r11,1),%%rax\n"
    "mulq %%r13\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    /*d+a2*a2*/
    "movq %%r12,%%rax\n"
    "mulq %%r12\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    /*D+= C*R*/
    "movq %%r8,%%rax\n"
    "movq $0x1000003d10,%%rdx\n"
    "mulq %%rdx\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    /*t4=d&m（%%rsi）*/
    "movq %%rbx,%%rsi\n"
    "andq %%r15,%%rsi\n"
    /*d>＞52*/
    "shrdq $52,%%rcx,%%rbx\n"
    "xorq %%rcx,%%rcx\n"
    /*tx=t4>>48（tmp3）*/
    "movq %%rsi,%%rax\n"
    "shrq $48,%%rax\n"
    "movq %%rax,%q3\n"
    /*t4&=（m>>4）（tmp2）*/
    "movq $0xffffffffffff,%%rax\n"
    "andq %%rax,%%rsi\n"
    "movq %%rsi,%q2\n"
    /*C= A0*A0*/
    "movq %%r10,%%rax\n"
    "mulq %%r10\n"
    "movq %%rax,%%r8\n"
    "movq %%rdx,%%r9\n"
    /*d+a1*a4*/
    "movq %%r11,%%rax\n"
    "mulq %%r14\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    /*D+=（A2*2）*A3*/
    "leaq (%%r12,%%r12,1),%%rax\n"
    "mulq %%r13\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    /*U0=D&M（%%rsi）*/
    "movq %%rbx,%%rsi\n"
    "andq %%r15,%%rsi\n"
    /*d>＞52*/
    "shrdq $52,%%rcx,%%rbx\n"
    "xorq %%rcx,%%rcx\n"
    /*u0=（u0<<4）tx（%%rsi）*/
    "shlq $4,%%rsi\n"
    "movq %q3,%%rax\n"
    "orq %%rax,%%rsi\n"
    /*C+=U0*（R>>4）*/
    "movq $0x1000003d1,%%rax\n"
    "mulq %%rsi\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*R〔0〕＝C & m*/
    "movq %%r8,%%rax\n"
    "andq %%r15,%%rax\n"
    "movq %%rax,0(%%rdi)\n"
    /*C>＞52*/
    "shrdq $52,%%r9,%%r8\n"
    "xorq %%r9,%%r9\n"
    /*A0*＝2*/
    "addq %%r10,%%r10\n"
    /*C+A0*A1*/
    "movq %%r10,%%rax\n"
    "mulq %%r11\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*d+a2*a4*/
    "movq %%r12,%%rax\n"
    "mulq %%r14\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    /*d+a3*a3*/
    "movq %%r13,%%rax\n"
    "mulq %%r13\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    /*C+=（D&M）*R*/
    "movq %%rbx,%%rax\n"
    "andq %%r15,%%rax\n"
    "movq $0x1000003d10,%%rdx\n"
    "mulq %%rdx\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*d>＞52*/
    "shrdq $52,%%rcx,%%rbx\n"
    "xorq %%rcx,%%rcx\n"
    /*R〔1〕＝C & m*/
    "movq %%r8,%%rax\n"
    "andq %%r15,%%rax\n"
    "movq %%rax,8(%%rdi)\n"
    /*C>＞52*/
    "shrdq $52,%%r9,%%r8\n"
    "xorq %%r9,%%r9\n"
    /*C+=a0*a2（上次使用的是%%r10）*/
    "movq %%r10,%%rax\n"
    "mulq %%r12\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*获取t3（%%r10，覆盖a0），t4（%%rsi）*/
    "movq %q2,%%rsi\n"
    "movq %q1,%%r10\n"
    /*C+A1*A1*/
    "movq %%r11,%%rax\n"
    "mulq %%r11\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*d+a3*a4*/
    "movq %%r13,%%rax\n"
    "mulq %%r14\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    /*C+=（D&M）*R*/
    "movq %%rbx,%%rax\n"
    "andq %%r15,%%rax\n"
    "movq $0x1000003d10,%%rdx\n"
    "mulq %%rdx\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*d>>=52（%%rbx仅限）*/
    "shrdq $52,%%rcx,%%rbx\n"
    /*R〔2〕＝C & m*/
    "movq %%r8,%%rax\n"
    "andq %%r15,%%rax\n"
    "movq %%rax,16(%%rdi)\n"
    /*C>＞52*/
    "shrdq $52,%%r9,%%r8\n"
    "xorq %%r9,%%r9\n"
    /*C+= T3*/
    "addq %%r10,%%r8\n"
    /*C+= D*R*/
    "movq %%rbx,%%rax\n"
    "movq $0x1000003d10,%%rdx\n"
    "mulq %%rdx\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%r9\n"
    /*R〔3〕＝C & m*/
    "movq %%r8,%%rax\n"
    "andq %%r15,%%rax\n"
    "movq %%rax,24(%%rdi)\n"
    /*C>>=52（%%R8专用）*/
    "shrdq $52,%%r9,%%r8\n"
    /*C+=t4（%%r8）*/
    "addq %%rsi,%%r8\n"
    /*R〔4〕＝C*/
    "movq %%r8,32(%%rdi)\n"
: "+S"(a), "=m"(tmp1), "=m"(tmp2), "=m"(tmp3)
: "D"(r)
: "%rax", "%rbx", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15", "cc", "memory"
);
}

/*dif/*secp256k1_field_inner5x52_impl_h*/
