
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

#include <crypto/siphash.h>
#include <random.h>
#include <util/bytevectorhash.h>

ByteVectorHash::ByteVectorHash()
{
    GetRandBytes(reinterpret_cast<unsigned char*>(&m_k0), sizeof(m_k0));
    GetRandBytes(reinterpret_cast<unsigned char*>(&m_k1), sizeof(m_k1));
}

size_t ByteVectorHash::operator()(const std::vector<unsigned char>& input) const
{
    return CSipHasher(m_k0, m_k1).Write(input.data(), input.size()).Finalize();
}
