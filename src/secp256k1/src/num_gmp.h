
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


#ifndef SECP256K1_NUM_REPR_H
#define SECP256K1_NUM_REPR_H

#include <gmp.h>

#define NUM_LIMBS ((256+GMP_NUMB_BITS-1)/GMP_NUMB_BITS)

typedef struct {
    mp_limb_t data[2*NUM_LIMBS];
    int neg;
    int limbs;
} secp256k1_num;

/*dif/*secp256k1_num_repr_h*/
