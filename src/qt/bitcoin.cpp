
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

#include <qt/bitcoin.h>
#include <qt/bitcoingui.h>

#include <chainparams.h>
#include <qt/clientmodel.h>
#include <fs.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/intro.h>
#include <qt/networkstyle.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/splashscreen.h>
#include <qt/utilitydialog.h>
#include <qt/winshutdownmonitor.h>

#ifdef ENABLE_WALLET
#include <qt/paymentserver.h>
#include <qt/walletcontroller.h>
#endif

#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <noui.h>
#include <rpc/server.h>
#include <ui_interface.h>
#include <uint256.h>
#include <util/system.h>
#include <warnings.h>

#include <walletinitinterface.h>

#include <memory>
#include <stdint.h>

#include <boost/thread.hpp>

#include <QApplication>
#include <QDebug>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QTranslator>

#if defined(QT_STATICPLUGIN)
#include <QtPlugin>
#if QT_VERSION < 0x050400
Q_IMPORT_PLUGIN(AccessibleFactory)
#endif
#if defined(QT_QPA_PLATFORM_XCB)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_WINDOWS)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_COCOA)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#endif
#endif

//声明用于qmetaObject:：InvokeMethod的元类型
Q_DECLARE_METATYPE(bool*)
Q_DECLARE_METATYPE(CAmount)
Q_DECLARE_METATYPE(uint256)

static QString GetLangTerritory()
{
    QSettings settings;
//获取所需的区域设置（例如“de_de”）。
//1）系统默认语言
    QString lang_territory = QLocale::system().name();
//2）qsettings语言
    QString lang_territory_qsettings = settings.value("language", "").toString();
    if(!lang_territory_qsettings.isEmpty())
        lang_territory = lang_territory_qsettings;
//3）-lang命令行参数
    lang_territory = QString::fromStdString(gArgs.GetArg("-lang", lang_territory.toStdString()));
    return lang_territory;
}

/*设置翻译*/
static void initTranslations(QTranslator &qtTranslatorBase, QTranslator &qtTranslator, QTranslator &translatorBase, QTranslator &translator)
{
//删除旧翻译器
    QApplication::removeTranslator(&qtTranslatorBase);
    QApplication::removeTranslator(&qtTranslator);
    QApplication::removeTranslator(&translatorBase);
    QApplication::removeTranslator(&translator);

//获取所需的区域设置（例如“de_de”）。
//1）系统默认语言
    QString lang_territory = GetLangTerritory();

//仅通过截断“_de”转换为“de”
    QString lang = lang_territory;
    lang.truncate(lang_territory.lastIndexOf('_'));

//为配置的区域设置加载语言文件：
//-首先加载基本语言的翻译程序，无区域
//-然后加载更具体的区域设置转换器

//负载，如qt_de.qm
    if (qtTranslatorBase.load("qt_" + lang, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTranslatorBase);

//负载，如qt_de_de_de.qm
    if (qtTranslator.load("qt_" + lang_territory, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTranslator);

//加载，例如比特币质量管理（快捷方式“de”需要在bitcoin.qrc中定义）
    if (translatorBase.load(lang, ":/translations/"))
        QApplication::installTranslator(&translatorBase);

//加载，例如比特币质量管理（快捷方式“de_de”需要在bitcoin.qrc中定义）
    if (translator.load(lang_territory, ":/translations/"))
        QApplication::installTranslator(&translator);
}

/*qdebug（）消息处理程序-->debug.log*/
void DebugMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString &msg)
{
    Q_UNUSED(context);
    if (type == QtDebugMsg) {
        LogPrint(BCLog::QT, "GUI: %s\n", msg.toStdString());
    } else {
        LogPrintf("GUI: %s\n", msg.toStdString());
    }
}

BitcoinCore::BitcoinCore(interfaces::Node& node) :
    QObject(), m_node(node)
{
}

void BitcoinCore::handleRunawayException(const std::exception *e)
{
    PrintExceptionContinue(e, "Runaway exception");
    Q_EMIT runawayException(QString::fromStdString(m_node.getWarnings("gui")));
}

void BitcoinCore::initialize()
{
    try
    {
        qDebug() << __func__ << ": Running initialization in thread";
        bool rv = m_node.appInitMain();
        Q_EMIT initializeResult(rv);
    } catch (const std::exception& e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(nullptr);
    }
}

void BitcoinCore::shutdown()
{
    try
    {
        qDebug() << __func__ << ": Running Shutdown in thread";
        m_node.appShutdown();
        qDebug() << __func__ << ": Shutdown finished";
        Q_EMIT shutdownResult();
    } catch (const std::exception& e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(nullptr);
    }
}

BitcoinApplication::BitcoinApplication(interfaces::Node& node, int &argc, char **argv):
    QApplication(argc, argv),
    coreThread(nullptr),
    m_node(node),
    optionsModel(nullptr),
    clientModel(nullptr),
    window(nullptr),
    pollShutdownTimer(nullptr),
    returnValue(0),
    platformStyle(nullptr)
{
    setQuitOnLastWindowClosed(false);
}

void BitcoinApplication::setupPlatformStyle()
{
//每个平台的用户界面自定义
//这必须在bitcoin应用程序构造函数内部或之后完成，因为
//PlatformStyle:：Instantate需要QApplication
    std::string platformName;
    platformName = gArgs.GetArg("-uiplatform", BitcoinGUI::DEFAULT_UIPLATFORM);
    platformStyle = PlatformStyle::instantiate(QString::fromStdString(platformName));
if (!platformStyle) //如果找不到指定的名称，则返回“其他”
        platformStyle = PlatformStyle::instantiate("other");
    assert(platformStyle);
}

BitcoinApplication::~BitcoinApplication()
{
    if(coreThread)
    {
        qDebug() << __func__ << ": Stopping thread";
        coreThread->quit();
        coreThread->wait();
        qDebug() << __func__ << ": Stopped thread";
    }

    delete window;
    window = nullptr;
#ifdef ENABLE_WALLET
    delete paymentServer;
    paymentServer = nullptr;
#endif
    delete optionsModel;
    optionsModel = nullptr;
    delete platformStyle;
    platformStyle = nullptr;
}

#ifdef ENABLE_WALLET
void BitcoinApplication::createPaymentServer()
{
    paymentServer = new PaymentServer(this);
}
#endif

void BitcoinApplication::createOptionsModel(bool resetSettings)
{
    optionsModel = new OptionsModel(m_node, nullptr, resetSettings);
}

void BitcoinApplication::createWindow(const NetworkStyle *networkStyle)
{
    window = new BitcoinGUI(m_node, platformStyle, networkStyle, nullptr);

    pollShutdownTimer = new QTimer(window);
    connect(pollShutdownTimer, &QTimer::timeout, window, &BitcoinGUI::detectShutdown);
}

void BitcoinApplication::createSplashScreen(const NetworkStyle *networkStyle)
{
    SplashScreen *splash = new SplashScreen(m_node, nullptr, networkStyle);
//创建后，我们没有直接指向启动屏幕的指针，但是启动
//当finish（）发生时，屏幕将自行删除。
    splash->show();
    connect(this, &BitcoinApplication::splashFinished, splash, &SplashScreen::finish);
    connect(this, &BitcoinApplication::requestedShutdown, splash, &QWidget::close);
}

bool BitcoinApplication::baseInitialize()
{
    return m_node.baseInitialize();
}

void BitcoinApplication::startThread()
{
    if(coreThread)
        return;
    coreThread = new QThread(this);
    BitcoinCore *executor = new BitcoinCore(m_node);
    executor->moveToThread(coreThread);

    /*与线程的通信*/
    connect(executor, &BitcoinCore::initializeResult, this, &BitcoinApplication::initializeResult);
    connect(executor, &BitcoinCore::shutdownResult, this, &BitcoinApplication::shutdownResult);
    connect(executor, &BitcoinCore::runawayException, this, &BitcoinApplication::handleRunawayException);
    connect(this, &BitcoinApplication::requestedInitialize, executor, &BitcoinCore::initialize);
    connect(this, &BitcoinApplication::requestedShutdown, executor, &BitcoinCore::shutdown);
    /*确保执行器对象在其自己的线程中被删除*/
    connect(coreThread, &QThread::finished, executor, &QObject::deleteLater);

    coreThread->start();
}

void BitcoinApplication::parameterSetup()
{
//对于GUI，默认的printtoconsole为false。GUI程序不应该
//不必要地打印到控制台。
    gArgs.SoftSetBoolArg("-printtoconsole", false);

    m_node.initLogging();
    m_node.initParameterInteraction();
}

void BitcoinApplication::requestInitialize()
{
    qDebug() << __func__ << ": Requesting initialize";
    startThread();
    Q_EMIT requestedInitialize();
}

void BitcoinApplication::requestShutdown()
{
//显示一个指示关机状态的简单窗口
//先这样做，因为下面的一些步骤可能需要一些时间，
//例如，RPC控制台可能仍在执行命令。
    shutdownWindow.reset(ShutdownWindow::showShutdownWindow(window));

    qDebug() << __func__ << ": Requesting shutdown";
    startThread();
    window->hide();
    window->setClientModel(nullptr);
    pollShutdownTimer->stop();

#ifdef ENABLE_WALLET
    delete m_wallet_controller;
    m_wallet_controller = nullptr;
#endif
    delete clientModel;
    clientModel = nullptr;

    m_node.startShutdown();

//从核心线程请求关闭
    Q_EMIT requestedShutdown();
}

void BitcoinApplication::initializeResult(bool success)
{
    qDebug() << __func__ << ": Initialization result: " << success;
//设置退出结果。
    returnValue = success ? EXIT_SUCCESS : EXIT_FAILURE;
    if(success)
    {
//只有在AppInitMain完成后才记录此日志，因为这样日志设置才能保证完成。
        qWarning() << "Platform customization:" << platformStyle->getName();
#ifdef ENABLE_WALLET
        m_wallet_controller = new WalletController(m_node, platformStyle, optionsModel, this);
#ifdef ENABLE_BIP70
        PaymentServer::LoadRootCAs();
#endif
        if (paymentServer) {
            paymentServer->setOptionsModel(optionsModel);
#ifdef ENABLE_BIP70
            connect(m_wallet_controller, &WalletController::coinsSent, paymentServer, &PaymentServer::fetchPaymentACK);
#endif
        }
#endif

        clientModel = new ClientModel(m_node, optionsModel);
        window->setClientModel(clientModel);
#ifdef ENABLE_WALLET
        window->setWalletController(m_wallet_controller);
#endif

//如果传递了-min选项，则启动窗口最小化（iconified）或最小化到托盘
        if (!gArgs.GetBoolArg("-min", false)) {
            window->show();
        } else if (clientModel->getOptionsModel()->getMinimizeToTray() && window->hasTrayIcon()) {
//不执行任何操作，因为窗口由托盘图标管理
        } else {
            window->showMinimized();
        }
        Q_EMIT splashFinished();
        Q_EMIT windowShown(window);

#ifdef ENABLE_WALLET
//初始化/启动完成后，处理任何命令行
//比特币：URI或付款请求：
        if (paymentServer) {
            connect(paymentServer, &PaymentServer::receivedPaymentRequest, window, &BitcoinGUI::handlePaymentRequest);
            connect(window, &BitcoinGUI::receivedURI, paymentServer, &PaymentServer::handleURIOrFile);
            connect(paymentServer, &PaymentServer::message, [this](const QString& title, const QString& message, unsigned int style) {
                window->message(title, message, style);
            });
            QTimer::singleShot(100, paymentServer, &PaymentServer::uiReady);
        }
#endif
        pollShutdownTimer->start(200);
    } else {
Q_EMIT splashFinished(); //关闭时，确保防溅屏不会粘在周围。
quit(); //退出第一个主循环调用
    }
}

void BitcoinApplication::shutdownResult()
{
quit(); //关闭完成后退出第二个主循环调用
}

void BitcoinApplication::handleRunawayException(const QString &message)
{
    QMessageBox::critical(nullptr, "Runaway exception", BitcoinGUI::tr("A fatal error occurred. Bitcoin can no longer continue safely and will quit.") + QString("\n\n") + message);
    ::exit(EXIT_FAILURE);
}

WId BitcoinApplication::getMainWinId() const
{
    if (!window)
        return 0;

    return window->winId();
}

static void SetupUIArgs()
{
#if defined(ENABLE_WALLET) && defined(ENABLE_BIP70)
    gArgs.AddArg("-allowselfsignedrootcertificates", strprintf("Allow self signed root certificates (default: %u)", DEFAULT_SELFSIGNED_ROOTCERTS), true, OptionsCategory::GUI);
#endif
    gArgs.AddArg("-choosedatadir", strprintf("Choose data directory on startup (default: %u)", DEFAULT_CHOOSE_DATADIR), false, OptionsCategory::GUI);
    gArgs.AddArg("-lang=<lang>", "Set language, for example \"de_DE\" (default: system locale)", false, OptionsCategory::GUI);
    gArgs.AddArg("-min", "Start minimized", false, OptionsCategory::GUI);
    gArgs.AddArg("-resetguisettings", "Reset all settings changed in the GUI", false, OptionsCategory::GUI);
    gArgs.AddArg("-rootcertificates=<file>", "Set SSL root certificates for payment request (default: -system-)", false, OptionsCategory::GUI);
    gArgs.AddArg("-splash", strprintf("Show splash screen on startup (default: %u)", DEFAULT_SPLASHSCREEN), false, OptionsCategory::GUI);
    gArgs.AddArg("-uiplatform", strprintf("Select platform to customize UI for (one of windows, macosx, other; default: %s)", BitcoinGUI::DEFAULT_UIPLATFORM), true, OptionsCategory::GUI);
}

#ifndef BITCOIN_QT_TEST
int GuiMain(int argc, char* argv[])
{
#ifdef WIN32
    util::WinCmdLineArgs winArgs;
    std::tie(argc, argv) = winArgs.get();
#endif
    SetupEnvironment();

    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

//订阅来自核心的全局信号
    std::unique_ptr<interfaces::Handler> handler_message_box = node->handleMessageBox(noui_ThreadSafeMessageBox);
    std::unique_ptr<interfaces::Handler> handler_question = node->handleQuestion(noui_ThreadSafeQuestion);
    std::unique_ptr<interfaces::Handler> handler_init_message = node->handleInitMessage(noui_InitMessage);

//还不引用数据目录，这可以由intro:：pickdatadirectory重写。

//1。基本qt初始化（不依赖于参数或配置）
    Q_INIT_RESOURCE(bitcoin);
    Q_INIT_RESOURCE(bitcoin_locale);

    BitcoinApplication app(*node, argc, argv);
//生成高dpi像素映射
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#if QT_VERSION >= 0x050600
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
#ifdef Q_OS_MAC
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

//注册用于qmetaObject:：InvokeMethod的元类型
    qRegisterMetaType< bool* >();
//需要在此处传递名称，因为camount是typedef（请参见http://qt project.org/doc/qt-5/qmetype.html_qregistermetatype）
//重要信息：如果它不再是typedef，请使用上面的正常变量
    qRegisterMetaType< CAmount >("CAmount");
    qRegisterMetaType< std::function<void()> >("std::function<void()>");

//
//命令行选项优先：
    node->setupServerArgs();
    SetupUIArgs();
    std::string error;
    if (!node->parseParameters(argc, argv, error)) {
        QMessageBox::critical(nullptr, QObject::tr(PACKAGE_NAME),
            QObject::tr("Error parsing command line arguments: %1.").arg(QString::fromStdString(error)));
        return EXIT_FAILURE;
    }

//既然已经设置了Qapplication并解析了参数，那么就可以设置平台样式了。
    app.setupPlatformStyle();

//3。应用程序标识
//必须在初始化OptionsModel或加载翻译之前设置，
//因为它用于定位qsettings
    QApplication::setOrganizationName(QAPP_ORG_NAME);
    QApplication::setOrganizationDomain(QAPP_ORG_DOMAIN);
    QApplication::setApplicationName(QAPP_APP_NAME_DEFAULT);

//4。初始化翻译，以便介绍对话框使用用户的语言
//既然可以访问qsettings，初始化翻译
    QTranslator qtTranslatorBase, qtTranslator, translatorBase, translator;
    initTranslations(qtTranslatorBase, qtTranslator, translatorBase, translator);

//分析命令行选项（用于“-lang”）并设置区域设置后立即显示帮助消息，
//但在显示闪屏之前。
    if (HelpRequested(gArgs) || gArgs.IsArgSet("-version")) {
        HelpMessageDialog help(*node, nullptr, gArgs.IsArgSet("-version"));
        help.showOrPrint();
        return EXIT_SUCCESS;
    }

//5。现在设置和转换可用，请向用户询问数据目录
//设置用户语言：选择数据目录
    if (!Intro::pickDataDirectory(*node))
        return EXIT_SUCCESS;

//6。确定数据和块目录的可用性并分析bitcoin.conf
///-在此步骤完成之前不要调用GetDataDir（true）
    if (!fs::is_directory(GetDataDir(false)))
    {
        QMessageBox::critical(nullptr, QObject::tr(PACKAGE_NAME),
            QObject::tr("Error: Specified data directory \"%1\" does not exist.").arg(QString::fromStdString(gArgs.GetArg("-datadir", ""))));
        return EXIT_FAILURE;
    }
    if (!node->readConfigFiles(error)) {
        QMessageBox::critical(nullptr, QObject::tr(PACKAGE_NAME),
            QObject::tr("Error: Cannot parse configuration file: %1.").arg(QString::fromStdString(error)));
        return EXIT_FAILURE;
    }

//7。确定网络（并切换到特定于网络的选项）
//-在此步骤之前不要调用params（）。
//-解析配置文件后执行此操作，因为可以在那里切换网络
//-qsettings（）将在此之后使用新的应用程序名称，从而产生特定于网络的设置
//-需要在创建选项模型之前完成

//检查-testnet或-regtest参数（params（）调用仅在此子句之后有效）
    try {
        node->selectParams(gArgs.GetChainName());
    } catch(std::exception &e) {
        QMessageBox::critical(nullptr, QObject::tr(PACKAGE_NAME), QObject::tr("Error: %1").arg(e.what()));
        return EXIT_FAILURE;
    }
#ifdef ENABLE_WALLET
//在命令行上分析uri——这会影响params（）。
    PaymentServer::ipcParseCommandLine(*node, argc, argv);
#endif

    QScopedPointer<const NetworkStyle> networkStyle(NetworkStyle::instantiate(QString::fromStdString(Params().NetworkIDString())));
    assert(!networkStyle.isNull());
//允许为测试网提供单独的用户界面设置
    QApplication::setApplicationName(networkStyle->getAppName());
//更改应用程序名称后重新初始化翻译（网络特定设置中的语言可能不同）
    initTranslations(qtTranslatorBase, qtTranslator, translatorBase, translator);

#ifdef ENABLE_WALLET
//8。IURI IPC发送
//-如果我们只是调用ipc，那么请尽早执行此操作，因为我们不想麻烦初始化
//-在*设置数据目录后执行此操作，因为数据目录哈希用于名称中
//服务器的。
//-创建应用程序并设置翻译后执行此操作，因此错误是
//翻译正确。
    if (PaymentServer::ipcSendCommandLine())
        exit(EXIT_SUCCESS);

//尽早启动支付服务器，因此不耐烦的用户点击
//比特币：链接反复将其付款请求发送到此流程：
    app.createPaymentServer();
#endif

//9。主GUI初始化
//安装全局事件筛选器，确保长工具提示可以换行。
    app.installEventFilter(new GUIUtil::ToolTipToRichTextFilter(TOOLTIP_WRAP_THRESHOLD, &app));
#if defined(Q_OS_WIN)
//安装全局事件筛选器以处理与Windows会话相关的Windows消息（wm_queryendsession和wm_endsession）
    qApp->installNativeEventFilter(new WinShutdownMonitor());
#endif
//安装qdebug（）消息处理程序以路由到debug.log
    qInstallMessageHandler(DebugMessageHandler);
//在创建选项模型之前允许参数交互
    app.parameterSetup();
//从qsettings加载GUI设置
    app.createOptionsModel(gArgs.GetBoolArg("-resetguisettings", false));

    if (gArgs.GetBoolArg("-splash", DEFAULT_SPLASHSCREEN) && !gArgs.GetBoolArg("-min", false))
        app.createSplashScreen(networkStyle.data());

    int rv = EXIT_SUCCESS;
    try
    {
        app.createWindow(networkStyle.data());
//在启动初始化/关闭线程之前执行基本初始化
//这是可以接受的，因为此函数只包含可快速执行的步骤，
//所以GUI线程不会被阻塞。
        if (app.baseInitialize()) {
            app.requestInitialize();
#if defined(Q_OS_WIN)
            WinShutdownMonitor::registerShutdownBlockReason(QObject::tr("%1 didn't yet exit safely...").arg(QObject::tr(PACKAGE_NAME)), (HWND)app.getMainWinId());
#endif
            app.exec();
            app.requestShutdown();
            app.exec();
            rv = app.getReturnValue();
        } else {
//initrerror（）将显示一个包含详细错误的对话框。
            rv = EXIT_FAILURE;
        }
    } catch (const std::exception& e) {
        PrintExceptionContinue(&e, "Runaway exception");
        app.handleRunawayException(QString::fromStdString(node->getWarnings("gui")));
    } catch (...) {
        PrintExceptionContinue(nullptr, "Runaway exception");
        app.handleRunawayException(QString::fromStdString(node->getWarnings("gui")));
    }
    return rv;
}
#endif //B-CONIIQ-QTY检验
