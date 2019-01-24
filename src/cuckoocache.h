
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2016 Jeremy Rubin
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_CUCKOOCACHE_H
#define BITCOIN_CUCKOOCACHE_H

#include <array>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cmath>
#include <memory>
#include <vector>


/*命名空间布谷缓存提供高性能缓存原语
 *
 ＊摘要：
 *
 *1）位打包的原子标记是用于垃圾收集的位打包的原子标记。
 *
 *2）缓存是一种在内存使用和查找速度上都具有性能的缓存。它
 *对于擦除操作是无锁的。元素在下一个元素上被延迟删除
 *插入。
 **/

namespace CuckooCache
{
/*位打包的原子标记实现垃圾收集标记的容器
 *这只是在调用安装程序时线程不安全。此类位包集合
 *内存效率标志。
 *
 *所有操作都是std:：memory_order_released，因此外部机制必须
 *确保写入和读取正确同步。
 *
 *在设置（n）时，所有高达n的位都标记为已收集。
 *
 *在引擎盖下面，因为它是一个8位类型，所以使用倍数是有意义的。
 *设置为8，但如果情况并非如此，则是安全的。
 *
 **/

class bit_packed_atomic_flags
{
    std::unique_ptr<std::atomic<uint8_t>[]> mem;

public:
    /*没有默认的构造函数，因为必须有一些大小*/
    bit_packed_atomic_flags() = delete;

    /*
     *位打包的原子标记构造函数创建足够的内存
     *跟踪垃圾收集信息中的大小条目。
     *
     *@param size为其分配空间的元素数
     *
     在所有x.x<
     *大小
     *@post所有对bit的调用都将返回\u set（没有后续的bit\u unset）
     *是真的。
     **/

    explicit bit_packed_atomic_flags(uint32_t size)
    {
//如果需要的话，垫出尺寸
        size = (size + 7) / 8;
        mem.reset(new std::atomic<uint8_t>[size]);
        for (uint32_t i = 0; i < size; ++i)
            mem[i].store(0xFF);
    };

    /*安装程序标记所有条目并确保位打包的原子标记可以存储
     *至少输入大小
     *
     *@param b为其分配空间的元素数
     在所有x.x<
     ＊B
     *@post所有对bit的调用都将返回\u set（没有后续的bit\u unset）
     *是真的。
     **/

    inline void setup(uint32_t b)
    {
        bit_packed_atomic_flags d(b);
        std::swap(mem, d.mem);
    }

    /*位设置将一个条目设置为可丢弃。
     *
     *@param s要设置的项的索引。
     *@立即发布后续调用（假定外部内存正确）
     *ordering）to bit_is_set（s）==真。
     *
     **/

    inline void bit_set(uint32_t s)
    {
        mem[s >> 3].fetch_or(1 << (s & 7), std::memory_order_relaxed);
    }

    /*位未设置将条目标记为不应覆盖的内容
     *
     *@param s要复位的项的索引。
     *@立即发布后续调用（假定外部内存正确）
     *ordering）to bit_is_set（s）==false.
     **/

    inline void bit_unset(uint32_t s)
    {
        mem[s >> 3].fetch_and(~(1 << (s & 7)), std::memory_order_relaxed);
    }

    /*bit_is_set在s查询表的可丢弃性
     *
     *@param s要读取的条目的索引。
     *@返回是否设置了索引S处的位。
     **/

    inline bool bit_is_set(uint32_t s) const
    {
        return (1 << (s & 7)) & mem[s >> 3].load(std::memory_order_relaxed);
    }
};

/*缓存实现具有类似布谷鸟集的属性的缓存
 *
 *缓存最多可容纳（~（uint32_t）0）-1个元素。
 *
 *读取操作：
 *-包含（*，false）
 *
 *读取+擦除操作：
 *-包含（*，真）
 *
 *擦除操作：
 *-允许擦除（）
 *
 *写入操作：
 *-设置（）
 *-设置字节（）
 *-插入（）
 *-请保持（））
 *
 *无同步操作：
 *-无效（）
 *-计算哈希（）
 *
 *用户必须保证：
 *
 *1）写入需要同步访问（例如，锁）
 *2）读取不需要并发写入，与上一次插入同步。
 *3）擦除不需要并发写入，与上次插入同步。
 *4）擦除调用程序在允许新的写入程序之前必须释放所有内存。
 *
 *
 *函数名说明：
 *-使用“allow_erase”这个名称，因为真正的放弃会在以后发生。
 *-使用名称“please\u keep”，因为元素在插入时可能会被擦除。
 *
 *@tparam元素应为可移动和可复制类型
 *@tparam散列应该是接受模板参数的函数/可调用的
 *哈希选择和一个元素，并从中提取哈希。应该返回
 *hash h；h<0>（e）的高熵uint32_t散列…H＜7＞（E）。
 **/

template <typename Element, typename Hash>
class cache
{
private:
    /*表存储所有元素*/
    std::vector<Element> table;

    /*大小存储哈希表中的可用插槽总数*/
    uint32_t size;

    /*位压缩的原子标记数组被标记为可变的，因为我们需要
     *允许从const方法进行垃圾收集*/

    mutable bit_packed_atomic_flags collection_flags;

    /*epoch_标志跟踪元素最近插入的时间
     *缓存。true表示最近，false表示最近。参见插入（）
     *完整语义的方法。
     **/

    mutable std::vector<bool> epoch_flags;

    /*epoch_启发式计数器用于确定某个时期的年龄。
     *应该进行昂贵的扫描。epoch_启发式计数器是
     *插入时减量，并重置为新插入数，这将
     *当达到零时，使epoch达到epoch_大小。
     **/

    uint32_t epoch_heuristic_counter;

    /*epoch_size设置为在
     *时代。当一个纪元中未删除元素的数目
     *超过epoch_大小，应开始新的epoch
     *当前条目已降级。epoch_大小设置为大小的45%，因为
     *我们希望负载保持在90%左右，同时支持3个阶段--
     *一个“死亡”已被删除，一个“死亡”已被标记为
     *删除下一个，和一个“活”的新插入添加。
     **/

    uint32_t epoch_size;

    /*深度限制决定应尝试替换多少元素。
     *应设置为log2（n*/

    uint8_t depth_limit;

    /*哈希函数是哈希函数的常量实例。它不能
     *静态或在调用时初始化，因为它可能具有内部状态（例如
     *一个随机数。
     **/

    const Hash hash_function;

    /*计算哈希对于不必写出这个很方便
     *表达式在我们使用元素散列值的任何地方。
     *
     *我们需要在一个
     *尽可能保持哈希一致性的方式。理想的
     *这可以通过位屏蔽实现，但大小通常不是2的幂。
     *
     *幼稚的方法是使用一个mod——它不是完全统一的，但是
     *只要散列值比大小大得多，就没有那么糟糕了。不幸的是，
     *在普通微处理器上，mod/division的速度相当慢（例如，在
     *haswell，arm甚至没有它的指令。）；当除数是
     *常量编译器会巧妙地将其转换为乘法+加法+移位，
     *但是大小是一个运行时值，所以编译器在这里不能这样做。
     *
     *一个选项是实现编译器使用的相同技巧并计算
     *根据大小精确划分的常量，如“n-位无符号”中所述。
     *2005年通过n位乘法加法”除以Arch D.Robison。但那条规则是
     *有些复杂，结果仍然比其他选项慢：
     *
     *相反，我们将32位随机数视为范围内的q32定点数。
     *[0,1）并简单地将其乘以大小。然后我们将结果向下移动
     *32位用于获取桶号。结果不均匀，与
     *mod，但计算速度要快得多。有关此技术的更多信息，请访问
     *http://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reducation/
     *
     *由此产生的不均匀性也更加均匀，这将是
     *有利于线性探测，但这并不重要。
     *布谷鸟桌可以这样或那样。
     *
     *这种方法的主要缺点是提高了中间精度
     *必需，但对于32位随机数，我们只需要
     *32*32->64乘，这意味着即使在
     *典型的32位处理器。
     *
     *@param e返回哈希值的元素
     *@返回从e派生的确定性散列的std:：array<uint32_t，8>
     **/

    inline std::array<uint32_t, 8> compute_hashes(const Element& e) const
    {
        return {{(uint32_t)(((uint64_t)hash_function.template operator()<0>(e) * (uint64_t)size) >> 32),
                 (uint32_t)(((uint64_t)hash_function.template operator()<1>(e) * (uint64_t)size) >> 32),
                 (uint32_t)(((uint64_t)hash_function.template operator()<2>(e) * (uint64_t)size) >> 32),
                 (uint32_t)(((uint64_t)hash_function.template operator()<3>(e) * (uint64_t)size) >> 32),
                 (uint32_t)(((uint64_t)hash_function.template operator()<4>(e) * (uint64_t)size) >> 32),
                 (uint32_t)(((uint64_t)hash_function.template operator()<5>(e) * (uint64_t)size) >> 32),
                 (uint32_t)(((uint64_t)hash_function.template operator()<6>(e) * (uint64_t)size) >> 32),
                 (uint32_t)(((uint64_t)hash_function.template operator()<7>(e) * (uint64_t)size) >> 32)}};
    }

    /*结束
     *@返回无法插入到的constexpr索引*/

    constexpr uint32_t invalid() const
    {
        return ~(uint32_t)0;
    }

    /*允许擦除将索引n处的元素标记为可丢弃。线程安全的
     *无任何并发插入。
     *@param n允许删除的索引
     **/

    inline void allow_erase(uint32_t n) const
    {
        collection_flags.bit_set(n);
    }

    /*请将索引n处的元素标记为应保留的条目。
     *螺纹安全，无任何并发插入。
     *@param n要优先保存的索引
     **/

    inline void please_keep(uint32_t n) const
    {
        collection_flags.bit_unset(n);
    }

    /*epoch_check处理存储在
     *缓存。每次插入前都要进行epoch检查。
     *
     *首先，epoch检查减量和检查便宜的启发式，然后检查
     *如果廉价的启发式算法用完了，扫描成本会更高。如果贵
     *扫描成功，年代老化，旧元素允许擦除。这个
     *廉价启发式在最坏情况下
     *当前epoch的元素将超过epoch的大小。
     **/

    void epoch_check()
    {
        if (epoch_heuristic_counter != 0) {
            --epoch_heuristic_counter;
            return;
        }
//计算最近一个epoch中
//没有被删除。
        uint32_t epoch_unused_count = 0;
        for (uint32_t i = 0; i < size; ++i)
            epoch_unused_count += epoch_flags[i] &&
                                  !collection_flags.bit_is_set(i);
//如果当前纪元中的未删除条目多于
//epoch大小，然后允许擦除旧epoch中的所有元素（已标记
//错误）并将当前纪元中的所有元素移到旧纪元
//但是不要调用allow-erase来删除它们的索引。
        if (epoch_unused_count >= epoch_size) {
            for (uint32_t i = 0; i < size; ++i)
                if (epoch_flags[i])
                    epoch_flags[i] = false;
                else
                    allow_erase(i);
            epoch_heuristic_counter = epoch_size;
        } else
//将“epoch”启发式计数器重置为“next do a scan when worst”
//案例行为（无间歇性擦除）将超过时代大小，
//具有合理的最小扫描尺寸。
//通常，我们必须检查std：：min（epoch_大小，
//epoch_unused_count），但我们已经知道'epoch_unused_count
//<epoch_size`在此分支中
            epoch_heuristic_counter = std::max(1u, std::max(epoch_size / 16,
                        epoch_size - epoch_unused_count));
    }

public:
    /*必须始终通过后续的
     *调用设置或设置字节，否则操作可能会出错。
     **/

    cache() : table(), size(), collection_flags(0), epoch_flags(),
    epoch_heuristic_counter(), epoch_size(), depth_limit(0), hash_function()
    {
    }

    /*安装程序初始化容器以存储不超过新的\u大小
     ＊元素。
     *
     *安装程序只能调用一次。
     *
     *@param new_调整要存储的元素的所需数量
     *@返回可存储的最大元素数
     */

    uint32_t setup(uint32_t new_size)
    {
//深度限制必须至少为一个，否则可能发生错误。
        depth_limit = static_cast<uint8_t>(std::log2(static_cast<float>(std::max((uint32_t)2, new_size))));
        size = std::max<uint32_t>(2, new_size);
        table.resize(size);
        collection_flags.setup(size);
        epoch_flags.resize(size);
//如上所述设置为45%
        epoch_size = std::max((uint32_t)1, (45 * size) / 100);
//最初设置为等待整个时代
        epoch_heuristic_counter = epoch_size;
        return size;
    }

    /*设置字节是一个方便的函数，它负责内部内存
     *决定要存储多少元素时的用法。不完美是因为
     *不考虑任何开销（结构大小、malocsage、集合）
     *和时代标志）。这样做是为了简化选择2的幂
     *尺寸。在预期的用例中，每个条目应该多加两位
     *与元件尺寸相比，可忽略不计。
     *
     *@param bytes用于此数据的大约字节数
     ＊结构。
     *@返回可存储的元素的最大数目（请参阅SETUP（））
     *有关详细信息，请参阅文档）
     **/

    uint32_t setup_bytes(size_t bytes)
    {
        return setup(bytes/sizeof(Element));
    }

    /*在最大深度插入循环\限制尝试插入哈希的时间
     *通过布谷鸟算法的变体在表中的不同位置
     *有八个哈希位置。
     *
     *如果上次尝试的元素超出了深度，则会将其丢弃。
     *遇到打开的插槽。
     *
     *因此
     *
     *插入（x）；
     *返回包含（x，false）；
     *
     *不保证返回true。
     *
     *@param e要插入的元素
     *@发布以下内容之一：所有先前插入的元素和e
     *现在在表中，一个先前插入的元素从
     *表中，将收回试图插入的条目。
     *
     **/

    inline void insert(Element e)
    {
        epoch_check();
        uint32_t last_loc = invalid();
        bool last_epoch = true;
        std::array<uint32_t, 8> locs = compute_hashes(e);
//确保尚未插入此元素
//如果有的话，确保它不会被删除。
        for (const uint32_t loc : locs)
            if (table[loc] == e) {
                please_keep(loc);
                epoch_flags[loc] = last_epoch;
                return;
            }
        for (uint8_t depth = 0; depth < depth_limit; ++depth) {
//首先尝试插入空插槽（如果存在）
            for (const uint32_t loc : locs) {
                if (!collection_flags.bit_is_set(loc))
                    continue;
                table[loc] = std::move(e);
                please_keep(loc);
                epoch_flags[loc] = last_epoch;
                return;
            }
            /*与之前所在位置的元素交换
            *不是最后一个看到的。例子：
            *
            *1）在第一次迭代中，最后一个loc==invalid（），find返回last，所以
            *最后一个loc默认为locs[0]。
            *2）在进一步的迭代中，最后一个loc==locs[k]，最后一个loc将
            *转到locs[k+1%8]，即8个指数中的下一个
            *必要时为0。
            *
            *这样可以防止移动刚放入的元件。
            *
            *交换不是移动--我们必须切换到被收回的元素
            *用于下一次迭代。
            **/

            last_loc = locs[(1 + (std::find(locs.begin(), locs.end(), last_loc) - locs.begin())) & 7];
            std::swap(table[last_loc], e);
//无法交换std:：vector<bool>：：reference和bool&。
            bool epoch = last_epoch;
            last_epoch = epoch_flags[last_loc];
            epoch_flags[last_loc] = epoch;

//重新计算位置——不幸的是，发生了太多次！
            locs = compute_hashes(e);
        }
    }

    /*包含对给定元素的哈希位置的迭代
     *并检查是否存在。
     *
     *包含不检查垃圾收集状态（换句话说，
     *垃圾仅在需要空间时收集），因此：
     *
     *插入（x）；
     *如果（包含（x，真）
     *返回包含（x，false）；
     *其他
     *返回真值；
     *
     *在单个线程上执行将始终返回true！
     *
     *例如，这是重新组织绩效的一大特点。
     *
     *如果找到元素，则contains返回bool set true。
     *
     *@param e要检查的元素
     *@ PARAM擦除
     *
     *@post如果erase为true并且找到元素，则垃圾收集
     ＊标志被设置
     *@如果找到元素则返回true，否则返回false
     **/

    inline bool contains(const Element& e, const bool erase) const
    {
        std::array<uint32_t, 8> locs = compute_hashes(e);
        for (const uint32_t loc : locs)
            if (table[loc] == e) {
                if (erase)
                    allow_erase(loc);
                return true;
            }
        return false;
    }
};
} //命名空间布谷缓存

#endif //比特币布谷库奇
