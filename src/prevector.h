
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2015-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_PREVECTOR_H
#define BITCOIN_PREVECTOR_H

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <type_traits>

#pragma pack(push, 1)
/*对存储最多n个
 *直接元素（无堆分配）。类型大小和差异是
 *用于存储元素计数，可以是任何无符号+有符号类型。
 *
 *存储布局为：
 *直接分配：
 *-大小\大小：已使用的元素数（介于0和n之间）
 *-t direct[n]：t类型的n个元素数组
 （仅初始化第一个\大小）。
 *间接分配：
 *-大小\大小：使用的元素数加上n+1
 *—大小容量：分配的元素数
 *-t*间接：指向类型t的容量元素数组的指针
 （仅初始化第一个\大小）。
 *
 *数据类型t必须可以通过memmove/realloc（）移动。一旦我们切换到C++，
 *可以改用move构造函数。
 **/

template<unsigned int N, typename T, typename Size = uint32_t, typename Diff = int32_t>
class prevector {
public:
    typedef Size size_type;
    typedef Diff difference_type;
    typedef T value_type;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;

    class iterator {
        T* ptr;
    public:
        typedef Diff difference_type;
        typedef T value_type;
        typedef T* pointer;
        typedef T& reference;
        typedef std::random_access_iterator_tag iterator_category;
        iterator(T* ptr_) : ptr(ptr_) {}
        T& operator*() const { return *ptr; }
        T* operator->() const { return ptr; }
        T& operator[](size_type pos) { return ptr[pos]; }
        const T& operator[](size_type pos) const { return ptr[pos]; }
        iterator& operator++() { ptr++; return *this; }
        iterator& operator--() { ptr--; return *this; }
        iterator operator++(int) { iterator copy(*this); ++(*this); return copy; }
        iterator operator--(int) { iterator copy(*this); --(*this); return copy; }
        difference_type friend operator-(iterator a, iterator b) { return (&(*a) - &(*b)); }
        iterator operator+(size_type n) { return iterator(ptr + n); }
        iterator& operator+=(size_type n) { ptr += n; return *this; }
        iterator operator-(size_type n) { return iterator(ptr - n); }
        iterator& operator-=(size_type n) { ptr -= n; return *this; }
        bool operator==(iterator x) const { return ptr == x.ptr; }
        bool operator!=(iterator x) const { return ptr != x.ptr; }
        bool operator>=(iterator x) const { return ptr >= x.ptr; }
        bool operator<=(iterator x) const { return ptr <= x.ptr; }
        bool operator>(iterator x) const { return ptr > x.ptr; }
        bool operator<(iterator x) const { return ptr < x.ptr; }
    };

    class reverse_iterator {
        T* ptr;
    public:
        typedef Diff difference_type;
        typedef T value_type;
        typedef T* pointer;
        typedef T& reference;
        typedef std::bidirectional_iterator_tag iterator_category;
        reverse_iterator(T* ptr_) : ptr(ptr_) {}
        T& operator*() { return *ptr; }
        const T& operator*() const { return *ptr; }
        T* operator->() { return ptr; }
        const T* operator->() const { return ptr; }
        reverse_iterator& operator--() { ptr++; return *this; }
        reverse_iterator& operator++() { ptr--; return *this; }
        reverse_iterator operator++(int) { reverse_iterator copy(*this); ++(*this); return copy; }
        reverse_iterator operator--(int) { reverse_iterator copy(*this); --(*this); return copy; }
        bool operator==(reverse_iterator x) const { return ptr == x.ptr; }
        bool operator!=(reverse_iterator x) const { return ptr != x.ptr; }
    };

    class const_iterator {
        const T* ptr;
    public:
        typedef Diff difference_type;
        typedef const T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        typedef std::random_access_iterator_tag iterator_category;
        const_iterator(const T* ptr_) : ptr(ptr_) {}
        const_iterator(iterator x) : ptr(&(*x)) {}
        const T& operator*() const { return *ptr; }
        const T* operator->() const { return ptr; }
        const T& operator[](size_type pos) const { return ptr[pos]; }
        const_iterator& operator++() { ptr++; return *this; }
        const_iterator& operator--() { ptr--; return *this; }
        const_iterator operator++(int) { const_iterator copy(*this); ++(*this); return copy; }
        const_iterator operator--(int) { const_iterator copy(*this); --(*this); return copy; }
        difference_type friend operator-(const_iterator a, const_iterator b) { return (&(*a) - &(*b)); }
        const_iterator operator+(size_type n) { return const_iterator(ptr + n); }
        const_iterator& operator+=(size_type n) { ptr += n; return *this; }
        const_iterator operator-(size_type n) { return const_iterator(ptr - n); }
        const_iterator& operator-=(size_type n) { ptr -= n; return *this; }
        bool operator==(const_iterator x) const { return ptr == x.ptr; }
        bool operator!=(const_iterator x) const { return ptr != x.ptr; }
        bool operator>=(const_iterator x) const { return ptr >= x.ptr; }
        bool operator<=(const_iterator x) const { return ptr <= x.ptr; }
        bool operator>(const_iterator x) const { return ptr > x.ptr; }
        bool operator<(const_iterator x) const { return ptr < x.ptr; }
    };

    class const_reverse_iterator {
        const T* ptr;
    public:
        typedef Diff difference_type;
        typedef const T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        typedef std::bidirectional_iterator_tag iterator_category;
        const_reverse_iterator(const T* ptr_) : ptr(ptr_) {}
        const_reverse_iterator(reverse_iterator x) : ptr(&(*x)) {}
        const T& operator*() const { return *ptr; }
        const T* operator->() const { return ptr; }
        const_reverse_iterator& operator--() { ptr++; return *this; }
        const_reverse_iterator& operator++() { ptr--; return *this; }
        const_reverse_iterator operator++(int) { const_reverse_iterator copy(*this); ++(*this); return copy; }
        const_reverse_iterator operator--(int) { const_reverse_iterator copy(*this); --(*this); return copy; }
        bool operator==(const_reverse_iterator x) const { return ptr == x.ptr; }
        bool operator!=(const_reverse_iterator x) const { return ptr != x.ptr; }
    };

private:
    size_type _size;
    union direct_or_indirect {
        char direct[sizeof(T) * N];
        struct {
            size_type capacity;
            char* indirect;
        };
    } _union;

    T* direct_ptr(difference_type pos) { return reinterpret_cast<T*>(_union.direct) + pos; }
    const T* direct_ptr(difference_type pos) const { return reinterpret_cast<const T*>(_union.direct) + pos; }
    T* indirect_ptr(difference_type pos) { return reinterpret_cast<T*>(_union.indirect) + pos; }
    const T* indirect_ptr(difference_type pos) const { return reinterpret_cast<const T*>(_union.indirect) + pos; }
    bool is_direct() const { return _size <= N; }

    void change_capacity(size_type new_capacity) {
        if (new_capacity <= N) {
            if (!is_direct()) {
                T* indirect = indirect_ptr(0);
                T* src = indirect;
                T* dst = direct_ptr(0);
                memcpy(dst, src, size() * sizeof(T));
                free(indirect);
                _size -= N + 1;
            }
        } else {
            if (!is_direct()) {
                /*Fixme:因为这里的malloc/realloc不会调用新的\u处理程序，如果分配失败，断言
                    成功。它们应该使用分配器或new/delete，以便处理程序
                    必要时调用，但这样做会略微降低性能。*/

                _union.indirect = static_cast<char*>(realloc(_union.indirect, ((size_t)sizeof(T)) * new_capacity));
                assert(_union.indirect);
                _union.capacity = new_capacity;
            } else {
                char* new_indirect = static_cast<char*>(malloc(((size_t)sizeof(T)) * new_capacity));
                assert(new_indirect);
                T* src = direct_ptr(0);
                T* dst = reinterpret_cast<T*>(new_indirect);
                memcpy(dst, src, size() * sizeof(T));
                _union.indirect = new_indirect;
                _union.capacity = new_capacity;
                _size += N + 1;
            }
        }
    }

    T* item_ptr(difference_type pos) { return is_direct() ? direct_ptr(pos) : indirect_ptr(pos); }
    const T* item_ptr(difference_type pos) const { return is_direct() ? direct_ptr(pos) : indirect_ptr(pos); }

    void fill(T* dst, ptrdiff_t count, const T& value = T{}) {
        std::fill_n(dst, count, value);
    }

    template<typename InputIterator>
    void fill(T* dst, InputIterator first, InputIterator last) {
        while (first != last) {
            new(static_cast<void*>(dst)) T(*first);
            ++dst;
            ++first;
        }
    }

public:
    void assign(size_type n, const T& val) {
        clear();
        if (capacity() < n) {
            change_capacity(n);
        }
        _size += n;
        fill(item_ptr(0), n, val);
    }

    template<typename InputIterator>
    void assign(InputIterator first, InputIterator last) {
        size_type n = last - first;
        clear();
        if (capacity() < n) {
            change_capacity(n);
        }
        _size += n;
        fill(item_ptr(0), first, last);
    }

    prevector() : _size(0), _union{{}} {}

    explicit prevector(size_type n) : prevector() {
        resize(n);
    }

    explicit prevector(size_type n, const T& val) : prevector() {
        change_capacity(n);
        _size += n;
        fill(item_ptr(0), n, val);
    }

    template<typename InputIterator>
    prevector(InputIterator first, InputIterator last) : prevector() {
        size_type n = last - first;
        change_capacity(n);
        _size += n;
        fill(item_ptr(0), first, last);
    }

    prevector(const prevector<N, T, Size, Diff>& other) : prevector() {
        size_type n = other.size();
        change_capacity(n);
        _size += n;
        fill(item_ptr(0), other.begin(),  other.end());
    }

    prevector(prevector<N, T, Size, Diff>&& other) : prevector() {
        swap(other);
    }

    prevector& operator=(const prevector<N, T, Size, Diff>& other) {
        if (&other == this) {
            return *this;
        }
        assign(other.begin(), other.end());
        return *this;
    }

    prevector& operator=(prevector<N, T, Size, Diff>&& other) {
        swap(other);
        return *this;
    }

    size_type size() const {
        return is_direct() ? _size : _size - N - 1;
    }

    bool empty() const {
        return size() == 0;
    }

    iterator begin() { return iterator(item_ptr(0)); }
    const_iterator begin() const { return const_iterator(item_ptr(0)); }
    iterator end() { return iterator(item_ptr(size())); }
    const_iterator end() const { return const_iterator(item_ptr(size())); }

    reverse_iterator rbegin() { return reverse_iterator(item_ptr(size() - 1)); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(item_ptr(size() - 1)); }
    reverse_iterator rend() { return reverse_iterator(item_ptr(-1)); }
    const_reverse_iterator rend() const { return const_reverse_iterator(item_ptr(-1)); }

    size_t capacity() const {
        if (is_direct()) {
            return N;
        } else {
            return _union.capacity;
        }
    }

    T& operator[](size_type pos) {
        return *item_ptr(pos);
    }

    const T& operator[](size_type pos) const {
        return *item_ptr(pos);
    }

    void resize(size_type new_size) {
        size_type cur_size = size();
        if (cur_size == new_size) {
            return;
        }
        if (cur_size > new_size) {
            erase(item_ptr(new_size), end());
            return;
        }
        if (new_size > capacity()) {
            change_capacity(new_size);
        }
        ptrdiff_t increase = new_size - cur_size;
        fill(item_ptr(cur_size), increase);
        _size += increase;
    }

    void reserve(size_type new_capacity) {
        if (new_capacity > capacity()) {
            change_capacity(new_capacity);
        }
    }

    void shrink_to_fit() {
        change_capacity(size());
    }

    void clear() {
        resize(0);
    }

    iterator insert(iterator pos, const T& value) {
        size_type p = pos - begin();
        size_type new_size = size() + 1;
        if (capacity() < new_size) {
            change_capacity(new_size + (new_size >> 1));
        }
        T* ptr = item_ptr(p);
        memmove(ptr + 1, ptr, (size() - p) * sizeof(T));
        _size++;
        new(static_cast<void*>(ptr)) T(value);
        return iterator(ptr);
    }

    void insert(iterator pos, size_type count, const T& value) {
        size_type p = pos - begin();
        size_type new_size = size() + count;
        if (capacity() < new_size) {
            change_capacity(new_size + (new_size >> 1));
        }
        T* ptr = item_ptr(p);
        memmove(ptr + count, ptr, (size() - p) * sizeof(T));
        _size += count;
        fill(item_ptr(p), count, value);
    }

    template<typename InputIterator>
    void insert(iterator pos, InputIterator first, InputIterator last) {
        size_type p = pos - begin();
        difference_type count = last - first;
        size_type new_size = size() + count;
        if (capacity() < new_size) {
            change_capacity(new_size + (new_size >> 1));
        }
        T* ptr = item_ptr(p);
        memmove(ptr + count, ptr, (size() - p) * sizeof(T));
        _size += count;
        fill(ptr, first, last);
    }

    iterator erase(iterator pos) {
        return erase(pos, pos + 1);
    }

    iterator erase(iterator first, iterator last) {
//不允许擦除来更改对象的容量。那意味着
//当以间接分配的Prevector开始时，
//大小和容量>n，结果可能仍然是间接分配的
//prevector的大小<=n，容量大于n。收缩到fit（）调用是
//必须切换到直接分配的（更高效）
//表示（容量n，大小<=n）。
        iterator p = first;
        char* endp = (char*)&(*end());
        if (!std::is_trivially_destructible<T>::value) {
            while (p != last) {
                (*p).~T();
                _size--;
                ++p;
            }
        } else {
            _size -= last - p;
        }
        memmove(&(*first), &(*last), endp - ((char*)(&(*last))));
        return first;
    }

    void push_back(const T& value) {
        size_type new_size = size() + 1;
        if (capacity() < new_size) {
            change_capacity(new_size + (new_size >> 1));
        }
        new(item_ptr(size())) T(value);
        _size++;
    }

    void pop_back() {
        erase(end() - 1, end());
    }

    T& front() {
        return *item_ptr(0);
    }

    const T& front() const {
        return *item_ptr(0);
    }

    T& back() {
        return *item_ptr(size() - 1);
    }

    const T& back() const {
        return *item_ptr(size() - 1);
    }

    void swap(prevector<N, T, Size, Diff>& other) {
        std::swap(_union, other._union);
        std::swap(_size, other._size);
    }

    ~prevector() {
        if (!std::is_trivially_destructible<T>::value) {
            clear();
        }
        if (!is_direct()) {
            free(_union.indirect);
            _union.indirect = nullptr;
        }
    }

    bool operator==(const prevector<N, T, Size, Diff>& other) const {
        if (other.size() != size()) {
            return false;
        }
        const_iterator b1 = begin();
        const_iterator b2 = other.begin();
        const_iterator e1 = end();
        while (b1 != e1) {
            if ((*b1) != (*b2)) {
                return false;
            }
            ++b1;
            ++b2;
        }
        return true;
    }

    bool operator!=(const prevector<N, T, Size, Diff>& other) const {
        return !(*this == other);
    }

    bool operator<(const prevector<N, T, Size, Diff>& other) const {
        if (size() < other.size()) {
            return true;
        }
        if (size() > other.size()) {
            return false;
        }
        const_iterator b1 = begin();
        const_iterator b2 = other.begin();
        const_iterator e1 = end();
        while (b1 != e1) {
            if ((*b1) < (*b2)) {
                return true;
            }
            if ((*b2) < (*b1)) {
                return false;
            }
            ++b1;
            ++b2;
        }
        return false;
    }

    size_t allocated_memory() const {
        if (is_direct()) {
            return 0;
        } else {
            return ((size_t)(sizeof(T))) * _union.capacity;
        }
    }

    value_type* data() {
        return item_ptr(0);
    }

    const value_type* data() const {
        return item_ptr(0);
    }
};
#pragma pack(pop)

#endif //比特币
