
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

#ifndef STORAGE_LEVELDB_TABLE_MERGER_H_
#define STORAGE_LEVELDB_TABLE_MERGER_H_

namespace leveldb {

class Comparator;
class Iterator;

//返回一个迭代器，该迭代器在
//儿童[0，n-1]。取得子迭代器的所有权，并且
//将在结果迭代器被删除时删除它们。
//
//结果没有重复抑制。也就是说，如果
//密钥存在于k子迭代器中，将生成k次。
//
//要求：n>=0
extern Iterator* NewMergingIterator(
    const Comparator* comparator, Iterator** children, int n);

}  //命名空间级别数据库

#endif  //存储级别表合并
