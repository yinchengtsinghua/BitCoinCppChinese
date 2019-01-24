
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2009-2010 Satoshi Nakamoto
//版权所有（c）2009-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#ifndef BITCOIN_POLICY_POLICY_H
#define BITCOIN_POLICY_POLICY_H

#include <consensus/consensus.h>
#include <policy/feerate.h>
#include <script/interpreter.h>
#include <script/standard.h>

#include <string>

class CCoinsViewCache;
class CTxOut;

/*-blockmaxweight的默认值，它控制挖掘代码将创建的块权重的范围。*/
static const unsigned int DEFAULT_BLOCK_MAX_WEIGHT = MAX_BLOCK_WEIGHT - 4000;
/*-blockmintxfee的默认值，它设置由挖掘代码创建的块中事务的最小频率*/
static const unsigned int DEFAULT_BLOCK_MIN_TX_FEE = 1000;
/*我们愿意转播/挖掘的交易的最大重量*/
static const unsigned int MAX_STANDARD_TX_WEIGHT = 400000;
/*我们愿意中继/挖掘的事务的最小非见证大小（1 segwit输入+1 p2wpkh输出=82字节）*/
static const unsigned int MIN_STANDARD_TX_NONWITNESS_SIZE = 82;
/*isStandard（）p2sh脚本中的最大签名检查操作数*/
static const unsigned int MAX_P2SH_SIGOPS = 15;
/*我们愿意在单个Tx中中继/挖掘的最大信号量*/
static const unsigned int MAX_STANDARD_TX_SIGOPS_COST = MAX_BLOCK_SIGOPS_COST/5;
/*-maxmempool的默认值，mempool内存使用的最大兆字节数*/
static const unsigned int DEFAULT_MAX_MEMPOOL_SIZE = 300;
/*-incrementalRelayFee的默认值，该值为mempool limiting或bip 125 replacement设置最小频率增加。*/
static const unsigned int DEFAULT_INCREMENTAL_RELAY_FEE = 1000;
/*-bytespesigop的默认值*/
static const unsigned int DEFAULT_BYTES_PER_SIGOP = 20;
/*标准p2wsh脚本中见证堆栈项的最大数目*/
static const unsigned int MAX_STANDARD_P2WSH_STACK_ITEMS = 100;
/*标准p2wsh脚本中每个见证堆栈项的最大大小*/
static const unsigned int MAX_STANDARD_P2WSH_STACK_ITEM_SIZE = 80;
/*标准见证脚本的最大大小*/
static const unsigned int MAX_STANDARD_P2WSH_SCRIPT_SIZE = 3600;
/*定义灰尘的最小频率。历史上，这是基于
 *minrelaytxfee，但是更改灰尘限制会更改哪些交易
 *标准且应小心完成，理想情况下很少。这是有意义的
 *只有在之前的释放没有产生粉尘限制后才增加粉尘限制。
 *输出低于新阈值*/

static const unsigned int DUST_RELAY_TX_FEE = 3000;
/*
 *标准事务将遵守的标准脚本验证标志
 *。但是，违反这些标志的脚本可能仍然存在于有效的
 *块，我们必须接受这些块。
 **/

static constexpr unsigned int STANDARD_SCRIPT_VERIFY_FLAGS = MANDATORY_SCRIPT_VERIFY_FLAGS |
                                                             SCRIPT_VERIFY_DERSIG |
                                                             SCRIPT_VERIFY_STRICTENC |
                                                             SCRIPT_VERIFY_MINIMALDATA |
                                                             SCRIPT_VERIFY_NULLDUMMY |
                                                             SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS |
                                                             SCRIPT_VERIFY_CLEANSTACK |
                                                             SCRIPT_VERIFY_MINIMALIF |
                                                             SCRIPT_VERIFY_NULLFAIL |
                                                             SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY |
                                                             SCRIPT_VERIFY_CHECKSEQUENCEVERIFY |
                                                             SCRIPT_VERIFY_LOW_S |
                                                             SCRIPT_VERIFY_WITNESS |
                                                             SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM |
                                                             SCRIPT_VERIFY_WITNESS_PUBKEYTYPE |
                                                             SCRIPT_VERIFY_CONST_SCRIPTCODE;

/*为了方便起见，标准但不是强制验证标志。*/
static constexpr unsigned int STANDARD_NOT_MANDATORY_VERIFY_FLAGS = STANDARD_SCRIPT_VERIFY_FLAGS & ~MANDATORY_SCRIPT_VERIFY_FLAGS;

/*用作非一致性代码中顺序和非时钟时间检查的标志参数。*/
static constexpr unsigned int STANDARD_LOCKTIME_VERIFY_FLAGS = LOCKTIME_VERIFY_SEQUENCE |
                                                               LOCKTIME_MEDIAN_TIME_PAST;

CAmount GetDustThreshold(const CTxOut& txout, const CFeeRate& dustRelayFee);

bool IsDust(const CTxOut& txout, const CFeeRate& dustRelayFee);

bool IsStandard(const CScript& scriptPubKey, txnouttype& whichType);
    /*
     *检查标准交易类型
     *@如果所有输出（scriptpubkeys）仅使用标准事务表单，则返回true
     **/

bool IsStandardTx(const CTransaction& tx, std::string& reason);
    /*
     *检查标准交易类型
     *@param[in]mapinputs我们正在支出的具有输出的以前事务的映射
     *@如果所有输入（scriptsigs）仅使用标准事务表单，则返回true
     **/

bool AreInputsStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs);
    /*
     *检查事务是否超过标准p2wsh资源限制：
     *3600字节见证脚本大小，每个见证堆栈元素80字节，100个见证堆栈元素
     *这些限制适用于使用op_checksig、op_add和op_equal的最多100个签名中的n个，
     **/

bool IsWitnessStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs);

extern CFeeRate incrementalRelayFee;
extern CFeeRate dustRelayFee;
extern unsigned int nBytesPerSigOp;

/*计算虚拟事务大小（权重重新解释为字节）。*/
int64_t GetVirtualTransactionSize(int64_t nWeight, int64_t nSigOpCost);
int64_t GetVirtualTransactionSize(const CTransaction& tx, int64_t nSigOpCost = 0);
int64_t GetVirtualTransactionInputSize(const CTxIn& tx, int64_t nSigOpCost = 0);

#endif //比特币政策
