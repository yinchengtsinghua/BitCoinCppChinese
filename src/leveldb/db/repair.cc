
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
//
//我们从找到的其他文件中恢复描述符的内容。
//（1）首先将所有日志文件转换为表
//（2）我们扫描每个表来计算
//（a）桌子最小/最大
//（b）表中最大的序列号
//（3）生成描述符内容：
//-日志号设置为零
//-下一个文件号设置为1+我们找到的最大文件号
//-最后一个序列号被设置为跨
//所有表格（见2c）
//-清除压缩指针
//-在0级添加每个表文件
//
//可能的优化1：
//（a）计算总尺寸并用于选择适当的最大M级
//（b）按表中的最大顺序对表进行排序
//（c）对于每个表：如果它与前面的表重叠，则放在0级，
//否则放在M层。
//可能的优化2：
//按表存储元数据（最小、最大、最大顺序…）
//在表的元部分加速扫描表。

#include "db/builder.h"
#include "db/db_impl.h"
#include "db/dbformat.h"
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/table_cache.h"
#include "db/version_edit.h"
#include "db/write_batch_internal.h"
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/env.h"

namespace leveldb {

namespace {

class Repairer {
 public:
  Repairer(const std::string& dbname, const Options& options)
      : dbname_(dbname),
        env_(options.env),
        icmp_(options.comparator),
        ipolicy_(options.filter_policy),
        options_(SanitizeOptions(dbname, &icmp_, &ipolicy_, options)),
        owns_info_log_(options_.info_log != options.info_log),
        owns_cache_(options_.block_cache != options.block_cache),
        next_file_number_(1) {
//TableCache可以很小，因为我们希望每个表都打开一次。
    table_cache_ = new TableCache(dbname_, &options_, 10);
  }

  ~Repairer() {
    delete table_cache_;
    if (owns_info_log_) {
      delete options_.info_log;
    }
    if (owns_cache_) {
      delete options_.block_cache;
    }
  }

  Status Run() {
    Status status = FindFiles();
    if (status.ok()) {
      ConvertLogFilesToTables();
      ExtractMetaData();
      status = WriteDescriptor();
    }
    if (status.ok()) {
      unsigned long long bytes = 0;
      for (size_t i = 0; i < tables_.size(); i++) {
        bytes += tables_[i].meta.file_size;
      }
      Log(options_.info_log,
          "**** Repaired leveldb %s; "
          "recovered %d files; %llu bytes. "
          "Some data may have been lost. "
          "****",
          dbname_.c_str(),
          static_cast<int>(tables_.size()),
          bytes);
    }
    return status;
  }

 private:
  struct TableInfo {
    FileMetaData meta;
    SequenceNumber max_sequence;
  };

  std::string const dbname_;
  Env* const env_;
  InternalKeyComparator const icmp_;
  InternalFilterPolicy const ipolicy_;
  Options const options_;
  bool owns_info_log_;
  bool owns_cache_;
  TableCache* table_cache_;
  VersionEdit edit_;

  std::vector<std::string> manifests_;
  std::vector<uint64_t> table_numbers_;
  std::vector<uint64_t> logs_;
  std::vector<TableInfo> tables_;
  uint64_t next_file_number_;

  Status FindFiles() {
    std::vector<std::string> filenames;
    Status status = env_->GetChildren(dbname_, &filenames);
    if (!status.ok()) {
      return status;
    }
    if (filenames.empty()) {
      return Status::IOError(dbname_, "repair found no files");
    }

    uint64_t number;
    FileType type;
    for (size_t i = 0; i < filenames.size(); i++) {
      if (ParseFileName(filenames[i], &number, &type)) {
        if (type == kDescriptorFile) {
          manifests_.push_back(filenames[i]);
        } else {
          if (number + 1 > next_file_number_) {
            next_file_number_ = number + 1;
          }
          if (type == kLogFile) {
            logs_.push_back(number);
          } else if (type == kTableFile) {
            table_numbers_.push_back(number);
          } else {
//忽略其他文件
          }
        }
      }
    }
    return status;
  }

  void ConvertLogFilesToTables() {
    for (size_t i = 0; i < logs_.size(); i++) {
      std::string logname = LogFileName(dbname_, logs_[i]);
      Status status = ConvertLogToTable(logs_[i]);
      if (!status.ok()) {
        Log(options_.info_log, "Log #%llu: ignoring conversion error: %s",
            (unsigned long long) logs_[i],
            status.ToString().c_str());
      }
      ArchiveFile(logname);
    }
  }

  Status ConvertLogToTable(uint64_t log) {
    struct LogReporter : public log::Reader::Reporter {
      Env* env;
      Logger* info_log;
      uint64_t lognum;
      virtual void Corruption(size_t bytes, const Status& s) {
//我们打印错误消息以防损坏，但继续修复。
        Log(info_log, "Log #%llu: dropping %d bytes; %s",
            (unsigned long long) lognum,
            static_cast<int>(bytes),
            s.ToString().c_str());
      }
    };

//打开日志文件
    std::string logname = LogFileName(dbname_, log);
    SequentialFile* lfile;
    Status status = env_->NewSequentialFile(logname, &lfile);
    if (!status.ok()) {
      return status;
    }

//创建日志读取器。
    LogReporter reporter;
    reporter.env = env_;
    reporter.info_log = options_.info_log;
    reporter.lognum = log;
//我们故意让日志：：reader进行校验和检查，以便
//腐败导致整个承诺被跳过而不是
//传播不良信息（如过大的序列
//数字）。
    /*：：读卡器读卡器（lfile，&reporter，false/*不校验和*/，
                       0/*初始偏移*/);


//读取所有记录并添加到memtable
    std::string scratch;
    Slice record;
    WriteBatch batch;
    MemTable* mem = new MemTable(icmp_);
    mem->Ref();
    int counter = 0;
    while (reader.ReadRecord(&record, &scratch)) {
      if (record.size() < 12) {
        reporter.Corruption(
            record.size(), Status::Corruption("log record too small", logname));
        continue;
      }
      WriteBatchInternal::SetContents(&batch, record);
      status = WriteBatchInternal::InsertInto(&batch, mem);
      if (status.ok()) {
        counter += WriteBatchInternal::Count(&batch);
      } else {
        Log(options_.info_log, "Log #%llu: ignoring %s",
            (unsigned long long) log,
            status.ToString().c_str());
status = Status::OK();  //继续和其他文件一起
      }
    }
    delete lfile;

//不记录此转换到表的版本编辑
//因为extractMetadata（）也将生成编辑。
    FileMetaData meta;
    meta.number = next_file_number_++;
    Iterator* iter = mem->NewIterator();
    status = BuildTable(dbname_, env_, options_, table_cache_, iter, &meta);
    delete iter;
    mem->Unref();
    mem = NULL;
    if (status.ok()) {
      if (meta.file_size > 0) {
        table_numbers_.push_back(meta.number);
      }
    }
    Log(options_.info_log, "Log #%llu: %d ops saved to Table #%llu %s",
        (unsigned long long) log,
        counter,
        (unsigned long long) meta.number,
        status.ToString().c_str());
    return status;
  }

  void ExtractMetaData() {
    for (size_t i = 0; i < table_numbers_.size(); i++) {
      ScanTable(table_numbers_[i]);
    }
  }

  Iterator* NewTableIterator(const FileMetaData& meta) {
//与压缩迭代器相同：如果启用偏执检查，则
//校验和验证时。
    ReadOptions r;
    r.verify_checksums = options_.paranoid_checks;
    return table_cache_->NewIterator(r, meta.number, meta.file_size);
  }

  void ScanTable(uint64_t number) {
    TableInfo t;
    t.meta.number = number;
    std::string fname = TableFileName(dbname_, number);
    Status status = env_->GetFileSize(fname, &t.meta.file_size);
    if (!status.ok()) {
//尝试其他文件名。
      fname = SSTTableFileName(dbname_, number);
      Status s2 = env_->GetFileSize(fname, &t.meta.file_size);
      if (s2.ok()) {
        status = Status::OK();
      }
    }
    if (!status.ok()) {
      ArchiveFile(TableFileName(dbname_, number));
      ArchiveFile(SSTTableFileName(dbname_, number));
      Log(options_.info_log, "Table #%llu: dropped: %s",
          (unsigned long long) t.meta.number,
          status.ToString().c_str());
      return;
    }

//通过扫描表提取元数据。
    int counter = 0;
    Iterator* iter = NewTableIterator(t.meta);
    bool empty = true;
    ParsedInternalKey parsed;
    t.max_sequence = 0;
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
      Slice key = iter->key();
      if (!ParseInternalKey(key, &parsed)) {
        Log(options_.info_log, "Table #%llu: unparsable key %s",
            (unsigned long long) t.meta.number,
            EscapeString(key).c_str());
        continue;
      }

      counter++;
      if (empty) {
        empty = false;
        t.meta.smallest.DecodeFrom(key);
      }
      t.meta.largest.DecodeFrom(key);
      if (parsed.sequence > t.max_sequence) {
        t.max_sequence = parsed.sequence;
      }
    }
    if (!iter->status().ok()) {
      status = iter->status();
    }
    delete iter;
    Log(options_.info_log, "Table #%llu: %d entries %s",
        (unsigned long long) t.meta.number,
        counter,
        status.ToString().c_str());

    if (status.ok()) {
      tables_.push_back(t);
    } else {
RepairTable(fname, t);  //可修复档案输入文件。
    }
  }

  void RepairTable(const std::string& src, TableInfo t) {
//我们将把SRC内容复制到一个新表中，然后重命名
//源上的新表。

//创建生成器。
    std::string copy = TableFileName(dbname_, next_file_number_++);
    WritableFile* file;
    Status s = env_->NewWritableFile(copy, &file);
    if (!s.ok()) {
      return;
    }
    TableBuilder* builder = new TableBuilder(options_, file);

//复制数据。
    Iterator* iter = NewTableIterator(t.meta);
    int counter = 0;
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
      builder->Add(iter->key(), iter->value());
      counter++;
    }
    delete iter;

    ArchiveFile(src);
    if (counter == 0) {
builder->Abandon();  //无济于事
    } else {
      s = builder->Finish();
      if (s.ok()) {
        t.meta.file_size = builder->FileSize();
      }
    }
    delete builder;
    builder = NULL;

    if (s.ok()) {
      s = file->Close();
    }
    delete file;
    file = NULL;

    if (counter > 0 && s.ok()) {
      std::string orig = TableFileName(dbname_, t.meta.number);
      s = env_->RenameFile(copy, orig);
      if (s.ok()) {
        Log(options_.info_log, "Table #%llu: %d entries repaired",
            (unsigned long long) t.meta.number, counter);
        tables_.push_back(t);
      }
    }
    if (!s.ok()) {
      env_->DeleteFile(copy);
    }
  }

  Status WriteDescriptor() {
    std::string tmp = TempFileName(dbname_, 1);
    WritableFile* file;
    Status status = env_->NewWritableFile(tmp, &file);
    if (!status.ok()) {
      return status;
    }

    SequenceNumber max_sequence = 0;
    for (size_t i = 0; i < tables_.size(); i++) {
      if (max_sequence < tables_[i].max_sequence) {
        max_sequence = tables_[i].max_sequence;
      }
    }

    edit_.SetComparatorName(icmp_.user_comparator()->Name());
    edit_.SetLogNumber(0);
    edit_.SetNextFile(next_file_number_);
    edit_.SetLastSequence(max_sequence);

    for (size_t i = 0; i < tables_.size(); i++) {
//TODO（opt）：分为多个级别
      const TableInfo& t = tables_[i];
      edit_.AddFile(0, t.meta.number, t.meta.file_size,
                    t.meta.smallest, t.meta.largest);
    }

//fprintf（stderr，“newdescriptor:\n%s\n”，edit_.debugString（）.c_str（））；
    {
      log::Writer log(file);
      std::string record;
      edit_.EncodeTo(&record);
      status = log.AddRecord(record);
    }
    if (status.ok()) {
      status = file->Close();
    }
    delete file;
    file = NULL;

    if (!status.ok()) {
      env_->DeleteFile(tmp);
    } else {
//放弃旧的清单
      for (size_t i = 0; i < manifests_.size(); i++) {
        ArchiveFile(dbname_ + "/" + manifests_[i]);
      }

//安装新清单
      status = env_->RenameFile(tmp, DescriptorFileName(dbname_, 1));
      if (status.ok()) {
        status = SetCurrentFile(env_, dbname_, 1);
      } else {
        env_->DeleteFile(tmp);
      }
    }
    return status;
  }

  void ArchiveFile(const std::string& fname) {
//移动到另一个目录。例如，用于
//DR/FO
//重命名为
//di/r/fo
    const char* slash = strrchr(fname.c_str(), '/');
    std::string new_dir;
    if (slash != NULL) {
      new_dir.assign(fname.data(), slash - fname.data());
    }
    new_dir.append("/lost");
env_->CreateDir(new_dir);  //忽略错误
    std::string new_file = new_dir;
    new_file.append("/");
    new_file.append((slash == NULL) ? fname.c_str() : slash + 1);
    Status s = env_->RenameFile(fname, new_file);
    Log(options_.info_log, "Archiving %s: %s\n",
        fname.c_str(), s.ToString().c_str());
  }
};
}  //命名空间

Status RepairDB(const std::string& dbname, const Options& options) {
  Repairer repairer(dbname, options);
  return repairer.Run();
}

}  //命名空间级别数据库
