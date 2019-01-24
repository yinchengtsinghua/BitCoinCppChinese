
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

#include "helpers/memenv/memenv.h"

#include "leveldb/env.h"
#include "leveldb/status.h"
#include "port/port.h"
#include "util/mutexlock.h"
#include <map>
#include <string.h>
#include <string>
#include <vector>

namespace leveldb {

namespace {

class FileState {
 public:
//文件状态被引用计数。初始参考计数为零
//调用方必须至少调用一次ref（）。
  FileState() : refs_(0), size_(0) {}

//增加引用计数。
  void Ref() {
    MutexLock lock(&refs_mutex_);
    ++refs_;
  }

//减少引用计数。如果这是最后一个引用，请删除。
  void Unref() {
    bool do_delete = false;

    {
      MutexLock lock(&refs_mutex_);
      --refs_;
      assert(refs_ >= 0);
      if (refs_ <= 0) {
        do_delete = true;
      }
    }

    if (do_delete) {
      delete this;
    }
  }

  uint64_t Size() const { return size_; }

  Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) const {
    if (offset > size_) {
      return Status::IOError("Offset greater than file size.");
    }
    const uint64_t available = size_ - offset;
    if (n > available) {
      n = static_cast<size_t>(available);
    }
    if (n == 0) {
      *result = Slice();
      return Status::OK();
    }

    assert(offset / kBlockSize <= SIZE_MAX);
    size_t block = static_cast<size_t>(offset / kBlockSize);
    size_t block_offset = offset % kBlockSize;

    if (n <= kBlockSize - block_offset) {
//请求的字节都在第一个块中。
      *result = Slice(blocks_[block] + block_offset, n);
      return Status::OK();
    }

    size_t bytes_to_copy = n;
    char* dst = scratch;

    while (bytes_to_copy > 0) {
      size_t avail = kBlockSize - block_offset;
      if (avail > bytes_to_copy) {
        avail = bytes_to_copy;
      }
      memcpy(dst, blocks_[block] + block_offset, avail);

      bytes_to_copy -= avail;
      dst += avail;
      block++;
      block_offset = 0;
    }

    *result = Slice(scratch, n);
    return Status::OK();
  }

  Status Append(const Slice& data) {
    const char* src = data.data();
    size_t src_len = data.size();

    while (src_len > 0) {
      size_t avail;
      size_t offset = size_ % kBlockSize;

      if (offset != 0) {
//最后一个街区有个房间。
        avail = kBlockSize - offset;
      } else {
//最后一个街区没有房间；推新的。
        blocks_.push_back(new char[kBlockSize]);
        avail = kBlockSize;
      }

      if (avail > src_len) {
        avail = src_len;
      }
      memcpy(blocks_.back() + offset, src, avail);
      src_len -= avail;
      src += avail;
      size_ += avail;
    }

    return Status::OK();
  }

 private:
//private，因为只应使用unref（）删除它。
  ~FileState() {
    for (std::vector<char*>::iterator i = blocks_.begin(); i != blocks_.end();
         ++i) {
      delete [] *i;
    }
  }

//不允许复制。
  FileState(const FileState&);
  void operator=(const FileState&);

  port::Mutex refs_mutex_;
int refs_;  //由refs_mutex_u保护；

//以下字段不受任何互斥体的保护。它们只是可变的
//在写入文件时，不允许并发访问
//到可写文件。
  std::vector<char*> blocks_;
  uint64_t size_;

  enum { kBlockSize = 8 * 1024 };
};

class SequentialFileImpl : public SequentialFile {
 public:
  explicit SequentialFileImpl(FileState* file) : file_(file), pos_(0) {
    file_->Ref();
  }

  ~SequentialFileImpl() {
    file_->Unref();
  }

  virtual Status Read(size_t n, Slice* result, char* scratch) {
    Status s = file_->Read(pos_, n, result, scratch);
    if (s.ok()) {
      pos_ += result->size();
    }
    return s;
  }

  virtual Status Skip(uint64_t n) {
    if (pos_ > file_->Size()) {
      return Status::IOError("pos_ > file_->Size()");
    }
    const uint64_t available = file_->Size() - pos_;
    if (n > available) {
      n = available;
    }
    pos_ += n;
    return Status::OK();
  }

  virtual std::string GetName() const { return "[memenv]"; }
 private:
  FileState* file_;
  uint64_t pos_;
};

class RandomAccessFileImpl : public RandomAccessFile {
 public:
  explicit RandomAccessFileImpl(FileState* file) : file_(file) {
    file_->Ref();
  }

  ~RandomAccessFileImpl() {
    file_->Unref();
  }

  virtual Status Read(uint64_t offset, size_t n, Slice* result,
                      char* scratch) const {
    return file_->Read(offset, n, result, scratch);
  }

  virtual std::string GetName() const { return "[memenv]"; }
 private:
  FileState* file_;
};

class WritableFileImpl : public WritableFile {
 public:
  WritableFileImpl(FileState* file) : file_(file) {
    file_->Ref();
  }

  ~WritableFileImpl() {
    file_->Unref();
  }

  virtual Status Append(const Slice& data) {
    return file_->Append(data);
  }

  virtual Status Close() { return Status::OK(); }
  virtual Status Flush() { return Status::OK(); }
  virtual Status Sync() { return Status::OK(); }

  virtual std::string GetName() const { return "[memenv]"; }
 private:
  FileState* file_;
};

class NoOpLogger : public Logger {
 public:
  virtual void Logv(const char* format, va_list ap) { }
};

class InMemoryEnv : public EnvWrapper {
 public:
  explicit InMemoryEnv(Env* base_env) : EnvWrapper(base_env) { }

  virtual ~InMemoryEnv() {
    for (FileSystem::iterator i = file_map_.begin(); i != file_map_.end(); ++i){
      i->second->Unref();
    }
  }

//env接口的部分实现。
  virtual Status NewSequentialFile(const std::string& fname,
                                   SequentialFile** result) {
    MutexLock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      *result = NULL;
      return Status::IOError(fname, "File not found");
    }

    *result = new SequentialFileImpl(file_map_[fname]);
    return Status::OK();
  }

  virtual Status NewRandomAccessFile(const std::string& fname,
                                     RandomAccessFile** result) {
    MutexLock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      *result = NULL;
      return Status::IOError(fname, "File not found");
    }

    *result = new RandomAccessFileImpl(file_map_[fname]);
    return Status::OK();
  }

  virtual Status NewWritableFile(const std::string& fname,
                                 WritableFile** result) {
    MutexLock lock(&mutex_);
    if (file_map_.find(fname) != file_map_.end()) {
      DeleteFileInternal(fname);
    }

    FileState* file = new FileState();
    file->Ref();
    file_map_[fname] = file;

    *result = new WritableFileImpl(file);
    return Status::OK();
  }

  virtual Status NewAppendableFile(const std::string& fname,
                                   WritableFile** result) {
    MutexLock lock(&mutex_);
    FileState** sptr = &file_map_[fname];
    FileState* file = *sptr;
    if (file == NULL) {
      file = new FileState();
      file->Ref();
    }
    *result = new WritableFileImpl(file);
    return Status::OK();
  }

  virtual bool FileExists(const std::string& fname) {
    MutexLock lock(&mutex_);
    return file_map_.find(fname) != file_map_.end();
  }

  virtual Status GetChildren(const std::string& dir,
                             std::vector<std::string>* result) {
    MutexLock lock(&mutex_);
    result->clear();

    for (FileSystem::iterator i = file_map_.begin(); i != file_map_.end(); ++i){
      const std::string& filename = i->first;

      if (filename.size() >= dir.size() + 1 && filename[dir.size()] == '/' &&
          Slice(filename).starts_with(Slice(dir))) {
        result->push_back(filename.substr(dir.size() + 1));
      }
    }

    return Status::OK();
  }

  void DeleteFileInternal(const std::string& fname) {
    if (file_map_.find(fname) == file_map_.end()) {
      return;
    }

    file_map_[fname]->Unref();
    file_map_.erase(fname);
  }

  virtual Status DeleteFile(const std::string& fname) {
    MutexLock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      return Status::IOError(fname, "File not found");
    }

    DeleteFileInternal(fname);
    return Status::OK();
  }

  virtual Status CreateDir(const std::string& dirname) {
    return Status::OK();
  }

  virtual Status DeleteDir(const std::string& dirname) {
    return Status::OK();
  }

  virtual Status GetFileSize(const std::string& fname, uint64_t* file_size) {
    MutexLock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      return Status::IOError(fname, "File not found");
    }

    *file_size = file_map_[fname]->Size();
    return Status::OK();
  }

  virtual Status RenameFile(const std::string& src,
                            const std::string& target) {
    MutexLock lock(&mutex_);
    if (file_map_.find(src) == file_map_.end()) {
      return Status::IOError(src, "File not found");
    }

    DeleteFileInternal(target);
    file_map_[target] = file_map_[src];
    file_map_.erase(src);
    return Status::OK();
  }

  virtual Status LockFile(const std::string& fname, FileLock** lock) {
    *lock = new FileLock;
    return Status::OK();
  }

  virtual Status UnlockFile(FileLock* lock) {
    delete lock;
    return Status::OK();
  }

  virtual Status GetTestDirectory(std::string* path) {
    *path = "/test";
    return Status::OK();
  }

  virtual Status NewLogger(const std::string& fname, Logger** result) {
    *result = new NoOpLogger;
    return Status::OK();
  }

 private:
//从文件名映射到文件状态对象，表示一个简单的文件系统。
  typedef std::map<std::string, FileState*> FileSystem;
  port::Mutex mutex_;
FileSystem file_map_;  //由互斥保护。
};

}  //命名空间

Env* NewMemEnv(Env* base_env) {
  return new InMemoryEnv(base_env);
}

}  //命名空间级别数据库
