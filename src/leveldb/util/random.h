
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011 LevelDB作者。版权所有。
//此源代码的使用受可以
//在许可证文件中找到。有关参与者的名称，请参阅作者文件。

#ifndef STORAGE_LEVELDB_UTIL_RANDOM_H_
#define STORAGE_LEVELDB_UTIL_RANDOM_H_

#include <stdint.h>

namespace leveldb {

//一个非常简单的随机数生成器。不是特别擅长
//生成真正的随机位，但足以满足我们的需求
//包裹。
class Random {
 private:
  uint32_t seed_;
 public:
  explicit Random(uint32_t s) : seed_(s & 0x7fffffffu) {
//避免坏种子。
    if (seed_ == 0 || seed_ == 2147483647L) {
      seed_ = 1;
    }
  }
  uint32_t Next() {
static const uint32_t M = 2147483647L;   //2 ^ 31 -1
static const uint64_t A = 16807;  //位14、8、7、5、2、1、0
//我们正在计算
//Seed_uu=（Seed_uux）%m，其中m=2^31-1
//
//种子_u不能为零或m，否则所有后续计算值
//分别为0或m。对于所有其他值，种子将结束
//在[1，m-1]中的每个数字上循环
    uint64_t product = seed_ * A;

//使用（（x<<31）=x这一事实计算（积%m）。
    seed_ = static_cast<uint32_t>((product >> 31) + (product & M));
//第一个约简可能溢出1位，因此我们可能需要
//重复。mod==m不可能；使用>允许更快
//符号位测试。
    if (seed_ > M) {
      seed_ -= M;
    }
    return seed_;
  }
//返回范围[0..n-1]内均匀分布的值
//要求：n＞0
  uint32_t Uniform(int n) { return Next() % n; }

//随机返回时间的真~“1/n”，否则返回假。
//要求：n＞0
  bool OneIn(int n) { return (Next() % n) == 0; }

//倾斜：从范围[0，max_log]中均匀地选取“base”，然后
//返回“基”随机位。效果是在
//范围[0,2^max_log-1]，指数偏向较小的数字。
  uint32_t Skewed(int max_log) {
    return Uniform(1 << Uniform(max_log + 1));
  }
};

}  //命名空间级别数据库

#endif  //存储级别
