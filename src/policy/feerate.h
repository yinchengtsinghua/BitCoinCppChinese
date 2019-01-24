
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_POLICY_FEERATE_H
#define BITCOIN_POLICY_FEERATE_H

#include <amount.h>
#include <serialize.h>

#include <string>

extern const std::string CURRENCY_UNIT;

/*
 *每千字节Satoshis的费率：camount/kb
 **/

class CFeeRate
{
private:
CAmount nSatoshisPerK; //单位为每1000字节Satoshis

public:
    /*每kb 0 Satoshis的费率*/
    CFeeRate() : nSatoshisPerK(0) { }
    template<typename I>
    CFeeRate(const I _nSatoshisPerK): nSatoshisPerK(_nSatoshisPerK) {
//我们以前有过从无声的双重转换到int转换的错误…
        static_assert(std::is_integral<I>::value, "CFeeRate should be used without floats");
    }
    /*以每千字节Satoshis为单位计算费率的构造函数。字节大小不得超过（2^63-1*/
    CFeeRate(const CAmount& nFeePaid, size_t nBytes);
    /*
     *返回给定大小的费用（以字节为单位）。
     **/

    CAmount GetFee(size_t nBytes) const;
    /*
     *返回1000字节大小的Satoshis费用
     **/

    CAmount GetFeePerK() const { return GetFee(1000); }
    friend bool operator<(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK < b.nSatoshisPerK; }
    friend bool operator>(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK > b.nSatoshisPerK; }
    friend bool operator==(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK == b.nSatoshisPerK; }
    friend bool operator<=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK <= b.nSatoshisPerK; }
    friend bool operator>=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK >= b.nSatoshisPerK; }
    friend bool operator!=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK != b.nSatoshisPerK; }
    CFeeRate& operator+=(const CFeeRate& a) { nSatoshisPerK += a.nSatoshisPerK; return *this; }
    std::string ToString() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nSatoshisPerK);
    }
};

#endif //比特币政策
