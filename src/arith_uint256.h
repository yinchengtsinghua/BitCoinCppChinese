
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

#ifndef BITCOIN_ARITH_UINT256_H
#define BITCOIN_ARITH_UINT256_H

#include <assert.h>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

class uint256;

class uint_error : public std::runtime_error {
public:
    explicit uint_error(const std::string& str) : std::runtime_error(str) {}
};

/*无符号大整数的模板基类。*/
template<unsigned int BITS>
class base_uint
{
protected:
    static constexpr int WIDTH = BITS / 32;
    uint32_t pn[WIDTH];
public:

    base_uint()
    {
        static_assert(BITS/32 > 0 && BITS%32 == 0, "Template parameter BITS must be a positive multiple of 32.");

        for (int i = 0; i < WIDTH; i++)
            pn[i] = 0;
    }

    base_uint(const base_uint& b)
    {
        static_assert(BITS/32 > 0 && BITS%32 == 0, "Template parameter BITS must be a positive multiple of 32.");

        for (int i = 0; i < WIDTH; i++)
            pn[i] = b.pn[i];
    }

    base_uint& operator=(const base_uint& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] = b.pn[i];
        return *this;
    }

    base_uint(uint64_t b)
    {
        static_assert(BITS/32 > 0 && BITS%32 == 0, "Template parameter BITS must be a positive multiple of 32.");

        pn[0] = (unsigned int)b;
        pn[1] = (unsigned int)(b >> 32);
        for (int i = 2; i < WIDTH; i++)
            pn[i] = 0;
    }

    explicit base_uint(const std::string& str);

    const base_uint operator~() const
    {
        base_uint ret;
        for (int i = 0; i < WIDTH; i++)
            ret.pn[i] = ~pn[i];
        return ret;
    }

    const base_uint operator-() const
    {
        base_uint ret;
        for (int i = 0; i < WIDTH; i++)
            ret.pn[i] = ~pn[i];
        ++ret;
        return ret;
    }

    double getdouble() const;

    base_uint& operator=(uint64_t b)
    {
        pn[0] = (unsigned int)b;
        pn[1] = (unsigned int)(b >> 32);
        for (int i = 2; i < WIDTH; i++)
            pn[i] = 0;
        return *this;
    }

    base_uint& operator^=(const base_uint& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] ^= b.pn[i];
        return *this;
    }

    base_uint& operator&=(const base_uint& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] &= b.pn[i];
        return *this;
    }

    base_uint& operator|=(const base_uint& b)
    {
        for (int i = 0; i < WIDTH; i++)
            pn[i] |= b.pn[i];
        return *this;
    }

    base_uint& operator^=(uint64_t b)
    {
        pn[0] ^= (unsigned int)b;
        pn[1] ^= (unsigned int)(b >> 32);
        return *this;
    }

    base_uint& operator|=(uint64_t b)
    {
        pn[0] |= (unsigned int)b;
        pn[1] |= (unsigned int)(b >> 32);
        return *this;
    }

    base_uint& operator<<=(unsigned int shift);
    base_uint& operator>>=(unsigned int shift);

    base_uint& operator+=(const base_uint& b)
    {
        uint64_t carry = 0;
        for (int i = 0; i < WIDTH; i++)
        {
            uint64_t n = carry + pn[i] + b.pn[i];
            pn[i] = n & 0xffffffff;
            carry = n >> 32;
        }
        return *this;
    }

    base_uint& operator-=(const base_uint& b)
    {
        *this += -b;
        return *this;
    }

    base_uint& operator+=(uint64_t b64)
    {
        base_uint b;
        b = b64;
        *this += b;
        return *this;
    }

    base_uint& operator-=(uint64_t b64)
    {
        base_uint b;
        b = b64;
        *this += -b;
        return *this;
    }

    base_uint& operator*=(uint32_t b32);
    base_uint& operator*=(const base_uint& b);
    base_uint& operator/=(const base_uint& b);

    base_uint& operator++()
    {
//前缀算子
        int i = 0;
        while (i < WIDTH && ++pn[i] == 0)
            i++;
        return *this;
    }

    const base_uint operator++(int)
    {
//后缀运算符
        const base_uint ret = *this;
        ++(*this);
        return ret;
    }

    base_uint& operator--()
    {
//前缀算子
        int i = 0;
        while (i < WIDTH && --pn[i] == std::numeric_limits<uint32_t>::max())
            i++;
        return *this;
    }

    const base_uint operator--(int)
    {
//后缀运算符
        const base_uint ret = *this;
        --(*this);
        return ret;
    }

    int CompareTo(const base_uint& b) const;
    bool EqualTo(uint64_t b) const;

    friend inline const base_uint operator+(const base_uint& a, const base_uint& b) { return base_uint(a) += b; }
    friend inline const base_uint operator-(const base_uint& a, const base_uint& b) { return base_uint(a) -= b; }
    friend inline const base_uint operator*(const base_uint& a, const base_uint& b) { return base_uint(a) *= b; }
    friend inline const base_uint operator/(const base_uint& a, const base_uint& b) { return base_uint(a) /= b; }
    friend inline const base_uint operator|(const base_uint& a, const base_uint& b) { return base_uint(a) |= b; }
    friend inline const base_uint operator&(const base_uint& a, const base_uint& b) { return base_uint(a) &= b; }
    friend inline const base_uint operator^(const base_uint& a, const base_uint& b) { return base_uint(a) ^= b; }
    friend inline const base_uint operator>>(const base_uint& a, int shift) { return base_uint(a) >>= shift; }
    friend inline const base_uint operator<<(const base_uint& a, int shift) { return base_uint(a) <<= shift; }
    friend inline const base_uint operator*(const base_uint& a, uint32_t b) { return base_uint(a) *= b; }
    friend inline bool operator==(const base_uint& a, const base_uint& b) { return memcmp(a.pn, b.pn, sizeof(a.pn)) == 0; }
    friend inline bool operator!=(const base_uint& a, const base_uint& b) { return memcmp(a.pn, b.pn, sizeof(a.pn)) != 0; }
    friend inline bool operator>(const base_uint& a, const base_uint& b) { return a.CompareTo(b) > 0; }
    friend inline bool operator<(const base_uint& a, const base_uint& b) { return a.CompareTo(b) < 0; }
    friend inline bool operator>=(const base_uint& a, const base_uint& b) { return a.CompareTo(b) >= 0; }
    friend inline bool operator<=(const base_uint& a, const base_uint& b) { return a.CompareTo(b) <= 0; }
    friend inline bool operator==(const base_uint& a, uint64_t b) { return a.EqualTo(b); }
    friend inline bool operator!=(const base_uint& a, uint64_t b) { return !a.EqualTo(b); }

    std::string GetHex() const;
    void SetHex(const char* psz);
    void SetHex(const std::string& str);
    std::string ToString() const;

    unsigned int size() const
    {
        return sizeof(pn);
    }

    /*
     *返回最高位集加1的位置，如果
     *值为零。
     **/

    unsigned int bits() const;

    uint64_t GetLow64() const
    {
        static_assert(WIDTH >= 2, "Assertion WIDTH >= 2 failed (WIDTH = BITS / 32). BITS is a template parameter.");
        return pn[0] | (uint64_t)pn[1] << 32;
    }
};

/*256位无符号大整数。*/
class arith_uint256 : public base_uint<256> {
public:
    arith_uint256() {}
    arith_uint256(const base_uint<256>& b) : base_uint<256>(b) {}
    arith_uint256(uint64_t b) : base_uint<256>(b) {}
    explicit arith_uint256(const std::string& str) : base_uint<256>(str) {}

    /*
     *压缩格式是一个整体的表示。
     *数字n使用与a类似的无符号32位数字
     *浮点格式。
     *最重要的8位是基256的无符号指数。
     *这个指数可以被认为是“n的字节数”。
     *下面的23位是尾数。
     *位号24（0x800000）表示n的符号。
     *n=（-1^符号）*尾数*256^（指数-3）
     *
     *Satoshi最初的实现使用了bn_bn2mpi（）和bn_mpi2bn（）。
     *MPI使用第一个字节的最高有效位作为符号。
     *因此，0x123456000紧凑（0x05123456）
     *且0xc0de000000紧凑（0x0600c0de）
     *
     *比特币仅使用这种“紧凑”格式来解决编码困难。
     *目标，无符号256bit数量。因此，所有
     *符号位和使用基256的复杂性可能是
     *实施事故。
     **/

    arith_uint256& SetCompact(uint32_t nCompact, bool *pfNegative = nullptr, bool *pfOverflow = nullptr);
    uint32_t GetCompact(bool fNegative = false) const;

    friend uint256 ArithToUint256(const arith_uint256 &);
    friend arith_uint256 UintToArith256(const uint256 &);
};

uint256 ArithToUint256(const arith_uint256 &);
arith_uint256 UintToArith256(const uint256 &);

#endif //比特币
