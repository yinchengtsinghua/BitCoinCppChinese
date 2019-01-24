
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

#include <wallet/walletutil.h>

#include <util/system.h>

fs::path GetWalletDir()
{
    fs::path path;

    if (gArgs.IsArgSet("-walletdir")) {
        path = gArgs.GetArg("-walletdir", "");
        if (!fs::is_directory(path)) {
//如果指定的路径不存在，我们会故意返回
//空字符串无效。
            path = "";
        }
    } else {
        path = GetDataDir();
//如果存在钱包目录，请使用该目录，否则默认为getdatadir
        if (fs::is_directory(path / "wallets")) {
            path /= "wallets";
        }
    }

    return path;
}

static bool IsBerkeleyBtree(const fs::path& path)
{
//伯克利数据库btree文件至少有4K。
//此检查还阻止打开锁定文件。
    boost::system::error_code ec;
    if (fs::file_size(path, ec) < 4096) return false;

    fs::ifstream file(path.string(), std::ios::binary);
    if (!file.is_open()) return false;

file.seekg(12, std::ios::beg); //魔法字节从偏移量12开始
    uint32_t data = 0;
file.read((char*) &data, sizeof(data)); //读取4个字节的文件与magic进行比较

//伯克利数据库btree magic bytes，来自：
//https://github.com/file/file/blob/5824af38469ec1ca9ac3fd251e7afe9dc11e277/magic/magdir/database l74-l75
//-大端系统-00 05 31 62
//-Little Endian系统-62 31 05 00
    return data == 0x00053162 || data == 0x62310500;
}

std::vector<fs::path> ListWalletDir()
{
    const fs::path wallet_dir = GetWalletDir();
    const size_t offset = wallet_dir.string().size() + 1;
    std::vector<fs::path> paths;

    for (auto it = fs::recursive_directory_iterator(wallet_dir); it != fs::recursive_directory_iterator(); ++it) {
//通过从Wallet路径中删除WalletDir，获取与WalletDir相关的Wallet路径。
//一旦boost被提升到1.60，就可以用boost:：filesystem:：lexically_relative来替换它。
        const fs::path path = it->path().string().substr(offset);

        if (it->status().type() == fs::directory_file && IsBerkeleyBtree(it->path() / "wallet.dat")) {
//找到一个包含wallet.dat btree文件的目录，将其添加为wallet。
            paths.emplace_back(path);
        } else if (it.level() == 0 && it->symlink_status().type() == fs::regular_file && IsBerkeleyBtree(it->path())) {
            if (it->path().filename() == "wallet.dat") {
//找到顶级wallet.dat btree文件，添加顶级目录“”。
//作为钱包。
                paths.emplace_back();
            } else {
//找到顶级btree文件，不称为wallet.dat。当前比特币
//软件不会创建这些文件，但会允许它们
//在共享数据库环境中打开以实现向后兼容性。
//将其添加到可用钱包列表中。
                paths.emplace_back(path);
            }
        }
    }

    return paths;
}

WalletLocation::WalletLocation(const std::string& name)
    : m_name(name)
    , m_path(fs::absolute(name, GetWalletDir()))
{
}

bool WalletLocation::Exists() const
{
    return fs::symlink_status(m_path).type() != fs::file_not_found;
}
