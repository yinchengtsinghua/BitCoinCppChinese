
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

#include <script/descriptor.h>

#include <key_io.h>
#include <pubkey.h>
#include <script/script.h>
#include <script/standard.h>

#include <span.h>
#include <util/system.h>
#include <util/strencodings.h>

#include <memory>
#include <string>
#include <vector>

namespace {

//////////////////////////////////////////////////
//内部表示//
//////////////////////////////////////////////////

typedef std::vector<uint32_t> KeyPath;

std::string FormatKeyPath(const KeyPath& path)
{
    std::string ret;
    for (auto i : path) {
        ret += strprintf("/%i", (i << 1) >> 1);
        if (i >> 31) ret += '\'';
    }
    return ret;
}

/*用于描述符中公钥对象的接口。*/
struct PubkeyProvider
{
    virtual ~PubkeyProvider() = default;

    /*派生公钥。如果key==nullptr，则只需要info。*/
    virtual bool GetPubKey(int pos, const SigningProvider& arg, CPubKey* key, KeyOriginInfo& info) const = 0;

    /*是否表示不同位置的多个公钥。*/
    virtual bool IsRange() const = 0;

    /*获取生成的公钥的大小（以字节（33或65）为单位）。*/
    virtual size_t GetSize() const = 0;

    /*获取描述符字符串形式。*/
    virtual std::string ToString() const = 0;

    /*获取包含私有数据的描述符字符串形式（如果在arg中可用）。*/
    virtual bool ToPrivateString(const SigningProvider& arg, std::string& out) const = 0;
};

class OriginPubkeyProvider final : public PubkeyProvider
{
    KeyOriginInfo m_origin;
    std::unique_ptr<PubkeyProvider> m_provider;

    std::string OriginString() const
    {
        return HexStr(std::begin(m_origin.fingerprint), std::end(m_origin.fingerprint)) + FormatKeyPath(m_origin.path);
    }

public:
    OriginPubkeyProvider(KeyOriginInfo info, std::unique_ptr<PubkeyProvider> provider) : m_origin(std::move(info)), m_provider(std::move(provider)) {}
    bool GetPubKey(int pos, const SigningProvider& arg, CPubKey* key, KeyOriginInfo& info) const override
    {
        if (!m_provider->GetPubKey(pos, arg, key, info)) return false;
        std::copy(std::begin(m_origin.fingerprint), std::end(m_origin.fingerprint), info.fingerprint);
        info.path.insert(info.path.begin(), m_origin.path.begin(), m_origin.path.end());
        return true;
    }
    bool IsRange() const override { return m_provider->IsRange(); }
    size_t GetSize() const override { return m_provider->GetSize(); }
    std::string ToString() const override { return "[" + OriginString() + "]" + m_provider->ToString(); }
    bool ToPrivateString(const SigningProvider& arg, std::string& ret) const override
    {
        std::string sub;
        if (!m_provider->ToPrivateString(arg, sub)) return false;
        ret = "[" + OriginString() + "]" + std::move(sub);
        return true;
    }
};

/*表示描述符中已解析的常量公钥的对象。*/
class ConstPubkeyProvider final : public PubkeyProvider
{
    CPubKey m_pubkey;

public:
    ConstPubkeyProvider(const CPubKey& pubkey) : m_pubkey(pubkey) {}
    bool GetPubKey(int pos, const SigningProvider& arg, CPubKey* key, KeyOriginInfo& info) const override
    {
        if (key) *key = m_pubkey;
        info.path.clear();
        CKeyID keyid = m_pubkey.GetID();
        std::copy(keyid.begin(), keyid.begin() + sizeof(info.fingerprint), info.fingerprint);
        return true;
    }
    bool IsRange() const override { return false; }
    size_t GetSize() const override { return m_pubkey.size(); }
    std::string ToString() const override { return HexStr(m_pubkey.begin(), m_pubkey.end()); }
    bool ToPrivateString(const SigningProvider& arg, std::string& ret) const override
    {
        CKey key;
        if (!arg.GetKey(m_pubkey.GetID(), key)) return false;
        ret = EncodeSecret(key);
        return true;
    }
};

enum class DeriveType {
    NO,
    UNHARDENED,
    HARDENED,
};

/*表示描述符中解析的扩展公钥的对象。*/
class BIP32PubkeyProvider final : public PubkeyProvider
{
    CExtPubKey m_extkey;
    KeyPath m_path;
    DeriveType m_derive;

    bool GetExtKey(const SigningProvider& arg, CExtKey& ret) const
    {
        CKey key;
        if (!arg.GetKey(m_extkey.pubkey.GetID(), key)) return false;
        ret.nDepth = m_extkey.nDepth;
        std::copy(m_extkey.vchFingerprint, m_extkey.vchFingerprint + sizeof(ret.vchFingerprint), ret.vchFingerprint);
        ret.nChild = m_extkey.nChild;
        ret.chaincode = m_extkey.chaincode;
        ret.key = key;
        return true;
    }

    bool IsHardened() const
    {
        if (m_derive == DeriveType::HARDENED) return true;
        for (auto entry : m_path) {
            if (entry >> 31) return true;
        }
        return false;
    }

public:
    BIP32PubkeyProvider(const CExtPubKey& extkey, KeyPath path, DeriveType derive) : m_extkey(extkey), m_path(std::move(path)), m_derive(derive) {}
    bool IsRange() const override { return m_derive != DeriveType::NO; }
    size_t GetSize() const override { return 33; }
    bool GetPubKey(int pos, const SigningProvider& arg, CPubKey* key, KeyOriginInfo& info) const override
    {
        if (key) {
            if (IsHardened()) {
                CExtKey extkey;
                if (!GetExtKey(arg, extkey)) return false;
                for (auto entry : m_path) {
                    extkey.Derive(extkey, entry);
                }
                if (m_derive == DeriveType::UNHARDENED) extkey.Derive(extkey, pos);
                if (m_derive == DeriveType::HARDENED) extkey.Derive(extkey, pos | 0x80000000UL);
                *key = extkey.Neuter().pubkey;
            } else {
//TODO:通过缓存优化
                CExtPubKey extkey = m_extkey;
                for (auto entry : m_path) {
                    extkey.Derive(extkey, entry);
                }
                if (m_derive == DeriveType::UNHARDENED) extkey.Derive(extkey, pos);
                assert(m_derive != DeriveType::HARDENED);
                *key = extkey.pubkey;
            }
        }
        CKeyID keyid = m_extkey.pubkey.GetID();
        std::copy(keyid.begin(), keyid.begin() + sizeof(info.fingerprint), info.fingerprint);
        info.path = m_path;
        if (m_derive == DeriveType::UNHARDENED) info.path.push_back((uint32_t)pos);
        if (m_derive == DeriveType::HARDENED) info.path.push_back(((uint32_t)pos) | 0x80000000L);
        return true;
    }
    std::string ToString() const override
    {
        std::string ret = EncodeExtPubKey(m_extkey) + FormatKeyPath(m_path);
        if (IsRange()) {
            /*+=“/*”；
            如果（m_derivate==derivateType:：hardend）ret+='\''；
        }
        返回RET；
    }
    bool toprivatestring（const signingprovider&arg，std:：string&out）const override
    {
        CestKkey密钥；
        如果（！）getextkey（arg，key））返回false；
        out=encodeextkey（key）+formatkeypath（m_path）；
        if（isrange（））
            输出+=“/*”；
            如果（m_derivate==derivateType:：hardend）out+='\''；
        }
        回归真实；
    }
}；

/**所有描述符实现的基类。*/

class DescriptorImpl : public Descriptor
{
//！此描述符的公钥参数（pk、pkh、wpkh的大小为1；multisig的任何大小）。
    const std::vector<std::unique_ptr<PubkeyProvider>> m_pubkey_args;
//！子描述符参数（除sh和wsh之外的所有内容的nullptr）。
    const std::unique_ptr<DescriptorImpl> m_script_arg;
//！描述符函数的字符串名称。
    const std::string m_name;

protected:
//！返回除pubkey和script参数之外的任何其他参数的序列化，并将其前置。
    virtual std::string ToStringExtra() const { return ""; }

    /*用于构造此描述符脚本的助手函数。
     *
     *此函数对通过评估生成的每个CScript调用一次。
     *m_script_arg，或者在m_script_arg为nullptr的情况下只执行一次。

     *@param pubkeys m_pubkey_args字段的计算。
     *@param script m_script_arg的计算结果（或当m_script_arg为nullptr时为nullptr）。
     *@param输出一个FlatSigningProvider，将解算器所需的脚本或公钥放入其中。
     *此函数的script和pubkeys参数将自动添加。
     *@返回带有此描述符的scriptPubKeys的向量。
     **/

    virtual std::vector<CScript> MakeScripts(const std::vector<CPubKey>& pubkeys, const CScript* script, FlatSigningProvider& out) const = 0;

public:
    DescriptorImpl(std::vector<std::unique_ptr<PubkeyProvider>> pubkeys, std::unique_ptr<DescriptorImpl> script, const std::string& name) : m_pubkey_args(std::move(pubkeys)), m_script_arg(std::move(script)), m_name(name) {}

    bool IsSolvable() const override
    {
        if (m_script_arg) {
            if (!m_script_arg->IsSolvable()) return false;
        }
        return true;
    }

    bool IsRange() const final
    {
        for (const auto& pubkey : m_pubkey_args) {
            if (pubkey->IsRange()) return true;
        }
        if (m_script_arg) {
            if (m_script_arg->IsRange()) return true;
        }
        return false;
    }

    bool ToStringHelper(const SigningProvider* arg, std::string& out, bool priv) const
    {
        std::string extra = ToStringExtra();
        size_t pos = extra.size() > 0 ? 1 : 0;
        std::string ret = m_name + "(" + extra;
        for (const auto& pubkey : m_pubkey_args) {
            if (pos++) ret += ",";
            std::string tmp;
            if (priv) {
                if (!pubkey->ToPrivateString(*arg, tmp)) return false;
            } else {
                tmp = pubkey->ToString();
            }
            ret += std::move(tmp);
        }
        if (m_script_arg) {
            if (pos++) ret += ",";
            std::string tmp;
            if (!m_script_arg->ToStringHelper(arg, tmp, priv)) return false;
            ret += std::move(tmp);
        }
        out = std::move(ret) + ")";
        return true;
    }

    std::string ToString() const final
    {
        std::string ret;
        ToStringHelper(nullptr, ret, false);
        return ret;
    }

    bool ToPrivateString(const SigningProvider& arg, std::string& out) const override final { return ToStringHelper(&arg, out, true); }

    bool ExpandHelper(int pos, const SigningProvider& arg, Span<const unsigned char>* cache_read, std::vector<CScript>& output_scripts, FlatSigningProvider& out, std::vector<unsigned char>* cache_write) const
    {
        std::vector<std::pair<CPubKey, KeyOriginInfo>> entries;
        entries.reserve(m_pubkey_args.size());

//在“entries”和“subscripts”中构造临时数据，以避免在失败时生成输出。
        for (const auto& p : m_pubkey_args) {
            entries.emplace_back();
            if (!p->GetPubKey(pos, arg, cache_read ? nullptr : &entries.back().first, entries.back().second)) return false;
            if (cache_read) {
//缓存的扩展公钥存在，请使用它。
                if (cache_read->size() == 0) return false;
                bool compressed = ((*cache_read)[0] == 0x02 || (*cache_read)[0] == 0x03) && cache_read->size() >= 33;
                bool uncompressed = ((*cache_read)[0] == 0x04) && cache_read->size() >= 65;
                if (!(compressed || uncompressed)) return false;
                CPubKey pubkey(cache_read->begin(), cache_read->begin() + (compressed ? 33 : 65));
                entries.back().first = pubkey;
                *cache_read = cache_read->subspan(compressed ? 33 : 65);
            }
            if (cache_write) {
                cache_write->insert(cache_write->end(), entries.back().first.begin(), entries.back().first.end());
            }
        }
        std::vector<CScript> subscripts;
        if (m_script_arg) {
            FlatSigningProvider subprovider;
            if (!m_script_arg->ExpandHelper(pos, arg, cache_read, subscripts, subprovider, cache_write)) return false;
            out = Merge(out, subprovider);
        }

        std::vector<CPubKey> pubkeys;
        pubkeys.reserve(entries.size());
        for (auto& entry : entries) {
            pubkeys.push_back(entry.first);
            out.origins.emplace(entry.first.GetID(), std::move(entry.second));
            out.pubkeys.emplace(entry.first.GetID(), entry.first);
        }
        if (m_script_arg) {
            for (const auto& subscript : subscripts) {
                out.scripts.emplace(CScriptID(subscript), subscript);
                std::vector<CScript> addscripts = MakeScripts(pubkeys, &subscript, out);
                for (auto& addscript : addscripts) {
                    output_scripts.push_back(std::move(addscript));
                }
            }
        } else {
            output_scripts = MakeScripts(pubkeys, nullptr, out);
        }
        return true;
    }

    bool Expand(int pos, const SigningProvider& provider, std::vector<CScript>& output_scripts, FlatSigningProvider& out, std::vector<unsigned char>* cache = nullptr) const final
    {
        return ExpandHelper(pos, provider, nullptr, output_scripts, out, cache);
    }

    bool ExpandFromCache(int pos, const std::vector<unsigned char>& cache, std::vector<CScript>& output_scripts, FlatSigningProvider& out) const final
    {
        Span<const unsigned char> span = MakeSpan(cache);
        return ExpandHelper(pos, DUMMY_SIGNING_PROVIDER, &span, output_scripts, out, nullptr) && span.size() == 0;
    }
};

/*用一个元素构造一个向量，这个元素被移入其中。*/
template<typename T>
std::vector<T> Singleton(T elem)
{
    std::vector<T> ret;
    ret.emplace_back(std::move(elem));
    return ret;
}

/*解析的地址（A）描述符。*/
class AddressDescriptor final : public DescriptorImpl
{
    const CTxDestination m_destination;
protected:
    std::string ToStringExtra() const override { return EncodeDestination(m_destination); }
    std::vector<CScript> MakeScripts(const std::vector<CPubKey>&, const CScript*, FlatSigningProvider&) const override { return Singleton(GetScriptForDestination(m_destination)); }
public:
    AddressDescriptor(CTxDestination destination) : DescriptorImpl({}, {}, "addr"), m_destination(std::move(destination)) {}
    bool IsSolvable() const final { return false; }
};

/*解析的原始（H）描述符。*/
class RawDescriptor final : public DescriptorImpl
{
    const CScript m_script;
protected:
    std::string ToStringExtra() const override { return HexStr(m_script.begin(), m_script.end()); }
    std::vector<CScript> MakeScripts(const std::vector<CPubKey>&, const CScript*, FlatSigningProvider&) const override { return Singleton(m_script); }
public:
    RawDescriptor(CScript script) : DescriptorImpl({}, {}, "raw"), m_script(std::move(script)) {}
    bool IsSolvable() const final { return false; }
};

/*解析的pk（p）描述符。*/
class PKDescriptor final : public DescriptorImpl
{
protected:
    std::vector<CScript> MakeScripts(const std::vector<CPubKey>& keys, const CScript*, FlatSigningProvider&) const override { return Singleton(GetScriptForRawPubKey(keys[0])); }
public:
    PKDescriptor(std::unique_ptr<PubkeyProvider> prov) : DescriptorImpl(Singleton(std::move(prov)), {}, "pk") {}
};

/*解析的pkh（p）描述符。*/
class PKHDescriptor final : public DescriptorImpl
{
protected:
    std::vector<CScript> MakeScripts(const std::vector<CPubKey>& keys, const CScript*, FlatSigningProvider&) const override { return Singleton(GetScriptForDestination(keys[0].GetID())); }
public:
    PKHDescriptor(std::unique_ptr<PubkeyProvider> prov) : DescriptorImpl(Singleton(std::move(prov)), {}, "pkh") {}
};

/*解析的wpkh（p）描述符。*/
class WPKHDescriptor final : public DescriptorImpl
{
protected:
    std::vector<CScript> MakeScripts(const std::vector<CPubKey>& keys, const CScript*, FlatSigningProvider&) const override { return Singleton(GetScriptForDestination(WitnessV0KeyHash(keys[0].GetID()))); }
public:
    WPKHDescriptor(std::unique_ptr<PubkeyProvider> prov) : DescriptorImpl(Singleton(std::move(prov)), {}, "wpkh") {}
};

/*解析的组合（P）描述符。*/
class ComboDescriptor final : public DescriptorImpl
{
protected:
    std::vector<CScript> MakeScripts(const std::vector<CPubKey>& keys, const CScript*, FlatSigningProvider& out) const override
    {
        std::vector<CScript> ret;
        CKeyID id = keys[0].GetID();
ret.emplace_back(GetScriptForRawPubKey(keys[0])); //2PK
ret.emplace_back(GetScriptForDestination(id)); //2PKH
        if (keys[0].IsCompressed()) {
            CScript p2wpkh = GetScriptForDestination(WitnessV0KeyHash(id));
            out.scripts.emplace(CScriptID(p2wpkh), p2wpkh);
            ret.emplace_back(p2wpkh);
ret.emplace_back(GetScriptForDestination(CScriptID(p2wpkh))); //2SH—2WPKH
        }
        return ret;
    }
public:
    ComboDescriptor(std::unique_ptr<PubkeyProvider> prov) : DescriptorImpl(Singleton(std::move(prov)), {}, "combo") {}
};

/*解析的多（…）描述符。*/
class MultisigDescriptor final : public DescriptorImpl
{
    const int m_threshold;
protected:
    std::string ToStringExtra() const override { return strprintf("%i", m_threshold); }
    std::vector<CScript> MakeScripts(const std::vector<CPubKey>& keys, const CScript*, FlatSigningProvider&) const override { return Singleton(GetScriptForMultisig(m_threshold, keys)); }
public:
    MultisigDescriptor(int threshold, std::vector<std::unique_ptr<PubkeyProvider>> providers) : DescriptorImpl(std::move(providers), {}, "multi"), m_threshold(threshold) {}
};

/*解析的sh（…）描述符。*/
class SHDescriptor final : public DescriptorImpl
{
protected:
    std::vector<CScript> MakeScripts(const std::vector<CPubKey>&, const CScript* script, FlatSigningProvider&) const override { return Singleton(GetScriptForDestination(CScriptID(*script))); }
public:
    SHDescriptor(std::unique_ptr<DescriptorImpl> desc) : DescriptorImpl({}, std::move(desc), "sh") {}
};

/*解析的wsh（…）描述符。*/
class WSHDescriptor final : public DescriptorImpl
{
protected:
    std::vector<CScript> MakeScripts(const std::vector<CPubKey>&, const CScript* script, FlatSigningProvider&) const override { return Singleton(GetScriptForDestination(WitnessV0ScriptHash(*script))); }
public:
    WSHDescriptor(std::unique_ptr<DescriptorImpl> desc) : DescriptorImpl({}, std::move(desc), "wsh") {}
};

//////////////////////////////////////////////////
//分析器//
//////////////////////////////////////////////////

enum class ParseScriptContext {
    TOP,
    P2SH,
    P2WSH,
};

/*分析常量。如果成功，将更新sp以跳过常量并返回true。*/
bool Const(const std::string& str, Span<const char>& sp)
{
    if ((size_t)sp.size() >= str.size() && std::equal(str.begin(), str.end(), sp.begin())) {
        sp = sp.subspan(str.size());
        return true;
    }
    return false;
}

/*分析函数调用。如果成功，sp将更新为函数的参数。*/
bool Func(const std::string& str, Span<const char>& sp)
{
    if ((size_t)sp.size() >= str.size() + 2 && sp[str.size()] == '(' && sp[sp.size() - 1] == ')' && std::equal(str.begin(), str.end(), sp.begin())) {
        sp = sp.subspan(str.size() + 1, sp.size() - str.size() - 2);
        return true;
    }
    return false;
}

/*返回sp开头的表达式，并更新sp以跳过它。*/
Span<const char> Expr(Span<const char>& sp)
{
    int level = 0;
    auto it = sp.begin();
    while (it != sp.end()) {
        if (*it == '(') {
            ++level;
        } else if (level && *it == ')') {
            --level;
        } else if (level == 0 && (*it == ')' || *it == ',')) {
            break;
        }
        ++it;
    }
    Span<const char> ret = sp.first(it - sp.begin());
    sp = sp.subspan(it - sp.begin());
    return ret;
}

/*在sep的每个实例上拆分一个字符串，返回一个向量。*/
std::vector<Span<const char>> Split(const Span<const char>& sp, char sep)
{
    std::vector<Span<const char>> ret;
    auto it = sp.begin();
    auto start = it;
    while (it != sp.end()) {
        if (*it == sep) {
            ret.emplace_back(start, it);
            start = it + 1;
        }
        ++it;
    }
    ret.emplace_back(start, it);
    return ret;
}

/*解析一个键路径，传递一个拆分的元素列表（忽略第一个元素）。*/
NODISCARD bool ParseKeyPath(const std::vector<Span<const char>>& split, KeyPath& out)
{
    for (size_t i = 1; i < split.size(); ++i) {
        Span<const char> elem = split[i];
        bool hardened = false;
        if (elem.size() > 0 && (elem[elem.size() - 1] == '\'' || elem[elem.size() - 1] == 'h')) {
            elem = elem.first(elem.size() - 1);
            hardened = true;
        }
        uint32_t p;
        if (!ParseUInt32(std::string(elem.begin(), elem.end()), &p) || p > 0x7FFFFFFFUL) return false;
        out.push_back(p | (((uint32_t)hardened) << 31));
    }
    return true;
}

/*分析排除源信息的公钥。*/
std::unique_ptr<PubkeyProvider> ParsePubkeyInner(const Span<const char>& sp, bool permit_uncompressed, FlatSigningProvider& out)
{
    auto split = Split(sp, '/');
    std::string str(split[0].begin(), split[0].end());
    if (split.size() == 1) {
        if (IsHex(str)) {
            std::vector<unsigned char> data = ParseHex(str);
            CPubKey pubkey(data);
            if (pubkey.IsFullyValid() && (permit_uncompressed || pubkey.IsCompressed())) return MakeUnique<ConstPubkeyProvider>(pubkey);
        }
        CKey key = DecodeSecret(str);
        if (key.IsValid() && (permit_uncompressed || key.IsCompressed())) {
            CPubKey pubkey = key.GetPubKey();
            out.keys.emplace(pubkey.GetID(), key);
            return MakeUnique<ConstPubkeyProvider>(pubkey);
        }
    }
    CExtKey extkey = DecodeExtKey(str);
    CExtPubKey extpubkey = DecodeExtPubKey(str);
    if (!extkey.key.IsValid() && !extpubkey.pubkey.IsValid()) return nullptr;
    KeyPath path;
    DeriveType type = DeriveType::NO;
    if (split.back() == MakeSpan("*").first(1)) {
        split.pop_back();
        type = DeriveType::UNHARDENED;
    } else if (split.back() == MakeSpan("*'").first(2) || split.back() == MakeSpan("*h").first(2)) {
        split.pop_back();
        type = DeriveType::HARDENED;
    }
    if (!ParseKeyPath(split, path)) return nullptr;
    if (extkey.key.IsValid()) {
        extpubkey = extkey.Neuter();
        out.keys.emplace(extpubkey.pubkey.GetID(), extkey.key);
    }
    return MakeUnique<BIP32PubkeyProvider>(extpubkey, std::move(path), type);
}

/*解析包含源信息的公钥（如果启用）。*/
std::unique_ptr<PubkeyProvider> ParsePubkey(const Span<const char>& sp, bool permit_uncompressed, FlatSigningProvider& out)
{
    auto origin_split = Split(sp, ']');
    if (origin_split.size() > 2) return nullptr;
    if (origin_split.size() == 1) return ParsePubkeyInner(origin_split[0], permit_uncompressed, out);
    if (origin_split[0].size() < 1 || origin_split[0][0] != '[') return nullptr;
    auto slash_split = Split(origin_split[0].subspan(1), '/');
    if (slash_split[0].size() != 8) return nullptr;
    std::string fpr_hex = std::string(slash_split[0].begin(), slash_split[0].end());
    if (!IsHex(fpr_hex)) return nullptr;
    auto fpr_bytes = ParseHex(fpr_hex);
    KeyOriginInfo info;
    static_assert(sizeof(info.fingerprint) == 4, "Fingerprint must be 4 bytes");
    assert(fpr_bytes.size() == 4);
    std::copy(fpr_bytes.begin(), fpr_bytes.end(), info.fingerprint);
    if (!ParseKeyPath(slash_split, info.path)) return nullptr;
    auto provider = ParsePubkeyInner(origin_split[1], permit_uncompressed, out);
    if (!provider) return nullptr;
    return MakeUnique<OriginPubkeyProvider>(std::move(info), std::move(provider));
}

/*在特定上下文中分析脚本。*/
std::unique_ptr<DescriptorImpl> ParseScript(Span<const char>& sp, ParseScriptContext ctx, FlatSigningProvider& out)
{
    auto expr = Expr(sp);
    if (Func("pk", expr)) {
        auto pubkey = ParsePubkey(expr, ctx != ParseScriptContext::P2WSH, out);
        if (!pubkey) return nullptr;
        return MakeUnique<PKDescriptor>(std::move(pubkey));
    }
    if (Func("pkh", expr)) {
        auto pubkey = ParsePubkey(expr, ctx != ParseScriptContext::P2WSH, out);
        if (!pubkey) return nullptr;
        return MakeUnique<PKHDescriptor>(std::move(pubkey));
    }
    if (ctx == ParseScriptContext::TOP && Func("combo", expr)) {
        auto pubkey = ParsePubkey(expr, true, out);
        if (!pubkey) return nullptr;
        return MakeUnique<ComboDescriptor>(std::move(pubkey));
    }
    if (Func("multi", expr)) {
        auto threshold = Expr(expr);
        uint32_t thres;
        std::vector<std::unique_ptr<PubkeyProvider>> providers;
        if (!ParseUInt32(std::string(threshold.begin(), threshold.end()), &thres)) return nullptr;
        size_t script_size = 0;
        while (expr.size()) {
            if (!Const(",", expr)) return nullptr;
            auto arg = Expr(expr);
            auto pk = ParsePubkey(arg, ctx != ParseScriptContext::P2WSH, out);
            if (!pk) return nullptr;
            script_size += pk->GetSize() + 1;
            providers.emplace_back(std::move(pk));
        }
        if (providers.size() < 1 || providers.size() > 16 || thres < 1 || thres > providers.size()) return nullptr;
        if (ctx == ParseScriptContext::TOP) {
if (providers.size() > 3) return nullptr; //对于原始multisig，不超过3个公钥
        }
        if (ctx == ParseScriptContext::P2SH) {
if (script_size + 3 > 520) return nullptr; //强制p2sh脚本大小限制
        }
        return MakeUnique<MultisigDescriptor>(thres, std::move(providers));
    }
    if (ctx != ParseScriptContext::P2WSH && Func("wpkh", expr)) {
        auto pubkey = ParsePubkey(expr, false, out);
        if (!pubkey) return nullptr;
        return MakeUnique<WPKHDescriptor>(std::move(pubkey));
    }
    if (ctx == ParseScriptContext::TOP && Func("sh", expr)) {
        auto desc = ParseScript(expr, ParseScriptContext::P2SH, out);
        if (!desc || expr.size()) return nullptr;
        return MakeUnique<SHDescriptor>(std::move(desc));
    }
    if (ctx != ParseScriptContext::P2WSH && Func("wsh", expr)) {
        auto desc = ParseScript(expr, ParseScriptContext::P2WSH, out);
        if (!desc || expr.size()) return nullptr;
        return MakeUnique<WSHDescriptor>(std::move(desc));
    }
    if (ctx == ParseScriptContext::TOP && Func("addr", expr)) {
        CTxDestination dest = DecodeDestination(std::string(expr.begin(), expr.end()));
        if (!IsValidDestination(dest)) return nullptr;
        return MakeUnique<AddressDescriptor>(std::move(dest));
    }
    if (ctx == ParseScriptContext::TOP && Func("raw", expr)) {
        std::string str(expr.begin(), expr.end());
        if (!IsHex(str)) return nullptr;
        auto bytes = ParseHex(str);
        return MakeUnique<RawDescriptor>(CScript(bytes.begin(), bytes.end()));
    }
    return nullptr;
}

std::unique_ptr<PubkeyProvider> InferPubkey(const CPubKey& pubkey, ParseScriptContext, const SigningProvider& provider)
{
    std::unique_ptr<PubkeyProvider> key_provider = MakeUnique<ConstPubkeyProvider>(pubkey);
    KeyOriginInfo info;
    if (provider.GetKeyOrigin(pubkey.GetID(), info)) {
        return MakeUnique<OriginPubkeyProvider>(std::move(info), std::move(key_provider));
    }
    return key_provider;
}

std::unique_ptr<DescriptorImpl> InferScript(const CScript& script, ParseScriptContext ctx, const SigningProvider& provider)
{
    std::vector<std::vector<unsigned char>> data;
    txnouttype txntype = Solver(script, data);

    if (txntype == TX_PUBKEY) {
        CPubKey pubkey(data[0].begin(), data[0].end());
        if (pubkey.IsValid()) {
            return MakeUnique<PKDescriptor>(InferPubkey(pubkey, ctx, provider));
        }
    }
    if (txntype == TX_PUBKEYHASH) {
        uint160 hash(data[0]);
        CKeyID keyid(hash);
        CPubKey pubkey;
        if (provider.GetPubKey(keyid, pubkey)) {
            return MakeUnique<PKHDescriptor>(InferPubkey(pubkey, ctx, provider));
        }
    }
    if (txntype == TX_WITNESS_V0_KEYHASH && ctx != ParseScriptContext::P2WSH) {
        uint160 hash(data[0]);
        CKeyID keyid(hash);
        CPubKey pubkey;
        if (provider.GetPubKey(keyid, pubkey)) {
            return MakeUnique<WPKHDescriptor>(InferPubkey(pubkey, ctx, provider));
        }
    }
    if (txntype == TX_MULTISIG) {
        std::vector<std::unique_ptr<PubkeyProvider>> providers;
        for (size_t i = 1; i + 1 < data.size(); ++i) {
            CPubKey pubkey(data[i].begin(), data[i].end());
            providers.push_back(InferPubkey(pubkey, ctx, provider));
        }
        return MakeUnique<MultisigDescriptor>((int)data[0][0], std::move(providers));
    }
    if (txntype == TX_SCRIPTHASH && ctx == ParseScriptContext::TOP) {
        uint160 hash(data[0]);
        CScriptID scriptid(hash);
        CScript subscript;
        if (provider.GetCScript(scriptid, subscript)) {
            auto sub = InferScript(subscript, ParseScriptContext::P2SH, provider);
            if (sub) return MakeUnique<SHDescriptor>(std::move(sub));
        }
    }
    if (txntype == TX_WITNESS_V0_SCRIPTHASH && ctx != ParseScriptContext::P2WSH) {
        CScriptID scriptid;
        CRIPEMD160().Write(data[0].data(), data[0].size()).Finalize(scriptid.begin());
        CScript subscript;
        if (provider.GetCScript(scriptid, subscript)) {
            auto sub = InferScript(subscript, ParseScriptContext::P2WSH, provider);
            if (sub) return MakeUnique<WSHDescriptor>(std::move(sub));
        }
    }

    CTxDestination dest;
    if (ExtractDestination(script, dest)) {
        if (GetScriptForDestination(dest) == script) {
            return MakeUnique<AddressDescriptor>(std::move(dest));
        }
    }

    return MakeUnique<RawDescriptor>(script);
}

} //命名空间

std::unique_ptr<Descriptor> Parse(const std::string& descriptor, FlatSigningProvider& out)
{
    Span<const char> sp(descriptor.data(), descriptor.size());
    auto ret = ParseScript(sp, ParseScriptContext::TOP, out);
    if (sp.size() == 0 && ret) return std::unique_ptr<Descriptor>(std::move(ret));
    return nullptr;
}

std::unique_ptr<Descriptor> InferDescriptor(const CScript& script, const SigningProvider& provider)
{
    return InferScript(script, ParseScriptContext::TOP, provider);
}
