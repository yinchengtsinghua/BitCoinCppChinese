
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
#include <stdlib.h>
#include <stdint.h>
#include "org_bitcoin_Secp256k1Context.h"
#include "include/secp256k1.h"

SECP256K1_API jlong JNICALL Java_org_bitcoin_Secp256k1Context_secp256k1_1init_1context
  (JNIEnv* env, jclass classObject)
{
  secp256k1_context *ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);

  (void)classObject;(void)env;

  return (uintptr_t)ctx;
}

