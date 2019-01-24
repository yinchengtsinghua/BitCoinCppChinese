
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

/*
 *用于将数据从/转换为字符串的实用程序。
 **/

#ifndef BITCOIN_UTIL_STRENCODINGS_H
#define BITCOIN_UTIL_STRENCODINGS_H

#include <attributes.h>

#include <cstdint>
#include <string>
#include <vector>

#define ARRAYLEN(array)     (sizeof(array)/sizeof((array)[0]))

/*消毒剂使用*/
enum SafeChars
{
SAFE_CHARS_DEFAULT, //！<允许的完整字符集
SAFE_CHARS_UA_COMMENT, //！<BIP-0014子集
SAFE_CHARS_FILENAME, //！<文件名中允许使用字符
SAFE_CHARS_URI, //！<uris中允许的字符数（RFC 3986）
};

/*
*删除不安全字符。选择安全字符以允许简单邮件/URL/电子邮件
*地址，但避免任何可能有远程危险的事情，如&or>
*@param[in]str要清理的字符串
*@param[in]规则要选择的安全字符集（默认：限制最少）
*@返回一个没有不安全字符的新字符串
**/

std::string SanitizeString(const std::string& str, int rule = SAFE_CHARS_DEFAULT);
std::vector<unsigned char> ParseHex(const char* psz);
std::vector<unsigned char> ParseHex(const std::string& str);
signed char HexDigit(char c);
/*如果str中的每个字符都是十六进制字符，并且具有偶数，则返回true。
 *十六进制数*/

bool IsHex(const std::string& str);
/*
*如果字符串是十六进制数，则返回true，也可以选择前缀为“0x”
**/

bool IsHexNumber(const std::string& str);
std::vector<unsigned char> DecodeBase64(const char* p, bool* pfInvalid = nullptr);
std::string DecodeBase64(const std::string& str);
std::string EncodeBase64(const unsigned char* pch, size_t len);
std::string EncodeBase64(const std::string& str);
std::vector<unsigned char> DecodeBase32(const char* p, bool* pfInvalid = nullptr);
std::string DecodeBase32(const std::string& str);
std::string EncodeBase32(const unsigned char* pch, size_t len);
std::string EncodeBase32(const std::string& str);

void SplitHostPort(std::string in, int &portOut, std::string &hostOut);
std::string i64tostr(int64_t n);
std::string itostr(int n);
int64_t atoi64(const char* psz);
int64_t atoi64(const std::string& str);
int atoi(const std::string& str);

/*
 *测试给定字符是否为十进制数字。
 *@param[in]c要测试的字符
 *@如果参数是十进制数字，则返回true；否则返回false。
 **/

constexpr bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

/*
 *测试给定字符是否为空白字符。空白字符
 *为：空格、换行符（“\f”）、换行符（“\n”）、回车符（“\r”）、水平
 *制表符（“\t”）和竖排制表符（“\v”）。
 *
 *此函数与区域设置无关。在C语言环境下，此函数给出
 *结果与std:：isspace相同。
 *
 *@param[in]c要测试的字符
 如果参数是空白字符，*@返回true；否则返回false
 **/

constexpr inline bool IsSpace(char c) noexcept {
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

/*
 *使用严格的分析错误反馈将字符串转换为有符号32位整数。
 *@如果整个字符串可以被分析为有效整数，则返回true，
 *如果不能解析整个字符串，或者发生溢出或下溢，则为false。
 **/

NODISCARD bool ParseInt32(const std::string& str, int32_t *out);

/*
 *使用严格的分析错误反馈，将字符串转换为带符号的64位整数。
 *@如果整个字符串可以被分析为有效整数，则返回true，
 *如果不能解析整个字符串，或者发生溢出或下溢，则为false。
 **/

NODISCARD bool ParseInt64(const std::string& str, int64_t *out);

/*
 *使用严格的分析错误反馈将十进制字符串转换为无符号32位整数。
 *@如果整个字符串可以被分析为有效整数，则返回true，
 *如果不能解析整个字符串，或者发生溢出或下溢，则为false。
 **/

NODISCARD bool ParseUInt32(const std::string& str, uint32_t *out);

/*
 *使用严格的分析错误反馈将十进制字符串转换为无符号64位整数。
 *@如果整个字符串可以被分析为有效整数，则返回true，
 *如果不能解析整个字符串，或者发生溢出或下溢，则为false。
 **/

NODISCARD bool ParseUInt64(const std::string& str, uint64_t *out);

/*
 *使用严格的分析错误反馈将字符串转换为double。
 *@如果整个字符串可以被解析为有效的double，则返回true，
 *如果不能解析整个字符串，或者发生溢出或下溢，则为false。
 **/

NODISCARD bool ParseDouble(const std::string& str, double *out);

template<typename T>
std::string HexStr(const T itbegin, const T itend, bool fSpaces=false)
{
    std::string rv;
    static const char hexmap[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                                     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    rv.reserve((itend-itbegin)*3);
    for(T it = itbegin; it < itend; ++it)
    {
        unsigned char val = (unsigned char)(*it);
        if(fSpaces && it != itbegin)
            rv.push_back(' ');
        rv.push_back(hexmap[val>>4]);
        rv.push_back(hexmap[val&15]);
    }

    return rv;
}

template<typename T>
inline std::string HexStr(const T& vch, bool fSpaces=false)
{
    return HexStr(vch.begin(), vch.end(), fSpaces);
}

/*
 *将文本段落格式化为固定宽度，并为其添加空格
 *缩进到任何添加的行。
 **/

std::string FormatParagraph(const std::string& in, size_t width = 79, size_t indent = 0);

/*
 *定时抗攻击比较。
 *花费的时间与长度成比例
 *第一个参数。
 **/

template <typename T>
bool TimingResistantEqual(const T& a, const T& b)
{
    if (b.size() == 0) return a.size() == 0;
    size_t accumulator = a.size() ^ b.size();
    for (size_t i = 0; i < a.size(); i++)
        accumulator |= a[i] ^ b[i%b.size()];
    return accumulator == 0;
}

/*根据JSON编号语法将编号解析为固定点。
 *见http://json.org/number.gif
 *@成功时返回true，错误时返回false。
 *@注意，结果必须在范围内（-10^18,10^18），否则将触发溢出错误。
 **/

NODISCARD bool ParseFixedPoint(const std::string &val, int decimals, int64_t *amount_out);

/*从2次幂的一个基数转换为另一个基数。*/
template<int frombits, int tobits, bool pad, typename O, typename I>
bool ConvertBits(const O& outfn, I it, I end) {
    size_t acc = 0;
    size_t bits = 0;
    constexpr size_t maxv = (1 << tobits) - 1;
    constexpr size_t max_acc = (1 << (frombits + tobits - 1)) - 1;
    while (it != end) {
        acc = ((acc << frombits) | *it) & max_acc;
        bits += frombits;
        while (bits >= tobits) {
            bits -= tobits;
            outfn((acc >> bits) & maxv);
        }
        ++it;
    }
    if (pad) {
        if (bits) outfn((acc << (tobits - bits)) & maxv);
    } else if (bits >= frombits || ((acc << (tobits - bits)) & maxv)) {
        return false;
    }
    return true;
}

/*分析HD键路径，如“m/7/0'/2000”。*/
NODISCARD bool ParseHDKeypath(const std::string& keypath_str, std::vector<uint32_t>& keypath);

/*
 *将给定字符转换为其小写等效字符。
 *此函数与区域设置无关。它只转换为大写
 *标准7位ASCII范围内的字符。
 *@param[in]c要转换为小写的字符。
 *@返回C的小写等价物；或参数
 *如果无法转换。
 **/

constexpr char ToLower(char c)
{
    return (c >= 'A' && c <= 'Z' ? (c - 'A') + 'a' : c);
}

/*
 *将给定字符串转换为其小写等效值。
 *此函数与区域设置无关。它只转换为大写
 *标准7位ASCII范围内的字符。
 *@param[in，out]str将字符串转换为小写。
 **/

void Downcase(std::string& str);

/*
 *将给定字符转换为其大写等效字符。
 *此函数与区域设置无关。它只转换小写
 *标准7位ASCII范围内的字符。
 *@param[in]c要转换为大写的字符。
 *@返回C的大写等价物；或参数
 *如果无法转换。
 **/

constexpr char ToUpper(char c)
{
    return (c >= 'a' && c <= 'z' ? (c - 'a') + 'A' : c);
}

/*
 *将给定字符串的第一个字符大写。
 *此函数与区域设置无关。它只利用
 *参数的第一个字符（如果它具有大写等效值）
 *在标准的7位ASCII范围内。
 *@param[in]str要大写的字符串。
 *@返回首字母大写的字符串。
 **/

std::string Capitalize(std::string str);

#endif //比特币
