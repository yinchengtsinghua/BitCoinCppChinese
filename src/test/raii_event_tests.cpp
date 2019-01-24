
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2016-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <event2/event.h>

#ifdef EVENT_SET_MEM_FUNCTIONS_IMPLEMENTED
//定义报告被跳过的虚拟测试可能是理想的，但是Boost:：Test似乎没有实现这一点（至少在公共发行版可用的版本中没有实现这一点）。

#include <map>
#include <stdlib.h>

#include <support/events.h>

#include <test/test_bitcoin.h>

#include <vector>

#include <boost/test/unit_test.hpp>

static std::map<void*, short> tags;
static std::map<void*, uint16_t> orders;
static uint16_t tagSequence = 0;

static void* tag_malloc(size_t sz) {
    void* mem = malloc(sz);
    if (!mem) return mem;
    tags[mem]++;
    orders[mem] = tagSequence++;
    return mem;
}

static void tag_free(void* mem) {
    tags[mem]--;
    orders[mem] = tagSequence++;
    free(mem);
}

BOOST_FIXTURE_TEST_SUITE(raii_event_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(raii_event_creation)
{
    event_set_mem_functions(tag_malloc, realloc, tag_free);

    void* base_ptr = nullptr;
    {
        auto base = obtain_event_base();
        base_ptr = (void*)base.get();
        BOOST_CHECK(tags[base_ptr] == 1);
    }
    BOOST_CHECK(tags[base_ptr] == 0);

    void* event_ptr = nullptr;
    {
        auto base = obtain_event_base();
        auto event = obtain_event(base.get(), -1, 0, nullptr, nullptr);

        base_ptr = (void*)base.get();
        event_ptr = (void*)event.get();

        BOOST_CHECK(tags[base_ptr] == 1);
        BOOST_CHECK(tags[event_ptr] == 1);
    }
    BOOST_CHECK(tags[base_ptr] == 0);
    BOOST_CHECK(tags[event_ptr] == 0);

    event_set_mem_functions(malloc, realloc, free);
}

BOOST_AUTO_TEST_CASE(raii_event_order)
{
    event_set_mem_functions(tag_malloc, realloc, tag_free);

    void* base_ptr = nullptr;
    void* event_ptr = nullptr;
    {
        auto base = obtain_event_base();
        auto event = obtain_event(base.get(), -1, 0, nullptr, nullptr);

        base_ptr = (void*)base.get();
        event_ptr = (void*)event.get();

//基应在事件之前分配
        BOOST_CHECK(orders[base_ptr] < orders[event_ptr]);
    }
//事件后应释放基
    BOOST_CHECK(orders[base_ptr] > orders[event_ptr]);

    event_set_mem_functions(malloc, realloc, free);
}

BOOST_AUTO_TEST_SUITE_END()

#endif  //事件\设置\内存\功能\已实现
