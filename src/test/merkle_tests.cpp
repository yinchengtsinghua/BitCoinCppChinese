
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2015-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <consensus/merkle.h>
#include <test/test_bitcoin.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(merkle_tests, TestingSetup)

static uint256 ComputeMerkleRootFromBranch(const uint256& leaf, const std::vector<uint256>& vMerkleBranch, uint32_t nIndex) {
    uint256 hash = leaf;
    for (std::vector<uint256>::const_iterator it = vMerkleBranch.begin(); it != vMerkleBranch.end(); ++it) {
        if (nIndex & 1) {
            hash = Hash(it->begin(), it->end(), hash.begin(), hash.end());
        } else {
            hash = Hash(hash.begin(), hash.end(), it->begin(), it->end());
        }
        nIndex >>= 1;
    }
    return hash;
}

/*这实现了一个常量空间merkle根/路径计算器，限制为2^32叶。*/
static void MerkleComputation(const std::vector<uint256>& leaves, uint256* proot, bool* pmutated, uint32_t branchpos, std::vector<uint256>* pbranch) {
    if (pbranch) pbranch->clear();
    if (leaves.size() == 0) {
        if (pmutated) *pmutated = false;
        if (proot) *proot = uint256();
        return;
    }
    bool mutated = false;
//Count是迄今处理的叶数。
    uint32_t count = 0;
//inner是一个热切计算的子树散列数组，由树索引。
//级别（0是叶）。
//例如，当count为25（二进制为11001）时，inner[4]是
//前16个叶，后8个叶的内部[3]和内部[0]等于
//最后一片叶子。其他内部条目未定义。
    uint256 inner[32];
//内部的哪个位置是散列，它取决于匹配的叶。
    int matchlevel = -1;
//首先，将所有的叶子处理成“内部”值。
    while (count < leaves.size()) {
        uint256 h = leaves[count];
        bool matchh = count == branchpos;
        count++;
        int level;
//对于计数中的每一个低位0，执行1步。各
//对应于处理前存在的内部值
//当前叶，每个叶都需要一个散列来组合它。
        for (level = 0; !(count & (((uint32_t)1) << level)); level++) {
            if (pbranch) {
                if (matchh) {
                    pbranch->push_back(inner[level]);
                } else if (matchlevel == level) {
                    pbranch->push_back(h);
                    matchh = true;
                }
            }
            mutated |= (inner[level] == h);
            CHash256().Write(inner[level].begin(), 32).Write(h.begin(), 32).Finalize(h.begin());
        }
//将结果哈希存储在内部位置级别。
        inner[level] = h;
        if (matchh) {
            matchlevel = level;
        }
    }
//对要处理的树的最右分支执行最后一次“扫描”
//奇数级别，并将所有内容减少到一个最高值。
//水平是指我们扫到的水平面（从底部算起）。
    int level = 0;
//只要计数中的位数字级别为零，就跳过它。意思是那里
//在这个水平上什么都没有。
    while (!(count & (((uint32_t)1) << level))) {
        level++;
    }
    uint256 h = inner[level];
    bool matchh = matchlevel == level;
    while (count != (((uint32_t)1) << level)) {
//如果我们达到这一点，h是一个内部值，而不是顶部。
//我们将其与自身结合起来（比特币的特殊规则，用于
//树）产生一个更高的层次。
        if (pbranch && matchh) {
            pbranch->push_back(h);
        }
        CHash256().Write(h.begin(), 32).Write(h.begin(), 32).Finalize(h.begin());
//将计数增加到如果在该位置有两个条目时它将具有的值
//等级已经存在。
        count += (((uint32_t)1) << level);
        level++;
//并相应地向上传播结果。
        while (!(count & (((uint32_t)1) << level))) {
            if (pbranch) {
                if (matchh) {
                    pbranch->push_back(inner[level]);
                } else if (matchlevel == level) {
                    pbranch->push_back(h);
                    matchh = true;
                }
            }
            CHash256().Write(inner[level].begin(), 32).Write(h.begin(), 32).Finalize(h.begin());
            level++;
        }
    }
//返回结果。
    if (pmutated) *pmutated = mutated;
    if (proot) *proot = h;
}

static std::vector<uint256> ComputeMerkleBranch(const std::vector<uint256>& leaves, uint32_t position) {
    std::vector<uint256> ret;
    MerkleComputation(leaves, nullptr, nullptr, position, &ret);
    return ret;
}

static std::vector<uint256> BlockMerkleBranch(const CBlock& block, uint32_t position)
{
    std::vector<uint256> leaves;
    leaves.resize(block.vtx.size());
    for (size_t s = 0; s < block.vtx.size(); s++) {
        leaves[s] = block.vtx[s]->GetHash();
    }
    return ComputeMerkleBranch(leaves, position);
}

//用于比较的旧版Merkle根计算代码。
static uint256 BlockBuildMerkleTree(const CBlock& block, bool* fMutated, std::vector<uint256>& vMerkleTree)
{
    vMerkleTree.clear();
vMerkleTree.reserve(block.vtx.size() * 2 + 16); //总节点数的安全上限。
    for (std::vector<CTransactionRef>::const_iterator it(block.vtx.begin()); it != block.vtx.end(); ++it)
        vMerkleTree.push_back((*it)->GetHash());
    int j = 0;
    bool mutated = false;
    for (int nSize = block.vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
    {
        for (int i = 0; i < nSize; i += 2)
        {
            int i2 = std::min(i+1, nSize-1);
            if (i2 == i + 1 && i2 + 1 == nSize && vMerkleTree[j+i] == vMerkleTree[j+i2]) {
//在特定级别的列表末尾有两个相同的哈希。
                mutated = true;
            }
            vMerkleTree.push_back(Hash(vMerkleTree[j+i].begin(), vMerkleTree[j+i].end(),
                                       vMerkleTree[j+i2].begin(), vMerkleTree[j+i2].end()));
        }
        j += nSize;
    }
    if (fMutated) {
        *fMutated = mutated;
    }
    return (vMerkleTree.empty() ? uint256() : vMerkleTree.back());
}

//用于比较的旧版Merkle分支计算代码。
static std::vector<uint256> BlockGetMerkleBranch(const CBlock& block, const std::vector<uint256>& vMerkleTree, int nIndex)
{
    std::vector<uint256> vMerkleBranch;
    int j = 0;
    for (int nSize = block.vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
    {
        int i = std::min(nIndex^1, nSize-1);
        vMerkleBranch.push_back(vMerkleTree[j+i]);
        nIndex >>= 1;
        j += nSize;
    }
    return vMerkleBranch;
}

static inline int ctz(uint32_t i) {
    if (i == 0) return 0;
    int j = 0;
    while (!(i & 1)) {
        j++;
        i >>= 1;
    }
    return j;
}

BOOST_AUTO_TEST_CASE(merkle_test)
{
    for (int i = 0; i < 32; i++) {
//尝试32个块大小：所有大小从0到16（包括0到16），然后15个随机大小。
        int ntx = (i <= 16) ? i : 17 + (InsecureRandRange(4000));
//尝试多达3个突变。
        for (int mutate = 0; mutate <= 3; mutate++) {
int duplicate1 = mutate >= 1 ? 1 << ctz(ntx) : 0; //最后要先复制多少个事务。
if (duplicate1 >= ntx) break; //整个树的重复会导致不同的根（它添加了一个级别）。
int ntx1 = ntx + duplicate1; //第一次复制后产生的事务数。
int duplicate2 = mutate >= 2 ? 1 << ctz(ntx1) : 0; //第二次突变也是如此。
            if (duplicate2 >= ntx1) break;
            int ntx2 = ntx1 + duplicate2;
int duplicate3 = mutate >= 3 ? 1 << ctz(ntx2) : 0; //第三次突变。
            if (duplicate3 >= ntx2) break;
            int ntx3 = ntx2 + duplicate3;
//用NTX不同的事务构建一个块。
            CBlock block;
            block.vtx.resize(ntx);
            for (int j = 0; j < ntx; j++) {
                CMutableTransaction mtx;
                mtx.nLockTime = j;
                block.vtx[j] = MakeTransactionRef(std::move(mtx));
            }
//在对块进行变异之前，先计算它的根。
            bool unmutatedMutated = false;
            uint256 unmutatedRoot = BlockMerkleRoot(block, &unmutatedMutated);
            BOOST_CHECK(unmutatedMutated == false);
//或者，通过复制最后的事务进行变异，从而产生相同的merkle根。
            block.vtx.resize(ntx3);
            for (int j = 0; j < duplicate1; j++) {
                block.vtx[ntx + j] = block.vtx[ntx + j - duplicate1];
            }
            for (int j = 0; j < duplicate2; j++) {
                block.vtx[ntx1 + j] = block.vtx[ntx1 + j - duplicate2];
            }
            for (int j = 0; j < duplicate3; j++) {
                block.vtx[ntx2 + j] = block.vtx[ntx2 + j - duplicate3];
            }
//使用旧机制计算merkle根和merkle树。
            bool oldMutated = false;
            std::vector<uint256> merkleTree;
            uint256 oldRoot = BlockBuildMerkleTree(block, &oldMutated, merkleTree);
//使用新机制计算merkle根。
            bool newMutated = false;
            uint256 newRoot = BlockMerkleRoot(block, &newMutated);
            BOOST_CHECK(oldRoot == newRoot);
            BOOST_CHECK(newRoot == unmutatedRoot);
            BOOST_CHECK((newRoot == uint256()) == (ntx == 0));
            BOOST_CHECK(oldMutated == newMutated);
            BOOST_CHECK(newMutated == !!mutate);
//如果没有突变（每个NTX值一次），尝试多达16个分支。
            if (mutate == 0) {
                for (int loop = 0; loop < std::min(ntx, 16); loop++) {
//如果ntx<=16，尝试所有分支。否则，尝试16个随机的。
                    int mtx = loop;
                    if (ntx > 16) {
                        mtx = InsecureRandRange(ntx);
                    }
                    std::vector<uint256> newBranch = BlockMerkleBranch(block, mtx);
                    std::vector<uint256> oldBranch = BlockGetMerkleBranch(block, merkleTree, mtx);
                    BOOST_CHECK(oldBranch == newBranch);
                    BOOST_CHECK(ComputeMerkleRootFromBranch(block.vtx[mtx]->GetHash(), newBranch, mtx) == oldRoot);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
