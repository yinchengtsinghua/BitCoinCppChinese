
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

#ifndef BITCOIN_SCRIPT_INTERPRETER_H
#define BITCOIN_SCRIPT_INTERPRETER_H

#include <script/script_error.h>
#include <primitives/transaction.h>

#include <vector>
#include <stdint.h>
#include <string>

class CPubKey;
class CScript;
class CTransaction;
class uint256;

/*签名哈希类型/标志*/
enum
{
    SIGHASH_ALL = 1,
    SIGHASH_NONE = 2,
    SIGHASH_SINGLE = 3,
    SIGHASH_ANYONECANPAY = 0x80,
};

/*脚本验证标志。
 *
 *所有标志都是软分叉：下面的一组可接受脚本
 *标志（a_b）是标志（a）下可接受脚本的子集。
 **/

enum
{
    SCRIPT_VERIFY_NONE      = 0,

//评估p2sh下标（bip16）。
    SCRIPT_VERIFY_P2SH      = (1U << 0),

//向checksig操作传递非严格的der签名或具有未定义哈希类型的der签名会导致脚本失败。
//通过checksig对非（0x04+64字节）或（0x02或0x03+32字节）的pubkey进行计算会导致脚本失败。
//（未用作或意图用作共识规则）。
    SCRIPT_VERIFY_STRICTENC = (1U << 1),

//向checksig操作传递非严格的der签名会导致脚本失败（bip62规则1）
    SCRIPT_VERIFY_DERSIG    = (1U << 2),

//向checksig操作传递非严格的der签名或具有s>or der/2的der签名会导致脚本失败
//（BIP62规则5）。
    SCRIPT_VERIFY_LOW_S     = (1U << 3),

//验证checkmultisig使用的虚拟堆栈项是否为零长度（bip62规则7）。
    SCRIPT_VERIFY_NULLDUMMY = (1U << 4),

//在scriptsig中使用非push运算符会导致脚本失败（bip62规则2）。
    SCRIPT_VERIFY_SIGPUSHONLY = (1U << 5),

//所有推送操作需要最少的编码（操作0…Op_16，Op_1在可能的情况下否定，直接
//向上推75个字节，向上推255个字节，向上推2个字节。评价
//任何其他推送都会导致脚本失败（bip62规则3）。
//此外，每当将堆栈元素解释为数字时，它的长度必须是最小的（bip62规则4）。
    SCRIPT_VERIFY_MINIMALDATA = (1U << 6),

//不鼓励使用为升级保留的nop（nop1-10）
//
//这样节点就可以避免接受或挖掘事务
//包含已执行的nop，其含义可能在软分叉后改变，
//从而使脚本无效；使用此标志集执行
//不鼓励的nops使脚本失败。此验证标志将永远不会
//应用于块中脚本的强制标志。不，那不是
//例如，在未执行的if endif块内执行，将被*不*拒绝。
//具有相关分叉以赋予它们新含义的nop（cltv、csv）
//不受此规则约束。
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS  = (1U << 7),

//要求在计算后只保留一个堆栈元素。这改变了成功标准
//“必须至少保留一个堆栈元素，并且当解释为布尔值时，它必须为真”to“
//“只有一个堆栈元素必须保留，当解释为布尔值时，它必须为真”。
//（BIP62规则6）
//注意：在没有p2sh或见证的情况下，不得使用cleanstack。
    SCRIPT_VERIFY_CLEANSTACK = (1U << 8),

//验证checklocktimeverify
//
//详见BIP65。
    SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY = (1U << 9),

//支持checkSequenceVerify操作码
//
//详见BIP112
    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY = (1U << 10),

//支持隔离证人
//
    SCRIPT_VERIFY_WITNESS = (1U << 11),

//使v1-v16见证程序非标准化
//
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM = (1U << 12),

//Segwit脚本：要求op_if/notif的参数正好是0x01或空向量
//
    SCRIPT_VERIFY_MINIMALIF = (1U << 13),

//如果检查（多）SIG操作失败，则签名必须为空矢量
//
    SCRIPT_VERIFY_NULLFAIL = (1U << 14),

//必须压缩隔离见证脚本中的公钥
//
    SCRIPT_VERIFY_WITNESS_PUBKEYTYPE = (1U << 15),

//使操作代码分隔符和find和delete失败任何非segwit脚本
//
    SCRIPT_VERIFY_CONST_SCRIPTCODE = (1U << 16),
};

bool CheckSignatureEncoding(const std::vector<unsigned char> &vchSig, unsigned int flags, ScriptError* serror);

struct PrecomputedTransactionData
{
    uint256 hashPrevouts, hashSequence, hashOutputs;
    bool ready = false;

    template <class T>
    explicit PrecomputedTransactionData(const T& tx);
};

enum class SigVersion
{
    BASE = 0,
    WITNESS_V0 = 1,
};

/*签名哈希大小*/
static constexpr size_t WITNESS_V0_SCRIPTHASH_SIZE = 32;
static constexpr size_t WITNESS_V0_KEYHASH_SIZE = 20;

template <class T>
uint256 SignatureHash(const CScript& scriptCode, const T& txTo, unsigned int nIn, int nHashType, const CAmount& amount, SigVersion sigversion, const PrecomputedTransactionData* cache = nullptr);

class BaseSignatureChecker
{
public:
    virtual bool CheckSig(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const
    {
        return false;
    }

    virtual bool CheckLockTime(const CScriptNum& nLockTime) const
    {
         return false;
    }

    virtual bool CheckSequence(const CScriptNum& nSequence) const
    {
         return false;
    }

    virtual ~BaseSignatureChecker() {}
};

template <class T>
class GenericTransactionSignatureChecker : public BaseSignatureChecker
{
private:
    const T* txTo;
    unsigned int nIn;
    const CAmount amount;
    const PrecomputedTransactionData* txdata;

protected:
    virtual bool VerifySignature(const std::vector<unsigned char>& vchSig, const CPubKey& vchPubKey, const uint256& sighash) const;

public:
    GenericTransactionSignatureChecker(const T* txToIn, unsigned int nInIn, const CAmount& amountIn) : txTo(txToIn), nIn(nInIn), amount(amountIn), txdata(nullptr) {}
    GenericTransactionSignatureChecker(const T* txToIn, unsigned int nInIn, const CAmount& amountIn, const PrecomputedTransactionData& txdataIn) : txTo(txToIn), nIn(nInIn), amount(amountIn), txdata(&txdataIn) {}
    bool CheckSig(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const override;
    bool CheckLockTime(const CScriptNum& nLockTime) const override;
    bool CheckSequence(const CScriptNum& nSequence) const override;
};

using TransactionSignatureChecker = GenericTransactionSignatureChecker<CTransaction>;
using MutableTransactionSignatureChecker = GenericTransactionSignatureChecker<CMutableTransaction>;

bool EvalScript(std::vector<std::vector<unsigned char> >& stack, const CScript& script, unsigned int flags, const BaseSignatureChecker& checker, SigVersion sigversion, ScriptError* error = nullptr);
bool VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness* witness, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* serror = nullptr);

size_t CountWitnessSigOps(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness* witness, unsigned int flags);

int FindAndDelete(CScript& script, const CScript& b);

#endif //比特币脚本解释程序
