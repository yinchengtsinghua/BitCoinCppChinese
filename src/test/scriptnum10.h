
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

#ifndef BITCOIN_TEST_SCRIPTNUM10_H
#define BITCOIN_TEST_SCRIPTNUM10_H

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>
#include <assert.h>

class scriptnum10_error : public std::runtime_error
{
public:
    explicit scriptnum10_error(const std::string& str) : std::runtime_error(str) {}
};

class CScriptNum10
{
/*
 *来自比特币核心0.10.0的scriptnum实现，用于交叉比较。
 **/

public:

    explicit CScriptNum10(const int64_t& n)
    {
        m_value = n;
    }

    static const size_t nDefaultMaxNumSize = 4;

    explicit CScriptNum10(const std::vector<unsigned char>& vch, bool fRequireMinimal,
                        const size_t nMaxNumSize = nDefaultMaxNumSize)
    {
        if (vch.size() > nMaxNumSize) {
            throw scriptnum10_error("script number overflow");
        }
        if (fRequireMinimal && vch.size() > 0) {
//检查数字是否以最小可能值编码
//字节数。
//
//如果最高有效字节（不包括符号位）为零
//那我们就不是最小的了。注意此测试如何也拒绝
//负零编码，0x80。
            if ((vch.back() & 0x7f) == 0) {
//一个例外：如果有多个字节
//设置第二个最高有效字节的有效位
//它将与符号位冲突。这个案例的一个例子
//为+-255，分别编码为0xff00和0xff80。
//（大字节）。
                if (vch.size() <= 1 || (vch[vch.size() - 2] & 0x80) == 0) {
                    throw scriptnum10_error("non-minimally encoded script number");
                }
            }
        }
        m_value = set_vch(vch);
    }

    inline bool operator==(const int64_t& rhs) const    { return m_value == rhs; }
    inline bool operator!=(const int64_t& rhs) const    { return m_value != rhs; }
    inline bool operator<=(const int64_t& rhs) const    { return m_value <= rhs; }
    inline bool operator< (const int64_t& rhs) const    { return m_value <  rhs; }
    inline bool operator>=(const int64_t& rhs) const    { return m_value >= rhs; }
    inline bool operator> (const int64_t& rhs) const    { return m_value >  rhs; }

    inline bool operator==(const CScriptNum10& rhs) const { return operator==(rhs.m_value); }
    inline bool operator!=(const CScriptNum10& rhs) const { return operator!=(rhs.m_value); }
    inline bool operator<=(const CScriptNum10& rhs) const { return operator<=(rhs.m_value); }
    inline bool operator< (const CScriptNum10& rhs) const { return operator< (rhs.m_value); }
    inline bool operator>=(const CScriptNum10& rhs) const { return operator>=(rhs.m_value); }
    inline bool operator> (const CScriptNum10& rhs) const { return operator> (rhs.m_value); }

    inline CScriptNum10 operator+(   const int64_t& rhs)    const { return CScriptNum10(m_value + rhs);}
    inline CScriptNum10 operator-(   const int64_t& rhs)    const { return CScriptNum10(m_value - rhs);}
    inline CScriptNum10 operator+(   const CScriptNum10& rhs) const { return operator+(rhs.m_value);   }
    inline CScriptNum10 operator-(   const CScriptNum10& rhs) const { return operator-(rhs.m_value);   }

    inline CScriptNum10& operator+=( const CScriptNum10& rhs)       { return operator+=(rhs.m_value);  }
    inline CScriptNum10& operator-=( const CScriptNum10& rhs)       { return operator-=(rhs.m_value);  }

    inline CScriptNum10 operator-()                         const
    {
        assert(m_value != std::numeric_limits<int64_t>::min());
        return CScriptNum10(-m_value);
    }

    inline CScriptNum10& operator=( const int64_t& rhs)
    {
        m_value = rhs;
        return *this;
    }

    inline CScriptNum10& operator+=( const int64_t& rhs)
    {
        assert(rhs == 0 || (rhs > 0 && m_value <= std::numeric_limits<int64_t>::max() - rhs) ||
                           (rhs < 0 && m_value >= std::numeric_limits<int64_t>::min() - rhs));
        m_value += rhs;
        return *this;
    }

    inline CScriptNum10& operator-=( const int64_t& rhs)
    {
        assert(rhs == 0 || (rhs > 0 && m_value >= std::numeric_limits<int64_t>::min() + rhs) ||
                           (rhs < 0 && m_value <= std::numeric_limits<int64_t>::max() + rhs));
        m_value -= rhs;
        return *this;
    }

    int getint() const
    {
        if (m_value > std::numeric_limits<int>::max())
            return std::numeric_limits<int>::max();
        else if (m_value < std::numeric_limits<int>::min())
            return std::numeric_limits<int>::min();
        return m_value;
    }

    std::vector<unsigned char> getvch() const
    {
        return serialize(m_value);
    }

    static std::vector<unsigned char> serialize(const int64_t& value)
    {
        if(value == 0)
            return std::vector<unsigned char>();

        std::vector<unsigned char> result;
        const bool neg = value < 0;
        uint64_t absvalue = neg ? -value : value;

        while(absvalue)
        {
            result.push_back(absvalue & 0xff);
            absvalue >>= 8;
        }

//-如果最高有效字节大于等于0x80且值为正数，则按
//新的零字节，使有效字节再次小于0x80。

//-如果最高有效字节大于等于0x80且值为负数，则按
//新的0x80字节将在转换为整数时弹出。

//-如果最高有效字节<0x80且值为负，则添加
//0x80，因为当
//转换为积分。

        if (result.back() & 0x80)
            result.push_back(neg ? 0x80 : 0);
        else if (neg)
            result.back() |= 0x80;

        return result;
    }

private:
    static int64_t set_vch(const std::vector<unsigned char>& vch)
    {
      if (vch.empty())
          return 0;

      int64_t result = 0;
      for (size_t i = 0; i != vch.size(); ++i)
          result |= static_cast<int64_t>(vch[i]) << 8*i;

//如果输入向量的最高有效字节是0x80，则将其从
//结果为MSB并返回负值。
      if (vch.back() & 0x80)
          return -((int64_t)(result & ~(0x80ULL << (8 * (vch.size() - 1)))));

      return result;
    }

    int64_t m_value;
};


#endif //比特币测试
