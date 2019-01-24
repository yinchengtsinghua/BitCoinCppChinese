
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2010 Satoshi Nakamoto
//版权所有（c）2012-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_UI_INTERFACE_H
#define BITCOIN_UI_INTERFACE_H

#include <functional>
#include <memory>
#include <stdint.h>
#include <string>

class CWallet;
class CBlockIndex;
namespace boost {
namespace signals2 {
class connection;
}
} //命名空间提升

/*常规更改类型（添加、更新、删除）。*/
enum ChangeType
{
    CT_NEW,
    CT_UPDATED,
    CT_DELETED
};

/*用户界面通信信号。*/
class CClientUIInterface
{
public:
    /*CClientuiInterface:：ThreadSafeMessageBox的标志*/
    enum MessageBoxFlags
    {
        ICON_INFORMATION    = 0,
        ICON_WARNING        = (1U << 0),
        ICON_ERROR          = (1U << 1),
        /*
         *CClientuiInterface:：MessageBoxFlags中所有可用图标的掩码
         *当图标在此处更改时，需要更新此项！
         **/

        ICON_MASK = (ICON_INFORMATION | ICON_WARNING | ICON_ERROR),

        /*这些值取自qmessagebox.h“枚举标准按钮”，可直接使用。*/
BTN_OK      = 0x00000400U, //QMessageBox：好吧
BTN_YES     = 0x00004000U, //qmessagebox:：是
BTN_NO      = 0x00010000U, //QMessageBox：没有
BTN_ABORT   = 0x00040000U, //qmessagebox：：中止
BTN_RETRY   = 0x00080000U, //qmessagebox:：重试
BTN_IGNORE  = 0x00100000U, //qmessagebox：：忽略
BTN_CLOSE   = 0x00200000U, //qmessagebox:：关闭
BTN_CANCEL  = 0x00400000U, //qmessagebox：：取消
BTN_DISCARD = 0x00800000U, //qmessagebox：：放弃
BTN_HELP    = 0x01000000U, //qmessagebox:：帮助
BTN_APPLY   = 0x02000000U, //qmessagebox：：应用
BTN_RESET   = 0x04000000U, //qmessagebox：：重置
        /*
         *CClientuiInterface:：MessageBoxFlags中所有可用按钮的掩码
         *当更改按钮时，需要更新此项！
         **/

        BTN_MASK = (BTN_OK | BTN_YES | BTN_NO | BTN_ABORT | BTN_RETRY | BTN_IGNORE |
                    BTN_CLOSE | BTN_CANCEL | BTN_DISCARD | BTN_HELP | BTN_APPLY | BTN_RESET),

        /*强制阻止，模式消息框对话框（不只是OS通知）*/
        MODAL               = 0x10000000U,

        /*不将消息内容打印到调试日志*/
        SECURE              = 0x40000000U,

        /*特定默认使用情况的预定义组合*/
        MSG_INFORMATION = ICON_INFORMATION,
        MSG_WARNING = (ICON_WARNING | BTN_OK | MODAL),
        MSG_ERROR = (ICON_ERROR | BTN_OK | MODAL)
    };

#define ADD_SIGNALS_DECL_WRAPPER(signal_name, rtype, ...)                                  \
    rtype signal_name(__VA_ARGS__);                                                        \
    using signal_name##Sig = rtype(__VA_ARGS__);                                           \
    boost::signals2::connection signal_name##_connect(std::function<signal_name##Sig> fn); \
    void signal_name##_disconnect(std::function<signal_name##Sig> fn);

    /*显示消息框。*/
    ADD_SIGNALS_DECL_WRAPPER(ThreadSafeMessageBox, bool, const std::string& message, const std::string& caption, unsigned int style);

    /*如果可能，请向用户提问。如果没有，则返回到threadsafeMessageBox（非交互消息、标题、样式），并返回false。*/
    ADD_SIGNALS_DECL_WRAPPER(ThreadSafeQuestion, bool, const std::string& message, const std::string& noninteractive_message, const std::string& caption, unsigned int style);

    /*初始化过程中的进度消息。*/
    ADD_SIGNALS_DECL_WRAPPER(InitMessage, void, const std::string& message);

    /*更改的网络连接数。*/
    ADD_SIGNALS_DECL_WRAPPER(NotifyNumConnectionsChanged, void, int newNumConnections);

    /*网络活动状态已更改。*/
    ADD_SIGNALS_DECL_WRAPPER(NotifyNetworkActiveChanged, void, bool networkActive);

    /*
     *状态栏警报已更改。
     **/

    ADD_SIGNALS_DECL_WRAPPER(NotifyAlertChanged, void, );

    /*一个钱包已经装了。*/
    ADD_SIGNALS_DECL_WRAPPER(LoadWallet, void, std::shared_ptr<CWallet> wallet);

    /*
     *显示进度，例如VerifyChain。
     *resume_possible表示现在关闭将导致重新启动时恢复当前进度操作。
     **/

    ADD_SIGNALS_DECL_WRAPPER(ShowProgress, void, const std::string& title, int nProgress, bool resume_possible);

    /*已接受新块*/
    ADD_SIGNALS_DECL_WRAPPER(NotifyBlockTip, void, bool, const CBlockIndex*);

    /*最佳标题已更改*/
    ADD_SIGNALS_DECL_WRAPPER(NotifyHeaderTip, void, bool, const CBlockIndex*);

    /*黑名单确实改变了。*/
    ADD_SIGNALS_DECL_WRAPPER(BannedListChanged, void, void);
};

/*显示警告消息*/
void InitWarning(const std::string& str);

/*显示错误消息*/
bool InitError(const std::string& str);

std::string AmountHighWarn(const std::string& optname);

std::string AmountErrMsg(const char* const optname, const std::string& strValue);

extern CClientUIInterface uiInterface;

#endif //比特币接口
