
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

#ifndef BITCOIN_SCRIPT_SCRIPT_H
#define BITCOIN_SCRIPT_SCRIPT_H

#include <crypto/common.h>
#include <prevector.h>
#include <serialize.h>

#include <assert.h>
#include <climits>
#include <limits>
#include <stdexcept>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

//可推送到堆栈的最大字节数
static const unsigned int MAX_SCRIPT_ELEMENT_SIZE = 520;

//每个脚本的最大非强制操作数
static const int MAX_OPS_PER_SCRIPT = 201;

//每个multisig的最大公钥数
static const int MAX_PUBKEYS_PER_MULTISIG = 20;

//最大脚本长度（字节）
static const int MAX_SCRIPT_SIZE = 10000;

//脚本解释器堆栈上的最大值数
static const int MAX_STACK_SIZE = 1000;

//nlocktime阈值：低于此值时，它被解释为块编号，
//否则作为unix时间戳。
static const unsigned int LOCKTIME_THRESHOLD = 500000000; //1985年11月5日星期二00:53:20 UTC

//最大开锁时间。因为锁定时间表示最后一个无效的时间戳，
//具有此锁定时间的事务将永远无效，除非锁定时间
//禁用检查（通过将所有输入序列号设置为
//顺序“最终”。
static const uint32_t LOCKTIME_MAX = 0xFFFFFFFFU;

template <typename T>
std::vector<unsigned char> ToByteVector(const T& in)
{
    return std::vector<unsigned char>(in.begin(), in.end());
}

/*脚本操作码*/
enum opcodetype
{
//推值
    OP_0 = 0x00,
    OP_FALSE = OP_0,
    OP_PUSHDATA1 = 0x4c,
    OP_PUSHDATA2 = 0x4d,
    OP_PUSHDATA4 = 0x4e,
    OP_1NEGATE = 0x4f,
    OP_RESERVED = 0x50,
    OP_1 = 0x51,
    OP_TRUE=OP_1,
    OP_2 = 0x52,
    OP_3 = 0x53,
    OP_4 = 0x54,
    OP_5 = 0x55,
    OP_6 = 0x56,
    OP_7 = 0x57,
    OP_8 = 0x58,
    OP_9 = 0x59,
    OP_10 = 0x5a,
    OP_11 = 0x5b,
    OP_12 = 0x5c,
    OP_13 = 0x5d,
    OP_14 = 0x5e,
    OP_15 = 0x5f,
    OP_16 = 0x60,

//控制
    OP_NOP = 0x61,
    OP_VER = 0x62,
    OP_IF = 0x63,
    OP_NOTIF = 0x64,
    OP_VERIF = 0x65,
    OP_VERNOTIF = 0x66,
    OP_ELSE = 0x67,
    OP_ENDIF = 0x68,
    OP_VERIFY = 0x69,
    OP_RETURN = 0x6a,

//堆栈操作系统
    OP_TOALTSTACK = 0x6b,
    OP_FROMALTSTACK = 0x6c,
    OP_2DROP = 0x6d,
    OP_2DUP = 0x6e,
    OP_3DUP = 0x6f,
    OP_2OVER = 0x70,
    OP_2ROT = 0x71,
    OP_2SWAP = 0x72,
    OP_IFDUP = 0x73,
    OP_DEPTH = 0x74,
    OP_DROP = 0x75,
    OP_DUP = 0x76,
    OP_NIP = 0x77,
    OP_OVER = 0x78,
    OP_PICK = 0x79,
    OP_ROLL = 0x7a,
    OP_ROT = 0x7b,
    OP_SWAP = 0x7c,
    OP_TUCK = 0x7d,

//剪接动作
    OP_CAT = 0x7e,
    OP_SUBSTR = 0x7f,
    OP_LEFT = 0x80,
    OP_RIGHT = 0x81,
    OP_SIZE = 0x82,

//位逻辑
    OP_INVERT = 0x83,
    OP_AND = 0x84,
    OP_OR = 0x85,
    OP_XOR = 0x86,
    OP_EQUAL = 0x87,
    OP_EQUALVERIFY = 0x88,
    OP_RESERVED1 = 0x89,
    OP_RESERVED2 = 0x8a,

//数字的
    OP_1ADD = 0x8b,
    OP_1SUB = 0x8c,
    OP_2MUL = 0x8d,
    OP_2DIV = 0x8e,
    OP_NEGATE = 0x8f,
    OP_ABS = 0x90,
    OP_NOT = 0x91,
    OP_0NOTEQUAL = 0x92,

    OP_ADD = 0x93,
    OP_SUB = 0x94,
    OP_MUL = 0x95,
    OP_DIV = 0x96,
    OP_MOD = 0x97,
    OP_LSHIFT = 0x98,
    OP_RSHIFT = 0x99,

    OP_BOOLAND = 0x9a,
    OP_BOOLOR = 0x9b,
    OP_NUMEQUAL = 0x9c,
    OP_NUMEQUALVERIFY = 0x9d,
    OP_NUMNOTEQUAL = 0x9e,
    OP_LESSTHAN = 0x9f,
    OP_GREATERTHAN = 0xa0,
    OP_LESSTHANOREQUAL = 0xa1,
    OP_GREATERTHANOREQUAL = 0xa2,
    OP_MIN = 0xa3,
    OP_MAX = 0xa4,

    OP_WITHIN = 0xa5,

//密码
    OP_RIPEMD160 = 0xa6,
    OP_SHA1 = 0xa7,
    OP_SHA256 = 0xa8,
    OP_HASH160 = 0xa9,
    OP_HASH256 = 0xaa,
    OP_CODESEPARATOR = 0xab,
    OP_CHECKSIG = 0xac,
    OP_CHECKSIGVERIFY = 0xad,
    OP_CHECKMULTISIG = 0xae,
    OP_CHECKMULTISIGVERIFY = 0xaf,

//膨胀
    OP_NOP1 = 0xb0,
    OP_CHECKLOCKTIMEVERIFY = 0xb1,
    OP_NOP2 = OP_CHECKLOCKTIMEVERIFY,
    OP_CHECKSEQUENCEVERIFY = 0xb2,
    OP_NOP3 = OP_CHECKSEQUENCEVERIFY,
    OP_NOP4 = 0xb3,
    OP_NOP5 = 0xb4,
    OP_NOP6 = 0xb5,
    OP_NOP7 = 0xb6,
    OP_NOP8 = 0xb7,
    OP_NOP9 = 0xb8,
    OP_NOP10 = 0xb9,

    OP_INVALIDOPCODE = 0xff,
};

//操作码的最大值
static const unsigned int MAX_OPCODE = OP_NOP10;

const char* GetOpName(opcodetype opcode);

class scriptnum_error : public std::runtime_error
{
public:
    explicit scriptnum_error(const std::string& str) : std::runtime_error(str) {}
};

class CScriptNum
{
/*
 *数字操作码（op_1添加等）仅限于在4字节整数上操作。
 *语义很微妙，但是：操作数必须在范围内[-2^31+1…2^31-1]，
 *但是结果可能溢出（并且只要它们不在后面使用就有效
 *数值运算）。CScriptNum通过将结果存储为
 *一个Int64，允许以字节向量的形式返回超出范围的值，但是
 *如果算术完成或结果被解释为整数，则引发异常。
 **/

public:

    explicit CScriptNum(const int64_t& n)
    {
        m_value = n;
    }

    static const size_t nDefaultMaxNumSize = 4;

    explicit CScriptNum(const std::vector<unsigned char>& vch, bool fRequireMinimal,
                        const size_t nMaxNumSize = nDefaultMaxNumSize)
    {
        if (vch.size() > nMaxNumSize) {
            throw scriptnum_error("script number overflow");
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
                    throw scriptnum_error("non-minimally encoded script number");
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

    inline bool operator==(const CScriptNum& rhs) const { return operator==(rhs.m_value); }
    inline bool operator!=(const CScriptNum& rhs) const { return operator!=(rhs.m_value); }
    inline bool operator<=(const CScriptNum& rhs) const { return operator<=(rhs.m_value); }
    inline bool operator< (const CScriptNum& rhs) const { return operator< (rhs.m_value); }
    inline bool operator>=(const CScriptNum& rhs) const { return operator>=(rhs.m_value); }
    inline bool operator> (const CScriptNum& rhs) const { return operator> (rhs.m_value); }

    inline CScriptNum operator+(   const int64_t& rhs)    const { return CScriptNum(m_value + rhs);}
    inline CScriptNum operator-(   const int64_t& rhs)    const { return CScriptNum(m_value - rhs);}
    inline CScriptNum operator+(   const CScriptNum& rhs) const { return operator+(rhs.m_value);   }
    inline CScriptNum operator-(   const CScriptNum& rhs) const { return operator-(rhs.m_value);   }

    inline CScriptNum& operator+=( const CScriptNum& rhs)       { return operator+=(rhs.m_value);  }
    inline CScriptNum& operator-=( const CScriptNum& rhs)       { return operator-=(rhs.m_value);  }

    inline CScriptNum operator&(   const int64_t& rhs)    const { return CScriptNum(m_value & rhs);}
    inline CScriptNum operator&(   const CScriptNum& rhs) const { return operator&(rhs.m_value);   }

    inline CScriptNum& operator&=( const CScriptNum& rhs)       { return operator&=(rhs.m_value);  }

    inline CScriptNum operator-()                         const
    {
        assert(m_value != std::numeric_limits<int64_t>::min());
        return CScriptNum(-m_value);
    }

    inline CScriptNum& operator=( const int64_t& rhs)
    {
        m_value = rhs;
        return *this;
    }

    inline CScriptNum& operator+=( const int64_t& rhs)
    {
        assert(rhs == 0 || (rhs > 0 && m_value <= std::numeric_limits<int64_t>::max() - rhs) ||
                           (rhs < 0 && m_value >= std::numeric_limits<int64_t>::min() - rhs));
        m_value += rhs;
        return *this;
    }

    inline CScriptNum& operator-=( const int64_t& rhs)
    {
        assert(rhs == 0 || (rhs > 0 && m_value >= std::numeric_limits<int64_t>::min() + rhs) ||
                           (rhs < 0 && m_value <= std::numeric_limits<int64_t>::max() + rhs));
        m_value -= rhs;
        return *this;
    }

    inline CScriptNum& operator&=( const int64_t& rhs)
    {
        m_value &= rhs;
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

/*
 *我们使用脚本的prevector来减少大量的内存开销。
 *通常包含少量小元素的向量。
 *2015年10月的测试显示，使用这种方法可将dbcache内存使用率降低23%。
 *使初始同步速度加快13%。
 **/

typedef prevector<28, unsigned char> CScriptBase;

bool GetScriptOp(CScriptBase::const_iterator& pc, CScriptBase::const_iterator end, opcodetype& opcodeRet, std::vector<unsigned char>* pvchRet);

/*序列化脚本，在事务输入和输出中使用*/
class CScript : public CScriptBase
{
protected:
    CScript& push_int64(int64_t n)
    {
        if (n == -1 || (n >= 1 && n <= 16))
        {
            push_back(n + (OP_1 - 1));
        }
        else if (n == 0)
        {
            push_back(OP_0);
        }
        else
        {
            *this << CScriptNum::serialize(n);
        }
        return *this;
    }
public:
    CScript() { }
    CScript(const_iterator pbegin, const_iterator pend) : CScriptBase(pbegin, pend) { }
    CScript(std::vector<unsigned char>::const_iterator pbegin, std::vector<unsigned char>::const_iterator pend) : CScriptBase(pbegin, pend) { }
    CScript(const unsigned char* pbegin, const unsigned char* pend) : CScriptBase(pbegin, pend) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(CScriptBase, *this);
    }

    CScript& operator+=(const CScript& b)
    {
        reserve(size() + b.size());
        insert(end(), b.begin(), b.end());
        return *this;
    }

    friend CScript operator+(const CScript& a, const CScript& b)
    {
        CScript ret = a;
        ret += b;
        return ret;
    }

    CScript(int64_t b)        { operator<<(b); }

    explicit CScript(opcodetype b)     { operator<<(b); }
    explicit CScript(const CScriptNum& b) { operator<<(b); }
    explicit CScript(const std::vector<unsigned char>& b) { operator<<(b); }


    CScript& operator<<(int64_t b) { return push_int64(b); }

    CScript& operator<<(opcodetype opcode)
    {
        if (opcode < 0 || opcode > 0xff)
            throw std::runtime_error("CScript::operator<<(): invalid opcode");
        insert(end(), (unsigned char)opcode);
        return *this;
    }

    CScript& operator<<(const CScriptNum& b)
    {
        *this << b.getvch();
        return *this;
    }

    CScript& operator<<(const std::vector<unsigned char>& b)
    {
        if (b.size() < OP_PUSHDATA1)
        {
            insert(end(), (unsigned char)b.size());
        }
        else if (b.size() <= 0xff)
        {
            insert(end(), OP_PUSHDATA1);
            insert(end(), (unsigned char)b.size());
        }
        else if (b.size() <= 0xffff)
        {
            insert(end(), OP_PUSHDATA2);
            uint8_t _data[2];
            WriteLE16(_data, b.size());
            insert(end(), _data, _data + sizeof(_data));
        }
        else
        {
            insert(end(), OP_PUSHDATA4);
            uint8_t _data[4];
            WriteLE32(_data, b.size());
            insert(end(), _data, _data + sizeof(_data));
        }
        insert(end(), b.begin(), b.end());
        return *this;
    }

    CScript& operator<<(const CScript& b)
    {
//我不确定这是否应该推送脚本或连接脚本。
//如果有将脚本推送到脚本上的用途，请删除此成员fn
        assert(!"Warning: Pushing a CScript onto a CScript with << is probably not intended, use + to concatenate!");
        return *this;
    }


    bool GetOp(const_iterator& pc, opcodetype& opcodeRet, std::vector<unsigned char>& vchRet) const
    {
        return GetScriptOp(pc, end(), opcodeRet, &vchRet);
    }

    bool GetOp(const_iterator& pc, opcodetype& opcodeRet) const
    {
        return GetScriptOp(pc, end(), opcodeRet, nullptr);
    }


    /*编码/解码小整数：*/
    static int DecodeOP_N(opcodetype opcode)
    {
        if (opcode == OP_0)
            return 0;
        assert(opcode >= OP_1 && opcode <= OP_16);
        return (int)opcode - (int)(OP_1 - 1);
    }
    static opcodetype EncodeOP_N(int n)
    {
        assert(n >= 0 && n <= 16);
        if (n == 0)
            return OP_0;
        return (opcodetype)(OP_1+n-1);
    }

    /*
     *0.6之前版本，比特币始终计数支票多张
     *作为20个信号。使用“按脚本付费”哈希，更改如下：
     *在脚本中序列化的checkmultisigs是
     *更准确地计算，假设它们的形式
     *…检查多图像…
     **/

    unsigned int GetSigOpCount(bool fAccurate) const;

    /*
     *准确计算sigops，包括sigops in
     *按脚本付费哈希事务：
     **/

    unsigned int GetSigOpCount(const CScript& scriptSig) const;

    bool IsPayToScriptHash() const;
    bool IsPayToWitnessScriptHash() const;
    bool IsWitnessProgram(int& version, std::vector<unsigned char>& program) const;

    /*由isstandardtx和p2sh/bip62 verifyscript调用（这使得共识非常关键）。*/
    bool IsPushOnly(const_iterator pc) const;
    bool IsPushOnly() const;

    /*检查脚本是否包含有效的操作代码*/
    bool HasValidOps() const;

    /*
     *返回是否保证脚本在执行时失败，
     *不考虑初始堆栈。这允许修剪输出
     *当输入UTXO集合时立即。
     **/

    bool IsUnspendable() const
    {
        return (size() > 0 && *begin() == OP_RETURN) || (size() > MAX_SCRIPT_SIZE);
    }

    void clear()
    {
//默认的prevector:：clear（）不释放内存
        CScriptBase::clear();
        shrink_to_fit();
    }
};

struct CScriptWitness
{
//注意，这对被推送的数据元素进行编码，而不是
//将它们编码为推动它们的CScript。
    std::vector<std::vector<unsigned char> > stack;

//一些编译器抱怨没有默认的构造函数
    CScriptWitness() { }

    bool IsNull() const { return stack.empty(); }

    void SetNull() { stack.clear(); stack.shrink_to_fit(); }

    std::string ToString() const;
};

class CReserveScript
{
public:
    CScript reserveScript;
    virtual void KeepScript() {}
    CReserveScript() {}
    virtual ~CReserveScript() {}
};

#endif //比特币脚本
