
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
#ifndef BITCOIN_QT_TEST_UTIL_H
#define BITCOIN_QT_TEST_UTIL_H

#include <QString>

/*
 *在消息框对话框中按“确定”按钮。
 *
 *@param text-可选存储对话框文本。
 *@param msec-触发回调前要暂停的毫秒数。
 **/

void ConfirmMessage(QString* text = nullptr, int msec = 0);

#endif //比特币qt测试
