
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011 LevelDB作者。版权所有。
//此源代码的使用受可以
//在许可证文件中找到。有关参与者的名称，请参阅作者文件。

#include "helpers/memenv/memenv.h"

#include "db/db_impl.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "util/testharness.h"
#include <string>
#include <vector>

namespace leveldb {

class MemEnvTest {
 public:
  Env* env_;

  MemEnvTest()
      : env_(NewMemEnv(Env::Default())) {
  }
  ~MemEnvTest() {
    delete env_;
  }
};

TEST(MemEnvTest, Basics) {
  uint64_t file_size;
  WritableFile* writable_file;
  std::vector<std::string> children;

  ASSERT_OK(env_->CreateDir("/dir"));

//检查目录是否为空。
  ASSERT_TRUE(!env_->FileExists("/dir/non_existent"));
  ASSERT_TRUE(!env_->GetFileSize("/dir/non_existent", &file_size).ok());
  ASSERT_OK(env_->GetChildren("/dir", &children));
  ASSERT_EQ(0, children.size());

//创建一个文件。
  ASSERT_OK(env_->NewWritableFile("/dir/f", &writable_file));
  ASSERT_OK(env_->GetFileSize("/dir/f", &file_size));
  ASSERT_EQ(0, file_size);
  delete writable_file;

//检查文件是否存在。
  ASSERT_TRUE(env_->FileExists("/dir/f"));
  ASSERT_OK(env_->GetFileSize("/dir/f", &file_size));
  ASSERT_EQ(0, file_size);
  ASSERT_OK(env_->GetChildren("/dir", &children));
  ASSERT_EQ(1, children.size());
  ASSERT_EQ("f", children[0]);

//写入文件。
  ASSERT_OK(env_->NewWritableFile("/dir/f", &writable_file));
  ASSERT_OK(writable_file->Append("abc"));
  delete writable_file;

//检查附加工作。
  ASSERT_OK(env_->NewAppendableFile("/dir/f", &writable_file));
  ASSERT_OK(env_->GetFileSize("/dir/f", &file_size));
  ASSERT_EQ(3, file_size);
  ASSERT_OK(writable_file->Append("hello"));
  delete writable_file;

//检查预期大小。
  ASSERT_OK(env_->GetFileSize("/dir/f", &file_size));
  ASSERT_EQ(8, file_size);

//检查重命名是否有效。
  ASSERT_TRUE(!env_->RenameFile("/dir/non_existent", "/dir/g").ok());
  ASSERT_OK(env_->RenameFile("/dir/f", "/dir/g"));
  ASSERT_TRUE(!env_->FileExists("/dir/f"));
  ASSERT_TRUE(env_->FileExists("/dir/g"));
  ASSERT_OK(env_->GetFileSize("/dir/g", &file_size));
  ASSERT_EQ(8, file_size);

//检查打开不存在的文件是否失败。
  SequentialFile* seq_file;
  RandomAccessFile* rand_file;
  ASSERT_TRUE(!env_->NewSequentialFile("/dir/non_existent", &seq_file).ok());
  ASSERT_TRUE(!seq_file);
  ASSERT_TRUE(!env_->NewRandomAccessFile("/dir/non_existent", &rand_file).ok());
  ASSERT_TRUE(!rand_file);

//检查删除是否有效。
  ASSERT_TRUE(!env_->DeleteFile("/dir/non_existent").ok());
  ASSERT_OK(env_->DeleteFile("/dir/g"));
  ASSERT_TRUE(!env_->FileExists("/dir/g"));
  ASSERT_OK(env_->GetChildren("/dir", &children));
  ASSERT_EQ(0, children.size());
  ASSERT_OK(env_->DeleteDir("/dir"));
}

TEST(MemEnvTest, ReadWrite) {
  WritableFile* writable_file;
  SequentialFile* seq_file;
  RandomAccessFile* rand_file;
  Slice result;
  char scratch[100];

  ASSERT_OK(env_->CreateDir("/dir"));

  ASSERT_OK(env_->NewWritableFile("/dir/f", &writable_file));
  ASSERT_OK(writable_file->Append("hello "));
  ASSERT_OK(writable_file->Append("world"));
  delete writable_file;

//按顺序阅读。
  ASSERT_OK(env_->NewSequentialFile("/dir/f", &seq_file));
ASSERT_OK(seq_file->Read(5, &result, scratch)); //读“你好”。
  ASSERT_EQ(0, result.compare("hello"));
  ASSERT_OK(seq_file->Skip(1));
ASSERT_OK(seq_file->Read(1000, &result, scratch)); //读“世界”。
  ASSERT_EQ(0, result.compare("world"));
ASSERT_OK(seq_file->Read(1000, &result, scratch)); //试着读完EOF。
  ASSERT_EQ(0, result.size());
ASSERT_OK(seq_file->Skip(100)); //尝试跳过文件结尾。
  ASSERT_OK(seq_file->Read(1000, &result, scratch));
  ASSERT_EQ(0, result.size());
  delete seq_file;

//随机读取。
  ASSERT_OK(env_->NewRandomAccessFile("/dir/f", &rand_file));
ASSERT_OK(rand_file->Read(6, 5, &result, scratch)); //读“世界”。
  ASSERT_EQ(0, result.compare("world"));
ASSERT_OK(rand_file->Read(0, 5, &result, scratch)); //读“你好”。
  ASSERT_EQ(0, result.compare("hello"));
ASSERT_OK(rand_file->Read(10, 100, &result, scratch)); //读“D”。
  ASSERT_EQ(0, result.compare("d"));

//偏移量过高。
  ASSERT_TRUE(!rand_file->Read(1000, 5, &result, scratch).ok());
  delete rand_file;
}

TEST(MemEnvTest, Locks) {
  FileLock* lock;

//这些不是行动，但我们测试它们是否能成功。
  ASSERT_OK(env_->LockFile("some file", &lock));
  ASSERT_OK(env_->UnlockFile(lock));
}

TEST(MemEnvTest, Misc) {
  std::string test_dir;
  ASSERT_OK(env_->GetTestDirectory(&test_dir));
  ASSERT_TRUE(!test_dir.empty());

  WritableFile* writable_file;
  ASSERT_OK(env_->NewWritableFile("/a/b", &writable_file));

//这些不是行动，但我们测试它们是否能成功。
  ASSERT_OK(writable_file->Sync());
  ASSERT_OK(writable_file->Flush());
  ASSERT_OK(writable_file->Close());
  delete writable_file;
}

TEST(MemEnvTest, LargeWrite) {
  const size_t kWriteSize = 300 * 1024;
  char* scratch = new char[kWriteSize * 2];

  std::string write_data;
  for (size_t i = 0; i < kWriteSize; ++i) {
    write_data.append(1, static_cast<char>(i));
  }

  WritableFile* writable_file;
  ASSERT_OK(env_->NewWritableFile("/dir/f", &writable_file));
  ASSERT_OK(writable_file->Append("foo"));
  ASSERT_OK(writable_file->Append(write_data));
  delete writable_file;

  SequentialFile* seq_file;
  Slice result;
  ASSERT_OK(env_->NewSequentialFile("/dir/f", &seq_file));
ASSERT_OK(seq_file->Read(3, &result, scratch)); //读“FO”。
  ASSERT_EQ(0, result.compare("foo"));

  size_t read = 0;
  std::string read_data;
  while (read < kWriteSize) {
    ASSERT_OK(seq_file->Read(kWriteSize - read, &result, scratch));
    read_data.append(result.data(), result.size());
    read += result.size();
  }
  ASSERT_TRUE(write_data == read_data);
  delete seq_file;
  delete [] scratch;
}

TEST(MemEnvTest, DBTest) {
  Options options;
  options.create_if_missing = true;
  options.env = env_;
  DB* db;

  const Slice keys[] = {Slice("aaa"), Slice("bbb"), Slice("ccc")};
  const Slice vals[] = {Slice("foo"), Slice("bar"), Slice("baz")};

  ASSERT_OK(DB::Open(options, "/dir/db", &db));
  for (size_t i = 0; i < 3; ++i) {
    ASSERT_OK(db->Put(WriteOptions(), keys[i], vals[i]));
  }

  for (size_t i = 0; i < 3; ++i) {
    std::string res;
    ASSERT_OK(db->Get(ReadOptions(), keys[i], &res));
    ASSERT_TRUE(res == vals[i]);
  }

  Iterator* iterator = db->NewIterator(ReadOptions());
  iterator->SeekToFirst();
  for (size_t i = 0; i < 3; ++i) {
    ASSERT_TRUE(iterator->Valid());
    ASSERT_TRUE(keys[i] == iterator->key());
    ASSERT_TRUE(vals[i] == iterator->value());
    iterator->Next();
  }
  ASSERT_TRUE(!iterator->Valid());
  delete iterator;

  DBImpl* dbi = reinterpret_cast<DBImpl*>(db);
  ASSERT_OK(dbi->TEST_CompactMemTable());

  for (size_t i = 0; i < 3; ++i) {
    std::string res;
    ASSERT_OK(db->Get(ReadOptions(), keys[i], &res));
    ASSERT_TRUE(res == vals[i]);
  }

  delete db;
}

}  //命名空间级别数据库

int main(int argc, char** argv) {
  return leveldb::test::RunAllTests();
}
