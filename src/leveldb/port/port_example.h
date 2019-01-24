
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
//此文件包含规范，但不包含实现，
//平台应定义的类型/操作/等
//特定端口<platform>.h文件。将此文件用作
//如何将此包移植到新平台。

#ifndef STORAGE_LEVELDB_PORT_PORT_EXAMPLE_H_
#define STORAGE_LEVELDB_PORT_PORT_EXAMPLE_H_

namespace leveldb {
namespace port {

//Todo（Jorlow）：其中许多属于环境类，而不是
//在这里。我们应该尝试移动它们，看看它是否影响性能。

//在小endian计算机上，下列布尔常量必须为true
//否则就错了。
/*tic const bool klittleendian=true/*或其他表达式*/；

///——————————————————————————————————————————————————————————————————————————————————————————————————————————————

//互斥体表示独占锁。
类互斥
 公众：
  MutExter（）；
  ~ MutExter（）；

  //锁定互斥体。等待其他储物柜退出。
  //如果互斥体已被此线程锁定，则将死锁。
  空锁（）；

  //解锁互斥体。
  //需要：此互斥体已被此线程锁定。
  空隙解锁（）；

  //如果此线程不包含此互斥体，则可以选择崩溃。
  //实现必须很快，尤其是当ndebug是
  / /定义。允许实现跳过所有检查。
  void asserthold（）；
}；

康德瓦尔级
 公众：
  显式condvar（mutex*mu）；
  ~·CONDVARY（）；

  //原子释放*mu并在此条件变量上阻塞，直到
  //调用signalAll（）或调用signal（）选择
  //要唤醒的线程。
  //需要：此线程包含*mu
  空WAITE（）；

  //如果有线程在等待，请至少唤醒其中一个线程。
  空信号（）；

  //唤醒所有等待的线程。
  无效信号lall（）；
}；

//线程安全初始化。
//用法如下：
//静态端口：：onceType init_control=leveldb_once_init；
//静态void initializer（）…做点什么…；
/…
//端口：：initonce（&init_control，&initializer）；
typedef intptr类型；
定义一次级别数据库_初始化0
extern void initonce（端口：：onceType*，void（*初始值设定项）（）；

//一个类型，它包含一个可以原子地读或写的指针
//（即，无文字撕裂。）
类原子指针
 私人：
  免疫应答；
 公众：
  //初始化为任意值
  原子指针（）；

  //初始化为保持V
  显式原子指针（void*v）：rep_

  //读取并返回存储指针，并保证
  //此线程以后的内存访问（读或写）可以
  //在此读取之前重新排序。
  void*Acquire_Load（）常量；

  //将v设置为存储指针，并保证
  //此线程的内存访问（读或写）可以重新排序
  //在这个商店之后。
  作废放行仓库（作废*V）；

  //读取存储的指针，没有排序保证。
  void*nobarrier_load（）常量；

  //将va设置为不带排序保证的存储指针。
  空的Nobarrier_商店（空的*v）；
}；

///————————————————压缩———————————————

//在*输出中存储“input[0，input_length-1]”的快速压缩。
//如果此端口不支持snappy，则返回false。
外部bool snappy_compress（const char*输入，大小输入长度，
                            std:：string*输出）；

//如果输入[0，输入长度-1]看起来像有效的快速压缩
//缓冲区，将未压缩数据的大小存储在*result和
//返回true。否则返回false。
外部bool snappy_getuncompressedlength（const char*input，size_t length，
                                         尺寸t*结果）；

//尝试将输入[0，input_length-1]快速解压缩到*output。
//如果成功则返回true，如果输入无效则返回false
//压缩数据。
/ /
//要求：输出[]的前“n”个字节必须可写
//其中“n”是成功调用
//snappy_getuncompressedlength。
外部bool snappy_uncompress（const char*输入数据，大小输入长度，
                              字符输出；

//————————————————————————————————————————————————————其他

//如果不支持堆分析，则返回false。
//否则重复调用（*func）（arg，data，n），然后返回true。
//所有“data[0，n-1]”片段的串联是堆配置文件。
extern bool getheapprofile（void（*func）（void*，const char*，int），void*arg）；

//确定是否存在工作加速的CRC32实现
//如果AcceleratedCRC32C可以安全调用，则返回true
bool hasAcceleratedcrc32c（）；

//扩展CRC以包含buf的前n个字节。
/ /
//如果CRC不能使用加速进行扩展，则返回零，否则返回
//新扩展的CRC值（也可以为零）。
uint32_t acceleratedcrc32c（uint32_t crc，const char*buf，size_t size）；

//命名空间端口
//命名空间级别db

endif//存储级别\u端口\u端口\u示例
