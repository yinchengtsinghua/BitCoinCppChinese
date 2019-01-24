
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//版权所有（c）2017 Zcash开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_PUBKEY_H
#define BITCOIN_PUBKEY_H

#include <hash.h>
#include <serialize.h>
#include <uint256.h>

#include <stdexcept>
#include <vector>

const unsigned int BIP32_EXTKEY_SIZE = 74;

/*对ckey的引用：其序列化公钥的hash160*/
class CKeyID : public uint160
{
public:
    CKeyID() : uint160() {}
    explicit CKeyID(const uint160& in) : uint160(in) {}
};

typedef uint256 ChainCode;

/*封装的公钥。*/
class CPubKey
{
public:
    /*
     ＊SECP256K1：
     **/

    static constexpr unsigned int PUBLIC_KEY_SIZE             = 65;
    static constexpr unsigned int COMPRESSED_PUBLIC_KEY_SIZE  = 33;
    static constexpr unsigned int SIGNATURE_SIZE              = 72;
    static constexpr unsigned int COMPACT_SIGNATURE_SIZE      = 65;
    /*
     *请访问www.keylength.com
     *对于单字节推送，脚本最多支持75个
     **/

    static_assert(
        PUBLIC_KEY_SIZE >= COMPRESSED_PUBLIC_KEY_SIZE,
        "COMPRESSED_PUBLIC_KEY_SIZE is larger than PUBLIC_KEY_SIZE");

private:

    /*
     *只需存储序列化数据。
     *它的长度可以很便宜地从第一个字节开始计算。
     **/

    unsigned char vch[PUBLIC_KEY_SIZE];

//！用给定的第一个字节计算pubkey的长度。
    unsigned int static GetLen(unsigned char chHeader)
    {
        if (chHeader == 2 || chHeader == 3)
            return COMPRESSED_PUBLIC_KEY_SIZE;
        if (chHeader == 4 || chHeader == 6 || chHeader == 7)
            return PUBLIC_KEY_SIZE;
        return 0;
    }

//！将此密钥数据设置为无效
    void Invalidate()
    {
        vch[0] = 0xFF;
    }

public:

    bool static ValidSize(const std::vector<unsigned char> &vch) {
      return vch.size() > 0 && GetLen(vch[0]) == vch.size();
    }

//！构造无效的公钥。
    CPubKey()
    {
        Invalidate();
    }

//！使用begin/end迭代器将公钥初始化为字节数据。
    template <typename T>
    void Set(const T pbegin, const T pend)
    {
        int len = pend == pbegin ? 0 : GetLen(pbegin[0]);
        if (len && len == (pend - pbegin))
            memcpy(vch, (unsigned char*)&pbegin[0], len);
        else
            Invalidate();
    }

//！使用begin/end迭代器对字节数据构造公钥。
    template <typename T>
    CPubKey(const T pbegin, const T pend)
    {
        Set(pbegin, pend);
    }

//！从字节向量构造公钥。
    explicit CPubKey(const std::vector<unsigned char>& _vch)
    {
        Set(_vch.begin(), _vch.end());
    }

//！pubkey数据的简单的只读类矢量接口。
    unsigned int size() const { return GetLen(vch[0]); }
    const unsigned char* data() const { return vch; }
    const unsigned char* begin() const { return vch; }
    const unsigned char* end() const { return vch + size(); }
    const unsigned char& operator[](unsigned int pos) const { return vch[pos]; }

//！比较器实现。
    friend bool operator==(const CPubKey& a, const CPubKey& b)
    {
        return a.vch[0] == b.vch[0] &&
               memcmp(a.vch, b.vch, a.size()) == 0;
    }
    friend bool operator!=(const CPubKey& a, const CPubKey& b)
    {
        return !(a == b);
    }
    friend bool operator<(const CPubKey& a, const CPubKey& b)
    {
        return a.vch[0] < b.vch[0] ||
               (a.vch[0] == b.vch[0] && memcmp(a.vch, b.vch, a.size()) < 0);
    }

//！实现序列化，就好像这是一个字节向量。
    template <typename Stream>
    void Serialize(Stream& s) const
    {
        unsigned int len = size();
        ::WriteCompactSize(s, len);
        s.write((char*)vch, len);
    }
    template <typename Stream>
    void Unserialize(Stream& s)
    {
        unsigned int len = ::ReadCompactSize(s);
        if (len <= PUBLIC_KEY_SIZE) {
            s.read((char*)vch, len);
        } else {
//pubkey无效，跳过可用数据
            char dummy;
            while (len--)
                s.read(&dummy, 1);
            Invalidate();
        }
    }

//！获取此公钥的keyid（其序列化的哈希）
    CKeyID GetID() const
    {
        return CKeyID(Hash160(vch, vch + size()));
    }

//！获取此公钥的256位哈希。
    uint256 GetHash() const
    {
        return Hash(vch, vch + size());
    }

    /*
     *检查语法正确性。
     *
     *请注意，当checksig（）调用它时，这是协商一致的关键！
     **/

    bool IsValid() const
    {
        return size() > 0;
    }

//！完全验证这是否是有效的公钥（比is valid（）更昂贵）
    bool IsFullyValid() const;

//！检查这是否是压缩的公钥。
    bool IsCompressed() const
    {
        return size() == COMPRESSED_PUBLIC_KEY_SIZE;
    }

    /*
     *验证一个der签名（~72字节）。
     *如果此公钥不完全有效，返回值将为假。
     **/

    bool Verify(const uint256& hash, const std::vector<unsigned char>& vchSig) const;

    /*
     *检查签名是否规范化（低S）。
     **/

    static bool CheckLowS(const std::vector<unsigned char>& vchSig);

//！从压缩签名恢复公钥。
    bool RecoverCompact(const uint256& hash, const std::vector<unsigned char>& vchSig);

//！将此公钥转换为未压缩的公钥。
    bool Decompress();

//！派生bip32子pubkey。
    bool Derive(CPubKey& pubkeyChild, ChainCode &ccChild, unsigned int nChild, const ChainCode& cc) const;
};

struct CExtPubKey {
    unsigned char nDepth;
    unsigned char vchFingerprint[4];
    unsigned int nChild;
    ChainCode chaincode;
    CPubKey pubkey;

    friend bool operator==(const CExtPubKey &a, const CExtPubKey &b)
    {
        return a.nDepth == b.nDepth &&
            memcmp(&a.vchFingerprint[0], &b.vchFingerprint[0], sizeof(vchFingerprint)) == 0 &&
            a.nChild == b.nChild &&
            a.chaincode == b.chaincode &&
            a.pubkey == b.pubkey;
    }

    void Encode(unsigned char code[BIP32_EXTKEY_SIZE]) const;
    void Decode(const unsigned char code[BIP32_EXTKEY_SIZE]);
    bool Derive(CExtPubKey& out, unsigned int nChild) const;

    void Serialize(CSizeComputer& s) const
    {
//为：：GetSerializeSize优化的实现，避免复制。
s.seek(BIP32_EXTKEY_SIZE + 1); //为大小添加一个字节（compact int）
    }
    template <typename Stream>
    void Serialize(Stream& s) const
    {
        unsigned int len = BIP32_EXTKEY_SIZE;
        ::WriteCompactSize(s, len);
        unsigned char code[BIP32_EXTKEY_SIZE];
        Encode(code);
        s.write((const char *)&code[0], len);
    }
    template <typename Stream>
    void Unserialize(Stream& s)
    {
        unsigned int len = ::ReadCompactSize(s);
        unsigned char code[BIP32_EXTKEY_SIZE];
        if (len != BIP32_EXTKEY_SIZE)
            throw std::runtime_error("Invalid extended key size\n");
        s.read((char *)&code[0], len);
        Decode(code);
    }
};

/*此模块的用户必须持有ECVerifyHandle。建造师和
 *但是，这些函数的析构函数不允许并行运行。*/

class ECCVerifyHandle
{
    static int refcount;

public:
    ECCVerifyHandle();
    ~ECCVerifyHandle();
};

#endif //比特币
