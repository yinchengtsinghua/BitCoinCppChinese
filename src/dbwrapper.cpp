
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

#include <dbwrapper.h>

#include <memory>
#include <random.h>

#include <leveldb/cache.h>
#include <leveldb/env.h>
#include <leveldb/filter_policy.h>
#include <memenv.h>
#include <stdint.h>
#include <algorithm>

class CBitcoinLevelDBLogger : public leveldb::Logger {
public:
//此代码改编自posix_logger.h，这就是它使用vsprintf的原因。
//请不要用普通代码做这个
    void Logv(const char * format, va_list ap) override {
            if (!LogAcceptCategory(BCLog::LEVELDB)) {
                return;
            }
            char buffer[500];
            for (int iter = 0; iter < 2; iter++) {
                char* base;
                int bufsize;
                if (iter == 0) {
                    bufsize = sizeof(buffer);
                    base = buffer;
                }
                else {
                    bufsize = 30000;
                    base = new char[bufsize];
                }
                char* p = base;
                char* limit = base + bufsize;

//打印邮件
                if (p < limit) {
                    va_list backup_ap;
                    va_copy(backup_ap, ap);
//不要在比特币源代码的其他地方使用vsnprintf，请参见上文。
                    p += vsnprintf(p, limit - p, format, backup_ap);
                    va_end(backup_ap);
                }

//必要时截断到可用空间
                if (p >= limit) {
                    if (iter == 0) {
continue;       //使用较大的缓冲区重试
                    }
                    else {
                        p = limit - 1;
                    }
                }

//必要时添加换行符
                if (p == base || p[-1] != '\n') {
                    *p++ = '\n';
                }

                assert(p <= limit);
                base[std::min(bufsize - 1, (int)(p - base))] = '\0';
                /*printf（“leveldb:%s”，base）；/*继续*/
                如果（基地）！=缓冲器）{
                    删除[]基；
                }
                断裂；
            }
    }
}；

静态void setmaxopenfiles（leveldb:：options*选项）
    //在大多数平台上，max_open_文件的默认设置（为1000）
    //是最优的。在Windows上使用大文件计数是正常的，因为句柄
    //不要干扰select（）循环。在64位UNIX主机上，此值为
    //也可以，因为达到这个级别，ldb将使用mmap
    //不使用额外文件描述符的实现（fds是
    //被命令后关闭）。
    / /
    //将值增加到默认值之外是危险的，因为leveldb将
    //当文件计数太大时，返回到非mmap实现。
    //在32位UNIX主机上，我们应该减小该值，因为句柄使用
    //up real fds，我们希望避免fd耗尽问题。
    / /
    //请参阅pr 12495进一步讨论。

    int default_open_files=options->max_open_files；
iNIFDEF Win32
    if（sizeof（void*）<8）
        选项->max_open_files=64；
    }
第二节
    logprint（bclog:：leveldb，“使用max_open_文件的leveldb=%d（默认值为%d））”，
             选项->max_open_files，默认_open_files）；
}

静态级别db：：选项getoptions（大小ncachesize）
{
    LEVELDB：：选项；
    options.block_cache=leveldb:：newlrucache（ncachesize/2）；
    options.write_buffer_size=ncachesize/4；//内存中最多可以同时保存两个写缓冲区
    options.filter_policy=leveldb:：newbloomfilterpolicy（10）；
    options.compression=leveldb:：knocompression；
    options.info_log=new cbitcoinleveldbogler（）；
    if（leveldb:：kmajorversion>1（leveldb:：kmajorversion==1&&leveldb:：kminorversion>=16））
        //1.16之前的LevelDB版本将短写视为损坏。仅触发错误
        //在更高版本中损坏。
        options.paranoid_checks=true；
    }
    setmaxopenfiles（&options）；
    返回选项；
}

cdbwrapper:：cdbwrapper（const fs:：path&path，size_cachesize，bool fmemory，bool fwipe，bool obfuscate）
    ：m_名称（fs:：basename（path））。
{
    PNV＝NulLPTR；
    readoptions.verify_checksums=true；
    iteroptions.verify_checksums=true；
    iteroptions.fill_cache=false；
    syncOptions.sync=真；
    选项=获取选项（ncachesize）；
    options.create_if_missing=true；
    如果（f存储器）{
        penv=leveldb:：newmemenv（leveldb:：env:：default（））；
        options.env=penv；
    }否则{
        如果（FWEW）{
            logprintf（“擦除%s中的levelldb\n”，path.string（））；
            leveldb:：status result=leveldb:：destroydb（path.string（），选项）；
            dbwrapper_private:：handleerrror（result）；
        }
        Try创建目录（路径）；
        logprintf（“opening levelldb in%s\n”，path.string（））；
    }
    leveldb:：status status=leveldb:：db:：open（选项，path.string（），&pdb）；
    dbwrapper_private:：handleerrror（状态）；
    logprintf（“opened leveldb successfully \n”）；

    if（gargs.getboolarg（“-forcecompactdb”，false））
        logprintf（“开始压缩数据库%s\n”，path.string（））；
        pdb->compactrange（nullptr，nullptr）；
        logprintf（“已完成%s的数据库压缩\n”，path.string（））；
    }

    //基本情况混淆键，它是noop。
    obfuscate_key=std:：vector<unsigned char>（obfuscate_key_num_bytes，'\000'）；

    bool key_exists=read（混淆\u key_key，混淆\u key）；

    如果（！）key_exists&&obfuscate&&isEmpty（））
        //如果不破坏，初始化非退化模糊
        //现有的非模糊数据。
        std:：vector<unsigned char>new_key=createObfuscatekey（）；

        //编写“new_key”，这样我们就不会混淆密钥本身
        写入（混淆键、新键）；
        obfuscate_key=新_key；

        logprintf（“为%s写入新的模糊密钥：%s\n”，path.string（），hexstr（模糊密钥））；
    }

    logprintf（“对%s使用模糊键：%s\n”，path.string（），hexstr（模糊键））；
}

CDBWrapper:：~CDBWrapper（）。
{
    删除PDB；
    PDB＝NulLPTR；
    删除选项。过滤策略；
    options.filter_policy=nullptr；
    删除options.info_log；
    options.info_log=nullptr；
    delete options.block_缓存；
    options.block_cache=nullptr；
    删除PENV；
    options.env=空指针；
}

bool cdbwrapper:：writebatch（cdbbatch&batch，bool fsync）
{
    const bool log_memory=logacceptCategory（bclog:：leveldb）；
    double mem_before=0；
    if（日志存储器）
        mem_before=dynamicmemoryusage（）/1024.0/1024；
    }
    leveldb:：status status=pdb->write（fsync？同步选项：writeoptions，&batch.batch）；
    dbwrapper_private:：handleerrror（状态）；
    if（日志存储器）
        double mem_after=dynamicmemoryusage（）/1024.0/1024；
        logprint（bclog:：leveldb，“writebatch内存使用率：db=%s，before=%.1fmib，after=%.1fmib\n”，
                 名字，之前的记忆，之后的记忆；
    }
    回归真实；
}

size_t cdbwrapper:：dynamicmemoryusage（）const_
    std：：字符串内存；
    如果（！）pdb->getproperty（“leveldb.approximate memory usage”，&memory））
        logprint（bclog:：leveldb，“获取大致内存使用属性失败\n”）；
        返回0；
    }
    返回stoul（内存）；
}

//前缀为空字符以避免与其他键冲突
/ /
//必须使用指定长度的字符串构造函数，以便复制
//超过了空终止符。
const std:：string cdbwrapper:：obfuscate_key_key（“\000obfuscate_key”，14）；

const unsigned int cdbwrapper:：obfuscate_key_num_bytes=8；

/**
 *返回适合用作
 *混淆异或键。
 **/

std::vector<unsigned char> CDBWrapper::CreateObfuscateKey() const
{
    unsigned char buff[OBFUSCATE_KEY_NUM_BYTES];
    GetRandBytes(buff, OBFUSCATE_KEY_NUM_BYTES);
    return std::vector<unsigned char>(&buff[0], &buff[OBFUSCATE_KEY_NUM_BYTES]);

}

bool CDBWrapper::IsEmpty()
{
    std::unique_ptr<CDBIterator> it(NewIterator());
    it->SeekToFirst();
    return !(it->Valid());
}

CDBIterator::~CDBIterator() { delete piter; }
bool CDBIterator::Valid() const { return piter->Valid(); }
void CDBIterator::SeekToFirst() { piter->SeekToFirst(); }
void CDBIterator::Next() { piter->Next(); }

namespace dbwrapper_private {

void HandleError(const leveldb::Status& status)
{
    if (status.ok())
        return;
    const std::string errmsg = "Fatal LevelDB error: " + status.ToString();
    LogPrintf("%s\n", errmsg);
    LogPrintf("You can use -debug=leveldb to get more complete diagnostic messages\n");
    throw dbwrapper_error(errmsg);
}

const std::vector<unsigned char>& GetObfuscateKey(const CDBWrapper &w)
{
    return w.obfuscate_key;
}

} //命名空间dbwrapper_private
