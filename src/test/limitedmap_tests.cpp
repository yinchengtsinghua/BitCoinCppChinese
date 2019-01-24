
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

#include <limitedmap.h>

#include <test/test_bitcoin.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(limitedmap_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(limitedmap_test)
{
//创建一个上限为10项的限制映射
    limitedmap<int, int> map(10);

//检查最大尺寸是否为10
    BOOST_CHECK(map.max_size() == 10);

//检查它是空的
    BOOST_CHECK(map.size() == 0);

//插入（- 1，- 1）
    map.insert(std::pair<int, int>(-1, -1));

//确保尺寸已更新
    BOOST_CHECK(map.size() == 1);

//确保新项目在地图中
    BOOST_CHECK(map.count(-1) == 1);

//插入10个新项目
    for (int i = 0; i < 10; i++) {
        map.insert(std::pair<int, int>(i, i + 1));
    }

//确保地图现在包含10个项目…
    BOOST_CHECK(map.size() == 10);

//…第一件物品已被丢弃
    BOOST_CHECK(map.count(-1) == 0);

//使用索引和迭代器遍历映射
    limitedmap<int, int>::const_iterator it = map.begin();
    for (int i = 0; i < 10; i++) {
//确保物品存在
        BOOST_CHECK(map.count(i) == 1);

//使用迭代器检查预期的键和值
        BOOST_CHECK(it->first == i);
        BOOST_CHECK(it->second == i + 1);

//使用“查找”检查值
        BOOST_CHECK(map.find(i)->second == i + 1);

//更新并重新检查
        map.update(it, i + 2);
        BOOST_CHECK(map.find(i)->second == i + 2);

        it++;
    }

//检查我们是否耗尽了迭代器
    BOOST_CHECK(it == map.end());

//将地图大小调整为5个项目
    map.max_size(5);

//检查最大尺寸和尺寸现在是5
    BOOST_CHECK(map.max_size() == 5);
    BOOST_CHECK(map.size() == 5);

//检查是否已丢弃少于5项
//保留大于5项
    for (int i = 0; i < 10; i++) {
        if (i < 5) {
            BOOST_CHECK(map.count(i) == 0);
        } else {
            BOOST_CHECK(map.count(i) == 1);
        }
    }

//删除地图上没有的项目
    for (int i = 100; i < 1000; i += 100) {
        map.erase(i);
    }

//检查尺寸是否不受影响
    BOOST_CHECK(map.size() == 5);

//删除其余元素
    for (int i = 5; i < 10; i++) {
        map.erase(i);
    }

//检查地图现在是空的
    BOOST_CHECK(map.empty());
}

BOOST_AUTO_TEST_SUITE_END()
