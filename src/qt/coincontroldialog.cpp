
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2011-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/coincontroldialog.h>
#include <qt/forms/ui_coincontroldialog.h>

#include <qt/addresstablemodel.h>
#include <base58.h>
#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <txmempool.h>
#include <qt/walletmodel.h>

#include <wallet/coincontrol.h>
#include <interfaces/node.h>
#include <key_io.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <validation.h> //内存池
#include <wallet/fees.h>
#include <wallet/wallet.h>

#include <QApplication>
#include <QCheckBox>
#include <QCursor>
#include <QDialogButtonBox>
#include <QFlags>
#include <QIcon>
#include <QSettings>
#include <QTreeWidget>

QList<CAmount> CoinControlDialog::payAmounts;
bool CoinControlDialog::fSubtractFeeFromAmount = false;

bool CCoinControlWidgetItem::operator<(const QTreeWidgetItem &other) const {
    int column = treeWidget()->sortColumn();
    if (column == CoinControlDialog::COLUMN_AMOUNT || column == CoinControlDialog::COLUMN_DATE || column == CoinControlDialog::COLUMN_CONFIRMATIONS)
        return data(column, Qt::UserRole).toLongLong() < other.data(column, Qt::UserRole).toLongLong();
    return QTreeWidgetItem::operator<(other);
}

CoinControlDialog::CoinControlDialog(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CoinControlDialog),
    model(nullptr),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);

//上下文菜单操作
    QAction *copyAddressAction = new QAction(tr("Copy address"), this);
    QAction *copyLabelAction = new QAction(tr("Copy label"), this);
    QAction *copyAmountAction = new QAction(tr("Copy amount"), this);
copyTransactionHashAction = new QAction(tr("Copy transaction ID"), this);  //我们需要启用/禁用这个
lockAction = new QAction(tr("Lock unspent"), this);                        //我们需要启用/禁用这个
unlockAction = new QAction(tr("Unlock unspent"), this);                    //我们需要启用/禁用这个

//上下文菜单
    contextMenu = new QMenu(this);
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(copyAmountAction);
    contextMenu->addAction(copyTransactionHashAction);
    contextMenu->addSeparator();
    contextMenu->addAction(lockAction);
    contextMenu->addAction(unlockAction);

//上下文菜单信号
    connect(ui->treeWidget, &QWidget::customContextMenuRequested, this, &CoinControlDialog::showMenu);
    connect(copyAddressAction, &QAction::triggered, this, &CoinControlDialog::copyAddress);
    connect(copyLabelAction, &QAction::triggered, this, &CoinControlDialog::copyLabel);
    connect(copyAmountAction, &QAction::triggered, this, &CoinControlDialog::copyAmount);
    connect(copyTransactionHashAction, &QAction::triggered, this, &CoinControlDialog::copyTransactionHash);
    connect(lockAction, &QAction::triggered, this, &CoinControlDialog::lockCoin);
    connect(unlockAction, &QAction::triggered, this, &CoinControlDialog::unlockCoin);

//剪贴板操作
    QAction *clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
    QAction *clipboardAmountAction = new QAction(tr("Copy amount"), this);
    QAction *clipboardFeeAction = new QAction(tr("Copy fee"), this);
    QAction *clipboardAfterFeeAction = new QAction(tr("Copy after fee"), this);
    QAction *clipboardBytesAction = new QAction(tr("Copy bytes"), this);
    QAction *clipboardLowOutputAction = new QAction(tr("Copy dust"), this);
    QAction *clipboardChangeAction = new QAction(tr("Copy change"), this);

    connect(clipboardQuantityAction, &QAction::triggered, this, &CoinControlDialog::clipboardQuantity);
    connect(clipboardAmountAction, &QAction::triggered, this, &CoinControlDialog::clipboardAmount);
    connect(clipboardFeeAction, &QAction::triggered, this, &CoinControlDialog::clipboardFee);
    connect(clipboardAfterFeeAction, &QAction::triggered, this, &CoinControlDialog::clipboardAfterFee);
    connect(clipboardBytesAction, &QAction::triggered, this, &CoinControlDialog::clipboardBytes);
    connect(clipboardLowOutputAction, &QAction::triggered, this, &CoinControlDialog::clipboardLowOutput);
    connect(clipboardChangeAction, &QAction::triggered, this, &CoinControlDialog::clipboardChange);

    ui->labelCoinControlQuantity->addAction(clipboardQuantityAction);
    ui->labelCoinControlAmount->addAction(clipboardAmountAction);
    ui->labelCoinControlFee->addAction(clipboardFeeAction);
    ui->labelCoinControlAfterFee->addAction(clipboardAfterFeeAction);
    ui->labelCoinControlBytes->addAction(clipboardBytesAction);
    ui->labelCoinControlLowOutput->addAction(clipboardLowOutputAction);
    ui->labelCoinControlChange->addAction(clipboardChangeAction);

//切换树/列表模式
    connect(ui->radioTreeMode, &QRadioButton::toggled, this, &CoinControlDialog::radioTreeMode);
    connect(ui->radioListMode, &QRadioButton::toggled, this, &CoinControlDialog::radioListMode);

//单击复选框
    connect(ui->treeWidget, &QTreeWidget::itemChanged, this, &CoinControlDialog::viewItemChanged);

//点击标题
    ui->treeWidget->header()->setSectionsClickable(true);
    connect(ui->treeWidget->header(), &QHeaderView::sectionClicked, this, &CoinControlDialog::headerSectionClicked);

//确定按钮
    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &CoinControlDialog::buttonBoxClicked);

//（联合）选择所有
    connect(ui->pushButtonSelectAll, &QPushButton::clicked, this, &CoinControlDialog::buttonSelectAllClicked);

    ui->treeWidget->setColumnWidth(COLUMN_CHECKBOX, 84);
    ui->treeWidget->setColumnWidth(COLUMN_AMOUNT, 110);
    ui->treeWidget->setColumnWidth(COLUMN_LABEL, 190);
    ui->treeWidget->setColumnWidth(COLUMN_ADDRESS, 320);
    ui->treeWidget->setColumnWidth(COLUMN_DATE, 130);
    ui->treeWidget->setColumnWidth(COLUMN_CONFIRMATIONS, 110);

//默认视图按金额描述排序
    sortView(COLUMN_AMOUNT, Qt::DescendingOrder);

//将列表模式和排序顺序还原为便利功能
    QSettings settings;
    if (settings.contains("nCoinControlMode") && !settings.value("nCoinControlMode").toBool())
        ui->radioTreeMode->click();
    if (settings.contains("nCoinControlSortColumn") && settings.contains("nCoinControlSortOrder"))
        sortView(settings.value("nCoinControlSortColumn").toInt(), (static_cast<Qt::SortOrder>(settings.value("nCoinControlSortOrder").toInt())));
}

CoinControlDialog::~CoinControlDialog()
{
    QSettings settings;
    settings.setValue("nCoinControlMode", ui->radioListMode->isChecked());
    settings.setValue("nCoinControlSortColumn", sortColumn);
    settings.setValue("nCoinControlSortOrder", (int)sortOrder);

    delete ui;
}

void CoinControlDialog::setModel(WalletModel *_model)
{
    this->model = _model;

    if(_model && _model->getOptionsModel() && _model->getAddressTableModel())
    {
        updateView();
        updateLabelLocked();
        CoinControlDialog::updateLabels(_model, this);
    }
}

//确定按钮
void CoinControlDialog::buttonBoxClicked(QAbstractButton* button)
{
    if (ui->buttonBox->buttonRole(button) == QDialogButtonBox::AcceptRole)
done(QDialog::Accepted); //关闭对话框
}

//（联合）选择所有
void CoinControlDialog::buttonSelectAllClicked()
{
    Qt::CheckState state = Qt::Checked;
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++)
    {
        if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != Qt::Unchecked)
        {
            state = Qt::Unchecked;
            break;
        }
    }
    ui->treeWidget->setEnabled(false);
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++)
            if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != state)
                ui->treeWidget->topLevelItem(i)->setCheckState(COLUMN_CHECKBOX, state);
    ui->treeWidget->setEnabled(true);
    if (state == Qt::Unchecked)
coinControl()->UnSelectAll(); //只是为了确定
    CoinControlDialog::updateLabels(model, this);
}

//上下文菜单
void CoinControlDialog::showMenu(const QPoint &point)
{
    QTreeWidgetItem *item = ui->treeWidget->itemAt(point);
    if(item)
    {
        contextMenuItem = item;

//在上下文菜单中禁用树根的某些项（如复制事务ID、锁定、解锁）
if (item->data(COLUMN_ADDRESS, TxHashRole).toString().length() == 64) //事务哈希为64个字符（这意味着它是子节点，因此在树模式下不是父节点）
        {
            copyTransactionHashAction->setEnabled(true);
            if (model->wallet().isLockedCoin(COutPoint(uint256S(item->data(COLUMN_ADDRESS, TxHashRole).toString().toStdString()), item->data(COLUMN_ADDRESS, VOutRole).toUInt())))
            {
                lockAction->setEnabled(false);
                unlockAction->setEnabled(true);
            }
            else
            {
                lockAction->setEnabled(true);
                unlockAction->setEnabled(false);
            }
        }
else //这意味着在树模式下单击父节点->禁用所有
        {
            copyTransactionHashAction->setEnabled(false);
            lockAction->setEnabled(false);
            unlockAction->setEnabled(false);
        }

//显示上下文菜单
        contextMenu->exec(QCursor::pos());
    }
}

//上下文菜单操作：复制金额
void CoinControlDialog::copyAmount()
{
    GUIUtil::setClipboard(BitcoinUnits::removeSpaces(contextMenuItem->text(COLUMN_AMOUNT)));
}

//上下文菜单操作：复制标签
void CoinControlDialog::copyLabel()
{
    if (ui->radioTreeMode->isChecked() && contextMenuItem->text(COLUMN_LABEL).length() == 0 && contextMenuItem->parent())
        GUIUtil::setClipboard(contextMenuItem->parent()->text(COLUMN_LABEL));
    else
        GUIUtil::setClipboard(contextMenuItem->text(COLUMN_LABEL));
}

//上下文菜单操作：复制地址
void CoinControlDialog::copyAddress()
{
    if (ui->radioTreeMode->isChecked() && contextMenuItem->text(COLUMN_ADDRESS).length() == 0 && contextMenuItem->parent())
        GUIUtil::setClipboard(contextMenuItem->parent()->text(COLUMN_ADDRESS));
    else
        GUIUtil::setClipboard(contextMenuItem->text(COLUMN_ADDRESS));
}

//上下文菜单操作：复制事务ID
void CoinControlDialog::copyTransactionHash()
{
    GUIUtil::setClipboard(contextMenuItem->data(COLUMN_ADDRESS, TxHashRole).toString());
}

//上下文菜单操作：锁定硬币
void CoinControlDialog::lockCoin()
{
    if (contextMenuItem->checkState(COLUMN_CHECKBOX) == Qt::Checked)
        contextMenuItem->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

    COutPoint outpt(uint256S(contextMenuItem->data(COLUMN_ADDRESS, TxHashRole).toString().toStdString()), contextMenuItem->data(COLUMN_ADDRESS, VOutRole).toUInt());
    model->wallet().lockCoin(outpt);
    contextMenuItem->setDisabled(true);
    contextMenuItem->setIcon(COLUMN_CHECKBOX, platformStyle->SingleColorIcon(":/icons/lock_closed"));
    updateLabelLocked();
}

//上下文菜单操作：解锁硬币
void CoinControlDialog::unlockCoin()
{
    COutPoint outpt(uint256S(contextMenuItem->data(COLUMN_ADDRESS, TxHashRole).toString().toStdString()), contextMenuItem->data(COLUMN_ADDRESS, VOutRole).toUInt());
    model->wallet().unlockCoin(outpt);
    contextMenuItem->setDisabled(false);
    contextMenuItem->setIcon(COLUMN_CHECKBOX, QIcon());
    updateLabelLocked();
}

//将标签“数量”复制到剪贴板
void CoinControlDialog::clipboardQuantity()
{
    GUIUtil::setClipboard(ui->labelCoinControlQuantity->text());
}

//将标签“金额”复制到剪贴板
void CoinControlDialog::clipboardAmount()
{
    GUIUtil::setClipboard(ui->labelCoinControlAmount->text().left(ui->labelCoinControlAmount->text().indexOf(" ")));
}

//将标签“费用”复制到剪贴板
void CoinControlDialog::clipboardFee()
{
    GUIUtil::setClipboard(ui->labelCoinControlFee->text().left(ui->labelCoinControlFee->text().indexOf(" ")).replace(ASYMP_UTF8, ""));
}

//将“收费后”标签复制到剪贴板
void CoinControlDialog::clipboardAfterFee()
{
    GUIUtil::setClipboard(ui->labelCoinControlAfterFee->text().left(ui->labelCoinControlAfterFee->text().indexOf(" ")).replace(ASYMP_UTF8, ""));
}

//将标签“字节”复制到剪贴板
void CoinControlDialog::clipboardBytes()
{
    GUIUtil::setClipboard(ui->labelCoinControlBytes->text().replace(ASYMP_UTF8, ""));
}

//将标签“灰尘”复制到剪贴板
void CoinControlDialog::clipboardLowOutput()
{
    GUIUtil::setClipboard(ui->labelCoinControlLowOutput->text());
}

//将标签“更改”复制到剪贴板
void CoinControlDialog::clipboardChange()
{
    GUIUtil::setClipboard(ui->labelCoinControlChange->text().left(ui->labelCoinControlChange->text().indexOf(" ")).replace(ASYMP_UTF8, ""));
}

//树视图：排序
void CoinControlDialog::sortView(int column, Qt::SortOrder order)
{
    sortColumn = column;
    sortOrder = order;
    ui->treeWidget->sortItems(column, order);
    ui->treeWidget->header()->setSortIndicator(sortColumn, sortOrder);
}

//TreeView:单击标题
void CoinControlDialog::headerSectionClicked(int logicalIndex)
{
if (logicalIndex == COLUMN_CHECKBOX) //单击最左边的列->不执行任何操作
    {
        ui->treeWidget->header()->setSortIndicator(sortColumn, sortOrder);
    }
    else
    {
        if (sortColumn == logicalIndex)
            sortOrder = ((sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder);
        else
        {
            sortColumn = logicalIndex;
sortOrder = ((sortColumn == COLUMN_LABEL || sortColumn == COLUMN_ADDRESS) ? Qt::AscendingOrder : Qt::DescendingOrder); //如果标签或地址，则默认值=>asc，否则默认值=>desc
        }

        sortView(sortColumn, sortOrder);
    }
}

//切换树模式
void CoinControlDialog::radioTreeMode(bool checked)
{
    if (checked && model)
        updateView();
}

//切换列表模式
void CoinControlDialog::radioListMode(bool checked)
{
    if (checked && model)
        updateView();
}

//用户单击的复选框
void CoinControlDialog::viewItemChanged(QTreeWidgetItem* item, int column)
{
if (column == COLUMN_CHECKBOX && item->data(COLUMN_ADDRESS, TxHashRole).toString().length() == 64) //事务哈希为64个字符（这意味着它是子节点，因此在树模式下不是父节点）
    {
        COutPoint outpt(uint256S(item->data(COLUMN_ADDRESS, TxHashRole).toString().toStdString()), item->data(COLUMN_ADDRESS, VOutRole).toUInt());

        if (item->checkState(COLUMN_CHECKBOX) == Qt::Unchecked)
            coinControl()->UnSelect(outpt);
else if (item->isDisabled()) //锁定（如果通过父节点“全部检查”会发生这种情况）
            item->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
        else
            coinControl()->Select(outpt);

//选择已更改->更新标签
if (ui->treeWidget->isEnabled()) //每次单击（取消）全选时不更新
            CoinControlDialog::updateLabels(model, this);
    }

//TODO:在不再使用qt5.3和qt5.4之后，删除此临时qt5固定。
//在qt5.5及更高版本中修复：https://bugreports.qt.io/browse/qtbug-43473
    else if (column == COLUMN_CHECKBOX && item->childCount() > 0)
    {
        if (item->checkState(COLUMN_CHECKBOX) == Qt::PartiallyChecked && item->child(0)->checkState(COLUMN_CHECKBOX) == Qt::PartiallyChecked)
            item->setCheckState(COLUMN_CHECKBOX, Qt::Checked);
    }
}

//显示锁定未暂停输出的计数
void CoinControlDialog::updateLabelLocked()
{
    std::vector<COutPoint> vOutpts;
    model->wallet().listLockedCoins(vOutpts);
    if (vOutpts.size() > 0)
    {
       ui->labelLocked->setText(tr("(%1 locked)").arg(vOutpts.size()));
       ui->labelLocked->setVisible(true);
    }
    else ui->labelLocked->setVisible(false);
}

void CoinControlDialog::updateLabels(WalletModel *model, QDialog* dialog)
{
    if (!model)
        return;

//纳金
    CAmount nPayAmount = 0;
    bool fDust = false;
    CMutableTransaction txDummy;
    for (const CAmount &amount : CoinControlDialog::payAmounts)
    {
        nPayAmount += amount;

        if (amount > 0)
        {
            CTxOut txout(amount, static_cast<CScript>(std::vector<unsigned char>(24, 0)));
            txDummy.vout.push_back(txout);
            fDust |= IsDust(txout, model->node().getDustRelayFee());
        }
    }

    CAmount nAmount             = 0;
    CAmount nPayFee             = 0;
    CAmount nAfterFee           = 0;
    CAmount nChange             = 0;
    unsigned int nBytes         = 0;
    unsigned int nBytesInputs   = 0;
    unsigned int nQuantity      = 0;
    bool fWitness               = false;

    std::vector<COutPoint> vCoinControl;
    coinControl()->ListSelected(vCoinControl);

    size_t i = 0;
    for (const auto& out : model->wallet().getCoins(vCoinControl)) {
        if (out.depth_in_main_chain < 0) continue;

//取消选择已经花费的，非常不可能的情况，这可能发生
//当选定项在其他地方使用时，如RPC或其他计算机
        const COutPoint& outpt = vCoinControl[i++];
        if (out.is_spent)
        {
            coinControl()->UnSelect(outpt);
            continue;
        }

//数量
        nQuantity++;

//数量
        nAmount += out.txout.nValue;

//字节
        CTxDestination address;
        int witnessversion = 0;
        std::vector<unsigned char> witnessprogram;
        if (out.txout.scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram))
        {
            nBytesInputs += (32 + 4 + 1 + (107 / WITNESS_SCALE_FACTOR) + 4);
            fWitness = true;
        }
        else if(ExtractDestination(out.txout.scriptPubKey, address))
        {
            CPubKey pubkey;
            CKeyID *keyid = boost::get<CKeyID>(&address);
            if (keyid && model->wallet().getPubKey(*keyid, pubkey))
            {
                nBytesInputs += (pubkey.IsCompressed() ? 148 : 180);
            }
            else
nBytesInputs += 148; //在所有错误情况下，只需假设148
        }
        else nBytesInputs += 148;
    }

//计算
    if (nQuantity > 0)
    {
//字节
nBytes = nBytesInputs + ((CoinControlDialog::payAmounts.size() > 0 ? CoinControlDialog::payAmounts.size() + 1 : 2) * 34) + 10; //在这里总是假设+1输出用于更改
        if (fWitness)
        {
//在这些与实际虚拟事务大小计算相关的数字中，存在一些回避的地方，这将使这个估计不准确。
//通常情况下，结果会在几个饱和期内被高估，这样确认对话框最终显示的费用会稍微小一些。
//此外，见证堆栈大小值是一个可变大小的整数。通常，堆栈项的数量将大大低于单字节var int限制。
nBytes += 2; //说明序列化标记和标志字节
nBytes += nQuantity; //说明保存每个输入的堆栈项数的见证字节。
        }

//在从金额中减去费用的情况下，我们可以判断零是否已经改变，并减去字节，这样之后的费用计算就准确了。
        if (CoinControlDialog::fSubtractFeeFromAmount)
            if (nAmount - nPayAmount == 0)
                nBytes -= 34;

//费用
        /*yfee=model->wallet（）.getMinimumFee（nbytes，*coinControl（），nullptr/*返回的_target*/，nullptr/*reason*/）；

        如果（npayamount>0）
        {
            nchange=namount-npayamount；
            如果（！）CoinControlDialog：：fsubtractfeefromomamount）
                nchange-=npayfee；

            //永远不要产生灰尘输出；如果我们愿意，只需将灰尘添加到费用中即可。
            if（nchange>0&&nchange<min_change）
            {
                ctxout txout（nchange，static_cast<cscript>（std:：vector<unsigned char>（24，0））；
                if（isdust（txout，model->node（）.getDustRelayFee（））
                {
                    npayFee+=n更改；
                    nchange＝0；
                    if（coinControlDialog:：fsubtractfeefromomamount）
                        nbytes-=34；//我们没有检测到上面缺少变化
                }
            }

            如果（nchange==0&&！CoinControlDialog：：fsubtractfeefromomamount）
                nBy-＝34；
        }

        /费后
        nafterfee=std:：max<camount>（namount-npayfee，0）；
    }

    //实际更新标签
    int ndisplayUnit=比特币单位：：btc；
    if（model和model->getOptionsModel（））
        ndisplayUnit=model->getOptionsModel（）->getDisplayUnit（）；

    qlabel*l1=dialog->findchild<qlabel**（“labelCoinControlQuantity”）；
    qlabel*l2=dialog->findchild<qlabel**（“labelCoinControlAmount”）；
    qlabel*l3=dialog->findchild<qlabel**（“labelCoinControlFee”）；
    qlabel*l4=dialog->findchild<qlabel/>（“labelCoinControlAfterFee”）；
    qlabel*l5=dialog->findchild<qlabel**（“labelCoinControlBytes”）；
    qlabel*l7=dialog->findchild<qlabel**（“labelCoinControlLowOutput”）；
    qlabel*l8=dialog->findchild<qlabel/>（“labelCoinControlChange”）；

    //启用/禁用“dust”和“change”
    dialog->findchild<qlabel**（“labelCoinControlLowOutputText”）->setEnabled（npayaMount>0）；
    dialog->findchild<qlabel**（“labelCoinControlLowOutput”）->setEnabled（npayamount>0）；
    dialog->findchild<qlabel**（“labelCoinControlChangeText”）->setEnabled（npayamount>0）；
    dialog->findchild<qlabel*>（“labelcoincontrolchange”）->setenabled（npayamount>0）；

    //STATS
    l1->settext（qString:：number（nQuantity））；//数量
    l2->settext（比特币单位：：formatWithUnit（ndisplayUnit，namount））；//金额
    L3->SetText（比特币单位：：FormatWithUnit（NdisPlayUnit，NpayFee））；//费用
    l4->settext（比特币单位：：formatWithUnit（ndisplayUnit，nafterfee））；//收费后
    l5->settext（（（nbytes>0）？asymp_tf8:“”）+qstring:：number（nbytes））；//字节
    l7->settext（fdust？tr（“是”）：tr（“否”））；//灰尘
    l8->settext（bitcoinUnits:：formatWithUnit（ndisplayUnit，nchange））；//更改
    如果（npayFee>0）
    {
        l3->settext（asymp_tf8+l3->text（））；
        l4->settext（asymp_utf8+l4->text（））；
        如果（nchange>0&&！CoinControlDialog：：fsubtractfeefromomamount）
            l8->settext（asymp_utf8+l8->text（））；
    }

    //灰尘时将标签变为红色
    L7->设置样式表（（fdust）？颜色：红色；“：”“）；

    /工具提示
    qString tooltipdust=tr（“如果任何收件人收到的数量小于当前灰尘阈值，则此标签变为红色”）；

    //估计的费用每个字节可能会有多少个Satoshis，我们猜错了
    双dfeevary=（nbytes！= 0）？（双）npayFee/nbytes:0；

    qString tooltip4=tr（“每个输入可以变化+/-%1个Satoshi。”）.arg（dfeevary）；

    L3->设置工具提示（tooltip4）；
    l4->settooltip（工具提示4）；
    L7->设置工具提示（tooltipdust）；
    l8->settooltip（工具提示4）；
    dialog->findchild<qlabel/>（“labelCoinControlFeetext”）->settooltip（l3->tooltip（））；
    dialog->findchild<qlabel/>（“labelCoinControlAfterFeetext”）->settooltip（l4->tooltip（））；
    dialog->findchild<qlabel/>（“labelCoinControlBytesText”）->settooltip（l5->tooltip（））；
    dialog->findchild<qlabel/>（“labelCoinControlLowOutputText”）->settooltip（l7->tooltip（））；
    dialog->findchild<qlabel/>（“labelCoinControlChangeText”）->settooltip（l8->tooltip（））；

    //资金不足
    qlabel*label=dialog->findchild<qlabel/>（“labelCoinControlInSufffunds”）；
    如果（标签）
        label->setvisible（nchange<0）；
}

ccoincontrol*CoinControlDialog:：CoinControl（）。
{
    静态CCoincontrol硬币控制；
    返回&coin_control；
}

void CoinControlDialog:：UpdateView（）。
{
    如果（！）模特儿！model->getOptionsModel（）！model->getAddressTableModel（））
        返回；

    bool treemode=ui->radiotreemode->ischecked（）；

    ui->treewidget->clear（）；
    ui->treewidget->setenabled（false）；//性能，否则将为每个选中的复选框调用updateLabels
    ui->treewidget->setAlternatingRowColors（！TeReMod；
    qFlags<qt:：itemFlag>flgCheckBox=qt:：itemissSelectable qt:：itemisEnabled qt:：itemisUserCheckable；
    qFlags<qt:：itemFlag>flgtristate=qt:：itemisseselectable qt:：itemissenabled qt:：itemissercheckable qt:：itemistrate；

    int-ndisplayUnit=model->getOptionsModel（）->getDisplayUnit（）；

    对于（const auto&coins:model->wallet（）.listcoins（））
        ccoincontrolwidgetitem*itemwalletaddress=new ccoincontrolwidgetitem（）；
        itemwalletaddress->setcheckstate（column_checkbox，qt:：unchecked）；
        qstring swelletaddress=qstring:：fromstdstring（encodedestination（coins.first））；
        qString swelletLabel=model->getAddressTableModel（）->labelForAddress（swelletAddress）；
        if（swelletLabel.isEmpty（））
            swelletLabel=tr（“（无标签）”）；

        如果（TrimeDE）
        {
            //钱包地址
            ui->treewidget->addTopLevelItem（itemWalletAddress）；

            itemwalletaddress->setflags（flgtristate）；
            itemwalletaddress->setcheckstate（column_checkbox，qt:：unchecked）；

            //标签
            itemwalletaddress->settext（column_label，swelletlabel）；

            //地址
            itemwalletaddress->settext（列地址，swelletaddress）；
        }

        camount nsum=0；
        int nchildren=0；
        用于（const auto&outpair:coins.second）
            const coutpoint&output=std:：get<0>（outpair）；
            const接口：：wallettxout&out=std:：get<1>（outpair）；
            nsum+=out.txout.nvalue；
            nCHOP++；

            ccoincontrolwidgetitem*项输出；
            if（treemode）itemOutput=新CCoincontrolWidGetItem（itemWalletAddress）；
            else itemOutput=新建CCoincontrolWidGetItem（UI->TreeWidget）；
            项输出->设置标志（flgcheckbox）；
            itemoutput->setcheckstate（column_checkbox，qt:：unchecked）；

            //地址
            CTX目标输出地址；
            qString saddress=“”；
            if（extractDestination（out.txout.scriptPubKey，outputAddress））。
            {
                saddress=qstring:：fromstdstring（encodedestination（outputaddress））；

                //如果listmode或change=>显示比特币地址。在树模式下，直接钱包地址输出不再显示地址。
                如果（！）TrimeDe~（！）（萨德莱斯==斯韦莱特地址）））
                    itemoutput->settext（列_-address，saddress）；
            }

            //标签
            如果（！）（saddress==swelletaddress））//更改
            {
                //更改来源的工具提示
                itemOutput->settooltip（column_label，tr（“更改自%1（%2）”）.arg（swelletlabel）.arg（swelletaddress））；
                itemoutput->settext（column_label，tr（“（change）”）；
            }
            否则如果（！）特雷莫德
            {
                qString slabel=model->getAddressTableModel（）->labelForAddress（saddress）；
                if（slabel.isEmpty（））
                    slabel=tr（“（无标签）”）；
                itemoutput->settext（column_label，slabel）；
            }

            /数量
            itemOutput->settext（column_amount，bitcoinUnits:：format（ndisplayUnit，out.txout.nvalue））；
            itemOutput->setData（column_amount，qt:：userRole，qvariant（（qlonglong）out.txout.nvalue））；//填充，以便正确排序

            /日期
            itemOutput->settext（column_date，guiutil:：dateTimeStr（out.time））；
            itemOutput->setData（column_date，qt:：userRole，qvariant（（qlonglong）out.time））；

            //确认
            itemoutput->settext（column_confirmations，qstring:：number（out.depth_in_main_chain））；
            itemOutput->setData（column_confirmations，qt:：userRole，qvariant（（qlonglong）out.depth_in_main_chain））；

            //事务哈希
            itemOutput->setData（column_address，txshashrole，qstring:：fromstdstring（output.hash.gethex（））；

            //VOUT指数
            itemoutput->setdata（column_address，voutrole，output.n）；

             //禁用锁定的硬币
            if（型号->wallet（）.isLockedCoin（输出））
            {
                coinControl（）->取消选择（output）；//只是为了确保
                itemOutput->setDisabled（真）；
                itemoutput->seticon（column_checkbox，platformstyle->singlecolorcon（“：/icons/lock_closed”））；
            }

            //设置复选框
            if（coinControl（）->isSelected（输出））
                itemoutput->setcheckstate（column_checkbox，qt:：checked）；
        }

        /数量
        如果（TrimeDE）
        {
            itemwalletaddress->settext（column_checkbox，“（”+qString:：number（nchildren）+”））；
            itemwalletaddress->settext（列金额，比特币单位：：格式（ndisplayUnit，nsum））；
            itemwalletaddress->setdata（column_amount，qt:：userrole，qvariant（（qlonglong）nsum））；
        }
    }

    //展开所有部分选定的
    如果（TrimeDE）
    {
        对于（int i=0；i<ui->treewidget->toplevelLiteMcount（）；i++）
            如果（ui->treewidget->topLevelItem（i）->checkState（column_checkbox）==qt:：partiallychecked）
                ui->treewidget->toplevelitem（i）->setexpanded（true）；
    }

    //排序视图
    SortView（SortColumn、SortOrder）；
    ui->treewidget->setenabled（真）；
}
