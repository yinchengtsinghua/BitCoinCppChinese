
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2019比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <rpc/server.h>

#include <chainparams.h>
#include <clientversion.h>
#include <core_io.h>
#include <net.h>
#include <net_processing.h>
#include <netbase.h>
#include <policy/policy.h>
#include <rpc/protocol.h>
#include <rpc/util.h>
#include <sync.h>
#include <timedata.h>
#include <ui_interface.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <validation.h>
#include <version.h>
#include <warnings.h>

#include <univalue.h>

static UniValue getconnectioncount(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            RPCHelpMan{"getconnectioncount",
                "\nReturns the number of connections to other nodes.\n", {}}
                .ToString() +
            "\nResult:\n"
            "n          (numeric) The connection count\n"
            "\nExamples:\n"
            + HelpExampleCli("getconnectioncount", "")
            + HelpExampleRpc("getconnectioncount", "")
        );

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    return (int)g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL);
}

static UniValue ping(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            RPCHelpMan{"ping",
                "\nRequests that a ping be sent to all other nodes, to measure ping time.\n"
                "Results provided in getpeerinfo, pingtime and pingwait fields are decimal seconds.\n"
                "Ping command is handled in queue with all other commands, so it measures processing backlog, not just network ping.\n",
                {}}
                .ToString() +
            "\nExamples:\n"
            + HelpExampleCli("ping", "")
            + HelpExampleRpc("ping", "")
        );

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

//请求每个节点在下一个消息处理过程中发送ping
    g_connman->ForEachNode([](CNode* pnode) {
        pnode->fPingQueued = true;
    });
    return NullUniValue;
}

static UniValue getpeerinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            RPCHelpMan{"getpeerinfo",
                "\nReturns data about each connected network node as a json array of objects.\n", {}}
                .ToString() +
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"id\": n,                   (numeric) Peer index\n"
            "    \"addr\":\"host:port\",      (string) The IP address and port of the peer\n"
            "    \"addrbind\":\"ip:port\",    (string) Bind address of the connection to the peer\n"
            "    \"addrlocal\":\"ip:port\",   (string) Local address as reported by the peer\n"
            "    \"services\":\"xxxxxxxxxxxxxxxx\",   (string) The services offered\n"
            "    \"relaytxes\":true|false,    (boolean) Whether peer has asked us to relay transactions to it\n"
            "    \"lastsend\": ttt,           (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last send\n"
            "    \"lastrecv\": ttt,           (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last receive\n"
            "    \"bytessent\": n,            (numeric) The total bytes sent\n"
            "    \"bytesrecv\": n,            (numeric) The total bytes received\n"
            "    \"conntime\": ttt,           (numeric) The connection time in seconds since epoch (Jan 1 1970 GMT)\n"
            "    \"timeoffset\": ttt,         (numeric) The time offset in seconds\n"
            "    \"pingtime\": n,             (numeric) ping time (if available)\n"
            "    \"minping\": n,              (numeric) minimum observed ping time (if any at all)\n"
            "    \"pingwait\": n,             (numeric) ping wait (if non-zero)\n"
            "    \"version\": v,              (numeric) The peer version, such as 70001\n"
            "    \"subver\": \"/Satoshi:0.8.5/\",  (string) The string version\n"
            "    \"inbound\": true|false,     (boolean) Inbound (true) or Outbound (false)\n"
            "    \"addnode\": true|false,     (boolean) Whether connection was due to addnode/-connect or if it was an automatic/inbound connection\n"
            "    \"startingheight\": n,       (numeric) The starting height (block) of the peer\n"
            "    \"banscore\": n,             (numeric) The ban score\n"
            "    \"synced_headers\": n,       (numeric) The last header we have in common with this peer\n"
            "    \"synced_blocks\": n,        (numeric) The last block we have in common with this peer\n"
            "    \"inflight\": [\n"
            "       n,                        (numeric) The heights of blocks we're currently asking from this peer\n"
            "       ...\n"
            "    ],\n"
            "    \"whitelisted\": true|false, (boolean) Whether the peer is whitelisted\n"
            "    \"minfeefilter\": n,         (numeric) The minimum fee rate for transactions this peer accepts\n"
            "    \"bytessent_per_msg\": {\n"
            "       \"msg\": n,               (numeric) The total bytes sent aggregated by message type\n"
            "                               When a message type is not listed in this json object, the bytes sent are 0.\n"
            "                               Only known message types can appear as keys in the object.\n"
            "       ...\n"
            "    },\n"
            "    \"bytesrecv_per_msg\": {\n"
            "       \"msg\": n,               (numeric) The total bytes received aggregated by message type\n"
            "                               When a message type is not listed in this json object, the bytes received are 0.\n"
            "                               Only known message types can appear as keys in the object and all bytes received of unknown message types are listed under '"+NET_MESSAGE_COMMAND_OTHER+"'.\n"
            "       ...\n"
            "    }\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getpeerinfo", "")
            + HelpExampleRpc("getpeerinfo", "")
        );

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    std::vector<CNodeStats> vstats;
    g_connman->GetNodeStats(vstats);

    UniValue ret(UniValue::VARR);

    for (const CNodeStats& stats : vstats) {
        UniValue obj(UniValue::VOBJ);
        CNodeStateStats statestats;
        bool fStateStats = GetNodeStateStats(stats.nodeid, statestats);
        obj.pushKV("id", stats.nodeid);
        obj.pushKV("addr", stats.addrName);
        if (!(stats.addrLocal.empty()))
            obj.pushKV("addrlocal", stats.addrLocal);
        if (stats.addrBind.IsValid())
            obj.pushKV("addrbind", stats.addrBind.ToString());
        obj.pushKV("services", strprintf("%016x", stats.nServices));
        obj.pushKV("relaytxes", stats.fRelayTxes);
        obj.pushKV("lastsend", stats.nLastSend);
        obj.pushKV("lastrecv", stats.nLastRecv);
        obj.pushKV("bytessent", stats.nSendBytes);
        obj.pushKV("bytesrecv", stats.nRecvBytes);
        obj.pushKV("conntime", stats.nTimeConnected);
        obj.pushKV("timeoffset", stats.nTimeOffset);
        if (stats.dPingTime > 0.0)
            obj.pushKV("pingtime", stats.dPingTime);
        if (stats.dMinPing < static_cast<double>(std::numeric_limits<int64_t>::max())/1e6)
            obj.pushKV("minping", stats.dMinPing);
        if (stats.dPingWait > 0.0)
            obj.pushKV("pingwait", stats.dPingWait);
        obj.pushKV("version", stats.nVersion);
//在这里使用经过消毒的颠覆者形式，以避免来自
//通过将特殊字符放入
//他们的版本信息。
        obj.pushKV("subver", stats.cleanSubVer);
        obj.pushKV("inbound", stats.fInbound);
        obj.pushKV("addnode", stats.m_manual_connection);
        obj.pushKV("startingheight", stats.nStartingHeight);
        if (fStateStats) {
            obj.pushKV("banscore", statestats.nMisbehavior);
            obj.pushKV("synced_headers", statestats.nSyncHeight);
            obj.pushKV("synced_blocks", statestats.nCommonHeight);
            UniValue heights(UniValue::VARR);
            for (const int height : statestats.vHeightInFlight) {
                heights.push_back(height);
            }
            obj.pushKV("inflight", heights);
        }
        obj.pushKV("whitelisted", stats.fWhitelisted);
        obj.pushKV("minfeefilter", ValueFromAmount(stats.minFeeFilter));

        UniValue sendPerMsgCmd(UniValue::VOBJ);
        for (const auto& i : stats.mapSendBytesPerMsgCmd) {
            if (i.second > 0)
                sendPerMsgCmd.pushKV(i.first, i.second);
        }
        obj.pushKV("bytessent_per_msg", sendPerMsgCmd);

        UniValue recvPerMsgCmd(UniValue::VOBJ);
        for (const auto& i : stats.mapRecvBytesPerMsgCmd) {
            if (i.second > 0)
                recvPerMsgCmd.pushKV(i.first, i.second);
        }
        obj.pushKV("bytesrecv_per_msg", recvPerMsgCmd);

        ret.push_back(obj);
    }

    return ret;
}

static UniValue addnode(const JSONRPCRequest& request)
{
    std::string strCommand;
    if (!request.params[1].isNull())
        strCommand = request.params[1].get_str();
    if (request.fHelp || request.params.size() != 2 ||
        (strCommand != "onetry" && strCommand != "add" && strCommand != "remove"))
        throw std::runtime_error(
            RPCHelpMan{"addnode",
                "\nAttempts to add or remove a node from the addnode list.\n"
                "Or try a connection to a node once.\n"
                "Nodes added using addnode (or -connect) are protected from DoS disconnection and are not required to be\n"
                "full nodes/support SegWit as other outbound peers are (though such peers will not be synced from).\n",
                {
                    /*ode“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”“，“节点（有关节点，请参阅getpeerinfo）”，
                    “command”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "'add' to add a node to the list, 'remove' to remove a node from the list, 'onetry' to try a connection to the node once"},

                }}
                .ToString() +
            "\nExamples:\n"
            + HelpExampleCli("addnode", "\"192.168.0.6:8333\" \"onetry\"")
            + HelpExampleRpc("addnode", "\"192.168.0.6:8333\", \"onetry\"")
        );

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    std::string strNode = request.params[0].get_str();

    if (strCommand == "onetry")
    {
        CAddress addr;
        g_connman->OpenNetworkConnection(addr, false, nullptr, strNode.c_str(), false, false, true);
        return NullUniValue;
    }

    if (strCommand == "add")
    {
        if(!g_connman->AddNode(strNode))
            throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: Node already added");
    }
    else if(strCommand == "remove")
    {
        if(!g_connman->RemoveAddedNode(strNode))
            throw JSONRPCError(RPC_CLIENT_NODE_NOT_ADDED, "Error: Node has not been added.");
    }

    return NullUniValue;
}

static UniValue disconnectnode(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() == 0 || request.params.size() >= 3)
        throw std::runtime_error(
            RPCHelpMan{"disconnectnode",
                "\nImmediately disconnects from the specified peer node.\n"
                "\nStrictly one out of 'address' and 'nodeid' can be provided to identify the node.\n"
                "\nTo disconnect by nodeid, either set 'address' to the empty string, or call using the named 'nodeid' argument only.\n",
                {
                    /*地址“，rpcarg:：type:：str，/*opt*/true，/*default_val*/”fallback to nodeid“，”节点的IP地址/端口“，
                    “nodeid”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "fallback to address", "The node ID (see getpeerinfo for node IDs)"},

                }}
                .ToString() +
            "\nExamples:\n"
            + HelpExampleCli("disconnectnode", "\"192.168.0.6:8333\"")
            + HelpExampleCli("disconnectnode", "\"\" 1")
            + HelpExampleRpc("disconnectnode", "\"192.168.0.6:8333\"")
            + HelpExampleRpc("disconnectnode", "\"\", 1")
        );

    if(!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    bool success;
    const UniValue &address_arg = request.params[0];
    const UniValue &id_arg = request.params[1];

    if (!address_arg.isNull() && id_arg.isNull()) {
        /*按地址处理断开连接*/
        success = g_connman->DisconnectNode(address_arg.get_str());
    } else if (!id_arg.isNull() && (address_arg.isNull() || (address_arg.isStr() && address_arg.get_str().empty()))) {
        /*按ID处理断开连接*/
        NodeId nodeid = (NodeId) id_arg.get_int64();
        success = g_connman->DisconnectNode(nodeid);
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMS, "Only one of address and nodeid should be provided.");
    }

    if (!success) {
        throw JSONRPCError(RPC_CLIENT_NODE_NOT_CONNECTED, "Node not found in connected nodes");
    }

    return NullUniValue;
}

static UniValue getaddednodeinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            RPCHelpMan{"getaddednodeinfo",
                "\nReturns information about the given added node, or all added nodes\n"
                "(note that onetry addnodes are not listed here)\n",
                {
                    /*ode“，rpcarg:：type:：str，/*opt*/true，/*default_val*/“所有节点”，“如果提供，则返回有关此特定节点的信息，否则返回所有节点。”，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “\n”
            “{n”
            “\”addednode\“：\”192.168.0.201\“，（string）节点IP地址或名称（如提供给addnode的那样）\n”
            “已连接\”：真假，（布尔值），如果已连接\n”
            “地址”：[（对象列表）仅当connected=true时\n”
            “{n”
            “address\”：\“192.168.0.201:8333\”，（string）我们连接的比特币服务器IP和端口\n”
            “已连接\”：“出站\”（字符串）连接，入站或出站\n”
            “}\n”
            “\n”
            “}\n”
            “……\n”
            “\n”
            “\n实例：\n”
            +helpexamplecli（“getaddednodeinfo”，“192.168.0.201”）。
            +helpexamplerpc（“getaddednodeinfo”，“192.168.0.201”）。
        ；

    如果（！）吉康曼
        throw jsonrpcerror（rpc_client_p2p_disabled，“错误：缺少或禁用对等功能”）；

    std:：vector<addednodeinfo>vinfo=g_connman->getaddednodeinfo（）；

    如果（！）request.params[0].isNull（））
        bool found=false；
        用于（const addednodeinfo&info:vinfo）
            if（info.straddednode==request.params[0].get_str（））
                vinfo.assign（1，信息）；
                发现=真；
                断裂；
            }
        }
        如果（！）发现）{
            throw jsonrpcerror（rpc_client_node_not_added，“错误：未添加节点。”）；
        }
    }

    单值ret（单值：：varr）；

    用于（const addednodeinfo&info:vinfo）
        单值对象（单值：：vobj）；
        对象pushkv（“addednode”，info.straddednode）；
        对象pushkv（“已连接”，信息fconnected）；
        单值地址（univalue:：varr）；
        如果（信息已连接）
            单值地址（univalue:：vobj）；
            address.pushkv（“地址”，info.resolvedAddress.toString（））；
            地址.pushkv（“已连接”，info.finbound？”inbound”：“出站”）；
            地址。按回（地址）；
        }
        对象pushkv（“地址”，地址）；
        后退（obj）；
    }

    返回RET；
}

静态单值getnettotals（const-jsonrpcrequest&request）
{
    if（request.fhelp request.params.size（）>0）
        throw std:：runtime_错误（
            rpchelpman“getnettotals”，
                \n返回有关网络流量的信息，包括字节输入、字节输出、\n
                “和当前时间。\n”，
                {}
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            ““totalbytesrecv\”：n，（数字）接收的总字节数\n”
            “total bytes sent”：n，（数字）发送的总字节数\n”
            ““TimeMillis”：t，（数字）当前Unix时间（以毫秒为单位）。\n”
            “上载目标\”：\n”
            “{n”
            ““Timeframe”：n，（数字）以秒为单位的测量时间范围的长度\n”
            “目标\”：n，（数字）以字节为单位的目标\n”
            “达到目标\”：真假，（布尔值）真，如果达到目标\n”
            “服务于\u历史块\”：真假，（布尔值）如果服务于历史块，则为真\n”
            “\”字节\u在\u周期中\“：t，（数字）当前时间周期中剩余的字节\n”
            “\”时间\以\周期\“：t（数字）当前时间周期还剩秒数\n”
            “}\n”
            “}\n”
            “\n实例：\n”
            +helpexamplecli（“getnettotals”，“”）
            +helpexamplerpc（“getnettotals”，“”）
       ；
    如果（！）吉康曼
        throw jsonrpcerror（rpc_client_p2p_disabled，“错误：缺少或禁用对等功能”）；

    单值对象（单值：：vobj）；
    obj.pushkv（“totalbytesrecv”，g_connman->gettotalbytesrecv（））；
    obj.pushkv（“totalbytessent”，g_connman->gettotalbytessent（））；
    obj.pushkv（“timemillis”，getTimemillis（））；

    单值边界外限制（单值：：vobj）；
    outboundLimit.pushkv（“timeframe”，g_connman->getMaxOutboundTimeframe（））；
    outboundLimit.pushkv（“目标”，g_connman->getMaxOutboundTarget（））；
    outboundLimit.pushkv（“达到目标”，g_connman->outboundTargetReached（false））；
    outboundlimit.pushkv（“服务于历史街区”！gou connman->OutboundTargetReached（true））；
    outboundLimit.pushkv（“bytes_left_in_cycle”，g_connman->getOutboundTargetBytesLeft（））；
    outboundlimit.pushkv（“time_left_in_cycle”，g_connman->getmaxoutboundtimeleftincycle（））；
    obj.pushkv（“UploadTarget”，OutboundLimit）；
    返回对象；
}

静态单值GetNetworkSinfo（）
{
    单值网络（univalue:：varr）；
    对于（int n=0；n<net_max；++n）
    {
        枚举网络network=static_cast<enum network>（n）；
        if（network==net_unroutable network==net_internal）
            继续；
        代理类型代理；
        单值对象（单值：：vobj）；
        getproxy（网络、代理）；
        对象pushkv（“名称”，getnetworkname（network））；
        对象pushkv（“有限”！可攻击（网络）；
        对象pushkv（“可到达”，可访问（网络））；
        obj.pushkv（“代理”，proxy.isvalid（）？proxy.proxy.toStringPPort（）：std:：string（））；
        obj.pushkv（“proxy_randomize_credentials”，proxy.randomize_credentials）；
        网络。推回（obj）；
    }
    返回网络；
}

静态单值getnetworkinfo（const jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 0）
        throw std:：runtime_错误（
            rpchelpman“获取网络信息”，
                “返回一个包含有关P2P网络的各种状态信息的对象。\n”，
                toSTRIN（）+
            “\NESRES:\N”
            “{n”
            “版本”：xxxxx，（数字）服务器版本\n”
            “Subversion\”：\“/Satoshi:x.x.x/\”，（string）服务器Subversion字符串\n”
            ““ProtocolVersion\”：xxxxx，（数字）协议版本\n”
            “localservices”：“XXXXXXXXXXXXXX”，（字符串）我们向网络提供的服务\n”
            “localrelay”：如果从对等方请求事务中继，则为true false，（bool）true \n”
            “TimeOffset”：XXXXX，（数字）时间偏移量\n”
            ““连接数”：xxxxx，（数字）连接数\n”
            “networkactive”：真假，（bool）是否启用P2P网络\n”
            “网络\”：[（数组）每个网络的信息\n”
            “{n”
            “name\”：\“xxx\”，（字符串）网络（IPv4、IPv6或洋葱）\n”
            “\”limited\”：true false，（布尔值）网络是否限制使用-onlynet？“N”
            “\”可访问\“：真假，（布尔值）网络是否可访问？“N”
            “代理\”：“主机：端口\”（string）用于此网络的代理，如果没有，则为空\n”
            “代理\随机化\凭证\”：真假，（字符串）是否使用随机化凭证\n”
            “}\n”
            “……\n”
            “”
            “\”relay fee\“：x.x x x x x x x x，（数字）以“+货币\单位+”/kb表示的交易的最低中继费\n”
            “递增费用\”：x.x x x x x x x x，（数字）mempool限制或bip 125替换的最低费用递增，单位为”+货币单位+“/kb\n”
            “local addresses”：[（数组）本地地址列表\n”
            “{n”
            “地址\”：“XXXX”，（字符串）网络地址\n”
            “端口\”：xxx，（数字）网络端口\n”
            “分数\”：XXX（数字）相对分数\n”
            “}\n”
            “……\n”
            “\n”
            “警告\”“：\”“…\”（字符串）任何网络和区块链警告\n”
            “}\n”
            “\n实例：\n”
            +helpexamplecli（“getnetworkinfo”，“”）
            +helpExampleRPC（“getNetworkInfo”，“”）
        ；

    锁（CSKEMAN）；
    单值对象（单值：：vobj）；
    obj.pushkv（“版本”，客户机版本）；
    obj.pushkv（“颠覆”，strSubversion）；
    obj.pushkv（“协议版本”，协议版本）；
    如果（GyCouman）
        obj.pushkv（“本地服务”，strprintf（“%016x”，g_connman->getlocalservices（））；
    对象pushkv（“本地中继”，frelaytxes）；
    obj.pushkv（“timeoffset”，getTimeoffset（））；
    如果（g_connman）
        obj.pushkv（“networkactive”，g_connman->getnetworkactive（））；
        obj.pushkv（“连接”，
    }
    obj.pushkv（“网络”，getnetworksinfo（））；
    obj.pushkv（“relayfee”，valuefromamount（：：minrelaytxfee.getfeeperk（））；
    obj.pushkv（“incrementalFee”，valueFromAmount（：：incrementalRelayFee.getFeeperk（））；
    univalue本地地址（univalue:：varr）；
    {
        锁定（cs_maplocalhost）；
        for（const std:：pair<const cnetaddr，localserviceinfo>&item:maplocalhost）
        {
            单值rec（单值：：vobj）；
            rec.pushkv（“地址”，item.first.toString（））；
            rec.pushkv（“端口”，第二项，nport）；
            rec.pushkv（“分数”，第二项，NScore）；
            本地地址。推回（rec）；
        }
    }
    obj.pushkv（“本地地址”，本地地址）；
    obj.pushkv（“警告”，getwarnings（“statusbar”））；
    返回对象；
}

静态单值setban（const-jsonrpcrequest&request）
{
    std：：字符串strcommand；
    如果（！）请求.params[1].isNull（））
        strcommand=request.params[1].get_str（）；
    if（request.fhelp request.params.size（）<2
        （命令！=“添加”&&strCommand！=“删除”）
        throw std:：runtime_错误（
            rpchelpman“setban”，
                \n试图从禁止列表中添加或删除IP/子网。\n“，
                {
                    “子网”，rpcarg:：type:：str，/*opt*/ false, /* default_val */ "", "The IP/Subnet (see getpeerinfo for nodes IP) with an optional netmask (default is /32 = single IP)"},

                    /*ommand“，rpcarg:：type:：str，/*opt*/false，/*default_val*/”，“add”将IP/子网添加到列表中，“remove”将IP/子网从列表中删除“，
                    “bantime”，rpcarg:：type:：num，/*opt*/ true, /* default_val */ "0", "time in seconds how long (or until when if [absolute] is set) the IP is banned (0 or empty means using the default time of 24h which can also be overwritten by the -bantime startup argument)"},

                    /*bsolute“，rpcarg:：type:：bool，/*opt*/true，/*default_val*/”false“，”如果设置，则bantime必须是自epoch（1970年1月1日，格林威治标准时间）以来的绝对时间戳（秒）”，
                }
                toSTRIN（）+
                            “\n实例：\n”
                            +helpExamplecli（“setban”，“192.168.0.6”，“add”，“86400”）。
                            +helpexamplecli（“setban”，“192.168.0.0/24”，“添加”）。
                            +helpexamplerpc（“setban”，“192.168.0.6”，“add”，“86400”）。
                            ；
    如果（！）吉康曼
        throw jsonrpcerror（rpc_client_p2p_disabled，“错误：缺少或禁用对等功能”）；

    子网子网；
    网络地址；
    bool issubnet=假；

    if（request.params[0].get_str（）.find（'/'）！=std：：字符串：：npos）
        issubnet=真；

    如果（！）iSub）{
        cnetaddr已解决；
        lookuphost（request.params[0].get_str（）.c_str（），resolved，false）；
        netaddr=已解决；
    }
    其他的
        lookupSubnet（request.params[0].get_str（）.c_str（），subnet）；

    如果（！）（ISSNET）？subnet.isvalid（）：netaddr.isvalid（））
        throw jsonrpcerror（rpc_client_invalid_ip_or_subnet，“错误：无效的ip/subnet”）；

    if（strCommand==“添加”）
    {
        如果（子网）？g_connman->isbanned（子网）：g_connman->isbanned（netaddr）
            throw jsonrpcerror（rpc_client_node_already_added，“错误：IP/子网已被禁止”）；

        int64_t bantime=0；//如果未指定，请使用标准bantime
        如果（！）请求.params[2].isNull（））
            bantime=request.params[2].get_int64（）；

        布尔绝对值=假；
        if（request.params[3].istrue（））
            绝对=真；

        ISSub？g_connman->ban（subnet，banreasonmanuallyadded，bantime，absolute）：g_connman->ban（netaddr，banreasonmanuallyadded，bantime，absolute）；
    }
    else if（strcommand==“删除”）
    {
        如果（！）（ISSNET）？g_connman->unban（子网）：g_connman->unban（netaddr）））
            throw jsonrpcerror（rpc_client_invalid_ip_or_subnet），错误：UNBAN失败。请求的地址/子网以前未被禁止。“）；
    }
    返回nullunivalue；
}

静态单值列表禁止（const-jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 0）
        throw std:：runtime_错误（
            rpchelpman“列表被禁止”，
                “\n列出所有禁止的IP/子网。\n”，
                toSTRIN（）+
                            “\n实例：\n”
                            +helpExamplecli（“listBanned”，“”）
                            +helpexamplerpc（“listBanned”，“”）
                            ；

    如果（！）吉康曼
        throw jsonrpcerror（rpc_client_p2p_disabled，“错误：缺少或禁用对等功能”）；

    班图；
    gou connman->getbanned（banmap）；

    univalue bannedaddresses（univalue:：varr）；
    for（const auto&entry:banmap）
    {
        const cbanentry&banentry=entry.second；
        单值rec（单值：：vobj）；
        rec.pushkv（“地址”，entry.first.toString（））；
        rec.pushkv（“禁止使用”until“，banentry.nbanuntil）；
        rec.pushkv（“ban_created”，banentry.ncreateTime）；
        rec.pushkv（“ban_reason”，banentry.banreasonToString（））；

        bannedaddresses.push-back（rec）；
    }

    返回bannedaddresses；
}

静态单值清除禁止（const-jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 0）
        throw std:：runtime_错误（
            rpchelpman“明令禁止”，
                \n清除所有被禁止的IP。\n“，”
                toSTRIN（）+
                            “\n实例：\n”
                            +helpExamplecli（“ClearBanned”，“”）
                            +helpexamplerpc（“ClearBanned”，“”）
                            ；
    如果（！）吉康曼
        throw jsonrpcerror（rpc_client_p2p_disabled，“错误：缺少或禁用对等功能”）；

    g_connman->ClearBanned（）；

    返回nullunivalue；
}

静态单值setnetworkactive（const jsonrpcrequest&request）
{
    如果（request.fhelp request.params.size（）！= 1）{
        throw std:：runtime_错误（
            rpchelpman“setnetworkactive”，
                \n禁用/启用所有P2P网络活动。\n“，
                {
                    “状态”，rpcarg:：type:：bool，/*opt*/ false, /* default_val */ "", "true to enable networking, false to disable"},

                }}
                .ToString()
        );
    }

    if (!g_connman) {
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");
    }

    g_connman->SetNetworkActive(request.params[0].get_bool());

    return g_connman->GetNetworkActive();
}

static UniValue getnodeaddresses(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1) {
        throw std::runtime_error(
            RPCHelpMan{"getnodeaddresses",
                "\nReturn known addresses which can potentially be used to find new nodes in the network\n",
                {
                    /*count“，rpcarg:：type:：num，/*opt*/true，/*default_val*/”1“，返回多少地址。限于所有已知地址的“+std:：to_string（addrman_getaddr_max）+”或“+std:：to_string（addrman_getaddr_max_pct）”+%中的较小者。“，
                }
                toSTRIN（）+
            “\NESRES:\N”
            “\n”
            “{n”
            ““时间”：ttt，（数字）从epoch（1970年1月1日，格林威治标准时间）起以秒为单位的时间戳，跟踪上次看到节点的时间\n”
            “服务\”：n，（数字）提供的服务\n”
            “\”地址\“：\”主机\“，（字符串）节点的地址\n”
            “端口\”：n（数字）节点的端口\n”
            “}\n”
            “，……”
            “\n”
            “\n实例：\n”
            +helpExamplecli（“getnodeadresss”，“8”）。
            +helpExampleRpc（“getNodeAddresses”，“8”）。
        ；
    }
    如果（！）G-康曼）
        throw jsonrpcerror（rpc_client_p2p_disabled，“错误：缺少或禁用对等功能”）；
    }

    int计数＝1；
    如果（！）request.params[0].isNull（））
        count=request.params[0].get_int（）；
        如果（计数<=0）
            throw jsonrpcerror（rpc_invalid_参数，“地址计数超出范围”）；
        }
    }
    //返回无序排列的caddress列表
    std:：vector<caddress>vaddr=g_connman->getaddresses（）；
    单值ret（单值：：varr）；

    int address_return_count=std:：min<int>（count，vaddr.size（））；
    对于（int i=0；i<address_return_count；++i）
        单值对象（单值：：vobj）；
        const caddress&addr=vaddr[i]；
        obj.pushkv（“时间”，（int）地址ntime）；
        obj.pushkv（“服务”，（uint64-t）地址服务）；
        obj.pushkv（“地址”，addr.toStringIP（））；
        obj.pushkv（“端口”，addr.getport（））；
        后退（obj）；
    }
    返回RET；
}

//关闭clang格式
静态const crpccommand命令[]
//类别名称actor（function）argnames
  ///————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    “网络”，“getConnectionCount”，&getConnectionCount，，
    “网络”，“Ping”，&Ping，，
    “network”，“getpeerinfo”，&getpeerinfo，，
    “network”，“addnode”，&addnode，“node”，“command”，
    “network”，“disconnectnode”，&disconnectnode，“address”，“nodeid”，
    “网络”，“getaddednodeinfo”，&getaddednodeinfo，“节点”，
    “网络”，“GetNettotals”，&GetNettotals，，
    “network”，“getnetworkinfo”，&getnetworkinfo，，
    “network”，“setban”，&setban，“subnet”，“command”，“bantime”，“absolute”，
    “network”，“listbanken”，&listbanken，，
    “network”，“clearbunned”，&clearbunned，，
    “network”，“setnetworkactive”，&setnetworkactive，“state”，
    “network”，“getnodeadresss”，&getnodeadresss，“count”，
}；
//打开Clang格式



    for（unsigned int vcidx=0；vcidx<arraylen（commands）；vcidx++）
        t.appendcommand（命令[vcidx].name，&commands[vcidx]）；
}
