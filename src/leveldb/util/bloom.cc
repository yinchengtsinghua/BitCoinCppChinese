
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2012 LevelDB作者。版权所有。
//此源代码的使用受可以
//在许可证文件中找到。有关参与者的名称，请参阅作者文件。

#include "leveldb/filter_policy.h"

#include "leveldb/slice.h"
#include "util/hash.h"

namespace leveldb {

namespace {
static uint32_t BloomHash(const Slice& key) {
  return Hash(key.data(), key.size(), 0xbc9f1d34);
}

class BloomFilterPolicy : public FilterPolicy {
 private:
  size_t bits_per_key_;
  size_t k_;

 public:
  explicit BloomFilterPolicy(int bits_per_key)
      : bits_per_key_(bits_per_key) {
//我们故意把探测成本降低一点
k_ = static_cast<size_t>(bits_per_key * 0.69);  //0.69＝Ln（2）
    if (k_ < 1) k_ = 1;
    if (k_ > 30) k_ = 30;
  }

  virtual const char* Name() const {
    return "leveldb.BuiltinBloomFilter2";
  }

  virtual void CreateFilter(const Slice* keys, int n, std::string* dst) const {
//计算Bloom筛选器大小（位和字节）
    size_t bits = n * bits_per_key_;

//对于小N，我们可以看到非常高的假阳性率。修复它
//通过实施最小布卢姆过滤器长度。
    if (bits < 64) bits = 64;

    size_t bytes = (bits + 7) / 8;
    bits = bytes * 8;

    const size_t init_size = dst->size();
    dst->resize(init_size + bytes, 0);
dst->push_back(static_cast<char>(k_));  //记住过滤器中探头的位置
    char* array = &(*dst)[init_size];
    for (int i = 0; i < n; i++) {
//使用双哈希生成哈希值序列。
//参见[Kirsch，Mitzenmacher 2006]中的分析。
      uint32_t h = BloomHash(keys[i]);
const uint32_t delta = (h >> 17) | (h << 15);  //向右旋转17位
      for (size_t j = 0; j < k_; j++) {
        const uint32_t bitpos = h % bits;
        array[bitpos/8] |= (1 << (bitpos % 8));
        h += delta;
      }
    }
  }

  virtual bool KeyMayMatch(const Slice& key, const Slice& bloom_filter) const {
    const size_t len = bloom_filter.size();
    if (len < 2) return false;

    const char* array = bloom_filter.data();
    const size_t bits = (len - 1) * 8;

//使用编码的k以便我们可以读取由
//使用不同参数创建的Bloom过滤器。
    const size_t k = array[len-1];
    if (k > 30) {
//为短布卢姆过滤器的潜在新编码保留。
//把它当作一场比赛。
      return true;
    }

    uint32_t h = BloomHash(key);
const uint32_t delta = (h >> 17) | (h << 15);  //向右旋转17位
    for (size_t j = 0; j < k; j++) {
      const uint32_t bitpos = h % bits;
      if ((array[bitpos/8] & (1 << (bitpos % 8))) == 0) return false;
      h += delta;
    }
    return true;
  }
};
}

const FilterPolicy* NewBloomFilterPolicy(int bits_per_key) {
  return new BloomFilterPolicy(bits_per_key);
}

}  //命名空间级别数据库
