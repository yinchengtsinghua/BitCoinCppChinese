
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

#include <clientversion.h>

#include <tinyformat.h>


/*
 *在“版本”消息中报告的客户端名称。报告相同的名称
 *对于比特币和比特币qt，使攻击者更难
 *特别是目标服务器或GUI用户。
 **/

const std::string CLIENT_NAME("Satoshi");

/*
 *客户端版本号
 **/

#define CLIENT_VERSION_SUFFIX ""


/*
 *代码的以下部分决定了客户机构建变量。
 *为此，使用了几种机制：
 **首先，如果定义了“有构建”信息，请包括build.h，即
 *由构建环境生成，可能包含输出
 *在名为build_desc的宏中描述
 **其次，如果这是代码的导出版本，Git_存档将
 *被定义（自动使用export subst git属性），以及
 *git_commit将包含commit id。
 **然后，存在三个用于确定客户机构建的选项：
 **如果定义了build_desc，则按字面意思使用（git的输出描述）
 **如果没有，但定义了git_commit，请使用v[maj]。[min]。[rev]。[build]-g[commit]
 **否则，使用v[maj]。[min]。[rev]。[build]-unk
 *最后添加客户端版本后缀
 **/


//！首先，如果需要，包括build.h
#ifdef HAVE_BUILD_INFO
#include <obj/build.h>
#endif

//！Git将在档案中的下一行放置“define Git_archive 1”。$格式：%n定义git_存档1$
#ifdef GIT_ARCHIVE
#define GIT_COMMIT_ID "$Format:%H$"
#define GIT_COMMIT_DATE "$Format:%cD$"
#endif

#define BUILD_DESC_WITH_SUFFIX(maj, min, rev, build, suffix) \
    "v" DO_STRINGIZE(maj) "." DO_STRINGIZE(min) "." DO_STRINGIZE(rev) "." DO_STRINGIZE(build) "-" DO_STRINGIZE(suffix)

#define BUILD_DESC_FROM_COMMIT(maj, min, rev, build, commit) \
    "v" DO_STRINGIZE(maj) "." DO_STRINGIZE(min) "." DO_STRINGIZE(rev) "." DO_STRINGIZE(build) "-g" commit

#define BUILD_DESC_FROM_UNKNOWN(maj, min, rev, build) \
    "v" DO_STRINGIZE(maj) "." DO_STRINGIZE(min) "." DO_STRINGIZE(rev) "." DO_STRINGIZE(build) "-unk"

#ifndef BUILD_DESC
#ifdef BUILD_SUFFIX
#define BUILD_DESC BUILD_DESC_WITH_SUFFIX(CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR, CLIENT_VERSION_REVISION, CLIENT_VERSION_BUILD, BUILD_SUFFIX)
#elif defined(GIT_COMMIT_ID)
#define BUILD_DESC BUILD_DESC_FROM_COMMIT(CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR, CLIENT_VERSION_REVISION, CLIENT_VERSION_BUILD, GIT_COMMIT_ID)
#else
#define BUILD_DESC BUILD_DESC_FROM_UNKNOWN(CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR, CLIENT_VERSION_REVISION, CLIENT_VERSION_BUILD)
#endif
#endif

const std::string CLIENT_BUILD(BUILD_DESC CLIENT_VERSION_SUFFIX);

static std::string FormatVersion(int nVersion)
{
    if (nVersion % 100 == 0)
        return strprintf("%d.%d.%d", nVersion / 1000000, (nVersion / 10000) % 100, (nVersion / 100) % 100);
    else
        return strprintf("%d.%d.%d.%d", nVersion / 1000000, (nVersion / 10000) % 100, (nVersion / 100) % 100, nVersion % 100);
}

std::string FormatFullVersion()
{
    return CLIENT_BUILD;
}

/*
 *根据BIP 14规范格式化Subversion字段（https://github.com/bitcoin/bips/blob/master/bip-0014.mediawiki）
 **/

std::string FormatSubVersion(const std::string& name, int nClientVersion, const std::vector<std::string>& comments)
{
    std::ostringstream ss;
    ss << "/";
    ss << name << ":" << FormatVersion(nClientVersion);
    if (!comments.empty())
    {
        std::vector<std::string>::const_iterator it(comments.begin());
        ss << "(" << *it;
        for(++it; it != comments.end(); ++it)
            ss << "; " << *it;
        ss << ")";
    }
    ss << "/";
    return ss.str();
}
