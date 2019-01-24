
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

#ifndef BITCOIN_SPAN_H
#define BITCOIN_SPAN_H

#include <type_traits>
#include <cstddef>
#include <algorithm>

/*跨度是可以引用连续对象序列的对象。
 *
 *它实现了C++ 20的STD::SPAN子集。
 **/

template<typename C>
class Span
{
    C* m_data;
    std::ptrdiff_t m_size;

public:
    constexpr Span() noexcept : m_data(nullptr), m_size(0) {}
    constexpr Span(C* data, std::ptrdiff_t size) noexcept : m_data(data), m_size(size) {}
    constexpr Span(C* data, C* end) noexcept : m_data(data), m_size(end - data) {}

    constexpr C* data() const noexcept { return m_data; }
    constexpr C* begin() const noexcept { return m_data; }
    constexpr C* end() const noexcept { return m_data + m_size; }
    constexpr std::ptrdiff_t size() const noexcept { return m_size; }
    constexpr C& operator[](std::ptrdiff_t pos) const noexcept { return m_data[pos]; }

    constexpr Span<C> subspan(std::ptrdiff_t offset) const noexcept { return Span<C>(m_data + offset, m_size - offset); }
    constexpr Span<C> subspan(std::ptrdiff_t offset, std::ptrdiff_t count) const noexcept { return Span<C>(m_data + offset, count); }
    constexpr Span<C> first(std::ptrdiff_t count) const noexcept { return Span<C>(m_data, count); }
    constexpr Span<C> last(std::ptrdiff_t count) const noexcept { return Span<C>(m_data + m_size - count, count); }

    friend constexpr bool operator==(const Span& a, const Span& b) noexcept { return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()); }
    friend constexpr bool operator!=(const Span& a, const Span& b) noexcept { return !(a == b); }
    friend constexpr bool operator<(const Span& a, const Span& b) noexcept { return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()); }
    friend constexpr bool operator<=(const Span& a, const Span& b) noexcept { return !(b < a); }
    friend constexpr bool operator>(const Span& a, const Span& b) noexcept { return (b < a); }
    friend constexpr bool operator>=(const Span& a, const Span& b) noexcept { return !(a < b); }
};

/*创建一个容器的范围，该容器公开data（）和size（）。
 *
 *这正确地处理了常量：返回的跨度的元素类型将为
 *whatever data（）返回指向的指针。如果传递的容器为常量，
 *或其元素类型为const，生成的跨度将具有const元素类型。
 *
 *std:：span将有一个直接实现此功能的构造函数。
 **/

template<typename A, int N>
constexpr Span<A> MakeSpan(A (&a)[N]) { return Span<A>(a, N); }

template<typename V>
constexpr Span<typename std::remove_pointer<decltype(std::declval<V>().data())>::type> MakeSpan(V& v) { return Span<typename std::remove_pointer<decltype(std::declval<V>().data())>::type>(v.data(), v.size()); }

#endif
