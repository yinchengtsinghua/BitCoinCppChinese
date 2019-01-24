
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//LevelDB版权所有（c）2011 LevelDB作者。版权所有。
//此源代码的使用受可以
//在许可证文件中找到。有关参与者的名称，请参阅作者文件。
//
//有关以下类型/功能的文档，请参阅port_example.h。

//以源和二进制形式重新分配和使用，有或无
//如果满足以下条件，则允许进行修改：
//
//*源代码的再分配必须保留上述版权。
//注意，此条件列表和以下免责声明。
//*二进制形式的再分配必须复制上述版权。
//注意，此条件列表和以下免责声明
//分发时提供的文件和/或其他材料。
//*加州大学伯克利分校和
//其贡献者的姓名可用于支持或推广产品。
//未经事先书面许可，从本软件派生。
//
//此软件由摄政者和贡献者“原样”和任何
//明示或默示保证，包括但不限于
//对适销性和特定用途适用性的保证是
//否认的在任何情况下，摄政者和投稿者对
//直接、间接、附带、特殊、惩戒性或后果性损害
//（包括但不限于替代货物或服务的采购；
//使用、数据或利润的损失；或业务中断），但造成的
//任何责任理论，无论是合同责任、严格责任还是侵权责任。
//（包括疏忽或其他）因使用本文件而引起的
//软件，即使被告知有可能发生此类损坏。
//

#ifndef STORAGE_LEVELDB_PORT_PORT_WIN_H_
#define STORAGE_LEVELDB_PORT_PORT_WIN_H_

#ifdef _MSC_VER
#define snprintf _snprintf
#define close _close
#define fread_unlocked _fread_nolock
#endif

#include <string>
#include <stdint.h>
#ifdef SNAPPY
#include <snappy.h>
#endif

namespace leveldb {
namespace port {

//Windows是小endian（现在：p）
static const bool kLittleEndian = true;

class CondVar;

class Mutex {
 public:
  Mutex();
  ~Mutex();

  void Lock();
  void Unlock();
  void AssertHeld();

 private:
  friend class CondVar;
//关键部分比互斥体更有效
//但它们不是递归的，只能用于同步同一进程中的线程
//我们使用不透明的空隙*以避免在端口\win.h中包含windows.h。
  void * cs_;

//无复制
  Mutex(const Mutex&);
  void operator=(const Mutex&);
};

//Win32 API提供了可靠的条件变量机制，但仅从
//Windows 2008和Vista
//不管我们将用信号量实现我们自己的条件变量
//在安德鲁D.比雷尔2003年写的一篇论文中描述的实施
class CondVar {
 public:
  explicit CondVar(Mutex* mu);
  ~CondVar();
  void Wait();
  void Signal();
  void SignalAll();
 private:
  Mutex* mu_;
  
  Mutex wait_mtx_;
  long waiting_;
  
  void * sem1_;
  void * sem2_;
  
  
};

class OnceType {
public:
//onceType（）：初始化（false）
    OnceType(const OnceType &once) : init_(once.init_) {}
    OnceType(bool f) : init_(f) {}
    void InitOnce(void (*initializer)()) {
        mutex_.Lock();
        if (!init_) {
            init_ = true;
            initializer();
        }
        mutex_.Unlock();
    }

private:
    bool init_;
    Mutex mutex_;
};

#define LEVELDB_ONCE_INIT false
extern void InitOnce(port::OnceType*, void (*initializer)());

//无锁指针的存储
class AtomicPointer {
 private:
  void * rep_;
 public:
  AtomicPointer() : rep_(NULL) { }
  explicit AtomicPointer(void* v); 
  void* Acquire_Load() const;

  void Release_Store(void* v);

  void* NoBarrier_Load() const;

  void NoBarrier_Store(void* v);
};

inline bool Snappy_Compress(const char* input, size_t length,
                            ::std::string* output) {
#ifdef SNAPPY
  output->resize(snappy::MaxCompressedLength(length));
  size_t outlen;
  snappy::RawCompress(input, length, &(*output)[0], &outlen);
  output->resize(outlen);
  return true;
#endif

  return false;
}

inline bool Snappy_GetUncompressedLength(const char* input, size_t length,
                                         size_t* result) {
#ifdef SNAPPY
  return snappy::GetUncompressedLength(input, length, result);
#else
  return false;
#endif
}

inline bool Snappy_Uncompress(const char* input, size_t length,
                              char* output) {
#ifdef SNAPPY
  return snappy::RawUncompress(input, length, output);
#else
  return false;
#endif
}

inline bool GetHeapProfile(void (*func)(void*, const char*, int), void* arg) {
  return false;
}

bool HasAcceleratedCRC32C();
uint32_t AcceleratedCRC32C(uint32_t crc, const char* buf, size_t size);

}
}

#endif  //存储级别b_port_port_win_h_
