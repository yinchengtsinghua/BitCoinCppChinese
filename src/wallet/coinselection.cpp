
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2017-2018比特币核心开发商
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <wallet/coinselection.h>

#include <util/system.h>
#include <util/moneystr.h>

#include <boost/optional.hpp>

//降阶比较器
struct {
    bool operator()(const OutputGroup& a, const OutputGroup& b) const
    {
        return a.effective_value > b.effective_value;
    }
} descending;

/*
 *这是Murch设计的分支绑定硬币选择算法。它搜索输入
 *设置可支付支出目标，且不超过支出目标
 *创建和支出变更输出的成本。该算法对二进制文件使用深度优先搜索。
 *树。在二叉树中，每个节点对应于utxo的包含或省略。UTXOS
 *按其有效值排序，并根据包含内容对树进行确定性探索。
 *先分支。在每个节点上，算法检查选择是否在目标范围内。
 *虽然选择未达到目标范围，但包含更多的utxo。当选定内容
 *值超出目标范围，可省略从此选择派生的完整子树。
 *此时，取消选择最后一个包含的utxo，并探索相应的省略分支。
 *相反。搜索完成树后或经过有限的尝试后，搜索结束。
 *
 *找到一个解决方案后，搜索会继续搜索更好的解决方案。最好的
 *通过最小化浪费指标来选择解决方案。废物指标定义为
 *以给定的费率减去长期预期成本来花费当前投入
 *输入，加上所选金额超出支出目标：
 *
 *浪费=选择总计-目标+输入×（当前频率-长期频率）
 *
 *该算法使用了两个额外的优化。展望跟踪
 *未经探索的utxos。如果lookahead指示目标范围，则不探索子树。
 *无法联系。此外，无需测试等效组合。这让我们
 *跳过测试是否包含与省略项的有效值和浪费相匹配的utxo。
 *前身。
 *
 *分支定界算法在Murch的硕士论文中有详细描述：
 *https://murch.one/wp-content/uploads/2016/11/erhardt2016coinselection.pdf
 *
 *@param const std:：vector<cinputcoin>&utxo_pool我们正在选择的utxos集。
 *这些utxo将按有效值和cinputcoins的降序排序。
 *值是它们的有效值。
 *@param const camount&target_value这是我们要选择的值。它是较低的
 *范围的界限。
 *@param const camount&cost_change这是创建和花费更改输出的成本。
 *这个加上目标值是范围的上限。
 *@param std:：set<cinputcoin>&out\set->这是一组cinputcoins的输出参数
 *已选定。
 *@param camount&value_ret->这是Cinputcoins总值的输出参数
 *已选定。
 *@param camount not_input_fees->输出和固定大小需要支付的费用
 *开销（版本、锁定时间、标记和标志）
 **/


static const size_t TOTAL_TRIES = 100000;

bool SelectCoinsBnB(std::vector<OutputGroup>& utxo_pool, const CAmount& target_value, const CAmount& cost_of_change, std::set<CInputCoin>& out_set, CAmount& value_ret, CAmount not_input_fees)
{
    out_set.clear();
    CAmount curr_value = 0;

std::vector<bool> curr_selection; //在此索引中选择utxo
    curr_selection.reserve(utxo_pool.size());
    CAmount actual_target = not_input_fees + target_value;

//计算当前可用值
    CAmount curr_available_value = 0;
    for (const OutputGroup& utxo : utxo_pool) {
//断言这个utxo不是负数。它不应该是负数，有效值计算应该删除它。
        assert(utxo.effective_value > 0);
        curr_available_value += utxo.effective_value;
    }
    if (curr_available_value < actual_target) {
        return false;
    }

//分类utxo_池
    std::sort(utxo_pool.begin(), utxo_pool.end(), descending);

    CAmount curr_waste = 0;
    std::vector<bool> best_selection;
    CAmount best_waste = MAX_MONEY;

//选择utxos的深度优先搜索循环
    for (size_t i = 0; i < TOTAL_TRIES; ++i) {
//开始回溯的条件
        bool backtrack = false;
if (curr_value + curr_available_value < actual_target ||                //无法达到当前可用价值中剩余金额的目标。
curr_value > actual_target + cost_of_change ||    //所选值超出范围，请返回并尝试其他分支
(curr_waste > best_waste && (utxo_pool.at(0).fee - utxo_pool.at(0).long_term_fee) > 0)) { //不要选择那些我们知道如果浪费增加会更浪费的东西。
            backtrack = true;
} else if (curr_value >= actual_target) {       //所选值在范围内
curr_waste += (curr_value - actual_target); //这是为进行以下比较而添加到废物中的超额价值。
//如果长期费用高于当前费用，在检查后再添加一个utxo可以减少浪费。
//但是，我们不会去探索这一点，因为只有当我们达到目标时，才会对废物进行优化。
//价值。再加上任何一个utxo都只会烧掉utxo，而这完全是要收费的。所以我们不会
//探索更多的UTXO，以避免像那样烧钱。
            if (curr_waste <= best_waste) {
                best_selection = curr_selection;
                best_selection.resize(utxo_pool.size());
                best_waste = curr_waste;
            }
curr_waste -= (curr_value - actual_target); //去掉多余的价值，因为我们现在要选择不同的硬币
            backtrack = true;
        }

//回溯，向后移动
        if (backtrack) {
//向后走，找到最后一个包含的utxo，它仍然需要遍历其省略分支。
            while (!curr_selection.empty() && !curr_selection.back()) {
                curr_selection.pop_back();
                curr_available_value += utxo_pool.at(curr_selection.size()).effective_value;
            }

if (curr_selection.empty()) { //我们已经走回了第一个utxo，没有一个分支是不受欢迎的。已搜索所有解决方案
                break;
            }

//输出包含在以前的迭代中，请尝试立即排除。
            curr_selection.back() = false;
            OutputGroup& utxo = utxo_pool.at(curr_selection.size() - 1);
            curr_value -= utxo.effective_value;
            curr_waste -= utxo.fee - utxo.long_term_fee;
} else { //向前走，继续沿着这条路走
            OutputGroup& utxo = utxo_pool.at(curr_selection.size());

//将此utxo从当前可用的utxo值中删除
            curr_available_value -= utxo.effective_value;

//如果先前的utxo具有相同的值和相同的浪费并且被排除在外，则避免搜索分支。因为费用与
//长期费用是一样的，我们只需要检查其中一个值是否匹配就可以知道浪费是一样的。
            if (!curr_selection.empty() && !curr_selection.back() &&
                utxo.effective_value == utxo_pool.at(curr_selection.size() - 1).effective_value &&
                utxo.fee == utxo_pool.at(curr_selection.size() - 1).fee) {
                curr_selection.push_back(false);
            } else {
//包容性分支第一（最大的第一次勘探）
                curr_selection.push_back(true);
                curr_value += utxo.effective_value;
                curr_waste += utxo.fee - utxo.long_term_fee;
            }
        }
    }

//检查解决方案
    if (best_selection.empty()) {
        return false;
    }

//集合输出集合
    value_ret = 0;
    for (size_t i = 0; i < best_selection.size(); ++i) {
        if (best_selection.at(i)) {
            util::insert(out_set, utxo_pool.at(i).m_outputs);
            value_ret += utxo_pool.at(i).m_value;
        }
    }

    return true;
}

static void ApproximateBestSubset(const std::vector<OutputGroup>& groups, const CAmount& nTotalLower, const CAmount& nTargetValue,
                                  std::vector<char>& vfBest, CAmount& nBest, int iterations = 1000)
{
    std::vector<char> vfIncluded;

    vfBest.assign(groups.size(), true);
    nBest = nTotalLower;

    FastRandomContext insecure_rand;

    for (int nRep = 0; nRep < iterations && nBest != nTargetValue; nRep++)
    {
        vfIncluded.assign(groups.size(), false);
        CAmount nTotal = 0;
        bool fReachedTarget = false;
        for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++)
        {
            for (unsigned int i = 0; i < groups.size(); i++)
            {
//这里的解算器使用随机算法，
//随机性不是真正的安全目的，只是
//需要防止退化行为，这很重要
//RNG很快。我们不使用常数随机序列，
//因为通过
//随机选择。
                if (nPass == 0 ? insecure_rand.randbool() : !vfIncluded[i])
                {
                    nTotal += groups[i].m_value;
                    vfIncluded[i] = true;
                    if (nTotal >= nTargetValue)
                    {
                        fReachedTarget = true;
                        if (nTotal < nBest)
                        {
                            nBest = nTotal;
                            vfBest = vfIncluded;
                        }
                        nTotal -= groups[i].m_value;
                        vfIncluded[i] = false;
                    }
                }
            }
        }
    }
}

bool KnapsackSolver(const CAmount& nTargetValue, std::vector<OutputGroup>& groups, std::set<CInputCoin>& setCoinsRet, CAmount& nValueRet)
{
    setCoinsRet.clear();
    nValueRet = 0;

//小于目标值的列表
    boost::optional<OutputGroup> lowest_larger;
    std::vector<OutputGroup> applicable_groups;
    CAmount nTotalLower = 0;

    Shuffle(groups.begin(), groups.end(), FastRandomContext());

    for (const OutputGroup& group : groups) {
        if (group.m_value == nTargetValue) {
            util::insert(setCoinsRet, group.m_outputs);
            nValueRet += group.m_value;
            return true;
        } else if (group.m_value < nTargetValue + MIN_CHANGE) {
            applicable_groups.push_back(group);
            nTotalLower += group.m_value;
        } else if (!lowest_larger || group.m_value < lowest_larger->m_value) {
            lowest_larger = group;
        }
    }

    if (nTotalLower == nTargetValue) {
        for (const auto& group : applicable_groups) {
            util::insert(setCoinsRet, group.m_outputs);
            nValueRet += group.m_value;
        }
        return true;
    }

    if (nTotalLower < nTargetValue) {
        if (!lowest_larger) return false;
        util::insert(setCoinsRet, lowest_larger->m_outputs);
        nValueRet += lowest_larger->m_value;
        return true;
    }

//用随机逼近法求解子集和
    std::sort(applicable_groups.begin(), applicable_groups.end(), descending);
    std::vector<char> vfBest;
    CAmount nBest;

    ApproximateBestSubset(applicable_groups, nTotalLower, nTargetValue, vfBest, nBest);
    if (nBest != nTargetValue && nTotalLower >= nTargetValue + MIN_CHANGE) {
        ApproximateBestSubset(applicable_groups, nTotalLower, nTargetValue + MIN_CHANGE, vfBest, nBest);
    }

//如果我们有一个更大的硬币（或者随机近似没有找到一个好的解决方案，
//或者下一个更大的硬币更近），把更大的硬币还给我
    if (lowest_larger &&
        ((nBest != nTargetValue && nBest < nTargetValue + MIN_CHANGE) || lowest_larger->m_value <= nBest)) {
        util::insert(setCoinsRet, lowest_larger->m_outputs);
        nValueRet += lowest_larger->m_value;
    } else {
        for (unsigned int i = 0; i < applicable_groups.size(); i++) {
            if (vfBest[i]) {
                util::insert(setCoinsRet, applicable_groups[i].m_outputs);
                nValueRet += applicable_groups[i].m_value;
            }
        }

        if (LogAcceptCategory(BCLog::SELECTCOINS)) {
            /*打印（bclog:：selectcoins，“selectcoins（）最佳子集：”）；/*续*/
            for（unsigned int i=0；i<applicable_groups.size（）；i++）
                如果（vfbest[i]）；
                    logprint（bclog:：selectcoins，“%s”，formatmoney（适用的_groups[i].m_value））；/*续*/

                }
            }
            LogPrint(BCLog::SELECTCOINS, "total %s\n", FormatMoney(nBest));
        }
    }

    return true;
}

/*************************************************************************

 输出组

 *************************************************************************/


void OutputGroup::Insert(const CInputCoin& output, int depth, bool from_me, size_t ancestors, size_t descendants) {
    m_outputs.push_back(output);
    m_from_me &= from_me;
    m_value += output.effective_value;
    m_depth = std::min(m_depth, depth);
//这里的祖先表达了新硬币最终拥有的祖先数量，也就是说
//总和，而不是最大值；在多个输入的情况下，这将高估
//有共同的祖先
    m_ancestors += ancestors;
//后代是从上一个祖先看到的计数，而不是从上一个祖先看到的后代。
//硬币本身；因此，这个值被计算为最大值，而不是总和。
    m_descendants = std::max(m_descendants, descendants);
    effective_value = m_value;
}

std::vector<CInputCoin>::iterator OutputGroup::Discard(const CInputCoin& output) {
    auto it = m_outputs.begin();
    while (it != m_outputs.end() && it->outpoint != output.outpoint) ++it;
    if (it == m_outputs.end()) return it;
    m_value -= output.effective_value;
    effective_value -= output.effective_value;
    return m_outputs.erase(it);
}

bool OutputGroup::EligibleForSpending(const CoinEligibilityFilter& eligibility_filter) const
{
    return m_depth >= (m_from_me ? eligibility_filter.conf_mine : eligibility_filter.conf_theirs)
        && m_ancestors <= eligibility_filter.max_ancestors
        && m_descendants <= eligibility_filter.max_descendants;
}
