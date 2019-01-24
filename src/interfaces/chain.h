
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

#ifndef BITCOIN_INTERFACES_CHAIN_H
#define BITCOIN_INTERFACES_CHAIN_H

#include <memory>
#include <string>
#include <vector>

class CScheduler;

namespace interfaces {

//！允许钱包进程访问区块链状态的接口。
class Chain
{
public:
    virtual ~Chain() {}

//！用于查询锁定链状态的接口，由
//！假定两次调用之间的状态不变。新代码应避免使用
//！锁接口，而不是调用更高级的链方法
//！它会返回更多信息，这样链条就不需要保持锁定。
//！通话之间。
    class Lock
    {
    public:
        virtual ~Lock() {}
    };

//！返回锁定接口。调用时链被锁定，并且
//！释放返回的接口时解锁。
    virtual std::unique_ptr<Lock> lock(bool try_lock = false) = 0;

//！返回锁接口，假定链已锁定。这个
//！方法是临时的，只在少数地方使用，以避免更改
//！代码转换为使用chain:：lock接口时的行为。
    virtual std::unique_ptr<Lock> assumeLocked() = 0;
};

//！允许节点管理链客户机的接口（钱包或
//！未来的监测和分析）。
class ChainClient
{
public:
    virtual ~ChainClient() {}

//！注册RPC。
    virtual void registerRpcs() = 0;

//！加载前检查错误。
    virtual bool verify() = 0;

//！加载保存状态。
    virtual bool load() = 0;

//！启动客户端执行并提供调度程序。
    virtual void start(CScheduler& scheduler) = 0;

//！将状态保存到磁盘。
    virtual void flush() = 0;

//！关闭客户端。
    virtual void stop() = 0;
};

//！返回链接口的实现。
std::unique_ptr<Chain> MakeChain();

//！返回钱包客户端的ChainClient接口的实现。这个
//！在启用钱包为假的版本中，函数将未定义。
//！
//！目前，钱包是唯一的连锁客户。但在未来，其他
//！可以添加链客户机的类型，例如用于监视的工具，
//！分析或费用估算。这些客户需要公开自己的
//！makexxxclient函数返回其chainclient的实现
//！接口。
std::unique_ptr<ChainClient> MakeWalletClient(Chain& chain, std::vector<std::string> wallet_filenames);

} //命名空间接口

#endif //比特币接口链
