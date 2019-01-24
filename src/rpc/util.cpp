
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

#include <key_io.h>
#include <keystore.h>
#include <rpc/protocol.h>
#include <rpc/util.h>
#include <tinyformat.h>
#include <util/strencodings.h>

InitInterfaces* g_rpc_interfaces = nullptr;

//如果可能，将十六进制字符串转换为公钥
CPubKey HexToPubKey(const std::string& hex_in)
{
    if (!IsHex(hex_in)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid public key: " + hex_in);
    }
    CPubKey vchPubKey(ParseHex(hex_in));
    if (!vchPubKey.IsFullyValid()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid public key: " + hex_in);
    }
    return vchPubKey;
}

//从给定的ckeystore中检索地址的公钥
CPubKey AddrToPubKey(CKeyStore* const keystore, const std::string& addr_in)
{
    CTxDestination dest = DecodeDestination(addr_in);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address: " + addr_in);
    }
    CKeyID key = GetKeyForDestination(*keystore, dest);
    if (key.IsNull()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("%s does not refer to a key", addr_in));
    }
    CPubKey vchPubKey;
    if (!keystore->GetPubKey(key, vchPubKey)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("no full public key for address %s", addr_in));
    }
    if (!vchPubKey.IsFullyValid()) {
       throw JSONRPCError(RPC_INTERNAL_ERROR, "Wallet contains an invalid public key");
    }
    return vchPubKey;
}

//从给定的公钥和所需数字列表中创建multisig redeemscript。
CScript CreateMultisigRedeemscript(const int required, const std::vector<CPubKey>& pubkeys)
{
//收集公共密钥
    if (required < 1) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "a multisignature address must require at least one key to redeem");
    }
    if ((int)pubkeys.size() < required) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("not enough keys supplied (got %u keys, but need at least %d to redeem)", pubkeys.size(), required));
    }
    if (pubkeys.size() > 16) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Number of keys involved in the multisignature address creation > 16\nReduce the number");
    }

    CScript result = GetScriptForMultisig(required, pubkeys);

    if (result.size() > MAX_SCRIPT_ELEMENT_SIZE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, (strprintf("redeemScript exceeds size limit: %d > %d", result.size(), MAX_SCRIPT_ELEMENT_SIZE)));
    }

    return result;
}

class DescribeAddressVisitor : public boost::static_visitor<UniValue>
{
public:
    explicit DescribeAddressVisitor() {}

    UniValue operator()(const CNoDestination& dest) const
    {
        return UniValue(UniValue::VOBJ);
    }

    UniValue operator()(const CKeyID& keyID) const
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("isscript", false);
        obj.pushKV("iswitness", false);
        return obj;
    }

    UniValue operator()(const CScriptID& scriptID) const
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("isscript", true);
        obj.pushKV("iswitness", false);
        return obj;
    }

    UniValue operator()(const WitnessV0KeyHash& id) const
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("isscript", false);
        obj.pushKV("iswitness", true);
        obj.pushKV("witness_version", 0);
        obj.pushKV("witness_program", HexStr(id.begin(), id.end()));
        return obj;
    }

    UniValue operator()(const WitnessV0ScriptHash& id) const
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("isscript", true);
        obj.pushKV("iswitness", true);
        obj.pushKV("witness_version", 0);
        obj.pushKV("witness_program", HexStr(id.begin(), id.end()));
        return obj;
    }

    UniValue operator()(const WitnessUnknown& id) const
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("iswitness", true);
        obj.pushKV("witness_version", (int)id.version);
        obj.pushKV("witness_program", HexStr(id.program, id.program + id.length));
        return obj;
    }
};

UniValue DescribeAddress(const CTxDestination& dest)
{
    return boost::apply_visitor(DescribeAddressVisitor(), dest);
}

struct Section {
    Section(const std::string& left, const std::string& right)
        : m_left{left}, m_right{right} {}
    const std::string m_left;
    const std::string m_right;
};

struct Sections {
    std::vector<Section> m_sections;
    size_t m_max_pad{0};

    void PushSection(const Section& s)
    {
        m_max_pad = std::max(m_max_pad, s.m_left.size());
        m_sections.push_back(s);
    }

    enum class OuterType {
        ARR,
        OBJ,
NAMED_ARG, //仅在第一个递归上设置
    };

    void Push(const RPCArg& arg, const size_t current_indent = 5, const OuterType outer_type = OuterType::NAMED_ARG)
    {
        const auto indent = std::string(current_indent, ' ');
        const auto indent_next = std::string(current_indent + 2, ' ');
        switch (arg.m_type) {
        case RPCArg::Type::STR_HEX:
        case RPCArg::Type::STR:
        case RPCArg::Type::NUM:
        case RPCArg::Type::AMOUNT:
        case RPCArg::Type::BOOL: {
if (outer_type == OuterType::NAMED_ARG) return; //对于第一个递归上的非递归类型，无需执行其他操作。
            auto left = indent;
            if (arg.m_type_str.size() != 0 && outer_type == OuterType::OBJ) {
                left += "\"" + arg.m_name + "\": " + arg.m_type_str.at(0);
            } else {
                /*t+=外部类型==外部类型：：obj？arg.tostringobj（/*oneline*/false）：arg.tostring（/*oneline*/false）；
            }
            左+ =“，”；
            pushsection（left，arg.ToDescriptionString）（*隐式需要*/ outer_type == OuterType::ARR)});

            break;
        }
        case RPCArg::Type::OBJ:
        case RPCArg::Type::OBJ_USER_KEYS: {
            /*st auto right=outer_type==outer type:：named_arg？“：arg.ToDescriptionString（/*隐式\u必需*/outer \u type==outer type:：arr）；
            pushsection（缩进+“”，右）；
            用于（const auto&arg_inner:arg.m_inner）
                push（arg_inner，current_indent+2，outertype:：obj）；
            }
            如果（arg.m_类型！=rpcarg：：类型：：obj）
                pushsection（缩进摼next+“…”，“”）；
            }
            pushsection（indent+“”+（外部_类型！=outerType：：已命名为“arg”？，“：”“”、“}”；
            断裂；
        }
        案例rpcarg:：type:：arr:
            自动向左=缩进；
            Left+=外部类型==外部类型：：obj？“\“”+arg.m_name+“\”：“”；
            左+=“”；
            const auto right=outer_type==outer type:：named_arg？“：arg.ToDescriptionString（/*隐式需要*/ outer_type == OuterType::ARR);

            PushSection({left, right});
            for (const auto& arg_inner : arg.m_inner) {
                Push(arg_inner, current_indent + 2, OuterType::ARR);
            }
            PushSection({indent_next + "...", ""});
            PushSection({indent + "]" + (outer_type != OuterType::NAMED_ARG ? "," : ""), ""});
            break;
        }

//没有默认情况，因此编译器可以警告丢失的情况
        }
    }

    std::string ToString() const
    {
        std::string ret;
        const size_t pad = m_max_pad + 4;
        for (const auto& s : m_sections) {
            if (s.m_right.empty()) {
                ret += s.m_left;
                ret += "\n";
                continue;
            }

            std::string left = s.m_left;
            left.resize(pad, ' ');
            ret += left;

//换行后正确填充
            std::string right;
            size_t begin = 0;
            size_t new_line_pos = s.m_right.find_first_of('\n');
            while (true) {
                right += s.m_right.substr(begin, new_line_pos - begin);
                if (new_line_pos == std::string::npos) {
break; //没有新线
                }
                right += "\n" + std::string(pad, ' ');
                begin = s.m_right.find_first_not_of(' ', new_line_pos + 1);
                if (begin == std::string::npos) {
break; //空行
                }
                new_line_pos = s.m_right.find_first_of('\n', begin + 1);
            }
            ret += right;
            ret += "\n";
        }
        return ret;
    }
};

RPCHelpMan::RPCHelpMan(const std::string& name, const std::string& description, const std::vector<RPCArg>& args)
    : m_name{name}, m_description{description}, m_args{args}
{
    std::set<std::string> named_args;
    for (const auto& arg : m_args) {
//应具有唯一的命名参数
        assert(named_args.insert(arg.m_name).second);
    }
}

std::string RPCHelpMan::ToString() const
{
    std::string ret;

//在线摘要
    ret += m_name;
    bool was_optional{false};
    for (const auto& arg : m_args) {
        ret += " ";
        if (arg.m_optional) {
            if (!was_optional) ret += "( ";
            was_optional = true;
        } else {
            if (was_optional) ret += ") ";
            was_optional = false;
        }
        /*+=arg.toString（/*oneline*/true）；
    }
    如果（可选）ret+=“）”；
    Rt+=“\n”；

    /描述
    ret+=m_描述；

    //参数
    截面图；
    对于（size_t i 0 i<m_args.size（）；++i）
        const auto&arg=m_args.at（i）；

        如果（i==0）ret+=“\n参数：\n”；

        //推送命名参数名称和说明
        sections.m_sections.emplace_back（std:：to_string（i+1）+”）。+arg.m_name，arg.ToDescriptionString（））；
        sections.m_max_pad=std:：max（sections.m_max_pad，sections.m_sections.back（）.m_left.size（））；

        //递归推送嵌套参数
        节。推（arg）；
    }
    ret+=节.toString（）；

    返回RET；
}

std:：string rpcarg:：todescriptionstring（const bool隐式要求）const
{
    std：：字符串ret；
    Rt+=“（）；
    如果（m_type_str.size（）！= 0）{
        ret+=m_型_str.at（1）；
    }否则{
        开关（M_型）
        案例类型：：str_hex:
        案例类型：：str：
            ret+=“字符串”；
            断裂；
        }
        案例类型：：数字：
            ret+=“数字”；
            断裂；
        }
        案例类型：：金额：
            ret+=“数字或字符串”；
            断裂；
        }
        案例类型：：bool:
            ret+=“布尔值”；
            断裂；
        }
        案例类型：：OB:
        案例类型：：obj_用户_键：
            ret+=“json对象”；
            断裂；
        }
        案例类型：：arr：
            ret+=“json数组”；
            断裂；
        }

            //没有默认情况，因此编译器可以警告丢失的情况
        }
    }
    如果（！）隐含地“需要”）；
        RET+=“，”；
        if（m_可选）
            ret+=“可选”；
            如果（！）m_default_value.empty（））
                ret+=“，default=”+m_default_value；
            }否则{
                //当所有可选参数都记录其默认值时，要启用此断言
                //断言（false）；
            }
        }否则{
            ret+=“必需”；
            assert（m_default_value.empty（））；//默认值被忽略，不能存在
        }
    }
    Rt+=“”；
    ret+=m_description.empty（）？“”：“”+m_说明；
    返回RET；
}

std:：string rpcarg:：tostringobj（const bool oneline）const
{
    std：：字符串res；
    RES+=“\”；
    RES+= MYNEX；
    如果（OnLayle）{
        RES+=“\”：
    }否则{
        RES+=“\”：
    }
    开关（M_型）
    案例类型：：STR：
        返回res+“\”str\“”；
    案例类型：：str_hex:
        返回res+“\”hex\“”；
    案例类型：NUM：
        返回res+“n”；
    案例类型：：金额：
        返回Res+“金额”；
    案例类型：：bool:
        返回res+“bool”；
    案例类型：：ARR：
        RE+= = [ ]；
        用于（const auto&i:m_inner）
            Res+=I.ToString（单线）+“，”
        }
        返回res+“…]”；
    案例类型：：OB:
    案例类型：：obj_用户密钥：
        //当前未使用，因此避免写入死码
        断言（假）；

        //没有默认情况，因此编译器可以警告丢失的情况
    }
    断言（假）；
}

std:：string rpcarg:：tostring（const bool oneline）const
{
    如果（一行&！mou oneline_description.empty（））返回mou oneline_description；

    开关（M_型）
    案例类型：：str_hex:
    案例类型：：str：
        返回“\”+m_name+“\”；
    }
    案例类型：NUM：
    案例类型：：金额：
    案例类型：：bool:
        返回MYNEX；
    }
    案例类型：：OB:
    案例类型：：obj_用户_键：
        std：：字符串res；
        对于（size_t i=0；i<m_inner.size（）；）
            Res+=M_inner[I].ToStringObj（单线）；
            if（++i<m_inner.size（））res+=“，”
        }
        if（m_type==type:：obj）
            返回“”+res+“”；
        }否则{
            返回“”+res+“，…”；
        }
    }
    案例类型：：arr：
        std：：字符串res；
        用于（const auto&i:m_inner）
            Res+=I.ToString（单线）+“，”
        }
        返回“[”+res+“…]”；
    }

        //没有默认情况，因此编译器可以警告丢失的情况
    }
    断言（假）；
}
