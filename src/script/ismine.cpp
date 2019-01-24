
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

#include <script/ismine.h>

#include <key.h>
#include <keystore.h>
#include <script/script.h>
#include <script/sign.h>


typedef std::vector<unsigned char> valtype;

namespace {

/*
 *这是跟踪脚本执行上下文的枚举，类似于
 *脚本/解释器中的sigversion。但是它是分开的，因为我们想
 *区分顶级scriptpubkey执行和p2sh redeemscript
 *执行（对共识规则没有影响的区别）。
 **/

enum class IsMineSigVersion
{
TOP = 0,        //！<scriptpubkey执行
P2SH = 1,       //！<p2sh redeemscript
WITNESS_V0 = 2, //！<p2wsh见证脚本执行
};

/*
 *这是isminetype+invalidity的内部表示。
 *它的顺序很重要，因为我们返回所有探索的最大值
 *可能性。
 **/

enum class IsMineResult
{
NO = 0,         //！不是我们的
WATCH_ONLY = 1, //！<包含在仅手表余额中
SPENDABLE = 2,  //！<包括在所有余额中
INVALID = 3,    //！<no spendable by anyone（segwit中的uncompressed pubkey，p2sh中的p2sh或witness，witness中的witness）
};

bool PermitsUncompressed(IsMineSigVersion sigversion)
{
    return sigversion == IsMineSigVersion::TOP || sigversion == IsMineSigVersion::P2SH;
}

bool HaveKeys(const std::vector<valtype>& pubkeys, const CKeyStore& keystore)
{
    for (const valtype& pubkey : pubkeys) {
        CKeyID keyID = CPubKey(pubkey).GetID();
        if (!keystore.HaveKey(keyID)) return false;
    }
    return true;
}

IsMineResult IsMineInner(const CKeyStore& keystore, const CScript& scriptPubKey, IsMineSigVersion sigversion)
{
    IsMineResult ret = IsMineResult::NO;

    std::vector<valtype> vSolutions;
    txnouttype whichType = Solver(scriptPubKey, vSolutions);

    CKeyID keyID;
    switch (whichType)
    {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
    case TX_WITNESS_UNKNOWN:
        break;
    case TX_PUBKEY:
        keyID = CPubKey(vSolutions[0]).GetID();
        if (!PermitsUncompressed(sigversion) && vSolutions[0].size() != 33) {
            return IsMineResult::INVALID;
        }
        if (keystore.HaveKey(keyID)) {
            ret = std::max(ret, IsMineResult::SPENDABLE);
        }
        break;
    case TX_WITNESS_V0_KEYHASH:
    {
        if (sigversion == IsMineSigVersion::WITNESS_V0) {
//p2wsh中的p2wpkh无效。
            return IsMineResult::INVALID;
        }
        if (sigversion == IsMineSigVersion::TOP && !keystore.HaveCScript(CScriptID(CScript() << OP_0 << vSolutions[0]))) {
//我们不支持裸见证输出，除非它的p2sh版本
//也可以接受。这可以在segwit激活之前防止匹配。
//这也适用于p2wsh情况。
            break;
        }
        ret = std::max(ret, IsMineInner(keystore, GetScriptForDestination(CKeyID(uint160(vSolutions[0]))), IsMineSigVersion::WITNESS_V0));
        break;
    }
    case TX_PUBKEYHASH:
        keyID = CKeyID(uint160(vSolutions[0]));
        if (!PermitsUncompressed(sigversion)) {
            CPubKey pubkey;
            if (keystore.GetPubKey(keyID, pubkey) && !pubkey.IsCompressed()) {
                return IsMineResult::INVALID;
            }
        }
        if (keystore.HaveKey(keyID)) {
            ret = std::max(ret, IsMineResult::SPENDABLE);
        }
        break;
    case TX_SCRIPTHASH:
    {
        if (sigversion != IsMineSigVersion::TOP) {
//p2wsh或p2sh内的p2sh无效。
            return IsMineResult::INVALID;
        }
        CScriptID scriptID = CScriptID(uint160(vSolutions[0]));
        CScript subscript;
        if (keystore.GetCScript(scriptID, subscript)) {
            ret = std::max(ret, IsMineInner(keystore, subscript, IsMineSigVersion::P2SH));
        }
        break;
    }
    case TX_WITNESS_V0_SCRIPTHASH:
    {
        if (sigversion == IsMineSigVersion::WITNESS_V0) {
//p2wsh中的p2wsh无效。
            return IsMineResult::INVALID;
        }
        if (sigversion == IsMineSigVersion::TOP && !keystore.HaveCScript(CScriptID(CScript() << OP_0 << vSolutions[0]))) {
            break;
        }
        uint160 hash;
        CRIPEMD160().Write(&vSolutions[0][0], vSolutions[0].size()).Finalize(hash.begin());
        CScriptID scriptID = CScriptID(hash);
        CScript subscript;
        if (keystore.GetCScript(scriptID, subscript)) {
            ret = std::max(ret, IsMineInner(keystore, subscript, IsMineSigVersion::WITNESS_V0));
        }
        break;
    }

    case TX_MULTISIG:
    {
//切勿将裸多信号输出视为我们的输出（但它们仍然只能被监视）
        if (sigversion == IsMineSigVersion::TOP) {
            break;
        }

//只有当我们拥有所有
//涉及的关键。多签名事务
//部分拥有（别人有一把钥匙可以花掉
//它们）能够从你的攻击下支出，特别是
//在共享钱包的情况下。
        std::vector<valtype> keys(vSolutions.begin()+1, vSolutions.begin()+vSolutions.size()-1);
        if (!PermitsUncompressed(sigversion)) {
            for (size_t i = 0; i < keys.size(); i++) {
                if (keys[i].size() != 33) {
                    return IsMineResult::INVALID;
                }
            }
        }
        if (HaveKeys(keys, keystore)) {
            ret = std::max(ret, IsMineResult::SPENDABLE);
        }
        break;
    }
    }

    if (ret == IsMineResult::NO && keystore.HaveWatchOnly(scriptPubKey)) {
        ret = std::max(ret, IsMineResult::WATCH_ONLY);
    }
    return ret;
}

} //命名空间

isminetype IsMine(const CKeyStore& keystore, const CScript& scriptPubKey)
{
    switch (IsMineInner(keystore, scriptPubKey, IsMineSigVersion::TOP)) {
    case IsMineResult::INVALID:
    case IsMineResult::NO:
        return ISMINE_NO;
    case IsMineResult::WATCH_ONLY:
        return ISMINE_WATCH_ONLY;
    case IsMineResult::SPENDABLE:
        return ISMINE_SPENDABLE;
    }
    assert(false);
}

isminetype IsMine(const CKeyStore& keystore, const CTxDestination& dest)
{
    CScript script = GetScriptForDestination(dest);
    return IsMine(keystore, script);
}
