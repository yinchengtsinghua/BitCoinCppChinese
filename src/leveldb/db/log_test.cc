
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

#include "db/log_reader.h"
#include "db/log_writer.h"
#include "leveldb/env.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include "util/random.h"
#include "util/testharness.h"

namespace leveldb {
namespace log {

//构造一个指定长度的字符串，该字符串由提供的
//部分字符串。
static std::string BigString(const std::string& partial_string, size_t n) {
  std::string result;
  while (result.size() < n) {
    result.append(partial_string);
  }
  result.resize(n);
  return result;
}

//从数字构造字符串
static std::string NumberString(int n) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%d.", n);
  return std::string(buf);
}

//返回一个倾斜的可能很长的字符串
static std::string RandomSkewedString(int i, Random* rnd) {
  return BigString(NumberString(i), rnd->Skewed(17));
}

class LogTest {
 private:
  class StringDest : public WritableFile {
   public:
    std::string contents_;

    virtual Status Close() { return Status::OK(); }
    virtual Status Flush() { return Status::OK(); }
    virtual Status Sync() { return Status::OK(); }
    virtual Status Append(const Slice& slice) {
      contents_.append(slice.data(), slice.size());
      return Status::OK();
    }
  };

  class StringSource : public SequentialFile {
   public:
    Slice contents_;
    bool force_error_;
    bool returned_partial_;
    StringSource() : force_error_(false), returned_partial_(false) { }

    virtual Status Read(size_t n, Slice* result, char* scratch) {
      ASSERT_TRUE(!returned_partial_) << "must not Read() after eof/error";

      if (force_error_) {
        force_error_ = false;
        returned_partial_ = true;
        return Status::Corruption("read error");
      }

      if (contents_.size() < n) {
        n = contents_.size();
        returned_partial_ = true;
      }
      *result = Slice(contents_.data(), n);
      contents_.remove_prefix(n);
      return Status::OK();
    }

    virtual Status Skip(uint64_t n) {
      if (n > contents_.size()) {
        contents_.clear();
        return Status::NotFound("in-memory file skipped past end");
      }

      contents_.remove_prefix(n);

      return Status::OK();
    }
  };

  class ReportCollector : public Reader::Reporter {
   public:
    size_t dropped_bytes_;
    std::string message_;

    ReportCollector() : dropped_bytes_(0) { }
    virtual void Corruption(size_t bytes, const Status& status) {
      dropped_bytes_ += bytes;
      message_.append(status.ToString());
    }
  };

  StringDest dest_;
  StringSource source_;
  ReportCollector report_;
  bool reading_;
  Writer* writer_;
  Reader* reader_;

//用于测试初始偏移功能的记录元数据
  static size_t initial_offset_record_sizes_[];
  static uint64_t initial_offset_last_record_offsets_[];
  static int num_initial_offset_records_;

 public:
  LogTest() : reading_(false),
              writer_(new Writer(&dest_)),
              /*der_u（新读卡器（&source_uu，&report_u，真/*校验和*/，
                      0/*初始偏移*/)) {

  }

  ~LogTest() {
    delete writer_;
    delete reader_;
  }

  void ReopenForAppend() {
    delete writer_;
    writer_ = new Writer(&dest_, dest_.contents_.size());
  }

  void Write(const std::string& msg) {
    ASSERT_TRUE(!reading_) << "Write() after starting to read";
    writer_->AddRecord(Slice(msg));
  }

  size_t WrittenBytes() const {
    return dest_.contents_.size();
  }

  std::string Read() {
    if (!reading_) {
      reading_ = true;
      source_.contents_ = Slice(dest_.contents_);
    }
    std::string scratch;
    Slice record;
    if (reader_->ReadRecord(&record, &scratch)) {
      return record.ToString();
    } else {
      return "EOF";
    }
  }

  void IncrementByte(int offset, int delta) {
    dest_.contents_[offset] += delta;
  }

  void SetByte(int offset, char new_byte) {
    dest_.contents_[offset] = new_byte;
  }

  void ShrinkSize(int bytes) {
    dest_.contents_.resize(dest_.contents_.size() - bytes);
  }

  void FixChecksum(int header_offset, int len) {
//计算类型/长度/数据的CRC
    uint32_t crc = crc32c::Value(&dest_.contents_[header_offset+6], 1 + len);
    crc = crc32c::Mask(crc);
    EncodeFixed32(&dest_.contents_[header_offset], crc);
  }

  void ForceError() {
    source_.force_error_ = true;
  }

  size_t DroppedBytes() const {
    return report_.dropped_bytes_;
  }

  std::string ReportMessage() const {
    return report_.message_;
  }

//返回OK iff recorded错误消息包含“msg”
  std::string MatchError(const std::string& msg) const {
    if (report_.message_.find(msg) == std::string::npos) {
      return report_.message_;
    } else {
      return "OK";
    }
  }

  void WriteInitialOffsetLog() {
    for (int i = 0; i < num_initial_offset_records_; i++) {
      std::string record(initial_offset_record_sizes_[i],
                         static_cast<char>('a' + i));
      Write(record);
    }
  }

  void StartReadingAt(uint64_t initial_offset) {
    delete reader_;
    /*der_u=新读卡器（&source_u，&report_u，true/*校验和*/，初始_偏移量）；
  }

  void checkoffsetpastendreturnsnorecords（uint64_t offset_over_end）
    writeInitialOffsetLog（）；
    读数为“真”；
    源目录片（目标目录）；
    读卡器*偏移量读卡器=新读卡器（&source和report），真/*checksu*/,

                                       WrittenBytes() + offset_past_end);
    Slice record;
    std::string scratch;
    ASSERT_TRUE(!offset_reader->ReadRecord(&record, &scratch));
    delete offset_reader;
  }

  void CheckInitialOffsetRecord(uint64_t initial_offset,
                                int expected_record_offset) {
    WriteInitialOffsetLog();
    reading_ = true;
    source_.contents_ = Slice(dest_.contents_);
    /*der*偏移量读卡器=新读卡器（&source和report，真/*校验和*/，
                                       初始偏移量）；

    //从预期的记录偏移量到最后一个记录，读取所有记录。
    断言“lt”（预期的“记录偏移量”，num“初始偏移量”）；
    对于（；预期的_记录_偏移量<num_初始_偏移量_记录_uuu；
         +预期的_记录_偏移量）
      切片记录；
      std：：字符串划痕；
      断言“真”（偏移量读卡器->读记录（&record，&scratch））；
      断言\u eq（初始\u偏移量\u记录大小\[预期\u记录\u偏移量]，
                记录大小（））；
      断言\u eq（初始\u偏移量\u最后一条记录\u偏移量[预期\u记录\u偏移量]，
                offset_reader->lastrecordoffset（））；
      assert_eq（（char）（'a'+预期的_record_offset），record.data（）[0]）；
    }
    删除偏移量读卡器；
  }
}；

大小日志测试：：初始偏移记录大小
    10000，//第一个块中有两个相当大的记录
     10000，
     2*Log:：KblockSize-1000，//跨越三个块
     1，
     13716，//只使用块3的两个字节。
     日志：：kblocksize-kheadersize，//使用块4的全部内容。
    }；

uint64_t logtest：：初始偏移量\最后一条记录偏移量
    { 0，
     kheadersize+10000，
     2*（kheaderize+10000）
     2*（kheaderize+10000）+
         （2*日志：：Kblocksize-1000）+3*Kheadersize，
     2*（kheaderize+10000）+
         （2*日志：：KblockSize-1000）+3*KheaderSize
         +kheadersize+1，
     3*日志：：KblockSize，
    }；

//logtest：：在此之前必须定义初始偏移量\最后一个记录偏移量。
int logtest：：num_initial_offset_records_=
    sizeof（logtest:：initial_offset_last_record_offsets_uu）/sizeof（uint64_t）；

测试（logtest，空）
  断言eq（“eof”，read（））；
}

测试（logtest，读写）
  写（“FO”）；
  写（栏）；
  写（“”）；
  写（XXXX）；
  断言“eq”（“foo”，read（））；
  断言_Eq（“bar”，read（））；
  断言_Eq（“”，read（））；
  断言_Eq（“XXXX”，read（））；
  断言eq（“eof”，read（））；
  assert_eq（“eof”，read（））；//确保在eof工作时读取
}

测试（logtest，manyblocks）
  对于（int i=0；i<100000；i++）
    写入（numberstring（i））；
  }
  对于（int i=0；i<100000；i++）
    断言eq（numberstring（i），read（））；
  }
  断言eq（“eof”，read（））；
}

测试（logtest，碎片）
  写（“小”）；
  写入（bigstring（“中”，50000））；
  写入（bigstring（“大”，100000））；
  断言eq（“small”，read（））；
  断言eq（bigstring（“medium”，50000），read（））；
  断言eq（bigstring（“large”，100000），read（））；
  断言eq（“eof”，read（））；
}

测试（logtest，边缘铁路公司）
  //生成与空记录长度完全相同的尾部。
  const int n=kblock大小-2*kheadersize；
  写入（bigstring（“foo”，n））；
  断言eq（kblocksize-kheadersize，writtenbytes（））；
  写（“”）；
  写（栏）；
  断言eq（bigstring（“foo”，n），read（））；
  断言_Eq（“”，read（））；
  断言_Eq（“bar”，read（））；
  断言eq（“eof”，read（））；
}

测试（logtest，边缘铁路2）
  //生成与空记录长度完全相同的尾部。
  const int n=kblock大小-2*kheadersize；
  写入（bigstring（“foo”，n））；
  断言eq（kblocksize-kheadersize，writtenbytes（））；
  写（栏）；
  断言eq（bigstring（“foo”，n），read（））；
  断言_Eq（“bar”，read（））；
  断言eq（“eof”，read（））；
  断言_eq（0，droppedBytes（））；
  断言_Eq（“”，reportMessage（））；
}

测试（日志测试，短拖车）
  const int n=kblock大小-2*kheadersize+4；
  写入（bigstring（“foo”，n））；
  断言eq（kblocksize-kheadersize+4，writtenbytes（））；
  写（“”）；
  写（栏）；
  断言eq（bigstring（“foo”，n），read（））；
  断言_Eq（“”，read（））；
  断言_Eq（“bar”，read（））；
  断言eq（“eof”，read（））；
}

测试（LogTest，Alignedeof）
  const int n=kblock大小-2*kheadersize+4；
  写入（bigstring（“foo”，n））；
  断言eq（kblocksize-kheadersize+4，writtenbytes（））；
  断言eq（bigstring（“foo”，n），read（））；
  断言eq（“eof”，read（））；
}

测试（logtest，openforappend）
  写（“你好”）；
  reopenforAppend（）；
  写（“世界”）；
  断言eq（“hello”，read（））；
  断言“eq”（“world”，read（））；
  断言eq（“eof”，read（））；
}

测试（logtest，randomread）
  常量int n=500；
  随机写入寄存器（301）；
  对于（int i=0；i<n；i++）
    写入（random歪斜字符串（i，&write-rnd））；
  }
  随机读取（301）；
  对于（int i=0；i<n；i++）
    断言eq（random歪斜字符串（i，&read\u rnd），read（））；
  }
  断言eq（“eof”，read（））；
}

//对log reader.cc中所有错误路径的测试如下：

测试（logtest，readerror）
  写（“FO”）；
  强制（）；
  断言eq（“eof”，read（））；
  断言eq（kblockSize，droppedBytes（））；
  断言_Eq（“OK”，matchError（“read error”））；
}

测试（logtest，badrecordtype）
  写（“FO”）；
  //类型存储在头[6]中
  递增字节（6100）；
  固定校验和（0，3）；
  断言eq（“eof”，read（））；
  断言_eq（3，droppedbytes（））；
  断言_Eq（“OK”，MatchError（“Unknown record type”））；
}

测试（logtest，截断的railingrecordinatored）
  写（“FO”）；
  shrinksize（4）；//删除所有有效负载和头字节
  断言eq（“eof”，read（））；
  //忽略截断的最后一条记录，不视为错误。
  断言_eq（0，droppedBytes（））；
  断言_Eq（“”，reportMessage（））；
}

测试（logtest，badlength）
  const int kpayloadsize=kblocksize-kheadersize；
  写入（bigstring（“bar”，kpayloadsize））；
  写（“FO”）；
  //最小有效大小字节存储在头[4]中。
  递增字节（4，1）；
  断言“eq”（“foo”，read（））；
  断言eq（kblockSize，droppedBytes（））；
  断言_Eq（“OK”，matchError（“坏记录长度”））；
}

测试（logtest，badlengthatendisigned）
  写（“FO”）；
  收缩尺寸（1）；
  断言eq（“eof”，read（））；
  断言_eq（0，droppedBytes（））；
  断言_Eq（“”，reportMessage（））；
}

测试（日志测试，校验和不匹配）
  写（“FO”）；
  递增字节（0，10）；
  断言eq（“eof”，read（））；
  断言_eq（10，droppedBytes（））；
  断言_eq（“OK”，matchError（“checksum mismatch”））；
}

测试（logtest，UnexpectedIddleType）
  写（“FO”）；
  设置字节（6，kmiddleType）；
  固定校验和（0，3）；
  断言eq（“eof”，read（））；
  断言_eq（3，droppedbytes（））；
  断言“eq”（“OK”，“MatchError”（“Missing Start”））；
}

测试（logtest，unexpectedlasttype）
  写（“FO”）；
  设置字节（6，klasttype）；
  固定校验和（0，3）；
  断言eq（“eof”，read（））；
  断言_eq（3，droppedbytes（））；
  断言“eq”（“OK”，“MatchError”（“Missing Start”））；
}

测试（logtest，unexpectedfulltype）
  写（“FO”）；
  写（栏）；
  设置字节（6，kfirsttype）；
  固定校验和（0，3）；
  断言_Eq（“bar”，read（））；
  断言eq（“eof”，read（））；
  断言_eq（3，droppedbytes（））；
  断言“eq”（“OK”，“MatchError”（“无结束的部分记录”））；
}

测试（logtest，unexpectedfirsttype）
  写（“FO”）；
  写入（bigstring（“bar”，100000））；
  设置字节（6，kfirsttype）；
  固定校验和（0，3）；
  断言eq（bigstring（“bar”，100000），read（））；
  断言eq（“eof”，read（））；
  断言_eq（3，droppedbytes（））；
  断言“eq”（“OK”，“MatchError”（“无结束的部分记录”））；
}

测试（logtest，missinglastisignored）
  写入（bigstring（“bar”，kblocksize））；
  //删除最后一个块，包括头。
  收缩尺寸（14）；
  断言eq（“eof”，read（））；
  断言_Eq（“”，reportMessage（））；
  断言_eq（0，droppedBytes（））；
}

测试（日志测试，部分已指定）
  写入（bigstring（“bar”，kblocksize））；
  //导致最后一个块中的记录长度错误。
  收缩尺寸（1）；
  断言eq（“eof”，read（））；
  断言_Eq（“”，reportMessage（））；
  断言_eq（0，droppedBytes（））；
}

测试（logtest，skipintomultirecord）
  //考虑碎片记录：
  //第一个（r1）、中间（r1）、最后一个（r1）、第一个（r2）
  //如果初始偏移量指向第一个（r1）之后但第一个（r2）之前的记录
  //不完整片段错误不是实际错误，必须取消
  //直到遇到新的第一条或完整记录。
  写入（bigstring（“foo”，3*kblocksize））；
  写（“正确”）；
  开始读取（KblockSize）；

  断言“eq”（“correct”，read（））；
  断言_Eq（“”，reportMessage（））；
  断言_eq（0，droppedBytes（））；
  断言eq（“eof”，read（））；
}

测试（日志测试、错误连接记录）
  //考虑两个碎片记录：
  //第一个（R1）最后一个（R1）第一个（R2）最后一个（R2）
  //中间两个片段消失的地方。我们不想要
  //第一个（r1），最后一个（r2）加入并作为有效记录返回。

  //写入跨越两个块的记录
  写入（bigstring（“foo”，kblocksize））；
  写入（bigstring（“bar”，kblocksize））；
  写（“正确”）；

  //擦除中间块
  for（int offset=kblocksize；offset<2*kblocksize；offset++）
    setbyte（偏移量，“x”）；
  }

  断言“eq”（“correct”，read（））；
  断言eq（“eof”，read（））；
  const size_t dropped=droppedbytes（）；
  断言（丢弃，2*KblockSize+100）；
  断言ge（丢弃，2*kblocksize）；
}

测试（logtest，readstart）
  checkinitial偏移量记录（0，0）；
}

测试（logtest，readsecondoneoff）
  核对偏移量记录（1，1）；
}

测试（logtest，readsecenthousand）
  核对偏移量记录（10000，1）；
}

测试（logtest，readsecondstart）
  核对偏移量记录（10007，1）；
}

测试（logtest，readthirdoneoff）
  核对偏移量记录（10008，2）；
}

测试（LogTest，readThirdStart）
  核对偏移量记录（20014，2）；
}

测试（logtest，read4thooneoff）
  核对偏移量记录（20015，3）；
}

测试（logtest，read4thfirstblocktrailer）
  checkInitialOffsetRecord（日志：：kblockSize-4，3）；
}

测试（logtest，read4thmiddleblock）
  checkInitialOffsetRecord（日志：：kblockSize+1，3）；
}

测试（logtest，read4thlastblock）
  checkInitialOffsetRecord（2*日志：：kblockSize+1，3）；
}

测试（logtest，read4thstart）
  checkinitial偏移量记录（
      2*（kheadersize+1000）+（2*log:：kblocksize-1000）+3*kheadersize，
      3）；
}

测试（logtest，readinitialoffsetintoblockpadding）
  checkInitialOffsetRecord（3*日志：：kblockSize-3，5）；
}

测试（logtest，readend）
  checkoffsetpastendreturnsnorecords（0）；
}

测试（logtest，readpastend）
  检查偏移量粘贴和返回记录（5）；
}

//命名空间日志
//命名空间级别db

int main（int argc，char**argv）
  返回leveldb:：test:：runalltests（）；
}
