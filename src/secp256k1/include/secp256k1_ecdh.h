
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
#ifndef SECP256K1_ECDH_H
#define SECP256K1_ECDH_H

#include "secp256k1.h"

#ifdef __cplusplus
extern "C" {
#endif

/*在恒定时间内计算EC Diffie-Hellman秘密
 *返回：1:求幂成功
 *0:标量无效（零或溢出）
 *args:ctx:指向上下文对象的指针（不能为空）
 *out：结果：32字节数组，由ECDH填充。
 *从点和标量计算的秘密
 *in:pubkey：指向secp256k1_pubkey的指针，其中包含
 *已初始化公钥
 *privkey：一个32字节的标量，与该点相乘。
 **/

SECP256K1_API SECP256K1_WARN_UNUSED_RESULT int secp256k1_ecdh(
  const secp256k1_context* ctx,
  unsigned char *result,
  const secp256k1_pubkey *pubkey,
  const unsigned char *privkey
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(4);

#ifdef __cplusplus
}
#endif

/*DIF/*secp256k1_ecdh_h*/
