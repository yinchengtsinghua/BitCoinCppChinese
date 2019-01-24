
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

#ifndef BITCOIN_QT_GUICONSTANTS_H
#define BITCOIN_QT_GUICONSTANTS_H

/*模型更新之间的毫秒数*/
static const int MODEL_UPDATE_DELAY = 250;

/*askpassphrasedialog--最大密码长度*/
static const int MAX_PASSPHRASE_SIZE = 1024;

/*bitcoingui——状态栏中图标的大小*/
static const int STATUSBAR_ICONSIZE = 16;

static const bool DEFAULT_SPLASHSCREEN = true;

/*字段背景样式无效*/
#define STYLE_INVALID "background:#FF8080"

/*事务列表--未确认的事务*/
#define COLOR_UNCONFIRMED QColor(128, 128, 128)
/*交易清单——负数*/
#define COLOR_NEGATIVE QColor(255, 0, 0)
/*事务列表—裸地址（无标签）*/
#define COLOR_BAREADDRESS QColor(140, 140, 140)
/*事务列表--Tx状态装饰-打开到日期*/
#define COLOR_TX_STATUS_OPENUNTILDATE QColor(64, 64, 255)
/*交易清单——Tx状态装饰——危险，需要注意Tx*/
#define COLOR_TX_STATUS_DANGER QColor(200, 100, 100)
/*事务列表--Tx状态装饰-默认颜色*/
#define COLOR_BLACK QColor(0, 0, 0)

/*超过此长度的工具提示（以字符为单位）将转换为RTF，
   这样它们就可以被文字包装起来。
 **/

static const int TOOLTIP_WRAP_THRESHOLD = 80;

/*允许的最大URI长度*/
static const int MAX_URI_LENGTH = 255;

/*qrcodedialog—导出二维码图像的大小*/
#define QR_IMAGE_SIZE 300

/*微调器动画中的帧数*/
#define SPINNER_FRAMES 36

#define QAPP_ORG_NAME "Bitcoin"
#define QAPP_ORG_DOMAIN "bitcoin.org"
#define QAPP_APP_NAME_DEFAULT "Bitcoin-Qt"
#define QAPP_APP_NAME_TESTNET "Bitcoin-Qt-testnet"
#define QAPP_APP_NAME_REGTEST "Bitcoin-Qt-regtest"

#endif //比特币
