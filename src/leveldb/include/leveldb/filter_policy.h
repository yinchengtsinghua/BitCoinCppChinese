
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2012 LevelDB作者。版权所有。
//此源代码的使用受可以
//在许可证文件中找到。有关参与者的名称，请参阅作者文件。
//
//可以使用自定义的filterpolicy对象配置数据库。
//此对象负责从集合创建小筛选器
//钥匙。这些过滤器存储在LEVELDB中，并被查询
//由LEVELDB自动决定是否读取
//来自磁盘的信息。在许多情况下，过滤器可以减少
//从一把磁盘搜索到一个磁盘搜索的次数
//d::GET（）调用。
//
//大多数人都希望使用内置的Bloom过滤器支持（请参见
//下面是newbloomfilterpolicy（）。

#ifndef STORAGE_LEVELDB_INCLUDE_FILTER_POLICY_H_
#define STORAGE_LEVELDB_INCLUDE_FILTER_POLICY_H_

#include <string>

namespace leveldb {

class Slice;

class FilterPolicy {
 public:
  virtual ~FilterPolicy();

//返回此策略的名称。注意如果过滤器编码
//以不兼容的方式更改，此方法返回的名称
//必须更改。否则，旧的不兼容筛选器可能是
//传递给此类型的方法。
  virtual const char* Name() const = 0;

//键[0，n-1]包含键列表（可能有重复项）
//根据用户提供的比较器订购。
//附加一个将键[0，n-1]汇总到*dst的过滤器。
//
//警告：不要更改*dst的初始内容。相反，
//将新构造的过滤器附加到*dst。
  virtual void CreateFilter(const Slice* keys, int n, std::string* dst)
      const = 0;

//“filter”包含前面调用附加到
//类上的CreateFilter（）。如果
//密钥在传递给CreateFilter（）的密钥列表中。
//如果密钥不在
//列表，但它的目标应该是以很高的概率返回false。
  virtual bool KeyMayMatch(const Slice& key, const Slice& filter) const = 0;
};

//返回一个新的筛选器策略，该策略使用大约
//每个键的指定位数。一个很好的每键位值
//是10，这会产生一个假阳性率为1%的过滤器。
//
//调用方必须在使用
//结果已关闭。
//
//注意：如果您使用的是忽略某些部分的自定义比较器
//在要比较的密钥中，不能使用newbloomfilterpolicy（）。
//并且必须提供自己的filterpolicy，它还忽略
//键的相应部分。例如，如果比较器
//忽略尾随空格，使用
//不忽略的filterpolicy（如newbloomfilterpolicy）
//键中的尾随空格。
extern const FilterPolicy* NewBloomFilterPolicy(int bits_per_key);

}

#endif  //存储级别包括过滤策略
