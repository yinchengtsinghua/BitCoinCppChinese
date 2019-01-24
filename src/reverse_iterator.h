
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//摘自https://gist.github.com/arvidsson/7231973

#ifndef BITCOIN_REVERSE_ITERATOR_H
#define BITCOIN_REVERSE_ITERATOR_H

/*
 *模板用于反向迭代在C + 11范围为基础的循环。
 *
 *std:：vector<int>v=1，2，3，4，5_
 *对于（auto x:reverse_iterate（v））。
 *标准：：cout<<x<<“”；
 **/


template <typename T>
class reverse_range
{
    T &m_x;

public:
    explicit reverse_range(T &x) : m_x(x) {}

    auto begin() const -> decltype(this->m_x.rbegin())
    {
        return m_x.rbegin();
    }

    auto end() const -> decltype(this->m_x.rend())
    {
        return m_x.rend();
    }
};

template <typename T>
reverse_range<T> reverse_iterate(T &x)
{
    return reverse_range<T>(x);
}

#endif //比特币\反向\迭代器\u h
