
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_SCRIPT_DESCRIPTOR_H
#define BITCOIN_SCRIPT_DESCRIPTOR_H

#include <script/script.h>
#include <script/sign.h>

#include <vector>

//描述符是描述一组scriptPubKeys的字符串，以及
//解决问题所需的所有信息。将所有信息合并到
//第一，它们避免了分别导入键和脚本的需要。
//
//描述符可以是范围的，当其中的公钥是
//以HD链（xpubs）的形式指定。
//
//描述符始终表示公共信息-公钥和脚本-
//但是在需要将私钥与描述符一起传送的情况下，
//通过将公钥更改为私钥（WIF）可以将它们包含在内部
//格式），并按xprvs更改xpubs。
//
//有关描述符语言的参考文档可在
//文档/描述符.md.

/*用于解析的描述符对象的接口。*/
struct Descriptor {
    virtual ~Descriptor() = default;

    /*此描述符的扩展是否取决于位置。*/
    virtual bool IsRange() const = 0;

    /*此描述符是否具有有关忽略缺少私钥的签名的所有信息。
     *除使用“raw”或“addr”结构的描述符外，所有描述符都是如此。*/

    virtual bool IsSolvable() const = 0;

    /*将描述符转换回字符串，撤消解析。*/
    virtual std::string ToString() const = 0;

    /*将描述符转换为私有字符串。如果提供的提供程序没有相关的私钥，则此操作失败。*/
    virtual bool ToPrivateString(const SigningProvider& provider, std::string& out) const = 0;

    /*在指定位置展开描述符。
     *
     *pos：扩展描述符的位置。如果isrange（）为false，则忽略它。
     *provider：在硬派生的情况下查询私钥的提供程序。
     *输出脚本：将在此处放置扩展的scriptPubKeys。
     *out：将在此处放置解决扩展的scriptPubKeys所需的脚本和公钥（可能等于提供程序）。
     *缓存：矢量，将被缓存数据覆盖，此时需要评估描述符，而不访问私钥。
     **/

    virtual bool Expand(int pos, const SigningProvider& provider, std::vector<CScript>& output_scripts, FlatSigningProvider& out, std::vector<unsigned char>* cache = nullptr) const = 0;

    /*使用缓存的扩展数据在指定位置展开描述符。
     *
     *pos：扩展描述符的位置。如果isrange（）为false，则忽略它。
     *缓存：从中读取缓存的扩展数据的向量。
     *输出脚本：将在此处放置扩展的scriptPubKeys。
     *out：将在此处放置解决扩展的scriptPubKeys所需的脚本和公钥（可能等于提供程序）。
     **/

    virtual bool ExpandFromCache(int pos, const std::vector<unsigned char>& cache, std::vector<CScript>& output_scripts, FlatSigningProvider& out) const = 0;
};

/*分析描述符字符串。包含的私钥将被放入。如果分析失败，则返回nullptr。*/
std::unique_ptr<Descriptor> Parse(const std::string& descriptor, FlatSigningProvider& out);

/*尽可能使用提供者提供的信息查找指定脚本的描述符。
 *
 *只生成指定脚本的非范围描述符将全部返回
 *环境。
 *
 *对于带有密钥来源信息的公钥，此信息将保存在返回的
 *描述符。
 *
 *-如果“provider”中存在解决“script”问题的所有信息，则将返回描述符。
 *它是“issolvable（）”，并封装了所述信息。
 *-如果失败，则如果“script”对应于已知的地址类型，“addr（）”描述符将
 *返回（不是'issolvable（）`）。
 *-如果失败，则返回“raw（）”描述符。
 **/

std::unique_ptr<Descriptor> InferDescriptor(const CScript& script, const SigningProvider& provider);

#endif //比特币脚本描述符
