
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2017比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。
//
//这是Intel从yasm代码转换为gcc扩展asm语法
//（此文件底部提供）。

#include <stdint.h>
#include <stdlib.h>

#if defined(__x86_64__) || defined(__amd64__)

namespace sha256_sse4
{
void Transform(uint32_t* s, const unsigned char* chunk, size_t blocks)
{
    static const uint32_t K256 alignas(16) [] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    };
    static const uint32_t FLIP_MASK alignas(16) [] = {0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f};
    static const uint32_t SHUF_00BA alignas(16) [] = {0x03020100, 0x0b0a0908, 0xffffffff, 0xffffffff};
    static const uint32_t SHUF_DC00 alignas(16) [] = {0xffffffff, 0xffffffff, 0x03020100, 0x0b0a0908};
    uint32_t a, b, c, d, f, g, h, y0, y1, y2;
    uint64_t tbl;
    uint64_t inp_end, inp;
    uint32_t xfer alignas(16) [4];

    __asm__ __volatile__(
        "shl    $0x6,%2;"
        "je     Ldone_hash_%=;"
        "add    %1,%2;"
        "mov    %2,%14;"
        "mov    (%0),%3;"
        "mov    0x4(%0),%4;"
        "mov    0x8(%0),%5;"
        "mov    0xc(%0),%6;"
        "mov    0x10(%0),%k2;"
        "mov    0x14(%0),%7;"
        "mov    0x18(%0),%8;"
        "mov    0x1c(%0),%9;"
        "movdqa %18,%%xmm12;"
        "movdqa %19,%%xmm10;"
        "movdqa %20,%%xmm11;"

        "Lloop0_%=:"
        "lea    %17,%13;"
        "movdqu (%1),%%xmm4;"
        "pshufb %%xmm12,%%xmm4;"
        "movdqu 0x10(%1),%%xmm5;"
        "pshufb %%xmm12,%%xmm5;"
        "movdqu 0x20(%1),%%xmm6;"
        "pshufb %%xmm12,%%xmm6;"
        "movdqu 0x30(%1),%%xmm7;"
        "pshufb %%xmm12,%%xmm7;"
        "mov    %1,%15;"
        "mov    $3,%1;"

        "Lloop1_%=:"
        "movdqa 0x0(%13),%%xmm9;"
        "paddd  %%xmm4,%%xmm9;"
        "movdqa %%xmm9,%16;"
        "movdqa %%xmm7,%%xmm0;"
        "mov    %k2,%10;"
        "ror    $0xe,%10;"
        "mov    %3,%11;"
        "palignr $0x4,%%xmm6,%%xmm0;"
        "ror    $0x9,%11;"
        "xor    %k2,%10;"
        "mov    %7,%12;"
        "ror    $0x5,%10;"
        "movdqa %%xmm5,%%xmm1;"
        "xor    %3,%11;"
        "xor    %8,%12;"
        "paddd  %%xmm4,%%xmm0;"
        "xor    %k2,%10;"
        "and    %k2,%12;"
        "ror    $0xb,%11;"
        "palignr $0x4,%%xmm4,%%xmm1;"
        "xor    %3,%11;"
        "ror    $0x6,%10;"
        "xor    %8,%12;"
        "movdqa %%xmm1,%%xmm2;"
        "ror    $0x2,%11;"
        "add    %10,%12;"
        "add    %16,%12;"
        "movdqa %%xmm1,%%xmm3;"
        "mov    %3,%10;"
        "add    %12,%9;"
        "mov    %3,%12;"
        "pslld  $0x19,%%xmm1;"
        "or     %5,%10;"
        "add    %9,%6;"
        "and    %5,%12;"
        "psrld  $0x7,%%xmm2;"
        "and    %4,%10;"
        "add    %11,%9;"
        "por    %%xmm2,%%xmm1;"
        "or     %12,%10;"
        "add    %10,%9;"
        "movdqa %%xmm3,%%xmm2;"
        "mov    %6,%10;"
        "mov    %9,%11;"
        "movdqa %%xmm3,%%xmm8;"
        "ror    $0xe,%10;"
        "xor    %6,%10;"
        "mov    %k2,%12;"
        "ror    $0x9,%11;"
        "pslld  $0xe,%%xmm3;"
        "xor    %9,%11;"
        "ror    $0x5,%10;"
        "xor    %7,%12;"
        "psrld  $0x12,%%xmm2;"
        "ror    $0xb,%11;"
        "xor    %6,%10;"
        "and    %6,%12;"
        "ror    $0x6,%10;"
        "pxor   %%xmm3,%%xmm1;"
        "xor    %9,%11;"
        "xor    %7,%12;"
        "psrld  $0x3,%%xmm8;"
        "add    %10,%12;"
        "add    4+%16,%12;"
        "ror    $0x2,%11;"
        "pxor   %%xmm2,%%xmm1;"
        "mov    %9,%10;"
        "add    %12,%8;"
        "mov    %9,%12;"
        "pxor   %%xmm8,%%xmm1;"
        "or     %4,%10;"
        "add    %8,%5;"
        "and    %4,%12;"
        "pshufd $0xfa,%%xmm7,%%xmm2;"
        "and    %3,%10;"
        "add    %11,%8;"
        "paddd  %%xmm1,%%xmm0;"
        "or     %12,%10;"
        "add    %10,%8;"
        "movdqa %%xmm2,%%xmm3;"
        "mov    %5,%10;"
        "mov    %8,%11;"
        "ror    $0xe,%10;"
        "movdqa %%xmm2,%%xmm8;"
        "xor    %5,%10;"
        "ror    $0x9,%11;"
        "mov    %6,%12;"
        "xor    %8,%11;"
        "ror    $0x5,%10;"
        "psrlq  $0x11,%%xmm2;"
        "xor    %k2,%12;"
        "psrlq  $0x13,%%xmm3;"
        "xor    %5,%10;"
        "and    %5,%12;"
        "psrld  $0xa,%%xmm8;"
        "ror    $0xb,%11;"
        "xor    %8,%11;"
        "xor    %k2,%12;"
        "ror    $0x6,%10;"
        "pxor   %%xmm3,%%xmm2;"
        "add    %10,%12;"
        "ror    $0x2,%11;"
        "add    8+%16,%12;"
        "pxor   %%xmm2,%%xmm8;"
        "mov    %8,%10;"
        "add    %12,%7;"
        "mov    %8,%12;"
        "pshufb %%xmm10,%%xmm8;"
        "or     %3,%10;"
        "add    %7,%4;"
        "and    %3,%12;"
        "paddd  %%xmm8,%%xmm0;"
        "and    %9,%10;"
        "add    %11,%7;"
        "pshufd $0x50,%%xmm0,%%xmm2;"
        "or     %12,%10;"
        "add    %10,%7;"
        "movdqa %%xmm2,%%xmm3;"
        "mov    %4,%10;"
        "ror    $0xe,%10;"
        "mov    %7,%11;"
        "movdqa %%xmm2,%%xmm4;"
        "ror    $0x9,%11;"
        "xor    %4,%10;"
        "mov    %5,%12;"
        "ror    $0x5,%10;"
        "psrlq  $0x11,%%xmm2;"
        "xor    %7,%11;"
        "xor    %6,%12;"
        "psrlq  $0x13,%%xmm3;"
        "xor    %4,%10;"
        "and    %4,%12;"
        "ror    $0xb,%11;"
        "psrld  $0xa,%%xmm4;"
        "xor    %7,%11;"
        "ror    $0x6,%10;"
        "xor    %6,%12;"
        "pxor   %%xmm3,%%xmm2;"
        "ror    $0x2,%11;"
        "add    %10,%12;"
        "add    12+%16,%12;"
        "pxor   %%xmm2,%%xmm4;"
        "mov    %7,%10;"
        "add    %12,%k2;"
        "mov    %7,%12;"
        "pshufb %%xmm11,%%xmm4;"
        "or     %9,%10;"
        "add    %k2,%3;"
        "and    %9,%12;"
        "paddd  %%xmm0,%%xmm4;"
        "and    %8,%10;"
        "add    %11,%k2;"
        "or     %12,%10;"
        "add    %10,%k2;"
        "movdqa 0x10(%13),%%xmm9;"
        "paddd  %%xmm5,%%xmm9;"
        "movdqa %%xmm9,%16;"
        "movdqa %%xmm4,%%xmm0;"
        "mov    %3,%10;"
        "ror    $0xe,%10;"
        "mov    %k2,%11;"
        "palignr $0x4,%%xmm7,%%xmm0;"
        "ror    $0x9,%11;"
        "xor    %3,%10;"
        "mov    %4,%12;"
        "ror    $0x5,%10;"
        "movdqa %%xmm6,%%xmm1;"
        "xor    %k2,%11;"
        "xor    %5,%12;"
        "paddd  %%xmm5,%%xmm0;"
        "xor    %3,%10;"
        "and    %3,%12;"
        "ror    $0xb,%11;"
        "palignr $0x4,%%xmm5,%%xmm1;"
        "xor    %k2,%11;"
        "ror    $0x6,%10;"
        "xor    %5,%12;"
        "movdqa %%xmm1,%%xmm2;"
        "ror    $0x2,%11;"
        "add    %10,%12;"
        "add    %16,%12;"
        "movdqa %%xmm1,%%xmm3;"
        "mov    %k2,%10;"
        "add    %12,%6;"
        "mov    %k2,%12;"
        "pslld  $0x19,%%xmm1;"
        "or     %8,%10;"
        "add    %6,%9;"
        "and    %8,%12;"
        "psrld  $0x7,%%xmm2;"
        "and    %7,%10;"
        "add    %11,%6;"
        "por    %%xmm2,%%xmm1;"
        "or     %12,%10;"
        "add    %10,%6;"
        "movdqa %%xmm3,%%xmm2;"
        "mov    %9,%10;"
        "mov    %6,%11;"
        "movdqa %%xmm3,%%xmm8;"
        "ror    $0xe,%10;"
        "xor    %9,%10;"
        "mov    %3,%12;"
        "ror    $0x9,%11;"
        "pslld  $0xe,%%xmm3;"
        "xor    %6,%11;"
        "ror    $0x5,%10;"
        "xor    %4,%12;"
        "psrld  $0x12,%%xmm2;"
        "ror    $0xb,%11;"
        "xor    %9,%10;"
        "and    %9,%12;"
        "ror    $0x6,%10;"
        "pxor   %%xmm3,%%xmm1;"
        "xor    %6,%11;"
        "xor    %4,%12;"
        "psrld  $0x3,%%xmm8;"
        "add    %10,%12;"
        "add    4+%16,%12;"
        "ror    $0x2,%11;"
        "pxor   %%xmm2,%%xmm1;"
        "mov    %6,%10;"
        "add    %12,%5;"
        "mov    %6,%12;"
        "pxor   %%xmm8,%%xmm1;"
        "or     %7,%10;"
        "add    %5,%8;"
        "and    %7,%12;"
        "pshufd $0xfa,%%xmm4,%%xmm2;"
        "and    %k2,%10;"
        "add    %11,%5;"
        "paddd  %%xmm1,%%xmm0;"
        "or     %12,%10;"
        "add    %10,%5;"
        "movdqa %%xmm2,%%xmm3;"
        "mov    %8,%10;"
        "mov    %5,%11;"
        "ror    $0xe,%10;"
        "movdqa %%xmm2,%%xmm8;"
        "xor    %8,%10;"
        "ror    $0x9,%11;"
        "mov    %9,%12;"
        "xor    %5,%11;"
        "ror    $0x5,%10;"
        "psrlq  $0x11,%%xmm2;"
        "xor    %3,%12;"
        "psrlq  $0x13,%%xmm3;"
        "xor    %8,%10;"
        "and    %8,%12;"
        "psrld  $0xa,%%xmm8;"
        "ror    $0xb,%11;"
        "xor    %5,%11;"
        "xor    %3,%12;"
        "ror    $0x6,%10;"
        "pxor   %%xmm3,%%xmm2;"
        "add    %10,%12;"
        "ror    $0x2,%11;"
        "add    8+%16,%12;"
        "pxor   %%xmm2,%%xmm8;"
        "mov    %5,%10;"
        "add    %12,%4;"
        "mov    %5,%12;"
        "pshufb %%xmm10,%%xmm8;"
        "or     %k2,%10;"
        "add    %4,%7;"
        "and    %k2,%12;"
        "paddd  %%xmm8,%%xmm0;"
        "and    %6,%10;"
        "add    %11,%4;"
        "pshufd $0x50,%%xmm0,%%xmm2;"
        "or     %12,%10;"
        "add    %10,%4;"
        "movdqa %%xmm2,%%xmm3;"
        "mov    %7,%10;"
        "ror    $0xe,%10;"
        "mov    %4,%11;"
        "movdqa %%xmm2,%%xmm5;"
        "ror    $0x9,%11;"
        "xor    %7,%10;"
        "mov    %8,%12;"
        "ror    $0x5,%10;"
        "psrlq  $0x11,%%xmm2;"
        "xor    %4,%11;"
        "xor    %9,%12;"
        "psrlq  $0x13,%%xmm3;"
        "xor    %7,%10;"
        "and    %7,%12;"
        "ror    $0xb,%11;"
        "psrld  $0xa,%%xmm5;"
        "xor    %4,%11;"
        "ror    $0x6,%10;"
        "xor    %9,%12;"
        "pxor   %%xmm3,%%xmm2;"
        "ror    $0x2,%11;"
        "add    %10,%12;"
        "add    12+%16,%12;"
        "pxor   %%xmm2,%%xmm5;"
        "mov    %4,%10;"
        "add    %12,%3;"
        "mov    %4,%12;"
        "pshufb %%xmm11,%%xmm5;"
        "or     %6,%10;"
        "add    %3,%k2;"
        "and    %6,%12;"
        "paddd  %%xmm0,%%xmm5;"
        "and    %5,%10;"
        "add    %11,%3;"
        "or     %12,%10;"
        "add    %10,%3;"
        "movdqa 0x20(%13),%%xmm9;"
        "paddd  %%xmm6,%%xmm9;"
        "movdqa %%xmm9,%16;"
        "movdqa %%xmm5,%%xmm0;"
        "mov    %k2,%10;"
        "ror    $0xe,%10;"
        "mov    %3,%11;"
        "palignr $0x4,%%xmm4,%%xmm0;"
        "ror    $0x9,%11;"
        "xor    %k2,%10;"
        "mov    %7,%12;"
        "ror    $0x5,%10;"
        "movdqa %%xmm7,%%xmm1;"
        "xor    %3,%11;"
        "xor    %8,%12;"
        "paddd  %%xmm6,%%xmm0;"
        "xor    %k2,%10;"
        "and    %k2,%12;"
        "ror    $0xb,%11;"
        "palignr $0x4,%%xmm6,%%xmm1;"
        "xor    %3,%11;"
        "ror    $0x6,%10;"
        "xor    %8,%12;"
        "movdqa %%xmm1,%%xmm2;"
        "ror    $0x2,%11;"
        "add    %10,%12;"
        "add    %16,%12;"
        "movdqa %%xmm1,%%xmm3;"
        "mov    %3,%10;"
        "add    %12,%9;"
        "mov    %3,%12;"
        "pslld  $0x19,%%xmm1;"
        "or     %5,%10;"
        "add    %9,%6;"
        "and    %5,%12;"
        "psrld  $0x7,%%xmm2;"
        "and    %4,%10;"
        "add    %11,%9;"
        "por    %%xmm2,%%xmm1;"
        "or     %12,%10;"
        "add    %10,%9;"
        "movdqa %%xmm3,%%xmm2;"
        "mov    %6,%10;"
        "mov    %9,%11;"
        "movdqa %%xmm3,%%xmm8;"
        "ror    $0xe,%10;"
        "xor    %6,%10;"
        "mov    %k2,%12;"
        "ror    $0x9,%11;"
        "pslld  $0xe,%%xmm3;"
        "xor    %9,%11;"
        "ror    $0x5,%10;"
        "xor    %7,%12;"
        "psrld  $0x12,%%xmm2;"
        "ror    $0xb,%11;"
        "xor    %6,%10;"
        "and    %6,%12;"
        "ror    $0x6,%10;"
        "pxor   %%xmm3,%%xmm1;"
        "xor    %9,%11;"
        "xor    %7,%12;"
        "psrld  $0x3,%%xmm8;"
        "add    %10,%12;"
        "add    4+%16,%12;"
        "ror    $0x2,%11;"
        "pxor   %%xmm2,%%xmm1;"
        "mov    %9,%10;"
        "add    %12,%8;"
        "mov    %9,%12;"
        "pxor   %%xmm8,%%xmm1;"
        "or     %4,%10;"
        "add    %8,%5;"
        "and    %4,%12;"
        "pshufd $0xfa,%%xmm5,%%xmm2;"
        "and    %3,%10;"
        "add    %11,%8;"
        "paddd  %%xmm1,%%xmm0;"
        "or     %12,%10;"
        "add    %10,%8;"
        "movdqa %%xmm2,%%xmm3;"
        "mov    %5,%10;"
        "mov    %8,%11;"
        "ror    $0xe,%10;"
        "movdqa %%xmm2,%%xmm8;"
        "xor    %5,%10;"
        "ror    $0x9,%11;"
        "mov    %6,%12;"
        "xor    %8,%11;"
        "ror    $0x5,%10;"
        "psrlq  $0x11,%%xmm2;"
        "xor    %k2,%12;"
        "psrlq  $0x13,%%xmm3;"
        "xor    %5,%10;"
        "and    %5,%12;"
        "psrld  $0xa,%%xmm8;"
        "ror    $0xb,%11;"
        "xor    %8,%11;"
        "xor    %k2,%12;"
        "ror    $0x6,%10;"
        "pxor   %%xmm3,%%xmm2;"
        "add    %10,%12;"
        "ror    $0x2,%11;"
        "add    8+%16,%12;"
        "pxor   %%xmm2,%%xmm8;"
        "mov    %8,%10;"
        "add    %12,%7;"
        "mov    %8,%12;"
        "pshufb %%xmm10,%%xmm8;"
        "or     %3,%10;"
        "add    %7,%4;"
        "and    %3,%12;"
        "paddd  %%xmm8,%%xmm0;"
        "and    %9,%10;"
        "add    %11,%7;"
        "pshufd $0x50,%%xmm0,%%xmm2;"
        "or     %12,%10;"
        "add    %10,%7;"
        "movdqa %%xmm2,%%xmm3;"
        "mov    %4,%10;"
        "ror    $0xe,%10;"
        "mov    %7,%11;"
        "movdqa %%xmm2,%%xmm6;"
        "ror    $0x9,%11;"
        "xor    %4,%10;"
        "mov    %5,%12;"
        "ror    $0x5,%10;"
        "psrlq  $0x11,%%xmm2;"
        "xor    %7,%11;"
        "xor    %6,%12;"
        "psrlq  $0x13,%%xmm3;"
        "xor    %4,%10;"
        "and    %4,%12;"
        "ror    $0xb,%11;"
        "psrld  $0xa,%%xmm6;"
        "xor    %7,%11;"
        "ror    $0x6,%10;"
        "xor    %6,%12;"
        "pxor   %%xmm3,%%xmm2;"
        "ror    $0x2,%11;"
        "add    %10,%12;"
        "add    12+%16,%12;"
        "pxor   %%xmm2,%%xmm6;"
        "mov    %7,%10;"
        "add    %12,%k2;"
        "mov    %7,%12;"
        "pshufb %%xmm11,%%xmm6;"
        "or     %9,%10;"
        "add    %k2,%3;"
        "and    %9,%12;"
        "paddd  %%xmm0,%%xmm6;"
        "and    %8,%10;"
        "add    %11,%k2;"
        "or     %12,%10;"
        "add    %10,%k2;"
        "movdqa 0x30(%13),%%xmm9;"
        "paddd  %%xmm7,%%xmm9;"
        "movdqa %%xmm9,%16;"
        "add    $0x40,%13;"
        "movdqa %%xmm6,%%xmm0;"
        "mov    %3,%10;"
        "ror    $0xe,%10;"
        "mov    %k2,%11;"
        "palignr $0x4,%%xmm5,%%xmm0;"
        "ror    $0x9,%11;"
        "xor    %3,%10;"
        "mov    %4,%12;"
        "ror    $0x5,%10;"
        "movdqa %%xmm4,%%xmm1;"
        "xor    %k2,%11;"
        "xor    %5,%12;"
        "paddd  %%xmm7,%%xmm0;"
        "xor    %3,%10;"
        "and    %3,%12;"
        "ror    $0xb,%11;"
        "palignr $0x4,%%xmm7,%%xmm1;"
        "xor    %k2,%11;"
        "ror    $0x6,%10;"
        "xor    %5,%12;"
        "movdqa %%xmm1,%%xmm2;"
        "ror    $0x2,%11;"
        "add    %10,%12;"
        "add    %16,%12;"
        "movdqa %%xmm1,%%xmm3;"
        "mov    %k2,%10;"
        "add    %12,%6;"
        "mov    %k2,%12;"
        "pslld  $0x19,%%xmm1;"
        "or     %8,%10;"
        "add    %6,%9;"
        "and    %8,%12;"
        "psrld  $0x7,%%xmm2;"
        "and    %7,%10;"
        "add    %11,%6;"
        "por    %%xmm2,%%xmm1;"
        "or     %12,%10;"
        "add    %10,%6;"
        "movdqa %%xmm3,%%xmm2;"
        "mov    %9,%10;"
        "mov    %6,%11;"
        "movdqa %%xmm3,%%xmm8;"
        "ror    $0xe,%10;"
        "xor    %9,%10;"
        "mov    %3,%12;"
        "ror    $0x9,%11;"
        "pslld  $0xe,%%xmm3;"
        "xor    %6,%11;"
        "ror    $0x5,%10;"
        "xor    %4,%12;"
        "psrld  $0x12,%%xmm2;"
        "ror    $0xb,%11;"
        "xor    %9,%10;"
        "and    %9,%12;"
        "ror    $0x6,%10;"
        "pxor   %%xmm3,%%xmm1;"
        "xor    %6,%11;"
        "xor    %4,%12;"
        "psrld  $0x3,%%xmm8;"
        "add    %10,%12;"
        "add    4+%16,%12;"
        "ror    $0x2,%11;"
        "pxor   %%xmm2,%%xmm1;"
        "mov    %6,%10;"
        "add    %12,%5;"
        "mov    %6,%12;"
        "pxor   %%xmm8,%%xmm1;"
        "or     %7,%10;"
        "add    %5,%8;"
        "and    %7,%12;"
        "pshufd $0xfa,%%xmm6,%%xmm2;"
        "and    %k2,%10;"
        "add    %11,%5;"
        "paddd  %%xmm1,%%xmm0;"
        "or     %12,%10;"
        "add    %10,%5;"
        "movdqa %%xmm2,%%xmm3;"
        "mov    %8,%10;"
        "mov    %5,%11;"
        "ror    $0xe,%10;"
        "movdqa %%xmm2,%%xmm8;"
        "xor    %8,%10;"
        "ror    $0x9,%11;"
        "mov    %9,%12;"
        "xor    %5,%11;"
        "ror    $0x5,%10;"
        "psrlq  $0x11,%%xmm2;"
        "xor    %3,%12;"
        "psrlq  $0x13,%%xmm3;"
        "xor    %8,%10;"
        "and    %8,%12;"
        "psrld  $0xa,%%xmm8;"
        "ror    $0xb,%11;"
        "xor    %5,%11;"
        "xor    %3,%12;"
        "ror    $0x6,%10;"
        "pxor   %%xmm3,%%xmm2;"
        "add    %10,%12;"
        "ror    $0x2,%11;"
        "add    8+%16,%12;"
        "pxor   %%xmm2,%%xmm8;"
        "mov    %5,%10;"
        "add    %12,%4;"
        "mov    %5,%12;"
        "pshufb %%xmm10,%%xmm8;"
        "or     %k2,%10;"
        "add    %4,%7;"
        "and    %k2,%12;"
        "paddd  %%xmm8,%%xmm0;"
        "and    %6,%10;"
        "add    %11,%4;"
        "pshufd $0x50,%%xmm0,%%xmm2;"
        "or     %12,%10;"
        "add    %10,%4;"
        "movdqa %%xmm2,%%xmm3;"
        "mov    %7,%10;"
        "ror    $0xe,%10;"
        "mov    %4,%11;"
        "movdqa %%xmm2,%%xmm7;"
        "ror    $0x9,%11;"
        "xor    %7,%10;"
        "mov    %8,%12;"
        "ror    $0x5,%10;"
        "psrlq  $0x11,%%xmm2;"
        "xor    %4,%11;"
        "xor    %9,%12;"
        "psrlq  $0x13,%%xmm3;"
        "xor    %7,%10;"
        "and    %7,%12;"
        "ror    $0xb,%11;"
        "psrld  $0xa,%%xmm7;"
        "xor    %4,%11;"
        "ror    $0x6,%10;"
        "xor    %9,%12;"
        "pxor   %%xmm3,%%xmm2;"
        "ror    $0x2,%11;"
        "add    %10,%12;"
        "add    12+%16,%12;"
        "pxor   %%xmm2,%%xmm7;"
        "mov    %4,%10;"
        "add    %12,%3;"
        "mov    %4,%12;"
        "pshufb %%xmm11,%%xmm7;"
        "or     %6,%10;"
        "add    %3,%k2;"
        "and    %6,%12;"
        "paddd  %%xmm0,%%xmm7;"
        "and    %5,%10;"
        "add    %11,%3;"
        "or     %12,%10;"
        "add    %10,%3;"
        "sub    $0x1,%1;"
        "jne    Lloop1_%=;"
        "mov    $0x2,%1;"

        "Lloop2_%=:"
        "paddd  0x0(%13),%%xmm4;"
        "movdqa %%xmm4,%16;"
        "mov    %k2,%10;"
        "ror    $0xe,%10;"
        "mov    %3,%11;"
        "xor    %k2,%10;"
        "ror    $0x9,%11;"
        "mov    %7,%12;"
        "xor    %3,%11;"
        "ror    $0x5,%10;"
        "xor    %8,%12;"
        "xor    %k2,%10;"
        "ror    $0xb,%11;"
        "and    %k2,%12;"
        "xor    %3,%11;"
        "ror    $0x6,%10;"
        "xor    %8,%12;"
        "add    %10,%12;"
        "ror    $0x2,%11;"
        "add    %16,%12;"
        "mov    %3,%10;"
        "add    %12,%9;"
        "mov    %3,%12;"
        "or     %5,%10;"
        "add    %9,%6;"
        "and    %5,%12;"
        "and    %4,%10;"
        "add    %11,%9;"
        "or     %12,%10;"
        "add    %10,%9;"
        "mov    %6,%10;"
        "ror    $0xe,%10;"
        "mov    %9,%11;"
        "xor    %6,%10;"
        "ror    $0x9,%11;"
        "mov    %k2,%12;"
        "xor    %9,%11;"
        "ror    $0x5,%10;"
        "xor    %7,%12;"
        "xor    %6,%10;"
        "ror    $0xb,%11;"
        "and    %6,%12;"
        "xor    %9,%11;"
        "ror    $0x6,%10;"
        "xor    %7,%12;"
        "add    %10,%12;"
        "ror    $0x2,%11;"
        "add    4+%16,%12;"
        "mov    %9,%10;"
        "add    %12,%8;"
        "mov    %9,%12;"
        "or     %4,%10;"
        "add    %8,%5;"
        "and    %4,%12;"
        "and    %3,%10;"
        "add    %11,%8;"
        "or     %12,%10;"
        "add    %10,%8;"
        "mov    %5,%10;"
        "ror    $0xe,%10;"
        "mov    %8,%11;"
        "xor    %5,%10;"
        "ror    $0x9,%11;"
        "mov    %6,%12;"
        "xor    %8,%11;"
        "ror    $0x5,%10;"
        "xor    %k2,%12;"
        "xor    %5,%10;"
        "ror    $0xb,%11;"
        "and    %5,%12;"
        "xor    %8,%11;"
        "ror    $0x6,%10;"
        "xor    %k2,%12;"
        "add    %10,%12;"
        "ror    $0x2,%11;"
        "add    8+%16,%12;"
        "mov    %8,%10;"
        "add    %12,%7;"
        "mov    %8,%12;"
        "or     %3,%10;"
        "add    %7,%4;"
        "and    %3,%12;"
        "and    %9,%10;"
        "add    %11,%7;"
        "or     %12,%10;"
        "add    %10,%7;"
        "mov    %4,%10;"
        "ror    $0xe,%10;"
        "mov    %7,%11;"
        "xor    %4,%10;"
        "ror    $0x9,%11;"
        "mov    %5,%12;"
        "xor    %7,%11;"
        "ror    $0x5,%10;"
        "xor    %6,%12;"
        "xor    %4,%10;"
        "ror    $0xb,%11;"
        "and    %4,%12;"
        "xor    %7,%11;"
        "ror    $0x6,%10;"
        "xor    %6,%12;"
        "add    %10,%12;"
        "ror    $0x2,%11;"
        "add    12+%16,%12;"
        "mov    %7,%10;"
        "add    %12,%k2;"
        "mov    %7,%12;"
        "or     %9,%10;"
        "add    %k2,%3;"
        "and    %9,%12;"
        "and    %8,%10;"
        "add    %11,%k2;"
        "or     %12,%10;"
        "add    %10,%k2;"
        "paddd  0x10(%13),%%xmm5;"
        "movdqa %%xmm5,%16;"
        "add    $0x20,%13;"
        "mov    %3,%10;"
        "ror    $0xe,%10;"
        "mov    %k2,%11;"
        "xor    %3,%10;"
        "ror    $0x9,%11;"
        "mov    %4,%12;"
        "xor    %k2,%11;"
        "ror    $0x5,%10;"
        "xor    %5,%12;"
        "xor    %3,%10;"
        "ror    $0xb,%11;"
        "and    %3,%12;"
        "xor    %k2,%11;"
        "ror    $0x6,%10;"
        "xor    %5,%12;"
        "add    %10,%12;"
        "ror    $0x2,%11;"
        "add    %16,%12;"
        "mov    %k2,%10;"
        "add    %12,%6;"
        "mov    %k2,%12;"
        "or     %8,%10;"
        "add    %6,%9;"
        "and    %8,%12;"
        "and    %7,%10;"
        "add    %11,%6;"
        "or     %12,%10;"
        "add    %10,%6;"
        "mov    %9,%10;"
        "ror    $0xe,%10;"
        "mov    %6,%11;"
        "xor    %9,%10;"
        "ror    $0x9,%11;"
        "mov    %3,%12;"
        "xor    %6,%11;"
        "ror    $0x5,%10;"
        "xor    %4,%12;"
        "xor    %9,%10;"
        "ror    $0xb,%11;"
        "and    %9,%12;"
        "xor    %6,%11;"
        "ror    $0x6,%10;"
        "xor    %4,%12;"
        "add    %10,%12;"
        "ror    $0x2,%11;"
        "add    4+%16,%12;"
        "mov    %6,%10;"
        "add    %12,%5;"
        "mov    %6,%12;"
        "or     %7,%10;"
        "add    %5,%8;"
        "and    %7,%12;"
        "and    %k2,%10;"
        "add    %11,%5;"
        "or     %12,%10;"
        "add    %10,%5;"
        "mov    %8,%10;"
        "ror    $0xe,%10;"
        "mov    %5,%11;"
        "xor    %8,%10;"
        "ror    $0x9,%11;"
        "mov    %9,%12;"
        "xor    %5,%11;"
        "ror    $0x5,%10;"
        "xor    %3,%12;"
        "xor    %8,%10;"
        "ror    $0xb,%11;"
        "and    %8,%12;"
        "xor    %5,%11;"
        "ror    $0x6,%10;"
        "xor    %3,%12;"
        "add    %10,%12;"
        "ror    $0x2,%11;"
        "add    8+%16,%12;"
        "mov    %5,%10;"
        "add    %12,%4;"
        "mov    %5,%12;"
        "or     %k2,%10;"
        "add    %4,%7;"
        "and    %k2,%12;"
        "and    %6,%10;"
        "add    %11,%4;"
        "or     %12,%10;"
        "add    %10,%4;"
        "mov    %7,%10;"
        "ror    $0xe,%10;"
        "mov    %4,%11;"
        "xor    %7,%10;"
        "ror    $0x9,%11;"
        "mov    %8,%12;"
        "xor    %4,%11;"
        "ror    $0x5,%10;"
        "xor    %9,%12;"
        "xor    %7,%10;"
        "ror    $0xb,%11;"
        "and    %7,%12;"
        "xor    %4,%11;"
        "ror    $0x6,%10;"
        "xor    %9,%12;"
        "add    %10,%12;"
        "ror    $0x2,%11;"
        "add    12+%16,%12;"
        "mov    %4,%10;"
        "add    %12,%3;"
        "mov    %4,%12;"
        "or     %6,%10;"
        "add    %3,%k2;"
        "and    %6,%12;"
        "and    %5,%10;"
        "add    %11,%3;"
        "or     %12,%10;"
        "add    %10,%3;"
        "movdqa %%xmm6,%%xmm4;"
        "movdqa %%xmm7,%%xmm5;"
        "sub    $0x1,%1;"
        "jne    Lloop2_%=;"
        "add    (%0),%3;"
        "mov    %3,(%0);"
        "add    0x4(%0),%4;"
        "mov    %4,0x4(%0);"
        "add    0x8(%0),%5;"
        "mov    %5,0x8(%0);"
        "add    0xc(%0),%6;"
        "mov    %6,0xc(%0);"
        "add    0x10(%0),%k2;"
        "mov    %k2,0x10(%0);"
        "add    0x14(%0),%7;"
        "mov    %7,0x14(%0);"
        "add    0x18(%0),%8;"
        "mov    %8,0x18(%0);"
        "add    0x1c(%0),%9;"
        "mov    %9,0x1c(%0);"
        "mov    %15,%1;"
        "add    $0x40,%1;"
        "cmp    %14,%1;"
        "jne    Lloop0_%=;"

        "Ldone_hash_%=:"

        /*+r“（s），“+r”（区块），“+r”（区块），“=r”（a），“=r”（b），“=r”（c），“=r”（d），/*e=区块*/“=r”（f），“=r”（g），“=r”（h），“=r”（y0），“=r”（y1），“=r”（y2），“=r”（tbl），“+m”（inp-end），“+m”（inp），“+m”（xfer）
        ：“M”（k256）、“M”（翻盖）、“M”（Shuf_00ba）、“M”（Shuf_DC00）
        ：“cc”，“memory”，“xmm0”，“xmm1”，“xmm2”，“xmm3”，“xmm4”，“xmm5”，“xmm6”，“xmm7”，“xmm8”，“xmm9”，“xmm10”，“xmm11”，“xmm12”
   ；
}
}

/*
呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜
；版权所有（c）2012，英特尔公司
；
；保留所有权利。
；
；以源和二进制形式重新分配和使用，有或无
；允许修改，前提是以下条件
MET：
；
；*源代码的再分配必须保留上述版权。
；注意，此条件列表和以下免责声明。
；
；*二进制形式的再分配必须复制上述版权。
；注意，此条件列表和以下免责声明
；提供的文件和/或其他材料
；分配。
；
；*无论是英特尔公司的名称还是它的名称
；贡献者可用于支持或推广源自
；本软件未经事先明确书面许可。
；
；
；此软件由Intel Corporation“按原样”和任何
；明示或暗示保证，包括但不限于
；对特定产品的适销性和适用性的暗示保证
；目的被放弃。在任何情况下，英特尔公司或
；贡献者对任何直接、间接、偶然、特殊的
；惩戒性或间接损害（包括但不限于，
替代货物或服务的采购；使用损失、数据或
；利润；或业务中断），但根据
责任，无论是合同责任、严格责任还是侵权责任（包括
；疏忽或其他）因使用本文件而引起的
；软件，即使已告知此类损坏的可能性。
呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜
；
；yasm命令行示例：
；windows:yasm-xvc-f x64-rnasm-pnasm-o sha256_sse4.obj-g cv8 sha256_sse4.asm
；linux:yasm-f x64-f elf64-x gnu-g dwarf2-d linux-o sha256_sse4.o sha256_sse4.asm
；
呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜；
；
；此代码在Intel白皮书中描述：
；“英特尔体系结构处理器上的快速SHA-256实现”
；
；要找到它，请访问http://www.intel.com/p/en_us/embedded
搜索那个标题。
；预计该论文将于2012年4月底发布。
；
呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜
；此代码一次计划1个街区，每个街区4条车道
呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜

%define movdq movdqu；；假定缓冲区未对齐

定义宏

；addm[mem]，注册
；使用reg mem add和store将reg添加到mem
%宏ADDM 2
    添加% 2，% 1
    MOV % 1，% 2
%Enm

呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜；

；复制_xmm_和_bswap xmm，[mem]，字节_翻转_掩码
；加载带有MEM的XMM，并交换每个DWORD的字节
%macro copy_xmm_和_bswap 3
    MOVDQ % 1，% 2
    pSUFFB % 1，% 3
%EngEng宏

呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜；

%定义x0 xMM4
%定义X1 xMM5
%定义X2 xMM6
定义x3 xMM7

%定义xtmp0 xmm0
%定义xtmp1 xmm1
%定义xtmp2 xmm2
%定义xtmp3 xmm3
%定义xtmp4 xmm8
%定义xfer xmm9

%define shuf00ba xmm10；shuffle xbxa->00ba
%define shuf_dc00 xmm11；shuffle xdxc->dc00
%定义字节翻转掩码xmm12
    
%IFDEF Linux
%define num_ks rdx；第三个参数
%define ctx rsi；第二个参数
%define inp rdi；第一个参数

%定义srnd rdi；clobbers inp
定义C ECX
%定义D R8D
定义E-EDX
%其他
%define num_blks r8；第三个参数
%define ctx rdx；第二个参数
%define inp rcx；第一个参数

%定义srnd rcx；clobbers inp
定义C EDI
%定义D ESI
%定义E R8D
    
%Endif
%定义TBL RBP
定义Eax
定义B EBX

%定义F R9D
%定义G R10D
定义H R11D

%定义y0 r13d
%定义Y1 R14D
%定义Y2 R15D



_inp_end_尺寸EQ 8
8英寸尺寸
尺寸EQU 8
%IFDEF Linux
_xmm_save_大小equ 0
%其他
_xmm_save_尺寸equ 7*16
%Endif
；堆栈大小加上推送必须是8的奇数倍
_对齐_大小EQU 8

Inπ端0
输入设备输入端+输入端尺寸
输入设备输入+输入尺寸
_xmm_save equ _xfer+_xfer_size+_align_size
堆栈\u大小equ \u xmm \u save+\u xmm \u save \u大小

旋转式
；旋转符号的值X0…X3
%宏旋转
xxx0定义x0 x0
xxx1 x1定义
xx1定义x1 x2
%xDeXixx2 x3
xxxxxi定义
%Enm

旋转木马
；旋转符号值a…h
%宏旋转参数0
定义为tMPH
定义为
%x定义g f
%x定义f
定义为
%x定义D
定义为
定义为
定义一个TMP*
%Enm

%macro four_rounds_and_sched 0
 ；；一次计算S0 4，一次计算S1 2
 ；；一次计算w[-16]+w[-7]4
 movdqa xtmp0，x3型
    移动y0，e；y0=e
    错误Y0，（25-11）；Y0=E>>（25-11）
    MOV Y1，A；Y1=A
 重新连接xtmp0，x2，4；xtmp0=w[-7]
    误差y1，（22-13）；y1=a>>（22-13）
    xor y0，e；y0=e^（e>>（25-11））
    mov y2，f；y2=f
    错误Y0，（11-6）；Y0=（e>>（11-6））^（e>>（25-6））
 MOVDQA XTMP1，x1
    xor y1，a；y1=a^（a>>（22-13）
    xor y2，g；y2=f^g
 Paddd Xtmp0，X0；Xtmp0=w[-7]+w[-16]
    xor y0，e；y0=e^（e>>（11-6））^（e>>（25-6））
    和y2，e；y2=（f^g）&e
    错误y1，（13-2）；y1=（a>>（13-2））^（a>>（22-2））
 计算S0
 重新连接xtmp1，x0，4；xtmp1=w[-15]
    xor y1，a；y1=a^（a>>（13-2））^（a>>（22-2））
    错误Y0，6；Y0=S1=（E>>6）&（E>>11）^（E>>25）
    xor y2，g；y2=ch=（f^g）&e）^g
 movdqa xtmp2，xtmp1；xtmp2=w[-15]
    错误Y1，2；Y1=S0=（A>>2）^（A>>13）^（A>>22）
    加y2，y0；y2=s1+ch
    加y2，[rsp+xfer+0*4]；y2=k+w+s1+ch
 movdqa xtmp3，xtmp1；xtmp3=w[-15]
    mov y0，a；y0=a
    加h，y2；h=h+s1+ch+k+w
    MOV Y2，A；Y2=A
 PSLLD XTMP1，（32-7）
    或y0，c；y0=a c
    加d，h；d=d+h+s1+ch+k+w
    和y2，c；y2=a&c
 PSRLD XTMP2，7
    和y0，b；y0=（a c）&b
    加h，y1；h=h+s1+ch+k+w+s0
 Por Xtmp1，Xtmp2；Xtmp1=w[-15]错误7
    或Y0、Y2；Y0=maj=（a c）&b）（a&c）
    加h，y0；h=h+s1+ch+k+w+s0+maj

旋转栅格
 movdqa xtmp2，xtmp3；xtmp2=w[-15]
    移动y0，e；y0=e
    MOV Y1，A；Y1=A
 movdqa xtmp4，xtmp3；xtmp4=w[-15]
    错误Y0，（25-11）；Y0=E>>（25-11）
    xor y0，e；y0=e^（e>>（25-11））
    mov y2，f；y2=f
    误差y1，（22-13）；y1=a>>（22-13）
 PSLLD XTM3，（32-18）
    xor y1，a；y1=a^（a>>（22-13）
    错误Y0，（11-6）；Y0=（e>>（11-6））^（e>>（25-6））
    xor y2，g；y2=f^g
 PSRLD XTMP2，18
    错误y1，（13-2）；y1=（a>>（13-2））^（a>>（22-2））
    xor y0，e；y0=e^（e>>（11-6））^（e>>（25-6））
    和y2，e；y2=（f^g）&e
    错误Y0，6；Y0=S1=（E>>6）&（E>>11）^（E>>25）
 像素或XTMP1、XTM3
    xor y1，a；y1=a^（a>>（13-2））^（a>>（22-2））
    xor y2，g；y2=ch=（f^g）&e）^g
 PSRLD XTMP4，3；XTMP4=W[-15]>>3
    加y2，y0；y2=s1+ch
    加y2，[rsp+xfer+1*4]；y2=k+w+s1+ch
    错误Y1，2；Y1=S0=（A>>2）^（A>>13）^（A>>22）
 pxor xtmp1，xtmp2；xtmp1=w[-15]错误7^w[-15]错误18
    mov y0，a；y0=a
    加h，y2；h=h+s1+ch+k+w
    MOV Y2，A；Y2=A
 pxor xtmp1，xtmp4；xtmp1=S0
    或y0，c；y0=a c
    加d，h；d=d+h+s1+ch+k+w
    和y2，c；y2=a&c
 ；；计算低位s1
 pshufd xtmp2，x3，11111 010b；xtmp2=w[-2]bbaa
    和y0，b；y0=（a c）&b
    加h，y1；h=h+s1+ch+k+w+s0
 paddd xtmp0，xtmp1；xtmp0=w[-16]+w[-7]+s0
    或Y0、Y2；Y0=maj=（a c）&b）（a&c）
    加h，y0；h=h+s1+ch+k+w+s0+maj

旋转栅格
 movdqa xtmp3，xtmp2；xtmp3=w[-2]bbaa_
    移动y0，e；y0=e
    MOV Y1，A；Y1=A
    错误Y0，（25-11）；Y0=E>>（25-11）
 movdqa xtmp4，xtmp2；xtmp4=w[-2]bbaa_
    xor y0，e；y0=e^（e>>（25-11））
    误差y1，（22-13）；y1=a>>（22-13）
    mov y2，f；y2=f
    xor y1，a；y1=a^（a>>（22-13）
    错误Y0，（11-6）；Y0=（e>>（11-6））^（e>>（25-6））
 psrlq xtmp2，17；xtmp2=w[-2]错误17 xbxa
    xor y2，g；y2=f^g
 psrlq xtmp3，19；xtmp3=w[-2]错误19 xbxa
    xor y0，e；y0=e^（e>>（11-6））^（e>>（25-6））
    和y2，e；y2=（f^g）&e
 psrld xtmp4，10；xtmp4=w[-2]>>10_bbaa_
    错误y1，（13-2）；y1=（a>>（13-2））^（a>>（22-2））
    xor y1，a；y1=a^（a>>（13-2））^（a>>（22-2））
    xor y2，g；y2=ch=（f^g）&e）^g
    错误Y0，6；Y0=S1=（E>>6）&（E>>11）^（E>>25）
 像素或XTMP2、XTM3
    加y2，y0；y2=s1+ch
    错误Y1，2；Y1=S0=（A>>2）^（A>>13）^（A>>22）
    加y2，[rsp+xfer+2*4]；y2=k+w+s1+ch
 pxor xtmp4，xtmp2；xtmp4=s1_xbxa_
    mov y0，a；y0=a
    加h，y2；h=h+s1+ch+k+w
    MOV Y2，A；Y2=A
 pshufb xtmp4，shuf00ba；xtmp4=s1_00ba_
    或y0，c；y0=a c
    加d，h；d=d+h+s1+ch+k+w
    和y2，c；y2=a&c
 paddd xtmp0，xtmp4；xtmp0=…，…，w[1]，w[0]
    和y0，b；y0=（a c）&b
    加h，y1；h=h+s1+ch+k+w+s0
 ；；计算高s1
 pshufd xtmp2，xtmp0，01010000B；xtmp2=w[-2]ddcc_
    或Y0、Y2；Y0=maj=（a c）&b）（a&c）
    加h，y0；h=h+s1+ch+k+w+s0+maj

旋转栅格
 movdqa xtmp3，xtmp2；xtmp3=w[-2]ddcc_
    移动y0，e；y0=e
    错误Y0，（25-11）；Y0=E>>（25-11）
    MOV Y1，A；Y1=A
 MOVDQA X0，XTMP2；X0=W[-2]DDCC_
    误差y1，（22-13）；y1=a>>（22-13）
    xor y0，e；y0=e^（e>>（25-11））
    mov y2，f；y2=f
    错误Y0，（11-6）；Y0=（e>>（11-6））^（e>>（25-6））
 psrlq xtmp2，17；xtmp2=w[-2]错误17 xdxc
    xor y1，a；y1=a^（a>>（22-13）
    xor y2，g；y2=f^g
 psrlq xtmp3，19；xtmp3=w[-2]错误19 xdxc
    xor y0，e；y0=e^（e>>（11-6））^（e>>（25-6））
    和y2，e；y2=（f^g）&e
    错误y1，（13-2）；y1=（a>>（13-2））^（a>>（22-2））
 psrld x0，10；x0=w[-2]>>10_ddcc_
    xor y1，a；y1=a^（a>>（13-2））^（a>>（22-2））
    错误Y0，6；Y0=S1=（E>>6）&（E>>11）^（E>>25）
    xor y2，g；y2=ch=（f^g）&e）^g
 像素或XTMP2、XTM3
    错误Y1，2；Y1=S0=（A>>2）^（A>>13）^（A>>22）
    加y2，y0；y2=s1+ch
    加y2，[rsp+xfer+3*4]；y2=k+w+s1+ch
 Pxor X0，Xtmp2；X0=s1 xdxc
    mov y0，a；y0=a
    加h，y2；h=h+s1+ch+k+w
    MOV Y2，A；Y2=A
 pshufb x0，shuf_dc00；x0=s1_dc00_
    或y0，c；y0=a c
    加d，h；d=d+h+s1+ch+k+w
    和y2，c；y2=a&c
 paddd x0，xtmp0；x0=w[3]，w[2]，w[1]，w[0]
    和y0，b；y0=（a c）&b
    加h，y1；h=h+s1+ch+k+w+s0
    或Y0、Y2；Y0=maj=（a c）&b）（a&c）
    加h，y0；h=h+s1+ch+k+w+s0+maj

旋转栅格
旋转式X射线光谱仪
%Enm

；；输入为[rsp+xfer+%1*4]
%宏执行第1轮
    移动y0，e；y0=e
    错误Y0，（25-11）；Y0=E>>（25-11）
    MOV Y1，A；Y1=A
    xor y0，e；y0=e^（e>>（25-11））
    误差y1，（22-13）；y1=a>>（22-13）
    mov y2，f；y2=f
    xor y1，a；y1=a^（a>>（22-13）
    错误Y0，（11-6）；Y0=（e>>（11-6））^（e>>（25-6））
    xor y2，g；y2=f^g
    xor y0，e；y0=e^（e>>（11-6））^（e>>（25-6））
    错误y1，（13-2）；y1=（a>>（13-2））^（a>>（22-2））
    和y2，e；y2=（f^g）&e
    xor y1，a；y1=a^（a>>（13-2））^（a>>（22-2））
    错误Y0，6；Y0=S1=（E>>6）&（E>>11）^（E>>25）
    xor y2，g；y2=ch=（f^g）&e）^g
    加y2，y0；y2=s1+ch
    错误Y1，2；Y1=S0=（A>>2）^（A>>13）^（A>>22）
    添加y2，[rsp+xfer+%1*4]；y2=k+w+s1+ch
    mov y0，a；y0=a
    加h，y2；h=h+s1+ch+k+w
    MOV Y2，A；Y2=A
    或y0，c；y0=a c
    加d，h；d=d+h+s1+ch+k+w
    和y2，c；y2=a&c
    和y0，b；y0=（a c）&b
    加h，y1；h=h+s1+ch+k+w+s0
    或Y0、Y2；Y0=maj=（a c）&b）（a&c）
    加h，y0；h=h+s1+ch+k+w+s0+maj
    旋转栅格
%Enm

呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜
呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜呜
；；void sha256_sse4（void*输入_data，uint32 digest[8]，uint64 num_blks）
；；arg 1：指向输入数据的指针
；；arg 2:指向摘要的指针
；；arg 3:num块
文本部分
全球Sha256
对齐32
Sa256SSE4:
    推动RBX
%IFNDEF Linux
    推式RSI
    推式RDI
%Endif
    推压
    推R13
    推R14
    推R15

    子RSP，堆栈大小
%IFNDEF Linux
    movdqa[rsp+_xmm_save+0*16]，xmm6
    movdqa[rsp+_xmm_save+1*16]，xmm7
    movdqa[rsp+_xmm_save+2*16]，xmm8
    movdqa[rsp+_xmm_save+3*16]，xmm9
    movdqa[rsp+_xmm_save+4*16]，xmm10
    movdqa[rsp+_xmm_save+5*16]，xmm11
    movdqa[rsp+_xmm_save+6*16]，xmm12
%Endif

    shl num_blks，6；转换为字节
    JZ DONEY散列
    添加num_blks，inp；指向数据结尾的指针
    mov[rsp+_inp_end]，num_blks

    ；；加载初始摘要
    MOV A，[4*0+CTX]
    MOV B，[4*1+CTX]
    MOV C，[4*2+CTX]
    移动d，[4*3+ctx]
    MOV E，[4*4+CTX]
    MOV F，[4*5+CTX]
    移动G，[4*6+CTX]
    MOV H，[4*7+CTX]

    movdqa byte_flip_mask，[pshuffle_byte_flip_mask wrt rip]
    Movdqa Shuf00ba，[\u Shuf00ba wrt rip]
    movdqa shuf_dc00，[_shuf_dc00 wrt rip]

ROC0:
    引线TBL，[k256 Wrt-Rip]

    ；；字节交换前16个双字
    复制_xmm_和_bswap x0，[inp+0*16]，字节_翻转_掩码
    复制_xmm_和_bswap x1，[inp+1*16]，字节_翻转_掩码
    复制_xmm_和_bswap x2，[inp+2*16]，字节_翻转_掩码
    复制_xmm_和_bswap x3，[inp+3*16]，字节_翻转_掩码
    
    mov[rsp+_inp]，输入

    ；；调度48个输入数据字，每个数据字执行3轮16次
    MOV SRND，3
对齐16
Loop1:
    movdqa xfer，[tbl+0*16]
    PADD XFER，X0
    movdqa[rsp+xfer]，xfer
    四个回合

    movdqa xfer，[tbl+1*16]
    PADD XFER，X0
    movdqa[rsp+xfer]，xfer
    四个回合

    movdqa xfer，[tbl+2*16]
    PADD XFER，X0
    movdqa[rsp+xfer]，xfer
    四个回合

    movdqa xfer，[tbl+3*16]
    PADD XFER，X0
    movdqa[rsp+xfer]，xfer
    添加TBL，4×16
    四个回合

    亚SRND，1
    JNE环1

    MOV SRND，2
环2：
    PADDD X0，[tbl+0*16]
    movdqa[rsp+xfer]，X0
    多伊回合0
    多伊回合1
    多伊回合2
    多伊回合3
    PADDD x1，[tbl+1*16]
    movdqa[rsp+xfer]，x1
    添加TBL，2×16
    多伊回合0
    多伊回合1
    多伊回合2
    多伊回合3

    MOVDQA X0，X2
    MOVDQA X1，X3

    亚SRND，1
    JNE环2

    addm[4*0+ctx]，A
    添加[4*1+ctx]，b
    添加[4*2+CTX]，C
    添加[4*3+ctx]，d
    添加[4*4+CTX]，E
    添加[4*5+CTX]，F
    加法[4*6+ctx]，g
    加法[4*7+ctx]，H

    mov-inp，[rsp+_-inp]
    添加InP，64
    CMP输入
    JNE ROCK0

DONEY散列：
%IFNDEF Linux
    movdqa xmm6，[rsp+_xmm_save+0*16]
    movdqa xmm7，[rsp+_xmm_save+1*16]
    movdqa xmm8，[rsp+_xmm_save+2*16]
    movdqa xmm9，[rsp+_xmm_save+3*16]
    movdqa xmm10，[rsp+_xmm_save+4*16]
    movdqa xmm11，[rsp+_xmm_save+5*16]
    movdqa xmm12，[rsp+_xmm_save+6*16]
%Endif

    添加RSP，堆栈大小

    POP R15
    POP R14
    POP R13
    流行性视网膜色素变性
%IFNDEF Linux
    波普RDI
    POP RSI
%Endif
    POP RBX

    雷特
    

截面数据
对齐64
K256：
    dd 0x428A2F98、0x71374491、0xb5c0fbcf、0xe9b5dba5
    dd 0x3956c25b，0x59f111f1，0x923f82a4，0xab1c5ed5
    DD 0XD807AA98、0X12835B01、0X243158BE、0X550C7DC3
    DD 0X72BE5D74,0X80DEB1FE，0X9BDC06A7,0XC19BF174
    DD 0XE49B69C1,0XEFBE4786,0X0FC19DC6,0X240CA1CC
    DD 0x2DE92C6F、0x4A7484AA、0x5CB0A9DC、0x76F988DA
    DD 0x983E5152、0XA831C66D、0XB00327C8、0XBF597FC7
    dd 0xc6e00bf3、0xd5a79147、0x06ca6351、0x14292967
    dd 0x27b70a85,0x2e1b2138,0x4d2c6dfc，0x53380d13
    DD 0x650A7354、0x766a0abb、0x81c2c92e、0x92722c85
    DD 0XA2BFE8A1、0XA81A664B、0XC24B8B70、0XC76C51A3
    DD 0XD192E819、0XD6990624、0XF40E3585、0X106AA070
    DD 0x19A4C116、0x1E376C08、0x2748774C、0x34b0bcb5
    DD 0x391C0CB3、0x4ED8AA4A、0x5B9CCA4F、0x682E6FF3
    DD 0x748F82EE、0x78A5636F、0x84C87814、0x8CC70208
    DD 0X90BEFFA、0XA4506CEB、0XBEF9A3F7、0XC67178F2

pshuffle_byte_flip_mask:ddq 0x0c0d0e0f08090a0b405060700010203

；随机播放xbxa->00ba
_shuf_00ba:ddq 0xffffffffffffffffff0b0a090803020100


_-shuf_-dc00:ddq 0x0b0a90803020100ffffffffffffffffffffff
**/


#endif
