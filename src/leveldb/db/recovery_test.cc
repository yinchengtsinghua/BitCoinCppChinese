
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2014 LevelDB作者。版权所有。
//此源代码的使用受可以
//在许可证文件中找到。有关参与者的名称，请参阅作者文件。

#include "db/db_impl.h"
#include "db/filename.h"
#include "db/version_set.h"
#include "db/write_batch_internal.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/write_batch.h"
#include "util/logging.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace leveldb {

class RecoveryTest {
 public:
  RecoveryTest() : env_(Env::Default()), db_(NULL) {
    dbname_ = test::TmpDir() + "/recovery_test";
    DestroyDB(dbname_, Options());
    Open();
  }

  ~RecoveryTest() {
    Close();
    DestroyDB(dbname_, Options());
  }

  DBImpl* dbfull() const { return reinterpret_cast<DBImpl*>(db_); }
  Env* env() const { return env_; }

  bool CanAppend() {
    WritableFile* tmp;
    Status s = env_->NewAppendableFile(CurrentFileName(dbname_), &tmp);
    delete tmp;
    if (s.IsNotSupportedError()) {
      return false;
    } else {
      return true;
    }
  }

  void Close() {
    delete db_;
    db_ = NULL;
  }

  void Open(Options* options = NULL) {
    Close();
    Options opts;
    if (options != NULL) {
      opts = *options;
    } else {
opts.reuse_logs = true;  //托多（桑杰）：双向测试
      opts.create_if_missing = true;
    }
    if (opts.env == NULL) {
      opts.env = env_;
    }
    ASSERT_OK(DB::Open(opts, dbname_, &db_));
    ASSERT_EQ(1, NumLogs());
  }

  Status Put(const std::string& k, const std::string& v) {
    return db_->Put(WriteOptions(), k, v);
  }

  std::string Get(const std::string& k, const Snapshot* snapshot = NULL) {
    std::string result;
    Status s = db_->Get(ReadOptions(), k, &result);
    if (s.IsNotFound()) {
      result = "NOT_FOUND";
    } else if (!s.ok()) {
      result = s.ToString();
    }
    return result;
  }

  std::string ManifestFileName() {
    std::string current;
    ASSERT_OK(ReadFileToString(env_, CurrentFileName(dbname_), &current));
    size_t len = current.size();
    if (len > 0 && current[len-1] == '\n') {
      current.resize(len - 1);
    }
    return dbname_ + "/" + current;
  }

  std::string LogName(uint64_t number) {
    return LogFileName(dbname_, number);
  }

  size_t DeleteLogFiles() {
    std::vector<uint64_t> logs = GetFiles(kLogFile);
    for (size_t i = 0; i < logs.size(); i++) {
      ASSERT_OK(env_->DeleteFile(LogName(logs[i]))) << LogName(logs[i]);
    }
    return logs.size();
  }

  uint64_t FirstLogFile() {
    return GetFiles(kLogFile)[0];
  }

  std::vector<uint64_t> GetFiles(FileType t) {
    std::vector<std::string> filenames;
    ASSERT_OK(env_->GetChildren(dbname_, &filenames));
    std::vector<uint64_t> result;
    for (size_t i = 0; i < filenames.size(); i++) {
      uint64_t number;
      FileType type;
      if (ParseFileName(filenames[i], &number, &type) && type == t) {
        result.push_back(number);
      }
    }
    return result;
  }

  int NumLogs() {
    return GetFiles(kLogFile).size();
  }

  int NumTables() {
    return GetFiles(kTableFile).size();
  }

  uint64_t FileSize(const std::string& fname) {
    uint64_t result;
    ASSERT_OK(env_->GetFileSize(fname, &result)) << fname;
    return result;
  }

  void CompactMemTable() {
    dbfull()->TEST_CompactMemTable();
  }

//直接构造一个将键设置为val的日志文件。
  void MakeLogFile(uint64_t lognum, SequenceNumber seq, Slice key, Slice val) {
    std::string fname = LogFileName(dbname_, lognum);
    WritableFile* file;
    ASSERT_OK(env_->NewWritableFile(fname, &file));
    log::Writer writer(file);
    WriteBatch batch;
    batch.Put(key, val);
    WriteBatchInternal::SetSequence(&batch, seq);
    ASSERT_OK(writer.AddRecord(WriteBatchInternal::Contents(&batch)));
    ASSERT_OK(file->Flush());
    delete file;
  }

 private:
  std::string dbname_;
  Env* env_;
  DB* db_;
};

TEST(RecoveryTest, ManifestReused) {
  if (!CanAppend()) {
    fprintf(stderr, "skipping test because env does not support appending\n");
    return;
  }
  ASSERT_OK(Put("foo", "bar"));
  Close();
  std::string old_manifest = ManifestFileName();
  Open();
  ASSERT_EQ(old_manifest, ManifestFileName());
  ASSERT_EQ("bar", Get("foo"));
  Open();
  ASSERT_EQ(old_manifest, ManifestFileName());
  ASSERT_EQ("bar", Get("foo"));
}

TEST(RecoveryTest, LargeManifestCompacted) {
  if (!CanAppend()) {
    fprintf(stderr, "skipping test because env does not support appending\n");
    return;
  }
  ASSERT_OK(Put("foo", "bar"));
  Close();
  std::string old_manifest = ManifestFileName();

//用零填充以使清单文件非常大。
  {
    uint64_t len = FileSize(old_manifest);
    WritableFile* file;
    ASSERT_OK(env()->NewAppendableFile(old_manifest, &file));
    std::string zeroes(3*1048576 - static_cast<size_t>(len), 0);
    ASSERT_OK(file->Append(zeroes));
    ASSERT_OK(file->Flush());
    delete file;
  }

  Open();
  std::string new_manifest = ManifestFileName();
  ASSERT_NE(old_manifest, new_manifest);
  ASSERT_GT(10000, FileSize(new_manifest));
  ASSERT_EQ("bar", Get("foo"));

  Open();
  ASSERT_EQ(new_manifest, ManifestFileName());
  ASSERT_EQ("bar", Get("foo"));
}

TEST(RecoveryTest, NoLogFiles) {
  ASSERT_OK(Put("foo", "bar"));
  ASSERT_EQ(1, DeleteLogFiles());
  Open();
  ASSERT_EQ("NOT_FOUND", Get("foo"));
  Open();
  ASSERT_EQ("NOT_FOUND", Get("foo"));
}

TEST(RecoveryTest, LogFileReuse) {
  if (!CanAppend()) {
    fprintf(stderr, "skipping test because env does not support appending\n");
    return;
  }
  for (int i = 0; i < 2; i++) {
    ASSERT_OK(Put("foo", "bar"));
    if (i == 0) {
//压缩以确保当前日志为空
      CompactMemTable();
    }
    Close();
    ASSERT_EQ(1, NumLogs());
    uint64_t number = FirstLogFile();
    if (i == 0) {
      ASSERT_EQ(0, FileSize(LogName(number)));
    } else {
      ASSERT_LT(0, FileSize(LogName(number)));
    }
    Open();
    ASSERT_EQ(1, NumLogs());
    ASSERT_EQ(number, FirstLogFile()) << "did not reuse log file";
    ASSERT_EQ("bar", Get("foo"));
    Open();
    ASSERT_EQ(1, NumLogs());
    ASSERT_EQ(number, FirstLogFile()) << "did not reuse log file";
    ASSERT_EQ("bar", Get("foo"));
  }
}

TEST(RecoveryTest, MultipleMemTables) {
//做一个大圆木。
  const int kNum = 1000;
  for (int i = 0; i < kNum; i++) {
    char buf[100];
    snprintf(buf, sizeof(buf), "%050d", i);
    ASSERT_OK(Put(buf, buf));
  }
  ASSERT_EQ(0, NumTables());
  Close();
  ASSERT_EQ(0, NumTables());
  ASSERT_EQ(1, NumLogs());
  uint64_t old_log_file = FirstLogFile();

//通过减小写缓冲区大小强制创建多个memtable。
  Options opt;
  opt.reuse_logs = true;
  opt.write_buffer_size = (kNum*100) / 2;
  Open(&opt);
  ASSERT_LE(2, NumTables());
  ASSERT_EQ(1, NumLogs());
  ASSERT_NE(old_log_file, FirstLogFile()) << "must not reuse log";
  for (int i = 0; i < kNum; i++) {
    char buf[100];
    snprintf(buf, sizeof(buf), "%050d", i);
    ASSERT_EQ(buf, Get(buf));
  }
}

TEST(RecoveryTest, MultipleLogFiles) {
  ASSERT_OK(Put("foo", "bar"));
  Close();
  ASSERT_EQ(1, NumLogs());

//制作一堆未压缩的日志文件。
  uint64_t old_log = FirstLogFile();
  MakeLogFile(old_log+1, 1000, "hello", "world");
  MakeLogFile(old_log+2, 1001, "hi", "there");
  MakeLogFile(old_log+3, 1002, "foo", "bar2");

//恢复并检查所有日志文件是否已处理。
  Open();
  ASSERT_LE(1, NumTables());
  ASSERT_EQ(1, NumLogs());
  uint64_t new_log = FirstLogFile();
  ASSERT_LE(old_log+3, new_log);
  ASSERT_EQ("bar2", Get("foo"));
  ASSERT_EQ("world", Get("hello"));
  ASSERT_EQ("there", Get("hi"));

//测试以前的恢复是否产生可恢复状态。
  Open();
  ASSERT_LE(1, NumTables());
  ASSERT_EQ(1, NumLogs());
  if (CanAppend()) {
    ASSERT_EQ(new_log, FirstLogFile());
  }
  ASSERT_EQ("bar2", Get("foo"));
  ASSERT_EQ("world", Get("hello"));
  ASSERT_EQ("there", Get("hi"));

//检查引入一个较旧的日志文件是否不会导致它被重新读取。
  Close();
  MakeLogFile(old_log+1, 2000, "hello", "stale write");
  Open();
  ASSERT_LE(1, NumTables());
  ASSERT_EQ(1, NumLogs());
  if (CanAppend()) {
    ASSERT_EQ(new_log, FirstLogFile());
  }
  ASSERT_EQ("bar2", Get("foo"));
  ASSERT_EQ("world", Get("hello"));
  ASSERT_EQ("there", Get("hi"));
}

}  //命名空间级别数据库

int main(int argc, char** argv) {
  return leveldb::test::RunAllTests();
}
