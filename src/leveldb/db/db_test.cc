
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

#include "leveldb/db.h"
#include "leveldb/filter_policy.h"
#include "db/db_impl.h"
#include "db/filename.h"
#include "db/version_set.h"
#include "db/write_batch_internal.h"
#include "leveldb/cache.h"
#include "leveldb/env.h"
#include "leveldb/table.h"
#include "util/hash.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace leveldb {

static std::string RandomString(Random* rnd, int len) {
  std::string r;
  test::RandomString(rnd, len, &r);
  return r;
}

namespace {
class AtomicCounter {
 private:
  port::Mutex mu_;
  int count_;
 public:
  AtomicCounter() : count_(0) { }
  void Increment() {
    IncrementBy(1);
  }
  void IncrementBy(int count) {
    MutexLock l(&mu_);
    count_ += count;
  }
  int Read() {
    MutexLock l(&mu_);
    return count_;
  }
  void Reset() {
    MutexLock l(&mu_);
    count_ = 0;
  }
};

void DelayMilliseconds(int millis) {
  Env::Default()->SleepForMicroseconds(millis * 1000);
}
}

//用于延迟后台操作的特殊env
class SpecialEnv : public EnvWrapper {
 public:
//当此指针非空时，sstable/log sync（）调用被阻止。
  port::AtomicPointer delay_data_sync_;

//sstable/log sync（）调用返回错误。
  port::AtomicPointer data_sync_error_;

//此指针非空时模拟无空间错误。
  port::AtomicPointer no_space_;

//当此指针非空时模拟不可写文件系统
  port::AtomicPointer non_writable_;

//当此指针非空时，强制同步清单文件失败
  port::AtomicPointer manifest_sync_error_;

//当此指针非空时，强制写入清单文件失败
  port::AtomicPointer manifest_write_error_;

  bool count_random_reads_;
  AtomicCounter random_read_counter_;

  explicit SpecialEnv(Env* base) : EnvWrapper(base) {
    delay_data_sync_.Release_Store(NULL);
    data_sync_error_.Release_Store(NULL);
    no_space_.Release_Store(NULL);
    non_writable_.Release_Store(NULL);
    count_random_reads_ = false;
    manifest_sync_error_.Release_Store(NULL);
    manifest_write_error_.Release_Store(NULL);
  }

  Status NewWritableFile(const std::string& f, WritableFile** r) {
    class DataFile : public WritableFile {
     private:
      SpecialEnv* env_;
      WritableFile* base_;

     public:
      DataFile(SpecialEnv* env, WritableFile* base)
          : env_(env),
            base_(base) {
      }
      ~DataFile() { delete base_; }
      Status Append(const Slice& data) {
        if (env_->no_space_.Acquire_Load() != NULL) {
//把字掉在地上
          return Status::OK();
        } else {
          return base_->Append(data);
        }
      }
      Status Close() { return base_->Close(); }
      Status Flush() { return base_->Flush(); }
      Status Sync() {
        if (env_->data_sync_error_.Acquire_Load() != NULL) {
          return Status::IOError("simulated data sync error");
        }
        while (env_->delay_data_sync_.Acquire_Load() != NULL) {
          DelayMilliseconds(100);
        }
        return base_->Sync();
      }
    };
    class ManifestFile : public WritableFile {
     private:
      SpecialEnv* env_;
      WritableFile* base_;
     public:
      ManifestFile(SpecialEnv* env, WritableFile* b) : env_(env), base_(b) { }
      ~ManifestFile() { delete base_; }
      Status Append(const Slice& data) {
        if (env_->manifest_write_error_.Acquire_Load() != NULL) {
          return Status::IOError("simulated writer error");
        } else {
          return base_->Append(data);
        }
      }
      Status Close() { return base_->Close(); }
      Status Flush() { return base_->Flush(); }
      Status Sync() {
        if (env_->manifest_sync_error_.Acquire_Load() != NULL) {
          return Status::IOError("simulated sync error");
        } else {
          return base_->Sync();
        }
      }
    };

    if (non_writable_.Acquire_Load() != NULL) {
      return Status::IOError("simulated write error");
    }

    Status s = target()->NewWritableFile(f, r);
    if (s.ok()) {
      if (strstr(f.c_str(), ".ldb") != NULL ||
          strstr(f.c_str(), ".log") != NULL) {
        *r = new DataFile(this, *r);
      } else if (strstr(f.c_str(), "MANIFEST") != NULL) {
        *r = new ManifestFile(this, *r);
      }
    }
    return s;
  }

  Status NewRandomAccessFile(const std::string& f, RandomAccessFile** r) {
    class CountingFile : public RandomAccessFile {
     private:
      RandomAccessFile* target_;
      AtomicCounter* counter_;
     public:
      CountingFile(RandomAccessFile* target, AtomicCounter* counter)
          : target_(target), counter_(counter) {
      }
      virtual ~CountingFile() { delete target_; }
      virtual Status Read(uint64_t offset, size_t n, Slice* result,
                          char* scratch) const {
        counter_->Increment();
        return target_->Read(offset, n, result, scratch);
      }
    };

    Status s = target()->NewRandomAccessFile(f, r);
    if (s.ok() && count_random_reads_) {
      *r = new CountingFile(*r, &random_read_counter_);
    }
    return s;
  }
};

class DBTest {
 private:
  const FilterPolicy* filter_policy_;

//要尝试的选项配置序列
  enum OptionConfig {
    kDefault,
    kReuse,
    kFilter,
    kUncompressed,
    kEnd
  };
  int option_config_;

 public:
  std::string dbname_;
  SpecialEnv* env_;
  DB* db_;

  Options last_options_;

  DBTest() : option_config_(kDefault),
             env_(new SpecialEnv(Env::Default())) {
    filter_policy_ = NewBloomFilterPolicy(10);
    dbname_ = test::TmpDir() + "/db_test";
    DestroyDB(dbname_, Options());
    db_ = NULL;
    Reopen();
  }

  ~DBTest() {
    delete db_;
    DestroyDB(dbname_, Options());
    delete env_;
    delete filter_policy_;
  }

//切换到具有下一个选项配置的新数据库
//测试。如果没有更多要测试的配置，则返回false。
  bool ChangeOptions() {
    option_config_++;
    if (option_config_ >= kEnd) {
      return false;
    } else {
      DestroyAndReopen();
      return true;
    }
  }

//返回当前选项配置。
  Options CurrentOptions() {
    Options options;
    options.reuse_logs = false;
    switch (option_config_) {
      case kReuse:
        options.reuse_logs = true;
        break;
      case kFilter:
        options.filter_policy = filter_policy_;
        break;
      case kUncompressed:
        options.compression = kNoCompression;
        break;
      default:
        break;
    }
    return options;
  }

  DBImpl* dbfull() {
    return reinterpret_cast<DBImpl*>(db_);
  }

  void Reopen(Options* options = NULL) {
    ASSERT_OK(TryReopen(options));
  }

  void Close() {
    delete db_;
    db_ = NULL;
  }

  void DestroyAndReopen(Options* options = NULL) {
    delete db_;
    db_ = NULL;
    DestroyDB(dbname_, Options());
    ASSERT_OK(TryReopen(options));
  }

  Status TryReopen(Options* options) {
    delete db_;
    db_ = NULL;
    Options opts;
    if (options != NULL) {
      opts = *options;
    } else {
      opts = CurrentOptions();
      opts.create_if_missing = true;
    }
    last_options_ = opts;

    return DB::Open(opts, dbname_, &db_);
  }

  Status Put(const std::string& k, const std::string& v) {
    return db_->Put(WriteOptions(), k, v);
  }

  Status Delete(const std::string& k) {
    return db_->Delete(WriteOptions(), k);
  }

  std::string Get(const std::string& k, const Snapshot* snapshot = NULL) {
    ReadOptions options;
    options.snapshot = snapshot;
    std::string result;
    Status s = db_->Get(options, k, &result);
    if (s.IsNotFound()) {
      result = "NOT_FOUND";
    } else if (!s.ok()) {
      result = s.ToString();
    }
    return result;
  }

//返回一个字符串，该字符串按顺序包含所有键、值对，
//格式类似“（k1->v1）（k2->v2）”。
  std::string Contents() {
    std::vector<std::string> forward;
    std::string result;
    Iterator* iter = db_->NewIterator(ReadOptions());
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
      std::string s = IterStatus(iter);
      result.push_back('(');
      result.append(s);
      result.push_back(')');
      forward.push_back(s);
    }

//检查反向迭代结果与正向结果相反
    size_t matched = 0;
    for (iter->SeekToLast(); iter->Valid(); iter->Prev()) {
      ASSERT_LT(matched, forward.size());
      ASSERT_EQ(IterStatus(iter), forward[forward.size() - matched - 1]);
      matched++;
    }
    ASSERT_EQ(matched, forward.size());

    delete iter;
    return result;
  }

  std::string AllEntriesFor(const Slice& user_key) {
    Iterator* iter = dbfull()->TEST_NewInternalIterator();
    InternalKey target(user_key, kMaxSequenceNumber, kTypeValue);
    iter->Seek(target.Encode());
    std::string result;
    if (!iter->status().ok()) {
      result = iter->status().ToString();
    } else {
      result = "[ ";
      bool first = true;
      while (iter->Valid()) {
        ParsedInternalKey ikey;
        if (!ParseInternalKey(iter->key(), &ikey)) {
          result += "CORRUPTED";
        } else {
          if (last_options_.comparator->Compare(ikey.user_key, user_key) != 0) {
            break;
          }
          if (!first) {
            result += ", ";
          }
          first = false;
          switch (ikey.type) {
            case kTypeValue:
              result += iter->value().ToString();
              break;
            case kTypeDeletion:
              result += "DEL";
              break;
          }
        }
        iter->Next();
      }
      if (!first) {
        result += " ";
      }
      result += "]";
    }
    delete iter;
    return result;
  }

  int NumTableFilesAtLevel(int level) {
    std::string property;
    ASSERT_TRUE(
        db_->GetProperty("leveldb.num-files-at-level" + NumberToString(level),
                         &property));
    return atoi(property.c_str());
  }

  int TotalTableFiles() {
    int result = 0;
    for (int level = 0; level < config::kNumLevels; level++) {
      result += NumTableFilesAtLevel(level);
    }
    return result;
  }

//返回每个级别的文件分布
  std::string FilesPerLevel() {
    std::string result;
    int last_non_zero_offset = 0;
    for (int level = 0; level < config::kNumLevels; level++) {
      int f = NumTableFilesAtLevel(level);
      char buf[100];
      snprintf(buf, sizeof(buf), "%s%d", (level ? "," : ""), f);
      result += buf;
      if (f > 0) {
        last_non_zero_offset = result.size();
      }
    }
    result.resize(last_non_zero_offset);
    return result;
  }

  int CountFiles() {
    std::vector<std::string> files;
    env_->GetChildren(dbname_, &files);
    return static_cast<int>(files.size());
  }

  uint64_t Size(const Slice& start, const Slice& limit) {
    Range r(start, limit);
    uint64_t size;
    db_->GetApproximateSizes(&r, 1, &size);
    return size;
  }

  void Compact(const Slice& start, const Slice& limit) {
    db_->CompactRange(&start, &limit);
  }

//做n个memtable压缩，每个压缩产生一个sstable
//覆盖范围[小，大]。
  void MakeTables(int n, const std::string& small, const std::string& large) {
    for (int i = 0; i < n; i++) {
      Put(small, "begin");
      Put(large, "end");
      dbfull()->TEST_CompactMemTable();
    }
  }

//通过添加
//覆盖所有级别的指定范围的表。
  void FillLevels(const std::string& smallest, const std::string& largest) {
    MakeTables(config::kNumLevels, smallest, largest);
  }

  void DumpFileCounts(const char* label) {
    fprintf(stderr, "---\n%s:\n", label);
    fprintf(stderr, "maxoverlap: %lld\n",
            static_cast<long long>(
                dbfull()->TEST_MaxNextLevelOverlappingBytes()));
    for (int level = 0; level < config::kNumLevels; level++) {
      int num = NumTableFilesAtLevel(level);
      if (num > 0) {
        fprintf(stderr, "  level %3d : %d files\n", level, num);
      }
    }
  }

  std::string DumpSSTableList() {
    std::string property;
    db_->GetProperty("leveldb.sstables", &property);
    return property;
  }

  std::string IterStatus(Iterator* iter) {
    std::string result;
    if (iter->Valid()) {
      result = iter->key().ToString() + "->" + iter->value().ToString();
    } else {
      result = "(invalid)";
    }
    return result;
  }

  bool DeleteAnSSTFile() {
    std::vector<std::string> filenames;
    ASSERT_OK(env_->GetChildren(dbname_, &filenames));
    uint64_t number;
    FileType type;
    for (size_t i = 0; i < filenames.size(); i++) {
      if (ParseFileName(filenames[i], &number, &type) && type == kTableFile) {
        ASSERT_OK(env_->DeleteFile(TableFileName(dbname_, number)));
        return true;
      }
    }
    return false;
  }

//返回重命名的文件数。
  int RenameLDBToSST() {
    std::vector<std::string> filenames;
    ASSERT_OK(env_->GetChildren(dbname_, &filenames));
    uint64_t number;
    FileType type;
    int files_renamed = 0;
    for (size_t i = 0; i < filenames.size(); i++) {
      if (ParseFileName(filenames[i], &number, &type) && type == kTableFile) {
        const std::string from = TableFileName(dbname_, number);
        const std::string to = SSTTableFileName(dbname_, number);
        ASSERT_OK(env_->RenameFile(from, to));
        files_renamed++;
      }
    }
    return files_renamed;
  }
};

TEST(DBTest, Empty) {
  do {
    ASSERT_TRUE(db_ != NULL);
    ASSERT_EQ("NOT_FOUND", Get("foo"));
  } while (ChangeOptions());
}

TEST(DBTest, ReadWrite) {
  do {
    ASSERT_OK(Put("foo", "v1"));
    ASSERT_EQ("v1", Get("foo"));
    ASSERT_OK(Put("bar", "v2"));
    ASSERT_OK(Put("foo", "v3"));
    ASSERT_EQ("v3", Get("foo"));
    ASSERT_EQ("v2", Get("bar"));
  } while (ChangeOptions());
}

TEST(DBTest, PutDeleteGet) {
  do {
    ASSERT_OK(db_->Put(WriteOptions(), "foo", "v1"));
    ASSERT_EQ("v1", Get("foo"));
    ASSERT_OK(db_->Put(WriteOptions(), "foo", "v2"));
    ASSERT_EQ("v2", Get("foo"));
    ASSERT_OK(db_->Delete(WriteOptions(), "foo"));
    ASSERT_EQ("NOT_FOUND", Get("foo"));
  } while (ChangeOptions());
}

TEST(DBTest, GetFromImmutableLayer) {
  do {
    Options options = CurrentOptions();
    options.env = env_;
options.write_buffer_size = 100000;  //小写缓冲区
    Reopen(&options);

    ASSERT_OK(Put("foo", "v1"));
    ASSERT_EQ("v1", Get("foo"));

env_->delay_data_sync_.Release_Store(env_);      //阻止同步调用
Put("k1", std::string(100000, 'x'));             //填表
Put("k2", std::string(100000, 'y'));             //触发压缩
    ASSERT_EQ("v1", Get("foo"));
env_->delay_data_sync_.Release_Store(NULL);      //释放同步调用
  } while (ChangeOptions());
}

TEST(DBTest, GetFromVersions) {
  do {
    ASSERT_OK(Put("foo", "v1"));
    dbfull()->TEST_CompactMemTable();
    ASSERT_EQ("v1", Get("foo"));
  } while (ChangeOptions());
}

TEST(DBTest, GetMemUsage) {
  do {
    ASSERT_OK(Put("foo", "v1"));
    std::string val;
    ASSERT_TRUE(db_->GetProperty("leveldb.approximate-memory-usage", &val));
    int mem_usage = atoi(val.c_str());
    ASSERT_GT(mem_usage, 0);
    ASSERT_LT(mem_usage, 5*1024*1024);
  } while (ChangeOptions());
}

TEST(DBTest, GetSnapshot) {
  do {
//试着用一把短钥匙和一把长钥匙
    for (int i = 0; i < 2; i++) {
      std::string key = (i == 0) ? std::string("foo") : std::string(200, 'x');
      ASSERT_OK(Put(key, "v1"));
      const Snapshot* s1 = db_->GetSnapshot();
      ASSERT_OK(Put(key, "v2"));
      ASSERT_EQ("v2", Get(key));
      ASSERT_EQ("v1", Get(key, s1));
      dbfull()->TEST_CompactMemTable();
      ASSERT_EQ("v2", Get(key));
      ASSERT_EQ("v1", Get(key, s1));
      db_->ReleaseSnapshot(s1);
    }
  } while (ChangeOptions());
}

TEST(DBTest, GetLevel0Ordering) {
  do {
//检查我们处理0级文件的顺序是否正确。代码
//下面生成两个0级文件
//在0级文件列表中的后一个文件之前
//一个有一个更小的“最小”键。
    ASSERT_OK(Put("bar", "b"));
    ASSERT_OK(Put("foo", "v1"));
    dbfull()->TEST_CompactMemTable();
    ASSERT_OK(Put("foo", "v2"));
    dbfull()->TEST_CompactMemTable();
    ASSERT_EQ("v2", Get("foo"));
  } while (ChangeOptions());
}

TEST(DBTest, GetOrderedByLevels) {
  do {
    ASSERT_OK(Put("foo", "v1"));
    Compact("a", "z");
    ASSERT_EQ("v1", Get("foo"));
    ASSERT_OK(Put("foo", "v2"));
    ASSERT_EQ("v2", Get("foo"));
    dbfull()->TEST_CompactMemTable();
    ASSERT_EQ("v2", Get("foo"));
  } while (ChangeOptions());
}

TEST(DBTest, GetPicksCorrectFile) {
  do {
//将多个文件安排为非0级。
    ASSERT_OK(Put("a", "va"));
    Compact("a", "b");
    ASSERT_OK(Put("x", "vx"));
    Compact("x", "y");
    ASSERT_OK(Put("f", "vf"));
    Compact("f", "g");
    ASSERT_EQ("va", Get("a"));
    ASSERT_EQ("vf", Get("f"));
    ASSERT_EQ("vx", Get("x"));
  } while (ChangeOptions());
}

TEST(DBTest, GetEncountersEmptyLevel) {
  do {
//安排发生以下情况：
//*S 0级表A
//*1级中无任何内容
//*二级表B
//然后执行足够的get（）调用来安排自动压缩
//错误会导致压缩被标记为
//发生在级别1（而不是正确的级别0）。

//第一步：将SSTables放在0和2级
    int compaction_count = 0;
    while (NumTableFilesAtLevel(0) == 0 ||
           NumTableFilesAtLevel(2) == 0) {
      ASSERT_LE(compaction_count, 100) << "could not fill levels 0 and 2";
      compaction_count++;
      Put("a", "begin");
      Put("z", "end");
      dbfull()->TEST_CompactMemTable();
    }

//步骤2：必要时清除1级。
    dbfull()->TEST_CompactRange(1, NULL, NULL);
    ASSERT_EQ(NumTableFilesAtLevel(0), 1);
    ASSERT_EQ(NumTableFilesAtLevel(1), 0);
    ASSERT_EQ(NumTableFilesAtLevel(2), 1);

//第三步：读很多遍
    for (int i = 0; i < 1000; i++) {
      ASSERT_EQ("NOT_FOUND", Get("missing"));
    }

//步骤4：等待压实完成
    DelayMilliseconds(1000);

    ASSERT_EQ(NumTableFilesAtLevel(0), 0);
  } while (ChangeOptions());
}

TEST(DBTest, IterEmpty) {
  Iterator* iter = db_->NewIterator(ReadOptions());

  iter->SeekToFirst();
  ASSERT_EQ(IterStatus(iter), "(invalid)");

  iter->SeekToLast();
  ASSERT_EQ(IterStatus(iter), "(invalid)");

  iter->Seek("foo");
  ASSERT_EQ(IterStatus(iter), "(invalid)");

  delete iter;
}

TEST(DBTest, IterSingle) {
  ASSERT_OK(Put("a", "va"));
  Iterator* iter = db_->NewIterator(ReadOptions());

  iter->SeekToFirst();
  ASSERT_EQ(IterStatus(iter), "a->va");
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "(invalid)");
  iter->SeekToFirst();
  ASSERT_EQ(IterStatus(iter), "a->va");
  iter->Prev();
  ASSERT_EQ(IterStatus(iter), "(invalid)");

  iter->SeekToLast();
  ASSERT_EQ(IterStatus(iter), "a->va");
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "(invalid)");
  iter->SeekToLast();
  ASSERT_EQ(IterStatus(iter), "a->va");
  iter->Prev();
  ASSERT_EQ(IterStatus(iter), "(invalid)");

  iter->Seek("");
  ASSERT_EQ(IterStatus(iter), "a->va");
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "(invalid)");

  iter->Seek("a");
  ASSERT_EQ(IterStatus(iter), "a->va");
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "(invalid)");

  iter->Seek("b");
  ASSERT_EQ(IterStatus(iter), "(invalid)");

  delete iter;
}

TEST(DBTest, IterMulti) {
  ASSERT_OK(Put("a", "va"));
  ASSERT_OK(Put("b", "vb"));
  ASSERT_OK(Put("c", "vc"));
  Iterator* iter = db_->NewIterator(ReadOptions());

  iter->SeekToFirst();
  ASSERT_EQ(IterStatus(iter), "a->va");
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "b->vb");
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "c->vc");
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "(invalid)");
  iter->SeekToFirst();
  ASSERT_EQ(IterStatus(iter), "a->va");
  iter->Prev();
  ASSERT_EQ(IterStatus(iter), "(invalid)");

  iter->SeekToLast();
  ASSERT_EQ(IterStatus(iter), "c->vc");
  iter->Prev();
  ASSERT_EQ(IterStatus(iter), "b->vb");
  iter->Prev();
  ASSERT_EQ(IterStatus(iter), "a->va");
  iter->Prev();
  ASSERT_EQ(IterStatus(iter), "(invalid)");
  iter->SeekToLast();
  ASSERT_EQ(IterStatus(iter), "c->vc");
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "(invalid)");

  iter->Seek("");
  ASSERT_EQ(IterStatus(iter), "a->va");
  iter->Seek("a");
  ASSERT_EQ(IterStatus(iter), "a->va");
  iter->Seek("ax");
  ASSERT_EQ(IterStatus(iter), "b->vb");
  iter->Seek("b");
  ASSERT_EQ(IterStatus(iter), "b->vb");
  iter->Seek("z");
  ASSERT_EQ(IterStatus(iter), "(invalid)");

//从后退切换到前进
  iter->SeekToLast();
  iter->Prev();
  iter->Prev();
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "b->vb");

//从前进档切换到后退档
  iter->SeekToFirst();
  iter->Next();
  iter->Next();
  iter->Prev();
  ASSERT_EQ(IterStatus(iter), "b->vb");

//确保ITER保持快照状态
  ASSERT_OK(Put("a",  "va2"));
  ASSERT_OK(Put("a2", "va3"));
  ASSERT_OK(Put("b",  "vb2"));
  ASSERT_OK(Put("c",  "vc2"));
  ASSERT_OK(Delete("b"));
  iter->SeekToFirst();
  ASSERT_EQ(IterStatus(iter), "a->va");
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "b->vb");
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "c->vc");
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "(invalid)");
  iter->SeekToLast();
  ASSERT_EQ(IterStatus(iter), "c->vc");
  iter->Prev();
  ASSERT_EQ(IterStatus(iter), "b->vb");
  iter->Prev();
  ASSERT_EQ(IterStatus(iter), "a->va");
  iter->Prev();
  ASSERT_EQ(IterStatus(iter), "(invalid)");

  delete iter;
}

TEST(DBTest, IterSmallAndLargeMix) {
  ASSERT_OK(Put("a", "va"));
  ASSERT_OK(Put("b", std::string(100000, 'b')));
  ASSERT_OK(Put("c", "vc"));
  ASSERT_OK(Put("d", std::string(100000, 'd')));
  ASSERT_OK(Put("e", std::string(100000, 'e')));

  Iterator* iter = db_->NewIterator(ReadOptions());

  iter->SeekToFirst();
  ASSERT_EQ(IterStatus(iter), "a->va");
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "b->" + std::string(100000, 'b'));
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "c->vc");
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "d->" + std::string(100000, 'd'));
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "e->" + std::string(100000, 'e'));
  iter->Next();
  ASSERT_EQ(IterStatus(iter), "(invalid)");

  iter->SeekToLast();
  ASSERT_EQ(IterStatus(iter), "e->" + std::string(100000, 'e'));
  iter->Prev();
  ASSERT_EQ(IterStatus(iter), "d->" + std::string(100000, 'd'));
  iter->Prev();
  ASSERT_EQ(IterStatus(iter), "c->vc");
  iter->Prev();
  ASSERT_EQ(IterStatus(iter), "b->" + std::string(100000, 'b'));
  iter->Prev();
  ASSERT_EQ(IterStatus(iter), "a->va");
  iter->Prev();
  ASSERT_EQ(IterStatus(iter), "(invalid)");

  delete iter;
}

TEST(DBTest, IterMultiWithDelete) {
  do {
    ASSERT_OK(Put("a", "va"));
    ASSERT_OK(Put("b", "vb"));
    ASSERT_OK(Put("c", "vc"));
    ASSERT_OK(Delete("b"));
    ASSERT_EQ("NOT_FOUND", Get("b"));

    Iterator* iter = db_->NewIterator(ReadOptions());
    iter->Seek("c");
    ASSERT_EQ(IterStatus(iter), "c->vc");
    iter->Prev();
    ASSERT_EQ(IterStatus(iter), "a->va");
    delete iter;
  } while (ChangeOptions());
}

TEST(DBTest, Recover) {
  do {
    ASSERT_OK(Put("foo", "v1"));
    ASSERT_OK(Put("baz", "v5"));

    Reopen();
    ASSERT_EQ("v1", Get("foo"));

    ASSERT_EQ("v1", Get("foo"));
    ASSERT_EQ("v5", Get("baz"));
    ASSERT_OK(Put("bar", "v2"));
    ASSERT_OK(Put("foo", "v3"));

    Reopen();
    ASSERT_EQ("v3", Get("foo"));
    ASSERT_OK(Put("foo", "v4"));
    ASSERT_EQ("v4", Get("foo"));
    ASSERT_EQ("v2", Get("bar"));
    ASSERT_EQ("v5", Get("baz"));
  } while (ChangeOptions());
}

TEST(DBTest, RecoveryWithEmptyLog) {
  do {
    ASSERT_OK(Put("foo", "v1"));
    ASSERT_OK(Put("foo", "v2"));
    Reopen();
    Reopen();
    ASSERT_OK(Put("foo", "v3"));
    Reopen();
    ASSERT_EQ("v3", Get("foo"));
  } while (ChangeOptions());
}

//检查Memtable压缩期间完成的写入是否恢复
//如果数据库在MEMTABLE压缩期间关闭。
TEST(DBTest, RecoverDuringMemtableCompaction) {
  do {
    Options options = CurrentOptions();
    options.env = env_;
    options.write_buffer_size = 1000000;
    Reopen(&options);

//触发长memtable压缩，并在压缩过程中重新打开数据库
ASSERT_OK(Put("foo", "v1"));                         //转到第一个日志文件
ASSERT_OK(Put("big1", std::string(10000000, 'x')));  //填充可记忆
ASSERT_OK(Put("big2", std::string(1000, 'y')));      //触发压缩
ASSERT_OK(Put("bar", "v2"));                         //转到新日志文件

    Reopen(&options);
    ASSERT_EQ("v1", Get("foo"));
    ASSERT_EQ("v2", Get("bar"));
    ASSERT_EQ(std::string(10000000, 'x'), Get("big1"));
    ASSERT_EQ(std::string(1000, 'y'), Get("big2"));
  } while (ChangeOptions());
}

static std::string Key(int i) {
  char buf[100];
  snprintf(buf, sizeof(buf), "key%06d", i);
  return std::string(buf);
}

TEST(DBTest, MinorCompactionsHappen) {
  Options options = CurrentOptions();
  options.write_buffer_size = 10000;
  Reopen(&options);

  const int N = 500;

  int starting_num_tables = TotalTableFiles();
  for (int i = 0; i < N; i++) {
    ASSERT_OK(Put(Key(i), Key(i) + std::string(1000, 'v')));
  }
  int ending_num_tables = TotalTableFiles();
  ASSERT_GT(ending_num_tables, starting_num_tables);

  for (int i = 0; i < N; i++) {
    ASSERT_EQ(Key(i) + std::string(1000, 'v'), Get(Key(i)));
  }

  Reopen();

  for (int i = 0; i < N; i++) {
    ASSERT_EQ(Key(i) + std::string(1000, 'v'), Get(Key(i)));
  }
}

TEST(DBTest, RecoverWithLargeLog) {
  {
    Options options = CurrentOptions();
    Reopen(&options);
    ASSERT_OK(Put("big1", std::string(200000, '1')));
    ASSERT_OK(Put("big2", std::string(200000, '2')));
    ASSERT_OK(Put("small3", std::string(10, '3')));
    ASSERT_OK(Put("small4", std::string(10, '4')));
    ASSERT_EQ(NumTableFilesAtLevel(0), 0);
  }

//确保如果我们用一个小的写缓冲区大小打开
//我们在一个大日志文件的中间刷新表文件。
  Options options = CurrentOptions();
  options.write_buffer_size = 100000;
  Reopen(&options);
  ASSERT_EQ(NumTableFilesAtLevel(0), 3);
  ASSERT_EQ(std::string(200000, '1'), Get("big1"));
  ASSERT_EQ(std::string(200000, '2'), Get("big2"));
  ASSERT_EQ(std::string(10, '3'), Get("small3"));
  ASSERT_EQ(std::string(10, '4'), Get("small4"));
  ASSERT_GT(NumTableFilesAtLevel(0), 1);
}

TEST(DBTest, CompactionsGenerateMultipleFiles) {
  Options options = CurrentOptions();
options.write_buffer_size = 100000000;        //大写缓冲区
  Reopen(&options);

  Random rnd(301);

//写入8MB（80个值，每个100K）
  ASSERT_EQ(NumTableFilesAtLevel(0), 0);
  std::vector<std::string> values;
  for (int i = 0; i < 80; i++) {
    values.push_back(RandomString(&rnd, 100000));
    ASSERT_OK(Put(Key(i), values[i]));
  }

//重新打开将更新移动到0级
  Reopen(&options);
  dbfull()->TEST_CompactRange(0, NULL, NULL);

  ASSERT_EQ(NumTableFilesAtLevel(0), 0);
  ASSERT_GT(NumTableFilesAtLevel(1), 1);
  for (int i = 0; i < 80; i++) {
    ASSERT_EQ(Get(Key(i)), values[i]);
  }
}

TEST(DBTest, RepeatedWritesToSameKey) {
  Options options = CurrentOptions();
  options.env = env_;
options.write_buffer_size = 100000;  //小写缓冲区
  Reopen(&options);

//每个级别最多只能有一个文件，级别0除外，
//最多可以有kl0_stopWriteStrigger文件。
  const int kMaxFiles = config::kNumLevels + config::kL0_StopWritesTrigger;

  Random rnd(301);
  std::string value = RandomString(&rnd, 2 * options.write_buffer_size);
  for (int i = 0; i < 5 * kMaxFiles; i++) {
    Put("key", value);
    ASSERT_LE(TotalTableFiles(), kMaxFiles);
    fprintf(stderr, "after %d: %d files\n", int(i+1), TotalTableFiles());
  }
}

TEST(DBTest, SparseMerge) {
  Options options = CurrentOptions();
  options.compression = kNoCompression;
  Reopen(&options);

  FillLevels("A", "Z");

//假设有：
//前缀为A的少量数据
//前缀为b的大量数据
//前缀为C的少量数据
//最近的更新对这三个前缀都做了小的改动。
//检查一下，我们没有进行一次压缩，将所有的B合并到一个镜头中。
  const std::string value(1000, 'x');
  Put("A", "va");
//写入大约100MB的“b”值
  for (int i = 0; i < 100000; i++) {
    char key[100];
    snprintf(key, sizeof(key), "B%010d", i);
    Put(key, value);
  }
  Put("C", "vc");
  dbfull()->TEST_CompactMemTable();
  dbfull()->TEST_CompactRange(0, NULL, NULL);

//进行稀疏更新
  Put("A",    "va2");
  Put("B100", "bvalue2");
  Put("C",    "vc2");
  dbfull()->TEST_CompactMemTable();

//压实不应使我们创造这样一种情况
//一个文件在下一级重叠了太多的数据。
  ASSERT_LE(dbfull()->TEST_MaxNextLevelOverlappingBytes(), 20*1048576);
  dbfull()->TEST_CompactRange(0, NULL, NULL);
  ASSERT_LE(dbfull()->TEST_MaxNextLevelOverlappingBytes(), 20*1048576);
  dbfull()->TEST_CompactRange(1, NULL, NULL);
  ASSERT_LE(dbfull()->TEST_MaxNextLevelOverlappingBytes(), 20*1048576);
}

static bool Between(uint64_t val, uint64_t low, uint64_t high) {
  bool result = (val >= low) && (val <= high);
  if (!result) {
    fprintf(stderr, "Value %llu is not in range [%llu, %llu]\n",
            (unsigned long long)(val),
            (unsigned long long)(low),
            (unsigned long long)(high));
  }
  return result;
}

TEST(DBTest, ApproximateSizes) {
  do {
    Options options = CurrentOptions();
options.write_buffer_size = 100000000;        //大写缓冲区
    options.compression = kNoCompression;
    DestroyAndReopen();

    ASSERT_TRUE(Between(Size("", "xyz"), 0, 0));
    Reopen(&options);
    ASSERT_TRUE(Between(Size("", "xyz"), 0, 0));

//写入8MB（80个值，每个100K）
    ASSERT_EQ(NumTableFilesAtLevel(0), 0);
    const int N = 80;
    static const int S1 = 100000;
static const int S2 = 105000;  //允许从元数据进行一些扩展
    Random rnd(301);
    for (int i = 0; i < N; i++) {
      ASSERT_OK(Put(Key(i), RandomString(&rnd, S1)));
    }

//0，因为getApproxidesizes（）不考虑memtable空间
    ASSERT_TRUE(Between(Size("", Key(50)), 0, 0));

    if (options.reuse_logs) {
//恢复将重用memtable，而getApproxidesizes（）则不会
//memtable使用说明；
      Reopen(&options);
      ASSERT_TRUE(Between(Size("", Key(50)), 0, 0));
      continue;
    }

//通过重新打开几次来检查整个恢复过程中的大小
    for (int run = 0; run < 3; run++) {
      Reopen(&options);

      for (int compact_start = 0; compact_start < N; compact_start += 10) {
        for (int i = 0; i < N; i += 10) {
          ASSERT_TRUE(Between(Size("", Key(i)), S1*i, S2*i));
          ASSERT_TRUE(Between(Size("", Key(i)+".suffix"), S1*(i+1), S2*(i+1)));
          ASSERT_TRUE(Between(Size(Key(i), Key(i+10)), S1*10, S2*10));
        }
        ASSERT_TRUE(Between(Size("", Key(50)), S1*50, S2*50));
        ASSERT_TRUE(Between(Size("", Key(50)+".suffix"), S1*50, S2*50));

        std::string cstart_str = Key(compact_start);
        std::string cend_str = Key(compact_start + 9);
        Slice cstart = cstart_str;
        Slice cend = cend_str;
        dbfull()->TEST_CompactRange(0, &cstart, &cend);
      }

      ASSERT_EQ(NumTableFilesAtLevel(0), 0);
      ASSERT_GT(NumTableFilesAtLevel(1), 0);
    }
  } while (ChangeOptions());
}

TEST(DBTest, ApproximateSizes_MixOfSmallAndLarge) {
  do {
    Options options = CurrentOptions();
    options.compression = kNoCompression;
    Reopen();

    Random rnd(301);
    std::string big1 = RandomString(&rnd, 100000);
    ASSERT_OK(Put(Key(0), RandomString(&rnd, 10000)));
    ASSERT_OK(Put(Key(1), RandomString(&rnd, 10000)));
    ASSERT_OK(Put(Key(2), big1));
    ASSERT_OK(Put(Key(3), RandomString(&rnd, 10000)));
    ASSERT_OK(Put(Key(4), big1));
    ASSERT_OK(Put(Key(5), RandomString(&rnd, 10000)));
    ASSERT_OK(Put(Key(6), RandomString(&rnd, 300000)));
    ASSERT_OK(Put(Key(7), RandomString(&rnd, 10000)));

    if (options.reuse_logs) {
//需要强制进行memtable压缩，因为恢复不会这样做。
      ASSERT_OK(dbfull()->TEST_CompactMemTable());
    }

//通过重新打开几次来检查整个恢复过程中的大小
    for (int run = 0; run < 3; run++) {
      Reopen(&options);

      ASSERT_TRUE(Between(Size("", Key(0)), 0, 0));
      ASSERT_TRUE(Between(Size("", Key(1)), 10000, 11000));
      ASSERT_TRUE(Between(Size("", Key(2)), 20000, 21000));
      ASSERT_TRUE(Between(Size("", Key(3)), 120000, 121000));
      ASSERT_TRUE(Between(Size("", Key(4)), 130000, 131000));
      ASSERT_TRUE(Between(Size("", Key(5)), 230000, 231000));
      ASSERT_TRUE(Between(Size("", Key(6)), 240000, 241000));
      ASSERT_TRUE(Between(Size("", Key(7)), 540000, 541000));
      ASSERT_TRUE(Between(Size("", Key(8)), 550000, 560000));

      ASSERT_TRUE(Between(Size(Key(3), Key(5)), 110000, 111000));

      dbfull()->TEST_CompactRange(0, NULL, NULL);
    }
  } while (ChangeOptions());
}

TEST(DBTest, IteratorPinsRef) {
  Put("foo", "hello");

//获取将生成数据库当前内容的迭代器。
  Iterator* iter = db_->NewIterator(ReadOptions());

//写入强制压缩
  Put("foo", "newvalue1");
  for (int i = 0; i < 100; i++) {
ASSERT_OK(Put(Key(i), Key(i) + std::string(100000, 'v'))); //100K值
  }
  Put("foo", "newvalue2");

  iter->SeekToFirst();
  ASSERT_TRUE(iter->Valid());
  ASSERT_EQ("foo", iter->key().ToString());
  ASSERT_EQ("hello", iter->value().ToString());
  iter->Next();
  ASSERT_TRUE(!iter->Valid());
  delete iter;
}

TEST(DBTest, Snapshot) {
  do {
    Put("foo", "v1");
    const Snapshot* s1 = db_->GetSnapshot();
    Put("foo", "v2");
    const Snapshot* s2 = db_->GetSnapshot();
    Put("foo", "v3");
    const Snapshot* s3 = db_->GetSnapshot();

    Put("foo", "v4");
    ASSERT_EQ("v1", Get("foo", s1));
    ASSERT_EQ("v2", Get("foo", s2));
    ASSERT_EQ("v3", Get("foo", s3));
    ASSERT_EQ("v4", Get("foo"));

    db_->ReleaseSnapshot(s3);
    ASSERT_EQ("v1", Get("foo", s1));
    ASSERT_EQ("v2", Get("foo", s2));
    ASSERT_EQ("v4", Get("foo"));

    db_->ReleaseSnapshot(s1);
    ASSERT_EQ("v2", Get("foo", s2));
    ASSERT_EQ("v4", Get("foo"));

    db_->ReleaseSnapshot(s2);
    ASSERT_EQ("v4", Get("foo"));
  } while (ChangeOptions());
}

TEST(DBTest, HiddenValuesAreRemoved) {
  do {
    Random rnd(301);
    FillLevels("a", "z");

    std::string big = RandomString(&rnd, 50000);
    Put("foo", big);
    Put("pastfoo", "v");
    const Snapshot* snapshot = db_->GetSnapshot();
    Put("foo", "tiny");
Put("pastfoo2", "v2");        //再推进一号序列

    ASSERT_OK(dbfull()->TEST_CompactMemTable());
    ASSERT_GT(NumTableFilesAtLevel(0), 0);

    ASSERT_EQ(big, Get("foo", snapshot));
    ASSERT_TRUE(Between(Size("", "pastfoo"), 50000, 60000));
    db_->ReleaseSnapshot(snapshot);
    ASSERT_EQ(AllEntriesFor("foo"), "[ tiny, " + big + " ]");
    Slice x("x");
    dbfull()->TEST_CompactRange(0, NULL, &x);
    ASSERT_EQ(AllEntriesFor("foo"), "[ tiny ]");
    ASSERT_EQ(NumTableFilesAtLevel(0), 0);
    ASSERT_GE(NumTableFilesAtLevel(1), 1);
    dbfull()->TEST_CompactRange(1, NULL, &x);
    ASSERT_EQ(AllEntriesFor("foo"), "[ tiny ]");

    ASSERT_TRUE(Between(Size("", "pastfoo"), 0, 1000));
  } while (ChangeOptions());
}

TEST(DBTest, DeletionMarkers1) {
  Put("foo", "v1");
  ASSERT_OK(dbfull()->TEST_CompactMemTable());
  const int last = config::kMaxMemCompactLevel;
ASSERT_EQ(NumTableFilesAtLevel(last), 1);   //foo=>v1现在处于最后一级

//将表放在最后-1级，以防止与前面的突变合并。
  Put("a", "begin");
  Put("z", "end");
  dbfull()->TEST_CompactMemTable();
  ASSERT_EQ(NumTableFilesAtLevel(last), 1);
  ASSERT_EQ(NumTableFilesAtLevel(last-1), 1);

  Delete("foo");
  Put("foo", "v2");
  ASSERT_EQ(AllEntriesFor("foo"), "[ v2, DEL, v1 ]");
ASSERT_OK(dbfull()->TEST_CompactMemTable());  //移到最后2级
  ASSERT_EQ(AllEntriesFor("foo"), "[ v2, DEL, v1 ]");
  Slice z("z");
  dbfull()->TEST_CompactRange(last-2, NULL, &z);
//删除了，但v1仍然存在，因为我们没有压缩那个级别
//（del可以消除，因为v2隐藏v1）。
  ASSERT_EQ(AllEntriesFor("foo"), "[ v2, v1 ]");
  dbfull()->TEST_CompactRange(last-1, NULL, NULL);
//合并last-1 w/last，所以我们是“foo”的基础级别，所以
//删除Del。（如V1）。
  ASSERT_EQ(AllEntriesFor("foo"), "[ v2 ]");
}

TEST(DBTest, DeletionMarkers2) {
  Put("foo", "v1");
  ASSERT_OK(dbfull()->TEST_CompactMemTable());
  const int last = config::kMaxMemCompactLevel;
ASSERT_EQ(NumTableFilesAtLevel(last), 1);   //foo=>v1现在处于最后一级

//将表放在最后-1级，以防止与前面的突变合并。
  Put("a", "begin");
  Put("z", "end");
  dbfull()->TEST_CompactMemTable();
  ASSERT_EQ(NumTableFilesAtLevel(last), 1);
  ASSERT_EQ(NumTableFilesAtLevel(last-1), 1);

  Delete("foo");
  ASSERT_EQ(AllEntriesFor("foo"), "[ DEL, v1 ]");
ASSERT_OK(dbfull()->TEST_CompactMemTable());  //移到最后2级
  ASSERT_EQ(AllEntriesFor("foo"), "[ DEL, v1 ]");
  dbfull()->TEST_CompactRange(last-2, NULL, NULL);
//del keepd：“last”文件重叠
  ASSERT_EQ(AllEntriesFor("foo"), "[ DEL, v1 ]");
  dbfull()->TEST_CompactRange(last-1, NULL, NULL);
//合并last-1 w/last，所以我们是“foo”的基础级别，所以
//删除Del。（如V1）。
  ASSERT_EQ(AllEntriesFor("foo"), "[ ]");
}

TEST(DBTest, OverlapInLevel0) {
  do {
    ASSERT_EQ(config::kMaxMemCompactLevel, 2) << "Fix test to match config";

//填充级别1和2以禁用将新的memtable推送到级别>0。
    ASSERT_OK(Put("100", "v100"));
    ASSERT_OK(Put("999", "v999"));
    dbfull()->TEST_CompactMemTable();
    ASSERT_OK(Delete("100"));
    ASSERT_OK(Delete("999"));
    dbfull()->TEST_CompactMemTable();
    ASSERT_EQ("0,1,1", FilesPerLevel());

//使文件在级别0中跨越以下范围：
//文件[0]200..九百
//文件[1]300..五百
//请注意，文件是按最小键排序的。
    ASSERT_OK(Put("300", "v300"));
    ASSERT_OK(Put("500", "v500"));
    dbfull()->TEST_CompactMemTable();
    ASSERT_OK(Put("200", "v200"));
    ASSERT_OK(Put("600", "v600"));
    ASSERT_OK(Put("900", "v900"));
    dbfull()->TEST_CompactMemTable();
    ASSERT_EQ("2,1,1", FilesPerLevel());

//压缩掉我们最初创建的占位符文件
    dbfull()->TEST_CompactRange(1, NULL, NULL);
    dbfull()->TEST_CompactRange(2, NULL, NULL);
    ASSERT_EQ("2", FilesPerLevel());

//进行MEMTABLE压缩。在修复bug之前，压缩将
//未检测到与0级文件的重叠，将错误地放置
//更深层次的删除。
    ASSERT_OK(Delete("600"));
    dbfull()->TEST_CompactMemTable();
    ASSERT_EQ("3", FilesPerLevel());
    ASSERT_EQ("NOT_FOUND", Get("600"));
  } while (ChangeOptions());
}

TEST(DBTest, L0_CompactionBug_Issue44_a) {
  Reopen();
  ASSERT_OK(Put("b", "v"));
  Reopen();
  ASSERT_OK(Delete("b"));
  ASSERT_OK(Delete("a"));
  Reopen();
  ASSERT_OK(Delete("a"));
  Reopen();
  ASSERT_OK(Put("a", "v"));
  Reopen();
  Reopen();
  ASSERT_EQ("(a->v)", Contents());
DelayMilliseconds(1000);  //等待压实完成
  ASSERT_EQ("(a->v)", Contents());
}

TEST(DBTest, L0_CompactionBug_Issue44_b) {
  Reopen();
  Put("","");
  Reopen();
  Delete("e");
  Put("","");
  Reopen();
  Put("c", "cv");
  Reopen();
  Put("","");
  Reopen();
  Put("","");
DelayMilliseconds(1000);  //等待压实完成
  Reopen();
  Put("d","dv");
  Reopen();
  Put("","");
  Reopen();
  Delete("d");
  Delete("b");
  Reopen();
  ASSERT_EQ("(->)(c->cv)", Contents());
DelayMilliseconds(1000);  //等待压实完成
  ASSERT_EQ("(->)(c->cv)", Contents());
}

TEST(DBTest, ComparatorCheck) {
  class NewComparator : public Comparator {
   public:
    virtual const char* Name() const { return "leveldb.NewComparator"; }
    virtual int Compare(const Slice& a, const Slice& b) const {
      return BytewiseComparator()->Compare(a, b);
    }
    virtual void FindShortestSeparator(std::string* s, const Slice& l) const {
      BytewiseComparator()->FindShortestSeparator(s, l);
    }
    virtual void FindShortSuccessor(std::string* key) const {
      BytewiseComparator()->FindShortSuccessor(key);
    }
  };
  NewComparator cmp;
  Options new_options = CurrentOptions();
  new_options.comparator = &cmp;
  Status s = TryReopen(&new_options);
  ASSERT_TRUE(!s.ok());
  ASSERT_TRUE(s.ToString().find("comparator") != std::string::npos)
      << s.ToString();
}

TEST(DBTest, CustomComparator) {
  class NumberComparator : public Comparator {
   public:
    virtual const char* Name() const { return "test.NumberComparator"; }
    virtual int Compare(const Slice& a, const Slice& b) const {
      return ToNumber(a) - ToNumber(b);
    }
    virtual void FindShortestSeparator(std::string* s, const Slice& l) const {
ToNumber(*s);     //检查格式
ToNumber(l);      //检查格式
    }
    virtual void FindShortSuccessor(std::string* key) const {
ToNumber(*key);   //检查格式
    }
   private:
    static int ToNumber(const Slice& x) {
//检查是否没有多余的字符。
      ASSERT_TRUE(x.size() >= 2 && x[0] == '[' && x[x.size()-1] == ']')
          << EscapeString(x);
      int val;
      char ignored;
      ASSERT_TRUE(sscanf(x.ToString().c_str(), "[%i]%c", &val, &ignored) == 1)
          << EscapeString(x);
      return val;
    }
  };
  NumberComparator cmp;
  Options new_options = CurrentOptions();
  new_options.create_if_missing = true;
  new_options.comparator = &cmp;
new_options.filter_policy = NULL;     //无法使用Bloom筛选器
new_options.write_buffer_size = 1000;  //更频繁地压缩
  DestroyAndReopen(&new_options);
  ASSERT_OK(Put("[10]", "ten"));
  ASSERT_OK(Put("[0x14]", "twenty"));
  for (int i = 0; i < 2; i++) {
    ASSERT_EQ("ten", Get("[10]"));
    ASSERT_EQ("ten", Get("[0xa]"));
    ASSERT_EQ("twenty", Get("[20]"));
    ASSERT_EQ("twenty", Get("[0x14]"));
    ASSERT_EQ("NOT_FOUND", Get("[15]"));
    ASSERT_EQ("NOT_FOUND", Get("[0xf]"));
    Compact("[0]", "[9999]");
  }

  for (int run = 0; run < 2; run++) {
    for (int i = 0; i < 1000; i++) {
      char buf[100];
      snprintf(buf, sizeof(buf), "[%d]", i*10);
      ASSERT_OK(Put(buf, buf));
    }
    Compact("[0]", "[1000000]");
  }
}

TEST(DBTest, ManualCompaction) {
  ASSERT_EQ(config::kMaxMemCompactLevel, 2)
      << "Need to update this test to match kMaxMemCompactLevel";

  MakeTables(3, "p", "q");
  ASSERT_EQ("1,1,1", FilesPerLevel());

//压缩范围在文件之前
  Compact("", "c");
  ASSERT_EQ("1,1,1", FilesPerLevel());

//压缩范围在文件之后
  Compact("r", "z");
  ASSERT_EQ("1,1,1", FilesPerLevel());

//压缩范围与文件重叠
  Compact("p1", "p9");
  ASSERT_EQ("0,0,1", FilesPerLevel());

//填充其他区域
  MakeTables(3, "c", "e");
  ASSERT_EQ("1,1,2", FilesPerLevel());

//缩小新范围
  Compact("b", "f");
  ASSERT_EQ("0,0,2", FilesPerLevel());

//全部压缩
  MakeTables(1, "a", "z");
  ASSERT_EQ("0,1,2", FilesPerLevel());
  db_->CompactRange(NULL, NULL);
  ASSERT_EQ("0,0,1", FilesPerLevel());
}

TEST(DBTest, DBOpen_Options) {
  std::string dbname = test::TmpDir() + "/db_options_test";
  DestroyDB(dbname, Options());

//不存在，并创建_if_missing==false:error
  DB* db = NULL;
  Options opts;
  opts.create_if_missing = false;
  Status s = DB::Open(opts, dbname, &db);
  ASSERT_TRUE(strstr(s.ToString().c_str(), "does not exist") != NULL);
  ASSERT_TRUE(db == NULL);

//不存在，并创建_if_missing==true:ok
  opts.create_if_missing = true;
  s = DB::Open(opts, dbname, &db);
  ASSERT_OK(s);
  ASSERT_TRUE(db != NULL);

  delete db;
  db = NULL;

//是否存在，如果存在，则返回错误“=true:error”
  opts.create_if_missing = false;
  opts.error_if_exists = true;
  s = DB::Open(opts, dbname, &db);
  ASSERT_TRUE(strstr(s.ToString().c_str(), "exists") != NULL);
  ASSERT_TRUE(db == NULL);

//是否存在，如果存在则出错_==false:ok
  opts.create_if_missing = true;
  opts.error_if_exists = false;
  s = DB::Open(opts, dbname, &db);
  ASSERT_OK(s);
  ASSERT_TRUE(db != NULL);

  delete db;
  db = NULL;
}

TEST(DBTest, Locking) {
  DB* db2 = NULL;
  Status s = DB::Open(CurrentOptions(), dbname_, &db2);
  ASSERT_TRUE(!s.ok()) << "Locking did not prevent re-opening db";
}

//检查当我们的空间不足时文件数量不会增加
TEST(DBTest, NoSpace) {
  Options options = CurrentOptions();
  options.env = env_;
  Reopen(&options);

  ASSERT_OK(Put("foo", "v1"));
  ASSERT_EQ("v1", Get("foo"));
  Compact("a", "z");
  const int num_files = CountFiles();
env_->no_space_.Release_Store(env_);   //强制超出空间错误
  for (int i = 0; i < 10; i++) {
    for (int level = 0; level < config::kNumLevels-1; level++) {
      dbfull()->TEST_CompactRange(level, NULL, NULL);
    }
  }
  env_->no_space_.Release_Store(NULL);
  ASSERT_LT(CountFiles(), num_files + 3);
}

TEST(DBTest, NonWritableFileSystem) {
  Options options = CurrentOptions();
  options.write_buffer_size = 1000;
  options.env = env_;
  Reopen(&options);
  ASSERT_OK(Put("foo", "v1"));
env_->non_writable_.Release_Store(env_);  //强制新文件出错
  std::string big(100000, 'x');
  int errors = 0;
  for (int i = 0; i < 20; i++) {
    fprintf(stderr, "iter %d; errors %d\n", i, errors);
    if (!Put("foo", big).ok()) {
      errors++;
      DelayMilliseconds(100);
    }
  }
  ASSERT_GT(errors, 0);
  env_->non_writable_.Release_Store(NULL);
}

TEST(DBTest, WriteSyncError) {
//检查日志同步错误是否导致数据库不允许将来写入。

//（A）导致日志同步调用失败
  Options options = CurrentOptions();
  options.env = env_;
  Reopen(&options);
  env_->data_sync_error_.Release_Store(env_);

//（B）正常写入应成功
  WriteOptions w;
  ASSERT_OK(db_->Put(w, "k1", "v1"));
  ASSERT_EQ("v1", Get("k1"));

//（c）进行同步写入；应失败
  w.sync = true;
  ASSERT_TRUE(!db_->Put(w, "k2", "v2").ok());
  ASSERT_EQ("v1", Get("k1"));
  ASSERT_EQ("NOT_FOUND", Get("k2"));

//（D）使同步行为正常
  env_->data_sync_error_.Release_Store(NULL);

//（e）进行非同步写入；应失败
  w.sync = false;
  ASSERT_TRUE(!db_->Put(w, "k3", "v3").ok());
  ASSERT_EQ("v1", Get("k1"));
  ASSERT_EQ("NOT_FOUND", Get("k2"));
  ASSERT_EQ("NOT_FOUND", Get("k3"));
}

TEST(DBTest, ManifestWriteError) {
//测试以下问题：
//（a）压缩产生文件F
//（B）包含f的日志记录被写入清单文件，但sync（）失败
//（c）gc删除f
//（d）重新打开数据库后，由于日志记录中指定了已删除的f，因此读取失败。

//我们重复两次。在第二次迭代中，一切都是
//除了日志记录之外，其他记录都不会进入清单文件。
  for (int iter = 0; iter < 2; iter++) {
    port::AtomicPointer* error_type = (iter == 0)
        ? &env_->manifest_sync_error_
        : &env_->manifest_write_error_;

//插入foo=>条形图
    Options options = CurrentOptions();
    options.env = env_;
    options.create_if_missing = true;
    options.error_if_exists = false;
    DestroyAndReopen(&options);
    ASSERT_OK(Put("foo", "bar"));
    ASSERT_EQ("bar", Get("foo"));

//MEMTABLE压缩（将成功）
    dbfull()->TEST_CompactMemTable();
    ASSERT_EQ("bar", Get("foo"));
    const int last = config::kMaxMemCompactLevel;
ASSERT_EQ(NumTableFilesAtLevel(last), 1);   //foo=>酒吧现在在最后一层

//合并压缩（将失败）
    error_type->Release_Store(env_);
dbfull()->TEST_CompactRange(last, NULL, NULL);  //应该失败
    ASSERT_EQ("bar", Get("foo"));

//恢复：不应丢失数据
    error_type->Release_Store(NULL);
    Reopen(&options);
    ASSERT_EQ("bar", Get("foo"));
  }
}

TEST(DBTest, MissingSSTFile) {
  ASSERT_OK(Put("foo", "bar"));
  ASSERT_EQ("bar", Get("foo"));

//将memtable转储到磁盘。
  dbfull()->TEST_CompactMemTable();
  ASSERT_EQ("bar", Get("foo"));

  Close();
  ASSERT_TRUE(DeleteAnSSTFile());
  Options options = CurrentOptions();
  options.paranoid_checks = true;
  Status s = TryReopen(&options);
  ASSERT_TRUE(!s.ok());
  ASSERT_TRUE(s.ToString().find("issing") != std::string::npos)
      << s.ToString();
}

TEST(DBTest, StillReadSST) {
  ASSERT_OK(Put("foo", "bar"));
  ASSERT_EQ("bar", Get("foo"));

//将memtable转储到磁盘。
  dbfull()->TEST_CompactMemTable();
  ASSERT_EQ("bar", Get("foo"));
  Close();
  ASSERT_GT(RenameLDBToSST(), 0);
  Options options = CurrentOptions();
  options.paranoid_checks = true;
  Status s = TryReopen(&options);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ("bar", Get("foo"));
}

TEST(DBTest, FilesDeletedAfterCompaction) {
  ASSERT_OK(Put("foo", "v2"));
  Compact("a", "z");
  const int num_files = CountFiles();
  for (int i = 0; i < 10; i++) {
    ASSERT_OK(Put("foo", "v2"));
    Compact("a", "z");
  }
  ASSERT_EQ(CountFiles(), num_files);
}

TEST(DBTest, BloomFilter) {
  env_->count_random_reads_ = true;
  Options options = CurrentOptions();
  options.env = env_;
options.block_cache = NewLRUCache(0);  //防止缓存命中
  options.filter_policy = NewBloomFilterPolicy(10);
  Reopen(&options);

//填充多个层
  const int N = 10000;
  for (int i = 0; i < N; i++) {
    ASSERT_OK(Put(Key(i), Key(i)));
  }
  Compact("a", "z");
  for (int i = 0; i < N; i += 100) {
    ASSERT_OK(Put(Key(i), Key(i)));
  }
  dbfull()->TEST_CompactMemTable();

//阻止由查找触发的自动压缩
  env_->delay_data_sync_.Release_Store(env_);

//查找当前密钥。应该很少从小型sstable中读取。
  env_->random_read_counter_.Reset();
  for (int i = 0; i < N; i++) {
    ASSERT_EQ(Key(i), Get(Key(i)));
  }
  int reads = env_->random_read_counter_.Read();
  fprintf(stderr, "%d present => %d reads\n", N, reads);
  ASSERT_GE(reads, N);
  ASSERT_LE(reads, N + 2*N/100);

//查找当前密钥。应该很少从sstable中读取。
  env_->random_read_counter_.Reset();
  for (int i = 0; i < N; i++) {
    ASSERT_EQ("NOT_FOUND", Get(Key(i) + ".missing"));
  }
  reads = env_->random_read_counter_.Read();
  fprintf(stderr, "%d missing => %d reads\n", N, reads);
  ASSERT_LE(reads, 3*N/100);

  env_->delay_data_sync_.Release_Store(NULL);
  Close();
  delete options.block_cache;
  delete options.filter_policy;
}

//多线程测试：
namespace {

static const int kNumThreads = 4;
static const int kTestSeconds = 10;
static const int kNumKeys = 1000;

struct MTState {
  DBTest* test;
  port::AtomicPointer stop;
  port::AtomicPointer counter[kNumThreads];
  port::AtomicPointer thread_done[kNumThreads];
};

struct MTThread {
  MTState* state;
  int id;
};

static void MTThreadBody(void* arg) {
  MTThread* t = reinterpret_cast<MTThread*>(arg);
  int id = t->id;
  DB* db = t->state->test->db_;
  uintptr_t counter = 0;
  fprintf(stderr, "... starting thread %d\n", id);
  Random rnd(1000 + id);
  std::string value;
  char valbuf[1500];
  while (t->state->stop.Acquire_Load() == NULL) {
    t->state->counter[id].Release_Store(reinterpret_cast<void*>(counter));

    int key = rnd.Uniform(kNumKeys);
    char keybuf[20];
    snprintf(keybuf, sizeof(keybuf), "%016d", key);

    if (rnd.OneIn(2)) {
//写入表单<key，my id，counter>的值。
//我们为力的压缩添加了一些填充物。
      snprintf(valbuf, sizeof(valbuf), "%d.%d.%-1000d",
               key, id, static_cast<int>(counter));
      ASSERT_OK(db->Put(WriteOptions(), Slice(keybuf), Slice(valbuf)));
    } else {
//读取一个值并验证它是否与上面写入的模式匹配。
      Status s = db->Get(ReadOptions(), Slice(keybuf), &value);
      if (s.IsNotFound()) {
//密钥尚未写入
      } else {
//检查写入线程计数器是否大于等于值中的计数器
        ASSERT_OK(s);
        int k, w, c;
        ASSERT_EQ(3, sscanf(value.c_str(), "%d.%d.%d", &k, &w, &c)) << value;
        ASSERT_EQ(k, key);
        ASSERT_GE(w, 0);
        ASSERT_LT(w, kNumThreads);
        ASSERT_LE(static_cast<uintptr_t>(c), reinterpret_cast<uintptr_t>(
            t->state->counter[w].Acquire_Load()));
      }
    }
    counter++;
  }
  t->state->thread_done[id].Release_Store(t);
  fprintf(stderr, "... stopping thread %d after %d ops\n", id, int(counter));
}

}  //命名空间

TEST(DBTest, MultiThreaded) {
  do {
//初始化状态
    MTState mt;
    mt.test = this;
    mt.stop.Release_Store(0);
    for (int id = 0; id < kNumThreads; id++) {
      mt.counter[id].Release_Store(0);
      mt.thread_done[id].Release_Store(0);
    }

//启动线程
    MTThread thread[kNumThreads];
    for (int id = 0; id < kNumThreads; id++) {
      thread[id].state = &mt;
      thread[id].id = id;
      env_->StartThread(MTThreadBody, &thread[id]);
    }

//让他们跑一会儿
    DelayMilliseconds(kTestSeconds * 1000);

//停止线程并等待它们完成
    mt.stop.Release_Store(&mt);
    for (int id = 0; id < kNumThreads; id++) {
      while (mt.thread_done[id].Acquire_Load() == NULL) {
        DelayMilliseconds(100);
      }
    }
  } while (ChangeOptions());
}

namespace {
typedef std::map<std::string, std::string> KVMap;
}

class ModelDB: public DB {
 public:
  class ModelSnapshot : public Snapshot {
   public:
    KVMap map_;
  };

  explicit ModelDB(const Options& options): options_(options) { }
  ~ModelDB() { }
  virtual Status Put(const WriteOptions& o, const Slice& k, const Slice& v) {
    return DB::Put(o, k, v);
  }
  virtual Status Delete(const WriteOptions& o, const Slice& key) {
    return DB::Delete(o, key);
  }
  virtual Status Get(const ReadOptions& options,
                     const Slice& key, std::string* value) {
assert(false);      //未实施
    return Status::NotFound(key);
  }
  virtual Iterator* NewIterator(const ReadOptions& options) {
    if (options.snapshot == NULL) {
      KVMap* saved = new KVMap;
      *saved = map_;
      return new ModelIter(saved, true);
    } else {
      const KVMap* snapshot_state =
          &(reinterpret_cast<const ModelSnapshot*>(options.snapshot)->map_);
      return new ModelIter(snapshot_state, false);
    }
  }
  virtual const Snapshot* GetSnapshot() {
    ModelSnapshot* snapshot = new ModelSnapshot;
    snapshot->map_ = map_;
    return snapshot;
  }

  virtual void ReleaseSnapshot(const Snapshot* snapshot) {
    delete reinterpret_cast<const ModelSnapshot*>(snapshot);
  }
  virtual Status Write(const WriteOptions& options, WriteBatch* batch) {
    class Handler : public WriteBatch::Handler {
     public:
      KVMap* map_;
      virtual void Put(const Slice& key, const Slice& value) {
        (*map_)[key.ToString()] = value.ToString();
      }
      virtual void Delete(const Slice& key) {
        map_->erase(key.ToString());
      }
    };
    Handler handler;
    handler.map_ = &map_;
    return batch->Iterate(&handler);
  }

  virtual bool GetProperty(const Slice& property, std::string* value) {
    return false;
  }
  virtual void GetApproximateSizes(const Range* r, int n, uint64_t* sizes) {
    for (int i = 0; i < n; i++) {
      sizes[i] = 0;
    }
  }
  virtual void CompactRange(const Slice* start, const Slice* end) {
  }

 private:
  class ModelIter: public Iterator {
   public:
    ModelIter(const KVMap* map, bool owned)
        : map_(map), owned_(owned), iter_(map_->end()) {
    }
    ~ModelIter() {
      if (owned_) delete map_;
    }
    virtual bool Valid() const { return iter_ != map_->end(); }
    virtual void SeekToFirst() { iter_ = map_->begin(); }
    virtual void SeekToLast() {
      if (map_->empty()) {
        iter_ = map_->end();
      } else {
        iter_ = map_->find(map_->rbegin()->first);
      }
    }
    virtual void Seek(const Slice& k) {
      iter_ = map_->lower_bound(k.ToString());
    }
    virtual void Next() { ++iter_; }
    virtual void Prev() { --iter_; }
    virtual Slice key() const { return iter_->first; }
    virtual Slice value() const { return iter_->second; }
    virtual Status status() const { return Status::OK(); }
   private:
    const KVMap* const map_;
const bool owned_;  //我们拥有地图吗？
    KVMap::const_iterator iter_;
  };
  const Options options_;
  KVMap map_;
};

static std::string RandomKey(Random* rnd) {
  int len = (rnd->OneIn(3)
? 1                //有时短以鼓励碰撞
             : (rnd->OneIn(100) ? rnd->Skewed(10) : rnd->Uniform(10)));
  return test::RandomKey(rnd, len);
}

static bool CompareIterators(int step,
                             DB* model,
                             DB* db,
                             const Snapshot* model_snap,
                             const Snapshot* db_snap) {
  ReadOptions options;
  options.snapshot = model_snap;
  Iterator* miter = model->NewIterator(options);
  options.snapshot = db_snap;
  Iterator* dbiter = db->NewIterator(options);
  bool ok = true;
  int count = 0;
  for (miter->SeekToFirst(), dbiter->SeekToFirst();
       ok && miter->Valid() && dbiter->Valid();
       miter->Next(), dbiter->Next()) {
    count++;
    if (miter->key().compare(dbiter->key()) != 0) {
      fprintf(stderr, "step %d: Key mismatch: '%s' vs. '%s'\n",
              step,
              EscapeString(miter->key()).c_str(),
              EscapeString(dbiter->key()).c_str());
      ok = false;
      break;
    }

    if (miter->value().compare(dbiter->value()) != 0) {
      fprintf(stderr, "step %d: Value mismatch for key '%s': '%s' vs. '%s'\n",
              step,
              EscapeString(miter->key()).c_str(),
              EscapeString(miter->value()).c_str(),
              EscapeString(miter->value()).c_str());
      ok = false;
    }
  }

  if (ok) {
    if (miter->Valid() != dbiter->Valid()) {
      fprintf(stderr, "step %d: Mismatch at end of iterators: %d vs. %d\n",
              step, miter->Valid(), dbiter->Valid());
      ok = false;
    }
  }
  fprintf(stderr, "%d entries compared: ok=%d\n", count, ok);
  delete miter;
  delete dbiter;
  return ok;
}

TEST(DBTest, Randomized) {
  Random rnd(test::RandomSeed());
  do {
    ModelDB model(CurrentOptions());
    const int N = 10000;
    const Snapshot* model_snap = NULL;
    const Snapshot* db_snap = NULL;
    std::string k, v;
    for (int step = 0; step < N; step++) {
      if (step % 100 == 0) {
        fprintf(stderr, "Step %d of %d\n", step, N);
      }
//todo（sanjay）：test get（）工作
      int p = rnd.Uniform(100);
if (p < 45) {                               //放
        k = RandomKey(&rnd);
        v = RandomString(&rnd,
                         rnd.OneIn(20)
                         ? 100 + rnd.Uniform(100)
                         : rnd.Uniform(8));
        ASSERT_OK(model.Put(WriteOptions(), k, v));
        ASSERT_OK(db_->Put(WriteOptions(), k, v));

} else if (p < 90) {                        //删除
        k = RandomKey(&rnd);
        ASSERT_OK(model.Delete(WriteOptions(), k));
        ASSERT_OK(db_->Delete(WriteOptions(), k));


} else {                                    //多元素批次
        WriteBatch b;
        const int num = rnd.Uniform(8);
        for (int i = 0; i < num; i++) {
          if (i == 0 || !rnd.OneIn(10)) {
            k = RandomKey(&rnd);
          } else {
//定期重复使用上一个ITER的相同密钥，因此
//在同一个密钥的写入批处理中有多个条目
          }
          if (rnd.OneIn(2)) {
            v = RandomString(&rnd, rnd.Uniform(10));
            b.Put(k, v);
          } else {
            b.Delete(k);
          }
        }
        ASSERT_OK(model.Write(WriteOptions(), &b));
        ASSERT_OK(db_->Write(WriteOptions(), &b));
      }

      if ((step % 100) == 0) {
        ASSERT_TRUE(CompareIterators(step, &model, db_, NULL, NULL));
        ASSERT_TRUE(CompareIterators(step, &model, db_, model_snap, db_snap));
//这次从每个数据库中保存一个快照，我们将使用下一个
//我们比较事物的时间，以确保当前状态是
//与快照一起保留
        if (model_snap != NULL) model.ReleaseSnapshot(model_snap);
        if (db_snap != NULL) db_->ReleaseSnapshot(db_snap);

        Reopen();
        ASSERT_TRUE(CompareIterators(step, &model, db_, NULL, NULL));

        model_snap = model.GetSnapshot();
        db_snap = db_->GetSnapshot();
      }
    }
    if (model_snap != NULL) model.ReleaseSnapshot(model_snap);
    if (db_snap != NULL) db_->ReleaseSnapshot(db_snap);
  } while (ChangeOptions());
}

std::string MakeKey(unsigned int num) {
  char buf[30];
  snprintf(buf, sizeof(buf), "%016u", num);
  return std::string(buf);
}

void BM_LogAndApply(int iters, int num_base_files) {
  std::string dbname = test::TmpDir() + "/leveldb_test_benchmark";
  DestroyDB(dbname, Options());

  DB* db = NULL;
  Options opts;
  opts.create_if_missing = true;
  Status s = DB::Open(opts, dbname, &db);
  ASSERT_OK(s);
  ASSERT_TRUE(db != NULL);

  delete db;
  db = NULL;

  Env* env = Env::Default();

  port::Mutex mu;
  MutexLock l(&mu);

  InternalKeyComparator cmp(BytewiseComparator());
  Options options;
  VersionSet vset(dbname, &options, NULL, &cmp);
  bool save_manifest;
  ASSERT_OK(vset.Recover(&save_manifest));
  VersionEdit vbase;
  uint64_t fnum = 1;
  for (int i = 0; i < num_base_files; i++) {
    InternalKey start(MakeKey(2*fnum), 1, kTypeValue);
    InternalKey limit(MakeKey(2*fnum+1), 1, kTypeDeletion);
    /*se.addfile（2，fnum++，1/*文件大小*/，开始，限制）；
  }
  断言“确定”（vset.logandapply（&vbase，&mu））；

  uint64_t start_micros=env->nowmicros（）；

  对于（int i=0；i<iters；i++）
    版本编辑视频；
    删除文件（2，fnum）；
    internalkey start（makekey（2*fnum），1，ktypeValue）；
    内部密钥限制（makekey（2*fnum+1），1，k类型删除）；
    vedit.addfile（2，fnum++，1/*文件大小*/, start, limit);

    vset.LogAndApply(&vedit, &mu);
  }
  uint64_t stop_micros = env->NowMicros();
  unsigned int us = stop_micros - start_micros;
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", num_base_files);
  fprintf(stderr,
          "BM_LogAndApply/%-6s   %8d iters : %9u us (%7.0f us / iter)\n",
          buf, iters, us, ((float)us) / iters);
}

}  //命名空间级别数据库

int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "--benchmark") {
    leveldb::BM_LogAndApply(1000, 1);
    leveldb::BM_LogAndApply(1000, 100);
    leveldb::BM_LogAndApply(1000, 10000);
    leveldb::BM_LogAndApply(100, 100000);
    return 0;
  }

  return leveldb::test::RunAllTests();
}
