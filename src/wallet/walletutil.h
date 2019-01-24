
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

#ifndef BITCOIN_WALLET_WALLETUTIL_H
#define BITCOIN_WALLET_WALLETUTIL_H

#include <fs.h>

#include <vector>

//！获取钱包目录的路径。
fs::path GetWalletDir();

//！把钱包放在钱包目录里。
std::vector<fs::path> ListWalletDir();

//！WalletLocation类提供钱包信息。
class WalletLocation final
{
    std::string m_name;
    fs::path m_path;

public:
    explicit WalletLocation() {}
    explicit WalletLocation(const std::string& name);

//！获取钱包名称。
    const std::string& GetName() const { return m_name; }

//！获取钱包绝对路径。
    const fs::path& GetPath() const { return m_path; }

//！返回钱包是否存在。
    bool Exists() const;
};

#endif //比特币钱包
