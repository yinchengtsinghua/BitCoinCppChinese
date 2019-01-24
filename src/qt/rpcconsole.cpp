
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011-2019比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/rpcconsole.h>
#include <qt/forms/ui_debugwindow.h>

#include <qt/bantablemodel.h>
#include <qt/clientmodel.h>
#include <qt/platformstyle.h>
#include <qt/walletmodel.h>
#include <chainparams.h>
#include <interfaces/node.h>
#include <netbase.h>
#include <rpc/server.h>
#include <rpc/client.h>
#include <util/strencodings.h>
#include <util/system.h>

#include <openssl/crypto.h>

#include <univalue.h>

#ifdef ENABLE_WALLET
#include <db_cxx.h>
#include <wallet/wallet.h>
#endif

#include <QDesktopWidget>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QSignalMapper>
#include <QTime>
#include <QTimer>
#include <QStringList>

//TODO:添加一个滚动限制，因为当前没有滚动限制
//TODO:使筛选出类别成为可能（实现时为esp调试消息）
//TODO:通过ClientModel接收错误和调试消息

const int CONSOLE_HISTORY = 50;
const int INITIAL_TRAFFIC_GRAPH_MINS = 30;
const QSize FONT_RANGE(4, 40);
const char fontSizeSettingsKey[] = "consoleFontSize";

const struct {
    const char *url;
    const char *source;
} ICON_MAPPING[] = {
    {"cmd-request", ":/icons/tx_input"},
    {"cmd-reply", ":/icons/tx_output"},
    {"cmd-error", ":/icons/tx_output"},
    {"misc", ":/icons/tx_inout"},
    {nullptr, nullptr}
};

namespace {

//不要将私钥处理命令添加到历史记录中
const QStringList historyFilter = QStringList()
    << "importprivkey"
    << "importmulti"
    << "sethdseed"
    << "signmessagewithprivkey"
    << "signrawtransaction"
    << "signrawtransactionwithkey"
    << "walletpassphrase"
    << "walletpassphrasechange"
    << "encryptwallet";

}

/*对象，用于在单独的线程中执行控制台RPC命令。
**/

class RPCExecutor : public QObject
{
    Q_OBJECT
public:
    explicit RPCExecutor(interfaces::Node& node) : m_node(node) {}

public Q_SLOTS:
    void request(const QString &command, const WalletModel* wallet_model);

Q_SIGNALS:
    void reply(int category, const QString &command);

private:
    interfaces::Node& m_node;
};

/*处理RPC计时器的类
 （用于超时后重新锁定钱包）
 **/

class QtRPCTimerBase: public QObject, public RPCTimerBase
{
    Q_OBJECT
public:
    QtRPCTimerBase(std::function<void()>& _func, int64_t millis):
        func(_func)
    {
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, [this]{ func(); });
        timer.start(millis);
    }
    ~QtRPCTimerBase() {}
private:
    QTimer timer;
    std::function<void()> func;
};

class QtRPCTimerInterface: public RPCTimerInterface
{
public:
    ~QtRPCTimerInterface() {}
    const char *Name() { return "Qt"; }
    RPCTimerBase* NewTimer(std::function<void()>& func, int64_t millis)
    {
        return new QtRPCTimerBase(func, millis);
    }
};


#include <qt/rpcconsole.moc>

/*
 *将shell命令行拆分为参数列表，并可以选择执行命令。
 *旨在模仿\c bash和朋友。
 *
 *-可以用括号嵌套命令；例如：validateAddress（getNewAddress（））
 *-参数用空格或逗号分隔
 *-参数开始和结束以及参数之间的多余空白将被忽略。
 *-文本可以是“双”或“单”引用
 *-反斜杠\c\用作转义符
 *在引号之外，任何字符都可以转义
 *-在双引号中，仅转义\c“和反斜杠在\c”或其他反斜杠之前
 *-在单引号中，不可能进行转义，也不会进行特殊解释。
 *
 *@param[in]节点要在其上执行命令的可选节点
 *@param[out]strresult执行命令（chain）的字符串化结果
 *@param[in]strcommand要拆分的命令行
 *@param[in]fexecute如果要执行命令，请设置为true
 *@param[out]pstrfilteredout命令行，已筛选以删除任何敏感数据
 **/


bool RPCConsole::RPCParseCommandLine(interfaces::Node* node, std::string &strResult, const std::string &strCommand, const bool fExecute, std::string * const pstrFilteredOut, const WalletModel* wallet_model)
{
    std::vector< std::vector<std::string> > stack;
    stack.push_back(std::vector<std::string>());

    enum CmdParseState
    {
        STATE_EATING_SPACES,
        STATE_EATING_SPACES_IN_ARG,
        STATE_EATING_SPACES_IN_BRACKETS,
        STATE_ARGUMENT,
        STATE_SINGLEQUOTED,
        STATE_DOUBLEQUOTED,
        STATE_ESCAPE_OUTER,
        STATE_ESCAPE_DOUBLEQUOTED,
        STATE_COMMAND_EXECUTED,
        STATE_COMMAND_EXECUTED_INNER
    } state = STATE_EATING_SPACES;
    std::string curarg;
    UniValue lastResult;
    unsigned nDepthInsideSensitive = 0;
    size_t filter_begin_pos = 0, chpos;
    std::vector<std::pair<size_t, size_t>> filter_ranges;

    auto add_to_current_stack = [&](const std::string& strArg) {
        if (stack.back().empty() && (!nDepthInsideSensitive) && historyFilter.contains(QString::fromStdString(strArg), Qt::CaseInsensitive)) {
            nDepthInsideSensitive = 1;
            filter_begin_pos = chpos;
        }
//在添加某些内容之前，请确保堆栈不为空
        if (stack.empty()) {
            stack.push_back(std::vector<std::string>());
        }
        stack.back().push_back(strArg);
    };

    auto close_out_params = [&]() {
        if (nDepthInsideSensitive) {
            if (!--nDepthInsideSensitive) {
                assert(filter_begin_pos);
                filter_ranges.push_back(std::make_pair(filter_begin_pos, chpos));
                filter_begin_pos = 0;
            }
        }
        stack.pop_back();
    };

    std::string strCommandTerminated = strCommand;
    if (strCommandTerminated.back() != '\n')
        strCommandTerminated += "\n";
    for (chpos = 0; chpos < strCommandTerminated.size(); ++chpos)
    {
        char ch = strCommandTerminated[chpos];
        switch(state)
        {
            case STATE_COMMAND_EXECUTED_INNER:
            case STATE_COMMAND_EXECUTED:
            {
                bool breakParsing = true;
                switch(ch)
                {
                    case '[': curarg.clear(); state = STATE_COMMAND_EXECUTED_INNER; break;
                    default:
                        if (state == STATE_COMMAND_EXECUTED_INNER)
                        {
                            if (ch != ']')
                            {
//将char附加到当前参数（也用于query命令）
                                curarg += ch;
                                break;
                            }
                            if (curarg.size() && fExecute)
                            {
//如果我们有一个值查询，那么用索引查询数组，用字符串键查询对象。
                                UniValue subelement;
                                if (lastResult.isArray())
                                {
                                    for(char argch: curarg)
                                        if (!IsDigit(argch))
                                            throw std::runtime_error("Invalid result query");
                                    subelement = lastResult[atoi(curarg.c_str())];
                                }
                                else if (lastResult.isObject())
                                    subelement = find_value(lastResult, curarg);
                                else
throw std::runtime_error("Invalid result query"); //无数组或对象：中止
                                lastResult = subelement;
                            }

                            state = STATE_COMMAND_EXECUTED;
                            break;
                        }
//当下一个参数需要char时不中断解析
                        breakParsing = false;

//弹出堆栈并将结果返回到当前命令参数
                        close_out_params();

//不要在字符串的情况下对JSON进行字符串化，以避免出现双引号。
                        if (lastResult.isStr())
                            curarg = lastResult.get_str();
                        else
                            curarg = lastResult.write(2);

//如果有一个非空的结果，请将其用作堆栈参数，否则将其用作常规结果。
                        if (curarg.size())
                        {
                            if (stack.size())
                                add_to_current_stack(curarg);
                            else
                                strResult = curarg;
                        }
                        curarg.clear();
//假设饮食空间状态
                        state = STATE_EATING_SPACES;
                }
                if (breakParsing)
                    break;
            }
case STATE_ARGUMENT: //争论中或争论后
            case STATE_EATING_SPACES_IN_ARG:
            case STATE_EATING_SPACES_IN_BRACKETS:
case STATE_EATING_SPACES: //处理空白运行
                switch(ch)
            {
                case '"': state = STATE_DOUBLEQUOTED; break;
                case '\'': state = STATE_SINGLEQUOTED; break;
                case '\\': state = STATE_ESCAPE_OUTER; break;
                case '(': case ')': case '\n':
                    if (state == STATE_EATING_SPACES_IN_ARG)
                        throw std::runtime_error("Invalid Syntax");
                    if (state == STATE_ARGUMENT)
                    {
                        if (ch == '(' && stack.size() && stack.back().size() > 0)
                        {
                            if (nDepthInsideSensitive) {
                                ++nDepthInsideSensitive;
                            }
                            stack.push_back(std::vector<std::string>());
                        }

//在基本级别上执行命令后不允许使用命令
                        if (!stack.size())
                            throw std::runtime_error("Invalid Syntax");

                        add_to_current_stack(curarg);
                        curarg.clear();
                        state = STATE_EATING_SPACES_IN_BRACKETS;
                    }
                    if ((ch == ')' || ch == '\n') && stack.size() > 0)
                    {
                        if (fExecute) {
//以依赖方法的方式将参数列表转换为JSON对象，
//并将它与方法名一起传递给调度程序。
                            UniValue params = RPCConvertValues(stack.back()[0], std::vector<std::string>(stack.back().begin() + 1, stack.back().end()));
                            std::string method = stack.back()[0];
                            std::string uri;
#ifdef ENABLE_WALLET
                            if (wallet_model) {
                                QByteArray encodedName = QUrl::toPercentEncoding(wallet_model->getWalletName());
                                uri = "/wallet/"+std::string(encodedName.constData(), encodedName.length());
                            }
#endif
                            assert(node);
                            lastResult = node->executeRpc(method, params, uri);
                        }

                        state = STATE_COMMAND_EXECUTED;
                        curarg.clear();
                    }
                    break;
                case ' ': case ',': case '\t':
                    if(state == STATE_EATING_SPACES_IN_ARG && curarg.empty() && ch == ',')
                        throw std::runtime_error("Invalid Syntax");

else if(state == STATE_ARGUMENT) //空格结束参数
                    {
                        add_to_current_stack(curarg);
                        curarg.clear();
                    }
                    if ((state == STATE_EATING_SPACES_IN_BRACKETS || state == STATE_ARGUMENT) && ch == ',')
                    {
                        state = STATE_EATING_SPACES_IN_ARG;
                        break;
                    }
                    state = STATE_EATING_SPACES;
                    break;
                default: curarg += ch; state = STATE_ARGUMENT;
            }
                break;
case STATE_SINGLEQUOTED: //单引号字符串
                switch(ch)
            {
                case '\'': state = STATE_ARGUMENT; break;
                default: curarg += ch;
            }
                break;
case STATE_DOUBLEQUOTED: //双引号字符串
                switch(ch)
            {
                case '"': state = STATE_ARGUMENT; break;
                case '\\': state = STATE_ESCAPE_DOUBLEQUOTED; break;
                default: curarg += ch;
            }
                break;
case STATE_ESCAPE_OUTER: //外引号'\'
                curarg += ch; state = STATE_ARGUMENT;
                break;
case STATE_ESCAPE_DOUBLEQUOTED: //双引号文本中的'\'
if(ch != '"' && ch != '\\') curarg += '\\'; //保留'\'用于除引号和'\'本身之外的所有内容
                curarg += ch; state = STATE_DOUBLEQUOTED;
                break;
        }
    }
    if (pstrFilteredOut) {
        if (STATE_COMMAND_EXECUTED == state) {
            assert(!stack.empty());
            close_out_params();
        }
        *pstrFilteredOut = strCommand;
        for (auto i = filter_ranges.rbegin(); i != filter_ranges.rend(); ++i) {
            pstrFilteredOut->replace(i->first, i->second - i->first, "(…)");
        }
    }
switch(state) //最终状态
    {
        case STATE_COMMAND_EXECUTED:
            if (lastResult.isStr())
                strResult = lastResult.get_str();
            else
                strResult = lastResult.write(2);
        case STATE_ARGUMENT:
        case STATE_EATING_SPACES:
            return true;
default: //以其他状态之一结束时出错
            return false;
    }
}

void RPCExecutor::request(const QString &command, const WalletModel* wallet_model)
{
    try
    {
        std::string result;
        std::string executableCommand = command.toStdString() + "\n";

//在执行rpc调用之前只捕获console-only-help命令，并使用帮助文本进行答复，就像使用rpc答复一样。
        if(executableCommand == "help-console\n") {
            Q_EMIT reply(RPCConsole::CMD_REPLY, QString(("\n"
                "This console accepts RPC commands using the standard syntax.\n"
                "   example:    getblockhash 0\n\n"

                "This console can also accept RPC commands using the parenthesized syntax.\n"
                "   example:    getblockhash(0)\n\n"

                "Commands may be nested when specified with the parenthesized syntax.\n"
                "   example:    getblock(getblockhash(0) 1)\n\n"

                "A space or a comma can be used to delimit arguments for either syntax.\n"
                "   example:    getblockhash 0\n"
                "               getblockhash,0\n\n"

                "Named results can be queried with a non-quoted key string in brackets using the parenthesized syntax.\n"
                "   example:    getblock(getblockhash(0) 1)[tx]\n\n"

                "Results without keys can be queried with an integer in brackets using the parenthesized syntax.\n"
                "   example:    getblock(getblockhash(0),1)[tx][0]\n\n")));
            return;
        }
        if (!RPCConsole::RPCExecuteCommandLine(m_node, result, executableCommand, nullptr, wallet_model)) {
            Q_EMIT reply(RPCConsole::CMD_ERROR, QString("Parse error: unbalanced ' or \""));
            return;
        }

        Q_EMIT reply(RPCConsole::CMD_REPLY, QString::fromStdString(result));
    }
    catch (UniValue& objError)
    {
try //标准格式错误的良好格式
        {
            int code = find_value(objError, "code").get_int();
            std::string message = find_value(objError, "message").get_str();
            Q_EMIT reply(RPCConsole::CMD_ERROR, QString::fromStdString(message) + " (code " + QString::number(code) + ")");
        }
catch (const std::runtime_error&) //转换为无效类型时引发，即缺少代码或消息
{   //显示原始JSON对象
            Q_EMIT reply(RPCConsole::CMD_ERROR, QString::fromStdString(objError.write()));
        }
    }
    catch (const std::exception& e)
    {
        Q_EMIT reply(RPCConsole::CMD_ERROR, QString("Error: ") + QString::fromStdString(e.what()));
    }
}

RPCConsole::RPCConsole(interfaces::Node& node, const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
    m_node(node),
    ui(new Ui::RPCConsole),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);
    QSettings settings;
    if (!restoreGeometry(settings.value("RPCConsoleWindowGeometry").toByteArray())) {
//还原失败（可能缺少设置），将窗口居中
        move(QApplication::desktop()->availableGeometry().center() - frameGeometry().center());
    }

    QChar nonbreaking_hyphen(8209);
    ui->dataDir->setToolTip(ui->dataDir->toolTip().arg(QString(nonbreaking_hyphen) + "datadir"));
    ui->blocksDir->setToolTip(ui->blocksDir->toolTip().arg(QString(nonbreaking_hyphen) + "blocksdir"));
    ui->openDebugLogfileButton->setToolTip(ui->openDebugLogfileButton->toolTip().arg(tr(PACKAGE_NAME)));

    if (platformStyle->getImagesOnButtons()) {
        ui->openDebugLogfileButton->setIcon(platformStyle->SingleColorIcon(":/icons/export"));
    }
    ui->clearButton->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
    ui->fontBiggerButton->setIcon(platformStyle->SingleColorIcon(":/icons/fontbigger"));
    ui->fontSmallerButton->setIcon(platformStyle->SingleColorIcon(":/icons/fontsmaller"));

//安装上下箭头的事件筛选器
    ui->lineEdit->installEventFilter(this);
    ui->messagesWidget->installEventFilter(this);

    connect(ui->clearButton, &QPushButton::clicked, this, &RPCConsole::clear);
    connect(ui->fontBiggerButton, &QPushButton::clicked, this, &RPCConsole::fontBigger);
    connect(ui->fontSmallerButton, &QPushButton::clicked, this, &RPCConsole::fontSmaller);
    connect(ui->btnClearTrafficGraph, &QPushButton::clicked, ui->trafficGraph, &TrafficGraphWidget::clear);

//默认情况下禁用钱包选择器
    ui->WalletSelector->setVisible(false);
    ui->WalletSelectorLabel->setVisible(false);

//设置库版本标签
#ifdef ENABLE_WALLET
    ui->berkeleyDBVersion->setText(DbEnv::version(nullptr, nullptr, nullptr));
#else
    ui->label_berkeleyDBVersion->hide();
    ui->berkeleyDBVersion->hide();
#endif
//注册RPC计时器接口
    rpcTimerInterface = new QtRPCTimerInterface();
//避免意外覆盖现有的非qthread
//基于定时器接口
    m_node.rpcSetTimerInterfaceIfUnset(rpcTimerInterface);

    setTrafficGraphRange(INITIAL_TRAFFIC_GRAPH_MINS);

    ui->detailWidget->hide();
    ui->peerHeading->setText(tr("Select a peer to view detailed information."));

    consoleFontSize = settings.value(fontSizeSettingsKey, QFontInfo(QFont()).pointSize()).toInt();
    clear();
}

RPCConsole::~RPCConsole()
{
    QSettings settings;
    settings.setValue("RPCConsoleWindowGeometry", saveGeometry());
    m_node.rpcUnsetTimerInterface(rpcTimerInterface);
    delete rpcTimerInterface;
    delete ui;
}

bool RPCConsole::eventFilter(QObject* obj, QEvent *event)
{
if(event->type() == QEvent::KeyPress) //特殊钥匙操作
    {
        QKeyEvent *keyevt = static_cast<QKeyEvent*>(event);
        int key = keyevt->key();
        Qt::KeyboardModifiers mod = keyevt->modifiers();
        switch(key)
        {
        case Qt::Key_Up: if(obj == ui->lineEdit) { browseHistory(-1); return true; } break;
        case Qt::Key_Down: if(obj == ui->lineEdit) { browseHistory(1); return true; } break;
        /*e qt:：key_pageup:/*将分页键传递到消息小部件*/
        案例qt:：key_pagedown:
            if（obj==ui->lineedit）
            {
                QApplication:：PostEvent（UI->MessageSwedget，New QkeyEvent（*keyevt））；
                回归真实；
            }
            断裂；
        案例qt：：键返回：
        案例qt:：key_enter:
            //将这些事件转发到lineedit
            if（obj==autocompleter->popup（））
                qApplication:：PostEvent（UI->lineedit，new qkeyEvent（*keyevt））；
                autocompleter->popup（）->hide（）；
                回归真实；
            }
            断裂；
        违约：
            //在messages小部件中键入，将焦点转到行编辑，并在那里重定向键
            //排除大多数不发出文本的组合和键，粘贴快捷方式除外
            if（obj==ui->messageswidget&&）
                  （！MOD & &！keyevt->text（）.isEmpty（）&&key！=qt：：键_tab）
                  （（mod&qt:：controlModifier）&&key==qt:：key_
                  （（mod&qt:：shiftModifier）&&key==qt:：key_insert）））
            {
                ui->lineedit->setfocus（）；
                qApplication:：PostEvent（UI->lineedit，new qkeyEvent（*keyevt））；
                回归真实；
            }
        }
    }
    返回qwidget:：eventfilter（obj，event）；
}

void rpcconsole:：setclientmodel（clientmodel*模型）
{
    clientModel=模型；
    ui->trafficgraph->setclientmodel（model）；
    if（model&&clientModel->getPeerTableModel（）&&clientModel->getBanTableModel（））
        //与客户保持同步
        setNumConnections（model->getNumConnections（））；
        连接（model，&clientmodel:：numConnectionsChanged，this，&rpcconsole:：setNumConnections）；

        接口：：node&node=clientModel->node（）；
        setNumBlocks（node.getNumBlocks（），qdateTime:：FromTime_t（node.getLastBlockTime（）），node.getVerificationProgress（），false）；
        连接（model，&clientmodel:：numblockschauled，this，&rpcconsole:：setnumblocks）；

        updateNetworkState（）；
        连接（model，&clientmodel:：networkactivechanged，this，&rpcconsole:：setnetworkactive）；

        updatetTrafficStats（node.gettotalbytesrecv（），node.gettotalbytessent（））；
        连接（model，&clientmodel:：byteschanged，this，&rpcconsole:：updatetrafficstats）；

        连接（model，&clientmodel:：mempoolsizechanged，this，&rpcconsole:：setmempoolsize）；

        //设置对等表
        ui->peerwidget->setmodel（model->getpeerTableModel（））；
        ui->peerwidget->verticalheader（）->hide（）；
        ui->peerwidget->setEditTriggers（qAbstratemView:：NoEditTriggers）；
        ui->peerwidget->setSelectionBehavior（qAbstratemView:：selectRows）；
        ui->peerwidget->setSelectionMode（qAbstratemView:：extendedSelection）；
        ui->peerwidget->setContextMenuPolicy（qt:：customContextMenu）；
        ui->peerwidget->setcolumnwidth（peerTableModel:：address，address_column_width）；
        ui->peerwidget->setColumnWidth（peerTableModel:：Subversion，Subversion_Column_Width）；
        ui->peerwidget->setcolumnwidth（peerTableModel:：ping，ping_column_width）；
        ui->peerwidget->horizontalheader（）->setStretchLastSection（true）；

        //创建对等表上下文菜单操作
        qaction*disconnectaction=新qaction（tr（“&disconnect”），this）；
        qaction*banaction1h=新qaction（tr（“ban for”）+“”+tr（“1&hour”），this）；
        qaction*banaction24h=新qaction（tr（“ban for”）+“”+tr（“1&day”），this）；
        qaction*banaction7d=新qaction（tr（“ban for”）+“”+tr（“1&week”），this）；
        qaction*banaction365d=新qaction（tr（“ban for”）+“”+tr（“1&year”），this）；

        //创建对等表上下文菜单
        peerstableContextMenu=新建qmenu（this）；
        peerstableContextMenu->addAction（disconnectAction）；
        PeerStableContextMenu->AddAction（BanAction1h）；
        PeerStableContextMenu->AddAction（BanAction24h）；
        PeerStableContextMenu->AddAction（BanAction7d）；
        PeerStableContextMenu->AddAction（BanAction365d）；

        //添加信号映射以允许动态上下文菜单参数。
        //我们需要使用int（而不是int64_t），因为信号映射器只支持
        //int或objects，这是正常的，因为max bantime（1年）小于int_max。
        qsignalmapper*信号映射器=新qsignalmapper（this）；
        信号映射器->设置映射（BanAction1h，60*60）；
        信号映射器->setmapping（banaction24h，60*60*24）；
        信号映射器->设置映射（BanAction7d，60*60*24*7）；
        信号映射器->设置映射（BanAction365d，60*60*24*365）；
        connect（banaction1h，&qaction:：trigged，signalmapper，static_cast<void（qsignalmapper:：*）（）>（&qsignalmapper:：map））；
        connect（banaction24h，&qaction:：trigged，signalmapper，static_cast<void（qsignalmapper:：*）（）>（&qsignalmapper:：map））；
        connect（banaction7d，&qaction:：trigged，signalmapper，static_cast<void（qsignalmapper:：*）（）>（&qsignalmapper:：map））；
        连接（banaction365d，&qaction:：trigged，signalmapper，static_cast<void（qsignalmapper:：*）（）>（&qsignalmapper:：map））；
        connect（signalmapper，static_cast<void（qsignalmapper:：*）（int）>（&qsignalmapper:：mapped），this，&rpcconsole:：banselectednode）；

        //对等表上下文菜单信号
        连接（ui->peerwidget，&qtableview:：customContextMenuRequested，this，&rpcconsole:：showpeerStableContextMenu）；
        连接（disconnectaction，&qaction:：trigged，this，&rpcconsole:：disconnectselectednode）；

        //对等表信号处理-选择新节点时更新对等详细信息
        连接（ui->peerwidget->selectionmodel（），&qitemselectionmodel:：selectionchanged，this，&rpcconsole:：peerselected）；
        //对等表信号处理-在模型中添加新节点时更新对等详细信息
        connect（model->getpeerTableModel（），&peerTableModel:：layoutChanged，this，&rpcconsole:：peerLayoutChanged）；
        //对等表信号处理-缓存所选节点ID
        connect（model->getpeerTableModel（），&peerTableModel:：layoutAboutToBechanged，this，&rpcconsole:：peerLayoutAboutToChange）；

        //设置BAN表
        ui->banListWidget->setModel（model->getBanTableModel（））；
        ui->banlistWidget->verticalHeader（）->hide（）；
        ui->banListWidget->setEditTriggers（qAbstratemView:：NoEditTriggers）；
        ui->banlistwidget->setSelectionBehavior（qAbstratemView:：selectRows）；
        ui->banListWidget->setSelectionMode（qAbstratemView:：singleSelection）；
        ui->banlistwidget->setContextMenuPolicy（qt:：customContextMenu）；
        ui->banlistwidget->setcolumnwidth（bantablemodel:：address，bansubnet_column_width）；
        ui->banListWidget->setColumnWidth（banTableModel:：banTime，banTime_Column_Width）；
        ui->banListWidget->horizontalHeader（）->setStretchLastSection（true）；

        //创建BAN表上下文菜单操作
        qaction*unbanaction=new qaction（tr（“&unban”），this）；

        //创建BAN表上下文菜单
        banTableContextMenu=新建qmenu（this）；
        banTableContextMenu->AddAction（UnbanAction）；

        //禁止表上下文菜单信号
        连接（ui->banlistwidget，&qtableview:：customContextMenuRequested，this，&rpcconsole:：showBanTableContextMenu）；
        连接（Unbanaction，&qaction:：Trigged，this，&rpcconsole:：UnbanselectedNode）；

        //BAN表信号处理-单击BAN表中的某个对等机时清除对等机详细信息
        连接（ui->banlistwidget，&qtableview:：clicked，this，&rpcconsole:：clearselectednode）；
        //BAN表信号处理-确保BAN表显示或隐藏（如果为空）
        connect（model->getBanTableModel（），&banTableModel：：layoutChanged，this，&rpcconsole：：showorhideBanTableifRequired）；
        showorHideBanTableifRequired（）；

        //提供初始值
        ui->clientversion->settext（model->formatfullversion（））；
        ui->clientUserAgent->settext（model->formatSubversion（））；
        ui->datadir->settext（model->datadir（））；
        ui->blocksdir->settext（model->blocksdir（））；
        ui->startuptime->settext（model->formatclientstartuptime（））；
        ui->networkname->settext（qstring:：fromstdstring（params（）.networkidstring（））；

        //设置自动完成并附加它
        qstringlist单词表；
        std:：vector<std:：string>commandlist=m_node.listrpcommands（）；
        for（size_t i=0；i<commandlist.size（）；++i）
        {
            wordlist<<commandlist[i].c_str（）；
            wordlist<（“帮助”+commandlist[i]）.c_str（）；
        }

        wordlist<“帮助控制台”；
        wordlist.sort（）；
        autocompleter=new qcompeter（单词表，this）；
        autocompleter->setModelSorting（qCompleter:：caseSensitivelySortedModel）；
        ui->lineedit->setcompleter（自动完成器）；
        autocompleter->popup（）->installEventFilter（this）；
        //启动线程以执行rpc命令。
        StartExecutor（）；
    }
    如果（！）模型）{
        //客户端模型设置为0，这意味着将要调用shutdown（）。
        线程。
        WAITE（）；
    }
}

ifdef启用_钱包
void rpcconsole:：addwallet（walletmodel*const walletmodel）
{
    //为文本使用名称，为内部数据对象使用钱包模型（允许稍后移动到钱包ID）
    ui->walletselector->additem（walletmodel->getdisplayname（），qvariant:：fromvalue（walletmodel））；
    如果（ui->walletselector->count（）==2&！ISVISILUBLE（））{
        //添加了第一个钱包，设置为默认，只要窗口当前不可见（并且可能正在使用）
        ui->walletselector->setcurrentindex（1）；
    }
    if（ui->walletselector->count（）>2）
        ui->walletselector->setvisible（真）；
        ui->walletselectorlabel->setvisible（真）；
    }
}

void rpcconsole:：removewallet（walletmodel*const walletmodel）
{
    ui->walletselector->removeitem（ui->walletselector->finddata（qvariant:：fromvalue（walletmodel）））；
    if（ui->walletselector->count（）==2）
        ui->walletselector->setvisible（false）；
        ui->walletselectorlabel->setvisible（false）；
    }
}
第二节

静态qString类别类（int类别）
{
    开关（类别）
    {
    case rpcconsole:：cmd_请求：返回“cmd请求”；中断；
    case rpcconsole:：cmd_reply:return“cmd reply”；break；
    case rpcconsole:：cmd_错误：返回“cmd错误”；中断；
    默认：返回“MISC”；
    }
}

void rpcconsole:：fontbigger（）。
{
    setfontsize（控制台fontsize+1）；
}

void rpcconsole:：fontsmaller（）。
{
    setfontsize（控制台fontsize-1）；
}

void rpcconsole:：setfontsize（int newsize）
{
    Q设置；

    //不允许使用疯狂的字体大小
    if（newSize<font_range.width（）newSize>font_range.height（））
        返回；

    //温度。存储控制台内容
    qString str=ui->messageswidget->tohtml（）；

    //替换当前内容中的字体标记大小
    str.replace（qstring（“字体大小：%1pt”）.arg（consolefontsize），qstring（“字体大小：%1pt”）.arg（newSize））；

    //存储新字体大小
    consolefontsize=新闻大小；
    settings.setvalue（fontsizesettingskey，consolefontsize）；

    //清除控制台（重置图标大小，默认样式表）并重新添加内容
    float oldposfactor=1.0/ui->messageswidget->verticalscrollbar（）->maximum（）*ui->messageswidget->verticalscrollbar（）->value（）；
    清除（假）；
    ui->messageswidget->sethtml（str）；
    ui->messageswidget->verticalscrollbar（）->setvalue（oldposfactor*ui->messageswidget->verticalscrollbar（）->maximum（））；
}

void rpcconsole：：清除（bool clearHistory）
{
    ui->messageswidget->clear（）；
    if（清除历史记录）
    {
        history.clear（）；
        历史PyrTr＝0；
    }
    ui->lineedit->clear（）；
    ui->lineedit->setfocus（）；

    //添加平滑缩放的图标图像。
    //（在img上使用宽度/高度时，qt使用最近的而不是线性插值）
    for（int i=0；icon_mapping[i].url；++i）
    {
        ui->messageswidget->document（）->addresource（）。
                    qtextDocument：：图像资源，
                    qurl（icon_-mapping[i].url）、
                    platformStyle->singleColorImage（icon_mapping[i].source）.scalled（qsize（consolefontsize*2，consolefontsize*2），qt:：ignoreaspectratio，qt:：smoothTransformation））；
    }

    //设置默认样式表
    qfontinfo fixedfontinfo（guiutil:：fixedPitchFont（））；
    ui->messageswidget->document（）->setdefaultstylesheet（
        QString
                “表{}”
                “td.time颜色：808080；字体大小：%2；填充顶部：3px；”
                “td.message字体系列：%1；字体大小：%2；空白：预换行；”
                “td.cmd-request颜色：006060；”
                “td.cmd-error颜色：红色；”
                “.secwarning颜色：红色；”
                “B颜色：006060；”
            .arg（fixedfontinfo.family（），qstring（“%1pt”）.arg（consolefontsize））。
        ；

γ-干扰素
    qString clskey=“（）-l”；
否则
    qString clskey=“ctrl-l”；
第二节

    message（cmd_reply，（tr（“欢迎使用%1 RPC控制台。”）.arg（tr（package_name））+“<br>”+
                        tr（“使用上下箭头导航历史记录，使用%1清除屏幕。”）.arg（“<b>”+clskey+“<b>”）+“<br>”+
                        tr（“type%1 for an overview of available commands.”）.arg（“<b>help<b>”）+“<br>”+
                        tr（“有关使用此控制台的详细信息，请键入%1。”）.arg（“<b>Help console<b>”）+
                        “<br><span class=\”secwarning\“><br>”+
                        tr（“警告：诈骗者一直活跃，告诉用户在这里输入命令，窃取他们的钱包内容。”如果不完全理解命令的后果，请不要使用此控制台。“）+
                        “</SPAN >”
                        真的）；
}

void rpcconsole:：keypressEvent（qkeyEvent*事件）
{
    如果（windowtype（）！=qt:：widget&&event->key（）==qt:：key_escape）
    {
        关闭（）；
    }
}

void rpcconsole:：message（int category、const qstring&message、bool html）
{
    qTime time=qTime:：currentTime（）；
    qString timeString=time.toString（）；
    退出；
    out+=“<table><tr><td class=\”time\“width=\”65\“>”+timestring+“<td>”；
    out+=“<td class=\”icon\“width=\”32\“><img src=\”“+category class（category）+”\“><td>”；
    out+=“<td class=\”message“+categoryClass（category）+”valign=\“middle\”>“；
    如果（HTML）
        输出+ =消息；
    其他的
        out+=guiutil:：htmlescape（消息，false）；
    out+=“<td><tr><table>”；
    ui->messageswidget->append（out）；
}

void rpcconsole:：UpdateNetworkState（）
{
    qString connections=qString:：number（clientModel->getNumConnections（））+“（”；
    connections+=tr（“in：”）+“”“+qString:：number（clientModel->getNumConnections（connections_in））+”/“；
    connections+=tr（“out：”）+“”“+qString:：number（clientModel->getNumConnections（connections_out））+”“；

    如果（！）clientModel->node（）.getNetworkActive（））；
        connections+=“（”+tr（“网络活动已禁用”）+“）”；
    }

    ui->numberofconnections->settext（连接）；
}

void rpcconsole:：setNumConnections（int count）
{
    如果（！）客户模型）
        返回；

    updateNetworkState（）；
}

void rpcconsole:：setnetworkactive（bool networkactive）
{
    updateNetworkState（）；
}

void rpcconsole:：setnumblocks（int count，const qdatetime&blockdate，double nverificationprogress，bool headers）
{
    如果（！）头）{
        ui->numberofblocks->settext（qstring:：number（count））；
        ui->lastBlockTime->settext（blockDate.toString（））；
    }
}

void rpcconsole：：setmempoolsize（long numberoftxs，大小为dynuse）
{
    ui->mempoolnumertxs->settext（qstring:：number（numberoftxs））；

    如果（Dynusage<1000000）
        ui->mempoolsize->settext（qstring:：number（dynusage/1000.0，'f'，2）+“kb”）；
    其他的
        ui->mempoolsize->settext（qstring:：number（dynusage/1000000.0，'f'，2）+“mb”）；
}

void rpcconsole:：on_lineedit_returnpressed（）
{
    qstring cmd=ui->lineedit->text（）；

    如果（！）CMD.ISUNTY（）
    {
        std：：字符串strfilteredCmd；
        尝试{
            std：：字符串虚拟；
            如果（！）rpcparsecommandline（nullptr，dummy，cmd.tostdstring（），false，&strfilteredCmd））
                //无法分析命令，因此我们甚至无法对其进行历史筛选
                throw std:：runtime_error（“无效命令行”）；
            }
        catch（const std:：exception&e）
            qmessagebox:：critical（此，“错误”，qstring（“错误：”）+qstring:：fromstdstring（e.what（））；
            返回；
        }

        ui->lineedit->clear（）；

        cmdbeforebrowsing=qstring（）；

        钱包型号*钱包型号空指针
ifdef启用_钱包
        const int wallet_index=ui->walletselector->currentIndex（）；
        如果（wallet_index>0）
            wallet_model=ui->walletselector->itemdata（wallet_index）.value<wallet model**（）；
        }

        如果是最后一个钱包模型！=钱包型号）
            如果（钱包型号）
                message（cmd_request，tr（“使用\“%1\”wallet执行命令”）.arg（wallet_model->getwalletname（））；
            }否则{
                message（命令请求，tr（“执行命令，不带任何钱包”）；
            }
            m_last_wallet_model=钱包_model；
        }
第二节

        消息（Cmd_request，qstring:：fromstdstring（strfilteredCmd））；
        发出命令请求（命令，最后一个钱包模型）；

        cmd=qstring:：fromstdstring（strfilteredCmd）；

        //删除命令（如果已在历史记录中）
        history.removeone（命令）；
        //将命令追加到历史记录
        history.append（命令）；
        //强制最大历史记录大小
        while（history.size（）>控制台历史记录）
            history.removeFirst（）；
        //设置指向历史结尾的指针
        historyptr=history.size（）；

        //将控制台视图滚动到末尾
        ScRotoToN（）；
    }
}

void rpcconsole:：browsehistory（int offset）
{
    //开始浏览历史记录时存储当前文本
    if（historyptr==history.size（））
        cmdbeforebrowsing=ui->lineedit->text（）；
    }

    historyptr+=偏移量；
    如果（HistoryPtr<0）
        历史PyrTr＝0；
    if（historyptr>history.size（））
        historyptr=history.size（）；
    qSnM CMD；
    if（historyptr<history.size（））
        cmd=history.at（历史指针）；
    否则如果（！）cmdbeforebrowsing.isnull（））
        cmd=cmdbeforebrowsing；
    }
    ui->lineedit->settext（cmd）；
}

void rpcconsole:：StartExecutor（）。
{
    rpcexecutor*executor=新rpcexecutor（m_节点）；
    执行器->movetothread（&thread）；

    //执行器对象的答复必须转到该对象
    connect（executor，&rpcexecutor:：reply，this，static_cast<void（rpcconsole:：*）（int，const qstring&）>（&rpcconsole:：message））；

    //来自此对象的请求必须转到Executor
    连接（this，&rpcconsole:：cmdrequest，executor，&rpcexecutor:：request）；

    //确保执行器对象在其自己的线程中被删除
    连接（&thread，&qthread:：finished，executor，&rpcexecutor:：deletelater）；

    //qthread:：run（）的默认实现只是在线程中旋转一个事件循环，
    //这就是我们想要的。
    线程，启动（）；
}

void rpcconsole:：on_tabWidget_currentChanged（int index）
{
    if（ui->tab widget->widget（index）==ui->tab_console）
        ui->lineedit->setfocus（）；
    }
}

void rpcconsole:：on_opendebugglogfilebutton_clicked（））
{
    guiutil:：opendebugglogfile（）；
}

void rpcconsole:：ScrollToEnd（））
{
    qscrollbar*scrollbar=ui->messageswidget->verticalscrollbar（）；
    scrollbar->setvalue（scrollbar->maximum（））；
}

void rpcconsole:：on_sldgraphrange_valuechanged（int value）
{
    const int乘数=5；//滑块上的每个位置代表5分钟
    int mins=值*乘数；
    设置流量图形范围（分钟）；
}

void rpcconsole:：settrafficgraphrange（int分钟）
{
    ui->trafficgraph->setgraphrangemins（分钟）；
    ui->lblgraphrange->settext（guiutil:：formatdurationstr（mins*60））；
}

void rpcconsole:：updatettrafficstats（共分64个字节，共分64个字节）
{
    ui->lblbytesin->settext（guiutil:：formatBytes（totalBytesin））；
    ui->lblbytesout->settext（guiutil:：formatBytes（totalBytesout））；
}

void rpcconsole:：peerselected（const qitemselection&selected，const qitemselection&selected）
{
    q_未使用（未选择）；

    如果（！）客户模式！clientModel->getPeerTableModel（）selected.indexes（）.isEmpty（））
        返回；

    const cnodecombinedstats*stats=clientModel->getPeerTableModel（）->getNodeStats（selected.indexes（）.first（）.row（））；
    如果（统计）
        updateNodeDetail（统计数据）；
}

void rpcconsole:：peerlayoutabouttochange（）。
{
    qmodelIndexList selected=ui->peerWidget->selectionModel（）->selectedIndexes（）；
    cachedNodeIds.clear（）；
    对于（int i=0；i<selected.size（）；i++）
    {
        const cnodecombinedstats*stats=clientmodel->getpeerTableModel（）->getnodestats（selected.at（i）.row（））；
        cachednodeids.append（stats->nodestats.nodeid）；
    }
}

void rpcconsole:：peerlayoutChanged（）。
{
    如果（！）客户模式！clientModel->getPeerTableModel（））
        返回；

    const cnodecombinedstats*stats=nullptr；
    bool funselect=假；
    bool freselect=false；

    if（cachednodeids.empty（））//尚未选择节点
        返回；

    //查找当前选定的行
    int selectedrow=-1；
    qmodelIndexList selectedModelIndex=ui->peerWidget->selectionModel（）->selectedIndex（）；
    如果（！）selectedModelIndex.isEmpty（））
        selectedRow=selectedModelIndex.First（）.Row（）；
    }

    //检查细节节点在表中是否有行（可能不一定
    //位于selectedrow，因为布局更改后其位置会更改）
    int detailnodeow=clientModel->getPeerTableModel（）->getRowByNodeID（cachedNodeID.first（））；

    如果（detailnoderow<0）
    {
        //细节节点从表中消失（节点已断开连接）
        funselect=真；
    }
    其他的
    {
        如果（详细信息请参阅！=选择题）
        {
            //细节节点移动位置
            funselect=真；
            freselect=真；
        }

        //获取细节节点的最新统计信息。
        stats=clientModel->getPeerTableModel（）->getNodeStats（detailNodeRow）；
    }

    if（funselect和selectedrow>=0）
        ClearSelectedNode（）；
    }

    如果（弗雷斯比）
    {
        对于（int i=0；i<cachednodeids.size（）；i++）
        {
            ui->peerwidget->selectrow（clientmodel->getpeerTableModel（）->getrowbynodeid（cachednodeids.at（i））；
        }
    }

    如果（统计）
        updateNodeDetail（统计数据）；
}

void rpcconsole:：updateNodeDetail（const cnodecombinedstats*stats）
{
    //使用最新的节点信息更新细节UI
    qstring peeraddrdetails（qstring:：fromstdstring（stats->nodestats.addrname）+”）；
    peerAddrDetails+=tr（“（节点ID:%1）”）.arg（qstring:：number（stats->node stats.node id））；
    如果（！）stats->nodestats.addrlocal.empty（））
        peerAddrDetails+=“<br/>”+tr（“via%1”）.arg（qstring:：fromstdstring（stats->nodestats.addrlocal））；
    ui->peerheading->settext（peeraddrdetails）；
    ui->peerservices->settext（guiutil:：formatsservicesstr（stats->nodestats.services））；
    ui->peerlastsend->settext（stats->nodestats.nlastsend？guiutil:：formatDurationstr（getSystemTimeInSeconds（）-stats->nodestats.nlastsend）：tr（“从不”）；
    ui->peerastrecv->settext（stats->nodestats.nlastrecv？guiutil:：formatDurationstr（getSystemTimeInSeconds（）-stats->nodestats.nlastRecv）：tr（“从不”）；
    ui->peerbytessent->settext（guiutil:：formatbytes（stats->nodestats.nsendbytes））；
    ui->peerbytesrecv->settext（guiutil:：formatbytes（stats->nodestats.nrecvbytes））；
    ui->peerconntime->settext（guiutil:：formatDurationstr（getSystemTimeInSeconds（）-stats->nodestats.ntimeConnected））；
    ui->peerpingtime->settext（guiutil:：formatpingtime（stats->nodestats.dpingtime））；
    ui->peerpingwait->settext（guiutil:：formatpingtime（stats->nodestats.dpingwait））；
    ui->peerminping->settext（guiutil:：formatpingtime（stats->nodestats.dminping））；
    ui->timeoffset->settext（guiutil:：formattimeoffset（stats->nodestats.ntimeoffset））；
    ui->peerversion->settext（qstring（“%1”）.arg（qstring:：number（stats->nodestats.nversion））；
    ui->peerSubversion->settext（qstring:：fromstdstring（stats->nodestats.cleansuber））；
    ui->peerdirection->settext（stats->nodestats.finbound？tr（“入站”）：tr（“出站”）；
    ui->peerheight->settext（qstring（“%1”）.arg（qstring:：number（stats->nodestats.instartingheight））；
    ui->peerwhitelisted->settext（stats->nodestats.fwhitelisted？tr（“是”）：tr（“否”）；

    //例如，如果锁正忙且
    //无法获取nodestateStats。
    if（stats->fnodestatestatsavailable）
        //BAN分数为初始到0
        ui->peerbanscore->settext（qstring（“%1”）.arg（stats->nodestatestats.nmisbehavior））；

        //同步高度初始化为-1
        if（stats->nodestatestats.nsynchheight>-1）
            ui->peersyncheight->settext（qstring（“%1”）.arg（stats->nodestatestats.nsynchheight））；
        其他的
            ui->peersyncheight->settext（tr（“unknown”））；

        //公共高度初始化为-1
        如果（stats->nodestatestats.ncommonheight>-1）
            ui->peercommonheight->settext（qstring（“%1”）.arg（stats->nodestatestats.ncommonheight））；
        其他的
            ui->peercommonheight->settext（tr（“未知”））；
    }

    ui->detailwidget->show（）；
}

void rpcconsole:：ResizeEvent（qResizeEvent*事件）
{
    QWidget:：ResizeEvent（事件）；
}

void rpcconsole:：ShowEvent（qShowEvent*事件）
{
    QWidget:：ShowEvent（事件）；

    如果（！）客户模式！clientModel->getPeerTableModel（））
        返回；

    //启动PeerTableModel自动刷新
    clientModel->getPeerTableModel（）->startAutoRefresh（）；
}

void rpcconsole:：hideEvent（qHideEvent*事件）
{
    QWidget:：HideEvent（事件）；

    如果（！）客户模式！clientModel->getPeerTableModel（））
        返回；

    //停止PeerTableModel自动刷新
    clientModel->getPeerTableModel（）->stopAutoRefresh（）；
}

void rpcconsole:：ShowPeerStableContextMenu（const qpoint&point）
{
    qmodelindex index=ui->peerwidget->indexat（点）；
    if（index.isvalid（））
        peerstableContextMenu->exec（qcursor:：pos（））；
}

void rpcconsole:：ShowBanTableContextMenu（const qpoint&point）
{
    qmodelindex index=ui->banlistwidget->indexat（点）；
    if（index.isvalid（））
        banTableContextMenu->Exec（qCursor:：pos（））；
}

void rpcconsole:：disconnectSelectedNode（））
{
    //获取选定的对等地址
    qlist<qmodelIndex>nodes=guiutil:：getEntryData（ui->peerWidget，peerTableModel:：netNodeID）；
    对于（int i=0；i<nodes.count（）；i++）
    {
        //获取当前选定的对等地址
        nodeid id=nodes.at（i）.data（）.tolonglong（）；
        //找到节点，断开连接并清除所选节点
        if（m_node.disconnect（id））。
            ClearSelectedNode（）；
    }
}

void rpcconsole:：banselectednode（int bantime）
{
    如果（！）客户模型）
        返回；

    //获取选定的对等地址
    qlist<qmodelIndex>nodes=guiutil:：getEntryData（ui->peerWidget，peerTableModel:：netNodeID）；
    对于（int i=0；i<nodes.count（）；i++）
    {
        //获取当前选定的对等地址
        nodeid id=nodes.at（i）.data（）.tolonglong（）；

 //获取当前选定的对等地址
 int detailnodeow=clientModel->getPeerTableModel（）->getRowByNodeID（id）；
 如果（detailnoderow<0）
     返回；

 //查找可能的节点，禁止并清除选定的节点
 const cnodecombinedstats*stats=clientModel->getPeerTableModel（）->getNodeStats（detailNodeRow）；
 如果（STATS）{
            m_node.ban（stats->node stats.addr，banreasonManuallyAdded，bantime）；
 }
    }
    ClearSelectedNode（）；
    clientModel->getBanTableModel（）->refresh（）；
}

void rpcconsole:：UnbanselectedNode（））
{
    如果（！）客户模型）
        返回；

    //获取选定的BAN地址
    qlist<qmodelIndex>nodes=guiutil:：getEntryData（ui->banListWidget，banTableModel:：Address）；
    对于（int i=0；i<nodes.count（）；i++）
    {
        //获取当前选定的BAN地址
        qString strnode=nodes.at（i）.data（）.toString（）；
        cSubnet可能的Subnet；

        lookupSubnet（strnode.tostdstring（）.c_str（），possibleSubnet）；
        if（possibleSubnet.isvalid（）&&m_node.unban（possibleSubnet））。
        {
            clientModel->getBanTableModel（）->refresh（）；
        }
    }
}

void rpcconsole:：ClearSelectedNode（））
{
    ui->peerwidget->selectionmodel（）->clearselection（）；
    cachedNodeIds.clear（）；
    ui->detailwidget->hide（）；
    ui->peerheading->settext（tr（“select a peer to view detailed information.”））；
}

void rpcconsole:：showorhidebantableifrequired（））
{
    如果（！）客户模型）
        返回；

    bool visible=clientModel->getBanTableModel（）->shouldshow（）；
    ui->banlistwidget->setvisible（可见）；
    ui->banheading->setvisible（可见）；
}

rpcconsole:：tabTypes rpcconsole:：tabFocus（）常量
{
    返回（tabtypes）ui->tabwidget->currentIndex（）；
}

void rpcconsole:：settabfocus（枚举tabtypes tabtype）
{
    ui->tabwidget->setcurrentindex（tabtype）；
}

qString rpcconsole:：TabTitle（TabTypes选项卡_类型）常量
{
    返回ui->tabwidget->tabtext（tab_type）；
}
