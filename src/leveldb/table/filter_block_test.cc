
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2012 LevelDB作者。版权所有。
//此源代码的使用受可以
//在许可证文件中找到。有关参与者的名称，请参阅作者文件。

#include "table/filter_block.h"

#include "leveldb/filter_policy.h"
#include "util/coding.h"
#include "util/hash.h"
#include "util/logging.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace leveldb {

//测试：发出一个数组，每个键有一个哈希值
class TestHashFilter : public FilterPolicy {
 public:
  virtual const char* Name() const {
    return "TestHashFilter";
  }

  virtual void CreateFilter(const Slice* keys, int n, std::string* dst) const {
    for (int i = 0; i < n; i++) {
      uint32_t h = Hash(keys[i].data(), keys[i].size(), 1);
      PutFixed32(dst, h);
    }
  }

  virtual bool KeyMayMatch(const Slice& key, const Slice& filter) const {
    uint32_t h = Hash(key.data(), key.size(), 1);
    for (size_t i = 0; i + 4 <= filter.size(); i += 4) {
      if (h == DecodeFixed32(filter.data() + i)) {
        return true;
      }
    }
    return false;
  }
};

class FilterBlockTest {
 public:
  TestHashFilter policy_;
};

TEST(FilterBlockTest, EmptyBuilder) {
  FilterBlockBuilder builder(&policy_);
  Slice block = builder.Finish();
  ASSERT_EQ("\\x00\\x00\\x00\\x00\\x0b", EscapeString(block));
  FilterBlockReader reader(&policy_, block);
  ASSERT_TRUE(reader.KeyMayMatch(0, "foo"));
  ASSERT_TRUE(reader.KeyMayMatch(100000, "foo"));
}

TEST(FilterBlockTest, SingleChunk) {
  FilterBlockBuilder builder(&policy_);
  builder.StartBlock(100);
  builder.AddKey("foo");
  builder.AddKey("bar");
  builder.AddKey("box");
  builder.StartBlock(200);
  builder.AddKey("box");
  builder.StartBlock(300);
  builder.AddKey("hello");
  Slice block = builder.Finish();
  FilterBlockReader reader(&policy_, block);
  ASSERT_TRUE(reader.KeyMayMatch(100, "foo"));
  ASSERT_TRUE(reader.KeyMayMatch(100, "bar"));
  ASSERT_TRUE(reader.KeyMayMatch(100, "box"));
  ASSERT_TRUE(reader.KeyMayMatch(100, "hello"));
  ASSERT_TRUE(reader.KeyMayMatch(100, "foo"));
  ASSERT_TRUE(! reader.KeyMayMatch(100, "missing"));
  ASSERT_TRUE(! reader.KeyMayMatch(100, "other"));
}

TEST(FilterBlockTest, MultiChunk) {
  FilterBlockBuilder builder(&policy_);

//第一滤波器
  builder.StartBlock(0);
  builder.AddKey("foo");
  builder.StartBlock(2000);
  builder.AddKey("bar");

//第二滤波器
  builder.StartBlock(3100);
  builder.AddKey("box");

//第三个筛选器为空

//最后过滤器
  builder.StartBlock(9000);
  builder.AddKey("box");
  builder.AddKey("hello");

  Slice block = builder.Finish();
  FilterBlockReader reader(&policy_, block);

//检查第一个过滤器
  ASSERT_TRUE(reader.KeyMayMatch(0, "foo"));
  ASSERT_TRUE(reader.KeyMayMatch(2000, "bar"));
  ASSERT_TRUE(! reader.KeyMayMatch(0, "box"));
  ASSERT_TRUE(! reader.KeyMayMatch(0, "hello"));

//检查第二个过滤器
  ASSERT_TRUE(reader.KeyMayMatch(3100, "box"));
  ASSERT_TRUE(! reader.KeyMayMatch(3100, "foo"));
  ASSERT_TRUE(! reader.KeyMayMatch(3100, "bar"));
  ASSERT_TRUE(! reader.KeyMayMatch(3100, "hello"));

//检查第三个过滤器（空）
  ASSERT_TRUE(! reader.KeyMayMatch(4100, "foo"));
  ASSERT_TRUE(! reader.KeyMayMatch(4100, "bar"));
  ASSERT_TRUE(! reader.KeyMayMatch(4100, "box"));
  ASSERT_TRUE(! reader.KeyMayMatch(4100, "hello"));

//检查最后一个筛选器
  ASSERT_TRUE(reader.KeyMayMatch(9000, "box"));
  ASSERT_TRUE(reader.KeyMayMatch(9000, "hello"));
  ASSERT_TRUE(! reader.KeyMayMatch(9000, "foo"));
  ASSERT_TRUE(! reader.KeyMayMatch(9000, "bar"));
}

}  //命名空间级别数据库

int main(int argc, char** argv) {
  return leveldb::test::RunAllTests();
}
