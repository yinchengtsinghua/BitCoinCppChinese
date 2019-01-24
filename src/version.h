
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2012-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_VERSION_H
#define BITCOIN_VERSION_H

/*
 *网络协议版本控制
 **/


static const int PROTOCOL_VERSION = 70015;

//！初始协议版本，在版本/verack协商后增加
static const int INIT_PROTO_VERSION = 209;

//！在此版本中，引入了“getheaders”。
static const int GETHEADERS_VERSION = 31800;

//！断开与早于此协议版本的对等机的连接
static const int MIN_PEER_PROTO_VERSION = GETHEADERS_VERSION;

//！ntime字段添加到caddress中，从这个版本开始；
//！如果可能，请避免请求早于此时间的地址节点
static const int CADDR_TIME_VERSION = 31402;

//！BIP 0031，pong message，在此版本之后的所有版本都启用
static const int BIP0031_VERSION = 60000;

//“filter*“命令在此版本之后（包括此版本）禁用，但不显示node_bloom
static const int NO_BLOOM_VERSION = 70011;

//“sendheaders“命令和带有headers的公告块从这个版本开始
static const int SENDHEADERS_VERSION = 70012;

//“fee filter“告诉同行从这个版本开始按费用筛选投资
static const int FEEFILTER_VERSION = 70013;

//！基于ID的短块下载从这个版本开始
static const int SHORT_IDS_BLOCKS_VERSION = 70014;

//！对于无效的压缩块不禁止从该版本开始
static const int INVALID_CB_NO_BAN_VERSION = 70015;

#endif //比特币版本
