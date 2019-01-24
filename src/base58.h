
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

/*
 *为什么用base-58代替标准的base-64编码？
 *-不要在某些字体和
 *可用于创建视觉上相同的外观数据。
 *带有非字母数字字符的字符串不像输入那样容易被接受。
 *如果没有标点符号，电子邮件通常不会换行。
 *-双击将整个字符串选为一个单词（如果全部是字母数字）。
 **/

#ifndef BITCOIN_BASE58_H
#define BITCOIN_BASE58_H

#include <attributes.h>

#include <string>
#include <vector>

/*
 *将字节序列编码为base58编码字符串。
 *除非两者都是，否则pbegin和pend不能为nullptr。
 **/

std::string EncodeBase58(const unsigned char* pbegin, const unsigned char* pend);

/*
 *将字节向量编码为base58编码字符串
 **/

std::string EncodeBase58(const std::vector<unsigned char>& vch);

/*
 *将base58编码字符串（psz）解码为字节向量（vchret）。
 *如果解码成功，则返回true。
 *psz不能为nullptr。
 **/

NODISCARD bool DecodeBase58(const char* psz, std::vector<unsigned char>& vchRet);

/*
 *将base58编码字符串（str）解码为字节向量（vchret）。
 *如果解码成功，则返回true。
 **/

NODISCARD bool DecodeBase58(const std::string& str, std::vector<unsigned char>& vchRet);

/*
 *将字节向量编码为base58编码字符串，包括校验和
 **/

std::string EncodeBase58Check(const std::vector<unsigned char>& vchIn);

/*
 *将包含校验和的base58编码字符串（psz）解码为字节。
 *vector（vchret），如果解码成功，则返回true
 **/

NODISCARD bool DecodeBase58Check(const char* psz, std::vector<unsigned char>& vchRet);

/*
 *将包含校验和的base58编码字符串（str）解码为字节。
 *vector（vchret），如果解码成功，则返回true
 **/

NODISCARD bool DecodeBase58Check(const std::string& str, std::vector<unsigned char>& vchRet);

#endif //比特币基础58
