
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2017 Pieter Wuille
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <bech32.h>

namespace
{

typedef std::vector<uint8_t> data;

/*用于编码的BECH32字符集。*/
const char* CHARSET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

/*用于解码的BECH32字符集。*/
const int8_t CHARSET_REV[128] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    15, -1, 10, 17, 21, 20, 26, 30,  7,  5, -1, -1, -1, -1, -1, -1,
    -1, 29, -1, 24, 13, 25,  9,  8, 23, -1, 18, 22, 31, 27, 19, -1,
     1,  0,  3, 16, 11, 28, 12, 14,  6,  4,  2, -1, -1, -1, -1, -1,
    -1, 29, -1, 24, 13, 25,  9,  8, 23, -1, 18, 22, 31, 27, 19, -1,
     1,  0,  3, 16, 11, 28, 12, 14,  6,  4,  2, -1, -1, -1, -1, -1
};

/*连接两个字节数组。*/
data Cat(data x, const data& y)
{
    x.insert(x.end(), y.begin(), y.end());
    return x;
}

/*这个函数将把XOR的6个5位值计算成最后6个输入值，以便
 *使校验和为0。这6个值打包在一个30位整数中。更高
 *位对应于早期的值。*/

uint32_t PolyMod(const data& v)
{
//输入被解释为F=gf（32）上多项式的系数列表，其中
//在前面隐式1。如果输入为[v0，v1，v2，v3，v4]，则该多项式为v（x）=
//1*x^5+v0*x^4+v1*x^3+v2*x^2+v3*x+v4。隐式1保证
//[v0，v1，v2，…]与[0，v0，v1，v2，…]具有不同的校验和。

//输出是一个30位整数，其5位组是
//v（x）mod g（x），其中g（x）是BECH32发电机，
//x^6+29 x^5+22 x^4+20 x^3+21 x^2+29 x+18。g（x）的选择方式
//生成的代码是BCH代码，保证在
//1023个字符的窗口。在各种可能的BCH代码中，有一个被选入
//事实保证在89个字符的窗口内检测最多4个错误。

//注意系数是gf（32）的元素，这里用十进制表示。
//在{}之间。在这个有限域中，加法只是相应数字的异或。为了
//例如，27+13=27 ^13=22。乘法比较复杂，需要
//把值的位本身当作一个小域上多项式的系数，
//gf（2），乘以那些多项式mod a^5+a^3+1。例如，5*26=
//（a^2+1）*（a^4+a^3+a）=（a^4+a^3+a）*a^2+（a^4+a^3+a）=a^6+a^5+a^4+a
//=A^3+1（模式A^5+A^3+1）=9。

//在下面的循环过程中，“c”包含
//仅根据迄今为止处理过的v值（mod g（x））构造的多项式。在
//上面的示例“c”最初对应于1 mod（x），在处理2个输入之后
//v，它对应于x^2+v0*x+v1 mod g（x）。作为1 mod g（x）=1，这是起始值
//为“C”。
    uint32_t c = 1;
    for (const auto v_i : v) {
//我们要更新'c'以对应一个多出一个项的多项式。如果初始
//“c”的值由c（x）=f（x）mod g（x）的系数组成，我们将其修改为
//对应于c'（x）=（f（x）*x+v_i）mod g（x），其中v_i是
//过程。简化：
//c'（x）=（f（x）*x+v_i）mod g（x）
//（（f（x）mod g（x））*x+v_i）mod g（x）
//（c（x）*x+v_i）模式g（x）
//如果c（x）=c0*x^5+c1*x^4+c2*x^3+c3*x^2+c4*x+c5，我们要计算
//c'（x）=（c0*x^5+c1*x^4+c2*x^3+c3*x^2+c4*x+c5）*x+v_i mod g（x）
//=c0*x^6+c1*x^5+c2*x^4+c3*x^3+c4*x^2+c5*x+v_i mod g（x）
//=c0*（x^6 mod g（x））+c1*x^5+c2*x^4+c3*x^3+c4*x^2+c5*x+v_i
//如果我们调用（x^6 mod g（x））=k（x），这可以写成
//c'（x）=（c1*x^5+c2*x^4+c3*x^3+c4*x^2+c5*x+v_i）+c0*k（x）

//首先，确定c0的值：
        uint8_t c0 = c >> 25;

//然后计算c1*x^5+c2*x^4+c3*x^3+c4*x^2+c5*x+v_i：
        c = ((c & 0x1ffffff) << 5) ^ v_i;

//最后，对于c0中的每个设置位n，有条件地加2^n k（x）：
if (c0 & 1)  c ^= 0x3b6a57b2; //K（x）=29 x^5+22 x^4+20 x^3+21 x^2+29 x+18
if (c0 & 2)  c ^= 0x26508e6d; //2 k（x）=19 x^5+5 x^4+x^3+3 x^2+19 x+13
if (c0 & 4)  c ^= 0x1ea119fa; //4 k（x）=15 x^5+10 x^4+2 x^3+6 x^2+15 x+26
if (c0 & 8)  c ^= 0x3d4233dd; //8 k（x）=30 x^5+20 x^4+4 x^3+12 x^2+30 x+29
if (c0 & 16) c ^= 0x2a1462b3; //16 k（x）=21 x^5+x^4+8 x^3+24 x^2+21 x+19
    }
    return c;
}

/*转换为小写。*/
inline unsigned char LowerCase(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') ? (c - 'A') + 'a' : c;
}

/*展开HRP以用于校验和计算。*/
data ExpandHRP(const std::string& hrp)
{
    data ret;
    ret.reserve(hrp.size() + 90);
    ret.resize(hrp.size() * 2 + 1);
    for (size_t i = 0; i < hrp.size(); ++i) {
        unsigned char c = hrp[i];
        ret[i] = c >> 5;
        ret[i + hrp.size() + 1] = c & 0x1f;
    }
    ret[hrp.size()] = 0;
    return ret;
}

/*验证校验和。*/
bool VerifyChecksum(const std::string& hrp, const data& values)
{
//polymod将xor的值计算为最终值，使校验和为0。然而，
//如果我们要求校验和为0，则将0附加到有效的
//值列表将生成新的有效列表。因此，BECH32要求
//结果校验和为1。
    return PolyMod(Cat(ExpandHRP(hrp), values)) == 1;
}

/*创建校验和。*/
data CreateChecksum(const std::string& hrp, const data& values)
{
    data enc = Cat(ExpandHRP(hrp), values);
enc.resize(enc.size() + 6); //追加6个零点
uint32_t mod = PolyMod(enc) ^ 1; //确定在这6个零中执行异或运算的内容。
    data ret(6);
    for (size_t i = 0; i < 6; ++i) {
//将mod中的5位组转换为校验和值。
        ret[i] = (mod >> (5 * (5 - i))) & 31;
    }
    return ret;
}

} //命名空间

namespace bech32
{

/*编码BECH32字符串。*/
std::string Encode(const std::string& hrp, const data& values) {
    data checksum = CreateChecksum(hrp, values);
    data combined = Cat(values, checksum);
    std::string ret = hrp + '1';
    ret.reserve(ret.size() + combined.size());
    for (const auto c : combined) {
        ret += CHARSET[c];
    }
    return ret;
}

/*解码BECH32字符串。*/
std::pair<std::string, data> Decode(const std::string& str) {
    bool lower = false, upper = false;
    for (size_t i = 0; i < str.size(); ++i) {
        unsigned char c = str[i];
        if (c >= 'a' && c <= 'z') lower = true;
        else if (c >= 'A' && c <= 'Z') upper = true;
        else if (c < 33 || c > 126) return {};
    }
    if (lower && upper) return {};
    size_t pos = str.rfind('1');
    if (str.size() > 90 || pos == str.npos || pos == 0 || pos + 7 > str.size()) {
        return {};
    }
    data values(str.size() - 1 - pos);
    for (size_t i = 0; i < str.size() - 1 - pos; ++i) {
        unsigned char c = str[i + pos + 1];
        int8_t rev = CHARSET_REV[c];

        if (rev == -1) {
            return {};
        }
        values[i] = rev;
    }
    std::string hrp;
    for (size_t i = 0; i < pos; ++i) {
        hrp += LowerCase(str[i]);
    }
    if (!VerifyChecksum(hrp, values)) {
        return {};
    }
    return {hrp, data(values.begin(), values.end() - 6)};
}

} //命名空间BECH32
