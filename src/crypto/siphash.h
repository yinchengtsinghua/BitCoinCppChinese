
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2016-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_CRYPTO_SIPHASH_H
#define BITCOIN_CRYPTO_SIPHASH_H

#include <stdint.h>

#include <uint256.h>

/*SiPHASH-2-4*/
class CSipHasher
{
private:
    uint64_t v[4];
    uint64_t tmp;
    int count;

public:
    /*构造用128位密钥（k0、k1）初始化的siphash计算器*/
    CSipHasher(uint64_t k0, uint64_t k1);
    /*散列值为64位整数的数据
     *它被视为8字节的小尾数解释。
     *此函数只能在到目前为止写入8个字节的倍数时使用。
     **/

    CSipHasher& Write(uint64_t data);
    /*哈希任意字节。*/
    CSipHasher& Write(const unsigned char* data, size_t size);
    /*计算到目前为止写入的64位siphash-2-4。对象保持不变。*/
    uint64_t Finalize() const;
};

/*为uint256优化了siphash-2-4实现。
 *
 *与以下内容相同：
 *siphasher（k0，k1）
 *.write（val.getuint64（0））。
 *.write（val.getuint64（1））。
 *.write（val.getuint64（2））。
 *.write（val.getuint64（3））。
 最后确定（）
 **/

uint64_t SipHashUint256(uint64_t k0, uint64_t k1, const uint256& val);
uint64_t SipHashUint256Extra(uint64_t k0, uint64_t k1, const uint256& val, uint32_t extra);

#endif //比特币加密
