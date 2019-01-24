
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

#ifndef STORAGE_LEVELDB_DB_DB_IMPL_H_
#define STORAGE_LEVELDB_DB_DB_IMPL_H_

#include <deque>
#include <set>
#include "db/dbformat.h"
#include "db/log_writer.h"
#include "db/snapshot.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "port/port.h"
#include "port/thread_annotations.h"

namespace leveldb {

class MemTable;
class TableCache;
class Version;
class VersionEdit;
class VersionSet;

class DBImpl : public DB {
 public:
  DBImpl(const Options& options, const std::string& dbname);
  virtual ~DBImpl();

//DB接口的实现
  virtual Status Put(const WriteOptions&, const Slice& key, const Slice& value);
  virtual Status Delete(const WriteOptions&, const Slice& key);
  virtual Status Write(const WriteOptions& options, WriteBatch* updates);
  virtual Status Get(const ReadOptions& options,
                     const Slice& key,
                     std::string* value);
  virtual Iterator* NewIterator(const ReadOptions&);
  virtual const Snapshot* GetSnapshot();
  virtual void ReleaseSnapshot(const Snapshot* snapshot);
  virtual bool GetProperty(const Slice& property, std::string* value);
  virtual void GetApproximateSizes(const Range* range, int n, uint64_t* sizes);
  virtual void CompactRange(const Slice* begin, const Slice* end);

//不在公共数据库接口中的额外方法（用于测试）

//压缩命名级别中重叠的任何文件[*开始，*结束]
  void TEST_CompactRange(int level, const Slice* begin, const Slice* end);

//强制当前内存表内容被压缩。
  Status TEST_CompactMemTable();

//返回数据库当前状态的内部迭代器。
//这个迭代器的键是内部键（参见format.h）。
//当不再需要时，应删除返回的迭代器。
  Iterator* TEST_NewInternalIterator();

//返回下一级任意
//文件级别大于等于1。
  int64_t TEST_MaxNextLevelOverlappingBytes();

//记录在指定的内部键处读取的字节样本。
//每个config:：kreabytespeid大约采集一次样本
//字节。
  void RecordReadSample(Slice key);

 private:
  friend class DB;
  struct CompactionState;
  struct Writer;

  Iterator* NewInternalIterator(const ReadOptions&,
                                SequenceNumber* latest_snapshot,
                                uint32_t* seed);

  Status NewDB();

//从持久存储中恢复描述符。可能会有重大影响
//要恢复最近记录的更新的工作量。任何改变
//添加到*编辑中。
  Status Recover(VersionEdit* edit, bool* save_manifest)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void MaybeIgnoreError(Status* s) const;

//删除所有不需要的文件和过时的内存条目。
  void DeleteObsoleteFiles();

//将内存中的写缓冲区压缩到磁盘。切换到新的
//记录文件/memtable并成功写入新的描述符iff。
//错误记录在bg_错误_u中。
  void CompactMemTable() EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Status RecoverLogFile(uint64_t log_number, bool last_log, bool* save_manifest,
                        VersionEdit* edit, SequenceNumber* max_sequence)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Status WriteLevel0Table(MemTable* mem, VersionEdit* edit, Version* base)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  /*tus makeroomforwrite（bool force/*紧凑型，即使有空间？*/）
      需要专用锁（mutex）；
  writebatch*buildbatchgroup（writer**最后一个写入程序）；

  void recordbackgrounderror（const status&s）；

  void maybescheduleCompaction（）需要独占锁（mutex）；
  静态空隙bgwork（空隙*db）；
  void backgroundCall（）；
  void backgroundcompaction（）需要唯一的锁（mutex）；
  空隙清理压实（压实状态*压实）
      需要专用锁（mutex）；
  状态文档CompactionWork（CompactionState*Compact）
      需要专用锁（mutex）；

  状态OpenCompactionOutputfile（CompactionState*Compact）；
  状态FinishCompactionOutputfile（CompactionState*Compact，Iterator*输入）；
  状态InstallCompactionResults（CompactionState*Compact）
      需要专用锁（mutex）；

  //构造后常量
  环境常数
  常量InternalKeyComparator Internal_Comparator_；
  const internal filter policy内部过滤器策略
  const options options_；//options_.comparator==内部_comparator_
  布尔拥有“信息日志”；
  布尔拥有“缓存”；
  const std：：字符串dbname_u；

  //表缓存提供自己的同步
  table cache*表缓存

  //锁定持久数据库状态。成功获取非空IFF。
  文件锁*数据库锁

  //下面的状态受mutex保护\u
  端口：：mutex mutex_uu；
  端口：：AtomicPointer正在关闭
  端口：：condvar bg_cv_；//后台工作完成时发出信号
  可记忆的*
  memtable*imm_；//正在压缩的memtable
  端口：：atomicpointer有\u imm_u；//所以bg线程可以检测到非空imm_
  可写文件*日志文件
  uint64日志文件编号；
  日志：：writer*日志
  uint32_t seed_u；//用于采样。

  //写入程序队列。
  std:：deque<writer*>writers_uux；
  writebatch*tmp_批处理

  快照列表快照

  //要防止删除的表文件集，因为它们是
  //正在进行的压缩的一部分。
  std:：set<uint64_t>pending_outputs_uuu；

  //后台压缩是否已计划或正在运行？
  bool bg_compaction_scheduled_uu；

  //手动压缩的信息
  结构手动压实
    int级；
    博尔完成；
    const internalkey*begin；//空表示键范围的开始
    const internalkey*end；//空表示键范围结束
    internalkey tmp_storage；//用于跟踪压缩进度
  }；
  手动压实*手动压实；

  版本集*版本

  //我们在偏执模式下遇到背景错误了吗？
  状态bg_错误

  //每级压缩统计。stats存储
  //为指定的“级别”生成数据的压缩。
  结构紧凑状态
    64微米；
    Int64字节读取；
    Int64字节已写入；

    compactionstats（）：micros（0），bytes_read（0），bytes_written（0）

    void add（const compactionstats&c）
      这->micros+=c.micros；
      这->bytes_read+=c.bytes_read；
      这->bytes_written+=c.bytes_written；
    }
  }；
  compactionstats stats_u[config:：knumlevels]；

  //不允许复制
  dbimpl（const dbimpl&）；
  void运算符=（const dbimpl&）；

  const comparator*用户_comparator（）const_
    返回内部_comparator_u.user_comparator（）；
  }
}；

//清理数据库选项。如果
//不等于src.info_log。
外部选项清理选项（const std:：string&db，
                               常量InternalKeyComparator*ICMP，
                               const internalfilterpolicy*ipolicy，
                               const选项&src）；

//命名空间级别db

endif//存储级别db_db_db_impl_h_
