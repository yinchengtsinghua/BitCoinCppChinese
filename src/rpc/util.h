
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2017-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_RPC_UTIL_H
#define BITCOIN_RPC_UTIL_H

#include <pubkey.h>
#include <script/standard.h>
#include <univalue.h>

#include <string>
#include <vector>

class CKeyStore;
class CPubKey;
class CScript;
struct InitInterfaces;

//！指向需要从RPC方法访问的接口的指针。由于
//！RPC框架的局限性，目前没有直接的方法来传递
//！状态到RPC方法实现。
extern InitInterfaces* g_rpc_interfaces;

CPubKey HexToPubKey(const std::string& hex_in);
CPubKey AddrToPubKey(CKeyStore* const keystore, const std::string& addr_in);
CScript CreateMultisigRedeemscript(const int required, const std::vector<CPubKey>& pubkeys);

UniValue DescribeAddress(const CTxDestination& dest);

struct RPCArg {
    enum class Type {
        OBJ,
        ARR,
        STR,
        NUM,
        BOOL,
OBJ_USER_KEYS, //！<特殊类型，其中用户必须设置键，例如定义多个地址；与此相反，例如，键是预定义的选项对象
AMOUNT,        //！<表示浮点数的特殊类型（可以是num或str）
STR_HEX,       //！<特殊类型，即只有十六进制字符的str
    };
const std::string m_name; //！<参数的名称（对于内部参数，可以为空）
    const Type m_type;
const std::vector<RPCArg> m_inner; //！<仅用于数组或dict
    const bool m_optional;
const std::string m_default_value; //！<仅用于可选参数
    const std::string m_description;
const std::string m_oneline_description; //！<应为空，除非它应重写自动生成的摘要行
const std::vector<std::string> m_type_str; //！<应为空，除非它应重写自动生成的类型字符串。向量长度为0或2，m_type_str。at（0）将重写键值对中的值类型，m_type_str。at（1）将重写参数描述中的类型。

    RPCArg(
        const std::string& name,
        const Type& type,
        const bool opt,
        const std::string& default_val,
        const std::string& description,
        const std::string& oneline_description = "",
        const std::vector<std::string>& type_str = {})
        : m_name{name},
          m_type{type},
          m_optional{opt},
          m_default_value{default_val},
          m_description{description},
          m_oneline_description{oneline_description},
          m_type_str{type_str}
    {
        assert(type != Type::ARR && type != Type::OBJ);
    }

    RPCArg(
        const std::string& name,
        const Type& type,
        const bool opt,
        const std::string& default_val,
        const std::string& description,
        const std::vector<RPCArg>& inner,
        const std::string& oneline_description = "",
        const std::vector<std::string>& type_str = {})
        : m_name{name},
          m_type{type},
          m_inner{inner},
          m_optional{opt},
          m_default_value{default_val},
          m_description{description},
          m_oneline_description{oneline_description},
          m_type_str{type_str}
    {
        assert(type == Type::ARR || type == Type::OBJ);
    }

    /*
     *返回参数的类型字符串。
     *设置OneLine以允许它被自定义的OneLine类型字符串（m_OneLine_描述）覆盖。
     **/

    std::string ToString(bool oneline) const;
    /*
     *返回参数在对象中时的类型字符串（dict）。
     *设置一行以获取一行表示（更少的空白）
     **/

    std::string ToStringObj(bool oneline) const;
    /*
     *返回描述字符串，包括参数类型和是否
     *此参数是必需的。
     *为数组中的参数设置了隐式“必需”，这既不是可选的也不是必需的。
     **/

    std::string ToDescriptionString(bool implicitly_required = false) const;
};

class RPCHelpMan
{
public:
    RPCHelpMan(const std::string& name, const std::string& description, const std::vector<RPCArg>& args);

    std::string ToString() const;

private:
    const std::string m_name;
    const std::string m_description;
    const std::vector<RPCArg> m_args;
};

