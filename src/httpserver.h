
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2015-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_HTTPSERVER_H
#define BITCOIN_HTTPSERVER_H

#include <string>
#include <stdint.h>
#include <functional>

static const int DEFAULT_HTTP_THREADS=4;
static const int DEFAULT_HTTP_WORKQUEUE=16;
static const int DEFAULT_HTTP_SERVER_TIMEOUT=30;

struct evhttp_request;
struct event_base;
class CService;
class HTTPRequest;

/*初始化HTTP服务器。
 *在registerhttphandler或eventbase（）之前调用此函数。
 **/

bool InitHTTPServer();
/*启动HTTP服务器。
 *这与inithttpserver是分开的，为用户提供了自由竞争条件的时间。
 *在inithttpserver和starthttpserver之间注册它们的处理程序。
 **/

void StartHTTPServer();
/*中断HTTP服务器线程*/
void InterruptHTTPServer();
/*停止HTTP服务器*/
void StopHTTPServer();

/*更改libevent的日志记录级别。从日志类别中删除bclog:：libevent，如果
 *libevent不支持调试日志记录*/

bool UpdateHTTPServerLogging(bool enable);

/*对特定HTTP路径的请求的处理程序*/
typedef std::function<bool(HTTPRequest* req, const std::string &)> HTTPRequestHandler;
/*前缀的注册处理程序。
 *如果多个处理程序匹配一个前缀，则第一个注册的处理程序将
 *被调用。
 **/

void RegisterHTTPHandler(const std::string &prefix, bool exactMatch, const HTTPRequestHandler &handler);
/*注销前缀的处理程序*/
void UnregisterHTTPHandler(const std::string &prefix, bool exactMatch);

/*返回evhttp事件基。子模块可以使用它来
 *队列计时器或自定义事件。
 **/

struct event_base* EventBase();

/*空中HTTP请求。
 *围绕EVHTTPL请求的瘦C++包装器。
 **/

class HTTPRequest
{
private:
    struct evhttp_request* req;
    bool replySent;

public:
    explicit HTTPRequest(struct evhttp_request* req);
    ~HTTPRequest();

    enum RequestMethod {
        UNKNOWN,
        GET,
        POST,
        HEAD,
        PUT
    };

    /*获取请求的URI。
     **/

    std::string GetURI() const;

    /*获取HTTP请求的源站的CSService（地址：IP）。
     **/

    CService GetPeer() const;

    /*获取请求方法。
     **/

    RequestMethod GetRequestMethod() const;

    /*
     *获取hdr指定的请求头或空字符串。
     *返回一对（ispresent，string）。
     **/

    std::pair<bool, std::string> GetHeader(const std::string& hdr) const;

    /*
     *读取请求正文。
     *
     *@注意，由于这会消耗底层缓冲区，因此只调用一次。
     *重复调用将返回空字符串。
     **/

    std::string ReadBody();

    /*
     *写入输出头。
     *
     *note在调用writeErrorReply或reply之前调用此函数。
     **/

    void WriteHeader(const std::string& hdr, const std::string& value);

    /*
     *编写HTTP回复。
     *status是要发送的HTTP状态代码。
     *strreply是回复的主体。保持为空以发送标准消息。
     *
     *@note只能调用一次。因为这会将请求返回给
     *主线程，调用后不要调用任何其他httpRequest方法。
     **/

    void WriteReply(int nStatus, const std::string& strReply = "");
};

/*事件处理程序关闭。
 **/

class HTTPClosure
{
public:
    virtual void operator()() = 0;
    virtual ~HTTPClosure() {}
};

/*事件类。这既可以用作跨线程触发器，也可以用作计时器。
 **/

class HTTPEvent
{
public:
    /*创建新事件。
     *DeleteWhenTriggered在事件触发后删除此事件对象（并调用处理程序）
     *handler是触发事件时要调用的处理程序。
     **/

    HTTPEvent(struct event_base* base, bool deleteWhenTriggered, const std::function<void()>& handler);
    ~HTTPEvent();

    /*触发事件。如果TV为0，则立即触发。否则在之后触发它
     *已过给定时间。
     **/

    void trigger(struct timeval* tv);

    bool deleteWhenTriggered;
    std::function<void()> handler;
private:
    struct event* ev;
};

std::string urlDecode(const std::string &urlEncoded);

#endif //比特币httpserver
