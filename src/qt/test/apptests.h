
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_QT_TEST_APPTESTS_H
#define BITCOIN_QT_TEST_APPTESTS_H

#include <QObject>
#include <set>
#include <string>
#include <utility>

class BitcoinApplication;
class BitcoinGUI;
class RPCConsole;

class AppTests : public QObject
{
    Q_OBJECT
public:
    explicit AppTests(BitcoinApplication& app) : m_app(app) {}

private Q_SLOTS:
    void appTests();
    void guiTests(BitcoinGUI* window);
    void consoleTests(RPCConsole* console);

private:
//！将预期的回调名称添加到挂起回调列表中。
    void expectCallback(std::string callback) { m_callbacks.emplace(std::move(callback)); }

//！要删除的raii助手不再挂起回调。
    struct HandleCallback
    {
        std::string m_callback;
        AppTests& m_app_tests;
        ~HandleCallback();
    };

//！比特币申请。
    BitcoinApplication& m_app;

//！一组挂起的回调名称。用于跟踪预期回调并关闭
//！在最后一次回调处理完毕且所有测试
//！运行或引发异常。这可能是一个简单的int计数器
//！而不是一组名称，但这些名称可能对调试有用。
    std::multiset<std::string> m_callbacks;
};

#endif //比特币qt测试
