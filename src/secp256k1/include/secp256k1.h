
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
#ifndef SECP256K1_H
#define SECP256K1_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/*这些规则指定API调用中参数的顺序：
 *
 * 1。上下文指针放在第一位，后跟输出参数，组合
 *输出/输入参数，最后只输入参数。
 * 2。数组长度始终紧跟其长度的参数
 *他们描述，即使这违反了规则1。
 * 3。在out/out in/in组中，指向通常生成的数据的指针。
 *以后先去。这意味着：签名、公开、私人，
 *信息、公共密钥、密钥、调整。
 * 4。不是数据指针的参数最后一个，从更复杂到更少
 *复杂：函数指针、算法名称、消息、空指针，
 *计数、标记、布尔值。
 * 5。不透明的数据指针跟随要传递给它们的函数指针。
 **/


/*包含上下文信息（预计算表等）的不透明数据结构。
 *
 *上下文结构的目的是缓存大型预计算数据表。
 *构建和维护随机化数据的成本很高。
 *用于盲板。
 *
 *不要为每个操作创建新的上下文对象，因为构造是
 *比所有其他API调用慢得多（比ECDSA慢约100倍）
 *验证）。
 *
 *构造的上下文可以从多个线程安全地使用
 *同时调用，但API调用将非常量指针带到上下文
 *需要对其进行独占访问。尤其是在这种情况下
 *secp256k1_context_destroy和secp256k1_context_随机化。
 *
 *关于随机化，要么在创建时执行一次（在这种情况下
 *其他调用不需要任何锁定），或者使用读写锁定。
 **/

typedef struct secp256k1_context_struct secp256k1_context;

/*持有已解析和有效公钥的不透明数据结构。
 *
 *内部数据的准确表示是实现定义的，而不是
 *保证在不同平台或版本之间可移植。它是
 *但是保证大小为64字节，并且可以安全地复制/移动。
 *如果需要转换为适合存储、传输或
 *比较，使用secp256k1_ec_pubkey_serialize和secp256k1_ec_pubkey_parse。
 **/

typedef struct {
    unsigned char data[64];
} secp256k1_pubkey;

/*包含已解析ECDSA签名的不透明数据。
 *
 *内部数据的准确表示是实现定义的，而不是
 *保证在不同平台或版本之间可移植。它是
 *但是保证大小为64字节，并且可以安全地复制/移动。
 *如果需要转换为适合存储、传输或
 *比较，使用secp256k1_ecdsa_signature_serialize_uux和
 *secp256k1_ecdsa_signature_parse_ux函数。
 **/

typedef struct {
    unsigned char data[64];
} secp256k1_ecdsa_signature;

/*指向函数的指针，用于确定地生成一个nonce。
 *
 *如果成功生成nonce，则返回1。0将导致签名失败。
 *out:nonce32：指向要由函数填充的32字节数组的指针。
 *in:msg32:正在验证的32字节消息哈希（不会为空）
 *key32：指向32字节密钥的指针（不为空）
 *algo16：指向描述签名的16字节数组的指针
 *算法（对于ECDSA，兼容性为空）。
 *数据：通过的任意数据指针。
 *尝试：我们尝试查找一个nonce的迭代次数。
 *这几乎总是0，但尝试值不同
 *需要产生不同的nonce。
 *
 *除了测试用例，此函数应计算
 *消息、算法、密钥和尝试。
 **/

typedef int (*secp256k1_nonce_function)(
    unsigned char *nonce32,
    const unsigned char *msg32,
    const unsigned char *key32,
    const unsigned char *algo16,
    void *data,
    unsigned int attempt
);

# if !defined(SECP256K1_GNUC_PREREQ)
#  if defined(__GNUC__)&&defined(__GNUC_MINOR__)
#   define SECP256K1_GNUC_PREREQ(_maj,_min) \
 ((__GNUC__<<16)+__GNUC_MINOR__>=((_maj)<<16)+(_min))
#  else
#   define SECP256K1_GNUC_PREREQ(_maj,_min) 0
#  endif
# endif

# if (!defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L) )
#  if SECP256K1_GNUC_PREREQ(2,7)
#   define SECP256K1_INLINE __inline__
#  elif (defined(_MSC_VER))
#   define SECP256K1_INLINE __inline
#  else
#   define SECP256K1_INLINE
#  endif
# else
#  define SECP256K1_INLINE inline
# endif

#ifndef SECP256K1_API
# if defined(_WIN32)
#  ifdef SECP256K1_BUILD
#   define SECP256K1_API __declspec(dllexport)
#  else
#   define SECP256K1_API
#  endif
# elif defined(__GNUC__) && defined(SECP256K1_BUILD)
#  define SECP256K1_API __attribute__ ((visibility ("default")))
# else
#  define SECP256K1_API
# endif
#endif

/*警告属性
  *如果设置secp256k1_build以避免编译器优化，则不使用非空值。
  *一些偏执的空检查。*/

# if defined(__GNUC__) && SECP256K1_GNUC_PREREQ(3, 4)
#  define SECP256K1_WARN_UNUSED_RESULT __attribute__ ((__warn_unused_result__))
# else
#  define SECP256K1_WARN_UNUSED_RESULT
# endif
# if !defined(SECP256K1_BUILD) && defined(__GNUC__) && SECP256K1_GNUC_PREREQ(3, 4)
#  define SECP256K1_ARG_NONNULL(_x)  __attribute__ ((__nonnull__(_x)))
# else
#  define SECP256K1_ARG_NONNULL(_x)
# endif

/*所有标志的低8位表示它们的用途。不要直接使用。*/
#define SECP256K1_FLAGS_TYPE_MASK ((1 << 8) - 1)
#define SECP256K1_FLAGS_TYPE_CONTEXT (1 << 0)
#define SECP256K1_FLAGS_TYPE_COMPRESSION (1 << 1)
/*较高的位包含实际数据。不要直接使用。*/
#define SECP256K1_FLAGS_BIT_CONTEXT_VERIFY (1 << 8)
#define SECP256K1_FLAGS_BIT_CONTEXT_SIGN (1 << 9)
#define SECP256K1_FLAGS_BIT_COMPRESSION (1 << 8)

/*传递到secp256k1_context_create的标志。*/
#define SECP256K1_CONTEXT_VERIFY (SECP256K1_FLAGS_TYPE_CONTEXT | SECP256K1_FLAGS_BIT_CONTEXT_VERIFY)
#define SECP256K1_CONTEXT_SIGN (SECP256K1_FLAGS_TYPE_CONTEXT | SECP256K1_FLAGS_BIT_CONTEXT_SIGN)
#define SECP256K1_CONTEXT_NONE (SECP256K1_FLAGS_TYPE_CONTEXT)

/*传递到secp256k1_ec_pubkey_serialize和secp256k1_ec_privkey_export的标志。*/
#define SECP256K1_EC_COMPRESSED (SECP256K1_FLAGS_TYPE_COMPRESSION | SECP256K1_FLAGS_BIT_COMPRESSION)
#define SECP256K1_EC_UNCOMPRESSED (SECP256K1_FLAGS_TYPE_COMPRESSION)

/*用于为特定目的标记各种编码曲线点的前缀字节*/
#define SECP256K1_TAG_PUBKEY_EVEN 0x02
#define SECP256K1_TAG_PUBKEY_ODD 0x03
#define SECP256K1_TAG_PUBKEY_UNCOMPRESSED 0x04
#define SECP256K1_TAG_PUBKEY_HYBRID_EVEN 0x06
#define SECP256K1_TAG_PUBKEY_HYBRID_ODD 0x07

/*创建secp256k1上下文对象。
 
 *返回：新创建的上下文对象。
 *in:flags：要初始化上下文的哪些部分。
 *
 *另见secp256k1_context_randomize。
 **/

SECP256K1_API secp256k1_context* secp256k1_context_create(
    unsigned int flags
) SECP256K1_WARN_UNUSED_RESULT;

/*复制secp256k1上下文对象。
 *
 *返回：新创建的上下文对象。
 *args:ctx:要复制的现有上下文（不能为空）
 **/

SECP256K1_API secp256k1_context* secp256k1_context_clone(
    const secp256k1_context* ctx
) SECP256K1_ARG_NONNULL(1) SECP256K1_WARN_UNUSED_RESULT;

/*销毁secp256k1上下文对象。
 *
 *以后不能使用上下文指针。
 *args:ctx:要销毁的现有上下文（不能为空）
 **/

SECP256K1_API void secp256k1_context_destroy(
    secp256k1_context* ctx
);

/*设置在传递非法参数时要调用的回调函数
 *API调用。它只会触发所提到的违规行为
 *在标题中明确显示。
 *
 *其理念是不应通过
 *特定的返回值，因为调用代码不应该有分支来处理
 *此代码本身被破坏的情况。
 *
 *另一方面，在调试阶段，您需要了解
 *这样的错误，违约（崩溃）可能是不可取的。
 *当触发此回调时，所调用的API函数不能保证
 *导致崩溃，尽管它的返回值和输出参数是
 *未定义。
 *
 *args:ctx:现有上下文对象（不能为空）
 *in:fun：当非法参数为
 *传递给API，获取消息和不透明指针
 *（NULL还原调用abort的默认处理程序）。
 *数据：不透明的指针，传递给上面的乐趣。
 **/

SECP256K1_API void secp256k1_context_set_illegal_callback(
    secp256k1_context* ctx,
    void (*fun)(const char* message, void* data),
    const void* data
) SECP256K1_ARG_NONNULL(1);

/*设置内部一致性检查时要调用的回调函数
 *失败。默认值是崩溃。
 *
 *这只能在硬件故障、编译错误时触发，
 *内存损坏、库中的严重错误或其他错误可能
 *否则会导致未定义的行为。它不会因为
 *API使用不正确（请参阅secp256k1_context_set_invalid_callback
 *为此）。此回调返回后，可能会发生任何情况，包括
 *崩溃。
 *
 *args:ctx:现有上下文对象（不能为空）
 *in:fun：当发生内部错误时，指向要调用的函数的指针，
 *获取一条消息和一个不透明指针（空值将恢复默认值
 *调用abort的处理程序）。
 *数据：不透明的指针，传递给上面的乐趣。
 **/

SECP256K1_API void secp256k1_context_set_error_callback(
    secp256k1_context* ctx,
    void (*fun)(const char* message, void* data),
    const void* data
) SECP256K1_ARG_NONNULL(1);

/*将长度可变的公钥分析到pubkey对象中。
 *
 *如果公钥完全有效，则返回1。
 *如果公钥无法分析或无效，则返回0。
 *args:ctx:secp256k1上下文对象。
 *out:pubkey：指向pubkey对象的指针。如果返回1，则将其设置为
 *输入的解析版本。如果不是，则其值未定义。
 *in:input:指向序列化公钥的指针
 *inputlen：输入指向的数组长度。
 *
 *此函数支持解析压缩（33字节，头字节0x02或
 *0x03）、未压缩（65字节、头字节0x04）或混合（65字节、头
 *字节0x06或0x07）格式化公钥。
 **/

SECP256K1_API SECP256K1_WARN_UNUSED_RESULT int secp256k1_ec_pubkey_parse(
    const secp256k1_context* ctx,
    secp256k1_pubkey* pubkey,
    const unsigned char *input,
    size_t inputlen
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

/*将pubkey对象序列化为序列化的字节序列。
 *
 *始终返回1。
 *args:ctx:secp256k1上下文对象。
 *out：输出：指向65字节（如果压缩=0）或33字节（如果
 *compressed==1）用于放置序列化密钥的字节数组
 *英寸
 *in/out:outputlen：指向整数的指针，该整数最初设置为
 *输出大小，并被写入的
 *尺寸。
 *in:pubkey：指向secp256k1_pubkey的指针，其中包含
 *已初始化公钥。
 *标志：如果序列化应该在
 *压缩格式，否则secp256k1_ec_未压缩。
 **/

SECP256K1_API int secp256k1_ec_pubkey_serialize(
    const secp256k1_context* ctx,
    unsigned char *output,
    size_t *outputlen,
    const secp256k1_pubkey* pubkey,
    unsigned int flags
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(4);

/*以紧凑（64字节）格式分析ECDSA签名。
 *
 *当可以解析签名时返回1，否则返回0。
 *args:ctx:secp256k1上下文对象
 *out:sig：指向签名对象的指针
 *in:input64：指向要分析的64字节数组的指针
 *
 *签名必须包含一个32字节的big endian r值，后跟一个
 *32字节的big endian s值。如果r或s不在[0..order-1]范围内，则
 *编码无效。编码中允许使用值为0的r和s。
 *
 *调用后，SIG将始终初始化。如果分析失败或R或
 *s为零，由此产生的sig值保证任何
 *消息和公钥。
 **/

SECP256K1_API int secp256k1_ecdsa_signature_parse_compact(
    const secp256k1_context* ctx,
    secp256k1_ecdsa_signature* sig,
    const unsigned char *input64
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

/*分析der ecdsa签名。
 *
 *当可以解析签名时返回1，否则返回0。
 *args:ctx:secp256k1上下文对象
 *out:sig：指向签名对象的指针
 *in:input：指向要分析的签名的指针。
 
 
 
 
 *
 *调用后，SIG将始终初始化。如果分析失败或
 *编码的数字超出范围，用它进行签名验证是
 *确保每条消息和公钥都失败。
 **/

SECP256K1_API int secp256k1_ecdsa_signature_parse_der(
    const secp256k1_context* ctx,
    secp256k1_ecdsa_signature* sig,
    const unsigned char *input,
    size_t inputlen
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

/*以der格式序列化ECDSA签名。
 *
 *返回：1如果有足够的空间进行序列化，则返回0，否则返回0。
 *args:ctx:secp256k1上下文对象
 *out:output：指向存储顺序序列化的数组的指针。
 *输入/输出：输出长度：指向长度整数的指针。最初，这个整数
 *应设置为输出长度。通话结束后
 *它将被设置为序列化的长度（偶数
 *如果返回0）。
 *in:sig：指向已初始化签名对象的指针。
 **/

SECP256K1_API int secp256k1_ecdsa_signature_serialize_der(
    const secp256k1_context* ctx,
    unsigned char *output,
    size_t *outputlen,
    const secp256k1_ecdsa_signature* sig
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(4);

/*以压缩（64字节）格式序列化ECDSA签名。
 *
 *返回：1
 *args:ctx:secp256k1上下文对象
 *out:output64：指向64字节数组的指针，用于存储压缩序列化
 *in:sig：指向已初始化签名对象的指针。
 *
 *有关编码的详细信息，请参阅secp256k1_ecdsa_signature_parse_compact。
 **/

SECP256K1_API int secp256k1_ecdsa_signature_serialize_compact(
    const secp256k1_context* ctx,
    unsigned char *output64,
    const secp256k1_ecdsa_signature* sig
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

/*验证ECDSA签名。
 *
 *返回：1:正确签名
 *0:签名不正确或不可解析
 *args:ctx:secp256k1上下文对象，已初始化以供验证。
 *in:sig：正在验证的签名（不能为空）
 *msg32:正在验证的32字节消息哈希（不能为空）
 *pubkey：指向要验证的已初始化公钥的指针（不能为空）
 *
 *为避免接受可延展性签名，仅在低层S中使用ECDSA签名。
 *接受表格。
 *
 *如果您需要接受来自不符合此要求的来源的ECDSA签名
 *规则，应用secp256k1_ecdsa_signature_normalize到签名之前
 *验证，但要注意这样做会产生可延展的签名。
 *
 *有关详细信息，请参阅该函数的注释。
 **/

SECP256K1_API SECP256K1_WARN_UNUSED_RESULT int secp256k1_ecdsa_verify(
    const secp256k1_context* ctx,
    const secp256k1_ecdsa_signature *sig,
    const unsigned char *msg32,
    const secp256k1_pubkey *pubkey
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(4);

/*将签名转换为规范化的lower-s形式。
 *
 *返回：1如果sigin未规范化，则返回0。
 *args:ctx:secp256k1上下文对象
 *out:sigout：指向签名的指针，用于填充规范化表单，
 *如果输入已经规范化，则复制。（可以为空）
 *您只关心输入是否已经
 *标准化）。
 *in:sigin：指向要检查/规范化的签名的指针（不能为空，
 *可以与sigout相同）
 *
 *使用ECDSA，第三方可以伪造相同的第二个不同签名。
 *消息，给出一个初始签名，但不知道密钥。这个
 *通过将s值模取为曲线的阶数，“翻转”来完成。
 *不包含在签名中的随机点R的符号。
 *
 *伪造同一条消息并不是普遍存在的问题，而是在系统中。
 *如果签名的消息可塑性或唯一性很重要，则可以
 *引起问题。强制签名的所有验证程序都可以阻止此伪造
 *使用标准化形式。
 *
 *当
 *使用可变长度编码（如DER），验证成本低，
 *让它成为一个好的选择。始终使用较低级别的安全性是有保证的，因为
 *任何人都可以在事实发生后修改签名以执行此操作。
 *无论如何都是财产。
 *
 *较低的S值始终在0x1和
 *0x7fffffffffffffffffffffffffff5d576e7357a4501dfe92f46681b20a0，
 *包含。
 *
 *目前还不知道其他形式的ECDSA延展性，也不太可能，但是
 *没有正式的证据证明ECDSA，即使有此附加限制，
 *没有其他延展性。常用的序列化方案也将
 *接受各种非唯一编码，因此应注意
 *应用程序需要属性。
 *
 *secp256k1_ecdsa_sign函数默认将在
 *Lower-S Form和Secp256k1_ecdsa_verify将不接受其他人。万一
 *签名来自无法强制执行此属性的系统，
 *验证前必须调用secp256k1_ecdsa_signature_normalize。
 **/

SECP256K1_API int secp256k1_ecdsa_signature_normalize(
    const secp256k1_context* ctx,
    secp256k1_ecdsa_signature *sigout,
    const secp256k1_ecdsa_signature *sigin
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(3);

/*rfc6979的一种实现（使用hmac-sha256）作为nonce生成函数。
 *如果传递了数据指针，则假定它是指向
 *额外熵。
 **/

SECP256K1_API extern const secp256k1_nonce_function secp256k1_nonce_function_rfc6979;

/*默认的安全nonce生成函数（当前等于secp256k1_nonce_函数_rfc6979）。*/
SECP256K1_API extern const secp256k1_nonce_function secp256k1_nonce_function_default;

/*创建ECDSA签名。
 *
 *返回：1:已创建签名
 *0:nonce生成函数失败，或者私钥无效。
 *args:ctx:指向上下文对象的指针，已初始化以便签名（不能为空）
 *out:sig：指向将放置签名的数组的指针（不能为空）
 *in:msg32:正在签名的32字节消息哈希（不能为空）
 *seckey：指向32字节密钥的指针（不能为空）
 *noncefp：指向nonce生成函数的指针。如果为空，则使用secp256k1_nonce_函数_default
 *data：指向nonce生成函数使用的任意数据的指针（可以为空）
 *
 *创建的签名始终采用较低的S格式。见
 *secp256k1_ecdsa_signature_normalize了解更多详细信息。
 **/

SECP256K1_API int secp256k1_ecdsa_sign(
    const secp256k1_context* ctx,
    secp256k1_ecdsa_signature *sig,
    const unsigned char *msg32,
    const unsigned char *seckey,
    secp256k1_nonce_function noncefp,
    const void *ndata
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(4);

/*验证ECDSA密钥。
 *
 *返回：1:密钥有效
 *0:密钥无效
 *args:ctx:指向上下文对象的指针（不能为空）
 *in:seckey：指向32字节密钥的指针（不能为空）
 **/

SECP256K1_API SECP256K1_WARN_UNUSED_RESULT int secp256k1_ec_seckey_verify(
    const secp256k1_context* ctx,
    const unsigned char *seckey
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2);

/*计算密钥的公钥。
 *
 *返回：1:秘密有效，公钥存储
 *0:机密无效，请重试。
 *args:ctx:指向上下文对象的指针，已初始化以便签名（不能为空）
 *out:pubkey：指向创建的公钥的指针（不能为空）
 *in:seckey：指向32字节私钥的指针（不能为空）
 **/

SECP256K1_API SECP256K1_WARN_UNUSED_RESULT int secp256k1_ec_pubkey_create(
    const secp256k1_context* ctx,
    secp256k1_pubkey *pubkey,
    const unsigned char *seckey
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

/*在适当的位置否定私钥。
 *
 *退换货：总是1件
 *args:ctx:指向上下文对象的指针
 *In/Out:PubKey：指向要否定的公钥的指针（不能为空）
 **/

SECP256K1_API SECP256K1_WARN_UNUSED_RESULT int secp256k1_ec_privkey_negate(
    const secp256k1_context* ctx,
    unsigned char *seckey
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2);

/*取消已到位的公钥。
 *
 *退换货：总是1件
 *args:ctx:指向上下文对象的指针
 *In/Out:PubKey：指向要否定的公钥的指针（不能为空）
 **/

SECP256K1_API SECP256K1_WARN_UNUSED_RESULT int secp256k1_ec_pubkey_negate(
    const secp256k1_context* ctx,
    secp256k1_pubkey *pubkey
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2);

/*通过添加tweak来调整私钥。
 *如果调整超出范围，则返回0（2^128中约有1的概率用于
 *均匀随机32字节数组，或者如果生成的私钥
 *将无效（仅当tweak是
 *私钥）。1，否则。
 *args:ctx:指向上下文对象的指针（不能为空）。
 *输入/输出：seckey：指向32字节私钥的指针。
 *in:tweak：指向32字节调整的指针。
 **/

SECP256K1_API SECP256K1_WARN_UNUSED_RESULT int secp256k1_ec_privkey_tweak_add(
    const secp256k1_context* ctx,
    unsigned char *seckey,
    const unsigned char *tweak
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

/*通过添加tweak times来调整公钥。
 *如果调整超出范围，则返回0（2^128中约有1的概率用于
 *均匀随机32字节数组，或者如果生成的公钥
 *将无效（仅当tweak是
 *对应的私钥）。1，否则。
 *args:ctx:指向为验证而初始化的上下文对象的指针
 *（不能为空）。
 *输入/输出：pubkey：指向公钥对象的指针。
 *in:tweak：指向32字节调整的指针。
 **/

SECP256K1_API SECP256K1_WARN_UNUSED_RESULT int secp256k1_ec_pubkey_tweak_add(
    const secp256k1_context* ctx,
    secp256k1_pubkey *pubkey,
    const unsigned char *tweak
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

/*通过将私钥乘以一个参数来调整它。
 *如果调整超出范围，则返回0（2^128中约有1的概率用于
 *均匀随机32字节数组，或等于零。1，否则。
 *args:ctx:指向上下文对象的指针（不能为空）。
 *输入/输出：seckey：指向32字节私钥的指针。
 *in:tweak：指向32字节调整的指针。
 **/

SECP256K1_API SECP256K1_WARN_UNUSED_RESULT int secp256k1_ec_privkey_tweak_mul(
    const secp256k1_context* ctx,
    unsigned char *seckey,
    const unsigned char *tweak
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

/*通过将公钥乘以tweak值来调整公钥。
 *如果调整超出范围，则返回0（2^128中约有1的概率用于
 *均匀随机32字节数组，或等于零。1，否则。
 *args:ctx:指向为验证而初始化的上下文对象的指针
 *（不能为空）。
 *输入/输出：pubkey：指向公钥对象的指针。
 *in:tweak：指向32字节调整的指针。
 **/

SECP256K1_API SECP256K1_WARN_UNUSED_RESULT int secp256k1_ec_pubkey_tweak_mul(
    const secp256k1_context* ctx,
    secp256k1_pubkey *pubkey,
    const unsigned char *tweak
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

/*更新上下文随机化以防止侧通道泄漏。
 *返回：1:已成功更新随机化
 *0:错误
 *args:ctx:指向上下文对象的指针（不能为空）
 *in:seed32：指向32字节随机种子的指针（空值重置为初始状态）
 *
 *当secp256k1代码被写入恒定时间时，无论什么秘密。
 *值是，未来的编译器可能输出的代码不是，
 *并且CPU可能不会发射相同的无线电频率或绘制相同的无线电频率。
 *所有数值的功率。
 *
 *此函数提供一个结合到盲值中的种子：即
 *每次乘法前加盲值（然后去掉），所以
 *它不会影响功能结果，但可以抵御
 *依赖于任何与输入相关的行为。
 *
 *您应该在secp256k1_context_create或
 *secp256k1_context_clone，以后可反复调用。
 **/

SECP256K1_API SECP256K1_WARN_UNUSED_RESULT int secp256k1_context_randomize(
    secp256k1_context* ctx,
    const unsigned char *seed32
) SECP256K1_ARG_NONNULL(1);

/*把一些公钥加在一起。
 *返回：1:公钥之和有效。
 *0:公钥之和无效。
 *args:ctx:指向上下文对象的指针
 *out:out：指向公钥对象的指针，用于放置生成的公钥。
 *（不能为空）
 *in:ins:指向指向公钥指针数组的指针（不能为空）
 *n：要加在一起的公钥数（必须至少为1）
 **/

SECP256K1_API SECP256K1_WARN_UNUSED_RESULT int secp256k1_ec_pubkey_combine(
    const secp256k1_context* ctx,
    secp256k1_pubkey *out,
    const secp256k1_pubkey * const * ins,
    size_t n
) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

#ifdef __cplusplus
}
#endif

/*dif/*秒256k1_h*/
