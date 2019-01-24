
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
/***************************************************************
 *版权所有（c）2015 Pieter Wuille*
 *根据麻省理工学院软件许可证分发，请参阅随附的*
 *文件复制或http://www.opensource.org/licenses/mit license.php.*
 ***************************************************************/


/***
 *请不要直接链接此文件。它不是libsecp256k1的一部分
 *不保证其API、功能或
 *存在。使用此代码的项目应改为复制此标题
 *及其附带的.c文件直接进入其代码库。
 ***/


/*此文件定义了一个函数，该函数分析具有各种错误的der，并且
 ＊违规行为。这不是库本身的一部分，因为
 *违规行为是任意选择的，不遵守或建立任何
 ＊标准。
 *
 *在许多地方，不同的实现不仅接受
 *同一套有效签名，但也拒绝同一套签名。
 *实现这一目标的唯一方法是严格遵守标准，而不是
 *接受任何其他东西。
 *
 *尽管如此，有时需要与以下系统兼容：
 *使用不严格遵守命令的签名。下面的代码片段显示了
 *某些违规行为很容易得到支持。你可能需要调整它。
 *
 *请勿将其用于新系统。使用定义明确的DER或紧凑签名
 *如果您有选择（请参阅secp256k1_ecdsa_signature_parse_der和
 *secp256k1_ecdsa_signature_parse_compact）。
 *
 *支持的违规行为包括：
 *-所有数字都被解析为非负整数，即使X.609-0207
 *第8.3.3节规定整数始终编码为2
 *补语。
 *-整数的长度可以是0，即使8.3.1节说不能。
 *-接受填充过长的整数，违例部分
 *8 3.2。
 *-127字节长的描述符被接受，即使是节
 *8.1.3.5.c说他们不是。
 *—忽略签名内部或之后的尾随垃圾数据。
 *忽略序列的长度描述符。
 *
 *与例如openssl相比，不支持许多冲突：
 *—对序列或内部整数使用过长的标记描述符，
 *违反第8.1.2.2节。
 *-将基元整数编码为构造值，违反节
 *8 3.1。
 **/


#ifndef SECP256K1_CONTRIB_LAX_DER_PARSING_H
#define SECP256K1_CONTRIB_LAX_DER_PARSING_H

#include <secp256k1.h>

#ifdef __cplusplus
extern "C" {
#endif

/*以“lax der”格式解析签名
 *
 *当可以解析签名时返回1，否则返回0。
 *args:ctx:secp256k1上下文对象
 *out:sig：指向签名对象的指针
 *in:input：指向要分析的签名的指针。
 *inputlen：要输入的数组的长度。
 *
 *此函数将接受任何有效的der编码签名，即使
 *编码的数字超出范围。此外，它将接受签名
 *以各种方式违反订单规范。其目的是允许
 *比特币区块链验证，包括非der签名
 *在更新网络规则以强制执行订单之前。注意
 *受支持的冲突集是OpenSSL将要执行的操作的严格子集。
 *接受。
 *
 *调用后，SIG将始终初始化。如果分析失败或
 *编码的数字超出范围，用它进行签名验证是
 *确保每条消息和公钥都失败。
 **/

int ecdsa_signature_parse_der_lax(
    const secp256k1_context* ctx,
    secp256k1_ecdsa_signature* sig,
    const unsigned char *input,
    size_t inputlen
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

#ifdef __cplusplus
}
#endif

/*dif/*secp256k1_contrib_lax_der_analysis_h*/
