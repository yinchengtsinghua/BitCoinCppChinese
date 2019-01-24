
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

#ifndef BITCOIN_INTERFACES_HANDLER_H
#define BITCOIN_INTERFACES_HANDLER_H

#include <memory>

namespace boost {
namespace signals2 {
class connection;
} //命名空间信号2
} //命名空间提升

namespace interfaces {

//！用于管理事件处理程序或回调函数的通用接口
//！已注册到另一个接口。有一个要取消的断开连接方法
//！注册并阻止将来的任何通知。
class Handler
{
public:
    virtual ~Handler() {}

//！断开处理程序。
    virtual void disconnect() = 0;
};

//！返回处理程序包装一个增压信号连接。
std::unique_ptr<Handler> MakeHandler(boost::signals2::connection connection);

} //命名空间接口

#endif //比特币接口处理程序
