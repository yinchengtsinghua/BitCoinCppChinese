
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_SCRIPT_BITCOINCONSENSUS_H
#define BITCOIN_SCRIPT_BITCOINCONSENSUS_H

#include <stdint.h>

#if defined(BUILD_BITCOIN_INTERNAL) && defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
  #if defined(_WIN32)
    #if defined(DLL_EXPORT)
      #if defined(HAVE_FUNC_ATTRIBUTE_DLLEXPORT)
        #define EXPORT_SYMBOL __declspec(dllexport)
      #else
        #define EXPORT_SYMBOL
      #endif
    #endif
  #elif defined(HAVE_FUNC_ATTRIBUTE_VISIBILITY)
    #define EXPORT_SYMBOL __attribute__ ((visibility ("default")))
  #endif
#elif defined(MSC_VER) && !defined(STATIC_LIBBITCOINCONSENSUS)
  #define EXPORT_SYMBOL __declspec(dllimport)
#endif

#ifndef EXPORT_SYMBOL
  #define EXPORT_SYMBOL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BITCOINCONSENSUS_API_VER 1

typedef enum bitcoinconsensus_error_t
{
    bitcoinconsensus_ERR_OK = 0,
    bitcoinconsensus_ERR_TX_INDEX,
    bitcoinconsensus_ERR_TX_SIZE_MISMATCH,
    bitcoinconsensus_ERR_TX_DESERIALIZE,
    bitcoinconsensus_ERR_AMOUNT_REQUIRED,
    bitcoinconsensus_ERR_INVALID_FLAGS,
} bitcoinconsensus_error;

/*脚本验证标志*/
enum
{
    bitcoinconsensus_SCRIPT_FLAGS_VERIFY_NONE                = 0,
bitcoinconsensus_SCRIPT_FLAGS_VERIFY_P2SH                = (1U << 0), //评估p2sh（bip16）下标
bitcoinconsensus_SCRIPT_FLAGS_VERIFY_DERSIG              = (1U << 2), //严格遵守DER（BIP66）
bitcoinconsensus_SCRIPT_FLAGS_VERIFY_NULLDUMMY           = (1U << 4), //强制nulldummy（bip147）
bitcoinconsensus_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY = (1U << 9), //启用checklocktimeverify（bip65）
bitcoinconsensus_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY = (1U << 10), //启用CheckSequenceVerify（BIP112）
bitcoinconsensus_SCRIPT_FLAGS_VERIFY_WITNESS             = (1U << 11), //启用见证（bip141）
    bitcoinconsensus_SCRIPT_FLAGS_VERIFY_ALL                 = bitcoinconsensus_SCRIPT_FLAGS_VERIFY_P2SH | bitcoinconsensus_SCRIPT_FLAGS_VERIFY_DERSIG |
                                                               bitcoinconsensus_SCRIPT_FLAGS_VERIFY_NULLDUMMY | bitcoinconsensus_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY |
                                                               bitcoinconsensus_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY | bitcoinconsensus_SCRIPT_FLAGS_VERIFY_WITNESS
};

///如果序列化事务的输入nin由指向，则返回1
///tx正确使用scriptpubkey指向的scriptpubkey
///由标志指定的附加约束。
///if not nullptr，err将包含操作的错误/成功代码。
EXPORT_SYMBOL int bitcoinconsensus_verify_script(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen,
                                                 const unsigned char *txTo        , unsigned int txToLen,
                                                 unsigned int nIn, unsigned int flags, bitcoinconsensus_error* err);

EXPORT_SYMBOL int bitcoinconsensus_verify_script_with_amount(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen, int64_t amount,
                                    const unsigned char *txTo        , unsigned int txToLen,
                                    unsigned int nIn, unsigned int flags, bitcoinconsensus_error* err);

EXPORT_SYMBOL unsigned int bitcoinconsensus_version();

#ifdef __cplusplus
} //外部“C”
#endif

#undef EXPORT_SYMBOL

#endif //比特币脚本比特币共识
