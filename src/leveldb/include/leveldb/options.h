
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

#ifndef STORAGE_LEVELDB_INCLUDE_OPTIONS_H_
#define STORAGE_LEVELDB_INCLUDE_OPTIONS_H_

#include <stddef.h>

namespace leveldb {

class Cache;
class Comparator;
class Env;
class FilterPolicy;
class Logger;
class Snapshot;

//数据库内容存储在一组块中，每个块包含一个
//键、值对的序列。每个块可以在
//存储在文件中。以下枚举描述了
//压缩方法（如果有）用于压缩块。
enum CompressionType {
//注意：不要更改现有条目的值，因为它们是
//磁盘上持久格式的一部分。
  kNoCompression     = 0x0,
  kSnappyCompression = 0x1
};

//控制数据库行为的选项（传递给db:：open）
struct Options {
//------------
//影响行为的参数

//比较器用于定义表中键的顺序。
//默认值：使用字典式字节顺序的比较器
//
//要求：客户端必须确保提供的比较器
//这里有相同的名称和顺序键*完全*和
//Comparator提供给同一数据库上以前的打开调用。
  const Comparator* comparator;

//如果为true，则将在缺少数据库时创建该数据库。
//默认：假
  bool create_if_missing;

//如果为true，则在数据库已存在时引发错误。
//默认：假
  bool error_if_exists;

//如果为真，则实现将对
//正在处理的数据，如果检测到任何
//错误。这可能会产生不可预见的后果：例如，
//一个数据库条目的损坏可能导致大量条目
//无法读取或整个数据库无法打开。
//默认：假
  bool paranoid_checks;

//使用指定的对象与环境交互，
//例如读/写文件、安排后台工作等。
//默认值：env:：default（））
  Env* env;

//数据库生成的任何内部进度/错误信息将
//如果不为空，则写入信息日志，或写入存储的文件
//如果信息日志为空，则与数据库内容位于同一目录中。
//默认值：空
  Logger* info_log;

//------------
//影响性能的参数

//要在内存中累积的数据量（由未排序的日志备份）
//在磁盘上）转换为按磁盘排序的文件之前。
//
//较大的值会提高性能，尤其是在大容量加载期间。
//内存中最多可以同时保存两个写缓冲区，
//所以您可能希望调整这个参数来控制内存的使用。
//此外，较大的写缓冲区将导致较长的恢复时间
//下次打开数据库时。
//
//默认值：4MB
  size_t write_buffer_size;

//数据库可以使用的打开文件数。你可能需要
//如果数据库有较大的工作集（预算
//每2MB工作集打开一个文件）。
//
//默认值：1000
  int max_open_files;

//控制块（用户数据存储在一组块中，以及
//块是从磁盘读取的单位）。

//如果非空，则对块使用指定的缓存。
//如果为空，则leveldb将自动创建并使用8MB内部缓存。
//默认值：空
  Cache* block_cache;

//每个块压缩的用户数据的近似大小。请注意
//此处指定的块大小对应于未压缩的数据。这个
//如果
//压缩已启用。此参数可以动态更改。
//
//默认值：4K
  size_t block_size;

//键的增量编码的重新启动点之间的键数。
//此参数可以动态更改。大多数客户应该
//不要使用此参数。
//
//默认值：16
  int block_restart_interval;

//LEVELDB将向一个文件写入此数量的字节。
//换一个新的。
//大多数客户机应该只保留这个参数。但是如果你
//文件系统对于较大的文件更有效，您可以
//考虑增加值。下跌的时间会更长
//压缩，从而延长延迟/性能中断。
//增加此参数的另一个原因可能是
//最初填充大型数据库。
//
//默认值：2MB
  size_t max_file_size;

//使用指定的压缩算法压缩块。这个
//参数可以动态更改。
//
//默认值：ksnappycompression，它提供轻量但快速的
//压缩。
//
//在Intel（R）Core（TM）2 2.4GHz上ksnapycompression的典型速度：
//~200-500MB/s压缩
//~400-800MB/s解压
//请注意，这些速度明显快于大多数
//持久的存储速度，因此通常不会
//值得切换到knocompression。即使输入数据是
//不可压缩，ksnappycompression实现将
//有效检测并切换到未压缩模式。
  CompressionType compression;

//实验：如果为真，则附加到现有清单和日志文件
//当数据库打开时。这可以显著加快打开速度。
//
//默认值：当前为假，但稍后可能变为真。
  bool reuse_logs;

//如果非空，请使用指定的筛选策略减少磁盘读取。
//许多应用程序将受益于传递
//这里是newbloomfilterpolicy（）。
//
//默认值：空
  const FilterPolicy* filter_policy;

//使用所有字段的默认值创建选项对象。
  Options();
};

//控制读取操作的选项
struct ReadOptions {
//如果为true，则从基础存储中读取的所有数据将
//根据相应的校验和进行验证。
//默认：假
  bool verify_checksums;

//是否应将为此迭代读取的数据缓存在内存中？
//对于批量扫描，调用方可能希望将此字段设置为false。
//默认值：真
  bool fill_cache;

//如果“快照”非空，则从提供的快照开始读取
//（必须属于正在读取的数据库，并且必须
//尚未发布）。如果“快照”为空，则使用隐式
//此读取操作开始时的状态快照。
//默认值：空
  const Snapshot* snapshot;

  ReadOptions()
      : verify_checksums(false),
        fill_cache(true),
        snapshot(NULL) {
  }
};

//控制写入操作的选项
struct WriteOptions {
//如果为真，则将从操作系统中刷新写入。
//缓冲区缓存（通过在写入之前调用writablefile:：sync（））
//被认为是完整的。如果此标志为真，则写入将
//更慢的。
//
//如果此标志为假，并且机器崩溃，则
//写入可能丢失。注意，如果只是这个过程，
//崩溃（即机器不重新启动），不会写入
//即使sync==false也会丢失。
//
//换句话说，sync==false的db write与
//作为“write（）”系统调用的崩溃语义。DB写
//with sync==true与“write（）”具有相似的崩溃语义。
//系统调用后跟“fsync（）”。
//
//默认：假
  bool sync;

  WriteOptions()
      : sync(false) {
  }
};

}  //命名空间级别数据库

#endif  //存储级别包括选项
