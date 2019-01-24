
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
/***************************************************************
 *版权所有（c）2014、2015 Pieter Wuille*
 *根据麻省理工学院软件许可证分发，请参阅随附的*
 *文件复制或http://www.opensource.org/licenses/mit license.php.*
 ***************************************************************/


/***
 *请不要直接链接此文件。它不是libsecp256k1的一部分
 *不保证其API、功能或
 *存在。使用此代码的项目应改为复制此标题
 *及其附带的.c文件直接进入其代码库。
 ***/


/*此文件包含用于分析der私钥的代码段
 *各种错误和违规行为。这不是图书馆的一部分
 *本身，因为允许的违规行为是任意选择的，并且
 *不要遵循或建立任何标准。
 *
 *它还包含用于在兼容的
 *态度。
 *
 *这些功能旨在与应用程序兼容。
 *需要BER编码密钥。当使用secp256k1特定
 *代码，通常由
 *图书馆足够了。
 **/


#ifndef SECP256K1_CONTRIB_BER_PRIVATEKEY_H
#define SECP256K1_CONTRIB_BER_PRIVATEKEY_H

#include <secp256k1.h>

#ifdef __cplusplus
extern "C" {
#endif

/*以der格式导出私钥。
 *
 *如果私钥有效，返回1。
 *args:ctx:指向上下文对象的指针，已初始化以便签名（无法
 *为空）
 *out:privkey：指向数组的指针，用于在BER中存储私钥。
 *应有279字节的空间，不能为空。
 *privkeylen：指向一个int的指针，其中private key的长度
 *将存储私钥。
 *in:seckey：指向要导出的32字节密钥的指针。
 *压缩：1如果密钥应导出到
 *压缩格式，否则为0
 *
 *此函数仅用于与
 *需要BER编码密钥。使用secp256k1特定代码时，
 *简单的32字节私钥就足够了。
 *
 *请注意，此功能不能保证正确的DER输出。它是
 *由Secp256k1_ec_privkey_import_der负责。
 **/

SECP256K1_WARN_UNUSED_RESULT int ec_privkey_export_der(
    const secp256k1_context* ctx,
    unsigned char *privkey,
    size_t *privkeylen,
    const unsigned char *seckey,
    int compressed
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(4);

/*以der格式导入私钥。
 *如果提取了私钥，则返回1。
 *args:ctx:指向上下文对象的指针（不能为空）。
 *out:seckey：指向用于存储私钥的32字节数组的指针。
 *（不能为空）。
 *in:privkey：指向der格式的私钥的指针（不能为空）。
 *privkeylen：指向privkey的der私钥的长度。
 *
 *此函数不仅接受严格的DER，甚至允许一些BER
 ＊违规行为。存储在DER编码的私钥中的公钥不是
 *验证是否正确，曲线参数也不正确。使用此功能
 *只有事先知道它应该包含secp256k1 private
 *密钥。
 **/

SECP256K1_WARN_UNUSED_RESULT int ec_privkey_import_der(
    const secp256k1_context* ctx,
    unsigned char *seckey,
    const unsigned char *privkey,
    size_t privkeylen
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

#ifdef __cplusplus
}
#endif

/*dif/*secp256k1_contrib_ber_privatekey_h*/
