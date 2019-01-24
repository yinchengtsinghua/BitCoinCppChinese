
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

#ifndef BITCOIN_CHAIN_H
#define BITCOIN_CHAIN_H

#include <arith_uint256.h>
#include <consensus/params.h>
#include <primitives/block.h>
#include <tinyformat.h>
#include <uint256.h>

#include <vector>

/*
 *允许块时间戳超过
 *接受块之前的当前网络调整时间。
 **/

static constexpr int64_t MAX_FUTURE_BLOCK_TIME = 2 * 60 * 60;

/*
 *时间戳窗口用作比较外部代码的宽限期
 *时间戳（例如传递给RPC的时间戳，或钱包密钥创建时间）
 *阻止时间戳。此值应至少设置为
 *最大未来阻塞时间。
 **/

static constexpr int64_t TIMESTAMP_WINDOW = MAX_FUTURE_BLOCK_TIME;

/*
 *使用的节点时间和块时间之间的最大间隔
 *对于GUI中的“追赶…”模式。
 *
 *参考：https://github.com/bitcoin/bitcoin/pull/1026
 **/

static constexpr int64_t MAX_BLOCK_TIME_GAP = 90 * 60;

class CBlockFileInfo
{
public:
unsigned int nBlocks;      //！<文件中存储的块数
unsigned int nSize;        //！<块文件的已用字节数
unsigned int nUndoSize;    //！<撤消文件中使用的字节数
unsigned int nHeightFirst; //！<文件中块的最低高度
unsigned int nHeightLast;  //！<文件中块的最高高度
uint64_t nTimeFirst;       //！<文件中块的最早时间
uint64_t nTimeLast;        //！<文件中块的最新时间

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(VARINT(nBlocks));
        READWRITE(VARINT(nSize));
        READWRITE(VARINT(nUndoSize));
        READWRITE(VARINT(nHeightFirst));
        READWRITE(VARINT(nHeightLast));
        READWRITE(VARINT(nTimeFirst));
        READWRITE(VARINT(nTimeLast));
    }

     void SetNull() {
         nBlocks = 0;
         nSize = 0;
         nUndoSize = 0;
         nHeightFirst = 0;
         nHeightLast = 0;
         nTimeFirst = 0;
         nTimeLast = 0;
     }

     CBlockFileInfo() {
         SetNull();
     }

     std::string ToString() const;

     /*更新统计信息（不更新nsize）*/
     void AddBlock(unsigned int nHeightIn, uint64_t nTimeIn) {
         if (nBlocks==0 || nHeightFirst > nHeightIn)
             nHeightFirst = nHeightIn;
         if (nBlocks==0 || nTimeFirst > nTimeIn)
             nTimeFirst = nTimeIn;
         nBlocks++;
         if (nHeightIn > nHeightLast)
             nHeightLast = nHeightIn;
         if (nTimeIn > nTimeLast)
             nTimeLast = nTimeIn;
     }
};

struct CDiskBlockPos
{
    int nFile;
    unsigned int nPos;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(VARINT(nFile, VarIntMode::NONNEGATIVE_SIGNED));
        READWRITE(VARINT(nPos));
    }

    CDiskBlockPos() {
        SetNull();
    }

    CDiskBlockPos(int nFileIn, unsigned int nPosIn) {
        nFile = nFileIn;
        nPos = nPosIn;
    }

    friend bool operator==(const CDiskBlockPos &a, const CDiskBlockPos &b) {
        return (a.nFile == b.nFile && a.nPos == b.nPos);
    }

    friend bool operator!=(const CDiskBlockPos &a, const CDiskBlockPos &b) {
        return !(a == b);
    }

    void SetNull() { nFile = -1; nPos = 0; }
    bool IsNull() const { return (nFile == -1); }

    std::string ToString() const
    {
        return strprintf("CDiskBlockPos(nFile=%i, nPos=%i)", nFile, nPos);
    }

};

enum BlockStatus: uint32_t {
//！未使用的
    BLOCK_VALID_UNKNOWN      =    0,

//！已解析，版本正常，哈希满足声明的POW，1<=vtx count<=max，时间戳不在将来
    BLOCK_VALID_HEADER       =    1,

//！找到所有父标题，困难匹配，时间戳>=前一个中位数，检查点。意味着所有的父母
//！至少也是树。
    BLOCK_VALID_TREE         =    2,

    /*
     *只有第一个Tx是CoinBase，2<=CoinBase输入脚本长度<=100，事务有效，没有重复的TxID，
     *sigops、size、merkle root。意味着所有的父母至少都是树，但不一定是交易。当一切
     *父块也有事务，将设置cBlockIndex:：nchaintx。
     **/

    BLOCK_VALID_TRANSACTIONS =    3,

//！输出不超支输入，没有双重支出，coinbase输出OK，没有未成熟的coinbase支出，bip30。
//！意味着所有的父母至少都是连锁的。
    BLOCK_VALID_CHAIN        =    4,

//！脚本和签名可以。意味着所有的父母至少也是脚本。
    BLOCK_VALID_SCRIPTS      =    5,

//！所有有效位。
    BLOCK_VALID_MASK         =   BLOCK_VALID_HEADER | BLOCK_VALID_TREE | BLOCK_VALID_TRANSACTIONS |
                                 BLOCK_VALID_CHAIN | BLOCK_VALID_SCRIPTS,

BLOCK_HAVE_DATA          =    8, //！<BLK*.DAT中提供完整块
BLOCK_HAVE_UNDO          =   16, //！<undo data available in rev*.dat
    BLOCK_HAVE_MASK          =   BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO,

BLOCK_FAILED_VALID       =   32, //！<最后达到有效性后的阶段失败
BLOCK_FAILED_CHILD       =   64, //！<从失败块下降
    BLOCK_FAILED_MASK        =   BLOCK_FAILED_VALID | BLOCK_FAILED_CHILD,

BLOCK_OPT_WITNESS       =   128, //！<BLK中的块数据。数据是通过证人强制客户端接收的。
};

/*区块链是一个树形结构，从
 *根的Genesis块，每个块可能有多个
 *候选人将成为下一个街区。块索引可以有多个pprev指向
 *到它，但其中至多有一个可以是当前活动分支的一部分。
 **/

class CBlockIndex
{
public:
//！指向块散列（如果有）的指针。内存属于此cblockindex
    const uint256* phashBlock;

//！指向此块的前辈的索引的指针
    CBlockIndex* pprev;

//！指向此块的其他前一个块的索引的指针
    CBlockIndex* pskip;

//！链中入口的高度。Genesis区块高度为0
    int nHeight;

//！此块存储在哪个文件中（BLK？？？？？？？DAT）
    int nFile;

//！字节偏移量在BLK内？？？？？？？.dat存储此块数据的位置
    unsigned int nDataPos;

//！字节偏移在rev内？？？？？？？.dat存储此块的撤消数据的位置
    unsigned int nUndoPos;

//！（仅限内存）在此块之前（包括此块）的链中的总工作量（哈希的预期数量）
    arith_uint256 nChainWork;

//！此块中的事务数。
//！注意：在“潜在邮件头优先”模式中，不能依赖此号码。
    unsigned int nTx;

//！（仅限内存）此块之前（包括此块）链中的事务数。
//！只有当且仅当此块及其所有父级的事务都可用时，此值才会为非零。
//！必要时更改为64位类型；2030年之前不会发生
    unsigned int nChainTx;

//！此块的验证状态。请参见枚举块状态
    uint32_t nStatus;

//！块标题
    int32_t nVersion;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;

//！（仅限内存）为区分接收块的顺序而分配的顺序ID。
    int32_t nSequenceId;

//！（仅限内存）链中最长的时间（包括此块）。
    unsigned int nTimeMax;

    void SetNull()
    {
        phashBlock = nullptr;
        pprev = nullptr;
        pskip = nullptr;
        nHeight = 0;
        nFile = 0;
        nDataPos = 0;
        nUndoPos = 0;
        nChainWork = arith_uint256();
        nTx = 0;
        nChainTx = 0;
        nStatus = 0;
        nSequenceId = 0;
        nTimeMax = 0;

        nVersion       = 0;
        hashMerkleRoot = uint256();
        nTime          = 0;
        nBits          = 0;
        nNonce         = 0;
    }

    CBlockIndex()
    {
        SetNull();
    }

    explicit CBlockIndex(const CBlockHeader& block)
    {
        SetNull();

        nVersion       = block.nVersion;
        hashMerkleRoot = block.hashMerkleRoot;
        nTime          = block.nTime;
        nBits          = block.nBits;
        nNonce         = block.nNonce;
    }

    CDiskBlockPos GetBlockPos() const {
        CDiskBlockPos ret;
        if (nStatus & BLOCK_HAVE_DATA) {
            ret.nFile = nFile;
            ret.nPos  = nDataPos;
        }
        return ret;
    }

    CDiskBlockPos GetUndoPos() const {
        CDiskBlockPos ret;
        if (nStatus & BLOCK_HAVE_UNDO) {
            ret.nFile = nFile;
            ret.nPos  = nUndoPos;
        }
        return ret;
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        if (pprev)
            block.hashPrevBlock = pprev->GetBlockHash();
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        return block;
    }

    uint256 GetBlockHash() const
    {
        return *phashBlock;
    }

    /*
     *检查此块和以前所有块的事务是否
     *在某个时间点下载（并存储到磁盘）。
     *
     *并不意味着交易是一致有效的（ConnectTip可能失败）
     *并不意味着事务仍存储在磁盘上。（IsBlockPruned可能返回true）
     **/

    bool HaveTxsDownloaded() const { return nChainTx != 0; }

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    int64_t GetBlockTimeMax() const
    {
        return (int64_t)nTimeMax;
    }

    static constexpr int nMedianTimeSpan = 11;

    int64_t GetMedianTimePast() const
    {
        int64_t pmedian[nMedianTimeSpan];
        int64_t* pbegin = &pmedian[nMedianTimeSpan];
        int64_t* pend = &pmedian[nMedianTimeSpan];

        const CBlockIndex* pindex = this;
        for (int i = 0; i < nMedianTimeSpan && pindex; i++, pindex = pindex->pprev)
            *(--pbegin) = pindex->GetBlockTime();

        std::sort(pbegin, pend);
        return pbegin[(pend - pbegin)/2];
    }

    std::string ToString() const
    {
        return strprintf("CBlockIndex(pprev=%p, nHeight=%d, merkle=%s, hashBlock=%s)",
            pprev, nHeight,
            hashMerkleRoot.ToString(),
            GetBlockHash().ToString());
    }

//！检查此块索引项在通过的有效级别之前是否有效。
    bool IsValid(enum BlockStatus nUpTo = BLOCK_VALID_TRANSACTIONS) const
    {
assert(!(nUpTo & ~BLOCK_VALID_MASK)); //只允许有效标志。
        if (nStatus & BLOCK_FAILED_MASK)
            return false;
        return ((nStatus & BLOCK_VALID_MASK) >= nUpTo);
    }

//！提高此块索引项的有效级别。
//！如果有效性已更改，则返回true。
    bool RaiseValidity(enum BlockStatus nUpTo)
    {
assert(!(nUpTo & ~BLOCK_VALID_MASK)); //只允许有效标志。
        if (nStatus & BLOCK_FAILED_MASK)
            return false;
        if ((nStatus & BLOCK_VALID_MASK) < nUpTo) {
            nStatus = (nStatus & ~BLOCK_VALID_MASK) | nUpTo;
            return true;
        }
        return false;
    }

//！为此项生成SkipList指针。
    void BuildSkip();

//！有效地找到这个块的祖先。
    CBlockIndex* GetAncestor(int height);
    const CBlockIndex* GetAncestor(int height) const;
};

arith_uint256 GetBlockProof(const CBlockIndex& block);
/*返回恢复“从”和“到”之间的工作差异所需的时间，假设当前哈希率与Tip处的难度相对应，以秒为单位。*/
int64_t GetBlockProofEquivalentTime(const CBlockIndex& to, const CBlockIndex& from, const CBlockIndex& tip, const Consensus::Params&);
/*找到两个链尖之间的分叉点。*/
const CBlockIndex* LastCommonAncestor(const CBlockIndex* pa, const CBlockIndex* pb);


/*用于将指针封送到数据库存储的哈希中。*/
class CDiskBlockIndex : public CBlockIndex
{
public:
    uint256 hashPrev;

    CDiskBlockIndex() {
        hashPrev = uint256();
    }

    explicit CDiskBlockIndex(const CBlockIndex* pindex) : CBlockIndex(*pindex) {
        hashPrev = (pprev ? pprev->GetBlockHash() : uint256());
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int _nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(VARINT(_nVersion, VarIntMode::NONNEGATIVE_SIGNED));

        READWRITE(VARINT(nHeight, VarIntMode::NONNEGATIVE_SIGNED));
        READWRITE(VARINT(nStatus));
        READWRITE(VARINT(nTx));
        if (nStatus & (BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO))
            READWRITE(VARINT(nFile, VarIntMode::NONNEGATIVE_SIGNED));
        if (nStatus & BLOCK_HAVE_DATA)
            READWRITE(VARINT(nDataPos));
        if (nStatus & BLOCK_HAVE_UNDO)
            READWRITE(VARINT(nUndoPos));

//块标题
        READWRITE(this->nVersion);
        READWRITE(hashPrev);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
    }

    uint256 GetBlockHash() const
    {
        CBlockHeader block;
        block.nVersion        = nVersion;
        block.hashPrevBlock   = hashPrev;
        block.hashMerkleRoot  = hashMerkleRoot;
        block.nTime           = nTime;
        block.nBits           = nBits;
        block.nNonce          = nNonce;
        return block.GetHash();
    }


    std::string ToString() const
    {
        std::string str = "CDiskBlockIndex(";
        str += CBlockIndex::ToString();
        str += strprintf("\n                hashBlock=%s, hashPrev=%s)",
            GetBlockHash().ToString(),
            hashPrev.ToString());
        return str;
    }
};

/*内存中索引的块链。*/
class CChain {
private:
    std::vector<CBlockIndex*> vChain;

public:
    /*返回此链的Genesis块的索引项，如果没有，则返回nullptr。*/
    CBlockIndex *Genesis() const {
        return vChain.size() > 0 ? vChain[0] : nullptr;
    }

    /*返回此链顶端的索引项，如果没有，则返回nullptr。*/
    CBlockIndex *Tip() const {
        return vChain.size() > 0 ? vChain[vChain.size() - 1] : nullptr;
    }

    /*返回此链中特定高度的索引项，如果不存在此类高度，则返回nullptr。*/
    CBlockIndex *operator[](int nHeight) const {
        if (nHeight < 0 || nHeight >= (int)vChain.size())
            return nullptr;
        return vChain[nHeight];
    }

    /*有效地比较两条链。*/
    friend bool operator==(const CChain &a, const CChain &b) {
        return a.vChain.size() == b.vChain.size() &&
               a.vChain[a.vChain.size() - 1] == b.vChain[b.vChain.size() - 1];
    }

    /*有效地检查此链中是否存在块。*/
    bool Contains(const CBlockIndex *pindex) const {
        return (*this)[pindex->nHeight] == pindex;
    }

    /*查找此链中某个块的后续块，如果找不到给定的索引或该索引是提示，则为nullptr。*/
    CBlockIndex *Next(const CBlockIndex *pindex) const {
        if (Contains(pindex))
            return (*this)[pindex->nHeight + 1];
        else
            return nullptr;
    }

    /*返回链中的最大高度。等于chain.tip（）？chain.tip（）->nheight:-1.*/
    int Height() const {
        return vChain.size() - 1;
    }

    /*使用给定的提示设置/初始化链。*/
    void SetTip(CBlockIndex *pindex);

    /*返回引用此链中的块的cblocklocator（默认情况下为tip）。*/
    CBlockLocator GetLocator(const CBlockIndex *pindex = nullptr) const;

    /*查找此链和块索引项之间的最后一个公共块。*/
    const CBlockIndex *FindFork(const CBlockIndex *pindex) const;

    /*查找时间戳等于或大于给定时间戳的最早块。*/
    CBlockIndex* FindEarliestAtLeast(int64_t nTime) const;
};

#endif //比特科因
