
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

#ifndef BITCOIN_KEYSTORE_H
#define BITCOIN_KEYSTORE_H

#include <key.h>
#include <pubkey.h>
#include <script/script.h>
#include <script/sign.h>
#include <script/standard.h>
#include <sync.h>

#include <boost/signals2/signal.hpp>

/*密钥存储的虚拟基类*/
class CKeyStore : public SigningProvider
{
public:
//！向商店添加密钥。
    virtual bool AddKeyPubKey(const CKey &key, const CPubKey &pubkey) =0;

//！检查存储中是否存在与给定地址对应的密钥。
    virtual bool HaveKey(const CKeyID &address) const =0;
    virtual std::set<CKeyID> GetKeys() const =0;

//！对BIP 0013的支持：请参阅https://github.com/bitcoin/bips/blob/master/bip-0013.mediawiki
    virtual bool AddCScript(const CScript& redeemScript) =0;
    virtual bool HaveCScript(const CScriptID &hash) const =0;
    virtual std::set<CScriptID> GetCScripts() const =0;

//！支持仅监视地址
    virtual bool AddWatchOnly(const CScript &dest) =0;
    virtual bool RemoveWatchOnly(const CScript &dest) =0;
    virtual bool HaveWatchOnly(const CScript &dest) const =0;
    virtual bool HaveWatchOnly() const =0;
};

/*基本密钥存储区，它将密钥保存在地址->秘密映射中*/
class CBasicKeyStore : public CKeyStore
{
protected:
    mutable CCriticalSection cs_KeyStore;

    using KeyMap = std::map<CKeyID, CKey>;
    using WatchKeyMap = std::map<CKeyID, CPubKey>;
    using ScriptMap = std::map<CScriptID, CScript>;
    using WatchOnlySet = std::set<CScript>;

    KeyMap mapKeys GUARDED_BY(cs_KeyStore);
    WatchKeyMap mapWatchKeys GUARDED_BY(cs_KeyStore);
    ScriptMap mapScripts GUARDED_BY(cs_KeyStore);
    WatchOnlySet setWatchOnly GUARDED_BY(cs_KeyStore);

    void ImplicitlyLearnRelatedKeyScripts(const CPubKey& pubkey) EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);

public:
    bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey) override;
    bool AddKey(const CKey &key) { return AddKeyPubKey(key, key.GetPubKey()); }
    bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const override;
    bool HaveKey(const CKeyID &address) const override;
    std::set<CKeyID> GetKeys() const override;
    bool GetKey(const CKeyID &address, CKey &keyOut) const override;
    bool AddCScript(const CScript& redeemScript) override;
    bool HaveCScript(const CScriptID &hash) const override;
    std::set<CScriptID> GetCScripts() const override;
    bool GetCScript(const CScriptID &hash, CScript& redeemScriptOut) const override;

    bool AddWatchOnly(const CScript &dest) override;
    bool RemoveWatchOnly(const CScript &dest) override;
    bool HaveWatchOnly(const CScript &dest) const override;
    bool HaveWatchOnly() const override;
};

/*返回脚本中涉及的键的CKeyID（如果有唯一的键）。*/
CKeyID GetKeyForDestination(const CKeyStore& store, const CTxDestination& dest);

/*检查是否在给定的压缩或其他文档中*/
bool HaveKey(const CKeyStore& store, const CKey& key);

#endif //比特币密钥库
