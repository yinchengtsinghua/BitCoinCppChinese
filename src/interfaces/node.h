
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

#ifndef BITCOIN_INTERFACES_NODE_H
#define BITCOIN_INTERFACES_NODE_H

#include <addrdb.h>     //班纳默特
#include <amount.h>     //为金钱
#include <net.h>        //对于cconnman:：numConnections
#include <netaddress.h> //为网络

#include <functional>
#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <tuple>
#include <vector>

class CCoinControl;
class CFeeRate;
class CNodeStats;
class Coin;
class RPCTimerInterface;
class UniValue;
class proxyType;
struct CNodeStateStats;

namespace interfaces {
class Handler;
class Wallet;

//！比特币节点的顶级接口（比特币进程）。
class Node
{
public:
    virtual ~Node() {}

//！设置命令行参数。
    virtual bool parseParameters(int argc, const char* const argv[], std::string& error) = 0;

//！如果命令行参数没有值，则设置该参数
    virtual bool softSetArg(const std::string& arg, const std::string& value) = 0;

//！如果命令行布尔参数还没有值，则设置该参数
    virtual bool softSetBoolArg(const std::string& arg, bool value) = 0;

//！从配置文件加载设置。
    virtual bool readConfigFiles(std::string& error) = 0;

//！选择网络参数。
    virtual void selectParams(const std::string& network) = 0;

//！获取（假定的）区块链大小。
    virtual uint64_t getAssumedBlockchainSize() = 0;

//！获取（假定的）链状态大小。
    virtual uint64_t getAssumedChainStateSize() = 0;

//！获取网络名称。
    virtual std::string getNetwork() = 0;

//！初始化日志。
    virtual void initLogging() = 0;

//！初始化参数交互。
    virtual void initParameterInteraction() = 0;

//！得到警告。
    virtual std::string getWarnings(const std::string& type) = 0;

//获取日志标志。
    virtual uint32_t getLogCategories() = 0;

//！初始化应用程序依赖项。
    virtual bool baseInitialize() = 0;

//！启动节点。
    virtual bool appInitMain() = 0;

//！停止节点。
    virtual void appShutdown() = 0;

//！开始关机。
    virtual void startShutdown() = 0;

//！返回是否请求关闭。
    virtual bool shutdownRequested() = 0;

//！设置参数
    virtual void setupServerArgs() = 0;

//！映射端口。
    virtual void mapPort(bool use_upnp) = 0;

//！获取代理。
    virtual bool getProxy(Network net, proxyType& proxy_info) = 0;

//！获取连接数。
    virtual size_t getNodeCount(CConnman::NumConnections flags) = 0;

//！获取已连接节点的统计信息。
    using NodesStats = std::vector<std::tuple<CNodeStats, bool, CNodeStateStats>>;
    virtual bool getNodesStats(NodesStats& stats) = 0;

//！获取禁令地图条目。
    virtual bool getBanned(banmap_t& banmap) = 0;

//！禁止节点。
    virtual bool ban(const CNetAddr& net_addr, BanReason reason, int64_t ban_time_offset) = 0;

//！UNBAN节点。
    virtual bool unban(const CSubNet& ip) = 0;

//！断开节点连接。
    virtual bool disconnect(NodeId id) = 0;

//！获取接收的总字节数。
    virtual int64_t getTotalBytesRecv() = 0;

//！获取发送的总字节数。
    virtual int64_t getTotalBytesSent() = 0;

//！获取内存池大小。
    virtual size_t getMempoolSize() = 0;

//！获取mempool动态用法。
    virtual size_t getMempoolDynamicUsage() = 0;

//！获取收割台顶部高度和时间。
    virtual bool getHeaderTip(int& height, int64_t& block_time) = 0;

//！得到Num块。
    virtual int getNumBlocks() = 0;

//！获取最后一个阻止时间。
    virtual int64_t getLastBlockTime() = 0;

//！获取验证进度。
    virtual double getVerificationProgress() = 0;

//！是初始块下载。
    virtual bool isInitialBlockDownload() = 0;

//！获取重新索引。
    virtual bool getReindex() = 0;

//！获取导入。
    virtual bool getImporting() = 0;

//！将网络设置为活动。
    virtual void setNetworkActive(bool active) = 0;

//！使网络处于活动状态。
    virtual bool getNetworkActive() = 0;

//！获取最大TX费用。
    virtual CAmount getMaxTxFee() = 0;

//！估计智能费用。
    virtual CFeeRate estimateSmartFee(int num_blocks, bool conservative, int* returned_target = nullptr) = 0;

//！收取粉尘继电器费。
    virtual CFeeRate getDustRelayFee() = 0;

//！执行rpc命令。
    virtual UniValue executeRpc(const std::string& command, const UniValue& params, const std::string& uri) = 0;

//！列出rpc命令。
    virtual std::vector<std::string> listRpcCommands() = 0;

//！如果不设置，则设置RPC计时器接口。
    virtual void rpcSetTimerInterfaceIfUnset(RPCTimerInterface* iface) = 0;

//！取消设置RPC计时器接口。
    virtual void rpcUnsetTimerInterface(RPCTimerInterface* iface) = 0;

//！获取与事务关联的未暂停输出。
    virtual bool getUnspentOutput(const COutPoint& output, Coin& coin) = 0;

//！返回默认钱包目录。
    virtual std::string getWalletDir() = 0;

//！返回钱包目录中的可用钱包。
    virtual std::vector<std::string> listWalletDir() = 0;

//！返回用于访问钱包的接口（如果有）。
    virtual std::vector<std::unique_ptr<Wallet>> getWallets() = 0;

//！为init消息注册处理程序。
    using InitMessageFn = std::function<void(const std::string& message)>;
    virtual std::unique_ptr<Handler> handleInitMessage(InitMessageFn fn) = 0;

//！注册消息框消息的处理程序。
    using MessageBoxFn =
        std::function<bool(const std::string& message, const std::string& caption, unsigned int style)>;
    virtual std::unique_ptr<Handler> handleMessageBox(MessageBoxFn fn) = 0;

//！注册问题消息的处理程序。
    using QuestionFn = std::function<bool(const std::string& message,
        const std::string& non_interactive_message,
        const std::string& caption,
        unsigned int style)>;
    virtual std::unique_ptr<Handler> handleQuestion(QuestionFn fn) = 0;

//！注册进度消息的处理程序。
    using ShowProgressFn = std::function<void(const std::string& title, int progress, bool resume_possible)>;
    virtual std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) = 0;

//！注册加载钱包消息的处理程序。
    using LoadWalletFn = std::function<void(std::unique_ptr<Wallet> wallet)>;
    virtual std::unique_ptr<Handler> handleLoadWallet(LoadWalletFn fn) = 0;

//！注册连接更改消息数的处理程序。
    using NotifyNumConnectionsChangedFn = std::function<void(int new_num_connections)>;
    virtual std::unique_ptr<Handler> handleNotifyNumConnectionsChanged(NotifyNumConnectionsChangedFn fn) = 0;

//！注册网络活动消息的处理程序。
    using NotifyNetworkActiveChangedFn = std::function<void(bool network_active)>;
    virtual std::unique_ptr<Handler> handleNotifyNetworkActiveChanged(NotifyNetworkActiveChangedFn fn) = 0;

//！注册通知警报消息的处理程序。
    using NotifyAlertChangedFn = std::function<void()>;
    virtual std::unique_ptr<Handler> handleNotifyAlertChanged(NotifyAlertChangedFn fn) = 0;

//！注册禁止列表消息的处理程序。
    using BannedListChangedFn = std::function<void()>;
    virtual std::unique_ptr<Handler> handleBannedListChanged(BannedListChangedFn fn) = 0;

//！为块提示消息注册处理程序。
    using NotifyBlockTipFn =
        std::function<void(bool initial_download, int height, int64_t block_time, double verification_progress)>;
    virtual std::unique_ptr<Handler> handleNotifyBlockTip(NotifyBlockTipFn fn) = 0;

//！为头提示消息注册处理程序。
    using NotifyHeaderTipFn =
        std::function<void(bool initial_download, int height, int64_t block_time, double verification_progress)>;
    virtual std::unique_ptr<Handler> handleNotifyHeaderTip(NotifyHeaderTipFn fn) = 0;
};

//！返回节点接口的实现。
std::unique_ptr<Node> MakeNode();

} //命名空间接口

#endif //比特币接口节点
