
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2012-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_LIMITEDMAP_H
#define BITCOIN_LIMITEDMAP_H

#include <assert.h>
#include <map>

/*类STL的映射容器，只保留具有最高值的n个元素。*/
template <typename K, typename V>
class limitedmap
{
public:
    typedef K key_type;
    typedef V mapped_type;
    typedef std::pair<const key_type, mapped_type> value_type;
    typedef typename std::map<K, V>::const_iterator const_iterator;
    typedef typename std::map<K, V>::size_type size_type;

protected:
    std::map<K, V> map;
    typedef typename std::map<K, V>::iterator iterator;
    std::multimap<V, iterator> rmap;
    typedef typename std::multimap<V, iterator>::iterator rmap_iterator;
    size_type nMaxSize;

public:
    explicit limitedmap(size_type nMaxSizeIn)
    {
        assert(nMaxSizeIn > 0);
        nMaxSize = nMaxSizeIn;
    }
    const_iterator begin() const { return map.begin(); }
    const_iterator end() const { return map.end(); }
    size_type size() const { return map.size(); }
    bool empty() const { return map.empty(); }
    const_iterator find(const key_type& k) const { return map.find(k); }
    size_type count(const key_type& k) const { return map.count(k); }
    void insert(const value_type& x)
    {
        std::pair<iterator, bool> ret = map.insert(x);
        if (ret.second) {
            if (map.size() > nMaxSize) {
                map.erase(rmap.begin()->second);
                rmap.erase(rmap.begin());
            }
            rmap.insert(make_pair(x.second, ret.first));
        }
    }
    void erase(const key_type& k)
    {
        iterator itTarget = map.find(k);
        if (itTarget == map.end())
            return;
        std::pair<rmap_iterator, rmap_iterator> itPair = rmap.equal_range(itTarget->second);
        for (rmap_iterator it = itPair.first; it != itPair.second; ++it)
            if (it->second == itTarget) {
                rmap.erase(it);
                map.erase(itTarget);
                return;
            }
//不该来这里
        assert(0);
    }
    void update(const_iterator itIn, const mapped_type& v)
    {
//使用带有空范围的map:：erase（）而不是map:：find（）来获取非常量迭代器，
//因为它是C++ 11中的一个常数时间操作。有关详细信息，请参阅
//https://stackoverflow.com/questions/765148/how-to-remove-constness-of-const-iterator
        iterator itTarget = map.erase(itIn, itIn);

        if (itTarget == map.end())
            return;
        std::pair<rmap_iterator, rmap_iterator> itPair = rmap.equal_range(itTarget->second);
        for (rmap_iterator it = itPair.first; it != itPair.second; ++it)
            if (it->second == itTarget) {
                rmap.erase(it);
                itTarget->second = v;
                rmap.insert(make_pair(v, itTarget));
                return;
            }
//不该来这里
        assert(0);
    }
    size_type max_size() const { return nMaxSize; }
    size_type max_size(size_type s)
    {
        assert(s > 0);
        while (map.size() > s) {
            map.erase(rmap.begin()->second);
            rmap.erase(rmap.begin());
        }
        nMaxSize = s;
        return nMaxSize;
    }
};

#endif //比特币限额
