
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2017 Pieter Wuille
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

//BECH32是新地址类型中使用的字符串编码格式。
//输出由一个人类可读部分（字母数字）组成，
//分隔符（1），以及base32数据段，最后一个
//其中6个字符是校验和。
//
//有关更多信息，请参阅BIP 173。

#ifndef BITCOIN_BECH32_H
#define BITCOIN_BECH32_H

#include <stdint.h>
#include <string>
#include <vector>

namespace bech32
{

/*编码BECH32字符串。如果失败，则返回空字符串。*/
std::string Encode(const std::string& hrp, const std::vector<uint8_t>& values);

/*解码BECH32字符串。返回（hrp，数据）。空的hrp表示失败。*/
std::pair<std::string, std::vector<uint8_t>> Decode(const std::string& str);

} //命名空间BECH32

#endif //比特币
