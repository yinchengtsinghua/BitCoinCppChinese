
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2013 LevelDB作者。版权所有。
//此源代码的使用受可以
//在许可证文件中找到。有关参与者的名称，请参阅作者文件。

#include "leveldb/db.h"
#include "db/db_impl.h"
#include "leveldb/cache.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace leveldb {

class AutoCompactTest {
 public:
  std::string dbname_;
  Cache* tiny_cache_;
  Options options_;
  DB* db_;

  AutoCompactTest() {
    dbname_ = test::TmpDir() + "/autocompact_test";
    tiny_cache_ = NewLRUCache(100);
    options_.block_cache = tiny_cache_;
    DestroyDB(dbname_, options_);
    options_.create_if_missing = true;
    options_.compression = kNoCompression;
    ASSERT_OK(DB::Open(options_, dbname_, &db_));
  }

  ~AutoCompactTest() {
    delete db_;
    DestroyDB(dbname_, Options());
    delete tiny_cache_;
  }

  std::string Key(int i) {
    char buf[100];
    snprintf(buf, sizeof(buf), "key%06d", i);
    return std::string(buf);
  }

  uint64_t Size(const Slice& start, const Slice& limit) {
    Range r(start, limit);
    uint64_t size;
    db_->GetApproximateSizes(&r, 1, &size);
    return size;
  }

  void DoReads(int n);
};

static const int kValueSize = 200 * 1024;
static const int kTotalSize = 100 * 1024 * 1024;
static const int kCount = kTotalSize / kValueSize;

//反复阅读前n个键并检查它们是否
//压实（通过检查密钥空间的大小进行验证）。
void AutoCompactTest::DoReads(int n) {
  std::string value(kValueSize, 'x');
  DBImpl* dbi = reinterpret_cast<DBImpl*>(db_);

//填充数据库
  for (int i = 0; i < kCount; i++) {
    ASSERT_OK(db_->Put(WriteOptions(), Key(i), value));
  }
  ASSERT_OK(dbi->TEST_CompactMemTable());

//删除所有内容
  for (int i = 0; i < kCount; i++) {
    ASSERT_OK(db_->Delete(WriteOptions(), Key(i)));
  }
  ASSERT_OK(dbi->TEST_CompactMemTable());

//获取我们将要读取的空间的初始测量值。
  const int64_t initial_size = Size(Key(0), Key(n));
  const int64_t initial_other_size = Size(Key(n), Key(kCount));

//阅读直到尺寸显著下降。
  std::string limit_key = Key(n);
  for (int read = 0; true; read++) {
    ASSERT_LT(read, 100) << "Taking too long to compact";
    Iterator* iter = db_->NewIterator(ReadOptions());
    for (iter->SeekToFirst();
         iter->Valid() && iter->key().ToString() < limit_key;
         iter->Next()) {
//删除数据
    }
    delete iter;
//稍等，让任何触发的压缩完成。
    Env::Default()->SleepForMicroseconds(1000000);
    uint64_t size = Size(Key(0), Key(n));
    fprintf(stderr, "iter %3d => %7.3f MB [other %7.3f MB]\n",
            read+1, size/1048576.0, Size(Key(n), Key(kCount))/1048576.0);
    if (size <= initial_size/10) {
      break;
    }
  }

//验证读操作未触及的密钥空间大小
//几乎没有变化。
  const int64_t final_other_size = Size(Key(n), Key(kCount));
  ASSERT_LE(final_other_size, initial_other_size + 1048576);
  ASSERT_GE(final_other_size, initial_other_size/5 - 1048576);
}

TEST(AutoCompactTest, ReadAll) {
  DoReads(kCount);
}

TEST(AutoCompactTest, ReadHalf) {
  DoReads(kCount/2);
}

}  //命名空间级别数据库

int main(int argc, char** argv) {
  return leveldb::test::RunAllTests();
}
