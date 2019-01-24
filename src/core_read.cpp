
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <core_io.h>

#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/sign.h>
#include <serialize.h>
#include <streams.h>
#include <univalue.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <version.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>

#include <algorithm>

CScript ParseScript(const std::string& s)
{
    CScript result;

    static std::map<std::string, opcodetype> mapOpNames;

    if (mapOpNames.empty())
    {
        for (unsigned int op = 0; op <= MAX_OPCODE; op++)
        {
//允许opu reserved进入mapopnames
            if (op < OP_NOP && op != OP_RESERVED)
                continue;

            const char* name = GetOpName(static_cast<opcodetype>(op));
            if (strcmp(name, "OP_UNKNOWN") == 0)
                continue;
            std::string strName(name);
            mapOpNames[strName] = static_cast<opcodetype>(op);
//方便：Op_Add和Just Add都可以识别：
            boost::algorithm::replace_first(strName, "OP_", "");
            mapOpNames[strName] = static_cast<opcodetype>(op);
        }
    }

    std::vector<std::string> words;
    boost::algorithm::split(words, s, boost::algorithm::is_any_of(" \t\n"), boost::algorithm::token_compress_on);

    for (std::vector<std::string>::const_iterator w = words.begin(); w != words.end(); ++w)
    {
        if (w->empty())
        {
//空字符串，忽略。（boost:：split给定的“”将返回一个单词）
        }
        else if (std::all_of(w->begin(), w->end(), ::IsDigit) ||
            (w->front() == '-' && w->size() > 1 && std::all_of(w->begin()+1, w->end(), ::IsDigit)))
        {
//数
            int64_t n = atoi64(*w);
            result << n;
        }
        else if (w->substr(0,2) == "0x" && w->size() > 2 && IsHex(std::string(w->begin()+2, w->end())))
        {
//原始十六进制数据，未插入堆栈：
            std::vector<unsigned char> raw = ParseHex(std::string(w->begin()+2, w->end()));
            result.insert(result.end(), raw.begin(), raw.end());
        }
        else if (w->size() >= 2 && w->front() == '\'' && w->back() == '\'')
        {
//单引号字符串，作为数据推送。注：这是穷人的
//解析时，单引号字符串中的空格/制表符/换行符将不起作用。
            std::vector<unsigned char> value(w->begin()+1, w->end()-1);
            result << value;
        }
        else if (mapOpNames.count(*w))
        {
//操作码，例如opu-add或add：
            result << mapOpNames[*w];
        }
        else
        {
            throw std::runtime_error("script parse error");
        }
    }

    return result;
}

//检查事务的所有输入和输出脚本是否包含有效的操作码
static bool CheckTxScriptsSanity(const CMutableTransaction& tx)
{
//检查非CoinBase TXS的输入脚本
    if (!CTransaction(tx).IsCoinBase()) {
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            if (!tx.vin[i].scriptSig.HasValidOps() || tx.vin[i].scriptSig.size() > MAX_SCRIPT_SIZE) {
                return false;
            }
        }
    }
//检查输出脚本
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        if (!tx.vout[i].scriptPubKey.HasValidOps() || tx.vout[i].scriptPubKey.size() > MAX_SCRIPT_SIZE) {
            return false;
        }
    }

    return true;
}

bool DecodeHexTx(CMutableTransaction& tx, const std::string& hex_tx, bool try_no_witness, bool try_witness)
{
    if (!IsHex(hex_tx)) {
        return false;
    }

    std::vector<unsigned char> txData(ParseHex(hex_tx));

    if (try_no_witness) {
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS);
        try {
            ssData >> tx;
            if (ssData.eof() && (!try_witness || CheckTxScriptsSanity(tx))) {
                return true;
            }
        } catch (const std::exception&) {
//倒下。
        }
    }

    if (try_witness) {
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        try {
            ssData >> tx;
            if (ssData.empty()) {
                return true;
            }
        } catch (const std::exception&) {
//倒下。
        }
    }

    return false;
}

bool DecodeHexBlockHeader(CBlockHeader& header, const std::string& hex_header)
{
    if (!IsHex(hex_header)) return false;

    const std::vector<unsigned char> header_data{ParseHex(hex_header)};
    CDataStream ser_header(header_data, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ser_header >> header;
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

bool DecodeHexBlk(CBlock& block, const std::string& strHexBlk)
{
    if (!IsHex(strHexBlk))
        return false;

    std::vector<unsigned char> blockData(ParseHex(strHexBlk));
    CDataStream ssBlock(blockData, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ssBlock >> block;
    }
    catch (const std::exception&) {
        return false;
    }

    return true;
}

bool DecodePSBT(PartiallySignedTransaction& psbt, const std::string& base64_tx, std::string& error)
{
    std::vector<unsigned char> tx_data = DecodeBase64(base64_tx.c_str());
    CDataStream ss_data(tx_data, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ss_data >> psbt;
        if (!ss_data.empty()) {
            error = "extra data after PSBT";
            return false;
        }
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    return true;
}

bool ParseHashStr(const std::string& strHex, uint256& result)
{
    if ((strHex.size() != 64) || !IsHex(strHex))
        return false;

    result.SetHex(strHex);
    return true;
}

std::vector<unsigned char> ParseHexUV(const UniValue& v, const std::string& strName)
{
    std::string strHex;
    if (v.isStr())
        strHex = v.getValStr();
    if (!IsHex(strHex))
        throw std::runtime_error(strName + " must be hexadecimal string (not '" + strHex + "')");
    return ParseHex(strHex);
}

int ParseSighashString(const UniValue& sighash)
{
    int hash_type = SIGHASH_ALL;
    if (!sighash.isNull()) {
        static std::map<std::string, int> map_sighash_values = {
            {std::string("ALL"), int(SIGHASH_ALL)},
            {std::string("ALL|ANYONECANPAY"), int(SIGHASH_ALL|SIGHASH_ANYONECANPAY)},
            {std::string("NONE"), int(SIGHASH_NONE)},
            {std::string("NONE|ANYONECANPAY"), int(SIGHASH_NONE|SIGHASH_ANYONECANPAY)},
            {std::string("SINGLE"), int(SIGHASH_SINGLE)},
            {std::string("SINGLE|ANYONECANPAY"), int(SIGHASH_SINGLE|SIGHASH_ANYONECANPAY)},
        };
        std::string strHashType = sighash.get_str();
        const auto& it = map_sighash_values.find(strHashType);
        if (it != map_sighash_values.end()) {
            hash_type = it->second;
        } else {
            throw std::runtime_error(strHashType + " is not a valid sighash parameter.");
        }
    }
    return hash_type;
}
