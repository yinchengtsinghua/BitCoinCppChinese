
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/optionsmodel.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>

#include <interfaces/node.h>
#include <validation.h> //对于默认的脚本检查线程
#include <net.h>
#include <netbase.h>
#include <txdb.h> //for-dbcache默认值
#include <qt/intro.h>

#include <QNetworkProxy>
#include <QSettings>
#include <QStringList>

const char *DEFAULT_GUI_PROXY_HOST = "127.0.0.1";

static const QString GetDefaultProxyAddress();

OptionsModel::OptionsModel(interfaces::Node& node, QObject *parent, bool resetSettings) :
    QAbstractListModel(parent), m_node(node)
{
    Init(resetSettings);
}

void OptionsModel::addOverriddenOption(const std::string &option)
{
    strOverriddenByCommandLine += QString::fromStdString(option) + "=" + QString::fromStdString(gArgs.GetArg(option, "")) + " ";
}

//用默认值写入所有缺少的qsettings
void OptionsModel::Init(bool resetSettings)
{
    if (resetSettings)
        Reset();

    checkAndMigrate();

    QSettings settings;

//确保重新启动标志在客户端启动时未设置
    setRestartRequired(false);

//这些是仅qt设置：

//窗口
    if (!settings.contains("fHideTrayIcon"))
        settings.setValue("fHideTrayIcon", false);
    fHideTrayIcon = settings.value("fHideTrayIcon").toBool();
    Q_EMIT hideTrayIconChanged(fHideTrayIcon);

    if (!settings.contains("fMinimizeToTray"))
        settings.setValue("fMinimizeToTray", false);
    fMinimizeToTray = settings.value("fMinimizeToTray").toBool() && !fHideTrayIcon;

    if (!settings.contains("fMinimizeOnClose"))
        settings.setValue("fMinimizeOnClose", false);
    fMinimizeOnClose = settings.value("fMinimizeOnClose").toBool();

//显示
    if (!settings.contains("nDisplayUnit"))
        settings.setValue("nDisplayUnit", BitcoinUnits::BTC);
    nDisplayUnit = settings.value("nDisplayUnit").toInt();

    if (!settings.contains("strThirdPartyTxUrls"))
        settings.setValue("strThirdPartyTxUrls", "");
    strThirdPartyTxUrls = settings.value("strThirdPartyTxUrls", "").toString();

    if (!settings.contains("fCoinControlFeatures"))
        settings.setValue("fCoinControlFeatures", false);
    fCoinControlFeatures = settings.value("fCoinControlFeatures", false).toBool();

//它们与核心共享或具有命令行参数
//我们需要命令行参数来覆盖GUI设置。
//
//如果设置不存在，则使用默认值创建它。
//
//如果gargs.softsetarg（）或gargs.softsetboolarg（）返回false，我们将被重写
//通过命令行并在用户界面中显示。

//主要的
    if (!settings.contains("bPrune"))
        settings.setValue("bPrune", false);
    if (!settings.contains("nPruneSize"))
        settings.setValue("nPruneSize", 2);
//将修剪大小转换为MB:
    const uint64_t nPruneSizeMB = settings.value("nPruneSize").toInt() * 1000;
    if (!m_node.softSetArg("-prune", settings.value("bPrune").toBool() ? std::to_string(nPruneSizeMB) : "0")) {
      addOverriddenOption("-prune");
    }

    if (!settings.contains("nDatabaseCache"))
        settings.setValue("nDatabaseCache", (qint64)nDefaultDbCache);
    if (!m_node.softSetArg("-dbcache", settings.value("nDatabaseCache").toString().toStdString()))
        addOverriddenOption("-dbcache");

    if (!settings.contains("nThreadsScriptVerif"))
        settings.setValue("nThreadsScriptVerif", DEFAULT_SCRIPTCHECK_THREADS);
    if (!m_node.softSetArg("-par", settings.value("nThreadsScriptVerif").toString().toStdString()))
        addOverriddenOption("-par");

    if (!settings.contains("strDataDir"))
        settings.setValue("strDataDir", Intro::getDefaultDataDirectory());

//钱包
#ifdef ENABLE_WALLET
    if (!settings.contains("bSpendZeroConfChange"))
        settings.setValue("bSpendZeroConfChange", true);
    if (!m_node.softSetBoolArg("-spendzeroconfchange", settings.value("bSpendZeroConfChange").toBool()))
        addOverriddenOption("-spendzeroconfchange");
#endif

//网络
    if (!settings.contains("fUseUPnP"))
        settings.setValue("fUseUPnP", DEFAULT_UPNP);
    if (!m_node.softSetBoolArg("-upnp", settings.value("fUseUPnP").toBool()))
        addOverriddenOption("-upnp");

    if (!settings.contains("fListen"))
        settings.setValue("fListen", DEFAULT_LISTEN);
    if (!m_node.softSetBoolArg("-listen", settings.value("fListen").toBool()))
        addOverriddenOption("-listen");

    if (!settings.contains("fUseProxy"))
        settings.setValue("fUseProxy", false);
    if (!settings.contains("addrProxy"))
        settings.setValue("addrProxy", GetDefaultProxyAddress());
//如果用户已启用FuseProxy，则仅尝试设置-proxy
    if (settings.value("fUseProxy").toBool() && !m_node.softSetArg("-proxy", settings.value("addrProxy").toString().toStdString()))
        addOverriddenOption("-proxy");
    else if(!settings.value("fUseProxy").toBool() && !gArgs.GetArg("-proxy", "").empty())
        addOverriddenOption("-proxy");

    if (!settings.contains("fUseSeparateProxyTor"))
        settings.setValue("fUseSeparateProxyTor", false);
    if (!settings.contains("addrSeparateProxyTor"))
        settings.setValue("addrSeparateProxyTor", GetDefaultProxyAddress());
//如果用户启用了FuseParateProxytor，则仅尝试设置-Onion
    if (settings.value("fUseSeparateProxyTor").toBool() && !m_node.softSetArg("-onion", settings.value("addrSeparateProxyTor").toString().toStdString()))
        addOverriddenOption("-onion");
    else if(!settings.value("fUseSeparateProxyTor").toBool() && !gArgs.GetArg("-onion", "").empty())
        addOverriddenOption("-onion");

//显示
    if (!settings.contains("language"))
        settings.setValue("language", "");
    if (!m_node.softSetArg("-lang", settings.value("language").toString().toStdString()))
        addOverriddenOption("-lang");

    language = settings.value("language").toString();
}

/*helper函数将内容从一个qsettings复制到另一个qsettings。
 *通过使用allkeys，这也涵盖了层次结构中的嵌套设置。
 **/

static void CopySettings(QSettings& dst, const QSettings& src)
{
    for (const QString& key : src.allKeys()) {
        dst.setValue(key, src.value(key));
    }
}

/*将qsettings备份到ini格式的文件。*/
static void BackupSettings(const fs::path& filename, const QSettings& src)
{
    qWarning() << "Backing up GUI settings to" << GUIUtil::boostPathToQString(filename);
    QSettings dst(GUIUtil::boostPathToQString(filename), QSettings::IniFormat);
    dst.clear();
    CopySettings(dst, src);
}

void OptionsModel::Reset()
{
    QSettings settings;

//将旧设置备份到链特定的datadir以进行故障排除
    BackupSettings(GetDataDir(true) / "guisettings.ini.bak", settings);

//保存strdatadir设置
    QString dataDir = Intro::getDefaultDataDirectory();
    dataDir = settings.value("strDataDir", dataDir).toString();

//从qsettings对象中删除所有条目
    settings.clear();

//设置坐标数据
    settings.setValue("strDataDir", dataDir);

//设置为重置
    settings.setValue("fReset", true);

//OptionsModel:：StartAtStartup的默认设置-已禁用
    if (GUIUtil::GetStartOnSystemStartup())
        GUIUtil::SetStartOnSystemStartup(false);
}

int OptionsModel::rowCount(const QModelIndex & parent) const
{
    return OptionIDRowCount;
}

struct ProxySetting {
    bool is_set;
    QString ip;
    QString port;
};

static ProxySetting GetProxySetting(QSettings &settings, const QString &name)
{
    static const ProxySetting default_val = {false, DEFAULT_GUI_PROXY_HOST, QString("%1").arg(DEFAULT_GUI_PROXY_PORT)};
//处理设置完全未设置的情况
    if (!settings.contains(name)) {
        return default_val;
    }
//包含索引0处的IP和索引1处的端口
    QStringList ip_port = settings.value(name).toString().split(":", QString::SkipEmptyParts);
    if (ip_port.size() == 2) {
        return {true, ip_port.at(0), ip_port.at(1)};
} else { //无效：返回默认值
        return default_val;
    }
}

static void SetProxySetting(QSettings &settings, const QString &name, const ProxySetting &ip_port)
{
    settings.setValue(name, ip_port.ip + ":" + ip_port.port);
}

static const QString GetDefaultProxyAddress()
{
    return QString("%1:%2").arg(DEFAULT_GUI_PROXY_HOST).arg(DEFAULT_GUI_PROXY_PORT);
}

//读取qsettings值并返回它们
QVariant OptionsModel::data(const QModelIndex & index, int role) const
{
    if(role == Qt::EditRole)
    {
        QSettings settings;
        switch(index.row())
        {
        case StartAtStartup:
            return GUIUtil::GetStartOnSystemStartup();
        case HideTrayIcon:
            return fHideTrayIcon;
        case MinimizeToTray:
            return fMinimizeToTray;
        case MapPortUPnP:
#ifdef USE_UPNP
            return settings.value("fUseUPnP");
#else
            return false;
#endif
        case MinimizeOnClose:
            return fMinimizeOnClose;

//缺省代理
        case ProxyUse:
            return settings.value("fUseProxy", false);
        case ProxyIP:
            return GetProxySetting(settings, "addrProxy").ip;
        case ProxyPort:
            return GetProxySetting(settings, "addrProxy").port;

//单独的TOR代理
        case ProxyUseTor:
            return settings.value("fUseSeparateProxyTor", false);
        case ProxyIPTor:
            return GetProxySetting(settings, "addrSeparateProxyTor").ip;
        case ProxyPortTor:
            return GetProxySetting(settings, "addrSeparateProxyTor").port;

#ifdef ENABLE_WALLET
        case SpendZeroConfChange:
            return settings.value("bSpendZeroConfChange");
#endif
        case DisplayUnit:
            return nDisplayUnit;
        case ThirdPartyTxUrls:
            return strThirdPartyTxUrls;
        case Language:
            return settings.value("language");
        case CoinControlFeatures:
            return fCoinControlFeatures;
        case Prune:
            return settings.value("bPrune");
        case PruneSize:
            return settings.value("nPruneSize");
        case DatabaseCache:
            return settings.value("nDatabaseCache");
        case ThreadsScriptVerif:
            return settings.value("nThreadsScriptVerif");
        case Listen:
            return settings.value("fListen");
        default:
            return QVariant();
        }
    }
    return QVariant();
}

//写入qsettings值
bool OptionsModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    /*l successful=true；/*分析错误时设置为false*/
    if（角色==qt:：editRole）
    {
        Q设置；
        开关（index.row（））
        {
        启动时启动案例：
            成功=guiutil:：setStartOnSystemStartup（value.tobool（））；
            断裂；
        病例隐藏：
            fHideTrayIcon=value.tobool（）；
            settings.setvalue（“fhidetrayicon”，fhidetrayicon）；
      q_发射hidetrayiconchanged（fhidetrayicon）；
            断裂；
        案例最小化：
            fMinimizeToTray=value.tobool（）；
            settings.setvalue（“fminimizeToTray”，fminimizeToTray）；
            断裂；
        case mapportupnp://core选项-可随时更改
            settings.setvalue（“fuseupnp”，value.tobool（））；
            m_node.mapport（value.tobool（））；
            断裂；
        案例最小化损失：
            fMinimizeOnClose=value.tobool（）；
            settings.setvalue（“fminimizeonclose”，fminimizeonclose）；
            断裂；

        //默认代理
        ProxyUse案：
            如果（settings.value（“fuseProxy”）！=值）{
                settings.setValue（“fuseProxy”，value.tobool（））；
                setRestartRequired（真）；
            }
            断裂；
        案例PROXIP：{
            auto ip_port=getProxySetting（设置，“addRproxy”）；
            如果（！）ip_port.is_u set_ip_port.ip！=value.toString（））
                ip_port.ip=value.toString（）；
                setProxySetting（设置，“addrProxy”，IP_端口）；
                setRestartRequired（真）；
            }
        }
        断裂；
        案例代理端口：
            auto ip_port=getProxySetting（设置，“addRproxy”）；
            如果（！）ip_port.is_u set_ip_port.port！=value.toString（））
                ip_port.port=value.toString（）；
                setProxySetting（设置，“addrProxy”，IP_端口）；
                setRestartRequired（真）；
            }
        }
        断裂；

        //单独的TOR代理
        案例代理服务器：
            如果（settings.value（“fusseparateproxytor”）！=值）{
                settings.setvalue（“fusseparateproxytor”，value.tobool（））；
                setRestartRequired（真）；
            }
            断裂；
        案件代理人：
            auto ip_port=getproxysetting（设置，“addrseparteproxytor”）；
            如果（！）ip_port.is_u set_ip_port.ip！=value.toString（））
                ip_port.ip=value.toString（）；
                setproxysetting（设置，“addrseparteproxytor”，IP_端口）；
                setRestartRequired（真）；
            }
        }
        断裂；
        案例代理端口：
            auto ip_port=getproxysetting（设置，“addrseparteproxytor”）；
            如果（！）ip_port.is_u set_ip_port.port！=value.toString（））
                ip_port.port=value.toString（）；
                setproxysetting（设置，“addrseparteproxytor”，IP_端口）；
                setRestartRequired（真）；
            }
        }
        断裂；

ifdef启用_钱包
        案例SpendzeroConfChange：
            if（settings.value（“bspendZeroConfChange”）！=值）{
                settings.setvalue（“bspendZeroConfChange”，值）；
                setRestartRequired（真）；
            }
            断裂；
第二节
        外壳显示单元：
            设置显示单位（值）；
            断裂；
        第三方案例：
            如果（strthirdpartyTxurls！=value.toString（））
                strThirdPartyXurls=value.toString（）；
                settings.setValue（“strThirdPartyXurls”，strThirdPartyXurls）；
                setRestartRequired（真）；
            }
            断裂；
        案例语言：
            如果（settings.value（“language”）！=值）{
                settings.setvalue（“语言”，value）；
                setRestartRequired（真）；
            }
            断裂；
        case coincontrol功能：
            fcoincontrolFeatures=value.tobool（）；
            settings.setvalue（“fcoincontrolFeatures”，fcoincontrolFeatures）；
            Q_Emit CoinControlFeaturesChanged（fcoincontrolFeatures）；
            断裂；
        Prune案：
            如果（settings.value（“bprune”）！=值）{
                settings.setvalue（“bprune”，value）；
                setRestartRequired（真）；
            }
            断裂；
        PruneSize案：
            如果（settings.value（“nrunesize”）！=值）{
                settings.setvalue（“nrunesize”，值）；
                setRestartRequired（真）；
            }
            断裂；
        案例数据库缓存：
            如果（settings.value（“databasecache”）！=值）{
                settings.setvalue（“数据缓存”，value）；
                setRestartRequired（真）；
            }
            断裂；
        案例线程脚本验证：
            if（settings.value（“nthreadscriptverif”）！=值）{
                settings.setvalue（“nthreadscriptverif”，值）；
                setRestartRequired（真）；
            }
            断裂；
        案例听：
            如果（settings.value（“flisten”）！=值）{
                settings.setvalue（“flisten”，值）；
                setRestartRequired（真）；
            }
            断裂；
        违约：
            断裂；
        }
    }

    发出数据更改（索引，索引）；

    返回成功；
}

/**更新内存中的当前单元，设置并发出DisplayUnitChanged（newUnit）信号*/

void OptionsModel::setDisplayUnit(const QVariant &value)
{
    if (!value.isNull())
    {
        QSettings settings;
        nDisplayUnit = value.toInt();
        settings.setValue("nDisplayUnit", nDisplayUnit);
        Q_EMIT displayUnitChanged(nDisplayUnit);
    }
}

bool OptionsModel::getProxySettings(QNetworkProxy& proxy) const
{
//直接查询当前基本代理，因为
//可以用-proxy覆盖GUI设置。
    proxyType curProxy;
    if (m_node.getProxy(NET_IPV4, curProxy)) {
        proxy.setType(QNetworkProxy::Socks5Proxy);
        proxy.setHostName(QString::fromStdString(curProxy.proxy.ToStringIP()));
        proxy.setPort(curProxy.proxy.GetPort());

        return true;
    }
    else
        proxy.setType(QNetworkProxy::NoProxy);

    return false;
}

void OptionsModel::setRestartRequired(bool fRequired)
{
    QSettings settings;
    return settings.setValue("fRestartRequired", fRequired);
}

bool OptionsModel::isRestartRequired() const
{
    QSettings settings;
    return settings.value("fRestartRequired", false).toBool();
}

void OptionsModel::checkAndMigrate()
{
//默认值迁移
//检查qsettings容器是否已加载此客户端版本
    QSettings settings;
    static const char strSettingsVersionKey[] = "nSettingsVersion";
    int settingsVersion = settings.contains(strSettingsVersionKey) ? settings.value(strSettingsVersionKey).toInt() : 0;
    if (settingsVersion < CLIENT_VERSION)
    {
//-0.13中dbcache从100增加到300
//参见https://github.com/bitcoin/bitcoin/pull/8273
//如果使用100MB，则强制用户升级到新值
        if (settingsVersion < 130000 && settings.contains("nDatabaseCache") && settings.value("nDatabaseCache").toLongLong() == 100)
            settings.setValue("nDatabaseCache", (qint64)nDefaultDbCache);

        settings.setValue(strSettingsVersionKey, CLIENT_VERSION);
    }

//如果设置为非法，则覆盖“addrProxy”设置
//默认值（见第12623号问题；第12650号问题）。
    if (settings.contains("addrProxy") && settings.value("addrProxy").toString().endsWith("%2")) {
        settings.setValue("addrProxy", GetDefaultProxyAddress());
    }

//如果“addrseparteproxytor”设置被设置为非法，则覆盖该设置
//默认值（见第12623号问题；第12650号问题）。
    if (settings.contains("addrSeparateProxyTor") && settings.value("addrSeparateProxyTor").toString().endsWith("%2")) {
        settings.setValue("addrSeparateProxyTor", GetDefaultProxyAddress());
    }
}
