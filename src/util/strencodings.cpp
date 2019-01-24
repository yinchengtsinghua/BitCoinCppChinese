
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

#include <util/strencodings.h>

#include <tinyformat.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <limits>

static const std::string CHARS_ALPHA_NUM = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static const std::string SAFE_CHARS[] =
{
CHARS_ALPHA_NUM + " .,;-_/:?@()", //安全字符默认值
CHARS_ALPHA_NUM + " .,;-_?@", //安全评论
CHARS_ALPHA_NUM + ".-_", //安全文件名
CHARS_ALPHA_NUM + "!*'();:@&=+$,/?#[]-_.~%", //安全保险柜
};

std::string SanitizeString(const std::string& str, int rule)
{
    std::string strResult;
    for (std::string::size_type i = 0; i < str.size(); i++)
    {
        if (SAFE_CHARS[rule].find(str[i]) != std::string::npos)
            strResult.push_back(str[i]);
    }
    return strResult;
}

const signed char p_util_hexdigit[256] =
{ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  0,1,2,3,4,5,6,7,8,9,-1,-1,-1,-1,-1,-1,
  -1,0xa,0xb,0xc,0xd,0xe,0xf,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,0xa,0xb,0xc,0xd,0xe,0xf,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, };

signed char HexDigit(char c)
{
    return p_util_hexdigit[(unsigned char)c];
}

bool IsHex(const std::string& str)
{
    for(std::string::const_iterator it(str.begin()); it != str.end(); ++it)
    {
        if (HexDigit(*it) < 0)
            return false;
    }
    return (str.size() > 0) && (str.size()%2 == 0);
}

bool IsHexNumber(const std::string& str)
{
    size_t starting_location = 0;
    if (str.size() > 2 && *str.begin() == '0' && *(str.begin()+1) == 'x') {
        starting_location = 2;
    }
    for (const char c : str.substr(starting_location)) {
        if (HexDigit(c) < 0) return false;
    }
//对于空字符串或“0x”，返回false。
    return (str.size() > starting_location);
}

std::vector<unsigned char> ParseHex(const char* psz)
{
//将十六进制转储转换为矢量
    std::vector<unsigned char> vch;
    while (true)
    {
        while (IsSpace(*psz))
            psz++;
        signed char c = HexDigit(*psz++);
        if (c == (signed char)-1)
            break;
        unsigned char n = (c << 4);
        c = HexDigit(*psz++);
        if (c == (signed char)-1)
            break;
        n |= c;
        vch.push_back(n);
    }
    return vch;
}

std::vector<unsigned char> ParseHex(const std::string& str)
{
    return ParseHex(str.c_str());
}

void SplitHostPort(std::string in, int &portOut, std::string &hostOut) {
    size_t colon = in.find_last_of(':');
//如果找到一个：并且它要么跟在一个[…]后面，要么在字符串中没有其他：则将其视为端口分隔符。
    bool fHaveColon = colon != in.npos;
bool fBracketed = fHaveColon && (in[0]=='[' && in[colon-1]==']'); //如果有冒号，并且在[0]=“[”，冒号不是0，那么在[Colon-1]中是安全的。
    bool fMultiColon = fHaveColon && (in.find_last_of(':',colon-1) != in.npos);
    if (fHaveColon && (colon==0 || fBracketed || !fMultiColon)) {
        int32_t n;
        if (ParseInt32(in.substr(colon + 1), &n) && n > 0 && n < 0x10000) {
            in = in.substr(0, colon);
            portOut = n;
        }
    }
    if (in.size()>0 && in[0] == '[' && in[in.size()-1] == ']')
        hostOut = in.substr(1, in.size()-2);
    else
        hostOut = in;
}

std::string EncodeBase64(const unsigned char* pch, size_t len)
{
    static const char *pbase64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string str;
    str.reserve(((len + 2) / 3) * 4);
    ConvertBits<8, 6, true>([&](int v) { str += pbase64[v]; }, pch, pch + len);
    while (str.size() % 4) str += '=';
    return str;
}

std::string EncodeBase64(const std::string& str)
{
    return EncodeBase64((const unsigned char*)str.c_str(), str.size());
}

std::vector<unsigned char> DecodeBase64(const char* p, bool* pfInvalid)
{
    static const int decode64_table[256] =
    {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1,
        -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28,
        29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
        49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
    };

    const char* e = p;
    std::vector<uint8_t> val;
    val.reserve(strlen(p));
    while (*p != 0) {
        int x = decode64_table[(unsigned char)*p];
        if (x == -1) break;
        val.push_back(x);
        ++p;
    }

    std::vector<unsigned char> ret;
    ret.reserve((val.size() * 3) / 4);
    bool valid = ConvertBits<6, 8, false>([&](unsigned char c) { ret.push_back(c); }, val.begin(), val.end());

    const char* q = p;
    while (valid && *p != 0) {
        if (*p != '=') {
            valid = false;
            break;
        }
        ++p;
    }
    valid = valid && (p - e) % 4 == 0 && p - q < 4;
    if (pfInvalid) *pfInvalid = !valid;

    return ret;
}

std::string DecodeBase64(const std::string& str)
{
    std::vector<unsigned char> vchRet = DecodeBase64(str.c_str());
    return std::string((const char*)vchRet.data(), vchRet.size());
}

std::string EncodeBase32(const unsigned char* pch, size_t len)
{
    static const char *pbase32 = "abcdefghijklmnopqrstuvwxyz234567";

    std::string str;
    str.reserve(((len + 4) / 5) * 8);
    ConvertBits<8, 5, true>([&](int v) { str += pbase32[v]; }, pch, pch + len);
    while (str.size() % 8) str += '=';
    return str;
}

std::string EncodeBase32(const std::string& str)
{
    return EncodeBase32((const unsigned char*)str.c_str(), str.size());
}

std::vector<unsigned char> DecodeBase32(const char* p, bool* pfInvalid)
{
    static const int decode32_table[256] =
    {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, -1, -1, -1, -1,
        -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1,  0,  1,  2,
         3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 25, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
    };

    const char* e = p;
    std::vector<uint8_t> val;
    val.reserve(strlen(p));
    while (*p != 0) {
        int x = decode32_table[(unsigned char)*p];
        if (x == -1) break;
        val.push_back(x);
        ++p;
    }

    std::vector<unsigned char> ret;
    ret.reserve((val.size() * 5) / 8);
    bool valid = ConvertBits<5, 8, false>([&](unsigned char c) { ret.push_back(c); }, val.begin(), val.end());

    const char* q = p;
    while (valid && *p != 0) {
        if (*p != '=') {
            valid = false;
            break;
        }
        ++p;
    }
    valid = valid && (p - e) % 8 == 0 && p - q < 8;
    if (pfInvalid) *pfInvalid = !valid;

    return ret;
}

std::string DecodeBase32(const std::string& str)
{
    std::vector<unsigned char> vchRet = DecodeBase32(str.c_str());
    return std::string((const char*)vchRet.data(), vchRet.size());
}

NODISCARD static bool ParsePrechecks(const std::string& str)
{
if (str.empty()) //不允许空字符串
        return false;
if (str.size() >= 1 && (IsSpace(str[0]) || IsSpace(str[str.size()-1]))) //不允许填充
        return false;
if (str.size() != strlen(str.c_str())) //不允许嵌入nul字符
        return false;
    return true;
}

bool ParseInt32(const std::string& str, int32_t *out)
{
    if (!ParsePrechecks(str))
        return false;
    char *endp = nullptr;
errno = 0; //如果有效，strtol将不设置errno
    long int n = strtol(str.c_str(), &endp, 10);
    if(out) *out = (int32_t)n;
//请注意，strtol返回一个*long int*，因此即使strtol没有报告溢出/溢出
//我们仍然需要检查返回值是否在*int32_*的范围内。关于64位
//这些类型的平台的大小可能不同。
    return endp && *endp == 0 && !errno &&
        n >= std::numeric_limits<int32_t>::min() &&
        n <= std::numeric_limits<int32_t>::max();
}

bool ParseInt64(const std::string& str, int64_t *out)
{
    if (!ParsePrechecks(str))
        return false;
    char *endp = nullptr;
errno = 0; //如果有效，strtoll将不设置errno
    long long int n = strtoll(str.c_str(), &endp, 10);
    if(out) *out = (int64_t)n;
//请注意，strtoll返回一个*long long int*，因此即使strtol没有报告溢出/溢出
//我们仍然需要检查返回值是否在*int64_*的范围内。
    return endp && *endp == 0 && !errno &&
        n >= std::numeric_limits<int64_t>::min() &&
        n <= std::numeric_limits<int64_t>::max();
}

bool ParseUInt32(const std::string& str, uint32_t *out)
{
    if (!ParsePrechecks(str))
        return false;
if (str.size() >= 1 && str[0] == '-') //拒绝负值，不幸的是，strtoul默认接受这些值（如果它们符合范围）
        return false;
    char *endp = nullptr;
errno = 0; //如果有效，strtoul将不设置errno
    unsigned long int n = strtoul(str.c_str(), &endp, 10);
    if(out) *out = (uint32_t)n;
//请注意，strtoul返回一个*无符号long int*，因此即使它没有报告溢出/下溢
//我们仍然需要检查返回值是否在*uint32_t*的范围内。关于64位
//这些类型的平台的大小可能不同。
    return endp && *endp == 0 && !errno &&
        n <= std::numeric_limits<uint32_t>::max();
}

bool ParseUInt64(const std::string& str, uint64_t *out)
{
    if (!ParsePrechecks(str))
        return false;
if (str.size() >= 1 && str[0] == '-') //拒绝负值，不幸的是，strtoull默认情况下接受这些值，如果它们符合范围
        return false;
    char *endp = nullptr;
errno = 0; //如果有效，strtoull将不设置errno
    unsigned long long int n = strtoull(str.c_str(), &endp, 10);
    if(out) *out = (uint64_t)n;
//请注意，strtoull返回一个*无符号长整型int*，因此即使它没有报告溢出/下溢
//我们仍然需要检查返回值是否在*uint64_t*的范围内。
    return endp && *endp == 0 && !errno &&
        n <= std::numeric_limits<uint64_t>::max();
}


bool ParseDouble(const std::string& str, double *out)
{
    if (!ParsePrechecks(str))
        return false;
if (str.size() >= 2 && str[0] == '0' && str[1] == 'x') //不允许十六进制浮点数
        return false;
    std::istringstream text(str);
    text.imbue(std::locale::classic());
    double result;
    text >> result;
    if(out) *out = result;
    return text.eof() && !text.fail();
}

std::string FormatParagraph(const std::string& in, size_t width, size_t indent)
{
    std::stringstream out;
    size_t ptr = 0;
    size_t indented = 0;
    while (ptr < in.size())
    {
        size_t lineend = in.find_first_of('\n', ptr);
        if (lineend == std::string::npos) {
            lineend = in.size();
        }
        const size_t linelen = lineend - ptr;
        const size_t rem_width = width - indented;
        if (linelen <= rem_width) {
            out << in.substr(ptr, linelen + 1);
            ptr = lineend + 1;
            indented = 0;
        } else {
            size_t finalspace = in.find_last_of(" \n", ptr + rem_width);
            if (finalspace == std::string::npos || finalspace < ptr) {
//没有地方可以打破；只需包含整个单词并继续
                finalspace = in.find_first_of("\n ", ptr);
                if (finalspace == std::string::npos) {
//字符串的结尾，只需添加它并中断
                    out << in.substr(ptr);
                    break;
                }
            }
            out << in.substr(ptr, finalspace - ptr) << "\n";
            if (in[finalspace] == '\n') {
                indented = 0;
            } else if (indent) {
                out << std::string(indent, ' ');
                indented = indent;
            }
            ptr = finalspace + 1;
        }
    }
    return out.str();
}

std::string i64tostr(int64_t n)
{
    return strprintf("%d", n);
}

std::string itostr(int n)
{
    return strprintf("%d", n);
}

int64_t atoi64(const char* psz)
{
#ifdef _MSC_VER
    return _atoi64(psz);
#else
    return strtoll(psz, nullptr, 10);
#endif
}

int64_t atoi64(const std::string& str)
{
#ifdef _MSC_VER
    return _atoi64(str.c_str());
#else
    return strtoll(str.c_str(), nullptr, 10);
#endif
}

int atoi(const std::string& str)
{
    return atoi(str.c_str());
}

/*尾数的上界。
 *10^18-1是最大的任意十进制数，适合于有符号的64位整数。
 *较大整数不能由0-9的任意组合组成：
 *
 *99999999999999 1^18-1
 *9223372036854775807（1<<63）-1（最大Int64_t）
 *999999999999999 1^19-1（将溢出）
 **/

static const int64_t UPPER_BOUND = 1000000000000000000LL - 1LL;

/*parseFixedPoint的helper函数*/
static inline bool ProcessMantissaDigit(char ch, int64_t &mantissa, int &mantissa_tzeros)
{
    if(ch == '0')
        ++mantissa_tzeros;
    else {
        for (int i=0; i<=mantissa_tzeros; ++i) {
            if (mantissa > (UPPER_BOUND / 10LL))
                /*urn false；/*溢出*/
            尾数*＝10；
        }
        尾数+=ch-‘0’；
        尾数=0；
    }
    回归真实；
}

bool parseFixedPoint（const std:：string&val，int decimals，int64_t*amount_out）
{
    int64尾数=0；
    Int64_t指数=0；
    尾数int=0；
    布尔尾数符号=假；
    bool exponent_sign=false；
    INTPR＝0；
    int end=val.size（）；
    int点ou ofs=0；

    if（ptr<end&&val[ptr]='-'）
        尾数符号=真；
        +pTR；
    }
    如果（PTR＜结束）
    {
        if（val[ptr]=‘0’）
            /*单程0*/

            ++ptr;
        } else if (val[ptr] >= '1' && val[ptr] <= '9') {
            while (ptr < end && IsDigit(val[ptr])) {
                if (!ProcessMantissaDigit(val[ptr], mantissa, mantissa_tzeros))
                    /*urn false；/*溢出*/
                +pTR；
            }
        else返回false；/*缺少预期数字*/

    /*lse返回false；/*空字符串或松散的'-'*/
    if（ptr<end&&val[ptr]=''）
    {
        +pTR；
        如果（ptr<end&isdigit（val[ptr]））
        {
            while（ptr<end&&isdigit（val[ptr]））
                如果（！）进程尾数数字（val[ptr]、尾数、尾数
                    返回false；/*溢出*/

                ++ptr;
                ++point_ofs;
            }
        /*LSE返回FALSE；/*缺少预期数字*/
    }
    if（ptr<end&&（val[ptr]=‘e’val[ptr]=‘e’）
    {
        +pTR；
        if（ptr<end&&val[ptr]='+'）
            +pTR；
        else if（ptr<end&&val[ptr]='-'）
            指数符号=真；
            +pTR；
        }
        if（ptr<end&isdigit（val[ptr]））；
            while（ptr<end&&isdigit（val[ptr]））
                if（指数>（上限/10ll））
                    返回false；/*溢出*/

                exponent = exponent * 10 + val[ptr] - '0';
                ++ptr;
            }
        /*LSE返回FALSE；/*缺少预期数字*/
    }
    如果（PTR）！=结束）
        返回false；/*尾随垃圾*/


    /*完成指数*/
    if (exponent_sign)
        exponent = -exponent;
    exponent = exponent - point_ofs + mantissa_tzeros;

    /*最后确定尾数*/
    if (mantissa_sign)
        mantissa = -mantissa;

    /*转换为一个64位定点值*/
    exponent += decimals;
    if (exponent < 0)
        /*urn false；/*不能表示小于10^-小数的值*/
    如果（指数>=18）
        返回false；/*不能表示大于或等于10^（18位小数）的值*/


    for (int i=0; i < exponent; ++i) {
        if (mantissa > (UPPER_BOUND / 10LL) || mantissa < -(UPPER_BOUND / 10LL))
            /*urn false；/*溢出*/
        尾数*＝10；
    }
    if（尾数>上界尾数<-上界）
        返回false；/*溢出*/


    if (amount_out)
        *amount_out = mantissa;

    return true;
}

bool ParseHDKeypath(const std::string& keypath_str, std::vector<uint32_t>& keypath)
{
    std::stringstream ss(keypath_str);
    std::string item;
    bool first = true;
    while (std::getline(ss, item, '/')) {
        if (item.compare("m") == 0) {
            if (first) {
                first = false;
                continue;
            }
            return false;
        }
//发现是否硬化
        uint32_t path = 0;
        size_t pos = item.find("'");
        if (pos != std::string::npos) {
//硬标记只能位于字符串的最后一个索引中
            if (pos != item.size() - 1) {
                return false;
            }
            path |= 0x80000000;
item = item.substr(0, item.size() - 1); //删除最后一个字符，即硬勾号
        }

//确保这只是数字
        if (item.find_first_not_of( "0123456789" ) != std::string::npos) {
            return false;
        }
        uint32_t number;
        if (!ParseUInt32(item, &number)) {
            return false;
        }
        path |= number;

        keypath.push_back(path);
        first = false;
    }
    return true;
}

void Downcase(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](char c){return ToLower(c);});
}

std::string Capitalize(std::string str)
{
    if (str.empty()) return str;
    str[0] = ToUpper(str.front());
    return str;
}
