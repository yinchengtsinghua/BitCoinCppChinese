
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

#include <interfaces/chain.h>

#include <sync.h>
#include <util/system.h>
#include <validation.h>

#include <memory>
#include <utility>

namespace interfaces {
namespace {

class LockImpl : public Chain::Lock
{
};

class LockingStateImpl : public LockImpl, public UniqueLock<CCriticalSection>
{
    using UniqueLock::UniqueLock;
};

class ChainImpl : public Chain
{
public:
    std::unique_ptr<Chain::Lock> lock(bool try_lock) override
    {
        auto result = MakeUnique<LockingStateImpl>(::cs_main, "cs_main", __FILE__, __LINE__, try_lock);
        if (try_lock && result && !*result) return {};
//std：：由于从
//LockingStateImpl锁定指针
        return std::move(result);
    }
    std::unique_ptr<Chain::Lock> assumeLocked() override { return MakeUnique<LockImpl>(); }
};

} //命名空间

std::unique_ptr<Chain> MakeChain() { return MakeUnique<ChainImpl>(); }

} //命名空间接口
