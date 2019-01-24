
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

#include <script/interpreter.h>

#include <crypto/ripemd160.h>
#include <crypto/sha1.h>
#include <crypto/sha256.h>
#include <pubkey.h>
#include <script/script.h>
#include <uint256.h>

typedef std::vector<unsigned char> valtype;

namespace {

inline bool set_success(ScriptError* ret)
{
    if (ret)
        *ret = SCRIPT_ERR_OK;
    return true;
}

inline bool set_error(ScriptError* ret, const ScriptError serror)
{
    if (ret)
        *ret = serror;
    return false;
}

} //命名空间

bool CastToBool(const valtype& vch)
{
    for (unsigned int i = 0; i < vch.size(); i++)
    {
        if (vch[i] != 0)
        {
//可以是负零
            if (i == vch.size()-1 && vch[i] == 0x80)
                return false;
            return true;
        }
    }
    return false;
}

/*
 *脚本是计算谓词的堆栈机器（如forth）。
 *返回一个指示有效与否的bool。没有回路。
 **/

#define stacktop(i)  (stack.at(stack.size()+(i)))
#define altstacktop(i)  (altstack.at(altstack.size()+(i)))
static inline void popstack(std::vector<valtype>& stack)
{
    if (stack.empty())
        throw std::runtime_error("popstack(): stack empty");
    stack.pop_back();
}

bool static IsCompressedOrUncompressedPubKey(const valtype &vchPubKey) {
    if (vchPubKey.size() < CPubKey::COMPRESSED_PUBLIC_KEY_SIZE) {
//非规范公钥：太短
        return false;
    }
    if (vchPubKey[0] == 0x04) {
        if (vchPubKey.size() != CPubKey::PUBLIC_KEY_SIZE) {
//非规范公钥：未压缩密钥的长度无效
            return false;
        }
    } else if (vchPubKey[0] == 0x02 || vchPubKey[0] == 0x03) {
        if (vchPubKey.size() != CPubKey::COMPRESSED_PUBLIC_KEY_SIZE) {
//非规范公钥：压缩密钥的长度无效
            return false;
        }
    } else {
//非规范公钥：既不是压缩的也不是未压缩的
        return false;
    }
    return true;
}

bool static IsCompressedPubKey(const valtype &vchPubKey) {
    if (vchPubKey.size() != CPubKey::COMPRESSED_PUBLIC_KEY_SIZE) {
//非规范公钥：压缩密钥的长度无效
        return false;
    }
    if (vchPubKey[0] != 0x02 && vchPubKey[0] != 0x03) {
//非规范公钥：压缩密钥的前缀无效
        return false;
    }
    return true;
}

/*
 *存在规范签名：<30><total len><02><len r><r><02><len s><s><hashtype>
 *其中r和s不是负的（它们的第一个字节没有设置其最高位），而不是
 *过度填充（除非后面有负数，否则不要以0字节开头，
 *在这种情况下，一个0字节是必需的，甚至是必需的）。
 *
 *参见https://bitcointalk.org/index.php？主题=8392.MSG127623 MSG127623
 *
 *自BIP66以来，该功能是共识关键。
 **/

bool static IsValidSignatureEncoding(const std::vector<unsigned char> &sig) {
//格式：0x30[总长度]0x02[R-长度][R]0x02[S-长度][S][Sighash]
//*总长度：后面所有内容的1字节长度描述符，
//不包括sighash字节。
//*r-length：后面的r值的1字节长度描述符。
//*r：任意长度的big endian编码的r值。必须用最短的
//正整数的可能编码（这意味着在
//开始，但当下一个字节设置了其最高位时只有一个）。
//*s-length：后面的s值的1字节长度描述符。
//*s：任意长度的big endian编码的s值。同样的规则也适用。
//*sighash：1字节的值，指示哪些数据是散列的（不是der的一部分）
//签名）

//最小和最大大小约束。
    if (sig.size() < 9) return false;
    if (sig.size() > 73) return false;

//签名的类型为0x30（复合）。
    if (sig[0] != 0x30) return false;

//确保长度覆盖整个签名。
    if (sig[1] != sig.size() - 3) return false;

//提取r元素的长度。
    unsigned int lenR = sig[3];

//确保s元素的长度仍在签名中。
    if (5 + lenR >= sig.size()) return false;

//提取s元素的长度。
    unsigned int lenS = sig[5 + lenR];

//验证签名的长度是否与长度之和匹配
//元素。
    if ((size_t)(lenR + lenS + 7) != sig.size()) return false;

//检查r元素是否为整数。
    if (sig[2] != 0x02) return false;

//r不允许使用零长度整数。
    if (lenR == 0) return false;

//R不允许使用负数。
    if (sig[4] & 0x80) return false;

//不允许R开头的空字节，除非R
//否则将被解释为负数。
    if (lenR > 1 && (sig[4] == 0x00) && !(sig[5] & 0x80)) return false;

//检查s元素是否为整数。
    if (sig[lenR + 4] != 0x02) return false;

//s不允许使用零长度整数。
    if (lenS == 0) return false;

//s不允许使用负数。
    if (sig[lenR + 6] & 0x80) return false;

//不允许在s开头使用空字节，除非s
//解释为负数。
    if (lenS > 1 && (sig[lenR + 6] == 0x00) && !(sig[lenR + 7] & 0x80)) return false;

    return true;
}

bool static IsLowDERSignature(const valtype &vchSig, ScriptError* serror) {
    if (!IsValidSignatureEncoding(vchSig)) {
        return set_error(serror, SCRIPT_ERR_SIG_DER);
    }
//https://bitcoin.stackexchange.com/a/12556：
//还要注意，在事务签名中，一个额外的hashtype字节
//遵循实际签名数据。
    std::vector<unsigned char> vchSigCopy(vchSig.begin(), vchSig.begin() + vchSig.size() - 1);
//如果S值高于曲线除以2的顺序，则其
//补码模可以用顺序代替，即
//正确编码时短一个字节。
    if (!CPubKey::CheckLowS(vchSigCopy)) {
        return set_error(serror, SCRIPT_ERR_SIG_HIGH_S);
    }
    return true;
}

bool static IsDefinedHashtypeSignature(const valtype &vchSig) {
    if (vchSig.size() == 0) {
        return false;
    }
    unsigned char nHashType = vchSig[vchSig.size() - 1] & (~(SIGHASH_ANYONECANPAY));
    if (nHashType < SIGHASH_ALL || nHashType > SIGHASH_SINGLE)
        return false;

    return true;
}

bool CheckSignatureEncoding(const std::vector<unsigned char> &vchSig, unsigned int flags, ScriptError* serror) {
//空签名。不严格按顺序编码，但允许提供
//提供用于检查（多）SIG的无效签名的简洁方法
    if (vchSig.size() == 0) {
        return true;
    }
    if ((flags & (SCRIPT_VERIFY_DERSIG | SCRIPT_VERIFY_LOW_S | SCRIPT_VERIFY_STRICTENC)) != 0 && !IsValidSignatureEncoding(vchSig)) {
        return set_error(serror, SCRIPT_ERR_SIG_DER);
    } else if ((flags & SCRIPT_VERIFY_LOW_S) != 0 && !IsLowDERSignature(vchSig, serror)) {
//瑟罗被设定
        return false;
    } else if ((flags & SCRIPT_VERIFY_STRICTENC) != 0 && !IsDefinedHashtypeSignature(vchSig)) {
        return set_error(serror, SCRIPT_ERR_SIG_HASHTYPE);
    }
    return true;
}

bool static CheckPubKeyEncoding(const valtype &vchPubKey, unsigned int flags, const SigVersion &sigversion, ScriptError* serror) {
    if ((flags & SCRIPT_VERIFY_STRICTENC) != 0 && !IsCompressedOrUncompressedPubKey(vchPubKey)) {
        return set_error(serror, SCRIPT_ERR_PUBKEYTYPE);
    }
//Segwit中只接受压缩密钥
    if ((flags & SCRIPT_VERIFY_WITNESS_PUBKEYTYPE) != 0 && sigversion == SigVersion::WITNESS_V0 && !IsCompressedPubKey(vchPubKey)) {
        return set_error(serror, SCRIPT_ERR_WITNESS_PUBKEYTYPE);
    }
    return true;
}

bool static CheckMinimalPush(const valtype& data, opcodetype opcode) {
//排除op1negate，op_1-16，因为根据定义它们是最小的
    assert(0 <= opcode && opcode <= OP_PUSHDATA4);
    if (data.size() == 0) {
//应该使用op_0。
        return opcode == OP_0;
    } else if (data.size() == 1 && data[0] >= 1 && data[0] <= 16) {
//应该使用操作1。op16
        return false;
    } else if (data.size() == 1 && data[0] == 0x81) {
//应该使用op 1negate。
        return false;
    } else if (data.size() <= 75) {
//必须使用直接推送（操作码指示推送的字节数+这些字节）。
        return opcode == data.size();
    } else if (data.size() <= 255) {
//必须使用了op pushdata。
        return opcode == OP_PUSHDATA1;
    } else if (data.size() <= 65535) {
//必须使用了op pushdata2。
        return opcode == OP_PUSHDATA2;
    }
    return true;
}

int FindAndDelete(CScript& script, const CScript& b)
{
    int nFound = 0;
    if (b.empty())
        return nFound;
    CScript result;
    CScript::const_iterator pc = script.begin(), pc2 = script.begin(), end = script.end();
    opcodetype opcode;
    do
    {
        result.insert(result.end(), pc2, pc);
        while (static_cast<size_t>(end - pc) >= b.size() && std::equal(b.begin(), b.end(), pc))
        {
            pc = pc + b.size();
            ++nFound;
        }
        pc2 = pc;
    }
    while (script.GetOp(pc, opcode));

    if (nFound > 0) {
        result.insert(result.end(), pc2, end);
        script = std::move(result);
    }

    return nFound;
}

bool EvalScript(std::vector<std::vector<unsigned char> >& stack, const CScript& script, unsigned int flags, const BaseSignatureChecker& checker, SigVersion sigversion, ScriptError* serror)
{
    static const CScriptNum bnZero(0);
    static const CScriptNum bnOne(1);
//静态常量cscriptnum bnfass（0）；
//静态常量cscriptnum bntrue（1）；
    static const valtype vchFalse(0);
//静态常数vchzero（0）；
    static const valtype vchTrue(1, 1);

    CScript::const_iterator pc = script.begin();
    CScript::const_iterator pend = script.end();
    CScript::const_iterator pbegincodehash = script.begin();
    opcodetype opcode;
    valtype vchPushValue;
    std::vector<bool> vfExec;
    std::vector<valtype> altstack;
    set_error(serror, SCRIPT_ERR_UNKNOWN_ERROR);
    if (script.size() > MAX_SCRIPT_SIZE)
        return set_error(serror, SCRIPT_ERR_SCRIPT_SIZE);
    int nOpCount = 0;
    bool fRequireMinimal = (flags & SCRIPT_VERIFY_MINIMALDATA) != 0;

    try
    {
        while (pc < pend)
        {
            bool fExec = !count(vfExec.begin(), vfExec.end(), false);

//
//阅读说明
//
            if (!script.GetOp(pc, opcode, vchPushValue))
                return set_error(serror, SCRIPT_ERR_BAD_OPCODE);
            if (vchPushValue.size() > MAX_SCRIPT_ELEMENT_SIZE)
                return set_error(serror, SCRIPT_ERR_PUSH_SIZE);

//请注意，op_reserved不计入操作码限制。
            if (opcode > OP_16 && ++nOpCount > MAX_OPS_PER_SCRIPT)
                return set_error(serror, SCRIPT_ERR_OP_COUNT);

            if (opcode == OP_CAT ||
                opcode == OP_SUBSTR ||
                opcode == OP_LEFT ||
                opcode == OP_RIGHT ||
                opcode == OP_INVERT ||
                opcode == OP_AND ||
                opcode == OP_OR ||
                opcode == OP_XOR ||
                opcode == OP_2MUL ||
                opcode == OP_2DIV ||
                opcode == OP_MUL ||
                opcode == OP_DIV ||
                opcode == OP_MOD ||
                opcode == OP_LSHIFT ||
                opcode == OP_RSHIFT)
return set_error(serror, SCRIPT_ERR_DISABLED_OPCODE); //已禁用操作码。

//使用script_verify_const_scriptcode，即使在未执行的分支中，也会拒绝非segwit脚本中的op_codeseparator
            if (opcode == OP_CODESEPARATOR && sigversion == SigVersion::BASE && (flags & SCRIPT_VERIFY_CONST_SCRIPTCODE))
                return set_error(serror, SCRIPT_ERR_OP_CODESEPARATOR);

            if (fExec && 0 <= opcode && opcode <= OP_PUSHDATA4) {
                if (fRequireMinimal && !CheckMinimalPush(vchPushValue, opcode)) {
                    return set_error(serror, SCRIPT_ERR_MINIMALDATA);
                }
                stack.push_back(vchPushValue);
            } else if (fExec || (OP_IF <= opcode && opcode <= OP_ENDIF))
            switch (opcode)
            {
//
//推值
//
                case OP_1NEGATE:
                case OP_1:
                case OP_2:
                case OP_3:
                case OP_4:
                case OP_5:
                case OP_6:
                case OP_7:
                case OP_8:
                case OP_9:
                case OP_10:
                case OP_11:
                case OP_12:
                case OP_13:
                case OP_14:
                case OP_15:
                case OP_16:
                {
//（值）
                    CScriptNum bn((int)opcode - (int)(OP_1 - 1));
                    stack.push_back(bn.getvch());
//这些操作码的结果应该总是推送数据的最小方式。
//他们会推，所以不需要在这里进行检查。
                }
                break;


//
//控制
//
                case OP_NOP:
                    break;

                case OP_CHECKLOCKTIMEVERIFY:
                {
                    if (!(flags & SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY)) {
//未启用；视为nop2
                        break;
                    }

                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);

//请注意，其他地方的数字操作码仅限于
//操作数在-2**31+1到2**31-1范围内，但是它是
//允许操作码产生超过该值的结果
//范围。这个限制是由CScriptNum实现的
//默认4字节限制。
//
//如果我们保持这个限度，2038年的问题就会出现，
//即使事务中的nlockTime字段
//它们本身就是uint32，只是变得毫无意义
//2106年之后。
//
//因此，作为一种特殊情况，我们告诉CScriptnum接受
//到5字节的bignums，这在2**39-1之前是很好的。
//超过nLockTime字段本身的2**32-1限制。
                    const CScriptNum nLockTime(stacktop(-1), fRequireMinimal, 5);

//在极少数情况下，由于
//先做一些算术，你总是可以用
//0最大检查锁定时间验证。
                    if (nLockTime < 0)
                        return set_error(serror, SCRIPT_ERR_NEGATIVE_LOCKTIME);

//实际比较指定的锁定时间与事务。
                    if (!checker.CheckLockTime(nLockTime))
                        return set_error(serror, SCRIPT_ERR_UNSATISFIED_LOCKTIME);

                    break;
                }

                case OP_CHECKSEQUENCEVERIFY:
                {
                    if (!(flags & SCRIPT_VERIFY_CHECKSEQUENCEVERIFY)) {
//未启用；视为NOP3
                        break;
                    }

                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);

//nsequence和nlocktime一样，是一个32位无符号整数。
//字段。请参阅checklocktimeverify中有关
//5字节数字操作数。
                    const CScriptNum nSequence(stacktop(-1), fRequireMinimal, 5);

//在极少数情况下，由于
//先做一些算术，你总是可以用
//0最大检查序列验证。
                    if (nSequence < 0)
                        return set_error(serror, SCRIPT_ERR_NEGATIVE_LOCKTIME);

//如果
//操作数设置了禁用的锁定时间标志，
//checkSequenceVerify的行为与nop相同。
                    if ((nSequence & CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG) != 0)
                        break;

//将指定的序列号与输入进行比较。
                    if (!checker.CheckSequence(nSequence))
                        return set_error(serror, SCRIPT_ERR_UNSATISFIED_LOCKTIME);

                    break;
                }

                case OP_NOP1: case OP_NOP4: case OP_NOP5:
                case OP_NOP6: case OP_NOP7: case OP_NOP8: case OP_NOP9: case OP_NOP10:
                {
                    if (flags & SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS)
                        return set_error(serror, SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS);
                }
                break;

                case OP_IF:
                case OP_NOTIF:
                {
//<expression>if[语句][其他[语句]]endif
                    bool fValue = false;
                    if (fExec)
                    {
                        if (stack.size() < 1)
                            return set_error(serror, SCRIPT_ERR_UNBALANCED_CONDITIONAL);
                        valtype& vch = stacktop(-1);
                        if (sigversion == SigVersion::WITNESS_V0 && (flags & SCRIPT_VERIFY_MINIMALIF)) {
                            if (vch.size() > 1)
                                return set_error(serror, SCRIPT_ERR_MINIMALIF);
                            if (vch.size() == 1 && vch[0] != 1)
                                return set_error(serror, SCRIPT_ERR_MINIMALIF);
                        }
                        fValue = CastToBool(vch);
                        if (opcode == OP_NOTIF)
                            fValue = !fValue;
                        popstack(stack);
                    }
                    vfExec.push_back(fValue);
                }
                break;

                case OP_ELSE:
                {
                    if (vfExec.empty())
                        return set_error(serror, SCRIPT_ERR_UNBALANCED_CONDITIONAL);
                    vfExec.back() = !vfExec.back();
                }
                break;

                case OP_ENDIF:
                {
                    if (vfExec.empty())
                        return set_error(serror, SCRIPT_ERR_UNBALANCED_CONDITIONAL);
                    vfExec.pop_back();
                }
                break;

                case OP_VERIFY:
                {
//（真）
//（假--假）然后返回
                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    bool fValue = CastToBool(stacktop(-1));
                    if (fValue)
                        popstack(stack);
                    else
                        return set_error(serror, SCRIPT_ERR_VERIFY);
                }
                break;

                case OP_RETURN:
                {
                    return set_error(serror, SCRIPT_ERR_OP_RETURN);
                }
                break;


//
//堆栈操作系统
//
                case OP_TOALTSTACK:
                {
                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    altstack.push_back(stacktop(-1));
                    popstack(stack);
                }
                break;

                case OP_FROMALTSTACK:
                {
                    if (altstack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_ALTSTACK_OPERATION);
                    stack.push_back(altstacktop(-1));
                    popstack(altstack);
                }
                break;

                case OP_2DROP:
                {
//（x1 x2）
                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    popstack(stack);
                    popstack(stack);
                }
                break;

                case OP_2DUP:
                {
//（x1 x2--x1 x2 x1 x2）
                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch1 = stacktop(-2);
                    valtype vch2 = stacktop(-1);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                }
                break;

                case OP_3DUP:
                {
//（x1 x2 x3--x1 x2 x3 x1 x2 x3）
                    if (stack.size() < 3)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch1 = stacktop(-3);
                    valtype vch2 = stacktop(-2);
                    valtype vch3 = stacktop(-1);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                    stack.push_back(vch3);
                }
                break;

                case OP_2OVER:
                {
//（x1 x2 x3 x4--x1 x2 x3 x4 x1 x2）
                    if (stack.size() < 4)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch1 = stacktop(-4);
                    valtype vch2 = stacktop(-3);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                }
                break;

                case OP_2ROT:
                {
//（x1 x2 x3 x4 x5 x6--x3 x4 x5 x6 x1 x2）
                    if (stack.size() < 6)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch1 = stacktop(-6);
                    valtype vch2 = stacktop(-5);
                    stack.erase(stack.end()-6, stack.end()-4);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                }
                break;

                case OP_2SWAP:
                {
//（x1 x2 x3 x4--x3 x4 x1 x2）
                    if (stack.size() < 4)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    swap(stacktop(-4), stacktop(-2));
                    swap(stacktop(-3), stacktop(-1));
                }
                break;

                case OP_IFDUP:
                {
//
                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch = stacktop(-1);
                    if (CastToBool(vch))
                        stack.push_back(vch);
                }
                break;

                case OP_DEPTH:
                {
//--堆栈大小
                    CScriptNum bn(stack.size());
                    stack.push_back(bn.getvch());
                }
                break;

                case OP_DROP:
                {
//（X）
                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    popstack(stack);
                }
                break;

                case OP_DUP:
                {
//（x-xx）
                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch = stacktop(-1);
                    stack.push_back(vch);
                }
                break;

                case OP_NIP:
                {
//（x1 x2-x2）
                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    stack.erase(stack.end() - 2);
                }
                break;

                case OP_OVER:
                {
//（x1 x2--x1 x2 x1）
                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch = stacktop(-2);
                    stack.push_back(vch);
                }
                break;

                case OP_PICK:
                case OP_ROLL:
                {
//（XN…x2 x1 x0 n-xn…X2 x1 x0×n）
//（XN…x2 x1 x0 N-…X2 x1 x0×n）
                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    int n = CScriptNum(stacktop(-1), fRequireMinimal).getint();
                    popstack(stack);
                    if (n < 0 || n >= (int)stack.size())
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch = stacktop(-n-1);
                    if (opcode == OP_ROLL)
                        stack.erase(stack.end()-n-1);
                    stack.push_back(vch);
                }
                break;

                case OP_ROT:
                {
//（x1 x2 x3--x2 x3 x1）
//第一次交换后的x2 x1 x3
//第二次交换后的x2 x3 x1
                    if (stack.size() < 3)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    swap(stacktop(-3), stacktop(-2));
                    swap(stacktop(-2), stacktop(-1));
                }
                break;

                case OP_SWAP:
                {
//（x1 x2--x2 x1）
                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    swap(stacktop(-2), stacktop(-1));
                }
                break;

                case OP_TUCK:
                {
//（x1 x2--x2 x1 x2）
                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype vch = stacktop(-1);
                    stack.insert(stack.end()-2, vch);
                }
                break;


                case OP_SIZE:
                {
//（尺寸）
                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    CScriptNum bn(stacktop(-1).size());
                    stack.push_back(bn.getvch());
                }
                break;


//
//Bitwise逻辑
//
                case OP_EQUAL:
                case OP_EQUALVERIFY:
//case op_notequal://use op_numnotequal
                {
//（x1 x2-BOOL）
                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype& vch1 = stacktop(-2);
                    valtype& vch2 = stacktop(-1);
                    bool fEqual = (vch1 == vch2);
//因为说起来太容易了，所以op notequal被禁用了。
//有点像N！=1，让一些聪明人用额外的
//后面零字节（数字上，0x01==0x001==0x000001）
//如果（opcode==op_notequal）
//费奎尔=！费夸尔；
                    popstack(stack);
                    popstack(stack);
                    stack.push_back(fEqual ? vchTrue : vchFalse);
                    if (opcode == OP_EQUALVERIFY)
                    {
                        if (fEqual)
                            popstack(stack);
                        else
                            return set_error(serror, SCRIPT_ERR_EQUALVERIFY);
                    }
                }
                break;


//
//数字的
//
                case OP_1ADD:
                case OP_1SUB:
                case OP_NEGATE:
                case OP_ABS:
                case OP_NOT:
                case OP_0NOTEQUAL:
                {
//（输入输出）
                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    CScriptNum bn(stacktop(-1), fRequireMinimal);
                    switch (opcode)
                    {
                    case OP_1ADD:       bn += bnOne; break;
                    case OP_1SUB:       bn -= bnOne; break;
                    case OP_NEGATE:     bn = -bn; break;
                    case OP_ABS:        if (bn < bnZero) bn = -bn; break;
                    case OP_NOT:        bn = (bn == bnZero); break;
                    case OP_0NOTEQUAL:  bn = (bn != bnZero); break;
                    default:            assert(!"invalid opcode"); break;
                    }
                    popstack(stack);
                    stack.push_back(bn.getvch());
                }
                break;

                case OP_ADD:
                case OP_SUB:
                case OP_BOOLAND:
                case OP_BOOLOR:
                case OP_NUMEQUAL:
                case OP_NUMEQUALVERIFY:
                case OP_NUMNOTEQUAL:
                case OP_LESSTHAN:
                case OP_GREATERTHAN:
                case OP_LESSTHANOREQUAL:
                case OP_GREATERTHANOREQUAL:
                case OP_MIN:
                case OP_MAX:
                {
//（x1 x2-out）
                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    CScriptNum bn1(stacktop(-2), fRequireMinimal);
                    CScriptNum bn2(stacktop(-1), fRequireMinimal);
                    CScriptNum bn(0);
                    switch (opcode)
                    {
                    case OP_ADD:
                        bn = bn1 + bn2;
                        break;

                    case OP_SUB:
                        bn = bn1 - bn2;
                        break;

                    case OP_BOOLAND:             bn = (bn1 != bnZero && bn2 != bnZero); break;
                    case OP_BOOLOR:              bn = (bn1 != bnZero || bn2 != bnZero); break;
                    case OP_NUMEQUAL:            bn = (bn1 == bn2); break;
                    case OP_NUMEQUALVERIFY:      bn = (bn1 == bn2); break;
                    case OP_NUMNOTEQUAL:         bn = (bn1 != bn2); break;
                    case OP_LESSTHAN:            bn = (bn1 < bn2); break;
                    case OP_GREATERTHAN:         bn = (bn1 > bn2); break;
                    case OP_LESSTHANOREQUAL:     bn = (bn1 <= bn2); break;
                    case OP_GREATERTHANOREQUAL:  bn = (bn1 >= bn2); break;
                    case OP_MIN:                 bn = (bn1 < bn2 ? bn1 : bn2); break;
                    case OP_MAX:                 bn = (bn1 > bn2 ? bn1 : bn2); break;
                    default:                     assert(!"invalid opcode"); break;
                    }
                    popstack(stack);
                    popstack(stack);
                    stack.push_back(bn.getvch());

                    if (opcode == OP_NUMEQUALVERIFY)
                    {
                        if (CastToBool(stacktop(-1)))
                            popstack(stack);
                        else
                            return set_error(serror, SCRIPT_ERR_NUMEQUALVERIFY);
                    }
                }
                break;

                case OP_WITHIN:
                {
//（X最小最大值--输出）
                    if (stack.size() < 3)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    CScriptNum bn1(stacktop(-3), fRequireMinimal);
                    CScriptNum bn2(stacktop(-2), fRequireMinimal);
                    CScriptNum bn3(stacktop(-1), fRequireMinimal);
                    bool fValue = (bn2 <= bn1 && bn1 < bn3);
                    popstack(stack);
                    popstack(stack);
                    popstack(stack);
                    stack.push_back(fValue ? vchTrue : vchFalse);
                }
                break;


//
//密码
//
                case OP_RIPEMD160:
                case OP_SHA1:
                case OP_SHA256:
                case OP_HASH160:
                case OP_HASH256:
                {
//（in -哈希）
                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    valtype& vch = stacktop(-1);
                    valtype vchHash((opcode == OP_RIPEMD160 || opcode == OP_SHA1 || opcode == OP_HASH160) ? 20 : 32);
                    if (opcode == OP_RIPEMD160)
                        CRIPEMD160().Write(vch.data(), vch.size()).Finalize(vchHash.data());
                    else if (opcode == OP_SHA1)
                        CSHA1().Write(vch.data(), vch.size()).Finalize(vchHash.data());
                    else if (opcode == OP_SHA256)
                        CSHA256().Write(vch.data(), vch.size()).Finalize(vchHash.data());
                    else if (opcode == OP_HASH160)
                        CHash160().Write(vch.data(), vch.size()).Finalize(vchHash.data());
                    else if (opcode == OP_HASH256)
                        CHash256().Write(vch.data(), vch.size()).Finalize(vchHash.data());
                    popstack(stack);
                    stack.push_back(vchHash);
                }
                break;

                case OP_CODESEPARATOR:
                {
//如果设置了script_verify_const_scriptcode标志，则在Segwit之前将拒绝使用op_codeseparator。
//脚本，即使在未执行的分支中（在opcode case语句上方选中）。

//哈希从代码分隔符后开始
                    pbegincodehash = pc;
                }
                break;

                case OP_CHECKSIG:
                case OP_CHECKSIGVERIFY:
                {
//（sig pubkey--bool）
                    if (stack.size() < 2)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);

                    valtype& vchSig    = stacktop(-2);
                    valtype& vchPubKey = stacktop(-1);

//脚本的子集，从最新的代码分隔符开始
                    CScript scriptCode(pbegincodehash, pend);

//删除Segwit前脚本中的签名，但不删除Segwit脚本
                    if (sigversion == SigVersion::BASE) {
                        int found = FindAndDelete(scriptCode, CScript(vchSig));
                        if (found > 0 && (flags & SCRIPT_VERIFY_CONST_SCRIPTCODE))
                            return set_error(serror, SCRIPT_ERR_SIG_FINDANDDELETE);
                    }

                    if (!CheckSignatureEncoding(vchSig, flags, serror) || !CheckPubKeyEncoding(vchPubKey, flags, sigversion, serror)) {
//瑟罗被设定
                        return false;
                    }
                    bool fSuccess = checker.CheckSig(vchSig, vchPubKey, scriptCode, sigversion);

                    if (!fSuccess && (flags & SCRIPT_VERIFY_NULLFAIL) && vchSig.size())
                        return set_error(serror, SCRIPT_ERR_SIG_NULLFAIL);

                    popstack(stack);
                    popstack(stack);
                    stack.push_back(fSuccess ? vchTrue : vchFalse);
                    if (opcode == OP_CHECKSIGVERIFY)
                    {
                        if (fSuccess)
                            popstack(stack);
                        else
                            return set_error(serror, SCRIPT_ERR_CHECKSIGVERIFY);
                    }
                }
                break;

                case OP_CHECKMULTISIG:
                case OP_CHECKMULTISIGVERIFY:
                {
//（[sig…]num_of_signatures[pubkey…]num_of_pubkeys--bool）

                    int i = 1;
                    if ((int)stack.size() < i)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);

                    int nKeysCount = CScriptNum(stacktop(-i), fRequireMinimal).getint();
                    if (nKeysCount < 0 || nKeysCount > MAX_PUBKEYS_PER_MULTISIG)
                        return set_error(serror, SCRIPT_ERR_PUBKEY_COUNT);
                    nOpCount += nKeysCount;
                    if (nOpCount > MAX_OPS_PER_SCRIPT)
                        return set_error(serror, SCRIPT_ERR_OP_COUNT);
                    int ikey = ++i;
//ikey2是堆栈中最后一个非签名项的位置。顶部堆栈项=1。
//
                    int ikey2 = nKeysCount + 2;
                    i += nKeysCount;
                    if ((int)stack.size() < i)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);

                    int nSigsCount = CScriptNum(stacktop(-i), fRequireMinimal).getint();
                    if (nSigsCount < 0 || nSigsCount > nKeysCount)
                        return set_error(serror, SCRIPT_ERR_SIG_COUNT);
                    int isig = ++i;
                    i += nSigsCount;
                    if ((int)stack.size() < i)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);

//脚本的子集，从最新的代码分隔符开始
                    CScript scriptCode(pbegincodehash, pend);

//删除Segwit前脚本中的签名，但不删除Segwit脚本
                    for (int k = 0; k < nSigsCount; k++)
                    {
                        valtype& vchSig = stacktop(-isig-k);
                        if (sigversion == SigVersion::BASE) {
                            int found = FindAndDelete(scriptCode, CScript(vchSig));
                            if (found > 0 && (flags & SCRIPT_VERIFY_CONST_SCRIPTCODE))
                                return set_error(serror, SCRIPT_ERR_SIG_FINDANDDELETE);
                        }
                    }

                    bool fSuccess = true;
                    while (fSuccess && nSigsCount > 0)
                    {
                        valtype& vchSig    = stacktop(-isig);
                        valtype& vchPubKey = stacktop(-ikey);

//请注意，这是如何使pubkey/签名评估的确切顺序
//如果设置了strictenc标志，则可通过checkmultisig进行区分。
//有关详细信息，请参阅脚本“有效测试”。
                        if (!CheckSignatureEncoding(vchSig, flags, serror) || !CheckPubKeyEncoding(vchPubKey, flags, sigversion, serror)) {
//瑟罗被设定
                            return false;
                        }

//支票签名
                        bool fOk = checker.CheckSig(vchSig, vchPubKey, scriptCode, sigversion);

                        if (fOk) {
                            isig++;
                            nSigsCount--;
                        }
                        ikey++;
                        nKeysCount--;

//如果留下的签名比留下的密钥多，
//那么失败的签名太多了。早退出，
//不检查任何进一步的签名。
                        if (nSigsCount > nKeysCount)
                            fSuccess = false;
                    }

//清除实际参数堆栈
                    while (i-- > 1) {
//如果操作失败，我们要求所有签名都必须是空向量
                        if (!fSuccess && (flags & SCRIPT_VERIFY_NULLFAIL) && !ikey2 && stacktop(-1).size())
                            return set_error(serror, SCRIPT_ERR_SIG_NULLFAIL);
                        if (ikey2 > 0)
                            ikey2--;
                        popstack(stack);
                    }

//错误导致checkmultisig使用一个额外参数
//其内容未经任何检查。
//
//不幸的是，这是变异性的一个潜在来源，
//所以可以选择验证它是否完全等于零先验
//将其从堆栈中移除。
                    if (stack.size() < 1)
                        return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
                    if ((flags & SCRIPT_VERIFY_NULLDUMMY) && stacktop(-1).size())
                        return set_error(serror, SCRIPT_ERR_SIG_NULLDUMMY);
                    popstack(stack);

                    stack.push_back(fSuccess ? vchTrue : vchFalse);

                    if (opcode == OP_CHECKMULTISIGVERIFY)
                    {
                        if (fSuccess)
                            popstack(stack);
                        else
                            return set_error(serror, SCRIPT_ERR_CHECKMULTISIGVERIFY);
                    }
                }
                break;

                default:
                    return set_error(serror, SCRIPT_ERR_BAD_OPCODE);
            }

//尺寸限制
            if (stack.size() + altstack.size() > MAX_STACK_SIZE)
                return set_error(serror, SCRIPT_ERR_STACK_SIZE);
        }
    }
    catch (...)
    {
        return set_error(serror, SCRIPT_ERR_UNKNOWN_ERROR);
    }

    if (!vfExec.empty())
        return set_error(serror, SCRIPT_ERR_UNBALANCED_CONDITIONAL);

    return set_success(serror);
}

namespace {

/*
 *像ctransaction一样序列化但经过修改的包装器
 *就地完成的签名哈希需要
 **/

template <class T>
class CTransactionSignatureSerializer
{
private:
const T& txTo;             //！<支出事务的引用（正在序列化的事务）
const CScript& scriptCode; //！<正在使用的输出脚本
const unsigned int nIn;    //！<正在签名的txto的输入索引
const bool fAnyoneCanPay;  //！<hashtype是否设置了sighashanyonecanpay标志
const bool fHashSingle;    //！<哈希类型是否为sighash_single
const bool fHashNone;      //！<哈希类型是否为sighash_none

public:
    CTransactionSignatureSerializer(const T& txToIn, const CScript& scriptCodeIn, unsigned int nInIn, int nHashTypeIn) :
        txTo(txToIn), scriptCode(scriptCodeIn), nIn(nInIn),
        fAnyoneCanPay(!!(nHashTypeIn & SIGHASH_ANYONECANPAY)),
        fHashSingle((nHashTypeIn & 0x1f) == SIGHASH_SINGLE),
        fHashNone((nHashTypeIn & 0x1f) == SIGHASH_NONE) {}

    /*序列化传递的脚本代码，跳过操作代码分隔符*/
    template<typename S>
    void SerializeScriptCode(S &s) const {
        CScript::const_iterator it = scriptCode.begin();
        CScript::const_iterator itBegin = it;
        opcodetype opcode;
        unsigned int nCodeSeparators = 0;
        while (scriptCode.GetOp(it, opcode)) {
            if (opcode == OP_CODESEPARATOR)
                nCodeSeparators++;
        }
        ::WriteCompactSize(s, scriptCode.size() - nCodeSeparators);
        it = itBegin;
        while (scriptCode.GetOp(it, opcode)) {
            if (opcode == OP_CODESEPARATOR) {
                s.write((char*)&itBegin[0], it-itBegin-1);
                itBegin = it;
            }
        }
        if (itBegin != scriptCode.end())
            s.write((char*)&itBegin[0], it-itBegin);
    }

    /*序列化txto的输入*/
    template<typename S>
    void SerializeInput(S &s, unsigned int nInput) const {
//如果是sighashou anyonecanpay，则只序列化正在签名的输入
        if (fAnyoneCanPay)
            nInput = nIn;
//序列化预览
        ::Serialize(s, txTo.vin[nInput].prevout);
//序列化脚本
        if (nInput != nIn)
//删除其他输入的签名
            ::Serialize(s, CScript());
        else
            SerializeScriptCode(s);
//序列化N序列
        if (nInput != nIn && (fHashSingle || fHashNone))
//让其他人随意更新
            ::Serialize(s, (int)0);
        else
            ::Serialize(s, txTo.vin[nInput].nSequence);
    }

    /*序列化txto的输出*/
    template<typename S>
    void SerializeOutput(S &s, unsigned int nOutput) const {
        if (fHashSingle && nOutput != nIn)
//不要将txout收款人锁定在其他指数作为txin
            ::Serialize(s, CTxOut());
        else
            ::Serialize(s, txTo.vout[nOutput]);
    }

    /*序列化TXTO*/
    template<typename S>
    void Serialize(S &s) const {
//序列化转换
        ::Serialize(s, txTo.nVersion);
//串行化VIN
        unsigned int nInputs = fAnyoneCanPay ? 1 : txTo.vin.size();
        ::WriteCompactSize(s, nInputs);
        for (unsigned int nInput = 0; nInput < nInputs; nInput++)
             SerializeInput(s, nInput);
//序列化VOUT
        unsigned int nOutputs = fHashNone ? 0 : (fHashSingle ? nIn+1 : txTo.vout.size());
        ::WriteCompactSize(s, nOutputs);
        for (unsigned int nOutput = 0; nOutput < nOutputs; nOutput++)
             SerializeOutput(s, nOutput);
//序列化nlocktime
        ::Serialize(s, txTo.nLockTime);
    }
};

template <class T>
uint256 GetPrevoutHash(const T& txTo)
{
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto& txin : txTo.vin) {
        ss << txin.prevout;
    }
    return ss.GetHash();
}

template <class T>
uint256 GetSequenceHash(const T& txTo)
{
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto& txin : txTo.vin) {
        ss << txin.nSequence;
    }
    return ss.GetHash();
}

template <class T>
uint256 GetOutputsHash(const T& txTo)
{
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto& txout : txTo.vout) {
        ss << txout;
    }
    return ss.GetHash();
}

} //命名空间

template <class T>
PrecomputedTransactionData::PrecomputedTransactionData(const T& txTo)
{
//仅为具有见证的事务计算缓存
    if (txTo.HasWitness()) {
        hashPrevouts = GetPrevoutHash(txTo);
        hashSequence = GetSequenceHash(txTo);
        hashOutputs = GetOutputsHash(txTo);
        ready = true;
    }
}

//显式实例化
template PrecomputedTransactionData::PrecomputedTransactionData(const CTransaction& txTo);
template PrecomputedTransactionData::PrecomputedTransactionData(const CMutableTransaction& txTo);

template <class T>
uint256 SignatureHash(const CScript& scriptCode, const T& txTo, unsigned int nIn, int nHashType, const CAmount& amount, SigVersion sigversion, const PrecomputedTransactionData* cache)
{
    assert(nIn < txTo.vin.size());

    if (sigversion == SigVersion::WITNESS_V0) {
        uint256 hashPrevouts;
        uint256 hashSequence;
        uint256 hashOutputs;
        const bool cacheready = cache && cache->ready;

        if (!(nHashType & SIGHASH_ANYONECANPAY)) {
            hashPrevouts = cacheready ? cache->hashPrevouts : GetPrevoutHash(txTo);
        }

        if (!(nHashType & SIGHASH_ANYONECANPAY) && (nHashType & 0x1f) != SIGHASH_SINGLE && (nHashType & 0x1f) != SIGHASH_NONE) {
            hashSequence = cacheready ? cache->hashSequence : GetSequenceHash(txTo);
        }


        if ((nHashType & 0x1f) != SIGHASH_SINGLE && (nHashType & 0x1f) != SIGHASH_NONE) {
            hashOutputs = cacheready ? cache->hashOutputs : GetOutputsHash(txTo);
        } else if ((nHashType & 0x1f) == SIGHASH_SINGLE && nIn < txTo.vout.size()) {
            CHashWriter ss(SER_GETHASH, 0);
            ss << txTo.vout[nIn];
            hashOutputs = ss.GetHash();
        }

        CHashWriter ss(SER_GETHASH, 0);
//版本
        ss << txTo.nVersion;
//输入预防/序列（无/全部，取决于标志）
        ss << hashPrevouts;
        ss << hashSequence;
//正在签名的输入（用scriptcode+amount替换scriptsig）
//prevout可能已经包含在hashprevout和nsequence中
//可能已包含在哈希序列中。
        ss << txTo.vin[nIn].prevout;
        ss << scriptCode;
        ss << amount;
        ss << txTo.vin[nIn].nSequence;
//输出（无/一/全部，取决于标志）
        ss << hashOutputs;
//锁定时间
        ss << txTo.nLockTime;
//花叶病型
        ss << nHashType;

        return ss.GetHash();
    }

    static const uint256 one(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));

//检查Sighash_Single的无效使用
    if ((nHashType & 0x1f) == SIGHASH_SINGLE) {
        if (nIn >= txTo.vout.size()) {
//没有超出范围
            return one;
        }
    }

//仅序列化正在签名的事务的必要部分的包装
    CTransactionSignatureSerializer<T> txTmp(txTo, scriptCode, nIn, nHashType);

//序列化和哈希
    CHashWriter ss(SER_GETHASH, 0);
    ss << txTmp << nHashType;
    return ss.GetHash();
}

template <class T>
bool GenericTransactionSignatureChecker<T>::VerifySignature(const std::vector<unsigned char>& vchSig, const CPubKey& pubkey, const uint256& sighash) const
{
    return pubkey.Verify(sighash, vchSig);
}

template <class T>
bool GenericTransactionSignatureChecker<T>::CheckSig(const std::vector<unsigned char>& vchSigIn, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const
{
    CPubKey pubkey(vchPubKey);
    if (!pubkey.IsValid())
        return false;

//哈希类型是附加到签名末尾的一个字节
    std::vector<unsigned char> vchSig(vchSigIn);
    if (vchSig.empty())
        return false;
    int nHashType = vchSig.back();
    vchSig.pop_back();

    uint256 sighash = SignatureHash(scriptCode, *txTo, nIn, nHashType, amount, sigversion, this->txdata);

    if (!VerifySignature(vchSig, pubkey, sighash))
        return false;

    return true;
}

template <class T>
bool GenericTransactionSignatureChecker<T>::CheckLockTime(const CScriptNum& nLockTime) const
{
//有两种非锁定时间：按块高度锁定
//并按阻塞时间锁定，以是否
//nlocktime<locktime_阈值。
//
//我们想把苹果和苹果作比较，所以剧本不及格。
//除非正在测试的nlocktime类型与
//事务中的nlockTime。
    if (!(
        (txTo->nLockTime <  LOCKTIME_THRESHOLD && nLockTime <  LOCKTIME_THRESHOLD) ||
        (txTo->nLockTime >= LOCKTIME_THRESHOLD && nLockTime >= LOCKTIME_THRESHOLD)
    ))
        return false;

//既然我们知道我们在比较苹果和苹果，
//比较是一个简单的数字。
    if (nLockTime > (int64_t)txTo->nLockTime)
        return false;

//最后，可以禁用nlocktime功能，从而
//如果每个txin都被
//通过将Nsequence设置为MaxInt完成。这个
//交易将被允许进入区块链，使
//操作码无效。
//
//如果此VIN不是最终版本，则测试足以
//防止这种情况。或者我们可以测试所有
//输入，但仅测试此输入会将数据最小化
//需要证明正确的checklocktimeverify执行。
    if (CTxIn::SEQUENCE_FINAL == txTo->vin[nIn].nSequence)
        return false;

    return true;
}

template <class T>
bool GenericTransactionSignatureChecker<T>::CheckSequence(const CScriptNum& nSequence) const
{
//通过比较传递的
//在操作数中输入的序列号。
    const int64_t txToSequence = (int64_t)txTo->vin[nIn].nSequence;

//如果事务的版本号未设置为高，则失败
//足以触发BIP 68规则。
    if (static_cast<uint32_t>(txTo->nVersion) < 2)
        return false;

//具有最高有效位集的序列号不是
//共识受限。测试交易的顺序
//数字没有此位集，禁止使用此属性
//要绕过checkSequenceVerify检查。
    if (txToSequence & CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG)
        return false;

//屏蔽任何没有共识强制意义的部分
//在进行整数比较之前
    const uint32_t nLockTimeMask = CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG | CTxIn::SEQUENCE_LOCKTIME_MASK;
    const int64_t txToSequenceMasked = txToSequence & nLockTimeMask;
    const CScriptNum nSequenceMasked = nSequence & nLockTimeMask;

//有两种类型的序列：按块高度锁定
//并按阻塞时间锁定，以是否
//nsequenceMasked<ctxin:：sequence_ktime_type_flag.
//
//我们想把苹果和苹果作比较，所以剧本不及格。
//除非正在测试的nsequenceMask类型与
//事务中要求的N序列。
    if (!(
        (txToSequenceMasked <  CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG && nSequenceMasked <  CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG) ||
        (txToSequenceMasked >= CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG && nSequenceMasked >= CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG)
    )) {
        return false;
    }

//既然我们知道我们在比较苹果和苹果，
//比较是一个简单的数字。
    if (nSequenceMasked > txToSequenceMasked)
        return false;

    return true;
}

//显式实例化
template class GenericTransactionSignatureChecker<CTransaction>;
template class GenericTransactionSignatureChecker<CMutableTransaction>;

static bool VerifyWitnessProgram(const CScriptWitness& witness, int witversion, const std::vector<unsigned char>& program, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* serror)
{
    std::vector<std::vector<unsigned char> > stack;
    CScript scriptPubKey;

    if (witversion == 0) {
        if (program.size() == WITNESS_V0_SCRIPTHASH_SIZE) {
//0版隔离见证程序：程序内的sha256（cscript），cscript+见证输入
            if (witness.stack.size() == 0) {
                return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_WITNESS_EMPTY);
            }
            scriptPubKey = CScript(witness.stack.back().begin(), witness.stack.back().end());
            stack = std::vector<std::vector<unsigned char> >(witness.stack.begin(), witness.stack.end() - 1);
            uint256 hashScriptPubKey;
            CSHA256().Write(&scriptPubKey[0], scriptPubKey.size()).Finalize(hashScriptPubKey.begin());
            if (memcmp(hashScriptPubKey.begin(), program.data(), 32)) {
                return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH);
            }
        } else if (program.size() == WITNESS_V0_KEYHASH_SIZE) {
//支付给pubkeyhash的特殊情况；签名+见证pubkey
            if (witness.stack.size() != 2) {
return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH); //2见证项目
            }
            scriptPubKey << OP_DUP << OP_HASH160 << program << OP_EQUALVERIFY << OP_CHECKSIG;
            stack = witness.stack;
        } else {
            return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_WRONG_LENGTH);
        }
    } else if (flags & SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM) {
        return set_error(serror, SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM);
    } else {
//更高版本的见证脚本为将来的SoftFork兼容性返回true
        return set_success(serror);
    }

//不允许堆栈项大小>见证堆栈中的最大脚本元素大小
    for (unsigned int i = 0; i < stack.size(); i++) {
        if (stack.at(i).size() > MAX_SCRIPT_ELEMENT_SIZE)
            return set_error(serror, SCRIPT_ERR_PUSH_SIZE);
    }

    if (!EvalScript(stack, scriptPubKey, flags, checker, SigVersion::WITNESS_V0, serror)) {
        return false;
    }

//见证内的脚本隐式要求cleanstack行为
    if (stack.size() != 1)
        return set_error(serror, SCRIPT_ERR_CLEANSTACK);
    if (!CastToBool(stack.back()))
        return set_error(serror, SCRIPT_ERR_EVAL_FALSE);
    return true;
}

bool VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness* witness, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* serror)
{
    static const CScriptWitness emptyWitness;
    if (witness == nullptr) {
        witness = &emptyWitness;
    }
    bool hadWitness = false;

    set_error(serror, SCRIPT_ERR_UNKNOWN_ERROR);

    if ((flags & SCRIPT_VERIFY_SIGPUSHONLY) != 0 && !scriptSig.IsPushOnly()) {
        return set_error(serror, SCRIPT_ERR_SIG_PUSHONLY);
    }

    std::vector<std::vector<unsigned char> > stack, stackCopy;
    if (!EvalScript(stack, scriptSig, flags, checker, SigVersion::BASE, serror))
//瑟罗被设定
        return false;
    if (flags & SCRIPT_VERIFY_P2SH)
        stackCopy = stack;
    if (!EvalScript(stack, scriptPubKey, flags, checker, SigVersion::BASE, serror))
//瑟罗被设定
        return false;
    if (stack.empty())
        return set_error(serror, SCRIPT_ERR_EVAL_FALSE);
    if (CastToBool(stack.back()) == false)
        return set_error(serror, SCRIPT_ERR_EVAL_FALSE);

//裸证人程序
    int witnessversion;
    std::vector<unsigned char> witnessprogram;
    if (flags & SCRIPT_VERIFY_WITNESS) {
        if (scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
            hadWitness = true;
            if (scriptSig.size() != 0) {
//scriptsig必须是\u exactly\cscript（），否则我们将重新引入延展性。
                return set_error(serror, SCRIPT_ERR_WITNESS_MALLEATED);
            }
            if (!VerifyWitnessProgram(*witness, witnessversion, witnessprogram, flags, checker, serror)) {
                return false;
            }
//在末端绕过清洁堆栈检查。实际的堆栈显然不干净
//见证程序。
            stack.resize(1);
        }
    }

//用于编写哈希事务脚本的额外验证：
    if ((flags & SCRIPT_VERIFY_P2SH) && scriptPubKey.IsPayToScriptHash())
    {
//scriptsig只能是文本，否则验证失败
        if (!scriptSig.IsPushOnly())
            return set_error(serror, SCRIPT_ERR_SIG_PUSHONLY);

//恢复堆栈。
        swap(stack, stackCopy);

//堆栈不能为空，因为如果它是
//p2sh hash<>equal scriptpubkey将用
//空堆栈和上面的evalscript将返回false。
        assert(!stack.empty());

        const valtype& pubKeySerialized = stack.back();
        CScript pubKey2(pubKeySerialized.begin(), pubKeySerialized.end());
        popstack(stack);

        if (!EvalScript(stack, pubKey2, flags, checker, SigVersion::BASE, serror))
//瑟罗被设定
            return false;
        if (stack.empty())
            return set_error(serror, SCRIPT_ERR_EVAL_FALSE);
        if (!CastToBool(stack.back()))
            return set_error(serror, SCRIPT_ERR_EVAL_FALSE);

//P2SH见证计划
        if (flags & SCRIPT_VERIFY_WITNESS) {
            if (pubKey2.IsWitnessProgram(witnessversion, witnessprogram)) {
                hadWitness = true;
                if (scriptSig != CScript() << std::vector<unsigned char>(pubKey2.begin(), pubKey2.end())) {
//scriptsig必须是\完全\一次redeemscript推送。否则我们
//重新引入延展性。
                    return set_error(serror, SCRIPT_ERR_WITNESS_MALLEATED_P2SH);
                }
                if (!VerifyWitnessProgram(*witness, witnessversion, witnessprogram, flags, checker, serror)) {
                    return false;
                }
//在末端绕过清洁堆栈检查。实际的堆栈显然不干净
//见证程序。
                stack.resize(1);
            }
        }
    }

//只有在潜在的p2sh评估之后才执行cleanstack检查。
//由于p2sh脚本的非p2sh评估显然不会导致
//一个干净的堆栈（p2sh输入保持不变）。证人评估同样适用。
    if ((flags & SCRIPT_VERIFY_CLEANSTACK) != 0) {
//不允许没有p2sh的cleanstack，否则是开关cleanstack->p2sh+cleanstack
//可能，这不是一个软叉（p2sh应该是一个）。
        assert((flags & SCRIPT_VERIFY_P2SH) != 0);
        assert((flags & SCRIPT_VERIFY_WITNESS) != 0);
        if (stack.size() != 1) {
            return set_error(serror, SCRIPT_ERR_CLEANSTACK);
        }
    }

    if (flags & SCRIPT_VERIFY_WITNESS) {
//如果p2sh关闭，我们无法检查正确的意外见证数据，因此需要
//该证人暗示p2sh，否则，从“证人”->p2sh+证人
//可能，这不是软叉。
        assert((flags & SCRIPT_VERIFY_P2SH) != 0);
        if (!hadWitness && !witness->IsNull()) {
            return set_error(serror, SCRIPT_ERR_WITNESS_UNEXPECTED);
        }
    }

    return set_success(serror);
}

size_t static WitnessSigOps(int witversion, const std::vector<unsigned char>& witprogram, const CScriptWitness& witness)
{
    if (witversion == 0) {
        if (witprogram.size() == WITNESS_V0_KEYHASH_SIZE)
            return 1;

        if (witprogram.size() == WITNESS_V0_SCRIPTHASH_SIZE && witness.stack.size() > 0) {
            CScript subscript(witness.stack.back().begin(), witness.stack.back().end());
            return subscript.GetSigOpCount(true);
        }
    }

//未来的标志可以在这里实现。
    return 0;
}

size_t CountWitnessSigOps(const CScript& scriptSig, const CScript& scriptPubKey, const CScriptWitness* witness, unsigned int flags)
{
    static const CScriptWitness witnessEmpty;

    if ((flags & SCRIPT_VERIFY_WITNESS) == 0) {
        return 0;
    }
    assert((flags & SCRIPT_VERIFY_P2SH) != 0);

    int witnessversion;
    std::vector<unsigned char> witnessprogram;
    if (scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
        return WitnessSigOps(witnessversion, witnessprogram, witness ? *witness : witnessEmpty);
    }

    if (scriptPubKey.IsPayToScriptHash() && scriptSig.IsPushOnly()) {
        CScript::const_iterator pc = scriptSig.begin();
        std::vector<unsigned char> data;
        while (pc < scriptSig.end()) {
            opcodetype opcode;
            scriptSig.GetOp(pc, opcode, data);
        }
        CScript subscript(data.begin(), data.end());
        if (subscript.IsWitnessProgram(witnessversion, witnessprogram)) {
            return WitnessSigOps(witnessversion, witnessprogram, witness ? *witness : witnessEmpty);
        }
    }

    return 0;
}
