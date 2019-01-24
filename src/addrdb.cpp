
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <addrdb.h>

#include <addrman.h>
#include <chainparams.h>
#include <clientversion.h>
#include <hash.h>
#include <random.h>
#include <streams.h>
#include <tinyformat.h>
#include <util/system.h>

namespace {

template <typename Stream, typename Data>
bool SerializeDB(Stream& stream, const Data& data)
{
//写入并提交头、数据
    try {
        CHashWriter hasher(SER_DISK, CLIENT_VERSION);
        stream << Params().MessageStart() << data;
        hasher << Params().MessageStart() << data;
        stream << hasher.GetHash();
    } catch (const std::exception& e) {
        return error("%s: Serialize or I/O error - %s", __func__, e.what());
    }

    return true;
}

template <typename Data>
bool SerializeFileDB(const std::string& prefix, const fs::path& path, const Data& data)
{
//生成随机临时文件名
    unsigned short randv = 0;
    GetRandBytes((unsigned char*)&randv, sizeof(randv));
    std::string tmpfn = strprintf("%s.%04x", prefix, randv);

//打开临时输出文件，并与cautofile关联
    fs::path pathTmp = GetDataDir() / tmpfn;
    FILE *file = fsbridge::fopen(pathTmp, "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s: Failed to open file %s", __func__, pathTmp.string());

//串行化
    if (!SerializeDB(fileout, data)) return false;
    if (!FileCommit(fileout.Get()))
        return error("%s: Failed to flush file %s", __func__, pathTmp.string());
    fileout.fclose();

//用新文件替换现有文件（如果有）
    if (!RenameOver(pathTmp, path))
        return error("%s: Rename-into-place failed", __func__);

    return true;
}

template <typename Stream, typename Data>
bool DeserializeDB(Stream& stream, Data& data, bool fCheckSum = true)
{
    try {
        CHashVerifier<Stream> verifier(&stream);
//反序列化文件头（特定于网络的幻数）和..
        unsigned char pchMsgTmp[4];
        verifier >> pchMsgTmp;
//…验证网络是否符合我们的
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp)))
            return error("%s: Invalid network magic number", __func__);

//反序列化数据
        verifier >> data;

//校验校验和
        if (fCheckSum) {
            uint256 hashTmp;
            stream >> hashTmp;
            if (hashTmp != verifier.GetHash()) {
                return error("%s: Checksum mismatch, data corrupted", __func__);
            }
        }
    }
    catch (const std::exception& e) {
        return error("%s: Deserialize or I/O error - %s", __func__, e.what());
    }

    return true;
}

template <typename Data>
bool DeserializeFileDB(const fs::path& path, Data& data)
{
//打开输入文件，并与cautofile关联
    FILE *file = fsbridge::fopen(path, "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("%s: Failed to open file %s", __func__, path.string());

    return DeserializeDB(filein, data);
}

}

CBanDB::CBanDB()
{
    pathBanlist = GetDataDir() / "banlist.dat";
}

bool CBanDB::Write(const banmap_t& banSet)
{
    return SerializeFileDB("banlist", pathBanlist, banSet);
}

bool CBanDB::Read(banmap_t& banSet)
{
    return DeserializeFileDB(pathBanlist, banSet);
}

CAddrDB::CAddrDB()
{
    pathAddr = GetDataDir() / "peers.dat";
}

bool CAddrDB::Write(const CAddrMan& addr)
{
    return SerializeFileDB("peers", pathAddr, addr);
}

bool CAddrDB::Read(CAddrMan& addr)
{
    return DeserializeFileDB(pathAddr, addr);
}

bool CAddrDB::Read(CAddrMan& addr, CDataStream& ssPeers)
{
    bool ret = DeserializeDB(ssPeers, addr, false);
    if (!ret) {
//确保addrman处于干净状态
        addr.Clear();
    }
    return ret;
}
