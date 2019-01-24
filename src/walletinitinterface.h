
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

#ifndef BITCOIN_WALLETINITINTERFACE_H
#define BITCOIN_WALLETINITINTERFACE_H

#include <string>

class CScheduler;
class CRPCTable;
struct InitInterfaces;

class WalletInitInterface {
public:
    /*钱包组件启用了吗*/
    virtual bool HasWalletSupport() const = 0;
    /*获取钱包帮助字符串*/
    virtual void AddWalletOptions() const = 0;
    /*检查钱包参数交互*/
    virtual bool ParameterInteraction() const = 0;
    /*将应打开的钱包添加到init接口列表中。*/
    virtual void Construct(InitInterfaces& interfaces) const = 0;

    virtual ~WalletInitInterface() {}
};

extern const WalletInitInterface& g_wallet_init_interface;

#endif //比特币沃勒蒂尼琴
