
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

//测试问题200：当迭代器从后向切换时
//要转发，如果新的
//在当前键之前添加了突变。

#include "leveldb/db.h"
#include "util/testharness.h"

namespace leveldb {

class Issue200 { };

TEST(Issue200, Test) {
//从旧的运行中摆脱任何状态。
  std::string dbpath = test::TmpDir() + "/leveldb_issue200_test";
  DestroyDB(dbpath, Options());

  DB *db;
  Options options;
  options.create_if_missing = true;
  ASSERT_OK(DB::Open(options, dbpath, &db));

  WriteOptions write_options;
  ASSERT_OK(db->Put(write_options, "1", "b"));
  ASSERT_OK(db->Put(write_options, "2", "c"));
  ASSERT_OK(db->Put(write_options, "3", "d"));
  ASSERT_OK(db->Put(write_options, "4", "e"));
  ASSERT_OK(db->Put(write_options, "5", "f"));

  ReadOptions read_options;
  Iterator *iter = db->NewIterator(read_options);

//添加一个不应在迭代器中反映的元素。
  ASSERT_OK(db->Put(write_options, "25", "cd"));

  iter->Seek("5");
  ASSERT_EQ(iter->key().ToString(), "5");
  iter->Prev();
  ASSERT_EQ(iter->key().ToString(), "4");
  iter->Prev();
  ASSERT_EQ(iter->key().ToString(), "3");
  iter->Next();
  ASSERT_EQ(iter->key().ToString(), "4");
  iter->Next();
  ASSERT_EQ(iter->key().ToString(), "5");

  delete iter;
  delete db;
  DestroyDB(dbpath, options);
}

}  //命名空间级别数据库

int main(int argc, char** argv) {
  return leveldb::test::RunAllTests();
}
