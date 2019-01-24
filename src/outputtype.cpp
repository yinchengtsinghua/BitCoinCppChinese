
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

#include <outputtype.h>

#include <keystore.h>
#include <pubkey.h>
#include <script/script.h>
#include <script/standard.h>

#include <assert.h>
#include <string>

static const std::string OUTPUT_TYPE_STRING_LEGACY = "legacy";
static const std::string OUTPUT_TYPE_STRING_P2SH_SEGWIT = "p2sh-segwit";
static const std::string OUTPUT_TYPE_STRING_BECH32 = "bech32";

bool ParseOutputType(const std::string& type, OutputType& output_type)
{
    if (type == OUTPUT_TYPE_STRING_LEGACY) {
        output_type = OutputType::LEGACY;
        return true;
    } else if (type == OUTPUT_TYPE_STRING_P2SH_SEGWIT) {
        output_type = OutputType::P2SH_SEGWIT;
        return true;
    } else if (type == OUTPUT_TYPE_STRING_BECH32) {
        output_type = OutputType::BECH32;
        return true;
    }
    return false;
}

const std::string& FormatOutputType(OutputType type)
{
    switch (type) {
    case OutputType::LEGACY: return OUTPUT_TYPE_STRING_LEGACY;
    case OutputType::P2SH_SEGWIT: return OUTPUT_TYPE_STRING_P2SH_SEGWIT;
    case OutputType::BECH32: return OUTPUT_TYPE_STRING_BECH32;
    default: assert(false);
    }
}

CTxDestination GetDestinationForKey(const CPubKey& key, OutputType type)
{
    switch (type) {
    case OutputType::LEGACY: return key.GetID();
    case OutputType::P2SH_SEGWIT:
    case OutputType::BECH32: {
        if (!key.IsCompressed()) return key.GetID();
        CTxDestination witdest = WitnessV0KeyHash(key.GetID());
        CScript witprog = GetScriptForDestination(witdest);
        if (type == OutputType::P2SH_SEGWIT) {
            return CScriptID(witprog);
        } else {
            return witdest;
        }
    }
    default: assert(false);
    }
}

std::vector<CTxDestination> GetAllDestinationsForKey(const CPubKey& key)
{
    CKeyID keyid = key.GetID();
    if (key.IsCompressed()) {
        CTxDestination segwit = WitnessV0KeyHash(keyid);
        CTxDestination p2sh = CScriptID(GetScriptForDestination(segwit));
        return std::vector<CTxDestination>{std::move(keyid), std::move(p2sh), std::move(segwit)};
    } else {
        return std::vector<CTxDestination>{std::move(keyid)};
    }
}

CTxDestination AddAndGetDestinationForScript(CKeyStore& keystore, const CScript& script, OutputType type)
{
//将脚本添加到密钥库
    keystore.AddCScript(script);
//请注意，还不支持超过520字节的脚本。
    switch (type) {
    case OutputType::LEGACY:
        return CScriptID(script);
    case OutputType::P2SH_SEGWIT:
    case OutputType::BECH32: {
        CTxDestination witdest = WitnessV0ScriptHash(script);
        CScript witprog = GetScriptForDestination(witdest);
//检查生成的程序是否可解决（即不使用未压缩的密钥）
        if (!IsSolvable(keystore, witprog)) return CScriptID(script);
//添加redeemscript，以便将p2wsh和p2sh-p2wsh输出识别为我们的输出。
        keystore.AddCScript(witprog);
        if (type == OutputType::BECH32) {
            return witdest;
        } else {
            return CScriptID(witprog);
        }
    }
    default: assert(false);
    }
}

