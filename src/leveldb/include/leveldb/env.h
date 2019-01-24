
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
//env是leveldb实现用来访问的接口
//操作系统功能，如文件系统等调用程序
//打开数据库时可能希望提供自定义env对象
//获得精细的增益控制；例如，速率限制文件系统操作。
//
//所有env实现对来自的并发访问都是安全的。
//多线程，没有任何外部同步。

#ifndef STORAGE_LEVELDB_INCLUDE_ENV_H_
#define STORAGE_LEVELDB_INCLUDE_ENV_H_

#include <string>
#include <vector>
#include <stdarg.h>
#include <stdint.h>
#include "leveldb/status.h"

namespace leveldb {

class FileLock;
class Logger;
class RandomAccessFile;
class SequentialFile;
class Slice;
class WritableFile;

class Env {
 public:
  Env() { }
  virtual ~Env();

//返回适合当前操作的默认环境
//系统。成熟的用户可能希望提供自己的环境
//实现而不是依赖于此默认环境。
//
//default（）的结果属于leveldb，不能删除。
  static Env* Default();

//用指定的名称创建一个全新的顺序可读文件。
//成功后，将指向新文件的指针存储在*result中，并返回OK。
//失败时，将空值存储在*result中，并返回非OK。如果文件有
//不存在，返回非OK状态。
//
//返回的文件一次只能由一个线程访问。
  virtual Status NewSequentialFile(const std::string& fname,
                                   SequentialFile** result) = 0;

//创建一个全新的随机访问只读文件
//指定的名称。成功后，将指向新文件的指针存储在
//*结果并返回OK。失败时，在*result和
//返回非OK。如果文件不存在，则返回一个非OK
//状态。
//
//多个线程可以同时访问返回的文件。
  virtual Status NewRandomAccessFile(const std::string& fname,
                                     RandomAccessFile** result) = 0;

//创建一个对象，该对象使用指定的
//姓名。删除具有相同名称的任何现有文件并创建
//新文件。成功后，将指向新文件的指针存储在
//*结果并返回OK。失败时，在*result和
//返回非OK。
//
//返回的文件一次只能由一个线程访问。
  virtual Status NewWritableFile(const std::string& fname,
                                 WritableFile** result) = 0;

//创建一个附加到现有文件的对象，或者
//写入新文件（如果该文件不存在，则以开头）。
//成功后，将指向新文件的指针存储在*result和
//返回OK。失败时，在*result中存储空值并返回
//非OK。
//
//返回的文件一次只能由一个线程访问。
//
//如果此env返回isNotSupportedError错误
//不允许附加到现有文件。env的用户（包括
//必须准备好处理
//不支持附加的env。
  virtual Status NewAppendableFile(const std::string& fname,
                                   WritableFile** result);

//如果命名文件存在，则返回true。
  virtual bool FileExists(const std::string& fname) = 0;

//将指定目录的子目录名存储在*result中。
//这些名称是相对于“dir”的。
//*结果的原始内容将被删除。
  virtual Status GetChildren(const std::string& dir,
                             std::vector<std::string>* result) = 0;

//删除命名文件。
  virtual Status DeleteFile(const std::string& fname) = 0;

//创建指定的目录。
  virtual Status CreateDir(const std::string& dirname) = 0;

//删除指定的目录。
  virtual Status DeleteDir(const std::string& dirname) = 0;

//将fname的大小存储在*文件\大小中。
  virtual Status GetFileSize(const std::string& fname, uint64_t* file_size) = 0;

//将文件src重命名为target。
  virtual Status RenameFile(const std::string& src,
                            const std::string& target) = 0;

//锁定指定的文件。用于防止同时访问
//通过多个进程访问同一数据库。失败时，将空存储在
//*锁定并返回非OK。
//
//成功时，存储指向表示
//获得锁定*锁定并返回OK。打电话的人应该打电话
//解锁文件（*lock）以释放锁。如果进程退出，
//锁将自动释放。
//
//如果有人已经锁上了锁，立即结束
//失败了。即，此调用不等待现有锁
//走开。
//
//如果命名文件不存在，则可以创建它。
  virtual Status LockFile(const std::string& fname, FileLock** lock) = 0;

//释放先前成功调用lockfile获得的锁。
//要求：成功的lockfile（）调用返回了lock
//要求：锁尚未解锁。
  virtual Status UnlockFile(FileLock* lock) = 0;

//安排在后台线程中运行一次“（*function）（arg）”。
//
//“函数”可以在未指定的线程中运行。多功能
//添加到同一个env可以在不同的线程中并发运行。
//也就是说，调用者不能假定后台工作项是
//序列化。
  virtual void Schedule(
      void (*function)(void* arg),
      void* arg) = 0;

//启动一个新线程，在新线程中调用“函数（arg）”。
//当“函数（arg）”返回时，线程将被破坏。
  virtual void StartThread(void (*function)(void* arg), void* arg) = 0;

//*路径设置为可用于测试的临时目录。它可能
//或者有许多还没有被创造出来。目录可能不同，也可能不同
//在同一进程的运行之间，但随后的调用将返回
//相同的目录。
  virtual Status GetTestDirectory(std::string* path) = 0;

//创建并返回用于存储信息消息的日志文件。
  virtual Status NewLogger(const std::string& fname, Logger** result) = 0;

//返回自某个固定时间点以来的微秒数。只有
//对于计算时间的增量很有用。
  virtual uint64_t NowMicros() = 0;

//休眠/将线程延迟规定的微秒数。
  virtual void SleepForMicroseconds(int micros) = 0;

 private:
//不允许复制
  Env(const Env&);
  void operator=(const Env&);
};

//用于按顺序读取文件的文件抽象
class SequentialFile {
 public:
  SequentialFile() { }
  virtual ~SequentialFile();

//从文件中读取最多“n”个字节。划痕[0..n-1]“可能是
//由这个程序编写。将“*result”设置为
//读取（包括成功读取少于“n”个字节）。
//可以将“*result”设置为指向“scratch[0..n-1]”中的数据，因此
//使用“*result”时，“scratch[0..n-1]”必须有效。
//如果遇到错误，则返回非OK状态。
//
//要求：外部同步
  virtual Status Read(size_t n, Slice* result, char* scratch) = 0;

//跳过文件中的“n”字节。这肯定不是
//读取相同数据的速度较慢，但可能更快。
//
//如果到达文件结尾，跳过将在
//文件，跳过将返回OK。
//
//要求：外部同步
  virtual Status Skip(uint64_t n) = 0;

//获取文件名，仅用于错误报告
  virtual std::string GetName() const = 0;

 private:
//不允许复制
  SequentialFile(const SequentialFile&);
  void operator=(const SequentialFile&);
};

//随机读取文件内容的文件抽象。
class RandomAccessFile {
 public:
  RandomAccessFile() { }
  virtual ~RandomAccessFile();

//从文件中读取最多“n”个字节，从“offset”开始。
//“scratch[0..n-1]”可由该例程写入。设置“*结果”
//读取的数据（包括如果小于“n”字节
//读取成功）。可以将“*result”设置为指向
//“scratch[0..n-1]”，因此“scratch[0..n-1]”必须在
//使用“*结果”。如果遇到错误，返回一个非OK
//状态。
//
//多线程并发使用安全。
  virtual Status Read(uint64_t offset, size_t n, Slice* result,
                      char* scratch) const = 0;

//获取文件名，仅用于错误报告
  virtual std::string GetName() const = 0;

 private:
//不允许复制
  RandomAccessFile(const RandomAccessFile&);
  void operator=(const RandomAccessFile&);
};

//用于顺序写入的文件抽象。实施
//必须提供缓冲，因为调用方可能附加小片段
//一次到文件。
class WritableFile {
 public:
  WritableFile() { }
  virtual ~WritableFile();

  virtual Status Append(const Slice& data) = 0;
  virtual Status Close() = 0;
  virtual Status Flush() = 0;
  virtual Status Sync() = 0;

//获取文件名，仅用于错误报告
  virtual std::string GetName() const = 0;

 private:
//不允许复制
  WritableFile(const WritableFile&);
  void operator=(const WritableFile&);
};

//用于写入日志消息的接口。
class Logger {
 public:
  Logger() { }
  virtual ~Logger();

//用指定的格式将条目写入日志文件。
  virtual void Logv(const char* format, va_list ap) = 0;

 private:
//不允许复制
  Logger(const Logger&);
  void operator=(const Logger&);
};


//标识锁定的文件。
class FileLock {
 public:
  FileLock() { }
  virtual ~FileLock();
 private:
//不允许复制
  FileLock(const FileLock&);
  void operator=(const FileLock&);
};

//如果信息日志非空，则将指定的数据记录到*信息日志。
extern void Log(Logger* info_log, const char* format, ...)
#   if defined(__GNUC__) || defined(__clang__)
    __attribute__((__format__ (__printf__, 2, 3)))
#   endif
    ;

//实用程序例程：将“数据”写入命名文件。
extern Status WriteStringToFile(Env* env, const Slice& data,
                                const std::string& fname);

//实用程序例程：将命名文件的内容读取到*数据中
extern Status ReadFileToString(Env* env, const std::string& fname,
                               std::string* data);

//将所有调用转发给另一个env的env实现。
//对于希望覆盖
//另一个环境的功能。
class EnvWrapper : public Env {
 public:
//初始化将所有调用委托给*T的envWrapper
  explicit EnvWrapper(Env* t) : target_(t) { }
  virtual ~EnvWrapper();

//返回此env将所有调用转发到的目标
  Env* target() const { return target_; }

//下面的文本是将所有方法转发到target（）的样板文件。
  Status NewSequentialFile(const std::string& f, SequentialFile** r) {
    return target_->NewSequentialFile(f, r);
  }
  Status NewRandomAccessFile(const std::string& f, RandomAccessFile** r) {
    return target_->NewRandomAccessFile(f, r);
  }
  Status NewWritableFile(const std::string& f, WritableFile** r) {
    return target_->NewWritableFile(f, r);
  }
  Status NewAppendableFile(const std::string& f, WritableFile** r) {
    return target_->NewAppendableFile(f, r);
  }
  bool FileExists(const std::string& f) { return target_->FileExists(f); }
  Status GetChildren(const std::string& dir, std::vector<std::string>* r) {
    return target_->GetChildren(dir, r);
  }
  Status DeleteFile(const std::string& f) { return target_->DeleteFile(f); }
  Status CreateDir(const std::string& d) { return target_->CreateDir(d); }
  Status DeleteDir(const std::string& d) { return target_->DeleteDir(d); }
  Status GetFileSize(const std::string& f, uint64_t* s) {
    return target_->GetFileSize(f, s);
  }
  Status RenameFile(const std::string& s, const std::string& t) {
    return target_->RenameFile(s, t);
  }
  Status LockFile(const std::string& f, FileLock** l) {
    return target_->LockFile(f, l);
  }
  Status UnlockFile(FileLock* l) { return target_->UnlockFile(l); }
  void Schedule(void (*f)(void*), void* a) {
    return target_->Schedule(f, a);
  }
  void StartThread(void (*f)(void*), void* a) {
    return target_->StartThread(f, a);
  }
  virtual Status GetTestDirectory(std::string* path) {
    return target_->GetTestDirectory(path);
  }
  virtual Status NewLogger(const std::string& fname, Logger** result) {
    return target_->NewLogger(fname, result);
  }
  uint64_t NowMicros() {
    return target_->NowMicros();
  }
  void SleepForMicroseconds(int micros) {
    target_->SleepForMicroseconds(micros);
  }
 private:
  Env* target_;
};

}  //命名空间级别数据库

#endif  //存储级别包括
