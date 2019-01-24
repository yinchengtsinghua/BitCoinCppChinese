
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

#ifndef BITCOIN_PRIMITIVES_TRANSACTION_H
#define BITCOIN_PRIMITIVES_TRANSACTION_H

#include <stdint.h>
#include <amount.h>
#include <script/script.h>
#include <serialize.h>
#include <uint256.h>

static const int SERIALIZE_TRANSACTION_NO_WITNESS = 0x40000000;

/*输出点-将事务哈希和索引n组合到其VOUT中*/
class COutPoint
{
public:
    uint256 hash;
    uint32_t n;

    static constexpr uint32_t NULL_INDEX = std::numeric_limits<uint32_t>::max();

    COutPoint(): n(NULL_INDEX) { }
    COutPoint(const uint256& hashIn, uint32_t nIn): hash(hashIn), n(nIn) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(hash);
        READWRITE(n);
    }

    void SetNull() { hash.SetNull(); n = NULL_INDEX; }
    bool IsNull() const { return (hash.IsNull() && n == NULL_INDEX); }

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        int cmp = a.hash.Compare(b.hash);
        return cmp < 0 || (cmp == 0 && a.n < b.n);
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const COutPoint& a, const COutPoint& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

/*事务的输入。它包含上一个的位置
 *它声明的事务输出和与
 *输出的公钥。
 **/

class CTxIn
{
public:
    COutPoint prevout;
    CScript scriptSig;
    uint32_t nSequence;
CScriptWitness scriptWitness; //！<仅通过ctransaction序列化

    /*为事务中的每个输入设置此值的Nsequence
     *禁用nlocktime。*/

    static const uint32_t SEQUENCE_FINAL = 0xffffffff;

    /*以下标志适用于BIP 6的上下文*/
    /*如果设置了此标志，则ctxin:：nsequence不会解释为
     *相对锁定时间。*/

    static const uint32_t SEQUENCE_LOCKTIME_DISABLE_FLAG = (1U << 31);

    /*如果ctxin:：nsequence编码相对锁定时间和此标志
     *已设置，相对锁定时间的单位为512秒，
     *否则，它指定粒度为1的块。*/

    static const uint32_t SEQUENCE_LOCKTIME_TYPE_FLAG = (1 << 22);

    /*如果ctxin:：nsequence编码相对锁定时间，则此掩码为
     *用于从序列字段中提取该锁定时间。*/

    static const uint32_t SEQUENCE_LOCKTIME_MASK = 0x0000ffff;

    /*为了使用相同的位数来大致编码
     *相同的墙时钟持续时间，因为块是自然的
     *平均每600秒发生一次，最小粒度
     *基于时间的相对锁定时间固定为512秒。
     *从ctxin:：n序列转换为秒由执行
     *乘以512=2^9，或等于向上移动
     * 9位。*/

    static const int SEQUENCE_LOCKTIME_GRANULARITY = 9;

    CTxIn()
    {
        nSequence = SEQUENCE_FINAL;
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=SEQUENCE_FINAL);
    CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=SEQUENCE_FINAL);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(prevout);
        READWRITE(scriptSig);
        READWRITE(nSequence);
    }

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout   == b.prevout &&
                a.scriptSig == b.scriptSig &&
                a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

/*事务的输出。它包含下一个输入的公钥
 *必须能够与签名以声明它。
 **/

class CTxOut
{
public:
    CAmount nValue;
    CScript scriptPubKey;

    CTxOut()
    {
        SetNull();
    }

    CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nValue);
        READWRITE(scriptPubKey);
    }

    void SetNull()
    {
        nValue = -1;
        scriptPubKey.clear();
    }

    bool IsNull() const
    {
        return (nValue == -1);
    }

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue       == b.nValue &&
                a.scriptPubKey == b.scriptPubKey);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

struct CMutableTransaction;

/*
 *基本事务序列化格式：
 *-Int32转换
 *-std:：vector<ctxin>vin
 *-std:：vector<ctxout>vout
 *-uint32下载时间
 *
 *扩展事务序列化格式：
 *-Int32转换
 *-无符号字符虚拟值=0x00
 *-无符号字符标志（！= 0）
 *-std:：vector<ctxin>vin
 *-std:：vector<ctxout>vout
 *-如果（标志1）：
 *CTX见证人
 *-uint32下载时间
 **/

template<typename Stream, typename TxType>
inline void UnserializeTransaction(TxType& tx, Stream& s) {
    const bool fAllowWitness = !(s.GetVersion() & SERIALIZE_TRANSACTION_NO_WITNESS);

    s >> tx.nVersion;
    unsigned char flags = 0;
    tx.vin.clear();
    tx.vout.clear();
    /*尝试读取VIN。如果存在虚拟对象，则将其读取为空向量。*/
    s >> tx.vin;
    if (tx.vin.size() == 0 && fAllowWitness) {
        /*我们读取一个虚拟的或空的VIN。*/
        s >> flags;
        if (flags != 0) {
            s >> tx.vin;
            s >> tx.vout;
        }
    } else {
        /*我们读取了一个非空的VIN。假设一个正常的凭证跟随。*/
        s >> tx.vout;
    }
    if ((flags & 1) && fAllowWitness) {
        /*证人旗在，我们支持证人。*/
        flags ^= 1;
        for (size_t i = 0; i < tx.vin.size(); i++) {
            s >> tx.vin[i].scriptWitness.stack;
        }
    }
    if (flags) {
        /*序列化中的未知标志*/
        throw std::ios_base::failure("Unknown transaction optional data");
    }
    s >> tx.nLockTime;
}

template<typename Stream, typename TxType>
inline void SerializeTransaction(const TxType& tx, Stream& s) {
    const bool fAllowWitness = !(s.GetVersion() & SERIALIZE_TRANSACTION_NO_WITNESS);

    s << tx.nVersion;
    unsigned char flags = 0;
//一致性检查
    if (fAllowWitness) {
        /*检查证人是否需要连载。*/
        if (tx.HasWitness()) {
            flags |= 1;
        }
    }
    if (flags) {
        /*使用扩展格式以防证人被序列化。*/
        std::vector<CTxIn> vinDummy;
        s << vinDummy;
        s << flags;
    }
    s << tx.vin;
    s << tx.vout;
    if (flags & 1) {
        for (size_t i = 0; i < tx.vin.size(); i++) {
            s << tx.vin[i].scriptWitness.stack;
        }
    }
    s << tx.nLockTime;
}


/*在网络上广播并包含在
 *块。一个事务可以包含多个输入和输出。
 **/

class CTransaction
{
public:
//默认事务版本。
    static const int32_t CURRENT_VERSION=2;

//更改默认事务版本需要两个步骤：首先
//通过碰撞max_标准_版本来调整中继策略，然后更新日期
//在当前版本和
//最大标准版本将相等。
    static const int32_t MAX_STANDARD_VERSION=2;

//将局部变量设置为常量，以防止意外修改。
//不更新缓存的哈希值。但是，ctransaction不是
//实际上是不可变的；实现了反序列化和赋值，
//绕过警察。这是安全的，因为它们更新了整个
//结构，包括哈希。
    const std::vector<CTxIn> vin;
    const std::vector<CTxOut> vout;
    const int32_t nVersion;
    const uint32_t nLockTime;

private:
    /*只有记忆。*/
    const uint256 hash;
    const uint256 m_witness_hash;

    uint256 ComputeHash() const;
    uint256 ComputeWitnessHash() const;

public:
    /*构造一个符合isNull（）的ctransaction*/
    CTransaction();

    /*将cmutableTransaction转换为ctransaction。*/
    CTransaction(const CMutableTransaction &tx);
    CTransaction(CMutableTransaction &&tx);

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeTransaction(*this, s);
    }

    /*提供了此反序列化构造函数，而不是非序列化方法。
     *不可能进行非序列化，因为它需要覆盖常量字段。*/

    template <typename Stream>
    CTransaction(deserialize_type, Stream& s) : CTransaction(CMutableTransaction(deserialize, s)) {}

    bool IsNull() const {
        return vin.empty() && vout.empty();
    }

    const uint256& GetHash() const { return hash; }
    const uint256& GetWitnessHash() const { return m_witness_hash; };

//返回txout的和。
    CAmount GetValueOut() const;
//getValueIn（）是ccoinsViewCache上的一个方法，因为
//必须知道输入才能计算中的值。

    /*
     *获取总事务大小（字节），包括见证数据。
     *bip141和bip144中定义的“总尺寸”。
     *@返回以字节为单位的事务总大小
     **/

    unsigned int GetTotalSize() const;

    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].prevout.IsNull());
    }

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return a.hash == b.hash;
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return a.hash != b.hash;
    }

    std::string ToString() const;

    bool HasWitness() const
    {
        for (size_t i = 0; i < vin.size(); i++) {
            if (!vin[i].scriptWitness.IsNull()) {
                return true;
            }
        }
        return false;
    }
};

/*ctransaction的可变版本。*/
struct CMutableTransaction
{
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    int32_t nVersion;
    uint32_t nLockTime;

    CMutableTransaction();
    explicit CMutableTransaction(const CTransaction& tx);

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeTransaction(*this, s);
    }


    template <typename Stream>
    inline void Unserialize(Stream& s) {
        UnserializeTransaction(*this, s);
    }

    template <typename Stream>
    CMutableTransaction(deserialize_type, Stream& s) {
        Unserialize(s);
    }

    /*计算此CMutableTransaction的哈希。这是根据
     *fly，与ctransaction中使用缓存结果的gethash（）不同。
     **/

    uint256 GetHash() const;

    bool HasWitness() const
    {
        for (size_t i = 0; i < vin.size(); i++) {
            if (!vin[i].scriptWitness.IsNull()) {
                return true;
            }
        }
        return false;
    }
};

typedef std::shared_ptr<const CTransaction> CTransactionRef;
static inline CTransactionRef MakeTransactionRef() { return std::make_shared<const CTransaction>(); }
template <typename Tx> static inline CTransactionRef MakeTransactionRef(Tx&& txIn) { return std::make_shared<const CTransaction>(std::forward<Tx>(txIn)); }

#endif //比特币原语交易
