
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

#ifndef BITCOIN_CORE_IO_H
#define BITCOIN_CORE_IO_H

#include <amount.h>
#include <attributes.h>

#include <string>
#include <vector>

class CBlock;
class CBlockHeader;
class CScript;
class CTransaction;
struct CMutableTransaction;
struct PartiallySignedTransaction;
class uint256;
class UniValue;

//CaleEndo.CPP
CScript ParseScript(const std::string& s);
std::string ScriptToAsmStr(const CScript& script, const bool fAttemptSighashDecode = false);
NODISCARD bool DecodeHexTx(CMutableTransaction& tx, const std::string& hex_tx, bool try_no_witness = false, bool try_witness = true);
NODISCARD bool DecodeHexBlk(CBlock&, const std::string& strHexBlk);
bool DecodeHexBlockHeader(CBlockHeader&, const std::string& hex_header);

/*
 *将十六进制字符串解析为256位
 *@param[in]strhex十六进制格式的64字符字符串
 *@param[out]结果
 *@如果成功返回true，否则返回false
 *
 *@有关此的面向RPC版本，请参阅parsehashv
 **/

bool ParseHashStr(const std::string& strHex, uint256& result);
std::vector<unsigned char> ParseHexUV(const UniValue& v, const std::string& strName);
NODISCARD bool DecodePSBT(PartiallySignedTransaction& psbt, const std::string& base64_tx, std::string& error);
int ParseSighashString(const UniValue& sighash);

//CordeWr.CPP
UniValue ValueFromAmount(const CAmount& amount);
std::string FormatScript(const CScript& script);
std::string EncodeHexTx(const CTransaction& tx, const int serializeFlags = 0);
std::string SighashToStr(unsigned char sighash_type);
void ScriptPubKeyToUniv(const CScript& scriptPubKey, UniValue& out, bool fIncludeHex);
void ScriptToUniv(const CScript& script, UniValue& out, bool include_address);
void TxToUniv(const CTransaction& tx, const uint256& hashBlock, UniValue& entry, bool include_hex = true, int serialize_flags = 0);

#endif //比特币核心
