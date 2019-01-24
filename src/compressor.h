
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

#ifndef BITCOIN_COMPRESSOR_H
#define BITCOIN_COMPRESSOR_H

#include <primitives/transaction.h>
#include <script/script.h>
#include <serialize.h>
#include <span.h>

class CKeyID;
class CPubKey;
class CScriptID;

bool CompressScript(const CScript& script, std::vector<unsigned char> &out);
unsigned int GetSpecialScriptSize(unsigned int nSize);
bool DecompressScript(CScript& script, unsigned int nSize, const std::vector<unsigned char> &out);

uint64_t CompressAmount(uint64_t nAmount);
uint64_t DecompressAmount(uint64_t nAmount);

/*脚本的压缩序列化程序。
 *
 *它可以检测常见情况并更有效地对其进行编码。
 *定义了3种特殊情况：
 **支付到PubKey哈希（编码为21字节）
 **付费脚本哈希（编码为21字节）
 **以0x02、0x03或0x04开头的Pay-to-PubKey（编码为33字节）
 *
 *其他高达121字节的脚本需要1字节+脚本长度。上面
 *即，高达16505字节的脚本需要2个字节+脚本长度。
 **/

class CScriptCompressor
{
private:
    /*
     *暂时将其设为静态（只定义了6个特殊脚本）
     *这可能会与新的版本一起扩展
     *交易，在这种情况下，该值取决于转换
     *以及所附交易的所有权。
     **/

    static const unsigned int nSpecialScripts = 6;

    CScript &script;
public:
    explicit CScriptCompressor(CScript &scriptIn) : script(scriptIn) { }

    template<typename Stream>
    void Serialize(Stream &s) const {
        std::vector<unsigned char> compr;
        if (CompressScript(script, compr)) {
            s << MakeSpan(compr);
            return;
        }
        unsigned int nSize = script.size() + nSpecialScripts;
        s << VARINT(nSize);
        s << MakeSpan(script);
    }

    template<typename Stream>
    void Unserialize(Stream &s) {
        unsigned int nSize = 0;
        s >> VARINT(nSize);
        if (nSize < nSpecialScripts) {
            std::vector<unsigned char> vch(GetSpecialScriptSize(nSize), 0x00);
            s >> MakeSpan(vch);
            DecompressScript(script, nSize, vch);
            return;
        }
        nSize -= nSpecialScripts;
        if (nSize > MAX_SCRIPT_SIZE) {
//脚本太长，请替换为短的无效脚本
            script << OP_RETURN;
            s.ignore(nSize);
        } else {
            script.resize(nSize);
            s >> MakeSpan(script);
        }
    }
};

/*提供更紧凑序列化的ctxout的包装器*/
class CTxOutCompressor
{
private:
    CTxOut &txout;

public:
    explicit CTxOutCompressor(CTxOut &txoutIn) : txout(txoutIn) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        if (!ser_action.ForRead()) {
            uint64_t nVal = CompressAmount(txout.nValue);
            READWRITE(VARINT(nVal));
        } else {
            uint64_t nVal = 0;
            READWRITE(VARINT(nVal));
            txout.nValue = DecompressAmount(nVal);
        }
        CScriptCompressor cscript(REF(txout.scriptPubKey));
        READWRITE(cscript);
    }
};

#endif //比特币压缩机
