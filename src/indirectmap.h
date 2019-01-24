
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2016比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_INDIRECTMAP_H
#define BITCOIN_INDIRECTMAP_H

template <class T>
struct DereferencingComparator { bool operator()(const T a, const T b) const { return *a < *b; } };

/*其键是指针，但通过其未引用的值进行比较的映射。
 *
 *不同于普通标准：：map<const k*，t，dereferencengcomparator<k*>>in
 *使用键进行比较的方法采用k而不是k*
 （取k*会让人困惑，因为它是值而不是地址
 *由于取消引用比较器而重要的用于比较的对象）。
 *
 *键指向的对象不能以任何方式修改
 *解引用比较程序的结果。
 **/

template <class K, class T>
class indirectmap {
private:
    typedef std::map<const K*, T, DereferencingComparator<const K*> > base;
    base m;
public:
    typedef typename base::iterator iterator;
    typedef typename base::const_iterator const_iterator;
    typedef typename base::size_type size_type;
    typedef typename base::value_type value_type;

//直通（指针接口）
    std::pair<iterator, bool> insert(const value_type& value) { return m.insert(value); }

//传递地址（值接口）
    iterator find(const K& key)                     { return m.find(&key); }
    const_iterator find(const K& key) const         { return m.find(&key); }
    iterator lower_bound(const K& key)              { return m.lower_bound(&key); }
    const_iterator lower_bound(const K& key) const  { return m.lower_bound(&key); }
    size_type erase(const K& key)                   { return m.erase(&key); }
    size_type count(const K& key) const             { return m.count(&key); }

//通行证
    bool empty() const              { return m.empty(); }
    size_type size() const          { return m.size(); }
    size_type max_size() const      { return m.max_size(); }
    void clear()                    { m.clear(); }
    iterator begin()                { return m.begin(); }
    iterator end()                  { return m.end(); }
    const_iterator begin() const    { return m.begin(); }
    const_iterator end() const      { return m.end(); }
    const_iterator cbegin() const   { return m.cbegin(); }
    const_iterator cend() const     { return m.cend(); }
};

#endif //比特币间接地图
