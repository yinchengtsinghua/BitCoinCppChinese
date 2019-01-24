
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


#ifndef SECP256K1_BASIC_CONFIG_H
#define SECP256K1_BASIC_CONFIG_H

#ifdef USE_BASIC_CONFIG

#undef USE_ASM_X86_64
#undef USE_ENDOMORPHISM
#undef USE_FIELD_10X26
#undef USE_FIELD_5X52
#undef USE_FIELD_INV_BUILTIN
#undef USE_FIELD_INV_NUM
#undef USE_NUM_GMP
#undef USE_NUM_NONE
#undef USE_SCALAR_4X64
#undef USE_SCALAR_8X32
#undef USE_SCALAR_INV_BUILTIN
#undef USE_SCALAR_INV_NUM

#define USE_NUM_NONE 1
#define USE_FIELD_INV_BUILTIN 1
#define USE_SCALAR_INV_BUILTIN 1
#define USE_FIELD_10X26 1
#define USE_SCALAR_8X32 1

/*DIF/*使用_-basic_-config*/

endif/*secp256k1_-basic_-config_-h*/

