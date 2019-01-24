
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


#ifndef SECP256K1_GROUP_H
#define SECP256K1_GROUP_H

#include "num.h"
#include "field.h"

/*仿射坐标系中secp256k1曲线的一组元素。*/
typedef struct {
    secp256k1_fe x;
    secp256k1_fe y;
    /*无穷大；/*是否表示无穷大处的点*/
CSEP256K1GGE；

定义secp256k1_g e_const（a，b，c，d，e，f，g，h，i，j，k，l，m，n，o，p）secp256k1_f e_const（（a），（b），（c），（d），（e），（f），（g），（h）），secp256k1_f e_const（（i），（j），（k），（l），（m），（n），（o），（p）），0_
定义secp256k1_ge_const_infinity_secp256k1_fe_const（0，0，0，0，0，0，0，0，0，0），0，0，0），1_

/**secp256k1曲线的一个组元素，雅可比坐标。*/

typedef struct {
    /*p256k1_fe x；/*实际x:x/z^2*/
    secp256k1_fe y；/*实际y:y/z^3*/

    secp256k1_fe z;
    /*无穷大；/*是否表示无穷大处的点*/
secp256k1_gej；


define secp256k1_gej_const_secp256k1_fe_const（0，0，0，0，0，0，0，0，0），0，0，0），0，0，0），secp256k1_fe_const（0，0，0，0，0，0，0，0），1_

typedef结构
    secp256k1_fe_存储x；
    secp256k1_fe_存储y；
secp256k1_ge_存储；

定义secp256k1_g e_u存储常数（a、b、c、d、e、f、g、h、i、j、k、l、m、n、o、p）secp256k1_f e_u存储常数（（a）、（b）、（c）、（d）、（e）、（f）、（g）、（h））、secp256k1_f e u存储常数（（i）、（j）、（k）、（l）、（m）、（n）、（o）、（p））

定义secp256k1_ge_storage_const_ge t（t）secp256k1_fe_storage_const_ge t（t.x），secp256k1_fe_storage_const_ge t（t.y）

/**设置一个组元素等于给定x和y坐标的点。*/

static void secp256k1_ge_set_xy(secp256k1_ge *r, const secp256k1_fe *x, const secp256k1_fe *y);

/*设置一个组元素（仿射）等于给定x坐标的点
 *和y坐标，即二次剩余模p。返回值
 *如果存在具有给定x坐标的坐标，则为真。
 **/

static int secp256k1_ge_set_xquad(secp256k1_ge *r, const secp256k1_fe *x);

/*将一个组元素（仿射）设置为具有给定x坐标的点，并给定奇数。
 *对于Y，返回值表示结果是否有效。*/

static int secp256k1_ge_set_xo_var(secp256k1_ge *r, const secp256k1_fe *x, int odd);

/*检查组元素是否是无穷远处的点。*/
static int secp256k1_ge_is_infinity(const secp256k1_ge *a);

/*检查组元素是否有效（例如，在曲线上）。*/
static int secp256k1_ge_is_valid_var(const secp256k1_ge *a);

static void secp256k1_ge_neg(secp256k1_ge *r, const secp256k1_ge *a);

/*将一个组元素设置为雅可比坐标中给出的另一个组元素。*/
static void secp256k1_ge_set_gej(secp256k1_ge *r, secp256k1_gej *a);

/*将一批组元素设置为雅可比坐标中给定的输入*/
static void secp256k1_ge_set_all_gej_var(secp256k1_ge *r, const secp256k1_gej *a, size_t len, const secp256k1_callback *cb);

/*将一批组元素设置为Jacobian中给定的输入
 *坐标（已知z比）。ZR必须包含已知的z比，例如
 *该mul（a[i].z，zr[i+1]）==a[i+1].z.zr[0]被忽略。*/

static void secp256k1_ge_set_table_gej_var(secp256k1_ge *r, const secp256k1_gej *a, const secp256k1_fe *zr, size_t len);

/*将以雅可比坐标（已知z比）给出的批处理输入
 *相同的全局z“分母”。ZR必须包含已知的z比，例如
 *该mul（a[i].z，zr[i+1]）==a[i+1].z.zr[0]被忽略。X和Y
 *结果坐标存储在R中，常用的Z坐标为
 *存储在globalz中。*/

static void secp256k1_ge_globalz_set_table_gej(size_t len, secp256k1_ge *r, secp256k1_fe *globalz, const secp256k1_gej *a, const secp256k1_fe *zr);

/*设置一个组元素（Jacobian）等于无穷远处的点。*/
static void secp256k1_gej_set_infinity(secp256k1_gej *r);

/*将一组元素（雅可比）设置为仿射坐标中给出的另一组元素。*/
static void secp256k1_gej_set_ge(secp256k1_gej *r, const secp256k1_ge *a);

/*比较群元素的x坐标（雅可比）。*/
static int secp256k1_gej_eq_x_var(const secp256k1_fe *x, const secp256k1_gej *a);

/*将r设置为a的倒数（即，围绕x轴镜像）*/
static void secp256k1_gej_neg(secp256k1_gej *r, const secp256k1_gej *a);

/*检查组元素是否是无穷远处的点。*/
static int secp256k1_gej_is_infinity(const secp256k1_gej *a);

/*检查一个组元素的y坐标是否是二次余数。*/
static int secp256k1_gej_has_quad_y_var(const secp256k1_gej *a);

/*将r设置为a的双精度。如果r z r不为空，r->z=a->z**r z r（其中无穷大表示隐式z=0）。
 *a不能为零。固定时间。*/

static void secp256k1_gej_double_nonzero(secp256k1_gej *r, const secp256k1_gej *a, secp256k1_fe *rzr);

/*将r设置为a的双精度。如果r z r不为空，r->z=a->z**r z r（其中无穷大表示隐式z=0）。*/
static void secp256k1_gej_double_var(secp256k1_gej *r, const secp256k1_gej *a, secp256k1_fe *rzr);

/*设置r等于a和b的和。如果r z r不为空，r->z=a->z**r z r（在这种情况下，a不能为无穷大）。*/
static void secp256k1_gej_add_var(secp256k1_gej *r, const secp256k1_gej *a, const secp256k1_gej *b, secp256k1_fe *rzr);

/*设置r等于a和b的和（b在仿射坐标中给出，而不是无穷大）。*/
static void secp256k1_gej_add_ge(secp256k1_gej *r, const secp256k1_gej *a, const secp256k1_ge *b);

/*设r等于a和b的和（用仿射坐标给出b）。这个效率更高
    与secp256k1_gej_add_var相同，与secp256k1_gej_add_ge相同，但没有固定时间
    保证，b允许无穷大。如果r z r非空，r->z=a->z**r z r（在这种情况下a不能是无穷大）。*/

static void secp256k1_gej_add_ge_var(secp256k1_gej *r, const secp256k1_gej *a, const secp256k1_ge *b, secp256k1_fe *rzr);

/*设置r等于a和b的和（b的z坐标的倒数作为bzinv传递）。*/
static void secp256k1_gej_add_zinv_var(secp256k1_gej *r, const secp256k1_gej *a, const secp256k1_ge *b, const secp256k1_fe *bzinv);

#ifdef USE_ENDOMORPHISM
/*将r设置为等于lambda乘以a，其中lambda的选择方式使其非常快。*/
static void secp256k1_ge_mul_lambda(secp256k1_ge *r, const secp256k1_ge *a);
#endif

/*清除secp256k1_gej以防止敏感信息泄漏。*/
static void secp256k1_gej_clear(secp256k1_gej *r);

/*清除一个secp256k1_ge以防止敏感信息泄漏。*/
static void secp256k1_ge_clear(secp256k1_ge *r);

/*将group元素转换为存储类型。*/
static void secp256k1_ge_to_storage(secp256k1_ge_storage *r, const secp256k1_ge *a);

/*将组元素从存储类型转换回。*/
static void secp256k1_ge_from_storage(secp256k1_ge *r, const secp256k1_ge_storage *a);

/*如果标志为真，则将*R设置为*A；否则保留它。固定时间。*/
static void secp256k1_ge_storage_cmov(secp256k1_ge_storage *r, const secp256k1_ge_storage *a, int flag);

/*用b重新缩放雅可比点，该点必须为非零。固定时间。*/
static void secp256k1_gej_rescale(secp256k1_gej *r, const secp256k1_fe *b);

/*dif/*secp256k1_group_h*/
