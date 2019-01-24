
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
#ifndef SECP256K1_RECOVERY_H
#define SECP256K1_RECOVERY_H

#include "secp256k1.h"

#ifdef __cplusplus
extern "C" {
#endif

/*不透明的数据结构，包含已解析的ECDSA签名，
 *支持pubkey恢复。
 *
 *内部数据的准确表示是实现定义的，而不是
 *保证在不同平台或版本之间可移植。它是
 *但是保证大小为65字节，并且可以安全地复制/移动。
 *如果需要转换为适合存储或传输的格式，请使用
 *secp256k1_ecdsa_signature_serialize_ux和
 *secp256k1_ecdsa_signature_parse_ux函数。
 *
 *此外，保证相同的签名（包括
 *可恢复性）具有相同的表示，因此它们可以
 * MeMCP'ED.
 **/

typedef struct {
    unsigned char data[65];
} secp256k1_ecdsa_recoverable_signature;

/*解析一个紧凑的ECDSA签名（64字节+恢复ID）。
 *
 *当可以解析签名时返回1，否则返回0
 *args:ctx:secp256k1上下文对象
 *out:sig：指向签名对象的指针
 *in:input64：指向64字节压缩签名的指针
 *recid：恢复ID（0、1、2或3）
 **/

SECP256K1_API int secp256k1_ecdsa_recoverable_signature_parse_compact(
    const secp256k1_context* ctx,
    secp256k1_ecdsa_recoverable_signature* sig,
    const unsigned char *input64,
    int recid
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

/*将可恢复签名转换为普通签名。
 *
 *返回：1
 *out:sig：指向普通签名的指针（不能为空）。
 *in:sigin：指向可恢复签名的指针（不能为空）。
 **/

SECP256K1_API int secp256k1_ecdsa_recoverable_signature_convert(
    const secp256k1_context* ctx,
    secp256k1_ecdsa_signature* sig,
    const secp256k1_ecdsa_recoverable_signature* sigin
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

/*以压缩格式（64字节+恢复ID）序列化ECDSA签名。
 *
 *返回：1
 *args:ctx:secp256k1上下文对象
 *out:output64:指向压缩签名的64字节数组的指针（不能为空）
 *recid：指向整数的指针，用于保存恢复ID（可以为空）。
 *in:sig：指向已初始化签名对象的指针（不能为空）
 **/

SECP256K1_API int secp256k1_ecdsa_recoverable_signature_serialize_compact(
    const secp256k1_context* ctx,
    unsigned char *output64,
    int *recid,
    const secp256k1_ecdsa_recoverable_signature* sig
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(4);

/*创建可恢复的ECDSA签名。
 *
 *返回：1:已创建签名
 *0:nonce生成函数失败，或者私钥无效。
 *args:ctx:指向上下文对象的指针，已初始化以便签名（不能为空）
 *out:sig：指向将放置签名的数组的指针（不能为空）
 *in:msg32:正在签名的32字节消息哈希（不能为空）
 *seckey：指向32字节密钥的指针（不能为空）
 *noncefp：指向nonce生成函数的指针。如果为空，则使用secp256k1_nonce_函数_default
 *data：指向nonce生成函数使用的任意数据的指针（可以为空）
 **/

SECP256K1_API int secp256k1_ecdsa_sign_recoverable(
    const secp256k1_context* ctx,
    secp256k1_ecdsa_recoverable_signature *sig,
    const unsigned char *msg32,
    const unsigned char *seckey,
    secp256k1_nonce_function noncefp,
    const void *ndata
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(4);

/*从签名恢复ECDSA公钥。
 *
 *返回：1:公钥恢复成功（保证签名正确）。
 *0：否则。
 *args:ctx:指向上下文对象的指针，已初始化以供验证（不能为空）
 *out:pubkey：指向恢复的公钥的指针（不能为空）
 *in:sig：指向支持pubkey恢复的初始化签名的指针（不能为空）
 *msg32:假定32字节消息哈希已签名（不能为空）
 **/

SECP256K1_API SECP256K1_WARN_UNUSED_RESULT int secp256k1_ecdsa_recover(
    const secp256k1_context* ctx,
    secp256k1_pubkey *pubkey,
    const secp256k1_ecdsa_recoverable_signature *sig,
    const unsigned char *msg32
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(4);

#ifdef __cplusplus
}
#endif

