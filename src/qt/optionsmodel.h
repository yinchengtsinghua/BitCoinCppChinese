
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

#ifndef BITCOIN_QT_OPTIONSMODEL_H
#define BITCOIN_QT_OPTIONSMODEL_H

#include <amount.h>

#include <QAbstractListModel>

namespace interfaces {
class Node;
}

QT_BEGIN_NAMESPACE
class QNetworkProxy;
QT_END_NAMESPACE

extern const char *DEFAULT_GUI_PROXY_HOST;
static constexpr unsigned short DEFAULT_GUI_PROXY_PORT = 9050;

/*比特币客户端从qt到配置数据结构的接口。
   对于qt，选项以列表形式显示，其中包含不同的选项
   垂直布置。
   一旦设置充分，可以将其更改为树。
   复杂的。
 **/

class OptionsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit OptionsModel(interfaces::Node& node, QObject *parent = nullptr, bool resetSettings = false);

    enum OptionID {
StartAtStartup,         //布尔
HideTrayIcon,           //布尔
MinimizeToTray,         //布尔
MapPortUPnP,            //布尔
MinimizeOnClose,        //布尔
ProxyUse,               //布尔
ProxyIP,                //Q字符串
ProxyPort,              //int
ProxyUseTor,            //布尔
ProxyIPTor,             //Q字符串
ProxyPortTor,           //int
DisplayUnit,            //比特币单位：单位
ThirdPartyTxUrls,       //Q字符串
Language,               //Q字符串
CoinControlFeatures,    //布尔
ThreadsScriptVerif,     //int
Prune,                  //布尔
PruneSize,              //int
DatabaseCache,          //int
SpendZeroConfChange,    //布尔
Listen,                 //布尔
        OptionIDRowCount,
    };

    void Init(bool resetSettings = false);
    void Reset();

    int rowCount(const QModelIndex & parent = QModelIndex()) const;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole);
    /*更新内存中的当前单元，设置并发出DisplayUnitChanged（NewUnit）信号*/
    void setDisplayUnit(const QVariant &value);

    /*显式getter*/
    bool getHideTrayIcon() const { return fHideTrayIcon; }
    bool getMinimizeToTray() const { return fMinimizeToTray; }
    bool getMinimizeOnClose() const { return fMinimizeOnClose; }
    int getDisplayUnit() const { return nDisplayUnit; }
    QString getThirdPartyTxUrls() const { return strThirdPartyTxUrls; }
    bool getProxySettings(QNetworkProxy& proxy) const;
    bool getCoinControlFeatures() const { return fCoinControlFeatures; }
    const QString& getOverriddenByCommandLine() { return strOverriddenByCommandLine; }

    /*重新启动标志帮助程序*/
    void setRestartRequired(bool fRequired);
    bool isRestartRequired() const;

    interfaces::Node& node() const { return m_node; }

private:
    interfaces::Node& m_node;
    /*仅qt设置*/
    bool fHideTrayIcon;
    bool fMinimizeToTray;
    bool fMinimizeOnClose;
    QString language;
    int nDisplayUnit;
    QString strThirdPartyTxUrls;
    bool fCoinControlFeatures;
    /*被命令行覆盖的设置*/
    QString strOverriddenByCommandLine;

//将选项添加到通过命令行/config文件覆盖的GUI选项列表中
    void addOverriddenOption(const std::string &option);

//检查设置版本，必要时升级默认值
    void checkAndMigrate();
Q_SIGNALS:
    void displayUnitChanged(int unit);
    void coinControlFeaturesChanged(bool);
    void hideTrayIconChanged(bool);
};

#endif //比特币夸脱期权
