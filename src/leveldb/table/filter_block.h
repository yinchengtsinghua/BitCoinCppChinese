
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
//筛选块存储在表文件末尾附近。它包含
//表中所有数据块的过滤器（如Bloom过滤器）组合在一起
//变成一个滤块。

#ifndef STORAGE_LEVELDB_TABLE_FILTER_BLOCK_H_
#define STORAGE_LEVELDB_TABLE_FILTER_BLOCK_H_

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "leveldb/slice.h"
#include "util/hash.h"

namespace leveldb {

class FilterPolicy;

//filterBlockBuilder用于构造
//特殊表格。它生成一个字符串，存储为
//桌子上的一块特别的木块。
//
//对filterblockbuilder的调用序列必须与regexp匹配：
//（startblock addkey*）*完成
class FilterBlockBuilder {
 public:
  explicit FilterBlockBuilder(const FilterPolicy*);

  void StartBlock(uint64_t block_offset);
  void AddKey(const Slice& key);
  Slice Finish();

 private:
  void GenerateFilter();

  const FilterPolicy* policy_;
std::string keys_;              //扁平键内容
std::vector<size_t> start_;     //每个键的起始索引
std::string result_;            //过滤到目前为止计算的数据
std::vector<Slice> tmp_keys_;   //policy_u->createfilter（）参数
  std::vector<uint32_t> filter_offsets_;

//不允许复制
  FilterBlockBuilder(const FilterBlockBuilder&);
  void operator=(const FilterBlockBuilder&);
};

class FilterBlockReader {
 public:
//要求：“内容”和*策略在*这是活动状态时必须保持活动状态。
  FilterBlockReader(const FilterPolicy* policy, const Slice& contents);
  bool KeyMayMatch(uint64_t block_offset, const Slice& key);

 private:
  const FilterPolicy* policy_;
const char* data_;    //指向筛选数据的指针（在块开始处）
const char* offset_;  //指向偏移数组开始的指针（在块结束处）
size_t num_;          //偏移数组中的条目数
size_t base_lg_;      //编码参数（参见.cc文件中的kfilterbaselg）
};

}

#endif  //存储级别表过滤块
