
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
//dbimpl的表示由一组版本组成。这个
//最新版本称为“当前”。可以保留旧版本
//为实时迭代器提供一致的视图。
//
//每个版本跟踪每个级别的一组表文件。这个
//整个版本集在一个版本集中维护。
//
//版本、版本集与线程兼容，但需要外部
//所有访问的同步。

#ifndef STORAGE_LEVELDB_DB_VERSION_SET_H_
#define STORAGE_LEVELDB_DB_VERSION_SET_H_

#include <map>
#include <set>
#include <vector>
#include "db/dbformat.h"
#include "db/version_edit.h"
#include "port/port.h"
#include "port/thread_annotations.h"

namespace leveldb {

namespace log { class Writer; }

class Compaction;
class Iterator;
class MemTable;
class TableBuilder;
class TableCache;
class Version;
class VersionSet;
class WritableFile;

//返回最小的索引i，使文件[i]->最大的>=键。
//如果没有这样的文件，则返回files.size（）。
//要求：“文件”包含非重叠文件的排序列表。
extern int FindFile(const InternalKeyComparator& icmp,
                    const std::vector<FileMetaData*>& files,
                    const Slice& key);

//返回true iff“files”中的某些文件与用户密钥范围重叠
//[*最小，*最大]。
//最小值==null表示一个小于数据库中所有键的键。
//maximum==null表示一个键比数据库中的所有键都大。
//要求：如果disjoint_sorted_files，files[]包含disjoint范围
//按排序顺序。
extern bool SomeFileOverlapsRange(
    const InternalKeyComparator& icmp,
    bool disjoint_sorted_files,
    const std::vector<FileMetaData*>& files,
    const Slice* smallest_user_key,
    const Slice* largest_user_key);

class Version {
 public:
//附加到*iters的迭代器序列将
//合并时生成此版本的内容。
//要求：此版本已保存（请参阅versionset:：saveto）
  void AddIterators(const ReadOptions&, std::vector<Iterator*>* iters);

//查找键的值。如果找到，将其存储在*val中，然后
//返回OK。否则返回非OK状态。填充*STATS。
//要求：未锁定
  struct GetStats {
    FileMetaData* seek_file;
    int seek_file_level;
  };
  Status Get(const ReadOptions&, const LookupKey& key, std::string* val,
             GetStats* stats);

//将“stats”添加到当前状态。如果是新的
//可能需要触发压缩，否则为假。
//要求：保持锁定
  bool UpdateStats(const GetStats& stats);

//记录在指定的内部键处读取的字节样本。
//每个config:：kreabytespeid大约采集一次样本
//字节。如果可能需要触发新的压缩，则返回true。
//要求：保持锁定
  bool RecordReadSample(Slice key);

//引用计数管理（因此版本不会从
//在实时迭代器下）
  void Ref();
  void Unref();

  void GetOverlappingInputs(
      int level,
const InternalKey* begin,         //空表示所有键之前
const InternalKey* end,           //空表示所有键之后
      std::vector<FileMetaData*>* inputs);

//返回true iff指定级别中的某些文件重叠
//[*最小用户密钥，*最大用户密钥]的某些部分。
//最小的_user_key==null表示一个小于db中所有键的键。
//maximum_user_key==null表示一个密钥比数据库中的所有密钥都大。
  bool OverlapInLevel(int level,
                      const Slice* smallest_user_key,
                      const Slice* largest_user_key);

//返回应放置新MEMTABLE压缩的级别
//覆盖范围的结果[最小用户密钥，最大用户密钥]。
  int PickLevelForMemTableOutput(const Slice& smallest_user_key,
                                 const Slice& largest_user_key);

  int NumFiles(int level) const { return files_[level].size(); }

//返回描述此版本内容的可读字符串。
  std::string DebugString() const;

 private:
  friend class Compaction;
  friend class VersionSet;

  class LevelFileNumIterator;
  Iterator* NewConcatenatingIterator(const ReadOptions&, int level) const;

//为每个与用户密钥重叠的文件调用func（arg，level，f）
//从最新到最旧订购。如果函数调用返回
//错，不再打电话。
//
//需要：内部_key==user_key的用户部分。
  void ForEachOverlapping(Slice user_key, Slice internal_key,
                          void* arg,
                          bool (*func)(void*, int, FileMetaData*));

VersionSet* vset_;            //此版本所属的版本集
Version* next_;               //链接列表中的下一个版本
Version* prev_;               //链接列表中的上一个版本
int refs_;                    //此版本的实时引用数

//每个级别的文件列表
  std::vector<FileMetaData*> files_[config::kNumLevels];

//下一个要压缩的基于SEEK统计的文件。
  FileMetaData* file_to_compact_;
  int file_to_compact_level_;

//下一步应压实的水平面及其压实分数。
//评分<1表示不严格要求压实。这些领域
//由Finalize（）初始化。
  double compaction_score_;
  int compaction_level_;

  explicit Version(VersionSet* vset)
      : vset_(vset), next_(this), prev_(this), refs_(0),
        file_to_compact_(NULL),
        file_to_compact_level_(-1),
        compaction_score_(-1),
        compaction_level_(-1) {
  }

  ~Version();

//不允许复制
  Version(const Version&);
  void operator=(const Version&);
};

class VersionSet {
 public:
  VersionSet(const std::string& dbname,
             const Options* options,
             TableCache* table_cache,
             const InternalKeyComparator*);
  ~VersionSet();

//将*编辑应用于当前版本以形成新的描述符
//同时保存为持久状态并作为新的
//当前版本。将在实际写入文件时释放*mu。
//需要：*亩在入境时被扣留。
//要求：没有其他线程同时调用logAndApply（）。
  Status LogAndApply(VersionEdit* edit, port::Mutex* mu)
      EXCLUSIVE_LOCKS_REQUIRED(mu);

//从持久存储中恢复上次保存的描述符。
  Status Recover(bool *save_manifest);

//返回当前版本。
  Version* current() const { return current_; }

//返回当前清单文件号
  uint64_t ManifestFileNumber() const { return manifest_file_number_; }

//分配并返回新的文件号
  uint64_t NewFileNumber() { return next_file_number_++; }

//安排重新使用“文件编号”，除非更新的文件编号
//已经分配。
//要求：调用newFileNumber（）返回了“文件编号”。
  void ReuseFileNumber(uint64_t file_number) {
    if (next_file_number_ == file_number + 1) {
      next_file_number_ = file_number;
    }
  }

//返回指定级别的表文件数。
  int NumLevelFiles(int level) const;

//返回指定级别上所有文件的组合文件大小。
  int64_t NumLevelBytes(int level) const;

//返回最后一个序列号。
  uint64_t LastSequence() const { return last_sequence_; }

//将最后一个序列号设置为S。
  void SetLastSequence(uint64_t s) {
    assert(s >= last_sequence_);
    last_sequence_ = s;
  }

//将指定的文件号标记为已用。
  void MarkFileNumberUsed(uint64_t number);

//返回当前日志文件号。
  uint64_t LogNumber() const { return log_number_; }

//返回当前日志文件的日志文件号
//正在压缩，如果没有这样的日志文件，则为零。
  uint64_t PrevLogNumber() const { return prev_log_number_; }

//选择新压缩的级别和输入。
//如果没有要执行的压缩操作，则返回空值。
//否则，返回指向堆分配对象的指针，
//描述压缩。调用方应删除结果。
  Compaction* PickCompaction();

//返回压缩对象以压缩范围[开始，结束]
//指定的级别。如果其中没有任何内容，则返回空值
//与指定范围重叠的级别。呼叫者应删除
//结果。
  Compaction* CompactRange(
      int level,
      const InternalKey* begin,
      const InternalKey* end);

//返回下一级任意
//文件级别大于等于1。
  int64_t MaxNextLevelOverlappingBytes();

//创建一个迭代器，读取“*c”的压缩输入。
//当不再需要迭代器时，调用方应该删除它。
  Iterator* MakeInputIterator(Compaction* c);

//如果某个级别需要压缩，则返回true。
  bool NeedsCompaction() const {
    Version* v = current_;
    return (v->compaction_score_ >= 1) || (v->file_to_compact_ != NULL);
  }

//将任何Live版本中列出的所有文件添加到*Live。
//也可能改变一些内部状态。
  void AddLiveFiles(std::set<uint64_t>* live);

//返回数据库中数据的近似偏移量
//“钥匙”从“V”版起。
  uint64_t ApproximateOffsetOf(Version* v, const InternalKey& key);

//返回一个人类可读的简短（单行）数字摘要
//每个级别的文件数。使用*刮痕作为备份存储。
  struct LevelSummaryStorage {
    char buffer[100];
  };
  const char* LevelSummary(LevelSummaryStorage* scratch) const;

 private:
  class Builder;

  friend class Compaction;
  friend class Version;

  bool ReuseManifest(const std::string& dscname, const std::string& dscbase);

  void Finalize(Version* v);

  void GetRange(const std::vector<FileMetaData*>& inputs,
                InternalKey* smallest,
                InternalKey* largest);

  void GetRange2(const std::vector<FileMetaData*>& inputs1,
                 const std::vector<FileMetaData*>& inputs2,
                 InternalKey* smallest,
                 InternalKey* largest);

  void SetupOtherInputs(Compaction* c);

//将当前内容保存到*日志
  Status WriteSnapshot(log::Writer* log);

  void AppendVersion(Version* v);

  Env* const env_;
  const std::string dbname_;
  const Options* const options_;
  TableCache* const table_cache_;
  const InternalKeyComparator icmp_;
  uint64_t next_file_number_;
  uint64_t manifest_file_number_;
  uint64_t last_sequence_;
  uint64_t log_number_;
uint64_t prev_log_number_;  //0或正在压缩的memtable的备份存储

//懒洋洋地打开
  WritableFile* descriptor_file_;
  log::Writer* descriptor_log_;
Version dummy_versions_;  //循环双链接版本列表的标题。
Version* current_;        //=虚拟版本

//每个级别的键，该级别的下一次压缩将在该键处开始。
//空字符串或有效的InternalKey。
  std::string compact_pointer_[config::kNumLevels];

//不允许复制
  VersionSet(const VersionSet&);
  void operator=(const VersionSet&);
};

//压缩将封装有关压缩的信息。
class Compaction {
 public:
  ~Compaction();

//返回正在压缩的级别。“级别”的输入
//“Level+1”将被合并以生成一组“Level+1”文件。
  int level() const { return level_; }

//返回保存对描述符所做编辑的对象。
//通过这种压缩。
  VersionEdit* edit() { return &edit_; }

//“which”必须是0或1
  int num_input_files(int which) const { return inputs_[which].size(); }

//返回“level（）+which”（“which”必须是0或1）处的第i个输入文件。
  FileMetaData* input(int which, int i) const { return inputs_[which][i]; }

//在此压缩期间要生成的最大文件大小。
  uint64_t MaxOutputFileSize() const { return max_output_file_size_; }

//这是一个可以通过
//将单个输入文件移动到下一个级别（不合并或拆分）
  bool IsTrivialMove() const;

//将此压缩的所有输入作为删除操作添加到*编辑。
  void AddInputDeletions(VersionEdit* edit);

//如果我们提供的信息保证
//压缩正在生成“级别+1”中不存在数据的数据。
//在大于“级别+1”的级别中。
  bool IsBaseLevelForKey(const Slice& user_key);

//返回真iff我们应该停止构建电流输出
//在处理“内部密钥”之前。
  bool ShouldStopBefore(const Slice& internal_key);

//一旦压缩完成，就释放压缩的输入版本
//是成功的。
  void ReleaseInputs();

 private:
  friend class Version;
  friend class VersionSet;

  Compaction(const Options* options, int level);

  int level_;
  uint64_t max_output_file_size_;
  Version* input_version_;
  VersionEdit edit_;

//每个压缩读取“level_uuu”和“level_uu1”的输入。
std::vector<FileMetaData*> inputs_[2];      //两组输入

//用于检查重叠祖父母文件数的状态
//（父级==级别1，父级==级别2）
  std::vector<FileMetaData*> grandparents_;
size_t grandparent_index_;  //祖父母索引开始
bool seen_key_;             //已看到一些输出键
int64_t overlapped_bytes_;  //当前输出之间的重叠字节数
//和祖父母的档案

//实现ISbaseLevelWorkey的状态

//级别ptrs将索引保存到输入版本\->级别\：我们的状态
//我们定位在每个文件的一个范围内
//比压实作业所涉及的水平更高（即
//所有L>=等级2）。
  size_t level_ptrs_[config::kNumLevels];
};

}  //命名空间级别数据库

#endif  //存储级别数据库版本设置
