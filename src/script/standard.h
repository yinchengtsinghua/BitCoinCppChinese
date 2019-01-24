
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

#ifndef BITCOIN_SCRIPT_STANDARD_H
#define BITCOIN_SCRIPT_STANDARD_H

#include <script/interpreter.h>
#include <uint256.h>

#include <boost/variant.hpp>

#include <stdint.h>

static const bool DEFAULT_ACCEPT_DATACARRIER = true;

class CKeyID;
class CScript;

/*对cscript的引用：其序列化的hash160（请参见script.h）*/
class CScriptID : public uint160
{
public:
    CScriptID() : uint160() {}
    explicit CScriptID(const CScript& in);
    CScriptID(const uint160& in) : uint160(in) {}
};

/*
 *nmaxDataCarrierBytes的默认设置。80字节的数据，+1用于op_返回，
 *+2表示pushdata操作码。
 **/

static const unsigned int MAX_OP_RETURN_RELAY = 83;

/*
 *数据承载输出是包含数据的不可依赖输出。剧本
 *类型被指定为Tx_空_数据。
 **/

extern bool fAcceptDatacarrier;

/*此节点认为标准的Tx_空_数据脚本的最大大小。*/
extern unsigned nMaxDatacarrierBytes;

/*
 *所有新块必须符合的强制脚本验证标志
 *有效。（但旧块可能不符合）目前只有p2sh，
 *但在将来可能会添加其他标志，例如用于强制执行的软分叉
 *严格的顺序编码。
 *
 *如果这些测试中有一个失败，可能会触发DoS禁令-请参阅checkinputs（）了解
 *详情。
 **/

static const unsigned int MANDATORY_SCRIPT_VERIFY_FLAGS = SCRIPT_VERIFY_P2SH;

enum txnouttype
{
    TX_NONSTANDARD,
//“标准”交易类型：
    TX_PUBKEY,
    TX_PUBKEYHASH,
    TX_SCRIPTHASH,
    TX_MULTISIG,
TX_NULL_DATA, //！<unspendable op_返回带有数据的脚本
    TX_WITNESS_V0_SCRIPTHASH,
    TX_WITNESS_V0_KEYHASH,
TX_WITNESS_UNKNOWN, //！<仅适用于上面未定义的见证版本
};

class CNoDestination {
public:
    friend bool operator==(const CNoDestination &a, const CNoDestination &b) { return true; }
    friend bool operator<(const CNoDestination &a, const CNoDestination &b) { return true; }
};

struct WitnessV0ScriptHash : public uint256
{
    WitnessV0ScriptHash() : uint256() {}
    explicit WitnessV0ScriptHash(const uint256& hash) : uint256(hash) {}
    explicit WitnessV0ScriptHash(const CScript& script);
    using uint256::uint256;
};

struct WitnessV0KeyHash : public uint160
{
    WitnessV0KeyHash() : uint160() {}
    explicit WitnessV0KeyHash(const uint160& hash) : uint160(hash) {}
    using uint160::uint160;
};

//！CTX目标子类型，用于编码任何未来的见证版本
struct WitnessUnknown
{
    unsigned int version;
    unsigned int length;
    unsigned char program[40];

    friend bool operator==(const WitnessUnknown& w1, const WitnessUnknown& w2) {
        if (w1.version != w2.version) return false;
        if (w1.length != w2.length) return false;
        return std::equal(w1.program, w1.program + w1.length, w2.program);
    }

    friend bool operator<(const WitnessUnknown& w1, const WitnessUnknown& w2) {
        if (w1.version < w2.version) return true;
        if (w1.version > w2.version) return false;
        if (w1.length < w2.length) return true;
        if (w1.length > w2.length) return false;
        return std::lexicographical_compare(w1.program, w1.program + w1.length, w2.program, w2.program + w2.length);
    }
};

/*
 *具有特定目标的txout脚本模板。要么是：
 **cOnDestination:未设置目标
 **ckeyid:tx_pubkeyhash目的地（p2pkh）
 **cscriptid:tx_scripthash目的地（p2sh）
 **witness v0 scripthash:tx_witness_v0_scripthash目的地（p2wsh）
 **见证人v0 keyhash:tx见证人v0 keyhash目的地（p2wpkh）
 **目击不明：Tx目击不明目的地（P2W？？？）
 *ctxdestination是以比特币地址编码的内部数据类型。
 **/

typedef boost::variant<CNoDestination, CKeyID, CScriptID, WitnessV0ScriptHash, WitnessV0KeyHash, WitnessUnknown> CTxDestination;

/*检查CTX目标是否为CNO目标。*/
bool IsValidDestination(const CTxDestination& dest);

/*以C字符串形式获取txnoutype的名称，如果未知，则获取nullptr。*/
const char* GetTxnOutputType(txnouttype t);

/*
 *分析scriptpubkey并标识标准脚本的脚本类型。如果
 *成功，返回脚本类型和已分析的pubkeys或hash，具体取决于
 *类型。例如，对于p2sh脚本，vsolutionsret将包含
 *脚本哈希，对于p2pkh，它将包含密钥哈希等。
 *
 *@param[in]要分析的scriptpubkey脚本
 *@param[out]vsolutionsret已分析的公钥和哈希的向量
 *@返回脚本类型。tx_非标准表示解决失败。
 **/

txnouttype Solver(const CScript& scriptPubKey, std::vector<std::vector<unsigned char>>& vSolutionsRet);

/*
 *分析目标地址的标准scriptpubkey。将结果分配给
 *addressret参数，如果成功，则返回true。用于多西格
 *脚本，而使用ExtractDestinations。目前只适用于p2pk，
 *p2pkh、p2sh、p2wpkh和p2wsh脚本。
 **/

bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet);

/*
 *分析具有一个或多个目标地址的标准scriptpubkey。为了
 *multisig脚本，它用pubkey id填充addressret向量。
 *并要求使用N。对于其他目的地，
 *AddressRet由单个值填充，RequiredRet设置为1。
 *如果成功，则返回“真”。当前不从中提取地址
 
 *
 *注意：此函数混淆了目的地（CScript的子集
 *可作为地址进行编码），带有密钥标识符（涉及到的密钥的标识符）
 *cscript），其使用应逐步停止。
 **/

bool ExtractDestinations(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<CTxDestination>& addressRet, int& nRequiredRet);

/*
 *为给定的CTX目标生成比特币scriptPubkey。返回一个2pkh
 *用于ckeyid目的地的脚本、用于cscriptid的p2sh脚本和空的
 *cnoadestination的脚本。
 **/

CScript GetScriptForDestination(const CTxDestination& dest);

/*为给定的pubkey生成p2pk脚本。*/
CScript GetScriptForRawPubKey(const CPubKey& pubkey);

/*生成多任务脚本。*/
CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey>& keys);

/*
 *为给定的兑换脚本生成付款见证脚本。如果赎回
 *脚本为p2pk或p2pkh，返回p2wpkh脚本，否则返回
 *2WSH脚本。
 *
 *TODO:将对getscriptforwitness的调用替换为使用
 *各种证人特定的CTX检测子类型。
 **/

CScript GetScriptForWitness(const CScript& redeemscript);

#endif //比特币脚本标准
