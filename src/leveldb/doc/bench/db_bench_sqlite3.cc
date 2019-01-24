
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

#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include "util/histogram.h"
#include "util/random.h"
#include "util/testutil.h"

//以逗号分隔的按指定顺序运行的操作列表
//实际基准：
//
//fillseq——异步模式下按顺序键顺序写入n个值
//fillseqsync—在同步模式下按顺序键顺序写入n/100个值
//fillseqbatch—异步模式下按顺序键顺序批写入n个值
//fillRandom——异步模式下按随机键顺序写入n个值
//fillRandSync--在同步模式下按随机键顺序写入n/100个值
//fillRandbatch—异步模式下按顺序键顺序批写入n个值
//overwrite——异步模式下按随机键顺序覆盖n个值
//fillRand100k——在异步模式下按随机顺序写入n/1000 100k值
//fillseq100k——异步模式下按顺序写入n/1000 100k值
//readseq—按顺序读取n次
//read random——随机读取n次
//readrand100k——异步模式下按顺序读取n/1000 100k值
static const char* FLAGS_benchmarks =
    "fillseq,"
    "fillseqsync,"
    "fillseqbatch,"
    "fillrandom,"
    "fillrandsync,"
    "fillrandbatch,"
    "overwrite,"
    "overwritebatch,"
    "readrandom,"
    "readseq,"
    "fillrand100K,"
    "fillseq100K,"
    "readseq,"
    "readrand100K,"
    ;

//要放入数据库的键/值的数目
static int FLAGS_num = 1000000;

//要执行的读取操作数。如果为负，则执行标记“num reads”。
static int FLAGS_reads = -1;

//每个值的大小
static int FLAGS_value_size = 100;

//打印操作时间的柱状图
static bool FLAGS_histogram = false;

//排列以生成收缩到此分数的值
//压缩后的原始尺寸
static double FLAGS_compression_ratio = 0.5;

//页面大小。默认值1 KB。
static int FLAGS_page_size = 1024;

//页数。
//默认缓存大小=标志页大小*标志页数=4 MB。
static int FLAGS_num_pages = 4096;

//如果为true，则不要销毁现有数据库。如果你设置这个
//标记并指定需要新数据库的基准，
//基准测试将失败。
static bool FLAGS_use_existing_db = false;

//如果为真，则允许进行批写入
static bool FLAGS_transaction = true;

//如果为真，则启用提前写入日志记录
static bool FLAGS_WAL_enabled = true;

//使用具有以下名称的数据库。
static const char* FLAGS_db = NULL;

inline
static void ExecErrorCheck(int status, char *err_msg) {
  if (status != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    exit(1);
  }
}

inline
static void StepErrorCheck(int status) {
  if (status != SQLITE_DONE) {
    fprintf(stderr, "SQL step error: status = %d\n", status);
    exit(1);
  }
}

inline
static void ErrorCheck(int status) {
  if (status != SQLITE_OK) {
    fprintf(stderr, "sqlite3 error: status = %d\n", status);
    exit(1);
  }
}

inline
static void WalCheckpoint(sqlite3* db_) {
//刷新对磁盘的所有写入
  if (FLAGS_WAL_enabled) {
    sqlite3_wal_checkpoint_v2(db_, NULL, SQLITE_CHECKPOINT_FULL, NULL, NULL);
  }
}

namespace leveldb {

//帮助快速生成随机数据。
namespace {
class RandomGenerator {
 private:
  std::string data_;
  int pos_;

 public:
  RandomGenerator() {
//我们反复使用有限数量的数据，并确保
//它大于压缩窗口（32KB），并且
//足够大以满足我们想要写入的所有典型值大小。
    Random rnd(301);
    std::string piece;
    while (data_.size() < 1048576) {
//添加可按指定压缩的短片段
//通过标记压缩比。
      test::CompressibleString(&rnd, FLAGS_compression_ratio, 100, &piece);
      data_.append(piece);
    }
    pos_ = 0;
  }

  Slice Generate(int len) {
    if (pos_ + len > data_.size()) {
      pos_ = 0;
      assert(len < data_.size());
    }
    pos_ += len;
    return Slice(data_.data() + pos_ - len, len);
  }
};

static Slice TrimSpace(Slice s) {
  int start = 0;
  while (start < s.size() && isspace(s[start])) {
    start++;
  }
  int limit = s.size();
  while (limit > start && isspace(s[limit-1])) {
    limit--;
  }
  return Slice(s.data() + start, limit - start);
}

}  //命名空间

class Benchmark {
 private:
  sqlite3* db_;
  int db_num_;
  int num_;
  int reads_;
  double start_;
  double last_op_finish_;
  int64_t bytes_;
  std::string message_;
  Histogram hist_;
  RandomGenerator gen_;
  Random rand_;

//为进度消息保留的状态
  int done_;
int next_report_;     //下一个报告时间

  void PrintHeader() {
    const int kKeySize = 16;
    PrintEnvironment();
    fprintf(stdout, "Keys:       %d bytes each\n", kKeySize);
    fprintf(stdout, "Values:     %d bytes each\n", FLAGS_value_size);
    fprintf(stdout, "Entries:    %d\n", num_);
    fprintf(stdout, "RawSize:    %.1f MB (estimated)\n",
            ((static_cast<int64_t>(kKeySize + FLAGS_value_size) * num_)
             / 1048576.0));
    PrintWarnings();
    fprintf(stdout, "------------------------------------------------\n");
  }

  void PrintWarnings() {
#if defined(__GNUC__) && !defined(__OPTIMIZE__)
    fprintf(stdout,
            "WARNING: Optimization is disabled: benchmarks unnecessarily slow\n"
            );
#endif
#ifndef NDEBUG
    fprintf(stdout,
            "WARNING: Assertions are enabled; benchmarks unnecessarily slow\n");
#endif
  }

  void PrintEnvironment() {
    fprintf(stderr, "SQLite:     version %s\n", SQLITE_VERSION);

#if defined(__linux)
    time_t now = time(NULL);
fprintf(stderr, "Date:       %s", ctime(&now));  //ctime（）添加换行符

    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo != NULL) {
      char line[1000];
      int num_cpus = 0;
      std::string cpu_type;
      std::string cache_size;
      while (fgets(line, sizeof(line), cpuinfo) != NULL) {
        const char* sep = strchr(line, ':');
        if (sep == NULL) {
          continue;
        }
        Slice key = TrimSpace(Slice(line, sep - 1 - line));
        Slice val = TrimSpace(Slice(sep + 1));
        if (key == "model name") {
          ++num_cpus;
          cpu_type = val.ToString();
        } else if (key == "cache size") {
          cache_size = val.ToString();
        }
      }
      fclose(cpuinfo);
      fprintf(stderr, "CPU:        %d * %s\n", num_cpus, cpu_type.c_str());
      fprintf(stderr, "CPUCache:   %s\n", cache_size.c_str());
    }
#endif
  }

  void Start() {
    start_ = Env::Default()->NowMicros() * 1e-6;
    bytes_ = 0;
    message_.clear();
    last_op_finish_ = start_;
    hist_.Clear();
    done_ = 0;
    next_report_ = 100;
  }

  void FinishedSingleOp() {
    if (FLAGS_histogram) {
      double now = Env::Default()->NowMicros() * 1e-6;
      double micros = (now - last_op_finish_) * 1e6;
      hist_.Add(micros);
      if (micros > 20000) {
        fprintf(stderr, "long op: %.1f micros%30s\r", micros, "");
        fflush(stderr);
      }
      last_op_finish_ = now;
    }

    done_++;
    if (done_ >= next_report_) {
      if      (next_report_ < 1000)   next_report_ += 100;
      else if (next_report_ < 5000)   next_report_ += 500;
      else if (next_report_ < 10000)  next_report_ += 1000;
      else if (next_report_ < 50000)  next_report_ += 5000;
      else if (next_report_ < 100000) next_report_ += 10000;
      else if (next_report_ < 500000) next_report_ += 50000;
      else                            next_report_ += 100000;
      fprintf(stderr, "... finished %d ops%30s\r", done_, "");
      fflush(stderr);
    }
  }

  void Stop(const Slice& name) {
    double finish = Env::Default()->NowMicros() * 1e-6;

//假设至少进行了一次操作，以防我们正在运行基准测试
//不调用FinishedSingleOp（）。
    if (done_ < 1) done_ = 1;

    if (bytes_ > 0) {
      char rate[100];
      snprintf(rate, sizeof(rate), "%6.1f MB/s",
               (bytes_ / 1048576.0) / (finish - start_));
      if (!message_.empty()) {
        message_  = std::string(rate) + " " + message_;
      } else {
        message_ = rate;
      }
    }

    fprintf(stdout, "%-12s : %11.3f micros/op;%s%s\n",
            name.ToString().c_str(),
            (finish - start_) * 1e6 / done_,
            (message_.empty() ? "" : " "),
            message_.c_str());
    if (FLAGS_histogram) {
      fprintf(stdout, "Microseconds per op:\n%s\n", hist_.ToString().c_str());
    }
    fflush(stdout);
  }

 public:
  enum Order {
    SEQUENTIAL,
    RANDOM
  };
  enum DBState {
    FRESH,
    EXISTING
  };

  Benchmark()
  : db_(NULL),
    db_num_(0),
    num_(FLAGS_num),
    reads_(FLAGS_reads < 0 ? FLAGS_num : FLAGS_reads),
    bytes_(0),
    rand_(301) {
    std::vector<std::string> files;
    std::string test_dir;
    Env::Default()->GetTestDirectory(&test_dir);
    Env::Default()->GetChildren(test_dir, &files);
    if (!FLAGS_use_existing_db) {
      for (int i = 0; i < files.size(); i++) {
        if (Slice(files[i]).starts_with("dbbench_sqlite3")) {
          std::string file_name(test_dir);
          file_name += "/";
          file_name += files[i];
          Env::Default()->DeleteFile(file_name.c_str());
        }
      }
    }
  }

  ~Benchmark() {
    int status = sqlite3_close(db_);
    ErrorCheck(status);
  }

  void Run() {
    PrintHeader();
    Open();

    const char* benchmarks = FLAGS_benchmarks;
    while (benchmarks != NULL) {
      const char* sep = strchr(benchmarks, ',');
      Slice name;
      if (sep == NULL) {
        name = benchmarks;
        benchmarks = NULL;
      } else {
        name = Slice(benchmarks, sep - benchmarks);
        benchmarks = sep + 1;
      }

      bytes_ = 0;
      Start();

      bool known = true;
      bool write_sync = false;
      if (name == Slice("fillseq")) {
        Write(write_sync, SEQUENTIAL, FRESH, num_, FLAGS_value_size, 1);
        WalCheckpoint(db_);
      } else if (name == Slice("fillseqbatch")) {
        Write(write_sync, SEQUENTIAL, FRESH, num_, FLAGS_value_size, 1000);
        WalCheckpoint(db_);
      } else if (name == Slice("fillrandom")) {
        Write(write_sync, RANDOM, FRESH, num_, FLAGS_value_size, 1);
        WalCheckpoint(db_);
      } else if (name == Slice("fillrandbatch")) {
        Write(write_sync, RANDOM, FRESH, num_, FLAGS_value_size, 1000);
        WalCheckpoint(db_);
      } else if (name == Slice("overwrite")) {
        Write(write_sync, RANDOM, EXISTING, num_, FLAGS_value_size, 1);
        WalCheckpoint(db_);
      } else if (name == Slice("overwritebatch")) {
        Write(write_sync, RANDOM, EXISTING, num_, FLAGS_value_size, 1000);
        WalCheckpoint(db_);
      } else if (name == Slice("fillrandsync")) {
        write_sync = true;
        Write(write_sync, RANDOM, FRESH, num_ / 100, FLAGS_value_size, 1);
        WalCheckpoint(db_);
      } else if (name == Slice("fillseqsync")) {
        write_sync = true;
        Write(write_sync, SEQUENTIAL, FRESH, num_ / 100, FLAGS_value_size, 1);
        WalCheckpoint(db_);
      } else if (name == Slice("fillrand100K")) {
        Write(write_sync, RANDOM, FRESH, num_ / 1000, 100 * 1000, 1);
        WalCheckpoint(db_);
      } else if (name == Slice("fillseq100K")) {
        Write(write_sync, SEQUENTIAL, FRESH, num_ / 1000, 100 * 1000, 1);
        WalCheckpoint(db_);
      } else if (name == Slice("readseq")) {
        ReadSequential();
      } else if (name == Slice("readrandom")) {
        Read(RANDOM, 1);
      } else if (name == Slice("readrand100K")) {
        int n = reads_;
        reads_ /= 1000;
        Read(RANDOM, 1);
        reads_ = n;
      } else {
        known = false;
if (name != Slice()) {  //空名称没有错误消息
          fprintf(stderr, "unknown benchmark '%s'\n", name.ToString().c_str());
        }
      }
      if (known) {
        Stop(name);
      }
    }
  }

  void Open() {
    assert(db_ == NULL);

    int status;
    char file_name[100];
    char* err_msg = NULL;
    db_num_++;

//开放式数据库
    std::string tmp_dir;
    Env::Default()->GetTestDirectory(&tmp_dir);
    snprintf(file_name, sizeof(file_name),
             "%s/dbbench_sqlite3-%d.db",
             tmp_dir.c_str(),
             db_num_);
    status = sqlite3_open(file_name, &db_);
    if (status) {
      fprintf(stderr, "open error: %s\n", sqlite3_errmsg(db_));
      exit(1);
    }

//更改sqlite缓存大小
    char cache_size[100];
    snprintf(cache_size, sizeof(cache_size), "PRAGMA cache_size = %d",
             FLAGS_num_pages);
    status = sqlite3_exec(db_, cache_size, NULL, NULL, &err_msg);
    ExecErrorCheck(status, err_msg);

//标志页大小默认为1024
    if (FLAGS_page_size != 1024) {
      char page_size[100];
      snprintf(page_size, sizeof(page_size), "PRAGMA page_size = %d",
               FLAGS_page_size);
      status = sqlite3_exec(db_, page_size, NULL, NULL, &err_msg);
      ExecErrorCheck(status, err_msg);
    }

//如果启用了Wal标志，则将日记模式更改为Wal
    if (FLAGS_WAL_enabled) {
      std::string WAL_stmt = "PRAGMA journal_mode = WAL";

//leveldb的默认缓存大小是4 MB的组合
      std::string WAL_checkpoint = "PRAGMA wal_autocheckpoint = 4096";
      status = sqlite3_exec(db_, WAL_stmt.c_str(), NULL, NULL, &err_msg);
      ExecErrorCheck(status, err_msg);
      status = sqlite3_exec(db_, WAL_checkpoint.c_str(), NULL, NULL, &err_msg);
      ExecErrorCheck(status, err_msg);
    }

//将锁定模式更改为独占并为数据库创建表/索引
    std::string locking_stmt = "PRAGMA locking_mode = EXCLUSIVE";
    std::string create_stmt =
          "CREATE TABLE test (key blob, value blob, PRIMARY KEY(key))";
    std::string stmt_array[] = { locking_stmt, create_stmt };
    int stmt_array_length = sizeof(stmt_array) / sizeof(std::string);
    for (int i = 0; i < stmt_array_length; i++) {
      status = sqlite3_exec(db_, stmt_array[i].c_str(), NULL, NULL, &err_msg);
      ExecErrorCheck(status, err_msg);
    }
  }

  void Write(bool write_sync, Order order, DBState state,
             int num_entries, int value_size, int entries_per_batch) {
//如果state==fresh，则创建新数据库
    if (state == FRESH) {
      if (FLAGS_use_existing_db) {
        message_ = "skipping (--use_existing_db is true)";
        return;
      }
      sqlite3_close(db_);
      db_ = NULL;
      Open();
      Start();
    }

    if (num_entries != num_) {
      char msg[100];
      snprintf(msg, sizeof(msg), "(%d ops)", num_entries);
      message_ = msg;
    }

    char* err_msg = NULL;
    int status;

    sqlite3_stmt *replace_stmt, *begin_trans_stmt, *end_trans_stmt;
    std::string replace_str = "REPLACE INTO test (key, value) VALUES (?, ?)";
    std::string begin_trans_str = "BEGIN TRANSACTION;";
    std::string end_trans_str = "END TRANSACTION;";

//检查选项中的同步标志
    std::string sync_stmt = (write_sync) ? "PRAGMA synchronous = FULL" :
                                           "PRAGMA synchronous = OFF";
    status = sqlite3_exec(db_, sync_stmt.c_str(), NULL, NULL, &err_msg);
    ExecErrorCheck(status, err_msg);

//准备sqlite3语句
    status = sqlite3_prepare_v2(db_, replace_str.c_str(), -1,
                                &replace_stmt, NULL);
    ErrorCheck(status);
    status = sqlite3_prepare_v2(db_, begin_trans_str.c_str(), -1,
                                &begin_trans_stmt, NULL);
    ErrorCheck(status);
    status = sqlite3_prepare_v2(db_, end_trans_str.c_str(), -1,
                                &end_trans_stmt, NULL);
    ErrorCheck(status);

    bool transaction = (entries_per_batch > 1);
    for (int i = 0; i < num_entries; i += entries_per_batch) {
//开始写入事务
      if (FLAGS_transaction && transaction) {
        status = sqlite3_step(begin_trans_stmt);
        StepErrorCheck(status);
        status = sqlite3_reset(begin_trans_stmt);
        ErrorCheck(status);
      }

//创建和执行SQL语句
      for (int j = 0; j < entries_per_batch; j++) {
        const char* value = gen_.Generate(value_size).data();

//为键值对创建值
        const int k = (order == SEQUENTIAL) ? i + j :
                      (rand_.Next() % num_entries);
        char key[100];
        snprintf(key, sizeof(key), "%016d", k);

//将kv值绑定到replace语句中
        status = sqlite3_bind_blob(replace_stmt, 1, key, 16, SQLITE_STATIC);
        ErrorCheck(status);
        status = sqlite3_bind_blob(replace_stmt, 2, value,
                                   value_size, SQLITE_STATIC);
        ErrorCheck(status);

//执行替换语句
        bytes_ += value_size + strlen(key);
        status = sqlite3_step(replace_stmt);
        StepErrorCheck(status);

//重置sqlite语句以供其他使用
        status = sqlite3_clear_bindings(replace_stmt);
        ErrorCheck(status);
        status = sqlite3_reset(replace_stmt);
        ErrorCheck(status);

        FinishedSingleOp();
      }

//结束写入事务
      if (FLAGS_transaction && transaction) {
        status = sqlite3_step(end_trans_stmt);
        StepErrorCheck(status);
        status = sqlite3_reset(end_trans_stmt);
        ErrorCheck(status);
      }
    }

    status = sqlite3_finalize(replace_stmt);
    ErrorCheck(status);
    status = sqlite3_finalize(begin_trans_stmt);
    ErrorCheck(status);
    status = sqlite3_finalize(end_trans_stmt);
    ErrorCheck(status);
  }

  void Read(Order order, int entries_per_batch) {
    int status;
    sqlite3_stmt *read_stmt, *begin_trans_stmt, *end_trans_stmt;

    std::string read_str = "SELECT * FROM test WHERE key = ?";
    std::string begin_trans_str = "BEGIN TRANSACTION;";
    std::string end_trans_str = "END TRANSACTION;";

//准备sqlite3语句
    status = sqlite3_prepare_v2(db_, begin_trans_str.c_str(), -1,
                                &begin_trans_stmt, NULL);
    ErrorCheck(status);
    status = sqlite3_prepare_v2(db_, end_trans_str.c_str(), -1,
                                &end_trans_stmt, NULL);
    ErrorCheck(status);
    status = sqlite3_prepare_v2(db_, read_str.c_str(), -1, &read_stmt, NULL);
    ErrorCheck(status);

    bool transaction = (entries_per_batch > 1);
    for (int i = 0; i < reads_; i += entries_per_batch) {
//开始读取事务
      if (FLAGS_transaction && transaction) {
        status = sqlite3_step(begin_trans_stmt);
        StepErrorCheck(status);
        status = sqlite3_reset(begin_trans_stmt);
        ErrorCheck(status);
      }

//创建和执行SQL语句
      for (int j = 0; j < entries_per_batch; j++) {
//创建键值
        char key[100];
        int k = (order == SEQUENTIAL) ? i + j : (rand_.Next() % reads_);
        snprintf(key, sizeof(key), "%016d", k);

//将键值绑定到read语句中
        status = sqlite3_bind_blob(read_stmt, 1, key, 16, SQLITE_STATIC);
        ErrorCheck(status);

//执行读取语句
        while ((status = sqlite3_step(read_stmt)) == SQLITE_ROW) {}
        StepErrorCheck(status);

//重置sqlite语句以供其他使用
        status = sqlite3_clear_bindings(read_stmt);
        ErrorCheck(status);
        status = sqlite3_reset(read_stmt);
        ErrorCheck(status);
        FinishedSingleOp();
      }

//结束读取事务
      if (FLAGS_transaction && transaction) {
        status = sqlite3_step(end_trans_stmt);
        StepErrorCheck(status);
        status = sqlite3_reset(end_trans_stmt);
        ErrorCheck(status);
      }
    }

    status = sqlite3_finalize(read_stmt);
    ErrorCheck(status);
    status = sqlite3_finalize(begin_trans_stmt);
    ErrorCheck(status);
    status = sqlite3_finalize(end_trans_stmt);
    ErrorCheck(status);
  }

  void ReadSequential() {
    int status;
    sqlite3_stmt *pStmt;
    std::string read_str = "SELECT * FROM test ORDER BY key";

    status = sqlite3_prepare_v2(db_, read_str.c_str(), -1, &pStmt, NULL);
    ErrorCheck(status);
    for (int i = 0; i < reads_ && SQLITE_ROW == sqlite3_step(pStmt); i++) {
      bytes_ += sqlite3_column_bytes(pStmt, 1) + sqlite3_column_bytes(pStmt, 2);
      FinishedSingleOp();
    }

    status = sqlite3_finalize(pStmt);
    ErrorCheck(status);
  }

};

}  //命名空间级别数据库

int main(int argc, char** argv) {
  std::string default_db_path;
  for (int i = 1; i < argc; i++) {
    double d;
    int n;
    char junk;
    if (leveldb::Slice(argv[i]).starts_with("--benchmarks=")) {
      FLAGS_benchmarks = argv[i] + strlen("--benchmarks=");
    } else if (sscanf(argv[i], "--histogram=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      FLAGS_histogram = n;
    } else if (sscanf(argv[i], "--compression_ratio=%lf%c", &d, &junk) == 1) {
      FLAGS_compression_ratio = d;
    } else if (sscanf(argv[i], "--use_existing_db=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      FLAGS_use_existing_db = n;
    } else if (sscanf(argv[i], "--num=%d%c", &n, &junk) == 1) {
      FLAGS_num = n;
    } else if (sscanf(argv[i], "--reads=%d%c", &n, &junk) == 1) {
      FLAGS_reads = n;
    } else if (sscanf(argv[i], "--value_size=%d%c", &n, &junk) == 1) {
      FLAGS_value_size = n;
    } else if (leveldb::Slice(argv[i]) == leveldb::Slice("--no_transaction")) {
      FLAGS_transaction = false;
    } else if (sscanf(argv[i], "--page_size=%d%c", &n, &junk) == 1) {
      FLAGS_page_size = n;
    } else if (sscanf(argv[i], "--num_pages=%d%c", &n, &junk) == 1) {
      FLAGS_num_pages = n;
    } else if (sscanf(argv[i], "--WAL_enabled=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      FLAGS_WAL_enabled = n;
    } else if (strncmp(argv[i], "--db=", 5) == 0) {
      FLAGS_db = argv[i] + 5;
    } else {
      fprintf(stderr, "Invalid flag '%s'\n", argv[i]);
      exit(1);
    }
  }

//选择测试数据库的位置，如果没有给定--db=<path>
  if (FLAGS_db == NULL) {
      leveldb::Env::Default()->GetTestDirectory(&default_db_path);
      default_db_path += "/dbbench";
      FLAGS_db = default_db_path.c_str();
  }

  leveldb::Benchmark benchmark;
  benchmark.Run();
  return 0;
}
