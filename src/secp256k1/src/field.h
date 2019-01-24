
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
/***************************************************************
 *版权所有（c）2013、2014 Pieter Wuille*
 *根据麻省理工学院软件许可证分发，请参阅随附的*
 *文件复制或http://www.opensource.org/licenses/mit license.php.*
 ***************************************************************/


#ifndef SECP256K1_FIELD_H
#define SECP256K1_FIELD_H

/*字段元素模块。
 *
 *字段元素可以用多种方式表示，但代码访问
 *IT（和实现）需要考虑某些属性：
 *每个字段元素是否可以规范化。
 *每个字段元素都有一个大小，表示距离
 *其表示远离规范化。归一化元素
 *始终具有1的数量级，但1的数量级并不意味着
 ＊正态性。
 **/


#if defined HAVE_CONFIG_H
#include "libsecp256k1-config.h"
#endif

#if defined(USE_FIELD_10X26)
#include "field_10x26.h"
#elif defined(USE_FIELD_5X52)
#include "field_5x52.h"
#else
#error "Please select field implementation"
#endif

#include "util.h"

/*规范化字段元素。*/
static void secp256k1_fe_normalize(secp256k1_fe *r);

/*弱规格化字段元素：将其大小减小到1，但不完全规格化。*/
static void secp256k1_fe_normalize_weak(secp256k1_fe *r);

/*规范化一个字段元素，而不需要恒定的时间保证。*/
static void secp256k1_fe_normalize_var(secp256k1_fe *r);

/*验证字段元素是否表示零，即是否将规范化为零值。田地
 *实现可以有选择地规范化输入，但不应依赖于此。*/

static int secp256k1_fe_normalizes_to_zero(secp256k1_fe *r);

/*验证字段元素是否表示零，即是否将规范化为零值。田地
 *实现可以有选择地规范化输入，但不应依赖于此。*/

static int secp256k1_fe_normalizes_to_zero_var(secp256k1_fe *r);

/*将字段元素设置为小整数。结果字段元素被规范化。*/
static void secp256k1_fe_set_int(secp256k1_fe *r, int a);

/*将字段元素设置为零，初始化所有字段。*/
static void secp256k1_fe_clear(secp256k1_fe *a);

/*验证字段元素是否为零。要求将输入规范化。*/
static int secp256k1_fe_is_zero(const secp256k1_fe *a);

/*检查字段元素的“奇异性”。要求将输入规范化。*/
static int secp256k1_fe_is_odd(const secp256k1_fe *a);

/*比较两个字段元素。需要1级输入。*/
static int secp256k1_fe_equal(const secp256k1_fe *a, const secp256k1_fe *b);

/*与secp256k1_fe_相等，但可能是可变时间。*/
static int secp256k1_fe_equal_var(const secp256k1_fe *a, const secp256k1_fe *b);

/*比较两个字段元素。要求对两个输入进行规范化*/
static int secp256k1_fe_cmp_var(const secp256k1_fe *a, const secp256k1_fe *b);

/*将字段元素设置为32字节的big endian值。如果成功，结果字段元素将被规范化。*/
static int secp256k1_fe_set_b32(secp256k1_fe *r, const unsigned char *a);

/*将field元素转换为32字节的big endian值。要求将输入规范化*/
static void secp256k1_fe_get_b32(unsigned char *r, const secp256k1_fe *a);

/*设置一个字段元素等于另一个元素的加性反转。获取输入的最大值
 *作为一个论点。输出的幅度要大一点。*/

static void secp256k1_fe_negate(secp256k1_fe *r, const secp256k1_fe *a, int m);

/*将传递的字段元素与一个小整数常量相乘。用这个数乘以这个数
 *小整数。*/

static void secp256k1_fe_mul_int(secp256k1_fe *r, int a);

/*向另一个字段元素添加字段元素。结果将输入的大小之和作为大小。*/
static void secp256k1_fe_add(secp256k1_fe *r, const secp256k1_fe *a);

/*将字段元素设置为其他两个元素的乘积。要求输入的幅度至多为8。
 *输出幅度为1（但不保证正常化）。*/

static void secp256k1_fe_mul(secp256k1_fe *r, const secp256k1_fe *a, const secp256k1_fe * SECP256K1_RESTRICT b);

/*将字段元素设置为另一个元素的平方。要求输入的幅度至多为8。
 *输出幅度为1（但不保证正常化）。*/

static void secp256k1_fe_sqr(secp256k1_fe *r, const secp256k1_fe *a);

/*如果a有一个平方根，它在r中计算，并返回1。如果A不
 *有一个平方根，计算它的负根，返回0。
 *输入的幅度最多为8。输出幅度为1（但不是
 *保证标准化）。r的结果总是一个平方
 *本身。*/

static int secp256k1_fe_sqrt(secp256k1_fe *r, const secp256k1_fe *a);

/*检查字段元素是否为二次余数。*/
static int secp256k1_fe_is_quad_var(const secp256k1_fe *a);

/*将字段元素设置为另一个元素的（模块化）倒数。要求输入的大小为
 最多8个。输出幅度为1（但不保证被标准化）。*/

static void secp256k1_fe_inv(secp256k1_fe *r, const secp256k1_fe *a);

/*可能更快版本的secp256k1_fe_inv，无需持续时间保证。*/
static void secp256k1_fe_inv_var(secp256k1_fe *r, const secp256k1_fe *a);

/*计算一批字段元素的（模块化）倒数。要求输入的大小为
 最多8个。输出幅度为1（但不保证被标准化）。输入和
 *输出在内存中不能重叠。*/

static void secp256k1_fe_inv_all_var(secp256k1_fe *r, const secp256k1_fe *a, size_t len);

/*将字段元素转换为存储类型。*/
static void secp256k1_fe_to_storage(secp256k1_fe_storage *r, const secp256k1_fe *a);

/*将字段元素从存储类型转换回。*/
static void secp256k1_fe_from_storage(secp256k1_fe *r, const secp256k1_fe_storage *a);

/*如果标志为真，则将*R设置为*A；否则保留它。固定时间。*/
static void secp256k1_fe_storage_cmov(secp256k1_fe_storage *r, const secp256k1_fe_storage *a, int flag);

/*如果标志为真，则将*R设置为*A；否则保留它。固定时间。*/
static void secp256k1_fe_cmov(secp256k1_fe *r, const secp256k1_fe *a, int flag);

/*dif/*secp256k1_field_h*/
