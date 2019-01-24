
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_UTIL_BYTEVECTORHASH_H
#define BITCOIN_UTIL_BYTEVECTORHASH_H

#include <stdint.h>
#include <vector>

/*
 *对内部存储字节数组的类型实现哈希命名要求。这可能
 *用作std:：unordered_set或std:：unordered_映射中的哈希函数。
 *在内部，它使用siphash-2-4的随机实例。
 **/

class ByteVectorHash final
{
private:
    uint64_t m_k0, m_k1;

public:
    ByteVectorHash();
    size_t operator()(const std::vector<unsigned char>& input) const;
};

#endif //比特币
