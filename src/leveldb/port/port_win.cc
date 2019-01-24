
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

#include "port/port_win.h"

#include <windows.h>
#include <cassert>
#include <intrin.h>

namespace leveldb {
namespace port {

Mutex::Mutex() :
    cs_(NULL) {
  assert(!cs_);
  cs_ = static_cast<void *>(new CRITICAL_SECTION());
  ::InitializeCriticalSection(static_cast<CRITICAL_SECTION *>(cs_));
  assert(cs_);
}

Mutex::~Mutex() {
  assert(cs_);
  ::DeleteCriticalSection(static_cast<CRITICAL_SECTION *>(cs_));
  delete static_cast<CRITICAL_SECTION *>(cs_);
  cs_ = NULL;
  assert(!cs_);
}

void Mutex::Lock() {
  assert(cs_);
  ::EnterCriticalSection(static_cast<CRITICAL_SECTION *>(cs_));
}

void Mutex::Unlock() {
  assert(cs_);
  ::LeaveCriticalSection(static_cast<CRITICAL_SECTION *>(cs_));
}

void Mutex::AssertHeld() {
  assert(cs_);
  assert(1);
}

CondVar::CondVar(Mutex* mu) :
    waiting_(0), 
    mu_(mu), 
    sem1_(::CreateSemaphore(NULL, 0, 10000, NULL)), 
    sem2_(::CreateSemaphore(NULL, 0, 10000, NULL)) {
  assert(mu_);
}

CondVar::~CondVar() {
  ::CloseHandle(sem1_);
  ::CloseHandle(sem2_);
}

void CondVar::Wait() {
  mu_->AssertHeld();

  wait_mtx_.Lock();
  ++waiting_;
  wait_mtx_.Unlock();

  mu_->Unlock();

//开始握手
  ::WaitForSingleObject(sem1_, INFINITE);
  ::ReleaseSemaphore(sem2_, 1, NULL);
  mu_->Lock();
}

void CondVar::Signal() {
  wait_mtx_.Lock();
  if (waiting_ > 0) {
    --waiting_;

//完成握手
    ::ReleaseSemaphore(sem1_, 1, NULL);
    ::WaitForSingleObject(sem2_, INFINITE);
  }
  wait_mtx_.Unlock();
}

void CondVar::SignalAll() {
  wait_mtx_.Lock();
  ::ReleaseSemaphore(sem1_, waiting_, NULL);
  while(waiting_ > 0) {
    --waiting_;
    ::WaitForSingleObject(sem2_, INFINITE);
  }
  wait_mtx_.Unlock();
}

AtomicPointer::AtomicPointer(void* v) {
  Release_Store(v);
}

void InitOnce(OnceType* once, void (*initializer)()) {
  once->InitOnce(initializer);
}

void* AtomicPointer::Acquire_Load() const {
  void * p = NULL;
  InterlockedExchangePointer(&p, rep_);
  return p;
}

void AtomicPointer::Release_Store(void* v) {
  InterlockedExchangePointer(&rep_, v);
}

void* AtomicPointer::NoBarrier_Load() const {
  return rep_;
}

void AtomicPointer::NoBarrier_Store(void* v) {
  rep_ = v;
}

bool HasAcceleratedCRC32C() {
#if defined(__x86_64__) || defined(__i386__)
  int cpu_info[4];
  __cpuid(cpu_info, 1);
  return (cpu_info[2] & (1 << 20)) != 0;
#else
  return false;
#endif
}

}
}
