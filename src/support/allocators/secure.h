
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

#ifndef BITCOIN_SUPPORT_ALLOCATORS_SECURE_H
#define BITCOIN_SUPPORT_ALLOCATORS_SECURE_H

#include <support/lockedpool.h>
#include <support/cleanse.h>

#include <string>

//
//阻止其内容被分页的分配器
//内存不足，在删除前清除其内容。
//
template <typename T>
struct secure_allocator : public std::allocator<T> {
//MSVC8默认复制构造函数已损坏
    typedef std::allocator<T> base;
    typedef typename base::size_type size_type;
    typedef typename base::difference_type difference_type;
    typedef typename base::pointer pointer;
    typedef typename base::const_pointer const_pointer;
    typedef typename base::reference reference;
    typedef typename base::const_reference const_reference;
    typedef typename base::value_type value_type;
    secure_allocator() noexcept {}
    secure_allocator(const secure_allocator& a) noexcept : base(a) {}
    template <typename U>
    secure_allocator(const secure_allocator<U>& a) noexcept : base(a)
    {
    }
    ~secure_allocator() noexcept {}
    template <typename _Other>
    struct rebind {
        typedef secure_allocator<_Other> other;
    };

    T* allocate(std::size_t n, const void* hint = 0)
    {
        T* allocation = static_cast<T*>(LockedPoolManager::Instance().alloc(sizeof(T) * n));
        if (!allocation) {
            throw std::bad_alloc();
        }
        return allocation;
    }

    void deallocate(T* p, std::size_t n)
    {
        if (p != nullptr) {
            memory_cleanse(p, sizeof(T) * n);
        }
        LockedPoolManager::Instance().free(p);
    }
};

//这与std：：string完全相同，但使用自定义分配器。
typedef std::basic_string<char, std::char_traits<char>, secure_allocator<char> > SecureString;

#endif //比特币支持分配程序安全
