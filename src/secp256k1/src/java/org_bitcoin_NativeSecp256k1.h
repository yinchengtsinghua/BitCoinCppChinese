
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
/*不要编辑此文件-它是由计算机生成的*/
#include <jni.h>
#include "include/secp256k1.h"
/*类别“组织比特币”标题“本机”第256K1页*/

#ifndef _Included_org_bitcoin_NativeSecp256k1
#define _Included_org_bitcoin_NativeSecp256k1
#ifdef __cplusplus
extern "C" {
#endif
/*
 *类别：组织比特币\u NativeSecP256K1
 *方法：secp256k1_ctx_克隆
 *签字：（j）j
 **/

SECP256K1_API jlong JNICALL Java_org_bitcoin_NativeSecp256k1_secp256k1_1ctx_1clone
  (JNIEnv *, jclass, jlong);

/*
 *类别：组织比特币\u NativeSecP256K1
 *方法：secp256k1_context_随机化
 *签名：（ljava/nio/bytebuffer；j）i
 **/

SECP256K1_API jint JNICALL Java_org_bitcoin_NativeSecp256k1_secp256k1_1context_1randomize
  (JNIEnv *, jclass, jobject, jlong);

/*
 *类别：组织比特币\u NativeSecP256K1
 *方法：secp256k1_privkey_tweak_add
 *签名：（ljava/nio/bytebuffer；j）[[b
 **/

SECP256K1_API jobjectArray JNICALL Java_org_bitcoin_NativeSecp256k1_secp256k1_1privkey_1tweak_1add
  (JNIEnv *, jclass, jobject, jlong);

/*
 *类别：组织比特币\u NativeSecP256K1
 *方法：secp256k1_privkey_tweak_mul
 *签名：（ljava/nio/bytebuffer；j）[[b
 **/

SECP256K1_API jobjectArray JNICALL Java_org_bitcoin_NativeSecp256k1_secp256k1_1privkey_1tweak_1mul
  (JNIEnv *, jclass, jobject, jlong);

/*
 *类别：组织比特币\u NativeSecP256K1
 *方法：secp256k1_pubkey_tweak_add
 *签名：（ljava/nio/bytebuffer；ji）[[b
 **/

SECP256K1_API jobjectArray JNICALL Java_org_bitcoin_NativeSecp256k1_secp256k1_1pubkey_1tweak_1add
  (JNIEnv *, jclass, jobject, jlong, jint);

/*
 *类别：组织比特币\u NativeSecP256K1
 *方法：secp256k1_pubkey_tweak_mul
 *签名：（ljava/nio/bytebuffer；ji）[[b
 **/

SECP256K1_API jobjectArray JNICALL Java_org_bitcoin_NativeSecp256k1_secp256k1_1pubkey_1tweak_1mul
  (JNIEnv *, jclass, jobject, jlong, jint);

/*
 *类别：组织比特币\u NativeSecP256K1
 *方法：secp256k1_destroy_context
 *签名：（j）v
 **/

SECP256K1_API void JNICALL Java_org_bitcoin_NativeSecp256k1_secp256k1_1destroy_1context
  (JNIEnv *, jclass, jlong);

/*
 *类别：组织比特币\u NativeSecP256K1
 *方法：secp256k1_ecdsa_verify
 *签名：（ljava/nio/bytebuffer；jii）i
 **/

SECP256K1_API jint JNICALL Java_org_bitcoin_NativeSecp256k1_secp256k1_1ecdsa_1verify
  (JNIEnv *, jclass, jobject, jlong, jint, jint);

/*
 *类别：组织比特币\u NativeSecP256K1
 *方法：secp256k1_ecdsa_sign
 *签名：（ljava/nio/bytebuffer；j）[[b
 **/

SECP256K1_API jobjectArray JNICALL Java_org_bitcoin_NativeSecp256k1_secp256k1_1ecdsa_1sign
  (JNIEnv *, jclass, jobject, jlong);

/*
 *类别：组织比特币\u NativeSecP256K1
 *方法：secp256k1_ec_seckey_verify
 *签名：（ljava/nio/bytebuffer；j）i
 **/

SECP256K1_API jint JNICALL Java_org_bitcoin_NativeSecp256k1_secp256k1_1ec_1seckey_1verify
  (JNIEnv *, jclass, jobject, jlong);

/*
 *类别：组织比特币\u NativeSecP256K1
 *方法：secp256k1_ec_pubkey_create
 *签名：（ljava/nio/bytebuffer；j）[[b
 **/

SECP256K1_API jobjectArray JNICALL Java_org_bitcoin_NativeSecp256k1_secp256k1_1ec_1pubkey_1create
  (JNIEnv *, jclass, jobject, jlong);

/*
 *类别：组织比特币\u NativeSecP256K1
 *方法：secp256k1_ec_pubkey_parse
 *签名：（ljava/nio/bytebuffer；ji）[[b
 **/

SECP256K1_API jobjectArray JNICALL Java_org_bitcoin_NativeSecp256k1_secp256k1_1ec_1pubkey_1parse
  (JNIEnv *, jclass, jobject, jlong, jint);

/*
 *类别：组织比特币\u NativeSecP256K1
 *方法：secp256k1_ecdh
 *签名：（ljava/nio/bytebuffer；ji）[[b
 **/

SECP256K1_API jobjectArray JNICALL Java_org_bitcoin_NativeSecp256k1_secp256k1_1ecdh
  (JNIEnv* env, jclass classObject, jobject byteBufferObject, jlong ctx_l, jint publen);


#ifdef __cplusplus
}
#endif
#endif
