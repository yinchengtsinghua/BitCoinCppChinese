
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <chain.h>
#include <clientversion.h>
#include <core_io.h>
#include <crypto/ripemd160.h>
#include <key_io.h>
#include <validation.h>
#include <httpserver.h>
#include <net.h>
#include <netbase.h>
#include <outputtype.h>
#include <rpc/blockchain.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <timedata.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <warnings.h>

#include <stdint.h>
#ifdef HAVE_MALLOC_INFO
#include <malloc.h>
#endif

#include <univalue.h>

static UniValue validateaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"validateaddress",
                "\nReturn information about the given bitcoin address.\n"
                "DEPRECATION WARNING: Parts of this command have been deprecated and moved to getaddressinfo. Clients must\n"
                "transition to using getaddressinfo to access this information before upgrading to v0.18. The following deprecated\n"
                "fields have moved to getaddressinfo and will only be shown here with -deprecatedrpc=validateaddress: ismine, iswatchonly,\n"
                "script, hex, pubkeys, sigsrequired, pubkey, addresses, embedded, iscompressed, account, timestamp, hdkeypath, kdmasterkeyid.\n",
                {
                    /*地址“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”，“要验证的比特币地址”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            如果地址有效或无效，则为“Isvalid”：真假，（布尔值）。如果没有，这是唯一返回的属性。\n“
            “\”地址\“：\”地址\“，（字符串）比特币地址已验证\n”
            “scriptpubkey\”：“hex\”，（string）由地址生成的十六进制编码的scriptpubkey \n”
            “IsScript\”：true false，（布尔值），如果键是脚本\n”
            如果地址是见证地址，则为“is witness”：true false，（布尔值）。\n
            “见证版本”：版本（数字，可选）见证程序的版本号\n”
            “见证程序”：“hex”（字符串，可选）见证程序的十六进制值\n”
            “}\n”
            “\n实例：\n”
            +helpExamplecli（“验证地址”，“1pssgefhdnknxieyfrd1wceahr9hrqddwc\”）
            +helpExampleRPC（“验证地址”，“1pssgefhdnknxieyfrd1wceahr9hrqddwc\”）
        ；

    ctxdestination dest=decodedestination（request.params[0].get_str（））；
    bool isvalid=isvaliddestination（目标）；

    单值ret（单值：：vobj）；
    ret.pushkv（“isvalid”，isvalid）；
    如果（有效）
    {
        std:：string currentaddress=encodedestination（dest）；
        ret.pushkv（“地址”，当前地址）；

        cscript scriptPubkey=getscriptForDestination（目标）；
        ret.pushkv（“scriptpubkey”，hexstr（scriptpubkey.begin（），scriptpubkey.end（））；

        单值细节=描述地址（dest）；
        ret.pushkvs（细节）；
    }
    返回RET；
}

静态单值createMultisig（const jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）<2 request.params.size（）>3）
    {
        标准：：字符串消息=
            rpchelpman“创建多重搜索”，
                \n创建一个多签名地址，需要m个密钥的n个签名。\n
                “它返回带有地址和redeemscript的JSON对象。\n”，
                {
                    “不需要”，rpcarg:：type:：num，/*opt*/ false, /* default_val */ "", "The number of required signatures out of the n keys."},

                    /*eys“，rpcarg:：type:：arr，/*opt*/false，/*default_val*/”“，“十六进制编码公钥的JSON数组。”，
                        {
                            “键”，rpcarg:：type:：str_hex，/*opt*/ false, /* default_val */ "", "The hex-encoded public key"},

                        }},
                    /*地址“type”，rpcarg:：type:：str，/*opt*/true，/*默认值“legacy”，“要使用的地址类型”。选项有“传统”、“p2sh segwit”和“bech32”。
                }
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            “address\”：“multisig address\”，（string）新multisig地址的值。\n”
            “\”redeemscript\“：\”script\“（string）十六进制编码的赎回脚本的字符串值。\n”
            “}\n”

            “\n实例：\n”
            \n从2个公钥创建多组地址\n
            +helpExamplecli（“createMultisig”，“2\”[\\\“03789ed0bb717d88f7d321a368d905e7430207ebbd82bd342cf11ae157a7ace5fd\\\\\，“，\\\”03db6764b8884a92e871274b87583e6d5c258819473e17e107ef3f6aa5a61626\\”“]”+
            “作为JSON-RPC调用\n”
            +帮助示例rpc（“CreateMultisig”，“2”，\“[\\\”03789ed0bb717d88f7d321a368d905e7430207ebbd82bd342cf11ae157a7ace5fd\\\”“，\\”03dbc6764b8884a922e871274b87583e6d5c2a8819473e17e107ef3f6aa5a61626\\\”“]”
        ；
        throw std：：运行时出错（msg）；
    }

    int required=request.params[0].get_int（）；

    //获取公钥
    const univalue&keys=request.params[1].get_array（）；
    std:：vector<cpubkey>pubkeys；
    for（unsigned int i=0；i<keys.size（）；++i）
        if（ishex（keys[i].get_str（））&（keys[i].get_str（）.length（）==66 keys[i].get_str（）.length（）==130））
            pubkeys.push_back（hextopubkey（keys[i].get_str（））；
        }否则{
            throw jsonrpcerror（rpc_invalid_address_or_key，strprintf（“无效公钥：%s\n.”，key[i].get_str（））；
        }
    }

    //获取输出类型
    output type output_type=output type:：legacy；
    如果（！）request.params[2].isNull（））
        如果（！）parseOutputType（request.params[2].get_str（），output_type））
            throw jsonrpcerror（rpc_invalid_address_or_key，strprintf（“未知地址类型'%s'”，request.params[2].get_str（））；
        }
    }

    //使用付薪脚本哈希构造：
    const cscript inner=createMulsigRedeemscript（必需，pubKeys）；
    cbasickeystore密钥库；
    const ctxdestination dest=addandgetdestinationforscript（keystore，inner，output_type）；

    单值结果（单值：：vobj）；
    结果.pushkv（“地址”，编码目的地（dest））；
    result.pushkv（“redeemscript”，hexstr（inner.begin（），inner.end（））；

    返回结果；
}

静态单值验证消息（const-jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 3）
        throw std:：runtime_错误（
            rpchelpman“验证消息”，
                “验证签名邮件\n”，
                {
                    “地址”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The bitcoin address to use for the signature."},

                    /*ignature“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”，“签名者以base 64编码提供的签名（请参见signmessage）。”
                    “消息”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The message that was signed."},

                }}
                .ToString() +
            "\nResult:\n"
            "true|false   (boolean) If the signature is verified or not.\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"signature\" \"my message\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\", \"signature\", \"my message\"")
        );

    LOCK(cs_main);

    std::string strAddress  = request.params[0].get_str();
    std::string strSign     = request.params[1].get_str();
    std::string strMessage  = request.params[2].get_str();

    CTxDestination destination = DecodeDestination(strAddress);
    if (!IsValidDestination(destination)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");
    }

    const CKeyID *keyID = boost::get<CKeyID>(&destination);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");
    }

    bool fInvalid = false;
    std::vector<unsigned char> vchSig = DecodeBase64(strSign.c_str(), &fInvalid);

    if (fInvalid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CPubKey pubkey;
    if (!pubkey.RecoverCompact(ss.GetHash(), vchSig))
        return false;

    return (pubkey.GetID() == *keyID);
}

static UniValue signmessagewithprivkey(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            RPCHelpMan{"signmessagewithprivkey",
                "\nSign a message with the private key of an address\n",
                {
                    /*rivkey“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，”用于对消息签名的私钥。“，
                    “消息”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The message to create a signature of."},

                }}
                .ToString() +
            "\nResult:\n"
            "\"signature\"          (string) The signature of the message encoded in base 64\n"
            "\nExamples:\n"
            "\nCreate the signature\n"
            + HelpExampleCli("signmessagewithprivkey", "\"privkey\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"signature\" \"my message\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("signmessagewithprivkey", "\"privkey\", \"my message\"")
        );

    std::string strPrivkey = request.params[0].get_str();
    std::string strMessage = request.params[1].get_str();

    CKey key = DecodeSecret(strPrivkey);
    if (!key.IsValid()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
    }

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    std::vector<unsigned char> vchSig;
    if (!key.SignCompact(ss.GetHash(), vchSig))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");

    return EncodeBase64(vchSig.data(), vchSig.size());
}

static UniValue setmocktime(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"setmocktime",
                "\nSet the local time to given timestamp (-regtest only)\n",
                {
                    /*IMESTAMP“，rpcarg:：type:：num，/*opt*/false，/*默认值“/”，“从epoch开始的Unix秒数时间戳\n”
            “通过0返回到使用系统时间。”，
                }
                toSTRIN（）
        ；

    如果（！）params（）.mineBlocksonDemand（））
        throw std：：runtime_error（“仅限回归测试的setmocktime（-regtest模式））；

    //现在，如果我们正在进行验证，不要更改mocktime，因为
    //这可能会对mempool基于时间的逐出产生影响，以及
    //IsCurrentForFeeEstimation（）和IsInitialBlockDownload（）。
    //TODO:找出在mocktime周围同步的正确方法，以及
    //确保gettime（）的所有调用站点都能安全地访问它。
    锁（CSKEMAN）；

    rpctypecheck（request.params，单值：：vnum）；
    setMockTime（request.params[0].get_int64（））；

    返回nullunivalue；
}

静态单值rpClockedMemoryInfo（）
{
    lockedPool:：stats stats=lockedPoolManager:：instance（）.stats（）；
    单值对象（单值：：vobj）；
    obj.pushkv（“已用”，uint64_t（stats.used））；
    obj.pushkv（“自由”，uint64_t（stats.free））；
    obj.pushkv（“总计”，uint64_t（stats.total））；
    obj.pushkv（“锁定”，uint64_t（stats.locked））；
    obj.pushkv（“使用块”，uint64_t（stats.chunks_used））；
    obj.pushkv（“块空闲”，uint64_t（stats.chunks_free））；
    返回对象；
}

ifdef有\u malloc \u信息
static std：：字符串rpcmallocinfo（）
{
    char*ptr=nullptr；
    尺寸=0；
    文件*f=打开内存流（&ptr，&size）；
    f（f）{
        malloc_信息（0，f）；
        fCf（f）；
        如果（PTR）{
            std：：字符串rv（ptr，大小）；
            自由（PTR）；
            返回RV；
        }
    }
    “返回”；
}
第二节

静态单值getmemoryinfo（const jsonrpcrequest&request）
{
    /*请避免在RPC接口或帮助中使用单词“pool”，
     *用户无疑会将其与其他“内存池”混淆。
     **/

    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            RPCHelpMan{"getmemoryinfo",
                "Returns an object containing information about memory usage.\n",
                {
                    /*ode“，rpcarg:：type:：str，/*opt*/true，/*default_val*/”“stats\”“，“确定返回的信息类型。\n”
            “-\”stats“返回有关守护进程内存使用情况的常规统计信息。\n”
            “-\”mallocinfo“返回描述低级堆状态的XML字符串（仅在使用glibc 2.10+编译时可用）。”，
                }
                toSTRIN（）+
            \n结果（模式\“状态\”）：\n
            “{n”
            “锁定\”：（JSON对象）有关锁定内存管理器的信息\n”
            “已用\”：xxxxx，（数字）已用字节数\n”
            “可用\”：xxxxx，（数字）当前区域中可用的字节数\n”
            “总计”：XXXXXXX，（数字）托管字节总数\n”
            “\”锁定\“：XXXXXX，（数字）成功锁定的字节数。如果此数字小于总数，则锁定页在某一点上失败，并且密钥数据可以交换到磁盘上。\n“
            “使用的块数”：xxxxx，（数字）分配的块数\n”
            “\”块\可用\“：xxxxx，（数字）未使用的块数\n”
            “}\n”
            “}\n”
            \n结果（模式\“mallocinfo”）：\n
            “\”<malloc version=\”1\“>…\”\n”
            “\n实例：\n”
            +helpExampleCLI（“getmemoryInfo”，“”）
            +helpExampleRPC（“GetMemoryInfo”，“”）
        ；

    std:：string mode=request.params[0].isNull（）？“stats“：请求.params[0].get_str（）；
    if（mode=“stats”）
        单值对象（单值：：vobj）；
        obj.pushkv（“锁定”，rpClockedMemoryInfo（））；
        返回对象；
    else if（mode=“mallocinfo”）
ifdef有\u malloc \u信息
        返回rpcmallocinfo（）；
否则
        throw jsonrpcerror（rpc_invalid_参数，“mallocinfo仅在使用glibc 2.10+编译时可用”）；
第二节
    }否则{
        throw jsonrpcerror（rpc_无效的_参数，“未知模式”+模式）；
    }
}

静态void enable或DisableLogCategories（单值cats，bool enable）
    cats=cats.get_array（）；
    for（unsigned int i=0；i<cats.size（）；++i）
        std:：string cat=cats[i].get_str（）；

        成功；
        如果（使能）{
            success=g_logger->enableCategory（cat）；
        }否则{
            success=g_logger->disablecategory（cat）；
        }

        如果（！）成功）
            throw jsonrpcerror（rpc_无效的_参数，“未知日志类别”+cat）；
        }
    }
}

单值日志记录（const jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）>2）
        throw std:：runtime_错误（
            rpchelpman“日志记录”，
            “获取和设置日志配置。\n”
            在没有参数的情况下调用时，返回当前是否正在调试日志记录状态的类别列表。\n
            使用参数调用时，从调试日志中添加或删除类别，并返回上面的列表。\n
            “参数按\”include“和\”exclude“的顺序计算。\n”
            如果一个项目同时被包括和排除，它将被排除。\n
            “有效的日志类别是：”+listLogCategories（）+“”\n“
            此外，以下是具有特殊含义的类别名称：\n
            “-\”all\“，\”1\“：表示所有日志类别。\n”
            -“无”、“0”：即使指定了其他日志类别，也忽略所有类别。\n
            ，
                {
                    “include”，rpcarg:：type:：arr，/*opt*/ true, /* default_val */ "null", "A json array of categories to add debug logging",

                        {
                            /*include_category“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，“有效日志类别”，
                        }，
                    “排除”，rpcarg:：type:：arr，/*opt*/ true, /* default_val */ "null", "A json array of categories to remove debug logging",

                        {
                            /*exclude_category“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，“有效日志类别”，
                        }，
                }
                toSTRIN（）+
            “\NESRES:\N”
            （json对象，其中键是日志记录类别，值表示其状态\n
            “类别”：0 1，（数字），如果是否记录调试。0:不活动，1:活动\n“
            “……n”
            “}\n”
            “\n实例：\n”
            +helpExamplecli（“日志记录”，“[\\\”all\\\“]\”“[\\\”http\\\“]\”“）
            +helpexamplerpc（“日志记录”、“[\”all\“]、\”[libevent]\“”）
        ；
    }

    uint32_t original_log_categories=g_logger->getCategoryMask（）；
    if（request.params[0].isarray（））
        启用或禁用日志类别（request.params[0]，true）；
    }
    if（request.params[1].isarray（））
        启用或禁用日志类别（request.params[1]，false）；
    }
    uint32_t updated_log_categories=g_logger->getCategoryMask（）；
    uint32_t changed_log_categories=原始_log_categories^更新的_log_categories；

    //如果bclog:：libevent已更改，则更新libevent日志记录。
    //如果库版本不允许，则updateHttpServerLogging（）返回false，
    //在这种情况下，我们应该清除bclog:：libevent标志。
    //如果用户已明确要求只更改libevent，则引发错误
    //标志失败。
    if（更改了日志类别&bclog:：libevent）
        如果（！）updateHTTPServerLogging（g_logger->willLogCategory（bclog:：libevent）））
            g_logger->disablecategory（bclog:：libevent）；
            if（更改的日志类别==bclog:：libevent）
            throw jsonrpcerror（rpc_invalid_参数，“在v2.1.1之前使用libevent时无法更新libevent日志记录”）；
            }
        }
    }

    单值结果（单值：：vobj）；
    std:：vector<clogCategoryActive>vLogCatactive=listactiveLogCategories（）；
    用于（const auto&logcatactive:vlogcatactive）
        结果.pushkv（logcatactive.category，logcatactive.active）；
    }

    返回结果；
}

静态单值echo（const-jsonrpcrequest&request）
{
    if（请求.fhelp）
        throw std:：runtime_错误（
            rpchelpman“echo echojson…”，
                \n很简单地回显输入参数。此命令用于测试。\n“
                \echo和echojson的区别在于echojson在中的客户端表中启用了参数转换。
                “比特币CLI和图形用户界面。没有服务器端差异。”，
                {}
                toSTRIN（）+
            “”；

    返回request.params；
}

//关闭clang格式
静态const crpccommand命令[]
//类别名称actor（function）argnames
  ///————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    “control”、“getmemoryinfo”、&getmemoryinfo、”模式“”，
    “control”、“logging”、&logging、“include”、“exclude”，
    “util”，“validateddress”，&validateddress，“address”，
    “util”，“createMultisig”，&createMultisig，“nRequired”，“keys”，“address_type”，
    “util”，“verifymessage”，&verifymessage，“address”，“signature”，“message”，
    “util”，“signmessagewithprivkey”，&signmessagewithprivkey，“privkey”，“message”，

    /*未在帮助中显示*/

    { "hidden",             "setmocktime",            &setmocktime,            {"timestamp"}},
    { "hidden",             "echo",                   &echo,                   {"arg0","arg1","arg2","arg3","arg4","arg5","arg6","arg7","arg8","arg9"}},
    { "hidden",             "echojson",               &echo,                   {"arg0","arg1","arg2","arg3","arg4","arg5","arg6","arg7","arg8","arg9"}},
};
//CLAN格式

void RegisterMiscRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
