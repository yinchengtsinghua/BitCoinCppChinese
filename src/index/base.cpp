
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

#include <chainparams.h>
#include <index/base.h>
#include <shutdown.h>
#include <tinyformat.h>
#include <ui_interface.h>
#include <util/system.h>
#include <validation.h>
#include <warnings.h>

constexpr char DB_BEST_BLOCK = 'B';

constexpr int64_t SYNC_LOG_INTERVAL = 30; //秒
constexpr int64_t SYNC_LOCATOR_WRITE_INTERVAL = 30; //秒

template<typename... Args>
static void FatalError(const char* fmt, const Args&... args)
{
    std::string strMessage = tfm::format(fmt, args...);
    SetMiscWarning(strMessage);
    LogPrintf("*** %s\n", strMessage);
    uiInterface.ThreadSafeMessageBox(
        "Error: A fatal internal error occurred, see debug.log for details",
        "", CClientUIInterface::MSG_ERROR);
    StartShutdown();
}

BaseIndex::DB::DB(const fs::path& path, size_t n_cache_size, bool f_memory, bool f_wipe, bool f_obfuscate) :
    CDBWrapper(path, n_cache_size, f_memory, f_wipe, f_obfuscate)
{}

bool BaseIndex::DB::ReadBestBlock(CBlockLocator& locator) const
{
    bool success = Read(DB_BEST_BLOCK, locator);
    if (!success) {
        locator.SetNull();
    }
    return success;
}

bool BaseIndex::DB::WriteBestBlock(const CBlockLocator& locator)
{
    return Write(DB_BEST_BLOCK, locator);
}

BaseIndex::~BaseIndex()
{
    Interrupt();
    Stop();
}

bool BaseIndex::Init()
{
    CBlockLocator locator;
    if (!GetDB().ReadBestBlock(locator)) {
        locator.SetNull();
    }

    LOCK(cs_main);
    if (locator.IsNull()) {
        m_best_block_index = nullptr;
    } else {
        m_best_block_index = FindForkInGlobalIndex(chainActive, locator);
    }
    m_synced = m_best_block_index.load() == chainActive.Tip();
    return true;
}

static const CBlockIndex* NextSyncBlock(const CBlockIndex* pindex_prev) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    if (!pindex_prev) {
        return chainActive.Genesis();
    }

    const CBlockIndex* pindex = chainActive.Next(pindex_prev);
    if (pindex) {
        return pindex;
    }

    return chainActive.Next(chainActive.FindFork(pindex_prev));
}

void BaseIndex::ThreadSync()
{
    const CBlockIndex* pindex = m_best_block_index.load();
    if (!m_synced) {
        auto& consensus_params = Params().GetConsensus();

        int64_t last_log_time = 0;
        int64_t last_locator_write_time = 0;
        while (true) {
            if (m_interrupt) {
                WriteBestBlock(pindex);
                return;
            }

            {
                LOCK(cs_main);
                const CBlockIndex* pindex_next = NextSyncBlock(pindex);
                if (!pindex_next) {
                    WriteBestBlock(pindex);
                    m_best_block_index = pindex;
                    m_synced = true;
                    break;
                }
                pindex = pindex_next;
            }

            int64_t current_time = GetTime();
            if (last_log_time + SYNC_LOG_INTERVAL < current_time) {
                LogPrintf("Syncing %s with block chain from height %d\n",
                          GetName(), pindex->nHeight);
                last_log_time = current_time;
            }

            if (last_locator_write_time + SYNC_LOCATOR_WRITE_INTERVAL < current_time) {
                WriteBestBlock(pindex);
                last_locator_write_time = current_time;
            }

            CBlock block;
            if (!ReadBlockFromDisk(block, pindex, consensus_params)) {
                FatalError("%s: Failed to read block %s from disk",
                           __func__, pindex->GetBlockHash().ToString());
                return;
            }
            if (!WriteBlock(block, pindex)) {
                FatalError("%s: Failed to write block %s to index database",
                           __func__, pindex->GetBlockHash().ToString());
                return;
            }
        }
    }

    if (pindex) {
        LogPrintf("%s is enabled at height %d\n", GetName(), pindex->nHeight);
    } else {
        LogPrintf("%s is enabled\n", GetName());
    }
}

bool BaseIndex::WriteBestBlock(const CBlockIndex* block_index)
{
    LOCK(cs_main);
    if (!GetDB().WriteBestBlock(chainActive.GetLocator(block_index))) {
        return error("%s: Failed to write locator to disk", __func__);
    }
    return true;
}

void BaseIndex::BlockConnected(const std::shared_ptr<const CBlock>& block, const CBlockIndex* pindex,
                               const std::vector<CTransactionRef>& txn_conflicted)
{
    if (!m_synced) {
        return;
    }

    const CBlockIndex* best_block_index = m_best_block_index.load();
    if (!best_block_index) {
        if (pindex->nHeight != 0) {
            FatalError("%s: First block connected is not the genesis block (height=%d)",
                       __func__, pindex->nHeight);
            return;
        }
    } else {
//确保块连接到当前最佳块的祖先。应该是这样的
//大多数时间，但可能不会在同步线程启动并设置之后立即进行
//My同步。考虑存在REORG的情况，陈旧分支上的块是
//即使在同步线程已达到
//新的链条头。在这种不太可能发生的情况下，记录一个警告并让队列清除。
        if (best_block_index->GetAncestor(pindex->nHeight - 1) != pindex->pprev) {
            /*printf（“%s：警告：块%s未连接到”/*Continued*/
                      “已知最佳链（tip=%s）；不更新索引\n”，
                      _uufunc_uuu，pindex->getBlockHash（）.toString（），
                      最佳_block_index->GetBlockHash（）.ToString（））；
            返回；
        }
    }

    if（writeblock（*block，pindex））
        m_best_block_index=pindex；
    }否则{
        FatalError（“%s:未能将块%s写入索引”，
                   _uufunc_uuu，pindex->getBlockHash（）.toString（））；
        返回；
    }
}

void baseindex:：chainStateFlushed（const cBlockLocator和locator）
{
    如果（！）MySycEd）{
        返回；
    }

    const uint256&locator_tip_hash=locator.vhave.front（）；
    const cblockindex*定位器提示索引；
    {
        锁（CSKEMAN）；
        locator_tip_index=lookupblockindex（locator_tip_hash）；
    }

    如果（！）定位器提示
        fatalerrror（“%s:在定位器中找不到第一个块（hash=%s）”，
                   _uufunc_uuu，定位器_tip_hash.toString（））；
        返回；
    }

    //检查在BlockConnected之后是否接收到chainStateFlushed回调。支票可能会失败
    //在同步线程启动并设置m_同步后立即执行。考虑一下这种情况
    //存在reorg，过时分支上的块位于validationInterface队列中
    //即使在同步线程赶上了新的链提示之后，也会积压工作。在这不太可能
    //事件，记录警告并清除队列。
    const cblockindex*best_block_index=m_best_block_index.load（）；
    如果（最佳块索引->getAncestor（定位器提示索引->nheight）！=locator_tip_index）
        logprintf（“%s:警告：定位器包含块（hash=%s）不在已知最佳位置”/*续*/

                  "chain (tip=%s); not writing index locator\n",
                  __func__, locator_tip_hash.ToString(),
                  best_block_index->GetBlockHash().ToString());
        return;
    }

    if (!GetDB().WriteBestBlock(locator)) {
        error("%s: Failed to write locator to disk", __func__);
    }
}

bool BaseIndex::BlockUntilSyncedToCurrentChain()
{
    AssertLockNotHeld(cs_main);

    if (!m_synced) {
        return false;
    }

    {
//如果我们知道我们已经赶上了，那就不要排队排尿了。
//chainActive.tip（）。
        LOCK(cs_main);
        const CBlockIndex* chain_tip = chainActive.Tip();
        const CBlockIndex* best_block_index = m_best_block_index.load();
        if (best_block_index->GetAncestor(chain_tip->nHeight) == chain_tip) {
            return true;
        }
    }

    LogPrintf("%s: %s is catching up on block notifications\n", __func__, GetName());
    SyncWithValidationInterfaceQueue();
    return true;
}

void BaseIndex::Interrupt()
{
    m_interrupt();
}

void BaseIndex::Start()
{
//需要在运行init（）之前注册此validationInterface，以便
//如果init将m_同步设置为true，则不会错过回调。
    RegisterValidationInterface(this);
    if (!Init()) {
        FatalError("%s: %s failed to initialize", __func__, GetName());
        return;
    }

    m_thread_sync = std::thread(&TraceThread<std::function<void()>>, GetName(),
                                std::bind(&BaseIndex::ThreadSync, this));
}

void BaseIndex::Stop()
{
    UnregisterValidationInterface(this);

    if (m_thread_sync.joinable()) {
        m_thread_sync.join();
    }
}
