
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2015-2018比特币核心开发商
//版权所有（c）2017 Zcash开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <torcontrol.h>
#include <util/strencodings.h>
#include <netbase.h>
#include <net.h>
#include <util/system.h>
#include <crypto/hmac_sha256.h>

#include <vector>
#include <deque>
#include <set>
#include <stdlib.h>

#include <boost/signals2/signal.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/thread.h>

/*默认控制端口*/
const std::string DEFAULT_TOR_CONTROL = "127.0.0.1:9051";
/*Tor cookie大小（来自control-spec.txt）*/
static const int TOR_COOKIE_SIZE = 32;
/*safecookie的客户端/服务器nonce大小*/
static const int TOR_NONCE_SIZE = 32;
/*用于计算safecookie中的serverhash*/
static const std::string TOR_SAFE_SERVERKEY = "Tor safe cookie authentication server-to-controller hash";
/*用于计算safecookie中的clienthash*/
static const std::string TOR_SAFE_CLIENTKEY = "Tor safe cookie authentication controller-to-server hash";
/*指数退避配置-初始超时（秒）*/
static const float RECONNECT_TIMEOUT_START = 1.0;
/*指数退避配置-增长因子*/
static const float RECONNECT_TIMEOUT_EXP = 1.5;
/*TorControlConnection上接收的线路的最大长度。
 *tor-control-spec.txt中提到，没有明确定义线条长度的限制，
 *这是安全带和吊带的健全限制，以防止记忆耗尽。
 **/

static const int MAX_LINE_LENGTH = 100000;

/*****低电平TorControlConnection******/

/*来自Tor的答复，可以是单行或多行*/
class TorControlReply
{
public:
    TorControlReply() { Clear(); }

    int code;
    std::vector<std::string> lines;

    void Clear()
    {
        code = 0;
        lines.clear();
    }
};

/*Tor控制连接的低水平处理。
 *使用Torspec/control-spec.txt中定义的类似于SMTP的协议
 **/

class TorControlConnection
{
public:
    typedef std::function<void(TorControlConnection&)> ConnectionCB;
    typedef std::function<void(TorControlConnection &,const TorControlReply &)> ReplyHandlerCB;

    /*创建新的TorControlConnection。
     **/

    explicit TorControlConnection(struct event_base *base);
    ~TorControlConnection();

    /*
     *连接至Tor控制端口。
     *目标是窗体host:port的地址。
     *Connected是成功建立连接时调用的处理程序。
     *disconnected是连接断开时调用的处理程序。
     *成功后返回true。
     **/

    bool Connect(const std::string &target, const ConnectionCB& connected, const ConnectionCB& disconnected);

    /*
     *从Tor控制端口断开。
     **/

    void Disconnect();

    /*发送一个命令，为回复注册一个处理程序。
     *自动添加尾随的CRLF。
     *成功后返回true。
     **/

    bool Command(const std::string &cmd, const ReplyHandlerCB& reply_handler);

    /*异步响应的响应处理程序*/
    boost::signals2::signal<void(TorControlConnection &,const TorControlReply &)> async_handler;
private:
    /*准备使用时回调*/
    std::function<void(TorControlConnection&)> connected;
    /*连接断开时回调*/
    std::function<void(TorControlConnection&)> disconnected;
    /*libevent事件基*/
    struct event_base *base;
    /*连接到控制插座*/
    struct bufferevent *b_conn;
    /*正在接收的消息*/
    TorControlReply message;
    /*响应处理程序*/
    std::deque<ReplyHandlerCB> reply_handlers;

    /*libevent处理程序：内部*/
    static void readcb(struct bufferevent *bev, void *ctx);
    static void eventcb(struct bufferevent *bev, short what, void *ctx);
};

TorControlConnection::TorControlConnection(struct event_base *_base):
    base(_base), b_conn(nullptr)
{
}

TorControlConnection::~TorControlConnection()
{
    if (b_conn)
        bufferevent_free(b_conn);
}

void TorControlConnection::readcb(struct bufferevent *bev, void *ctx)
{
    TorControlConnection *self = static_cast<TorControlConnection*>(ctx);
    struct evbuffer *input = bufferevent_get_input(bev);
    size_t n_read_out = 0;
    char *line;
    assert(input);
//如果没有整行要读取，则evbuffer_readln返回nullptr
    while((line = evbuffer_readln(input, &n_read_out, EVBUFFER_EOL_CRLF)) != nullptr)
    {
        std::string s(line, n_read_out);
        free(line);
if (s.size() < 4) //短线
            continue;
//<status>（-+）<data><crlf>
        self->message.code = atoi(s.substr(0,3));
        self->message.lines.push_back(s.substr(4));
char ch = s[3]; //“-”、“+”或“”
        if (ch == ' ') {
//终线、调度回复、清理
            if (self->message.code >= 600) {
//将异步通知分派到异步处理程序
//同步和异步消息从不交错
                self->async_handler(*self, self->message);
            } else {
                if (!self->reply_handlers.empty()) {
//使用消息调用答复处理程序
                    self->reply_handlers.front()(*self, self->message);
                    self->reply_handlers.pop_front();
                } else {
                    LogPrint(BCLog::TOR, "tor: Received unexpected sync reply %i\n", self->message.code);
                }
            }
            self->message.Clear();
        }
    }
//检查缓冲区的大小-用很长的行来防止内存耗尽
//在evbuffer_readln之后执行此操作，以确保所有完整行
//从缓冲器中取出。剩下的都是一条不完整的线。
    if (evbuffer_get_length(input) > MAX_LINE_LENGTH) {
        LogPrintf("tor: Disconnecting because MAX_LINE_LENGTH exceeded\n");
        self->Disconnect();
    }
}

void TorControlConnection::eventcb(struct bufferevent *bev, short what, void *ctx)
{
    TorControlConnection *self = static_cast<TorControlConnection*>(ctx);
    if (what & BEV_EVENT_CONNECTED) {
        LogPrint(BCLog::TOR, "tor: Successfully connected!\n");
        self->connected(*self);
    } else if (what & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
        if (what & BEV_EVENT_ERROR) {
            LogPrint(BCLog::TOR, "tor: Error connecting to Tor control socket\n");
        } else {
            LogPrint(BCLog::TOR, "tor: End of stream\n");
        }
        self->Disconnect();
        self->disconnected(*self);
    }
}

bool TorControlConnection::Connect(const std::string &target, const ConnectionCB& _connected, const ConnectionCB&  _disconnected)
{
    if (b_conn)
        Disconnect();
//分析目标地址：端口
    struct sockaddr_storage connect_to_addr;
    int connect_to_addrlen = sizeof(connect_to_addr);
    if (evutil_parse_sockaddr_port(target.c_str(),
        (struct sockaddr*)&connect_to_addr, &connect_to_addrlen)<0) {
        LogPrintf("tor: Error parsing socket address %s\n", target);
        return false;
    }

//创建新的套接字，设置回调并启用通知位
    b_conn = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
    if (!b_conn)
        return false;
    bufferevent_setcb(b_conn, TorControlConnection::readcb, nullptr, TorControlConnection::eventcb, this);
    bufferevent_enable(b_conn, EV_READ|EV_WRITE);
    this->connected = _connected;
    this->disconnected = _disconnected;

//最后，连接到目标
    if (bufferevent_socket_connect(b_conn, (struct sockaddr*)&connect_to_addr, connect_to_addrlen) < 0) {
        LogPrintf("tor: Error connecting to address %s\n", target);
        return false;
    }
    return true;
}

void TorControlConnection::Disconnect()
{
    if (b_conn)
        bufferevent_free(b_conn);
    b_conn = nullptr;
}

bool TorControlConnection::Command(const std::string &cmd, const ReplyHandlerCB& reply_handler)
{
    if (!b_conn)
        return false;
    struct evbuffer *buf = bufferevent_get_output(b_conn);
    if (!buf)
        return false;
    evbuffer_add(buf, cmd.data(), cmd.size());
    evbuffer_add(buf, "\r\n", 2);
    reply_handlers.push_back(reply_handler);
    return true;
}

/*****常规分析实用程序******/

/*将格式为“auth methods=…”的答复行拆分为类型
 *“auth”和参数“methods=…”。
 *语法在https://spec.torproject.org/control-spec中由
 *ProtocolInfo（S3.21）和AuthChallenge（S3.24）的服务器回复格式。
 **/

std::pair<std::string,std::string> SplitTorReplyLine(const std::string &s)
{
    size_t ptr=0;
    std::string type;
    while (ptr < s.size() && s[ptr] != ' ') {
        type.push_back(s[ptr]);
        ++ptr;
    }
    if (ptr < s.size())
++ptr; //跳过
    return make_pair(type, s.substr(ptr));
}

/*分析“methods=cookie，safecookie cookiefile=…/control_auth_cookie”形式的回复参数。
 *返回键到值的映射，如果出现错误，则返回空映射。
 *语法在https://spec.torproject.org/control-spec中由
 *ProtocolInfo（S3.21）、AuthChallenge（S3.24）的服务器回复格式，
 *加入洋葱（S3.27）。另见第2.1和2.3节。
 **/

std::map<std::string,std::string> ParseTorReplyMapping(const std::string &s)
{
    std::map<std::string,std::string> mapping;
    size_t ptr=0;
    while (ptr < s.size()) {
        std::string key, value;
        while (ptr < s.size() && s[ptr] != '=' && s[ptr] != ' ') {
            key.push_back(s[ptr]);
            ++ptr;
        }
if (ptr == s.size()) //意外的行尾
            return std::map<std::string,std::string>();
if (s[ptr] == ' ') //剩下的字符串是optarguments
            break;
++ptr; //跳过“=”
if (ptr < s.size() && s[ptr] == '"') { //引文串
++ptr; //跳过打开“”
            bool escape_next = false;
            while (ptr < s.size() && (escape_next || s[ptr] != '"')) {
//重复的反斜杠必须解释为对
                escape_next = (s[ptr] == '\\' && !escape_next);
                value.push_back(s[ptr]);
                ++ptr;
            }
if (ptr == s.size()) //意外的行尾
                return std::map<std::string,std::string>();
++ptr; //跳过关闭“”
            /*
             *无景观价值。根据https://spec.torproject.org/control-spec第2.1.1节：
             *
             *为了将来的验证，控制器实现者可以使用以下内容
             *与Buggy Tor实现和
             *未来按预期实施规范的项目：
             *
             *读取\n\t\r和\0…当C逃走时。
             *将反斜杠后跟任何其他字符视为该字符。
             **/

            std::string escaped_value;
            for (size_t i = 0; i < value.size(); ++i) {
                if (value[i] == '\\') {
//这将始终有效，因为如果
//以奇数个反斜杠结束，然后是分析器
//因为一个丢失的
//终止双引号。
                    ++i;
                    if (value[i] == 'n') {
                        escaped_value.push_back('\n');
                    } else if (value[i] == 't') {
                        escaped_value.push_back('\t');
                    } else if (value[i] == 'r') {
                        escaped_value.push_back('\r');
                    } else if ('0' <= value[i] && value[i] <= '7') {
                        size_t j;
//八进制转义序列有三个八进制数字的限制，
//但在无效的第一个字符处终止
//八进制数字如果遇到更快。
                        for (j = 1; j < 3 && (i+j) < value.size() && '0' <= value[i+j] && value[i+j] <= '7'; ++j) {}
//对于三位八进制，Tor将第一位限制为0-3。
//因此，前导数字4-7将被解释为
//两位数的八进制。
                        if (j == 3 && value[i] > '3') {
                            j--;
                        }
                        escaped_value.push_back(strtol(value.substr(i, j).c_str(), nullptr, 8));
//循环结束时自动递增的帐户
                        i += j - 1;
                    } else {
                        escaped_value.push_back(value[i]);
                    }
                } else {
                    escaped_value.push_back(value[i]);
                }
            }
            value = escaped_value;
} else { //未引用的值。请注意，值可以随意包含“=”，但不能包含空格。
            while (ptr < s.size() && s[ptr] != ' ') {
                value.push_back(s[ptr]);
                ++ptr;
            }
        }
        if (ptr < s.size() && s[ptr] == ' ')
++ptr; //键=值后跳过“”。
        mapping[key] = value;
    }
    return mapping;
}

/*读取文件的全部内容并以std：：string形式返回。
 *返回一对<status，string>。
 *如果发生错误，状态将为假，否则状态将为真，数据将以字符串形式返回。
 *
 *@param maxsize对读取的文件设置最大大小限制。如果文件大于此值，则截断数据
 *将返回（len>maxsize）。
 **/

static std::pair<bool,std::string> ReadBinaryFile(const fs::path &filename, size_t maxsize=std::numeric_limits<size_t>::max())
{
    FILE *f = fsbridge::fopen(filename, "rb");
    if (f == nullptr)
        return std::make_pair(false,"");
    std::string retval;
    char buffer[128];
    size_t n;
    while ((n=fread(buffer, 1, sizeof(buffer), f)) > 0) {
//检查读取错误，如果无法返回任何数据，则不会返回任何数据
//读取整个文件（或最大大小）
        if (ferror(f)) {
            fclose(f);
            return std::make_pair(false,"");
        }
        retval.append(buffer, buffer+n);
        if (retval.size() > maxsize)
            break;
    }
    fclose(f);
    return std::make_pair(true,retval);
}

/*将std:：string的内容写入文件。
 *@成功后返回true。
 **/

static bool WriteBinaryFile(const fs::path &filename, const std::string &data)
{
    FILE *f = fsbridge::fopen(filename, "wb");
    if (f == nullptr)
        return false;
    if (fwrite(data.data(), 1, data.size(), f) != data.size()) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

/*****比特币特定的TorController实现******/

/*连接到Tor控制套接字的控制器，进行身份验证，然后创建
 *并保持短暂的隐藏服务。
 **/

class TorController
{
public:
    TorController(struct event_base* base, const std::string& target);
    ~TorController();

    /*获取存储私钥的文件名*/
    fs::path GetPrivateKeyFile();

    /*断开后重新连接*/
    void Reconnect();
private:
    struct event_base* base;
    std::string target;
    TorControlConnection conn;
    std::string private_key;
    std::string service_id;
    bool reconnect;
    struct event *reconnect_ev;
    float reconnect_timeout;
    CService service;
    /*安全cookie验证的cookie*/
    std::vector<uint8_t> cookie;
    /*safecookie身份验证的clientnonce*/
    std::vector<uint8_t> clientNonce;

    /*添加洋葱结果的回调*/
    void add_onion_cb(TorControlConnection& conn, const TorControlReply& reply);
    /*身份验证结果的回调*/
    void auth_cb(TorControlConnection& conn, const TorControlReply& reply);
    /*AuthChallenge结果的回调*/
    void authchallenge_cb(TorControlConnection& conn, const TorControlReply& reply);
    /*对ProtocolInfo结果的回调*/
    void protocolinfo_cb(TorControlConnection& conn, const TorControlReply& reply);
    /*连接成功后回调*/
    void connected_cb(TorControlConnection& conn);
    /*连接丢失或连接尝试失败后的回调*/
    void disconnected_cb(TorControlConnection& conn);

    /*重新连接计时器的回调*/
    static void reconnect_cb(evutil_socket_t fd, short what, void *arg);
};

TorController::TorController(struct event_base* _base, const std::string& _target):
    base(_base),
    target(_target), conn(base), reconnect(true), reconnect_ev(0),
    reconnect_timeout(RECONNECT_TIMEOUT_START)
{
    reconnect_ev = event_new(base, -1, 0, reconnect_cb, this);
    if (!reconnect_ev)
        LogPrintf("tor: Failed to create event for reconnection: out of memory?\n");
//立即启动连接尝试
    if (!conn.Connect(_target, std::bind(&TorController::connected_cb, this, std::placeholders::_1),
         std::bind(&TorController::disconnected_cb, this, std::placeholders::_1) )) {
        LogPrintf("tor: Initiating connection to Tor control port %s failed\n", _target);
    }
//如果缓存，则读取服务私钥
    std::pair<bool,std::string> pkf = ReadBinaryFile(GetPrivateKeyFile());
    if (pkf.first) {
        LogPrint(BCLog::TOR, "tor: Reading cached private key from %s\n", GetPrivateKeyFile().string());
        private_key = pkf.second;
    }
}

TorController::~TorController()
{
    if (reconnect_ev) {
        event_free(reconnect_ev);
        reconnect_ev = nullptr;
    }
    if (service.IsValid()) {
        RemoveLocal(service);
    }
}

void TorController::add_onion_cb(TorControlConnection& _conn, const TorControlReply& reply)
{
    if (reply.code == 250) {
        LogPrint(BCLog::TOR, "tor: ADD_ONION successful\n");
        for (const std::string &s : reply.lines) {
            std::map<std::string,std::string> m = ParseTorReplyMapping(s);
            std::map<std::string,std::string>::iterator i;
            if ((i = m.find("ServiceID")) != m.end())
                service_id = i->second;
            if ((i = m.find("PrivateKey")) != m.end())
                private_key = i->second;
        }
        if (service_id.empty()) {
            LogPrintf("tor: Error parsing ADD_ONION parameters:\n");
            for (const std::string &s : reply.lines) {
                LogPrintf("    %s\n", SanitizeString(s));
            }
            return;
        }
        service = LookupNumeric(std::string(service_id+".onion").c_str(), GetListenPort());
        LogPrintf("tor: Got service ID %s, advertising service %s\n", service_id, service.ToString());
        if (WriteBinaryFile(GetPrivateKeyFile(), private_key)) {
            LogPrint(BCLog::TOR, "tor: Cached service private key to %s\n", GetPrivateKeyFile().string());
        } else {
            LogPrintf("tor: Error writing service private key to %s\n", GetPrivateKeyFile().string());
        }
        AddLocal(service, LOCAL_MANUAL);
//…要求洋葱-保持连接打开
} else if (reply.code == 510) { //510无法识别的命令
        LogPrintf("tor: Add onion failed with unrecognized command (You probably need to upgrade Tor)\n");
    } else {
        LogPrintf("tor: Add onion failed; error code %d\n", reply.code);
    }
}

void TorController::auth_cb(TorControlConnection& _conn, const TorControlReply& reply)
{
    if (reply.code == 250) {
        LogPrint(BCLog::TOR, "tor: Authentication successful\n");

//现在我们知道Tor正在运行为洋葱地址设置代理
//如果-洋葱没有放在别的地方。
        if (gArgs.GetArg("-onion", "") == "") {
            CService resolved(LookupNumeric("127.0.0.1", 9050));
            proxyType addrOnion = proxyType(resolved, true);
            SetProxy(NET_ONION, addrOnion);
            SetReachable(NET_ONION, true);
        }

//最后-现在创建服务
if (private_key.empty()) //没有私钥，生成一个
private_key = "NEW:RSA1024"; //明确要求RSA1024-见问题9214
//请求隐藏服务，重定向端口。
//请注意，“虚拟”端口不必与我们的内部端口相同，但这只是一个方便的
//选择。某天重构关机顺序。
        _conn.Command(strprintf("ADD_ONION %s Port=%i,127.0.0.1:%i", private_key, GetListenPort(), GetListenPort()),
            std::bind(&TorController::add_onion_cb, this, std::placeholders::_1, std::placeholders::_2));
    } else {
        LogPrintf("tor: Authentication failed\n");
    }
}

/*计算或安全cookie响应。
 *
 *ServerHash的计算公式为：
 *hmac-sha256（“Tor安全cookie认证服务器到控制器哈希”，
 *cookiestring clientnonce servernce）
 （第一个参数是hmac键）
 *
 *在控制器发送成功的authchallenge命令后，
 *连接上发送的下一个命令必须是authenticate命令，
 *和唯一用于验证命令的验证字符串
 *将接受的是：
 *
 *hmac-sha256（“Tor安全cookie认证控制器到服务器哈希”，
 *cookiestring clientnonce servernce）
 *
 **/

static std::vector<uint8_t> ComputeResponse(const std::string &key, const std::vector<uint8_t> &cookie,  const std::vector<uint8_t> &clientNonce, const std::vector<uint8_t> &serverNonce)
{
    CHMAC_SHA256 computeHash((const uint8_t*)key.data(), key.size());
    std::vector<uint8_t> computedHash(CHMAC_SHA256::OUTPUT_SIZE, 0);
    computeHash.Write(cookie.data(), cookie.size());
    computeHash.Write(clientNonce.data(), clientNonce.size());
    computeHash.Write(serverNonce.data(), serverNonce.size());
    computeHash.Finalize(computedHash.data());
    return computedHash;
}

void TorController::authchallenge_cb(TorControlConnection& _conn, const TorControlReply& reply)
{
    if (reply.code == 250) {
        LogPrint(BCLog::TOR, "tor: SAFECOOKIE authentication challenge successful\n");
        std::pair<std::string,std::string> l = SplitTorReplyLine(reply.lines[0]);
        if (l.first == "AUTHCHALLENGE") {
            std::map<std::string,std::string> m = ParseTorReplyMapping(l.second);
            if (m.empty()) {
                LogPrintf("tor: Error parsing AUTHCHALLENGE parameters: %s\n", SanitizeString(l.second));
                return;
            }
            std::vector<uint8_t> serverHash = ParseHex(m["SERVERHASH"]);
            std::vector<uint8_t> serverNonce = ParseHex(m["SERVERNONCE"]);
            LogPrint(BCLog::TOR, "tor: AUTHCHALLENGE ServerHash %s ServerNonce %s\n", HexStr(serverHash), HexStr(serverNonce));
            if (serverNonce.size() != 32) {
                LogPrintf("tor: ServerNonce is not 32 bytes, as required by spec\n");
                return;
            }

            std::vector<uint8_t> computedServerHash = ComputeResponse(TOR_SAFE_SERVERKEY, cookie, clientNonce, serverNonce);
            if (computedServerHash != serverHash) {
                LogPrintf("tor: ServerHash %s does not match expected ServerHash %s\n", HexStr(serverHash), HexStr(computedServerHash));
                return;
            }

            std::vector<uint8_t> computedClientHash = ComputeResponse(TOR_SAFE_CLIENTKEY, cookie, clientNonce, serverNonce);
            _conn.Command("AUTHENTICATE " + HexStr(computedClientHash), std::bind(&TorController::auth_cb, this, std::placeholders::_1, std::placeholders::_2));
        } else {
            LogPrintf("tor: Invalid reply to AUTHCHALLENGE\n");
        }
    } else {
        LogPrintf("tor: SAFECOOKIE authentication challenge failed\n");
    }
}

void TorController::protocolinfo_cb(TorControlConnection& _conn, const TorControlReply& reply)
{
    if (reply.code == 250) {
        std::set<std::string> methods;
        std::string cookiefile;
        /*
         *250-auth methods=cookie，safecookie cookiefile=“/home/x/.tor/control_auth_cookie”
         *250-auth方法=空
         *250-auth methods=哈希密码
         **/

        for (const std::string &s : reply.lines) {
            std::pair<std::string,std::string> l = SplitTorReplyLine(s);
            if (l.first == "AUTH") {
                std::map<std::string,std::string> m = ParseTorReplyMapping(l.second);
                std::map<std::string,std::string>::iterator i;
                if ((i = m.find("METHODS")) != m.end())
                    boost::split(methods, i->second, boost::is_any_of(","));
                if ((i = m.find("COOKIEFILE")) != m.end())
                    cookiefile = i->second;
            } else if (l.first == "VERSION") {
                std::map<std::string,std::string> m = ParseTorReplyMapping(l.second);
                std::map<std::string,std::string>::iterator i;
                if ((i = m.find("Tor")) != m.end()) {
                    LogPrint(BCLog::TOR, "tor: Connected to Tor version %s\n", i->second);
                }
            }
        }
        for (const std::string &s : methods) {
            LogPrint(BCLog::TOR, "tor: Supported authentication method: %s\n", s);
        }
//首选空值，否则选择安全cookie。如果提供了密码，请使用hashedpassword
        /*认证：
         *cookie:hex编码~/.tor/control_auth_cookie
         *密码：“密码”
         **/

        std::string torpassword = gArgs.GetArg("-torpassword", "");
        if (!torpassword.empty()) {
            if (methods.count("HASHEDPASSWORD")) {
                LogPrint(BCLog::TOR, "tor: Using HASHEDPASSWORD authentication\n");
                boost::replace_all(torpassword, "\"", "\\\"");
                _conn.Command("AUTHENTICATE \"" + torpassword + "\"", std::bind(&TorController::auth_cb, this, std::placeholders::_1, std::placeholders::_2));
            } else {
                LogPrintf("tor: Password provided with -torpassword, but HASHEDPASSWORD authentication is not available\n");
            }
        } else if (methods.count("NULL")) {
            LogPrint(BCLog::TOR, "tor: Using NULL authentication\n");
            _conn.Command("AUTHENTICATE", std::bind(&TorController::auth_cb, this, std::placeholders::_1, std::placeholders::_2));
        } else if (methods.count("SAFECOOKIE")) {
//cookie:hexdump-e'32/1“%02x”\n“~/.tor/control_auth_cookie
            LogPrint(BCLog::TOR, "tor: Using SAFECOOKIE authentication, reading cookie authentication from %s\n", cookiefile);
            std::pair<bool,std::string> status_cookie = ReadBinaryFile(cookiefile, TOR_COOKIE_SIZE);
            if (status_cookie.first && status_cookie.second.size() == TOR_COOKIE_SIZE) {
//_conn.command（“authenticate”+hexstr（status_cookie.second），std:：bind（&torcontroller:：auth_cb，this，std:：placeholders:：_1，std:：placeholders:：_2））；
                cookie = std::vector<uint8_t>(status_cookie.second.begin(), status_cookie.second.end());
                clientNonce = std::vector<uint8_t>(TOR_NONCE_SIZE, 0);
                GetRandBytes(clientNonce.data(), TOR_NONCE_SIZE);
                _conn.Command("AUTHCHALLENGE SAFECOOKIE " + HexStr(clientNonce), std::bind(&TorController::authchallenge_cb, this, std::placeholders::_1, std::placeholders::_2));
            } else {
                if (status_cookie.first) {
                    LogPrintf("tor: Authentication cookie %s is not exactly %i bytes, as is required by the spec\n", cookiefile, TOR_COOKIE_SIZE);
                } else {
                    LogPrintf("tor: Authentication cookie %s could not be opened (check permissions)\n", cookiefile);
                }
            }
        } else if (methods.count("HASHEDPASSWORD")) {
            LogPrintf("tor: The only supported authentication mechanism left is password, but no password provided with -torpassword\n");
        } else {
            LogPrintf("tor: No supported authentication method\n");
        }
    } else {
        LogPrintf("tor: Requesting protocol info failed\n");
    }
}

void TorController::connected_cb(TorControlConnection& _conn)
{
    reconnect_timeout = RECONNECT_TIMEOUT_START;
//首先发送protocolinfo命令以确定需要什么身份验证
    if (!_conn.Command("PROTOCOLINFO 1", std::bind(&TorController::protocolinfo_cb, this, std::placeholders::_1, std::placeholders::_2)))
        LogPrintf("tor: Error sending initial protocolinfo command\n");
}

void TorController::disconnected_cb(TorControlConnection& _conn)
{
//断开连接时停止广告服务
    if (service.IsValid())
        RemoveLocal(service);
    service = CService();
    if (!reconnect)
        return;

    LogPrint(BCLog::TOR, "tor: Not connected to Tor control port %s, trying to reconnect\n", target);

//用于重新连接的单次计时器。使用指数后退。
    struct timeval time = MillisToTimeval(int64_t(reconnect_timeout * 1000.0));
    if (reconnect_ev)
        event_add(reconnect_ev, &time);
    reconnect_timeout *= RECONNECT_TIMEOUT_EXP;
}

void TorController::Reconnect()
{
    /*尝试重新连接并重新建立如果我们被引导-例如，Tor
     *可能正在重新启动。
     **/

    if (!conn.Connect(target, std::bind(&TorController::connected_cb, this, std::placeholders::_1),
         std::bind(&TorController::disconnected_cb, this, std::placeholders::_1) )) {
        LogPrintf("tor: Re-initiating connection to Tor control port %s failed\n", target);
    }
}

fs::path TorController::GetPrivateKeyFile()
{
    return GetDataDir() / "onion_private_key";
}

void TorController::reconnect_cb(evutil_socket_t fd, short what, void *arg)
{
    TorController *self = static_cast<TorController*>(arg);
    self->Reconnect();
}

/*****螺纹*/
static struct event_base *gBase;
static std::thread torControlThread;

static void TorControlThread()
{
    TorController ctrl(gBase, gArgs.GetArg("-torcontrol", DEFAULT_TOR_CONTROL));

    event_base_dispatch(gBase);
}

void StartTorControl()
{
    assert(!gBase);
#ifdef WIN32
    evthread_use_windows_threads();
#else
    evthread_use_pthreads();
#endif
    gBase = event_base_new();
    if (!gBase) {
        LogPrintf("tor: Unable to create event_base\n");
        return;
    }

    torControlThread = std::thread(std::bind(&TraceThread<void (*)()>, "torcontrol", &TorControlThread));
}

void InterruptTorControl()
{
    if (gBase) {
        LogPrintf("tor: Thread interrupt\n");
        event_base_loopbreak(gBase);
    }
}

void StopTorControl()
{
    if (gBase) {
        torControlThread.join();
        event_base_free(gBase);
        gBase = nullptr;
    }
}
