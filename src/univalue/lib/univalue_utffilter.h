
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有2016 Wladimir J.van der Laan
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。
#ifndef UNIVALUE_UTFFILTER_H
#define UNIVALUE_UTFFILTER_H

#include <string>

/*
 *生成和验证UTF-8以及整理UTF-16的筛选器
 *RFC4627中规定的替代物对。
 **/

class JSONUTF8StringFilter
{
public:
    explicit JSONUTF8StringFilter(std::string &s):
        str(s), is_valid(true), codepoint(0), state(0), surpair(0)
    {
    }
//写入单个8位字符（可能是UTF-8序列的一部分）
    void push_back(unsigned char ch)
    {
        if (state == 0) {
if (ch < 0x80) //7位ASCII，快速直接通过
                str.push_back(ch);
else if (ch < 0xc0) //中间序列字符，在此状态下无效
                is_valid = false;
else if (ch < 0xe0) { //2字节序列的开始
                codepoint = (ch & 0x1f) << 6;
                state = 6;
} else if (ch < 0xf0) { //3字节序列的开始
                codepoint = (ch & 0x0f) << 12;
                state = 12;
} else if (ch < 0xf8) { //4字节序列的开始
                codepoint = (ch & 0x07) << 18;
                state = 18;
} else //保留，无效
                is_valid = false;
        } else {
if ((ch & 0xc0) != 0x80) //不是延续，无效
                is_valid = false;
            state -= 6;
            codepoint |= (ch & 0x3f) << state;
            if (state == 0)
                push_back_u(codepoint);
        }
    }
//直接编写代码点，可能对代理项对进行排序
    void push_back_u(unsigned int codepoint_)
    {
if (state) //仅接受处于打开状态的完整代码点
            is_valid = false;
if (codepoint_ >= 0xD800 && codepoint_ < 0xDC00) { //代理对的前半部分
if (surpair) //两个后续的代理对开启器-失败
                is_valid = false;
            else
                surpair = codepoint_;
} else if (codepoint_ >= 0xDC00 && codepoint_ < 0xE000) { //代理项对的后半部分
if (surpair) { //打开代理项对，预计下半部分
//从UTF-16代理项对计算代码点
                append_codepoint(0x10000 | ((surpair - 0xD800)<<10) | (codepoint_ - 0xDC00));
                surpair = 0;
} else //下半场不在上半场之后-失败
                is_valid = false;
        } else {
if (surpair) //代理对的前半部分不后面跟第二个-失败
                is_valid = false;
            else
                append_codepoint(codepoint_);
        }
    }
//检查是否处于可以结束字符串的状态
//没有打开的序列，没有打开的代理项对等
    bool finalize()
    {
        if (state || surpair)
            is_valid = false;
        return is_valid;
    }
private:
    std::string &str;
    bool is_valid;
//当前UTF-8解码状态
    unsigned int codepoint;
int state; //下一个utf-8字节要填充的顶位，或0

//跟踪以下状态以处理的以下部分
//RCF4627：
//
//转义不在基本多语言中的扩展字符
//平面，字符表示为12个字符的序列，
//编码UTF-16代理项对。例如，一个字符串
//仅包含g-clef字符（u+1d11e）可以表示为
//“\UD834 \UDD1E”。
//
//后面两个\u…。可能必须用一个实际的代码点替换。
unsigned int surpair; //打开的UTF-16代理对的前半部分，或0

    void append_codepoint(unsigned int codepoint_)
    {
        if (codepoint_ <= 0x7f)
            str.push_back((char)codepoint_);
        else if (codepoint_ <= 0x7FF) {
            str.push_back((char)(0xC0 | (codepoint_ >> 6)));
            str.push_back((char)(0x80 | (codepoint_ & 0x3F)));
        } else if (codepoint_ <= 0xFFFF) {
            str.push_back((char)(0xE0 | (codepoint_ >> 12)));
            str.push_back((char)(0x80 | ((codepoint_ >> 6) & 0x3F)));
            str.push_back((char)(0x80 | (codepoint_ & 0x3F)));
        } else if (codepoint_ <= 0x1FFFFF) {
            str.push_back((char)(0xF0 | (codepoint_ >> 18)));
            str.push_back((char)(0x80 | ((codepoint_ >> 12) & 0x3F)));
            str.push_back((char)(0x80 | ((codepoint_ >> 6) & 0x3F)));
            str.push_back((char)(0x80 | (codepoint_ & 0x3F)));
        }
    }
};

#endif
