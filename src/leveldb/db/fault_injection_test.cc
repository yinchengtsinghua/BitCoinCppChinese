
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有2014 LevelDB作者。版权所有。
//此源代码的使用受可以
//在许可证文件中找到。有关参与者的名称，请参阅作者文件。

//此测试使用自定义env跟踪文件系统的状态
//最后一个“同步”。然后，它通过故意删除来检查数据丢失错误
//文件数据（或整个文件）不受“同步”保护。

#include "leveldb/db.h"

#include <map>
#include <set>
#include "db/db_impl.h"
#include "db/filename.h"
#include "db/log_format.h"
#include "db/version_set.h"
#include "leveldb/cache.h"
#include "leveldb/env.h"
#include "leveldb/table.h"
#include "leveldb/write_batch.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace leveldb {

static const int kValueSize = 1000;
static const int kMaxNumValues = 2000;
static const size_t kNumIterations = 3;

class FaultInjectionTestEnv;

namespace {

//假定文件名，而不是目录名，如“/foo/bar/”
static std::string GetDirName(const std::string filename) {
  size_t found = filename.find_last_of("/\\");
  if (found == std::string::npos) {
    return "";
  } else {
    return filename.substr(0, found);
  }
}

Status SyncDir(const std::string& dir) {
//因为这是一个测试，所以不需要*实际*同步这个目录。
  return Status::OK();
}

//适用于此测试的基本文件截断函数。
Status Truncate(const std::string& filename, uint64_t length) {
  leveldb::Env* env = leveldb::Env::Default();

  SequentialFile* orig_file;
  Status s = env->NewSequentialFile(filename, &orig_file);
  if (!s.ok())
    return s;

  char* scratch = new char[length];
  leveldb::Slice result;
  s = orig_file->Read(length, &result, scratch);
  delete orig_file;
  if (s.ok()) {
    std::string tmp_name = GetDirName(filename) + "/truncate.tmp";
    WritableFile* tmp_file;
    s = env->NewWritableFile(tmp_name, &tmp_file);
    if (s.ok()) {
      s = tmp_file->Append(result);
      delete tmp_file;
      if (s.ok()) {
        s = env->RenameFile(tmp_name, filename);
      } else {
        env->DeleteFile(tmp_name);
      }
    }
  }

  delete[] scratch;

  return s;
}

struct FileState {
  std::string filename_;
  ssize_t pos_;
  ssize_t pos_at_last_sync_;
  ssize_t pos_at_last_flush_;

  FileState(const std::string& filename)
      : filename_(filename),
        pos_(-1),
        pos_at_last_sync_(-1),
        pos_at_last_flush_(-1) { }

  FileState() : pos_(-1), pos_at_last_sync_(-1), pos_at_last_flush_(-1) {}

  bool IsFullySynced() const { return pos_ <= 0 || pos_ == pos_at_last_sync_; }

  Status DropUnsyncedData() const;
};

}  //匿名命名空间

//可写文件周围的包装，每当此文件
//写入或同步。
class TestWritableFile : public WritableFile {
 public:
  TestWritableFile(const FileState& state,
                   WritableFile* f,
                   FaultInjectionTestEnv* env);
  virtual ~TestWritableFile();
  virtual Status Append(const Slice& data);
  virtual Status Close();
  virtual Status Flush();
  virtual Status Sync();

 private:
  FileState state_;
  WritableFile* target_;
  bool writable_file_opened_;
  FaultInjectionTestEnv* env_;

  Status SyncParent();
};

class FaultInjectionTestEnv : public EnvWrapper {
 public:
  FaultInjectionTestEnv() : EnvWrapper(Env::Default()), filesystem_active_(true) {}
  virtual ~FaultInjectionTestEnv() { }
  virtual Status NewWritableFile(const std::string& fname,
                                 WritableFile** result);
  virtual Status NewAppendableFile(const std::string& fname,
                                   WritableFile** result);
  virtual Status DeleteFile(const std::string& f);
  virtual Status RenameFile(const std::string& s, const std::string& t);

  void WritableFileClosed(const FileState& state);
  Status DropUnsyncedFileData();
  Status DeleteFilesCreatedAfterLastDirSync();
  void DirWasSynced();
  bool IsFileCreatedSinceLastDirSync(const std::string& filename);
  void ResetState();
  void UntrackFile(const std::string& f);
//将文件系统设置为非活动的测试相当于模拟
//系统复位。设置为inactive将冻结保存的文件系统状态，因此
//它将停止被记录。然后可以将其重置回状态
//重置时间。
  bool IsFilesystemActive() const { return filesystem_active_; }
  void SetFilesystemActive(bool active) { filesystem_active_ = active; }

 private:
  port::Mutex mutex_;
  std::map<std::string, FileState> db_file_state_;
  std::set<std::string> new_files_since_last_dir_sync_;
bool filesystem_active_;  //记录刷新、同步、写入
};

TestWritableFile::TestWritableFile(const FileState& state,
                                   WritableFile* f,
                                   FaultInjectionTestEnv* env)
    : state_(state),
      target_(f),
      writable_file_opened_(true),
      env_(env) {
  assert(f != NULL);
}

TestWritableFile::~TestWritableFile() {
  if (writable_file_opened_) {
    Close();
  }
  delete target_;
}

Status TestWritableFile::Append(const Slice& data) {
  Status s = target_->Append(data);
  if (s.ok() && env_->IsFilesystemActive()) {
    state_.pos_ += data.size();
  }
  return s;
}

Status TestWritableFile::Close() {
  writable_file_opened_ = false;
  Status s = target_->Close();
  if (s.ok()) {
    env_->WritableFileClosed(state_);
  }
  return s;
}

Status TestWritableFile::Flush() {
  Status s = target_->Flush();
  if (s.ok() && env_->IsFilesystemActive()) {
    state_.pos_at_last_flush_ = state_.pos_;
  }
  return s;
}

Status TestWritableFile::SyncParent() {
  Status s = SyncDir(GetDirName(state_.filename_));
  if (s.ok()) {
    env_->DirWasSynced();
  }
  return s;
}

Status TestWritableFile::Sync() {
  if (!env_->IsFilesystemActive()) {
    return Status::OK();
  }
//确保清单引用的新文件位于文件系统中。
  Status s = target_->Sync();
  if (s.ok()) {
    state_.pos_at_last_sync_ = state_.pos_;
  }
  if (env_->IsFileCreatedSinceLastDirSync(state_.filename_)) {
    Status ps = SyncParent();
    if (s.ok() && !ps.ok()) {
      s = ps;
    }
  }
  return s;
}

Status FaultInjectionTestEnv::NewWritableFile(const std::string& fname,
                                              WritableFile** result) {
  WritableFile* actual_writable_file;
  Status s = target()->NewWritableFile(fname, &actual_writable_file);
  if (s.ok()) {
    FileState state(fname);
    state.pos_ = 0;
    *result = new TestWritableFile(state, actual_writable_file, this);
//newwritablefile不附加到文件，因此如果相同的文件是
//再次打开后，它将被截断-所以忘记我们保存的
//状态。
    UntrackFile(fname);
    MutexLock l(&mutex_);
    new_files_since_last_dir_sync_.insert(fname);
  }
  return s;
}

Status FaultInjectionTestEnv::NewAppendableFile(const std::string& fname,
                                                WritableFile** result) {
  WritableFile* actual_writable_file;
  Status s = target()->NewAppendableFile(fname, &actual_writable_file);
  if (s.ok()) {
    FileState state(fname);
    state.pos_ = 0;
    {
      MutexLock l(&mutex_);
      if (db_file_state_.count(fname) == 0) {
        new_files_since_last_dir_sync_.insert(fname);
      } else {
        state = db_file_state_[fname];
      }
    }
    *result = new TestWritableFile(state, actual_writable_file, this);
  }
  return s;
}

Status FaultInjectionTestEnv::DropUnsyncedFileData() {
  Status s;
  MutexLock l(&mutex_);
  for (std::map<std::string, FileState>::const_iterator it =
           db_file_state_.begin();
       s.ok() && it != db_file_state_.end(); ++it) {
    const FileState& state = it->second;
    if (!state.IsFullySynced()) {
      s = state.DropUnsyncedData();
    }
  }
  return s;
}

void FaultInjectionTestEnv::DirWasSynced() {
  MutexLock l(&mutex_);
  new_files_since_last_dir_sync_.clear();
}

bool FaultInjectionTestEnv::IsFileCreatedSinceLastDirSync(
    const std::string& filename) {
  MutexLock l(&mutex_);
  return new_files_since_last_dir_sync_.find(filename) !=
         new_files_since_last_dir_sync_.end();
}

void FaultInjectionTestEnv::UntrackFile(const std::string& f) {
  MutexLock l(&mutex_);
  db_file_state_.erase(f);
  new_files_since_last_dir_sync_.erase(f);
}

Status FaultInjectionTestEnv::DeleteFile(const std::string& f) {
  Status s = EnvWrapper::DeleteFile(f);
  ASSERT_OK(s);
  if (s.ok()) {
    UntrackFile(f);
  }
  return s;
}

Status FaultInjectionTestEnv::RenameFile(const std::string& s,
                                         const std::string& t) {
  Status ret = EnvWrapper::RenameFile(s, t);

  if (ret.ok()) {
    MutexLock l(&mutex_);
    if (db_file_state_.find(s) != db_file_state_.end()) {
      db_file_state_[t] = db_file_state_[s];
      db_file_state_.erase(s);
    }

    if (new_files_since_last_dir_sync_.erase(s) != 0) {
      assert(new_files_since_last_dir_sync_.find(t) ==
             new_files_since_last_dir_sync_.end());
      new_files_since_last_dir_sync_.insert(t);
    }
  }

  return ret;
}

void FaultInjectionTestEnv::ResetState() {
//因为我们没有破坏数据库，现有的文件
//应保持记录的同步/刷新状态。因此
//自上次目录同步后，我们不会重置db_文件\状态\和新的\文件。
  MutexLock l(&mutex_);
  SetFilesystemActive(true);
}

Status FaultInjectionTestEnv::DeleteFilesCreatedAfterLastDirSync() {
//因为deletefile访问这个容器复制以避免死锁
  mutex_.Lock();
  std::set<std::string> new_files(new_files_since_last_dir_sync_.begin(),
                                  new_files_since_last_dir_sync_.end());
  mutex_.Unlock();
  Status s;
  std::set<std::string>::const_iterator it;
  for (it = new_files.begin(); s.ok() && it != new_files.end(); ++it) {
    s = DeleteFile(*it);
  }
  return s;
}

void FaultInjectionTestEnv::WritableFileClosed(const FileState& state) {
  MutexLock l(&mutex_);
  db_file_state_[state.filename_] = state;
}

Status FileState::DropUnsyncedData() const {
  ssize_t sync_pos = pos_at_last_sync_ == -1 ? 0 : pos_at_last_sync_;
  return Truncate(filename_, sync_pos);
}

class FaultInjectionTest {
 public:
  enum ExpectedVerifResult { VAL_EXPECT_NO_ERROR, VAL_EXPECT_ERROR };
  enum ResetMethod { RESET_DROP_UNSYNCED_DATA, RESET_DELETE_UNSYNCED_FILES };

  FaultInjectionTestEnv* env_;
  std::string dbname_;
  Cache* tiny_cache_;
  Options options_;
  DB* db_;

  FaultInjectionTest()
      : env_(new FaultInjectionTestEnv),
        tiny_cache_(NewLRUCache(100)),
        db_(NULL) {
    dbname_ = test::TmpDir() + "/fault_test";
DestroyDB(dbname_, Options());  //销毁以前运行的任何数据库
    options_.reuse_logs = true;
    options_.env = env_;
    options_.paranoid_checks = true;
    options_.block_cache = tiny_cache_;
    options_.create_if_missing = true;
  }

  ~FaultInjectionTest() {
    CloseDB();
    DestroyDB(dbname_, Options());
    delete tiny_cache_;
    delete env_;
  }

  void ReuseLogs(bool reuse) {
    options_.reuse_logs = reuse;
  }

  void Build(int start_idx, int num_vals) {
    std::string key_space, value_space;
    WriteBatch batch;
    for (int i = start_idx; i < start_idx + num_vals; i++) {
      Slice key = Key(i, &key_space);
      batch.Clear();
      batch.Put(key, Value(i, &value_space));
      WriteOptions options;
      ASSERT_OK(db_->Write(options, &batch));
    }
  }

  Status ReadValue(int i, std::string* val) const {
    std::string key_space, value_space;
    Slice key = Key(i, &key_space);
    Value(i, &value_space);
    ReadOptions options;
    return db_->Get(options, key, val);
  }

  Status Verify(int start_idx, int num_vals,
                ExpectedVerifResult expected) const {
    std::string val;
    std::string value_space;
    Status s;
    for (int i = start_idx; i < start_idx + num_vals && s.ok(); i++) {
      Value(i, &value_space);
      s = ReadValue(i, &val);
      if (expected == VAL_EXPECT_NO_ERROR) {
        if (s.ok()) {
          ASSERT_EQ(value_space, val);
        }
      } else if (s.ok()) {
        fprintf(stderr, "Expected an error at %d, but was OK\n", i);
        s = Status::IOError(dbname_, "Expected value error:");
      } else {
s = Status::OK();  //预期错误
      }
    }
    return s;
  }

//返回第i个键
  Slice Key(int i, std::string* storage) const {
    char buf[100];
    snprintf(buf, sizeof(buf), "%016d", i);
    storage->assign(buf, strlen(buf));
    return Slice(*storage);
  }

//返回要与指定键关联的值
  Slice Value(int k, std::string* storage) const {
    Random r(k);
    return test::RandomString(&r, kValueSize, storage);
  }

  Status OpenDB() {
    delete db_;
    db_ = NULL;
    env_->ResetState();
    return DB::Open(options_, dbname_, &db_);
  }

  void CloseDB() {
    delete db_;
    db_ = NULL;
  }

  void DeleteAllData() {
    Iterator* iter = db_->NewIterator(ReadOptions());
    WriteOptions options;
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
      ASSERT_OK(db_->Delete(WriteOptions(), iter->key()));
    }

    delete iter;
  }

  void ResetDBState(ResetMethod reset_method) {
    switch (reset_method) {
      case RESET_DROP_UNSYNCED_DATA:
        ASSERT_OK(env_->DropUnsyncedFileData());
        break;
      case RESET_DELETE_UNSYNCED_FILES:
        ASSERT_OK(env_->DeleteFilesCreatedAfterLastDirSync());
        break;
      default:
        assert(false);
    }
  }

  void PartialCompactTestPreFault(int num_pre_sync, int num_post_sync) {
    DeleteAllData();
    Build(0, num_pre_sync);
    db_->CompactRange(NULL, NULL);
    Build(num_pre_sync, num_post_sync);
  }

  void PartialCompactTestReopenWithFault(ResetMethod reset_method,
                                         int num_pre_sync,
                                         int num_post_sync) {
    env_->SetFilesystemActive(false);
    CloseDB();
    ResetDBState(reset_method);
    ASSERT_OK(OpenDB());
    ASSERT_OK(Verify(0, num_pre_sync, FaultInjectionTest::VAL_EXPECT_NO_ERROR));
    ASSERT_OK(Verify(num_pre_sync, num_post_sync, FaultInjectionTest::VAL_EXPECT_ERROR));
  }

  void NoWriteTestPreFault() {
  }

  void NoWriteTestReopenWithFault(ResetMethod reset_method) {
    CloseDB();
    ResetDBState(reset_method);
    ASSERT_OK(OpenDB());
  }

  void DoTest() {
    Random rnd(0);
    ASSERT_OK(OpenDB());
    for (size_t idx = 0; idx < kNumIterations; idx++) {
      int num_pre_sync = rnd.Uniform(kMaxNumValues);
      int num_post_sync = rnd.Uniform(kMaxNumValues);

      PartialCompactTestPreFault(num_pre_sync, num_post_sync);
      PartialCompactTestReopenWithFault(RESET_DROP_UNSYNCED_DATA,
                                        num_pre_sync,
                                        num_post_sync);

      NoWriteTestPreFault();
      NoWriteTestReopenWithFault(RESET_DROP_UNSYNCED_DATA);

      PartialCompactTestPreFault(num_pre_sync, num_post_sync);
//没有创建新文件，因此我们希望所有值，因为没有文件
//下降。
      PartialCompactTestReopenWithFault(RESET_DELETE_UNSYNCED_FILES,
                                        num_pre_sync + num_post_sync,
                                        0);

      NoWriteTestPreFault();
      NoWriteTestReopenWithFault(RESET_DELETE_UNSYNCED_FILES);
    }
  }
};

TEST(FaultInjectionTest, FaultTestNoLogReuse) {
  ReuseLogs(false);
  DoTest();
}

TEST(FaultInjectionTest, FaultTestWithLogReuse) {
  ReuseLogs(true);
  DoTest();
}

}  //命名空间级别数据库

int main(int argc, char** argv) {
  return leveldb::test::RunAllTests();
}
