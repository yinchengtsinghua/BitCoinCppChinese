
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

#include <httpserver.h>

#include <chainparamsbase.h>
#include <compat.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <netbase.h>
#include <rpc/protocol.h> //对于HTTP状态代码
#include <shutdown.h>
#include <sync.h>
#include <ui_interface.h>

#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include <event2/thread.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include <support/events.h>

#ifdef EVENT__HAVE_NETINET_IN_H
#include <netinet/in.h>
#ifdef _XOPEN_SOURCE_EXTENDED
#include <arpa/inet.h>
#endif
#endif

/*HTTP请求的最大大小（请求行+头）*/
static const size_t MAX_HEADERS_SIZE = 8192;

/*HTTP请求工作项*/
class HTTPWorkItem final : public HTTPClosure
{
public:
    HTTPWorkItem(std::unique_ptr<HTTPRequest> _req, const std::string &_path, const HTTPRequestHandler& _func):
        req(std::move(_req)), path(_path), func(_func)
    {
    }
    void operator()() override
    {
        func(req.get(), path);
    }

    std::unique_ptr<HTTPRequest> req;

private:
    std::string path;
    HTTPRequestHandler func;
};

/*在多个线程上分配工作的简单工作队列。
 *工作项只是可调用的对象。
 **/

template <typename WorkItem>
class WorkQueue
{
private:
    /*互斥保护整个对象*/
    Mutex cs;
    std::condition_variable cond;
    std::deque<std::unique_ptr<WorkItem>> queue;
    bool running;
    size_t maxDepth;

public:
    explicit WorkQueue(size_t _maxDepth) : running(true),
                                 maxDepth(_maxDepth)
    {
    }
    /*前提条件：工作线程已全部停止（它们已联接）。
     **/

    ~WorkQueue()
    {
    }
    /*将工作项排队*/
    bool Enqueue(WorkItem* item)
    {
        LOCK(cs);
        if (queue.size() >= maxDepth) {
            return false;
        }
        queue.emplace_back(std::unique_ptr<WorkItem>(item));
        cond.notify_one();
        return true;
    }
    /*线程函数*/
    void Run()
    {
        while (true) {
            std::unique_ptr<WorkItem> i;
            {
                WAIT_LOCK(cs, lock);
                while (running && queue.empty())
                    cond.wait(lock);
                if (!running)
                    break;
                i = std::move(queue.front());
                queue.pop_front();
            }
            (*i)();
        }
    }
    /*中断和退出循环*/
    void Interrupt()
    {
        LOCK(cs);
        running = false;
        cond.notify_all();
    }
};

struct HTTPPathHandler
{
    HTTPPathHandler(std::string _prefix, bool _exactMatch, HTTPRequestHandler _handler):
        prefix(_prefix), exactMatch(_exactMatch), handler(_handler)
    {
    }
    std::string prefix;
    bool exactMatch;
    HTTPRequestHandler handler;
};

/*HTTP模块状态*/

//！libevent事件循环
static struct event_base* eventBase = nullptr;
//！HTTP服务器
struct evhttp* eventHTTP = nullptr;
//！允许来自的RPC连接的子网列表
static std::vector<CSubNet> rpc_allow_subnets;
//！用于处理事件循环线程外较长请求的工作队列
static WorkQueue<HTTPClosure>* workQueue = nullptr;
//！（子）路径的处理程序
std::vector<HTTPPathHandler> pathHandlers;
//！绑定的侦听套接字
std::vector<evhttp_bound_socket *> boundSockets;

/*检查是否允许网络地址访问HTTP服务器*/
static bool ClientAllowed(const CNetAddr& netaddr)
{
    if (!netaddr.IsValid())
        return false;
    for(const CSubNet& subnet : rpc_allow_subnets)
        if (subnet.Match(netaddr))
            return true;
    return false;
}

/*初始化HTTP服务器的ACL列表*/
static bool InitHTTPAllowList()
{
    rpc_allow_subnets.clear();
    CNetAddr localv4;
    CNetAddr localv6;
    LookupHost("127.0.0.1", localv4, false);
    LookupHost("::1", localv6, false);
rpc_allow_subnets.push_back(CSubNet(localv4, 8));      //始终允许IPv4本地子网
rpc_allow_subnets.push_back(CSubNet(localv6));         //始终允许IPv6本地主机
    for (const std::string& strAllow : gArgs.GetArgs("-rpcallowip")) {
        CSubNet subnet;
        LookupSubNet(strAllow.c_str(), subnet);
        if (!subnet.IsValid()) {
            uiInterface.ThreadSafeMessageBox(
                strprintf("Invalid -rpcallowip subnet specification: %s. Valid are a single IP (e.g. 1.2.3.4), a network/netmask (e.g. 1.2.3.4/255.255.255.0) or a network/CIDR (e.g. 1.2.3.4/24).", strAllow),
                "", CClientUIInterface::MSG_ERROR);
            return false;
        }
        rpc_allow_subnets.push_back(subnet);
    }
    std::string strAllowed;
    for (const CSubNet& subnet : rpc_allow_subnets)
        strAllowed += subnet.ToString() + " ";
    LogPrint(BCLog::HTTP, "Allowing HTTP connections from: %s\n", strAllowed);
    return true;
}

/*HTTP请求方法作为字符串-仅用于日志记录*/
static std::string RequestMethodString(HTTPRequest::RequestMethod m)
{
    switch (m) {
    case HTTPRequest::GET:
        return "GET";
        break;
    case HTTPRequest::POST:
        return "POST";
        break;
    case HTTPRequest::HEAD:
        return "HEAD";
        break;
    case HTTPRequest::PUT:
        return "PUT";
        break;
    default:
        return "unknown";
    }
}

/*HTTP请求回调*/
static void http_request_cb(struct evhttp_request* req, void* arg)
{
//禁用读取以解决在2.2.0中修复的libevent错误。
    if (event_get_version_number() >= 0x02010600 && event_get_version_number() < 0x02020001) {
        evhttp_connection* conn = evhttp_request_get_connection(req);
        if (conn) {
            bufferevent* bev = evhttp_connection_get_bufferevent(conn);
            if (bev) {
                bufferevent_disable(bev, EV_READ);
            }
        }
    }
    std::unique_ptr<HTTPRequest> hreq(new HTTPRequest(req));

//基于早期地址的允许检查
    if (!ClientAllowed(hreq->GetPeer())) {
        LogPrint(BCLog::HTTP, "HTTP request from %s rejected: Client network is not allowed RPC access\n",
                 hreq->GetPeer().ToString());
        hreq->WriteReply(HTTP_FORBIDDEN);
        return;
    }

//早期拒绝未知的HTTP方法
    if (hreq->GetRequestMethod() == HTTPRequest::UNKNOWN) {
        LogPrint(BCLog::HTTP, "HTTP request from %s rejected: Unknown HTTP request method\n",
                 hreq->GetPeer().ToString());
        hreq->WriteReply(HTTP_BADMETHOD);
        return;
    }

    LogPrint(BCLog::HTTP, "Received a %s request for %s from %s\n",
             RequestMethodString(hreq->GetRequestMethod()), SanitizeString(hreq->GetURI(), SAFE_CHARS_URI).substr(0, 100), hreq->GetPeer().ToString());

//查找前缀的注册处理程序
    std::string strURI = hreq->GetURI();
    std::string path;
    std::vector<HTTPPathHandler>::const_iterator i = pathHandlers.begin();
    std::vector<HTTPPathHandler>::const_iterator iend = pathHandlers.end();
    for (; i != iend; ++i) {
        bool match = false;
        if (i->exactMatch)
            match = (strURI == i->prefix);
        else
            match = (strURI.substr(0, i->prefix.size()) == i->prefix);
        if (match) {
            path = strURI.substr(i->prefix.size());
            break;
        }
    }

//发送到工作线程
    if (i != iend) {
        std::unique_ptr<HTTPWorkItem> item(new HTTPWorkItem(std::move(hreq), path, i->handler));
        assert(workQueue);
        if (workQueue->Enqueue(item.get()))
            /*m.release（）；/*如果为真，则队列取得所有权*/
        否则{
            logprintf（“警告：由于超出了HTTP工作队列深度，请求被拒绝，可以使用-rpcWorkQueue=setting来增加请求深度”）；
            item->req->writereply（http_internal，“超过工作队列深度”）；
        }
    }否则{
        hreq->writereply（http_notfound）；
    }
}

/**在关闭后回调以拒绝HTTP请求。*/

static void http_reject_request_cb(struct evhttp_request* req, void*)
{
    LogPrint(BCLog::HTTP, "Rejecting request while shutting down\n");
    evhttp_send_error(req, HTTP_SERVUNAVAIL, nullptr);
}

/*事件调度程序线程*/
static bool ThreadHTTP(struct event_base* base)
{
    RenameThread("bitcoin-http");
    LogPrint(BCLog::HTTP, "Entering http event loop\n");
    event_base_dispatch(base);
//InterruptHttpServer（）将中断事件循环
    LogPrint(BCLog::HTTP, "Exited http event loop\n");
    return event_base_got_break(base) == 0;
}

/*将HTTP服务器绑定到指定的地址*/
static bool HTTPBindAddresses(struct evhttp* http)
{
    int http_port = gArgs.GetArg("-rpcport", BaseParams().RPCPort());
    std::vector<std::pair<std::string, uint16_t> > endpoints;

//确定要绑定到的地址
if (!(gArgs.IsArgSet("-rpcallowip") && gArgs.IsArgSet("-rpcbind"))) { //如果不允许外部IP，则默认为环回
        endpoints.push_back(std::make_pair("::1", http_port));
        endpoints.push_back(std::make_pair("127.0.0.1", http_port));
        if (gArgs.IsArgSet("-rpcallowip")) {
            LogPrintf("WARNING: option -rpcallowip was specified without -rpcbind; this doesn't usually make sense\n");
        }
        if (gArgs.IsArgSet("-rpcbind")) {
            LogPrintf("WARNING: option -rpcbind was ignored because -rpcallowip was not specified, refusing to allow everyone to connect\n");
        }
} else if (gArgs.IsArgSet("-rpcbind")) { //特定绑定地址
        for (const std::string& strRPCBind : gArgs.GetArgs("-rpcbind")) {
            int port = http_port;
            std::string host;
            SplitHostPort(strRPCBind, port, host);
            endpoints.push_back(std::make_pair(host, port));
        }
    }

//绑定地址
    for (std::vector<std::pair<std::string, uint16_t> >::iterator i = endpoints.begin(); i != endpoints.end(); ++i) {
        LogPrint(BCLog::HTTP, "Binding RPC on address %s port %i\n", i->first, i->second);
        evhttp_bound_socket *bind_handle = evhttp_bind_socket_with_handle(http, i->first.empty() ? nullptr : i->first.c_str(), i->second);
        if (bind_handle) {
            CNetAddr addr;
            if (i->first.empty() || (LookupHost(i->first.c_str(), addr, false) && addr.IsBindAny())) {
                LogPrintf("WARNING: the RPC server is not safe to expose to untrusted networks such as the public internet\n");
            }
            boundSockets.push_back(bind_handle);
        } else {
            LogPrintf("Binding RPC on address %s port %i failed.\n", i->first, i->second);
        }
    }
    return !boundSockets.empty();
}

/*设置线程名称和运行工作队列的简单包装*/
static void HTTPWorkQueueRun(WorkQueue<HTTPClosure>* queue)
{
    RenameThread("bitcoin-httpworker");
    queue->Run();
}

/*libevent事件日志回调*/
static void libevent_log_cb(int severity, const char *msg)
{
#ifndef EVENT_LOG_WARN
//在2.0.19中添加了事件日志警告；但在此之前存在事件日志警告。
# define EVENT_LOG_WARN _EVENT_LOG_WARN
#endif
if (severity >= EVENT_LOG_WARN) //记录警告消息及更高级别，无调试类别
        LogPrintf("libevent: %s\n", msg);
    else
        LogPrint(BCLog::LIBEVENT, "libevent: %s\n", msg);
}

bool InitHTTPServer()
{
    if (!InitHTTPAllowList())
        return false;

//将libevent的日志重定向到我们自己的日志
    event_set_log_callback(&libevent_log_cb);
//更新libevent的日志处理。如果我们的版本为
//libevent不支持调试日志记录，在这种情况下，我们应该
//清除bclog:：libevent标志。
    if (!UpdateHTTPServerLogging(g_logger->WillLogCategory(BCLog::LIBEVENT))) {
        g_logger->DisableCategory(BCLog::LIBEVENT);
    }

#ifdef WIN32
    evthread_use_windows_threads();
#else
    evthread_use_pthreads();
#endif

    raii_event_base base_ctr = obtain_event_base();

    /*创建一个新的EVHTTP对象来处理请求。*/
    raii_evhttp http_ctr = obtain_evhttp(base_ctr.get());
    struct evhttp* http = http_ctr.get();
    if (!http) {
        LogPrintf("couldn't create evhttp. Exiting.\n");
        return false;
    }

    evhttp_set_timeout(http, gArgs.GetArg("-rpcservertimeout", DEFAULT_HTTP_SERVER_TIMEOUT));
    evhttp_set_max_headers_size(http, MAX_HEADERS_SIZE);
    evhttp_set_max_body_size(http, MAX_SIZE);
    evhttp_set_gencb(http, http_request_cb, nullptr);

    if (!HTTPBindAddresses(http)) {
        LogPrintf("Unable to bind any endpoint for RPC server\n");
        return false;
    }

    LogPrint(BCLog::HTTP, "Initialized HTTP server\n");
    int workQueueDepth = std::max((long)gArgs.GetArg("-rpcworkqueue", DEFAULT_HTTP_WORKQUEUE), 1L);
    LogPrintf("HTTP: creating work queue of depth %d\n", workQueueDepth);

    workQueue = new WorkQueue<HTTPClosure>(workQueueDepth);
//通过.release（）将所有权转移到eventbase/http
    eventBase = base_ctr.release();
    eventHTTP = http_ctr.release();
    return true;
}

bool UpdateHTTPServerLogging(bool enable) {
#if LIBEVENT_VERSION_NUMBER >= 0x02010100
    if (enable) {
        event_enable_debug_logging(EVENT_DBG_ALL);
    } else {
        event_enable_debug_logging(EVENT_DBG_NONE);
    }
    return true;
#else
//如果版本<02010100，则无法更新libevent日志记录
    return false;
#endif
}

std::thread threadHTTP;
static std::vector<std::thread> g_thread_http_workers;

void StartHTTPServer()
{
    LogPrint(BCLog::HTTP, "Starting HTTP server\n");
    int rpcThreads = std::max((long)gArgs.GetArg("-rpcthreads", DEFAULT_HTTP_THREADS), 1L);
    LogPrintf("HTTP: starting %d worker threads\n", rpcThreads);
    threadHTTP = std::thread(ThreadHTTP, eventBase);

    for (int i = 0; i < rpcThreads; i++) {
        g_thread_http_workers.emplace_back(HTTPWorkQueueRun, workQueue);
    }
}

void InterruptHTTPServer()
{
    LogPrint(BCLog::HTTP, "Interrupting HTTP server\n");
    if (eventHTTP) {
//拒绝当前连接上的请求
        evhttp_set_gencb(eventHTTP, http_reject_request_cb, nullptr);
    }
    if (workQueue)
        workQueue->Interrupt();
}

void StopHTTPServer()
{
    LogPrint(BCLog::HTTP, "Stopping HTTP server\n");
    if (workQueue) {
        LogPrint(BCLog::HTTP, "Waiting for HTTP worker threads to exit\n");
        for (auto& thread: g_thread_http_workers) {
            thread.join();
        }
        g_thread_http_workers.clear();
        delete workQueue;
        workQueue = nullptr;
    }
//取消连接套接字，这是使事件循环运行的原因，这意味着
//关闭所有连接后，事件循环将退出。
    for (evhttp_bound_socket *socket : boundSockets) {
        evhttp_del_accept_socket(eventHTTP, socket);
    }
    boundSockets.clear();
    if (eventBase) {
        LogPrint(BCLog::HTTP, "Waiting for HTTP event thread to exit\n");
        threadHTTP.join();
    }
    if (eventHTTP) {
        evhttp_free(eventHTTP);
        eventHTTP = nullptr;
    }
    if (eventBase) {
        event_base_free(eventBase);
        eventBase = nullptr;
    }
    LogPrint(BCLog::HTTP, "Stopped HTTP server\n");
}

struct event_base* EventBase()
{
    return eventBase;
}

static void httpevent_callback_fn(evutil_socket_t, short, void* data)
{
//静态处理程序：只需调用内部处理程序
    HTTPEvent *self = static_cast<HTTPEvent*>(data);
    self->handler();
    if (self->deleteWhenTriggered)
        delete self;
}

HTTPEvent::HTTPEvent(struct event_base* base, bool _deleteWhenTriggered, const std::function<void()>& _handler):
    deleteWhenTriggered(_deleteWhenTriggered), handler(_handler)
{
    ev = event_new(base, -1, 0, httpevent_callback_fn, this);
    assert(ev);
}
HTTPEvent::~HTTPEvent()
{
    event_free(ev);
}
void HTTPEvent::trigger(struct timeval* tv)
{
    if (tv == nullptr)
event_active(ev, 0, 0); //在主线程中立即触发事件
    else
evtimer_add(ev, tv); //传递timeval之后的触发器
}
HTTPRequest::HTTPRequest(struct evhttp_request* _req) : req(_req),
                                                       replySent(false)
{
}
HTTPRequest::~HTTPRequest()
{
    if (!replySent) {
//跟踪是否发送了回复以避免请求泄漏
        LogPrintf("%s: Unhandled request\n", __func__);
        WriteReply(HTTP_INTERNAL, "Unhandled request");
    }
//只要发送了回复，evhttpd就会清除请求。
}

std::pair<bool, std::string> HTTPRequest::GetHeader(const std::string& hdr) const
{
    const struct evkeyvalq* headers = evhttp_request_get_input_headers(req);
    assert(headers);
    const char* val = evhttp_find_header(headers, hdr.c_str());
    if (val)
        return std::make_pair(true, val);
    else
        return std::make_pair(false, "");
}

std::string HTTPRequest::ReadBody()
{
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    if (!buf)
        return "";
    size_t size = evbuffer_get_length(buf);
    /*微不足道的实现：如果这曾经是性能瓶颈，
     *通过使用
     *evbuffer窥视和笨拙的循环。虽然那样的话，它会是平的
     *最好不要复制到中间字符串，而是使用流
     *抽象在解析算法中动态使用evbuffer。
     **/

    const char* data = (const char*)evbuffer_pullup(buf, size);
if (!data) //如果缓冲区为空，则返回nullptr
        return "";
    std::string rv(data, size);
    evbuffer_drain(buf, size);
    return rv;
}

void HTTPRequest::WriteHeader(const std::string& hdr, const std::string& value)
{
    struct evkeyvalq* headers = evhttp_request_get_output_headers(req);
    assert(headers);
    evhttp_add_header(headers, hdr.c_str(), value.c_str());
}

/*关闭发送到主线程以请求答复发送到
 *HTTP请求。
 *应答必须在主HTTP线程的主循环中发送，
 *这无法从工作线程完成。
 **/

void HTTPRequest::WriteReply(int nStatus, const std::string& strReply)
{
    assert(!replySent && req);
    if (ShutdownRequested()) {
        WriteHeader("Connection", "close");
    }
//向主HTTP线程发送事件以发送回复消息
    struct evbuffer* evb = evhttp_request_get_output_buffer(req);
    assert(evb);
    evbuffer_add(evb, strReply.data(), strReply.size());
    auto req_copy = req;
    HTTPEvent* ev = new HTTPEvent(eventBase, true, [req_copy, nStatus]{
        evhttp_send_reply(req_copy, nStatus, nullptr, nullptr);
//重新启用从插座读取。这是libevent的第二部分
//解决上述问题。
        if (event_get_version_number() >= 0x02010600 && event_get_version_number() < 0x02020001) {
            evhttp_connection* conn = evhttp_request_get_connection(req_copy);
            if (conn) {
                bufferevent* bev = evhttp_connection_get_bufferevent(conn);
                if (bev) {
                    bufferevent_enable(bev, EV_READ | EV_WRITE);
                }
            }
        }
    });
    ev->trigger(nullptr);
    replySent = true;
req = nullptr; //转移回主线程
}

CService HTTPRequest::GetPeer() const
{
    evhttp_connection* con = evhttp_request_get_connection(req);
    CService peer;
    if (con) {
//EVHTTP保留对返回的地址字符串的所有权
        const char* address = "";
        uint16_t port = 0;
        evhttp_connection_get_peer(con, (char**)&address, &port);
        peer = LookupNumeric(address, port);
    }
    return peer;
}

std::string HTTPRequest::GetURI() const
{
    return evhttp_request_get_uri(req);
}

HTTPRequest::RequestMethod HTTPRequest::GetRequestMethod() const
{
    switch (evhttp_request_get_command(req)) {
    case EVHTTP_REQ_GET:
        return GET;
        break;
    case EVHTTP_REQ_POST:
        return POST;
        break;
    case EVHTTP_REQ_HEAD:
        return HEAD;
        break;
    case EVHTTP_REQ_PUT:
        return PUT;
        break;
    default:
        return UNKNOWN;
        break;
    }
}

void RegisterHTTPHandler(const std::string &prefix, bool exactMatch, const HTTPRequestHandler &handler)
{
    LogPrint(BCLog::HTTP, "Registering HTTP handler for %s (exactmatch %d)\n", prefix, exactMatch);
    pathHandlers.push_back(HTTPPathHandler(prefix, exactMatch, handler));
}

void UnregisterHTTPHandler(const std::string &prefix, bool exactMatch)
{
    std::vector<HTTPPathHandler>::iterator i = pathHandlers.begin();
    std::vector<HTTPPathHandler>::iterator iend = pathHandlers.end();
    for (; i != iend; ++i)
        if (i->prefix == prefix && i->exactMatch == exactMatch)
            break;
    if (i != iend)
    {
        LogPrint(BCLog::HTTP, "Unregistering HTTP handler for %s (exactmatch %d)\n", prefix, exactMatch);
        pathHandlers.erase(i);
    }
}

std::string urlDecode(const std::string &urlEncoded) {
    std::string res;
    if (!urlEncoded.empty()) {
        char *decoded = evhttp_uridecode(urlEncoded.c_str(), false, nullptr);
        if (decoded) {
            res = std::string(decoded);
            free(decoded);
        }
    }
    return res;
}
