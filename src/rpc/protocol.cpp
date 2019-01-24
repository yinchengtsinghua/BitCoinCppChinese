
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <rpc/protocol.h>

#include <random.h>
#include <tinyformat.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <util/time.h>
#include <version.h>

/*
 *JSON-RPC协议。比特币语音版本1.0可实现最大兼容性，
 *但对于1.0标准的部分，使用JSON-RPC 1.1/2.0标准
 *未指定（HTTP错误和“错误”的内容）。
 *
 *1.0规范：http://json-rpc.org/wiki/specification
 *1.2规格：http://json rpc.org/historical/json-rpc-over-http.html
 **/


UniValue JSONRPCRequestObj(const std::string& strMethod, const UniValue& params, const UniValue& id)
{
    UniValue request(UniValue::VOBJ);
    request.pushKV("method", strMethod);
    request.pushKV("params", params);
    request.pushKV("id", id);
    return request;
}

UniValue JSONRPCReplyObj(const UniValue& result, const UniValue& error, const UniValue& id)
{
    UniValue reply(UniValue::VOBJ);
    if (!error.isNull())
        reply.pushKV("result", NullUniValue);
    else
        reply.pushKV("result", result);
    reply.pushKV("error", error);
    reply.pushKV("id", id);
    return reply;
}

std::string JSONRPCReply(const UniValue& result, const UniValue& error, const UniValue& id)
{
    UniValue reply = JSONRPCReplyObj(result, error, id);
    return reply.write() + "\n";
}

UniValue JSONRPCError(int code, const std::string& message)
{
    UniValue error(UniValue::VOBJ);
    error.pushKV("code", code);
    error.pushKV("message", message);
    return error;
}

/*使用cookie身份验证时使用的用户名（任意，仅用于
 *调试/日志记录的可识别性）
 **/

static const std::string COOKIEAUTH_USER = "__cookie__";
/*身份验证cookie文件的默认名称*/
static const std::string COOKIEAUTH_FILE = ".cookie";

/*获取RPC身份验证cookie文件的名称*/
static fs::path GetAuthCookieFile(bool temp=false)
{
    std::string arg = gArgs.GetArg("-rpccookiefile", COOKIEAUTH_FILE);
    if (temp) {
        arg += ".tmp";
    }
    return AbsPathForConfigVal(fs::path(arg));
}

bool GenerateAuthCookie(std::string *cookie_out)
{
    const size_t COOKIE_SIZE = 32;
    unsigned char rand_pwd[COOKIE_SIZE];
    GetRandBytes(rand_pwd, COOKIE_SIZE);
    std::string cookie = COOKIEAUTH_USER + ":" + HexStr(rand_pwd, rand_pwd+COOKIE_SIZE);

    /*umask确定用于创建此文件的权限-
     *除非用-sysperms重写，否则在init.cpp中将这些设置为077。
     **/

    fsbridge::ofstream file;
    fs::path filepath_tmp = GetAuthCookieFile(true);
    file.open(filepath_tmp);
    if (!file.is_open()) {
        LogPrintf("Unable to open cookie authentication file %s for writing\n", filepath_tmp.string());
        return false;
    }
    file << cookie;
    file.close();

    fs::path filepath = GetAuthCookieFile(false);
    if (!RenameOver(filepath_tmp, filepath)) {
        LogPrintf("Unable to rename cookie authentication file %s to %s\n", filepath_tmp.string(), filepath.string());
        return false;
    }
    LogPrintf("Generated RPC authentication cookie %s\n", filepath.string());

    if (cookie_out)
        *cookie_out = cookie;
    return true;
}

bool GetAuthCookie(std::string *cookie_out)
{
    fsbridge::ifstream file;
    std::string cookie;
    fs::path filepath = GetAuthCookieFile();
    file.open(filepath);
    if (!file.is_open())
        return false;
    std::getline(file, cookie);
    file.close();

    if (cookie_out)
        *cookie_out = cookie;
    return true;
}

void DeleteAuthCookie()
{
    try {
        fs::remove(GetAuthCookieFile());
    } catch (const fs::filesystem_error& e) {
        LogPrintf("%s: Unable to remove random auth cookie file: %s\n", __func__, fsbridge::get_filesystem_error_message(e));
    }
}

std::vector<UniValue> JSONRPCProcessBatchReply(const UniValue &in, size_t num)
{
    if (!in.isArray()) {
        throw std::runtime_error("Batch must be an array");
    }
    std::vector<UniValue> batch(num);
    for (size_t i=0; i<in.size(); ++i) {
        const UniValue &rec = in[i];
        if (!rec.isObject()) {
            throw std::runtime_error("Batch member must be object");
        }
        size_t id = rec["id"].get_int();
        if (id >= num) {
            throw std::runtime_error("Batch member id larger than size");
        }
        batch[id] = rec;
    }
    return batch;
}
