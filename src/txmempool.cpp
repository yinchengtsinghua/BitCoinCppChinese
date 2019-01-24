
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

#include <txmempool.h>

#include <consensus/consensus.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <validation.h>
#include <policy/policy.h>
#include <policy/fees.h>
#include <reverse_iterator.h>
#include <streams.h>
#include <timedata.h>
#include <util/system.h>
#include <util/moneystr.h>
#include <util/time.h>

CTxMemPoolEntry::CTxMemPoolEntry(const CTransactionRef& _tx, const CAmount& _nFee,
                                 int64_t _nTime, unsigned int _entryHeight,
                                 bool _spendsCoinbase, int64_t _sigOpsCost, LockPoints lp)
    : tx(_tx), nFee(_nFee), nTxWeight(GetTransactionWeight(*tx)), nUsageSize(RecursiveDynamicUsage(tx)), nTime(_nTime), entryHeight(_entryHeight),
    spendsCoinbase(_spendsCoinbase), sigOpCost(_sigOpsCost), lockPoints(lp)
{
    nCountWithDescendants = 1;
    nSizeWithDescendants = GetTxSize();
    nModFeesWithDescendants = nFee;

    feeDelta = 0;

    nCountWithAncestors = 1;
    nSizeWithAncestors = GetTxSize();
    nModFeesWithAncestors = nFee;
    nSigOpCostWithAncestors = sigOpCost;
}

void CTxMemPoolEntry::UpdateFeeDelta(int64_t newFeeDelta)
{
    nModFeesWithDescendants += newFeeDelta - feeDelta;
    nModFeesWithAncestors += newFeeDelta - feeDelta;
    feeDelta = newFeeDelta;
}

void CTxMemPoolEntry::UpdateLockPoints(const LockPoints& lp)
{
    lockPoints = lp;
}

size_t CTxMemPoolEntry::GetTxSize() const
{
    return GetVirtualTransactionSize(nTxWeight, sigOpCost);
}

//为任何内存池子代更新给定的Tx。
//假设setmempoolchildren对于给定的tx和all是正确的
//后裔。
void CTxMemPool::UpdateForDescendants(txiter updateIt, cacheMap &cachedDescendants, const std::set<uint256> &setExclude)
{
    setEntries stageEntries, setAllDescendants;
    stageEntries = GetMemPoolChildren(updateIt);

    while (!stageEntries.empty()) {
        const txiter cit = *stageEntries.begin();
        setAllDescendants.insert(cit);
        stageEntries.erase(cit);
        const setEntries &setChildren = GetMemPoolChildren(cit);
        for (txiter childEntry : setChildren) {
            cacheMap::iterator cacheIt = cachedDescendants.find(childEntry);
            if (cacheIt != cachedDescendants.end()) {
//我们已经计算了这个，只需添加这个集合的条目
//但不要再横穿了。
                for (txiter cacheEntry : cacheIt->second) {
                    setAllDescendants.insert(cacheEntry);
                }
            } else if (!setAllDescendants.count(childEntry)) {
//后期处理计划
                stageEntries.insert(childEntry);
            }
        }
    }
//setAllDescendants现在包含updateit的所有内存池子代。
//更新并添加到缓存的子代映射
    int64_t modifySize = 0;
    CAmount modifyFee = 0;
    int64_t modifyCount = 0;
    for (txiter cit : setAllDescendants) {
        if (!setExclude.count(cit->GetTx().GetHash())) {
            modifySize += cit->GetTxSize();
            modifyFee += cit->GetModifiedFee();
            modifyCount++;
            cachedDescendants[updateIt].insert(cit);
//更新每个后代的祖先状态
            mapTx.modify(cit, update_ancestor_state(updateIt->GetTxSize(), updateIt->GetModifiedFee(), 1, updateIt->GetSigOpCost()));
        }
    }
    mapTx.modify(updateIt, update_descendant_state(modifySize, modifyFee, modifyCount));
}

//vhashestopdate是来自断开连接块的事务哈希集
//已重新添加到mempool中。
//对于每个条目，查找vhashestopdate之外的后代，以及
//将此类子代的费用/大小信息添加到父代。
//对于每个这样的后代，也要更新祖先状态以包含父代。
void CTxMemPool::UpdateTransactionsFromBlock(const std::vector<uint256> &vHashesToUpdate)
{
    LOCK(cs);
//对于vhashestopdate中的每个条目，将的集合存储在mempool中，但不存储在mempool中。
//在vhashestopdate事务中，这样我们就不必重新计算
//当我们遇到以前看到的条目时，后代。
    cacheMap mapMemPoolDescendantsToUpdate;

//使用集合查找vhashestopdate（这些条目已经
//在他们的祖先的状态中）
    std::set<uint256> setAlreadyIncluded(vHashesToUpdate.begin(), vHashesToUpdate.end());

//反向迭代，这样每当我们查看事务时
//我们确信所有内存池中的后代都已被处理。
//这样可以最大化子代缓存的好处，并确保
//将更新setmempoolchildren，在
//更新荧光剂。
    for (const uint256 &hash : reverse_iterate(vHashesToUpdate)) {
//我们缓存内存池中的子项以避免重复的更新
        setEntries setChildren;
//从mapnextx计算子级
        txiter it = mapTx.find(hash);
        if (it == mapTx.end()) {
            continue;
        }
        auto iter = mapNextTx.lower_bound(COutPoint(hash, 0));
//首先计算子级，并将setmempoolchildren更新为
//包括它们，并更新它们的setmempoolparents以包含此tx。
        for (; iter != mapNextTx.end() && iter->first->hash == hash; ++iter) {
            const uint256 &childHash = iter->second->GetHash();
            txiter childIter = mapTx.find(childHash);
            assert(childIter != mapTx.end());
//我们可以跳过更新以前遇到的条目
//在块中（已说明）。
            if (setChildren.insert(childIter).second && !setAlreadyIncluded.count(childHash)) {
                UpdateChild(it, childIter, true);
                UpdateParent(childIter, it, true);
            }
        }
        UpdateForDescendants(it, mapMemPoolDescendantsToUpdate, setAlreadyIncluded);
    }
}

/*l ctxmempool:：calculatemempoolancestors（const ctxmempool entry&entry，setentries&setprecestors，uint64_t limitancestorcount，uint64_t limitancestorsize，uint64_t limitDescendantCount，uint64_t limitDescendantSize，std:：string&errstring，bool fsearchforparents/*=true*/）const
{
    setEntries父哈希；
    const ctransaction&tx=entry.gettx（）；

    如果（家长专用）
        //获取mempool中此事务的父级
        //GetMemPoolParents（）仅对MemPool中的条目有效，因此我们
        //迭代maptx以查找父级。
        for（unsigned int i=0；i<tx.vin.size（）；i++）
            boost：：可选<txter>piter=getiter（tx.vin[i].prevout.hash）；
            如果（PITER）{
                parenthashes.insert（*piter）；
                if（parentHashes.size（）+1>limitanceStorCount）
                    errstring=strprintf（“太多未确认的父项[限制：%u]”，limitancestorcount）；
                    返回错误；
                }
            }
        }
    }否则{
        //如果我们不寻找父母，我们要求这是一个
        //已在mempool中输入。
        txter it=maptx.iterator_to（entry）；
        parenthashes=getmempoolparents（it）；
    }

    size_t totalsizewith祖先=entry.gettxsize（）；

    而（！）parenthashes.empty（））
        txter statgeit=*parenthashes.begin（）；

        插入（stageit）；
        parenthashes.erase（stageit）；
        totalSizeWith祖先+=stageit->getTxsize（）；

        if（stageit->getsizeWithDescendants（）+entry.gettxsize（）>limitDescendantSize）
            errstring=strprintf（“超过了tx%s的后代大小限制[限制：%u]”，statgeit->gettx（）.gethash（）.toString（），limitDescendantSize）；
            返回错误；
        else if（stageit->getCountWithDescendants（）+1>limitDescendantCount）
            errstring=strprintf（“Tx%s的后代太多[限制：%u]”，statgeit->gettx（）.gethash（）.toString（），limitDescendantCount）；
            返回错误；
        else if（totalsizewith祖先>限制存储大小）
            errstring=strprintf（“超过祖先大小限制[限制：%u]”，limit ancestor size）；
            返回错误；
        }

        const setEntries&setMemPoolParents=getMemPoolParents（阶段）；
        对于（txter phash:setmempoolparents）
            //如果这是新的祖先，请添加它。
            if（set祖先.count（phash）==0）
                parenthashes.insert（phash）；
            }
            if（parentHashes.size（）+setAncestors.size（）+1>limitancestorCount）
                errstring=strprintf（“太多未确认的祖先[限制：%u]”，limitancestorcount）；
                返回错误；
            }
        }
    }

    回归真实；
}

void ctxmempool:：updateancestorsof（bool add、txter it、setentries和set祖先）
{
    setEntries parentiters=getMempoolParents（it）；
    //添加或删除此tx作为每个父级的子级
    对于（txter piter:parentiters）
        updatechild（piter，it，add）；
    }
    const int64_t updateCount=（添加？1：-1）；
    const int64_t updatesize=updateCount*it->gettxsize（）；
    const camount updatefee=updateCount*it->getModifiedFee（）；
    对于（txter ancestorit:set祖先）
        modify（ancestorit，update_子体_状态（updatesize，updatefee，updatecount））；
    }
}

void ctxmempool:：updateentryforancestors（txter it，const setentries&set祖先）
{
    int64_t updateCount=setAncestors.size（）；
    int64_t updatesize=0；
    camount updatefee=0；
    int64_t updatesigopscost=0；
    对于（txter ancestorit:set祖先）
        updatesize+=ancestorit->gettxsize（）；
        updatefee+=ancestorit->getModifiedFee（）；
        updatesigopscost+=ancestorit->getsigocopost（）；
    }
    modify（it，update_祖先_状态（updatesize，updatefee，updatecount，updatesigopscost））；
}

void ctxmempool:：updateChildrenforRemoval（txter it）
{
    const setentries&setmempoolchildren=getmempoolchildren（it）；
    对于（txter updateit:setmempoolchildren）
        updateParent（updateit，it，false）；
    }
}

void ctxmempool:：updateforremovefrommempool（const setentries&entriestoremove，bool updatescendants）
{
    //对于每个条目，返回与此关联的所有祖先和递减大小
    / /交易
    const uint64_t nnolimit=std:：numeric_limits<uint64_t>：：max（）；
    if（更新子体）
        //只要不是递归的，updateScendants就应该为true
        //删除Tx及其所有子代，例如当事务
        //在块中确认。
        //这里我们只更新统计数据，而不更新maplinks中的数据（其中
        //在完成所有
        //需要遍历mempool）。
        对于（txter removeit:entriestoremove）
            setEntries设置子体；
            计算后代（removeit，setDescendants）；
            setDescendants.erase（removeit）；//不更新自身的状态
            int64_t modifysize=-（（int64_t）removeit->gettxsize（））；
            camount modifyfee=-removeit->getmodifiedfee（）；
            int modifysigops=-removeit->getsigocopost（）；
            对于（txter dit:setDescendants）
                modify（dit，更新_祖先_状态（modifysize，modifyfee，-1，modifysigops））；
            }
        }
    }
    对于（txter removeit:entriestoremove）
        setEntries设置祖先；
        const ctxmempoolEntry&entry=*removeit；
        std：：字符串虚拟；
        //因为这是一个已经在mempool中的Tx，所以我们可以调用cmpa
        //使用fsearchforparents=false。如果mempool在
        //state，那么使用true或false应该都是正确的，尽管false
        //应该快一点。
        //但是，如果我们碰巧正在处理REORG，那么
        //内存池可能处于不一致状态。在这种情况下，集合
        //可通过maplinks访问的祖先将与
        //其包中包含此事务的祖先，因为当我们
        //在addunchecked（）中向mempool添加新事务，我们假定它
        //没有子级，在REORG的情况下，假设为
        //false，内存池中的子项没有链接到内存块Tx的
        //直到调用UpdateTransactionsFromBlock（）。
        //所以，如果我们在REORG期间被调用，
        //已调用UpdateTransactionsFromBlock（），则MapLinks[]将
        //与我们通过搜索计算的mempool父对象集不同，
        //重要的是我们使用maplinks[]的祖先概念
        //事务作为要更新以便删除的一组内容。
        计算的Mempoolancestors（entry、setPrecestors、nOnLimit、nOnLimit、nOnLimit、nOnLimit、dummy、false）；
        //请注意，updateancestorsof切断指向的子链接
        //在removeit父级的条目中删除。
        updateancestorsof（false，removeit，set祖先）；
    }
    //在更新了所有祖先大小之后，我们现在可以切断
    //正在删除的事务和任何mempool子级（即，更新setmempoolparents
    //对于要删除的事务的每个直接子级）。
    对于（txter removeit:entriestoremove）
        更新要删除的子级（removeit）；
    }
}

void ctxmempoolEntry:：updatedDescendantState（int64_t ModifySize，camount ModifyFee，int64_t ModifyCount）
{
    nsizeWithDescendants+=修改大小；
    断言（Int64_t（nsizeWithDescendants）>0）；
    nmodFeesWithDescendants+=修改fee；
    ncountWithDescendants+=修改计数；
    断言（int64_t（ncountWithDescendants）>0）；
}

void ctxmempoolEntry:：updateancestorstate（int64_t modifySize，camount modifyFee，int64_t modifyCount，int64_t modifySigops）
{
    nsizewith祖先+=修改；
    断言（int64_t（nsizewith祖先）>0）；
    nmodfeeswithancestors+=修改fee；
    ncountWithPrecestors+=修改计数；
    断言（Int64_t（ncountWithPrecestors）>0）；
    nsigopcostwith祖先+=修改igops；
    断言（int（nsigopcostwith祖先）>=0）；
}

ctxmempool:：ctxmempool（cBlockPolicyEstimator*Estimator）：
    合同执行日期（0），MinerPolicyEstimator（Estimator）
{
    _clear（）；//无锁清除

    //默认情况下，健全性会检查性能，否则
    //接受事务变为o（n^2），其中n是数字
    //池中的事务数
    ncheckfrequency=0；
}

bool ctxmempool:：isspent（const coutpoint&outpoint）const
{
    锁（CS）；
    返回mapnextx.count（outpoint）；
}

unsigned int ctxmempool:：getTransactionsupdate（）常量
{
    锁（CS）；
    退换货合同日期；
}

void ctxmempool:：addTransactionsUpdated（unsigned int n）
{
    锁（CS）；
    合同执行日期：+=n；
}

void ctxmempool:：addunchecked（const ctxmempoolEntry&Entry、setEntries&setAncestors、bool validFeeEstimate）
{
    notifyEntryAdded（entry.getSharedTx（））；
    //添加到内存池而不检查任何内容。
    //由acceptToMoryPool（）使用，它可以
    //所有适当的检查。
    indexed_transaction_set:：iterator newit=maptx.insert（entry）.first；
    maplinks.insert（make_pair（newit，txlinks（））；

    //更新由PrioritiseTransaction创建的任何feedelta的事务
    //TODO:重构，以便在插入之前计算费用增量
    //进入MAPTX。
    camount delta 0
    applydelta（entry.gettx（）.gethash（），delta）；
    如果（δ）{
            maptx.modify（newit，更新_fee_delta（delta））；
    }

    //更新cachedinnerusage以包括包含的事务的用法。
    //（更新内存池父项中的项时，内存使用率将为
    //进一步更新。）
    cachedinnerusage+=entry.dynamicmemoryusage（）；

    const ctransaction&tx=newit->gettx（）；
    std:：set<uint256>setParentTransactions；
    for（unsigned int i=0；i<tx.vin.size（）；i++）
        mapnextx.insert（std:：make_pair（&tx.vin[i].prevout，&tx））；
        setParentTransactions.insert（tx.vin[i].prevout.hash）；
    }
    //不用担心这个事务的子事务。
    //新事务到达的正常情况是
    //孩子，因为这样的孩子会成为孤儿。
    //一个例外是，如果一个事务进入了以前在块中的事务。
    //在这种情况下，我们的断开块逻辑将调用updateTransactionsFromBlock
    //清理我们要留下的烂摊子。

    //使用有关此tx的信息更新祖先
    for（const auto&pit:getiterset（setparenttransactions））
            更新父级（newit，pit，true）；
    }
    更新存储（true、newit、set祖先）；
    更新entryForancestors（newit，set祖先）；

    合同执行日期：++；
    totaltxsize+=entry.gettxsize（）；
    if（minerpolicyEstimator）minerpolicyEstimator->processTransaction（entry，validFeeEstimate）；

    vtxhash.emplace_back（tx.getwitnesshash（），newit）；
    newit->vtxhashesidx=vtxhash.size（）-1；
}

void ctxmempool:：removeunchecked（txter it，mempoolremovalreason原因）
{
    notifyEntryRemoved（it->GetSharedTX（），reason）；
    const uint256 hash=it->gettx（）.gethash（）；
    for（const ctxin&txin:it->gettx（）.vin）
        mapnextx.erase（txin.prevout）删除；

    if（vtxhash.size（）>1）
        vtxHash[it->vtxHashesidx]=std：：move（vtxHash.back（））；
        vtxhashesidx.second->vtxhashesidx=it->vtxhashesidx；
        vtxhash.pop_back（）；
        if（vtxhash.size（）*2<vtxhash.capacity（））
            vtxhash.shrink_to_fit（）；
    }其他
        vtxHash.clear（）；

    totaltxsize-=it->gettxsize（）；
    cachedinnerusage-=it->dynamicmemoryusage（）；
    cachedinnerusage-=memusage:：dynamicusage（maplinks[it].父级）+memusage:：dynamicusage（maplinks[it].子级）；
    删除（it）；
    删除（it）；
    合同执行日期：++；
    if（minerpolicyEstimator）minerpolicyEstimator->removetx（hash，false）；
}

//计算不在setDescendants中的项的后代，并将其添加到
//setDescendants。假设entryit已经是mempool和setmempoolchildren中的Tx
//对于Tx和所有后代都正确。
//还假定如果一个条目已经在setDescendants中，那么
//在mempool中，它的后代也已经在setDescendants中，因此我们
//不迭代这些条目可以节省时间。
void ctxmempool:：calculateDescendants（txter entryit、setEntries和setDescendants）常量
{
    setentries阶段；
    if（setDescendants.count（entryit）==0）
        阶段.插入（entryit）；
    }
    //遍历entry的子级，只添加
    //已经在setDescendants中进行了说明（因为这些子级
    //已被遍历，或将在此迭代中被遍历）。
    而（！）stage.empty（））
        txter it=*阶段.begin（）；
        setDescendants.insert（it）；
        阶段。删除（it）；

        const setentries&setchildren=getmempoolchildren（it）；
        对于（txter childiter:setchildren）
            如果（！）setDescendants.count（childiter））
                阶段插入（childiter）；
            }
        }
    }
}

void ctxmempool:：removerecursive（const ctransaction&origtx，mempoolremovalreason原因）
{
    //从内存池中删除事务
    {
        锁（CS）；
        设置条目txtoremove；
        txter origit=maptx.find（origtx.gethash（））；
        如果（起源）！=maptx.end（））
            txtoremove.insert（origit）；
        }否则{
            //递归删除但origtx不在mempool中时
            //确保删除池中的所有子级。这个罐头
            //如果origtx未被接受，则在链重新组织期间发生
            //内存池。
            for（unsigned int i=0；i<origtx.vout.size（）；i++）
                auto it=mapnextx.find（coutpoint（origtx.gethash（），i））；
                if（it==mapnextx.end（））
                    继续；
                txter nextit=maptx.find（it->second->gethash（））；
                断言（NexTyt）！= MATX.Enter（））；
                txtoremove.insert（下一个）；
            }
        }
        setEntries集合删除；
        对于（txter it:txtoremove）
            计算后代（it，setallremoves）；
        }

        移除（setallremoves，false，reason）；
    }
}

void ctxmempool:：removeforreorg（const ccoinsviewcache*pcoins，unsigned int nmompolheight，int flags）
{
    //删除使用CoinBase的事务，该CoinBase现在不成熟，不再是最终事务
    锁（CS）；
    设置条目txtoremove；
    for（indexed_transaction_set:：const_iterator it=maptx.begin（）；it！=maptx.end（）；it++）
        const ctransaction&tx=it->gettx（）；
        lockpoints lp=it->getlockpoints（）；
        bool validlp=测试锁定点有效性（&lp）；
        如果（！）检查finaltx（tx，flags）！checkSequenceLocks（*this，tx，flags，&lp，validlp））
            //注意，如果checkSequenceLocks失败，则锁定点可能仍然无效
            //所以我们必须移除tx，而不是依赖于锁定点。
            txtoremove.插入（it）；
        else if（it->GetSpendsCoinBase（））
            用于（const ctxin&txin:tx.vin）
                indexed_transaction_set:：const_iterator it2=maptx.find（txin.prevout.hash）；
                如果（IT2）！= MATX.Enter（）
                    继续；
                const coin&coin=pcoins->accesscoin（txin.prevent）；
                如果（ncheckfrequency！= 0）断言（！coin.isspent（））；
                if（coin.isspent（）（coin.iscoinBase（）&&（（signed long）nmompolheight）-coin.nheight<coinBase_maturity））
                    txtoremove.插入（it）；
                    断裂；
                }
            }
        }
        如果（！）验证LP）{
            maptx.modify（it，更新_lock_points（lp））；
        }
    }
    setEntries集合删除；
    对于（txter it:txtoremove）
        计算后代（it，setallremoves）；
    }
    已移除（setAllRemoves，false，memPoolRemovalReason:：reorg）；
}

void ctxmempool:：removeconflicts（const ctransaction&tx）
{
    //递归删除依赖于Tx输入的事务
    断言锁定（cs）；
    用于（const ctxin&txin:tx.vin）
        auto it=mapnextx.find（txin.prevout）；
        如果（它）！=mapnextx.end（））
            const ctransaction&txconflict=*it->second；
            如果（txconflict！= TX）
            {
                ClearPrioritization（txConflict.getHash（））；
                removerecursive（txconflict，mempoolremovalreason:：conflict）；
            }
        }
    }
}

/**
 *连接块时调用。从mempool中删除并更新矿工费用估算器。
 **/

void CTxMemPool::removeForBlock(const std::vector<CTransactionRef>& vtx, unsigned int nBlockHeight)
{
    LOCK(cs);
    std::vector<const CTxMemPoolEntry*> entries;
    for (const auto& tx : vtx)
    {
        uint256 hash = tx->GetHash();

        indexed_transaction_set::iterator i = mapTx.find(hash);
        if (i != mapTx.end())
            entries.push_back(&*i);
    }
//在从mempool中删除新块中的tx之前，请更新策略估计
    if (minerPolicyEstimator) {minerPolicyEstimator->processBlock(nBlockHeight, entries);}
    for (const auto& tx : vtx)
    {
        txiter it = mapTx.find(tx->GetHash());
        if (it != mapTx.end()) {
            setEntries stage;
            stage.insert(it);
            RemoveStaged(stage, true, MemPoolRemovalReason::BLOCK);
        }
        removeConflicts(*tx);
        ClearPrioritisation(tx->GetHash());
    }
    lastRollingFeeUpdate = GetTime();
    blockSinceLastRollingFeeBump = true;
}

void CTxMemPool::_clear()
{
    mapLinks.clear();
    mapTx.clear();
    mapNextTx.clear();
    totalTxSize = 0;
    cachedInnerUsage = 0;
    lastRollingFeeUpdate = GetTime();
    blockSinceLastRollingFeeBump = false;
    rollingMinimumFeeRate = 0;
    ++nTransactionsUpdated;
}

void CTxMemPool::clear()
{
    LOCK(cs);
    _clear();
}

static void CheckInputsAndUpdateCoins(const CTransaction& tx, CCoinsViewCache& mempoolDuplicate, const int64_t spendheight)
{
    CValidationState state;
    CAmount txfee = 0;
    bool fCheckResult = tx.IsCoinBase() || Consensus::CheckTxInputs(tx, state, mempoolDuplicate, spendheight, txfee);
    assert(fCheckResult);
    UpdateCoins(tx, mempoolDuplicate, 1000000);
}

void CTxMemPool::check(const CCoinsViewCache *pcoins) const
{
    LOCK(cs);
    if (nCheckFrequency == 0)
        return;

    if (GetRand(std::numeric_limits<uint32_t>::max()) >= nCheckFrequency)
        return;

    LogPrint(BCLog::MEMPOOL, "Checking mempool with %u transactions and %u inputs\n", (unsigned int)mapTx.size(), (unsigned int)mapNextTx.size());

    uint64_t checkTotal = 0;
    uint64_t innerUsage = 0;

    CCoinsViewCache mempoolDuplicate(const_cast<CCoinsViewCache*>(pcoins));
    const int64_t spendheight = GetSpendHeight(mempoolDuplicate);

    std::list<const CTxMemPoolEntry*> waitingOnDependants;
    for (indexed_transaction_set::const_iterator it = mapTx.begin(); it != mapTx.end(); it++) {
        unsigned int i = 0;
        checkTotal += it->GetTxSize();
        innerUsage += it->DynamicMemoryUsage();
        const CTransaction& tx = it->GetTx();
        txlinksMap::const_iterator linksiter = mapLinks.find(it);
        assert(linksiter != mapLinks.end());
        const TxLinks &links = linksiter->second;
        innerUsage += memusage::DynamicUsage(links.parents) + memusage::DynamicUsage(links.children);
        bool fDependsWait = false;
        setEntries setParentCheck;
        for (const CTxIn &txin : tx.vin) {
//检查每个mempool事务的输入是否指向可用的硬币或其他mempool tx。
            indexed_transaction_set::const_iterator it2 = mapTx.find(txin.prevout.hash);
            if (it2 != mapTx.end()) {
                const CTransaction& tx2 = it2->GetTx();
                assert(tx2.vout.size() > txin.prevout.n && !tx2.vout[txin.prevout.n].IsNull());
                fDependsWait = true;
                setParentCheck.insert(it2);
            } else {
                assert(pcoins->HaveCoin(txin.prevout));
            }
//检查其输入是否在mapnextx中标记。
            auto it3 = mapNextTx.find(txin.prevout);
            assert(it3 != mapNextTx.end());
            assert(it3->first == &txin.prevout);
            assert(it3->second == &tx);
            i++;
        }
        assert(setParentCheck == GetMemPoolParents(it));
//验证祖先状态是否正确。
        setEntries setAncestors;
        uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
        std::string dummy;
        CalculateMemPoolAncestors(*it, setAncestors, nNoLimit, nNoLimit, nNoLimit, nNoLimit, dummy);
        uint64_t nCountCheck = setAncestors.size() + 1;
        uint64_t nSizeCheck = it->GetTxSize();
        CAmount nFeesCheck = it->GetModifiedFee();
        int64_t nSigOpCheck = it->GetSigOpCost();

        for (txiter ancestorIt : setAncestors) {
            nSizeCheck += ancestorIt->GetTxSize();
            nFeesCheck += ancestorIt->GetModifiedFee();
            nSigOpCheck += ancestorIt->GetSigOpCost();
        }

        assert(it->GetCountWithAncestors() == nCountCheck);
        assert(it->GetSizeWithAncestors() == nSizeCheck);
        assert(it->GetSigOpCostWithAncestors() == nSigOpCheck);
        assert(it->GetModFeesWithAncestors() == nFeesCheck);

//根据mapnextx检查子项
        CTxMemPool::setEntries setChildrenCheck;
        auto iter = mapNextTx.lower_bound(COutPoint(it->GetTx().GetHash(), 0));
        uint64_t child_sizes = 0;
        for (; iter != mapNextTx.end() && iter->first->hash == it->GetTx().GetHash(); ++iter) {
            txiter childit = mapTx.find(iter->second->GetHash());
assert(childit != mapTx.end()); //mapnextx指向内存池中的事务
            if (setChildrenCheck.insert(childit).second) {
                child_sizes += childit->GetTxSize();
            }
        }
        assert(setChildrenCheck == GetMemPoolChildren(it));
//还要检查以确保大小大于直接子级的和。
//只是一个健全的检查，不确定这个计算是正确的…
        assert(it->GetSizeWithDescendants() >= child_sizes + it->GetTxSize());

        if (fDependsWait)
            waitingOnDependants.push_back(&(*it));
        else {
            CheckInputsAndUpdateCoins(tx, mempoolDuplicate, spendheight);
        }
    }
    unsigned int stepsSinceLastRemove = 0;
    while (!waitingOnDependants.empty()) {
        const CTxMemPoolEntry* entry = waitingOnDependants.front();
        waitingOnDependants.pop_front();
        if (!mempoolDuplicate.HaveInputs(entry->GetTx())) {
            waitingOnDependants.push_back(entry);
            stepsSinceLastRemove++;
            assert(stepsSinceLastRemove < waitingOnDependants.size());
        } else {
            CheckInputsAndUpdateCoins(entry->GetTx(), mempoolDuplicate, spendheight);
            stepsSinceLastRemove = 0;
        }
    }
    for (auto it = mapNextTx.cbegin(); it != mapNextTx.cend(); it++) {
        uint256 hash = it->second->GetHash();
        indexed_transaction_set::const_iterator it2 = mapTx.find(hash);
        const CTransaction& tx = it2->GetTx();
        assert(it2 != mapTx.end());
        assert(&tx == it->second);
    }

    assert(totalTxSize == checkTotal);
    assert(innerUsage == cachedInnerUsage);
}

bool CTxMemPool::CompareDepthAndScore(const uint256& hasha, const uint256& hashb)
{
    LOCK(cs);
    indexed_transaction_set::const_iterator i = mapTx.find(hasha);
    if (i == mapTx.end()) return false;
    indexed_transaction_set::const_iterator j = mapTx.find(hashb);
    if (j == mapTx.end()) return true;
    uint64_t counta = i->GetCountWithAncestors();
    uint64_t countb = j->GetCountWithAncestors();
    if (counta == countb) {
        return CompareTxMemPoolEntryByScore()(*i, *j);
    }
    return counta < countb;
}

namespace {
class DepthAndScoreComparator
{
public:
    bool operator()(const CTxMemPool::indexed_transaction_set::const_iterator& a, const CTxMemPool::indexed_transaction_set::const_iterator& b)
    {
        uint64_t counta = a->GetCountWithAncestors();
        uint64_t countb = b->GetCountWithAncestors();
        if (counta == countb) {
            return CompareTxMemPoolEntryByScore()(*a, *b);
        }
        return counta < countb;
    }
};
} //命名空间

std::vector<CTxMemPool::indexed_transaction_set::const_iterator> CTxMemPool::GetSortedDepthAndScore() const
{
    std::vector<indexed_transaction_set::const_iterator> iters;
    AssertLockHeld(cs);

    iters.reserve(mapTx.size());

    for (indexed_transaction_set::iterator mi = mapTx.begin(); mi != mapTx.end(); ++mi) {
        iters.push_back(mi);
    }
    std::sort(iters.begin(), iters.end(), DepthAndScoreComparator());
    return iters;
}

void CTxMemPool::queryHashes(std::vector<uint256>& vtxid)
{
    LOCK(cs);
    auto iters = GetSortedDepthAndScore();

    vtxid.clear();
    vtxid.reserve(mapTx.size());

    for (auto it : iters) {
        vtxid.push_back(it->GetTx().GetHash());
    }
}

static TxMempoolInfo GetInfo(CTxMemPool::indexed_transaction_set::const_iterator it) {
    return TxMempoolInfo{it->GetSharedTx(), it->GetTime(), CFeeRate(it->GetFee(), it->GetTxSize()), it->GetModifiedFee() - it->GetFee()};
}

std::vector<TxMempoolInfo> CTxMemPool::infoAll() const
{
    LOCK(cs);
    auto iters = GetSortedDepthAndScore();

    std::vector<TxMempoolInfo> ret;
    ret.reserve(mapTx.size());
    for (auto it : iters) {
        ret.push_back(GetInfo(it));
    }

    return ret;
}

CTransactionRef CTxMemPool::get(const uint256& hash) const
{
    LOCK(cs);
    indexed_transaction_set::const_iterator i = mapTx.find(hash);
    if (i == mapTx.end())
        return nullptr;
    return i->GetSharedTx();
}

TxMempoolInfo CTxMemPool::info(const uint256& hash) const
{
    LOCK(cs);
    indexed_transaction_set::const_iterator i = mapTx.find(hash);
    if (i == mapTx.end())
        return TxMempoolInfo();
    return GetInfo(i);
}

void CTxMemPool::PrioritiseTransaction(const uint256& hash, const CAmount& nFeeDelta)
{
    {
        LOCK(cs);
        CAmount &delta = mapDeltas[hash];
        delta += nFeeDelta;
        txiter it = mapTx.find(hash);
        if (it != mapTx.end()) {
            mapTx.modify(it, update_fee_delta(delta));
//现在用后代更新所有祖先的修改费用
            setEntries setAncestors;
            uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
            std::string dummy;
            CalculateMemPoolAncestors(*it, setAncestors, nNoLimit, nNoLimit, nNoLimit, nNoLimit, dummy, false);
            for (txiter ancestorIt : setAncestors) {
                mapTx.modify(ancestorIt, update_descendant_state(0, nFeeDelta, 0));
            }
//现在用祖先更新所有后代的修改费用
            setEntries setDescendants;
            CalculateDescendants(it, setDescendants);
            setDescendants.erase(it);
            for (txiter descendantIt : setDescendants) {
                mapTx.modify(descendantIt, update_ancestor_state(0, nFeeDelta, 0, 0));
            }
            ++nTransactionsUpdated;
        }
    }
    LogPrintf("PrioritiseTransaction: %s feerate += %s\n", hash.ToString(), FormatMoney(nFeeDelta));
}

void CTxMemPool::ApplyDelta(const uint256 hash, CAmount &nFeeDelta) const
{
    LOCK(cs);
    std::map<uint256, CAmount>::const_iterator pos = mapDeltas.find(hash);
    if (pos == mapDeltas.end())
        return;
    const CAmount &delta = pos->second;
    nFeeDelta += delta;
}

void CTxMemPool::ClearPrioritisation(const uint256 hash)
{
    LOCK(cs);
    mapDeltas.erase(hash);
}

const CTransaction* CTxMemPool::GetConflictTx(const COutPoint& prevout) const
{
    const auto it = mapNextTx.find(prevout);
    return it == mapNextTx.end() ? nullptr : it->second;
}

boost::optional<CTxMemPool::txiter> CTxMemPool::GetIter(const uint256& txid) const
{
    auto it = mapTx.find(txid);
    if (it != mapTx.end()) return it;
    return boost::optional<txiter>{};
}

CTxMemPool::setEntries CTxMemPool::GetIterSet(const std::set<uint256>& hashes) const
{
    CTxMemPool::setEntries ret;
    for (const auto& h : hashes) {
        const auto mi = GetIter(h);
        if (mi) ret.insert(*mi);
    }
    return ret;
}

bool CTxMemPool::HasNoInputsOf(const CTransaction &tx) const
{
    for (unsigned int i = 0; i < tx.vin.size(); i++)
        if (exists(tx.vin[i].prevout.hash))
            return false;
    return true;
}

CCoinsViewMemPool::CCoinsViewMemPool(CCoinsView* baseIn, const CTxMemPool& mempoolIn) : CCoinsViewBacked(baseIn), mempool(mempoolIn) { }

bool CCoinsViewMemPool::GetCoin(const COutPoint &outpoint, Coin &coin) const {
//如果内存池中存在某个条目，请始终返回该条目，因为它保证永远不会
//与基础缓存冲突，它不能有修剪的条目（因为它包含完整的条目）
//交易。首先检查底层缓存可能会返回修剪后的条目。
    CTransactionRef ptx = mempool.get(outpoint.hash);
    if (ptx) {
        if (outpoint.n < ptx->vout.size()) {
            coin = Coin(ptx->vout[outpoint.n], MEMPOOL_HEIGHT, false);
            return true;
        } else {
            return false;
        }
    }
    return base->GetCoin(outpoint, coin);
}

size_t CTxMemPool::DynamicMemoryUsage() const {
    LOCK(cs);
//估计maptx的开销为12个指针+一个分配，因为没有实现包含boost：：multi_index_的精确公式。
    return memusage::MallocUsage(sizeof(CTxMemPoolEntry) + 12 * sizeof(void*)) * mapTx.size() + memusage::DynamicUsage(mapNextTx) + memusage::DynamicUsage(mapDeltas) + memusage::DynamicUsage(mapLinks) + memusage::DynamicUsage(vTxHashes) + cachedInnerUsage;
}

void CTxMemPool::RemoveStaged(setEntries &stage, bool updateDescendants, MemPoolRemovalReason reason) {
    AssertLockHeld(cs);
    UpdateForRemoveFromMempool(stage, updateDescendants);
    for (txiter it : stage) {
        removeUnchecked(it, reason);
    }
}

int CTxMemPool::Expire(int64_t time) {
    LOCK(cs);
    indexed_transaction_set::index<entry_time>::type::iterator it = mapTx.get<entry_time>().begin();
    setEntries toremove;
    while (it != mapTx.get<entry_time>().end() && it->GetTime() < time) {
        toremove.insert(mapTx.project<0>(it));
        it++;
    }
    setEntries stage;
    for (txiter removeit : toremove) {
        CalculateDescendants(removeit, stage);
    }
    RemoveStaged(stage, false, MemPoolRemovalReason::EXPIRY);
    return stage.size();
}

void CTxMemPool::addUnchecked(const CTxMemPoolEntry &entry, bool validFeeEstimate)
{
    setEntries setAncestors;
    uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
    std::string dummy;
    CalculateMemPoolAncestors(entry, setAncestors, nNoLimit, nNoLimit, nNoLimit, nNoLimit, dummy);
    return addUnchecked(entry, setAncestors, validFeeEstimate);
}

void CTxMemPool::UpdateChild(txiter entry, txiter child, bool add)
{
    setEntries s;
    if (add && mapLinks[entry].children.insert(child).second) {
        cachedInnerUsage += memusage::IncrementalDynamicUsage(s);
    } else if (!add && mapLinks[entry].children.erase(child)) {
        cachedInnerUsage -= memusage::IncrementalDynamicUsage(s);
    }
}

void CTxMemPool::UpdateParent(txiter entry, txiter parent, bool add)
{
    setEntries s;
    if (add && mapLinks[entry].parents.insert(parent).second) {
        cachedInnerUsage += memusage::IncrementalDynamicUsage(s);
    } else if (!add && mapLinks[entry].parents.erase(parent)) {
        cachedInnerUsage -= memusage::IncrementalDynamicUsage(s);
    }
}

const CTxMemPool::setEntries & CTxMemPool::GetMemPoolParents(txiter entry) const
{
    assert (entry != mapTx.end());
    txlinksMap::const_iterator it = mapLinks.find(entry);
    assert(it != mapLinks.end());
    return it->second.parents;
}

const CTxMemPool::setEntries & CTxMemPool::GetMemPoolChildren(txiter entry) const
{
    assert (entry != mapTx.end());
    txlinksMap::const_iterator it = mapLinks.find(entry);
    assert(it != mapLinks.end());
    return it->second.children;
}

CFeeRate CTxMemPool::GetMinFee(size_t sizelimit) const {
    LOCK(cs);
    if (!blockSinceLastRollingFeeBump || rollingMinimumFeeRate == 0)
        return CFeeRate(llround(rollingMinimumFeeRate));

    int64_t time = GetTime();
    if (time > lastRollingFeeUpdate + 10) {
        double halflife = ROLLING_FEE_HALFLIFE;
        if (DynamicMemoryUsage() < sizelimit / 4)
            halflife /= 4;
        else if (DynamicMemoryUsage() < sizelimit / 2)
            halflife /= 2;

        rollingMinimumFeeRate = rollingMinimumFeeRate / pow(2.0, (time - lastRollingFeeUpdate) / halflife);
        lastRollingFeeUpdate = time;

        if (rollingMinimumFeeRate < (double)incrementalRelayFee.GetFeePerK() / 2) {
            rollingMinimumFeeRate = 0;
            return CFeeRate(0);
        }
    }
    return std::max(CFeeRate(llround(rollingMinimumFeeRate)), incrementalRelayFee);
}

void CTxMemPool::trackPackageRemoved(const CFeeRate& rate) {
    AssertLockHeld(cs);
    if (rate.GetFeePerK() > rollingMinimumFeeRate) {
        rollingMinimumFeeRate = rate.GetFeePerK();
        blockSinceLastRollingFeeBump = false;
    }
}

void CTxMemPool::TrimToSize(size_t sizelimit, std::vector<COutPoint>* pvNoSpendsRemaining) {
    LOCK(cs);

    unsigned nTxnRemoved = 0;
    CFeeRate maxFeeRateRemoved(0);
    while (!mapTx.empty() && DynamicMemoryUsage() > sizelimit) {
        indexed_transaction_set::index<descendant_score>::type::iterator it = mapTx.get<descendant_score>().begin();

//我们将新的mempool最低费用设置为已删除集的费用，再加上
//“最低合理费率”（即我们考虑TXN的某个价值
//不收费）。这样，我们就不允许txn和feerate一起进入mempool。
//等于Txn，两者之间没有阻塞。
        CFeeRate removed(it->GetModFeesWithDescendants(), it->GetSizeWithDescendants());
        removed += incrementalRelayFee;
        trackPackageRemoved(removed);
        maxFeeRateRemoved = std::max(maxFeeRateRemoved, removed);

        setEntries stage;
        CalculateDescendants(mapTx.project<0>(it), stage);
        nTxnRemoved += stage.size();

        std::vector<CTransaction> txn;
        if (pvNoSpendsRemaining) {
            txn.reserve(stage.size());
            for (txiter iter : stage)
                txn.push_back(iter->GetTx());
        }
        RemoveStaged(stage, false, MemPoolRemovalReason::SIZELIMIT);
        if (pvNoSpendsRemaining) {
            for (const CTransaction& tx : txn) {
                for (const CTxIn& txin : tx.vin) {
                    if (exists(txin.prevout.hash)) continue;
                    pvNoSpendsRemaining->push_back(txin.prevout);
                }
            }
        }
    }

    if (maxFeeRateRemoved > CFeeRate(0)) {
        LogPrint(BCLog::MEMPOOL, "Removed %u txn, rolling minimum fee bumped to %s\n", nTxnRemoved, maxFeeRateRemoved.ToString());
    }
}

uint64_t CTxMemPool::CalculateDescendantMaximum(txiter entry) const {
//查找子代计数最高的父代
    std::vector<txiter> candidates;
    setEntries counted;
    candidates.push_back(entry);
    uint64_t maximum = 0;
    while (candidates.size()) {
        txiter candidate = candidates.back();
        candidates.pop_back();
        if (!counted.insert(candidate).second) continue;
        const setEntries& parents = GetMemPoolParents(candidate);
        if (parents.size() == 0) {
            maximum = std::max(maximum, candidate->GetCountWithDescendants());
        } else {
            for (txiter i : parents) {
                candidates.push_back(i);
            }
        }
    }
    return maximum;
}

void CTxMemPool::GetTransactionAncestry(const uint256& txid, size_t& ancestors, size_t& descendants) const {
    LOCK(cs);
    auto it = mapTx.find(txid);
    ancestors = descendants = 0;
    if (it != mapTx.end()) {
        ancestors = it->GetCountWithAncestors();
        descendants = CalculateDescendantMaximum(it);
    }
}

SaltedTxidHasher::SaltedTxidHasher() : k0(GetRand(std::numeric_limits<uint64_t>::max())), k1(GetRand(std::numeric_limits<uint64_t>::max())) {}
