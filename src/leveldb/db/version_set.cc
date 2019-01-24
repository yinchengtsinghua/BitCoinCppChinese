
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

#include "db/version_set.h"

#include <algorithm>
#include <stdio.h>
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/table_cache.h"
#include "leveldb/env.h"
#include "leveldb/table_builder.h"
#include "table/merger.h"
#include "table/two_level_iterator.h"
#include "util/coding.h"
#include "util/logging.h"

namespace leveldb {

static size_t TargetFileSize(const Options* options) {
  return options->max_file_size;
}

//在我们之前，祖父母（即级别+2）中重叠的最大字节数
//停止在级别->级别+1压缩中生成单个文件。
static int64_t MaxGrandParentOverlapBytes(const Options* options) {
  return 10 * TargetFileSize(options);
}

//所有压缩文件中的最大字节数。我们避免扩张
//压缩的低级文件集，如果它将使
//总压缩覆盖的字节数超过了这个数量。
static int64_t ExpandedCompactionByteSizeLimit(const Options* options) {
  return 25 * TargetFileSize(options);
}

static double MaxBytesForLevel(const Options* options, int level) {
//注：由于我们设置了
//基于文件数的0级压缩阈值。

//0级和1级的结果
  double result = 10. * 1048576.0;
  while (level > 1) {
    result *= 10;
    level--;
  }
  return result;
}

static uint64_t MaxFileSizeForLevel(const Options* options, int level) {
//我们可以根据级别变化以减少文件数量？
  return TargetFileSize(options);
}

static int64_t TotalFileSize(const std::vector<FileMetaData*>& files) {
  int64_t sum = 0;
  for (size_t i = 0; i < files.size(); i++) {
    sum += files[i]->file_size;
  }
  return sum;
}

Version::~Version() {
  assert(refs_ == 0);

//从链接列表中删除
  prev_->next_ = next_;
  next_->prev_ = prev_;

//删除对文件的引用
  for (int level = 0; level < config::kNumLevels; level++) {
    for (size_t i = 0; i < files_[level].size(); i++) {
      FileMetaData* f = files_[level][i];
      assert(f->refs > 0);
      f->refs--;
      if (f->refs <= 0) {
        delete f;
      }
    }
  }
}

int FindFile(const InternalKeyComparator& icmp,
             const std::vector<FileMetaData*>& files,
             const Slice& key) {
  uint32_t left = 0;
  uint32_t right = files.size();
  while (left < right) {
    uint32_t mid = (left + right) / 2;
    const FileMetaData* f = files[mid];
    if (icmp.InternalKeyComparator::Compare(f->largest.Encode(), key) < 0) {
//“mid.maximum”处的键是<“target”。因此所有
//“mid”处或之前的文件无趣。
      left = mid + 1;
    } else {
//“mid.maximum”处的键大于等于“target”。因此所有文件
//“mid”之后就没意思了。
      right = mid;
    }
  }
  return right;
}

static bool AfterFile(const Comparator* ucmp,
                      const Slice* user_key, const FileMetaData* f) {
//空用户密钥出现在所有密钥之前，因此永远不会出现在*f之后
  return (user_key != NULL &&
          ucmp->Compare(*user_key, f->largest.user_key()) > 0);
}

static bool BeforeFile(const Comparator* ucmp,
                       const Slice* user_key, const FileMetaData* f) {
//空用户密钥出现在所有密钥之后，因此永远不会出现在*f之前。
  return (user_key != NULL &&
          ucmp->Compare(*user_key, f->smallest.user_key()) < 0);
}

bool SomeFileOverlapsRange(
    const InternalKeyComparator& icmp,
    bool disjoint_sorted_files,
    const std::vector<FileMetaData*>& files,
    const Slice* smallest_user_key,
    const Slice* largest_user_key) {
  const Comparator* ucmp = icmp.user_comparator();
  if (!disjoint_sorted_files) {
//需要检查所有文件
    for (size_t i = 0; i < files.size(); i++) {
      const FileMetaData* f = files[i];
      if (AfterFile(ucmp, smallest_user_key, f) ||
          BeforeFile(ucmp, largest_user_key, f)) {
//无重叠
      } else {
return true;  //重叠
      }
    }
    return false;
  }

//对文件列表进行二进制搜索
  uint32_t index = 0;
  if (smallest_user_key != NULL) {
//为最小的用户密钥查找最早的内部密钥
    InternalKey small(*smallest_user_key, kMaxSequenceNumber,kValueTypeForSeek);
    index = FindFile(icmp, files, small.Encode());
  }

  if (index >= files.size()) {
//范围的开始在所有文件之后，所以没有重叠。
    return false;
  }

  return !BeforeFile(ucmp, largest_user_key, files[index]);
}

//内部迭代器。对于给定的版本/级别对，生成
//有关级别中文件的信息。对于给定的条目，键（））
//是文件中出现的最大键，value（）是
//包含文件号和文件大小的16字节值
//使用EncodeFixed64编码。
class Version::LevelFileNumIterator : public Iterator {
 public:
  LevelFileNumIterator(const InternalKeyComparator& icmp,
                       const std::vector<FileMetaData*>* flist)
      : icmp_(icmp),
        flist_(flist),
index_(flist->size()) {        //标记为无效
  }
  virtual bool Valid() const {
    return index_ < flist_->size();
  }
  virtual void Seek(const Slice& target) {
    index_ = FindFile(icmp_, *flist_, target);
  }
  virtual void SeekToFirst() { index_ = 0; }
  virtual void SeekToLast() {
    index_ = flist_->empty() ? 0 : flist_->size() - 1;
  }
  virtual void Next() {
    assert(Valid());
    index_++;
  }
  virtual void Prev() {
    assert(Valid());
    if (index_ == 0) {
index_ = flist_->size();  //标记为无效
    } else {
      index_--;
    }
  }
  Slice key() const {
    assert(Valid());
    return (*flist_)[index_]->largest.Encode();
  }
  Slice value() const {
    assert(Valid());
    EncodeFixed64(value_buf_, (*flist_)[index_]->number);
    EncodeFixed64(value_buf_+8, (*flist_)[index_]->file_size);
    return Slice(value_buf_, sizeof(value_buf_));
  }
  virtual Status status() const { return Status::OK(); }
 private:
  const InternalKeyComparator icmp_;
  const std::vector<FileMetaData*>* const flist_;
  uint32_t index_;

//值（）的备份存储。保存文件编号和大小。
  mutable char value_buf_[16];
};

static Iterator* GetFileIterator(void* arg,
                                 const ReadOptions& options,
                                 const Slice& file_value) {
  TableCache* cache = reinterpret_cast<TableCache*>(arg);
  if (file_value.size() != 16) {
    return NewErrorIterator(
        Status::Corruption("FileReader invoked with unexpected value"));
  } else {
    return cache->NewIterator(options,
                              DecodeFixed64(file_value.data()),
                              DecodeFixed64(file_value.data() + 8));
  }
}

Iterator* Version::NewConcatenatingIterator(const ReadOptions& options,
                                            int level) const {
  return NewTwoLevelIterator(
      new LevelFileNumIterator(vset_->icmp_, &files_[level]),
      &GetFileIterator, vset_->table_cache_, options);
}

void Version::AddIterators(const ReadOptions& options,
                           std::vector<Iterator*>* iters) {
//合并所有零级文件，因为它们可能重叠
  for (size_t i = 0; i < files_[0].size(); i++) {
    iters->push_back(
        vset_->table_cache_->NewIterator(
            options, files_[0][i]->number, files_[0][i]->file_size));
  }

//对于大于0的级别，我们可以使用连续迭代器
//浏览级别中不重叠的文件，打开它们
//懒洋洋地
  for (int level = 1; level < config::kNumLevels; level++) {
    if (!files_[level].empty()) {
      iters->push_back(NewConcatenatingIterator(options, level));
    }
  }
}

//从TableCache:：get（）回调
namespace {
enum SaverState {
  kNotFound,
  kFound,
  kDeleted,
  kCorrupt,
};
struct Saver {
  SaverState state;
  const Comparator* ucmp;
  Slice user_key;
  std::string* value;
};
}
static void SaveValue(void* arg, const Slice& ikey, const Slice& v) {
  Saver* s = reinterpret_cast<Saver*>(arg);
  ParsedInternalKey parsed_key;
  if (!ParseInternalKey(ikey, &parsed_key)) {
    s->state = kCorrupt;
  } else {
    if (s->ucmp->Compare(parsed_key.user_key, s->user_key) == 0) {
      s->state = (parsed_key.type == kTypeValue) ? kFound : kDeleted;
      if (s->state == kFound) {
        s->value->assign(v.data(), v.size());
      }
    }
  }
}

static bool NewestFirst(FileMetaData* a, FileMetaData* b) {
  return a->number > b->number;
}

void Version::ForEachOverlapping(Slice user_key, Slice internal_key,
                                 void* arg,
                                 bool (*func)(void*, int, FileMetaData*)) {
//TODO（sanjay）：更改version:：get（）以使用此函数。
  const Comparator* ucmp = vset_->icmp_.user_comparator();

//按从新到旧的顺序搜索级别0。
  std::vector<FileMetaData*> tmp;
  tmp.reserve(files_[0].size());
  for (uint32_t i = 0; i < files_[0].size(); i++) {
    FileMetaData* f = files_[0][i];
    if (ucmp->Compare(user_key, f->smallest.user_key()) >= 0 &&
        ucmp->Compare(user_key, f->largest.user_key()) <= 0) {
      tmp.push_back(f);
    }
  }
  if (!tmp.empty()) {
    std::sort(tmp.begin(), tmp.end(), NewestFirst);
    for (uint32_t i = 0; i < tmp.size(); i++) {
      if (!(*func)(arg, 0, tmp[i])) {
        return;
      }
    }
  }

//搜索其他级别。
  for (int level = 1; level < config::kNumLevels; level++) {
    size_t num_files = files_[level].size();
    if (num_files == 0) continue;

//二进制搜索，查找最大键大于等于内部键的最早索引。
    uint32_t index = FindFile(vset_->icmp_, files_[level], internal_key);
    if (index < num_files) {
      FileMetaData* f = files_[level][index];
      if (ucmp->Compare(user_key, f->smallest.user_key()) < 0) {
//所有“f”都超过了用户密钥的任何数据
      } else {
        if (!(*func)(arg, level, f)) {
          return;
        }
      }
    }
  }
}

Status Version::Get(const ReadOptions& options,
                    const LookupKey& k,
                    std::string* value,
                    GetStats* stats) {
  Slice ikey = k.internal_key();
  Slice user_key = k.user_key();
  const Comparator* ucmp = vset_->icmp_.user_comparator();
  Status s;

  stats->seek_file = NULL;
  stats->seek_file_level = -1;
  FileMetaData* last_file_read = NULL;
  int last_file_read_level = -1;

//我们可以逐级搜索，因为条目从不跨越
//水平。因此，我们保证如果我们找到数据
//在较小的层次中，较后的层次是不相关的。
  std::vector<FileMetaData*> tmp;
  FileMetaData* tmp2;
  for (int level = 0; level < config::kNumLevels; level++) {
    size_t num_files = files_[level].size();
    if (num_files == 0) continue;

//获取要在此级别中搜索的文件列表
    FileMetaData* const* files = &files_[level][0];
    if (level == 0) {
//0级文件可能相互重叠。查找所有文件
//重叠用户密钥并按从新到旧的顺序处理它们。
      tmp.reserve(num_files);
      for (uint32_t i = 0; i < num_files; i++) {
        FileMetaData* f = files[i];
        if (ucmp->Compare(user_key, f->smallest.user_key()) >= 0 &&
            ucmp->Compare(user_key, f->largest.user_key()) <= 0) {
          tmp.push_back(f);
        }
      }
      if (tmp.empty()) continue;

      std::sort(tmp.begin(), tmp.end(), NewestFirst);
      files = &tmp[0];
      num_files = tmp.size();
    } else {
//二进制搜索，查找最大键大于等于ikey的最早索引。
      uint32_t index = FindFile(vset_->icmp_, files_[level], ikey);
      if (index >= num_files) {
        files = NULL;
        num_files = 0;
      } else {
        tmp2 = files[index];
        if (ucmp->Compare(user_key, tmp2->smallest.user_key()) < 0) {
//所有“tmp2”都超过了用户密钥的任何数据
          files = NULL;
          num_files = 0;
        } else {
          files = &tmp2;
          num_files = 1;
        }
      }
    }

    for (uint32_t i = 0; i < num_files; ++i) {
      if (last_file_read != NULL && stats->seek_file == NULL) {
//我们不止一次寻找这本书。对第一个文件收费。
        stats->seek_file = last_file_read;
        stats->seek_file_level = last_file_read_level;
      }

      FileMetaData* f = files[i];
      last_file_read = f;
      last_file_read_level = level;

      Saver saver;
      saver.state = kNotFound;
      saver.ucmp = ucmp;
      saver.user_key = user_key;
      saver.value = value;
      s = vset_->table_cache_->Get(options, f->number, f->file_size,
                                   ikey, &saver, SaveValue);
      if (!s.ok()) {
        return s;
      }
      switch (saver.state) {
        case kNotFound:
break;      //继续在其他文件中搜索
        case kFound:
          return s;
        case kDeleted:
s = Status::NotFound(Slice());  //速度使用空错误消息
          return s;
        case kCorrupt:
          s = Status::Corruption("corrupted key for ", user_key);
          return s;
      }
    }
  }

return Status::NotFound(Slice());  //为速度使用空错误消息
}

bool Version::UpdateStats(const GetStats& stats) {
  FileMetaData* f = stats.seek_file;
  if (f != NULL) {
    f->allowed_seeks--;
    if (f->allowed_seeks <= 0 && file_to_compact_ == NULL) {
      file_to_compact_ = f;
      file_to_compact_level_ = stats.seek_file_level;
      return true;
    }
  }
  return false;
}

bool Version::RecordReadSample(Slice internal_key) {
  ParsedInternalKey ikey;
  if (!ParseInternalKey(internal_key, &ikey)) {
    return false;
  }

  struct State {
GetStats stats;  //保留第一个匹配文件
    int matches;

    static bool Match(void* arg, int level, FileMetaData* f) {
      State* state = reinterpret_cast<State*>(arg);
      state->matches++;
      if (state->matches == 1) {
//记住第一场比赛。
        state->stats.seek_file = f;
        state->stats.seek_file_level = level;
      }
//一旦有第二个匹配，就可以停止迭代。
      return state->matches < 2;
    }
  };

  State state;
  state.matches = 0;
  ForEachOverlapping(ikey.user_key, internal_key, &state, &State::Match);

//必须至少有两个匹配项，因为我们要合并
//文件夹。但是如果我们有一个包含许多
//覆盖和删除？我们是否应该有另一种机制
//找到这样的文件？
  if (state.matches >= 2) {
//1MB成本大约是1次搜索（请参见builder:：apply中的注释）。
    return UpdateStats(state.stats);
  }
  return false;
}

void Version::Ref() {
  ++refs_;
}

void Version::Unref() {
  assert(this != &vset_->dummy_versions_);
  assert(refs_ >= 1);
  --refs_;
  if (refs_ == 0) {
    delete this;
  }
}

bool Version::OverlapInLevel(int level,
                             const Slice* smallest_user_key,
                             const Slice* largest_user_key) {
  return SomeFileOverlapsRange(vset_->icmp_, (level > 0), files_[level],
                               smallest_user_key, largest_user_key);
}

int Version::PickLevelForMemTableOutput(
    const Slice& smallest_user_key,
    const Slice& largest_user_key) {
  int level = 0;
  if (!OverlapInLevel(0, &smallest_user_key, &largest_user_key)) {
//如果下一层没有重叠，则推到下一层。
//在这之后，级别中重叠的字节是有限的。
    InternalKey start(smallest_user_key, kMaxSequenceNumber, kValueTypeForSeek);
    InternalKey limit(largest_user_key, 0, static_cast<ValueType>(0));
    std::vector<FileMetaData*> overlaps;
    while (level < config::kMaxMemCompactLevel) {
      if (OverlapInLevel(level + 1, &smallest_user_key, &largest_user_key)) {
        break;
      }
      if (level + 2 < config::kNumLevels) {
//检查文件是否重叠了太多的祖父母字节。
        GetOverlappingInputs(level + 2, &start, &limit, &overlaps);
        const int64_t sum = TotalFileSize(overlaps);
        if (sum > MaxGrandParentOverlapBytes(vset_->options_)) {
          break;
        }
      }
      level++;
    }
  }
  return level;
}

//存储在“*inputs”中所有重叠的“level”中的文件[开始，结束]
void Version::GetOverlappingInputs(
    int level,
    const InternalKey* begin,
    const InternalKey* end,
    std::vector<FileMetaData*>* inputs) {
  assert(level >= 0);
  assert(level < config::kNumLevels);
  inputs->clear();
  Slice user_begin, user_end;
  if (begin != NULL) {
    user_begin = begin->user_key();
  }
  if (end != NULL) {
    user_end = end->user_key();
  }
  const Comparator* user_cmp = vset_->icmp_.user_comparator();
  for (size_t i = 0; i < files_[level].size(); ) {
    FileMetaData* f = files_[level][i++];
    const Slice file_start = f->smallest.user_key();
    const Slice file_limit = f->largest.user_key();
    if (begin != NULL && user_cmp->Compare(file_limit, user_begin) < 0) {
//“f”完全在指定范围之前；跳过它
    } else if (end != NULL && user_cmp->Compare(file_start, user_end) > 0) {
//“f”完全在指定范围之后；跳过它
    } else {
      inputs->push_back(f);
      if (level == 0) {
//0级文件可能相互重叠。所以检查一下
//添加的文件已扩展范围。如果是，请重新启动搜索。
        if (begin != NULL && user_cmp->Compare(file_start, user_begin) < 0) {
          user_begin = file_start;
          inputs->clear();
          i = 0;
        } else if (end != NULL && user_cmp->Compare(file_limit, user_end) > 0) {
          user_end = file_limit;
          inputs->clear();
          i = 0;
        }
      }
    }
  }
}

std::string Version::DebugString() const {
  std::string r;
  for (int level = 0; level < config::kNumLevels; level++) {
//例如。，
//--- 1级---
//17:123 [ a '…D’
//20:43 [ e '…G’
    r.append("--- level ");
    AppendNumberTo(&r, level);
    r.append(" ---\n");
    const std::vector<FileMetaData*>& files = files_[level];
    for (size_t i = 0; i < files.size(); i++) {
      r.push_back(' ');
      AppendNumberTo(&r, files[i]->number);
      r.push_back(':');
      AppendNumberTo(&r, files[i]->file_size);
      r.append("[");
      r.append(files[i]->smallest.DebugString());
      r.append(" .. ");
      r.append(files[i]->largest.DebugString());
      r.append("]\n");
    }
  }
  return r;
}

//帮助程序类，这样我们可以有效地应用整个序列
//编辑特定状态而不创建中间
//包含中间状态完整副本的版本。
class VersionSet::Builder {
 private:
//帮助按v->files_[file_number]排序。最小
  struct BySmallestKey {
    const InternalKeyComparator* internal_comparator;

    bool operator()(FileMetaData* f1, FileMetaData* f2) const {
      int r = internal_comparator->Compare(f1->smallest, f2->smallest);
      if (r != 0) {
        return (r < 0);
      } else {
//按文件编号断开连接
        return (f1->number < f2->number);
      }
    }
  };

  typedef std::set<FileMetaData*, BySmallestKey> FileSet;
  struct LevelState {
    std::set<uint64_t> deleted_files;
    FileSet* added_files;
  };

  VersionSet* vset_;
  Version* base_;
  LevelState levels_[config::kNumLevels];

 public:
//使用来自*base的文件和来自*vset的其他信息初始化生成器
  Builder(VersionSet* vset, Version* base)
      : vset_(vset),
        base_(base) {
    base_->Ref();
    BySmallestKey cmp;
    cmp.internal_comparator = &vset_->icmp_;
    for (int level = 0; level < config::kNumLevels; level++) {
      levels_[level].added_files = new FileSet(cmp);
    }
  }

  ~Builder() {
    for (int level = 0; level < config::kNumLevels; level++) {
      const FileSet* added = levels_[level].added_files;
      std::vector<FileMetaData*> to_unref;
      to_unref.reserve(added->size());
      for (FileSet::const_iterator it = added->begin();
          it != added->end(); ++it) {
        to_unref.push_back(*it);
      }
      delete added;
      for (uint32_t i = 0; i < to_unref.size(); i++) {
        FileMetaData* f = to_unref[i];
        f->refs--;
        if (f->refs <= 0) {
          delete f;
        }
      }
    }
    base_->Unref();
  }

//将*编辑中的所有编辑应用于当前状态。
  void Apply(VersionEdit* edit) {
//更新压缩指针
    for (size_t i = 0; i < edit->compact_pointers_.size(); i++) {
      const int level = edit->compact_pointers_[i].first;
      vset_->compact_pointer_[level] =
          edit->compact_pointers_[i].second.Encode().ToString();
    }

//删除文件
    const VersionEdit::DeletedFileSet& del = edit->deleted_files_;
    for (VersionEdit::DeletedFileSet::const_iterator iter = del.begin();
         iter != del.end();
         ++iter) {
      const int level = iter->first;
      const uint64_t number = iter->second;
      levels_[level].deleted_files.insert(number);
    }

//添加新文件
    for (size_t i = 0; i < edit->new_files_.size(); i++) {
      const int level = edit->new_files_[i].first;
      FileMetaData* f = new FileMetaData(edit->new_files_[i].second);
      f->refs = 1;

//我们安排在
//一定数量的搜寻。让我们假设：
//（1）一次搜寻成本10毫秒
//（2）读写1MB成本为10ms（100MB/s）
//（3）1 MB的压实度为25 MB的IO:
//从该级别读取1MB
//从下一级读取10-12MB（边界可能错位）
//10-12MB写入下一级
//这意味着，25个寻找成本与压实相同。
//1兆数据。也就是说，一次搜索的成本大约是
//与40kb数据的压缩相同。我们有点
//保守，每16kb允许大约一次搜索
//在触发压缩之前的数据。
      f->allowed_seeks = (f->file_size / 16384);
      if (f->allowed_seeks < 100) f->allowed_seeks = 100;

      levels_[level].deleted_files.erase(f->number);
      levels_[level].added_files->insert(f);
    }
  }

//以*v保存当前状态。
  void SaveTo(Version* v) {
    BySmallestKey cmp;
    cmp.internal_comparator = &vset_->icmp_;
    for (int level = 0; level < config::kNumLevels; level++) {
//将添加的文件集与预先存在的文件集合并。
//删除所有已删除的文件。将结果存储在*v中。
      const std::vector<FileMetaData*>& base_files = base_->files_[level];
      std::vector<FileMetaData*>::const_iterator base_iter = base_files.begin();
      std::vector<FileMetaData*>::const_iterator base_end = base_files.end();
      const FileSet* added = levels_[level].added_files;
      v->files_[level].reserve(base_files.size() + added->size());
      for (FileSet::const_iterator added_iter = added->begin();
           added_iter != added->end();
           ++added_iter) {
//添加基目录中列出的所有较小文件
        for (std::vector<FileMetaData*>::const_iterator bpos
                 = std::upper_bound(base_iter, base_end, *added_iter, cmp);
             base_iter != bpos;
             ++base_iter) {
          MaybeAddFile(v, level, *base_iter);
        }

        MaybeAddFile(v, level, *added_iter);
      }

//添加其余基本文件
      for (; base_iter != base_end; ++base_iter) {
        MaybeAddFile(v, level, *base_iter);
      }

#ifndef NDEBUG
//确保级别>0没有重叠
      if (level > 0) {
        for (uint32_t i = 1; i < v->files_[level].size(); i++) {
          const InternalKey& prev_end = v->files_[level][i-1]->largest;
          const InternalKey& this_begin = v->files_[level][i]->smallest;
          if (vset_->icmp_.Compare(prev_end, this_begin) >= 0) {
            fprintf(stderr, "overlapping ranges in same level %s vs. %s\n",
                    prev_end.DebugString().c_str(),
                    this_begin.DebugString().c_str());
            abort();
          }
        }
      }
#endif
    }
  }

  void MaybeAddFile(Version* v, int level, FileMetaData* f) {
    if (levels_[level].deleted_files.count(f->number) > 0) {
//文件已删除：不执行任何操作
    } else {
      std::vector<FileMetaData*>* files = &v->files_[level];
      if (level > 0 && !files->empty()) {
//不能重叠
        assert(vset_->icmp_.Compare((*files)[files->size()-1]->largest,
                                    f->smallest) < 0);
      }
      f->refs++;
      files->push_back(f);
    }
  }
};

VersionSet::VersionSet(const std::string& dbname,
                       const Options* options,
                       TableCache* table_cache,
                       const InternalKeyComparator* cmp)
    : env_(options->env),
      dbname_(dbname),
      options_(options),
      table_cache_(table_cache),
      icmp_(*cmp),
      next_file_number_(2),
manifest_file_number_(0),  //由recover（）填充
      last_sequence_(0),
      log_number_(0),
      prev_log_number_(0),
      descriptor_file_(NULL),
      descriptor_log_(NULL),
      dummy_versions_(this),
      current_(NULL) {
  AppendVersion(new Version(this));
}

VersionSet::~VersionSet() {
  current_->Unref();
assert(dummy_versions_.next_ == &dummy_versions_);  //列表必须为空
  delete descriptor_log_;
  delete descriptor_file_;
}

void VersionSet::AppendVersion(Version* v) {
//使“V”电流
  assert(v->refs_ == 0);
  assert(v != current_);
  if (current_ != NULL) {
    current_->Unref();
  }
  current_ = v;
  v->Ref();

//附加到链接列表
  v->prev_ = dummy_versions_.prev_;
  v->next_ = &dummy_versions_;
  v->prev_->next_ = v;
  v->next_->prev_ = v;
}

Status VersionSet::LogAndApply(VersionEdit* edit, port::Mutex* mu) {
  if (edit->has_log_number_) {
    assert(edit->log_number_ >= log_number_);
    assert(edit->log_number_ < next_file_number_);
  } else {
    edit->SetLogNumber(log_number_);
  }

  if (!edit->has_prev_log_number_) {
    edit->SetPrevLogNumber(prev_log_number_);
  }

  edit->SetNextFile(next_file_number_);
  edit->SetLastSequence(last_sequence_);

  Version* v = new Version(this);
  {
    Builder builder(this, current_);
    builder.Apply(edit);
    builder.SaveTo(v);
  }
  Finalize(v);

//如果需要，通过创建
//包含当前版本快照的临时文件。
  std::string new_manifest_file;
  Status s;
  if (descriptor_log_ == NULL) {
//没有理由在这里解锁*mu，因为我们只在
//首先调用logandapply（打开数据库时）。
    assert(descriptor_file_ == NULL);
    new_manifest_file = DescriptorFileName(dbname_, manifest_file_number_);
    edit->SetNextFile(next_file_number_);
    s = env_->NewWritableFile(new_manifest_file, &descriptor_file_);
    if (s.ok()) {
      descriptor_log_ = new log::Writer(descriptor_file_);
      s = WriteSnapshot(descriptor_log_);
    }
  }

//在昂贵的清单日志写入期间解锁
  {
    mu->Unlock();

//将新记录写入清单日志
    if (s.ok()) {
      std::string record;
      edit->EncodeTo(&record);
      s = descriptor_log_->AddRecord(record);
      if (s.ok()) {
        s = descriptor_file_->Sync();
      }
      if (!s.ok()) {
        Log(options_->info_log, "MANIFEST write: %s\n", s.ToString().c_str());
      }
    }

//如果我们刚刚创建了一个新的描述符文件，请通过编写
//指向它的新当前文件。
    if (s.ok() && !new_manifest_file.empty()) {
      s = SetCurrentFile(env_, dbname_, manifest_file_number_);
    }

    mu->Lock();
  }

//安装新版本
  if (s.ok()) {
    AppendVersion(v);
    log_number_ = edit->log_number_;
    prev_log_number_ = edit->prev_log_number_;
  } else {
    delete v;
    if (!new_manifest_file.empty()) {
      delete descriptor_log_;
      delete descriptor_file_;
      descriptor_log_ = NULL;
      descriptor_file_ = NULL;
      env_->DeleteFile(new_manifest_file);
    }
  }

  return s;
}

Status VersionSet::Recover(bool *save_manifest) {
  struct LogReporter : public log::Reader::Reporter {
    Status* status;
    virtual void Corruption(size_t bytes, const Status& s) {
      if (this->status->ok()) *this->status = s;
    }
  };

//读取“当前”文件，其中包含指向当前清单文件的指针
  std::string current;
  Status s = ReadFileToString(env_, CurrentFileName(dbname_), &current);
  if (!s.ok()) {
    return s;
  }
  if (current.empty() || current[current.size()-1] != '\n') {
    return Status::Corruption("CURRENT file does not end with newline");
  }
  current.resize(current.size() - 1);

  std::string dscname = dbname_ + "/" + current;
  SequentialFile* file;
  s = env_->NewSequentialFile(dscname, &file);
  if (!s.ok()) {
    return s;
  }

  bool have_log_number = false;
  bool have_prev_log_number = false;
  bool have_next_file = false;
  bool have_last_sequence = false;
  uint64_t next_file = 0;
  uint64_t last_sequence = 0;
  uint64_t log_number = 0;
  uint64_t prev_log_number = 0;
  Builder builder(this, current_);

  {
    LogReporter reporter;
    reporter.status = &s;
    /*：：读卡器读卡器（文件和报告器，真/*校验和*/，0/*初始偏移量*/）；
    切片记录；
    std：：字符串划痕；
    while（reader.readrecord（&record，&scratch）&&s.ok（））
      版本编辑；
      s=编辑。解码（记录）；
      如果（S.O.（））{
        如果（编辑有“比较器”&&
            edit.comparator_uu！=icmp_u.user_comparator（）->name（））
          S=状态：：无效参数（
              edit.comparator“与现有的comparator不匹配”，
              icp_u.user_comparator（）->name（））；
        }
      }

      如果（S.O.（））{
        builder.apply（&edit）；
      }

      if（编辑有日志编号）
        日志编号=edit.log编号
        将“对数”设为“真”；
      }

      if（编辑有上一个日志编号）
        prev_log_number=edit.prev_log_number；
        将上一个日志编号设为真；
      }

      if（edit.has_next_file_number_
        next_file=edit.next_file_编号；
        将下一个文件设为真；
      }

      if（edit.has_last_sequence_
        last_sequence=edit.last_sequence_u；
        将最后一个序列设为真；
      }
    }
  }
  删除文件；
  文件=空；

  如果（S.O.（））{
    如果（！）有下一个文件）
      s=status:：corruption（“描述符中没有meta nextfile条目”）；
    否则，（如果）！有日志号）
      s=status:：corruption（“描述符中没有meta lognumber条目”）；
    否则，（如果）！有最后的顺序）
      s=status:：corruption（“描述符中没有最后一个序列号条目”）；
    }

    如果（！）有上一个日志编号）
      上一个日志编号=0；
    }

    markfilenumber已使用（上一个日志编号）；
    markfilenumber used（日志编号）；
  }

  如果（S.O.（））{
    version*v=新版本（this）；
    builder.saveto（v）；
    //安装恢复的版本
    完成（V）；
    附录版本（v）；
    清单文件编号下一个文件；
    下一个文件编号=下一个文件+1；
    最后一个序列=最后一个序列；
    日志编号=日志编号；
    上一个日志编号=上一个日志编号；

    //查看是否可以重用现有清单文件。
    if（ReuseManifest（dscname，current））
      //无需保存新清单
    }否则{
      *保存清单=真；
    }
  }

  返回S；
}

bool版本集：：reusemanifest（const std:：string&dscname，
                               const std：：字符串和dscbase）
  如果（！）选项->重用日志）
    返回错误；
  }
  文件类型清单\u类型；
  uint64清单编号；
  uint64清单大小；
  如果（！）parseFileName（dscbase，&manifest_number，&manifest_type）
      声明类型！=kDescriptorFile
      ！env_u->getfilesize（dscname，&manifest_size）.ok（）
      //如果旧的清单太大，则生成新的压缩清单
      manifest_size>=目标文件大小（选项_uu））
    返回错误；
  }

  断言（descriptor_file_==null）；
  断言（描述符_log_==null）；
  状态R=env_->newAppendableFile（dscname，&descriptor_file_u）；
  如果（！）r（））{
    日志（选项_->info_log，“重用清单：%s\n”，r.toString（）.c_str（））；
    断言（descriptor_file_==null）；
    返回错误；
  }

  日志（选项_->info_log，“重用清单%s\n”，dscname.c_str（））；
  descriptor_log_=new log:：writer（descriptor_file_u，manifest_size）；
  清单文件编号=清单编号；
  回归真实；
}

void versionset:：markfilenumberused（uint64_t number）
  if（下一个_文件_编号_<=编号）
    下一个文件编号=编号+1；
  }
}

void版本集：：完成（版本*v）
  //为下一次压缩预计算的最佳级别
  int最佳级别=-1；
  双倍最佳分数=-1；

  对于（int level=0；level<config:：knumlevels-1；level++）
    双评分；
    如果（水平==0）
      //我们通过限制文件数量来专门处理0级
      //而不是字节数，原因有两个：
      / /
      //（1）写缓冲区大小越大，最好不要这样做
      //许多0级压缩。
      / /
      //（2）0级文件在每次读取时合并，并且
      //因此，我们希望在
      //文件大小很小（可能是因为写缓冲区很小）
      //设置或非常高的压缩比，或大量
      //覆盖/删除）。
      score=v->files_[level].size（）/
          static_cast<double>（配置：：kl0_compactiontrigger）；
    }否则{
      //计算当前大小与大小限制的比率。
      const uint64_t level_bytes=totalfilesize（v->files_[level]）；
      得分=
          static_cast<double>（level_bytes）/maxbytesforlevel（options_u，level）；
    }

    如果（分数>最佳分数）
      最佳水平=水平；
      最佳分数=分数；
    }
  }

  v->压实水平uuu=最佳水平；
  v->compaction_score_uu=最佳_score；
}

状态版本集：：WriteSnapshot（日志：：Writer*日志）
  //TODO:拆分为多个记录以减少恢复时的内存使用量？

  //保存元数据
  版本编辑；
  edit.setComparatorName（icp_u.user_comparator（）->name（））；

  //保存压缩指针
  对于（int level=0；level<config:：knumlevels；level++）
    如果（！）compact_pointer_[level].empty（））
      内部密钥；
      key.decodefrom（compact_pointer_uu[level]）；
      edit.setCompactPointer（级别，键）；
    }
  }

  /保存文件
  对于（int level=0；level<config:：knumlevels；level++）
    const std：：vector<filemetadata*>&files=current_u->files_[level]；
    对于（size_t i=0；i<files.size（）；i++）
      const filemetadata*f=文件[i]；
      edit.addfile（level，f->number，f->file_size，f->minimum，f->maximum）；
    }
  }

  std：：字符串记录；
  编辑.encodeto（&record）；
  返回日志->addrecord（record）；
}

int版本集：：numlevelfiles（int level）const
  断言（级别>=0）；
  断言（level<config:：knumlevels）；
  返回当前_u->files_[level].size（）；
}

const char*版本集：：levelsummary（levelsummarystorage*scratch）const
  //如果knumlevels更改，则更新代码
  断言（config:：knumlevels==7）；
  snprintf（scratch->buffer，sizeof（scratch->buffer）、
           “文件[%d%d%d%d%d%d%d]”，
           int（当前_->files_[0].size（）），
           int（当前_->files_[1].size（）），
           int（当前_->files_[2].size（）），
           int（当前_->files_[3].size（）），
           int（当前_->files_[4].size（）），
           int（当前_->files_[5].size（）），
           int（当前_->files_[6].size（））；
  返回scratch->buffer；
}

uint64_t versionset:：approceoffsetof（version*v，const internalkey&ikey）
  uint64_t结果=0；
  对于（int level=0；level<config:：knumlevels；level++）
    const std：：vector<filemetadata*>&files=v->files_u[level]；
    对于（size_t i=0；i<files.size（）；i++）
      if（icmp_u.compare（文件[i]->最大，ikey）<=0）
        //整个文件在“ikey”之前，因此只需添加文件大小
        结果+=文件[i]->文件大小；
      else if（icmp_uu.compare（files[i]->minimum，ikey）>0）
        //整个文件位于“ikey”之后，因此忽略
        如果（级别>0）
          //0级以外的文件按meta->minimal排序，所以
          //此级别中没有其他文件将包含
          /“IKey”。
          断裂；
        }
      }否则{
        //“ikey”属于此表的范围。添加
        //表中“ikey”的近似偏移量。
        表*tableptr；
        迭代器*iter=table_cache_u->new迭代器（
            readoptions（），文件[i]->编号，文件[i]->文件大小，&tableptr）；
        如果（TabLabTr）！{NULL）{
          result+=tableptr->approxiteoffsetof（ikey.encode（））；
        }
        删除ITER；
      }
    }
  }
  返回结果；
}

void versionset:：addlivefiles（std:：set<uint64_t>*live）
  对于（version*v=dummy_versions_uu.next_uu；
       V！=虚拟版本
       v= v-＞nExt*）{
    对于（int level=0；level<config:：knumlevels；level++）
      const std：：vector<filemetadata*>&files=v->files_u[level]；
      对于（size_t i=0；i<files.size（）；i++）
        live->insert（文件[i]->number）；
      }
    }
  }
}

Int64_t版本集：：numlevelbytes（int level）const_
  断言（级别>=0）；
  断言（level<config:：knumlevels）；
  返回totalfilesize（当前_u->files_u[level]）；
}

Int64_t版本集：：MaxNextLevelOverlappingBytes（）
  Int64_t结果=0；
  std:：vector<filemetadata*>重叠；
  对于（int level=1；level<config:：knumlevels-1；level++）
    对于（size_t i=0；i<current_u->files_[level].size（）；i++）
      const filemetadata*f=当前_u->files_u[level][i]；
      当前_uu->GetOverlappingInputs（级别+1，&f->Minimum，&f->Maximum，
                                     重叠；
      const int64_t sum=总文件大小（重叠）；
      如果（总和>结果）
        结果＝和；
      }
    }
  }
  返回结果；
}

//存储包含输入中所有项的最小范围
//*最小，*最大。
//要求：输入不为空
void versionset:：getrange（const std:：vector<filemetadata*>&inputs，
                          InternalKey*最小，
                          internalkey*最大）
  断言（！）inputs.empty（））；
  最小->清除（）；
  最大->清除（）；
  对于（size_t i=0；i<inputs.size（）；i++）
    filemetadata*f=输入[i]；
    如果（i＝0）{
      *最小=f->最小；
      *最大值=f->最大值；
    }否则{
      if（icmp_uu.compare（f->最小，*最小）<0）
        *最小=f->最小；
      }
      if（icmp_uu.compare（f->maximum，*maximum）>0）
        *最大值=f->最大值；
      }
    }
  }
}

//存储包含inputs1和inputs2中所有项的最小范围
//在*最小，*最大。
//要求：输入不为空
void versionset:：getrange2（const std:：vector<filemetadata*>&inputs1，
                           const std：：vector<filemetadata*>和inputs2，
                           InternalKey*最小，
                           internalkey*最大）
  std:：vector<filemetadata*>all=inputs1；
  all.insert（all.end（），inputs2.begin（），inputs2.end（））；
  获取范围（全部、最小、最大）；
}

迭代器*版本集：：makeInputIterator（compaction*c）
  读数选项；
  options.verify_checksums=options_u->paranoid_checks；
  options.fill_cache=false；

  //0级文件必须合并在一起。对于其他级别，
  //我们将为每个级别创建一个串联迭代器。
  //TODO（opt）：如果没有重叠，则对级别0使用串联迭代器
  const int space=（c->level（）==0？c->inputs_[0].size（）+1:2）；
  迭代器**list=new迭代器*[空格]；
  int num＝0；
  for（int，其中=0；which<2；which++）
    如果（！）C->inputs_[which].empty（））
      if（c->level（）+which==0）
        const std：：vector<filemetadata*>&files=c->inputs_[which]；
        对于（size_t i=0；i<files.size（）；i++）
          list[num++]=表缓存u->newIterator（
              选项，文件[i]->编号，文件[i]->文件大小；
        }
      }否则{
        //为此级别的文件创建串联迭代器
        list[num++]=newtwoleveliterator（
            新版本：：LEVELFileNumIterator（ICMP_uu，&C->INPUTS_u哪个]，
            &getfileiterator，表缓存，选项）；
      }
    }
  }
  断言（num<=空格）；
  迭代器*result=newmerging迭代器（&icp_uu，list，num）；
  删除[]列表；
  返回结果；
}

compaction*版本集：：pickcompaction（）
  压实度C；
  int级；

  //我们更喜欢在一个级别中由太多数据触发的压缩，而不是
  //由seeks触发的压缩。
  const bool size_compaction=（当前_u->compaction_score_>=1）；
  const bool seek_compaction=（当前_u->file_to_compact_u！= NULL）；
  如果（尺寸压实）
    级别=当前_->压实_U级别；
    断言（级别>=0）；
    断言（级别+1<config:：knumlevels）；
    C=新的压实度（选项_uuLevel）；

    //选择压缩指针级别后的第一个文件
    对于（size_t i=0；i<current_u->files_[level].size（）；i++）
      filemetadata*f=current_->files_[level][i]；
      if（compact_pointer_[level].empty（）
          icmp_u.compare（f->maximum.encode（），compact_pointer_uu[level]）>0）
        C->INPUTS_[0].向后推（F）；
        断裂；
      }
    }
    if（c->inputs_[0].empty（））
      //环绕到键空间的开头
      C->inputs_[0].将_向后推（当前_->files_[level][0]）；
    }
  否则，如果（搜索压缩）
    level=current_->file_to_compact_level_；
    C=新的压实度（选项_uuLevel）；
    C->inputs_[0].将_向后推（当前_u->file_到_compact_uuu）；
  }否则{
    返回空；
  }

  C->input_version_uu=current_uu；
  c->input_version_u->ref（）；

  //级别0中的文件可能会相互重叠，因此请选取所有重叠的文件
  如果（水平==0）
    InternalKey最小，最大；
    getrange（c->inputs_[0]，&minimum，&maximum）；
    //请注意，下一个调用将丢弃我们放入的文件
    //c->inputs_[0]之前，将其替换为重叠集
    //这将包括拾取的文件。
    当前_u->getOverlappinginputs（0，&最小，&最大，&c->inputs_u0]）；
    断言（！）C->inputs_[0].empty（））；
  }

  设置其他输入（c）；

  返回C；
}

void versionset：：设置其他输入（compaction*c）
  const int level=c->level（）；
  InternalKey最小，最大；
  getrange（c->inputs_[0]，&minimum，&maximum）；

  当前_u->getOverlappinginputs（级别+1，&最小，&最大，&c->inputs_[1]）；

  //压缩覆盖整个范围
  internalkey所有\开始，所有\限制；
  getrange2（c->inputs_[0]，c->inputs_[1]，&all_start，&all_limit）；

  //看看我们是否可以在没有
  //更改我们接收的“level+1”文件的数量。
  如果（！）C->inputs_[1].empty（））
    std:：vector<filemetadata*>expanded0；
    当前_u->getOverlappingInputs（级别，&all_start，&all_limit，&expanded0）；
    const int64_t inputs0_size=totalfilesize（C->inputs0]）；
    const int64_t inputs1_size=totalfilesize（C->inputs1]）；
    const int64_t expanded0_size=总文件大小（expanded0）；
    if（expanded0.size（）>c->inputs_[0].size（）&&
        输入1_大小+扩展0_大小<
            扩展的compactionBytesizelimit（选项）
      internalkey new_start，new_limit；
      getrange（expanded0，&new_start，&new_limit）；
      std:：vector<filemetadata*>expanded1；
      当前_u->getOverlappingInputs（级别+1，&new_start，&new_limit，
                                     扩展1）；
      if（expanded1.size（）==c->inputs_[1].size（））
        日志（选项_->info_log，
            “正在将@%d%d+%d（%ld+%ld字节）扩展到%d+%d（%ld+%ld字节）\n”，
            水平，
            int（c->inputs_[0].size（）），
            int（c->inputs_[1].size（）），
            长（inputs0_大小），长（inputs1_大小）
            int（expanded0.size（）），
            int（expanded1.size（）），
            长（扩展0_大小），长（输入1_大小））；
        最小=新开始；
        最大值=新的_限制；
        C->inputs_[0]=expanded0；
        C->inputs_[1]=expanded1；
        getrange2（c->inputs_[0]，c->inputs_[1]，&all_start，&all_limit）；
      }
    }
  }

  //计算重叠此压缩的祖父母文件集
  //（父级==级别+1；祖父母==级别+2）
  如果（级别+2<config:：knumlevels）
    当前_u->getOverlappinginputs（级别+2，&all_start，&all_limit，
                                   &c->祖父母；
  }

  如果（false）{
    日志（选项_->info_log，“压缩%d”“%s”…”%s’，
        水平，
        最小的.debugString（）.c_str（），
        最大的.debugString（）.c_str（））；
  }

  //更新此级别下一次压缩的位置。
  //我们会立即更新此版本，而不是等待版本编辑
  //如果压缩失败，我们将尝试
  //下次键范围。
  compact_pointer_[level]=最大.encode（）.toString（）；
  C->edit_.setcompactpointer（级别，最大）；
}

压缩*版本集：：压缩范围（
    int级，
    const internalkey*开始，
    const internalkey*结束）
  std:：vector<filemetadata*>输入；
  当前_u->getOverlappingInputs（级别、开始、结束和输入）；
  if（inputs.empty（））
    返回空；
  }

  //如果距离较大，避免一次压缩过多。
  //但我们不能对0级执行此操作，因为0级文件可以重叠
  //并且如果
  //两个文件重叠。
  如果（级别>0）
    const uint64_t limit=maxfilesizeforlevel（选项_uu，level）；
    uint64总=0；
    对于（size_t i=0；i<inputs.size（）；i++）
      uint64_t s=输入[i]->文件大小；
      总数+= s；
      如果（总数>=极限）
        输入。调整大小（i+1）；
        断裂；
      }
    }
  }

  compaction*c=新的压缩（选项uu，级别）；
  C->input_version_uu=current_uu；
  c->input_version_u->ref（）；
  C->inputs_[0]=输入；
  设置其他输入（c）；
  返回C；
}

压缩：：压缩（const选项*选项，int级别）
    ：级别（级别），
      最大输出文件大小（maxfilesizeforlevel（选项，级别）），
      输入“版本”（空），
      祖父母指数
      看到“键”（假），
      重叠的_字节_（0）
  对于（int i=0；i<config:：knumlevels；i++）
    水平度ptrs_[i]=0；
  }
}

压实度：~compaction（）
  如果（输入“版本”！{NULL）{
    输入_version_u->unref（）；
  }
}

bool compaction:：ISrivaAlmove（）常量
  const versionset*vset=input_version_u->vset_u；
  //如果存在大量重叠的祖父母数据，请避免移动。
  //否则，移动会创建一个需要
  //很昂贵的合并。
  返回（num_input_files（0）==1&&num_input_files（1）==0&&
          总计文件大小（祖父母）<=
              maxgrandarentoverlappbytes（vset->options_u））；
}

void compaction:：addinputDeletions（versionedit*edit）
  for（int，其中=0；which<2；which++）
    对于（size_t i=0；i<inputs_[which].size（）；i++）
      edit->deletefile（level_u+which，inputs_[which][i]->number）；
    }
  }
}

bool compaction：：isbaseLevelWorkey（const slice&user_key）
  //也许使用二进制搜索来查找正确的条目，而不是线性搜索？
  const comparator*user_cmp=input_version_u->vset_->icmp_u.user_comparator（）；
  对于（int lvl=level_2；lvl<config:：knumlevels；lvl++）
    const std:：vector<filemetadata*>&files=input_version_u->files_[lvl]；
    对于（；level_ptrs_[lvl]<files.size（）；）
      filemetadata*f=files[level_ptrs_u[lvl]]；
      if（user_cmp->compare（user_key，f->maximum.user_key（））<=0）
        //我们已经前进了足够远了
        if（user_cmp->compare（user_key，f->minimal.user_key（））>=0）
          //键属于此文件的范围，因此绝对不是基本级别
          返回错误；
        }
        断裂；
      }
      等级_ptrs_u[lvl]++；
    }
  }
  回归真实；
}

bool compaction：：shouldstopbefore（const slice&internal_key）
  const versionset*vset=input_version_u->vset_u；
  //扫描以查找包含密钥的最早的祖父母文件。
  const internalKeyComparator*icmp=&vset->icmp_u；
  同时（祖父母索引
      ICMP->Compare（内部_键，
                    祖父母索引->最大的.encode（））>0）
    如果（见“钥匙”）
      重叠的\u字节\+=祖父母\[祖父母\索引\->文件\大小；
    }
    祖父母指数
  }
  看到了“键”=true；

  if（overlapped_bytes_>maxgrandrentoverlappetbytes（vset->options_uo））
    //电流输出重叠太多；启动新输出
    重叠的_字节_uu0；
    回归真实；
  }否则{
    返回错误；
  }
}

void compaction:：releaseinputs（）
  如果（输入“版本”！{NULL）{
    输入_version_u->unref（）；
    输入_version_uu=null；
  }
}

//命名空间级别db
