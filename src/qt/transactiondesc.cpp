
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

#ifdef HAVE_CONFIG_H
#include <config/bitcoin-config.h>
#endif

#include <qt/transactiondesc.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/paymentserver.h>
#include <qt/transactionrecord.h>

#include <consensus/consensus.h>
#include <interfaces/node.h>
#include <key_io.h>
#include <validation.h>
#include <script/script.h>
#include <timedata.h>
#include <util/system.h>
#include <wallet/db.h>
#include <wallet/wallet.h>
#include <policy/policy.h>

#include <stdint.h>
#include <string>

QString TransactionDesc::FormatTxStatus(const interfaces::WalletTx& wtx, const interfaces::WalletTxStatus& status, bool inMempool, int numBlocks)
{
    if (!status.is_final)
    {
        if (wtx.tx->nLockTime < LOCKTIME_THRESHOLD)
            return tr("Open for %n more block(s)", "", wtx.tx->nLockTime - numBlocks);
        else
            return tr("Open until %1").arg(GUIUtil::dateTimeStr(wtx.tx->nLockTime));
    }
    else
    {
        int nDepth = status.depth_in_main_chain;
        if (nDepth < 0)
            return tr("conflicted with a transaction with %1 confirmations").arg(-nDepth);
        else if (nDepth == 0)
            return tr("0/unconfirmed, %1").arg((inMempool ? tr("in memory pool") : tr("not in memory pool"))) + (status.is_abandoned ? ", "+tr("abandoned") : "");
        else if (nDepth < 6)
            return tr("%1/unconfirmed").arg(nDepth);
        else
            return tr("%1 confirmations").arg(nDepth);
    }
}

QString TransactionDesc::toHTML(interfaces::Node& node, interfaces::Wallet& wallet, TransactionRecord *rec, int unit)
{
    int numBlocks;
    interfaces::WalletTxStatus status;
    interfaces::WalletOrderForm orderForm;
    bool inMempool;
    interfaces::WalletTx wtx = wallet.getWalletTxDetails(rec->hash, status, orderForm, inMempool, numBlocks);

    QString strHTML;

    strHTML.reserve(4000);
    strHTML += "<html><font face='verdana, arial, helvetica, sans-serif'>";

    int64_t nTime = wtx.time;
    CAmount nCredit = wtx.credit;
    CAmount nDebit = wtx.debit;
    CAmount nNet = nCredit - nDebit;

    strHTML += "<b>" + tr("Status") + ":</b> " + FormatTxStatus(wtx, status, inMempool, numBlocks);
    strHTML += "<br>";

    strHTML += "<b>" + tr("Date") + ":</b> " + (nTime ? GUIUtil::dateTimeStr(nTime) : "") + "<br>";

//
//从
//
    if (wtx.is_coinbase)
    {
        strHTML += "<b>" + tr("Source") + ":</b> " + tr("Generated") + "<br>";
    }
    else if (wtx.value_map.count("from") && !wtx.value_map["from"].empty())
    {
//网上交易
        strHTML += "<b>" + tr("From") + ":</b> " + GUIUtil::HtmlEscape(wtx.value_map["from"]) + "<br>";
    }
    else
    {
//离线交易
        if (nNet > 0)
        {
//信用卡
            CTxDestination address = DecodeDestination(rec->address);
            if (IsValidDestination(address)) {
                std::string name;
                isminetype ismine;
                /*（wallet.getaddress（地址、名称和ismine，/*用途=*/nullptr））
                {
                    strhtml+=“<b>”+tr（“from”）+“：<b>”+tr（“unknown”）+“<br>”；
                    strhtml+=“<b>”+tr（“to”）+“：<b>”；
                    strhtml+=guiutil:：htmlescape（rec->address）；
                    qString addressOwned=ismine==ismine可消费？tr（“自己的地址”）：tr（“仅监视”）；
                    如果（！）No.Type（）
                        strhtml+=“（”+addressowned+“，”+tr（“label”）+“：”+guiutil:：htmlescape（name）+”）“；
                    其他的
                        strhtml+=“（”+addressowned+“）”；
                    strhtml+=“<br>”；
                }
            }
        }
    }

    / /
    // to
    / /
    如果（wtx.value_map.count（“to”）&！wtx.value_map[“to”].empty（））
    {
        //在线交易
        std:：string straddress=wtx.value_map[“to”]；
        strhtml+=“<b>”+tr（“to”）+“：<b>”；
        ctxdestination dest=解码目的地（跨地址）；
        std：：字符串名称；
        如果（wallet.getaddress（
                dest，&name，/*is_mine=*/ nullptr, /* purpose= */ nullptr) && !name.empty())

            strHTML += GUIUtil::HtmlEscape(name) + " ";
        strHTML += GUIUtil::HtmlEscape(strAddress) + "<br>";
    }

//
//数量
//
    if (wtx.is_coinbase && nCredit == 0)
    {
//
//钴基
//
        CAmount nUnmatured = 0;
        for (const CTxOut& txout : wtx.tx->vout)
            nUnmatured += wallet.getCredit(txout, ISMINE_ALL);
        strHTML += "<b>" + tr("Credit") + ":</b> ";
        if (status.is_in_main_chain)
            strHTML += BitcoinUnits::formatHtmlWithUnit(unit, nUnmatured)+ " (" + tr("matures in %n more block(s)", "", status.blocks_to_maturity) + ")";
        else
            strHTML += "(" + tr("not accepted") + ")";
        strHTML += "<br>";
    }
    else if (nNet > 0)
    {
//
//信用卡
//
        strHTML += "<b>" + tr("Credit") + ":</b> " + BitcoinUnits::formatHtmlWithUnit(unit, nNet) + "<br>";
    }
    else
    {
        isminetype fAllFromMe = ISMINE_SPENDABLE;
        for (const isminetype mine : wtx.txin_is_mine)
        {
            if(fAllFromMe > mine) fAllFromMe = mine;
        }

        isminetype fAllToMe = ISMINE_SPENDABLE;
        for (const isminetype mine : wtx.txout_is_mine)
        {
            if(fAllToMe > mine) fAllToMe = mine;
        }

        if (fAllFromMe)
        {
            if(fAllFromMe & ISMINE_WATCH_ONLY)
                strHTML += "<b>" + tr("From") + ":</b> " + tr("watch-only") + "<br>";

//
//借记
//
            auto mine = wtx.txout_is_mine.begin();
            for (const CTxOut& txout : wtx.tx->vout)
            {
//忽略变化
                isminetype toSelf = *(mine++);
                if ((toSelf == ISMINE_SPENDABLE) && (fAllFromMe == ISMINE_SPENDABLE))
                    continue;

                if (!wtx.value_map.count("to") || wtx.value_map["to"].empty())
                {
//离线交易
                    CTxDestination address;
                    if (ExtractDestination(txout.scriptPubKey, address))
                    {
                        strHTML += "<b>" + tr("To") + ":</b> ";
                        std::string name;
                        if (wallet.getAddress(
                                /*ress，&name，/*是mine=*/nullptr，/*目的=*/nullptr）&！No.Type（）
                            strhtml+=guiutil:：htmlescape（name）+“”；
                        strhtml+=guiutil:：htmlescape（encodedestination（address））；
                        如果（toself==ismine_spenable）
                            strhtml+=“（自己的地址）”；
                        Else if（仅限Toself&Ismine_Watch_）
                            strhtml+=“（仅限观看）”；
                        strhtml+=“<br>”；
                    }
                }

                strhtml+=“<b>”+tr（“debt”）+“：<b>”+bitcoinUnits:：formatHTML WithUnit（Unit，-txout.nvalue）+“<br>”；
                （如果）
                    strhtml+=“<b>”+tr（“credit”）+“：<b>”+bitcoinUnits:：formatHTML WithUnit（Unit，txOut.nValue）+“<br>”；
            }

            如果（瀑布）
            {
                //自付
                camount nchange=wtx.change；
                camount nvalue=ncredit-nchange；
                strhtml+=“<b>”+tr（“total debt”）+“：<b>”+bitcoinUnits:：formatHTML WithUnit（Unit，-nValue）+“<br>”；
                strhtml+=“<b>”+tr（“total credit”）+“：<b>”+bitcoinUnits:：formatHTML WithUnit（Unit，nValue）+“<br>”；
            }

            camount ntxfee=ndebit-wtx.tx->getValueout（）；
            如果（NTXFIX＞0）
                strhtml+=“<b>”+tr（“transaction fee”）+“：<b>”+bitcoinUnits:：formatHTML WithUnit（Unit，-ntxFee）+“<br>”；
        }
        其他的
        {
            / /
            //混合借记交易
            / /
            auto mine=wtx.txin_是_mine.begin（）；
            对于（const ctxin&txin:wtx.tx->vin）
                if（*（mine++）
                    strhtml+=“<b>”+tr（“debt”）+“：<b>”+bitcoinunits:：formathtml带单位（单位，-wallet.getdebit（txin，ismine_all））+“<br>”；
                }
            }
            mine=wtx.txout_是_mine.begin（）；
            对于（const ctxout&txout:wtx.tx->vout）
                if（*（mine++）
                    strhtml+=“<b>”+tr（“credit”）+“：<b>”+bitcoinUnits:：formatHTML WithUnit（Unit，wallet.getCredit（txout，ismine_all））+“<br>”；
                }
            }
        }
    }

    strhtml+=“<b>”+tr（“net amount”）+“：<b>”+bitcoinUnits:：formatHTML WithUnit（Unit，nnet，true）+“<br>”；

    / /
    / /消息
    / /
    如果（wtx.value_map.count（“message”）&！wtx.value_map[“message”].empty（））
        strhtml+=“<br><b>”+tr（“message”）+“：<b>”+guiutil:：htmlescape（wtx.value_map[“message”]，true）+“<br>”；
    如果（wtx.value_map.count（“comment”）&！wtx.value_map[“comment”].empty（））
        strhtml+=“<br><b>”+tr（“comment”）+“：<b>”+guiutil:：htmlescape（wtx.value_map[“comment”]，true）“<br>”；

    strhtml+=“<b>”+tr（“transaction id”）+“”：<b>“+rec->gettxhash（）+”<br>“；
    strhtml+=“<b>”+tr（“transaction total size”）+“：<b>”+qstring:：number（wtx.tx->gettotalize（））+“bytes<br>”；
    strhtml+=“<b>”+tr（“transaction virtual size”）+“：<b>”+qstring:：number（getvirtualTransactionsize（*wtx.tx））+“bytes<br>”；
    strhtml+=“<b>”+tr（“output index”）+“：<b>”+qstring:：number（rec->getOutputindex（））+“<br>”；

    //来自普通比特币的消息：uri（比特币：123…？消息=示例）
    for（const std:：pair<std:：string，std:：string>&r:orderform）
        if（r.first==“消息”）
            strhtml+=“<b r><b>”+tr（“message”）+“：<b>”+guiutil:：htmlescape（r.second，true）+“<b r>”；

ifdef启用_bip70
    / /
    //PaymentRequest信息：
    / /
    for（const std:：pair<std:：string，std:：string>&r:orderform）
    {
        if（r.first=“paymentrequest”）。
        {
            paymentrequestplus请求；
            请求分析（qbytearray:：fromrawdata（r.second.data（），r.second.size（））；
            qString商家；
            if（req.getMerchant（paymentServer:：getCertStore（），Merchant））。
                strhtml+=“<b>”+tr（“Merchant”）+“：<b>”+guiutil:：htmlescape（Merchant）+“<br>”；
        }
    }
第二节

    如果（wtx.isou coinbase）
    {
        Quint32 NumblockTourity=CoinBase_成熟度+1；
        strhtml+=“<br>”+tr（“生成的硬币必须在%1块成熟后才能使用。”当您生成此块时，它被广播到要添加到块链的网络。如果它不能进入这个链条，它的状态将变为“不被接受”，并且它将不可消费。如果另一个节点在您的几秒钟内生成一个块，则偶尔会发生这种情况。“.arg（qstring:：number（numblockTourity））+”<br>“；
    }

    / /
    //调试视图
    / /
    如果（node.getLogCategories（）！= BCLG:：没有）
    {
        strhtml+=“<hr><br>”+tr（“调试信息”）+“<br><br>”；
        用于（const ctxin&txin:wtx.tx->vin）
            如果（wallet.txinismine（txin））
                strhtml+=“<b>”+tr（“debt”）+“：<b>”+bitcoinunits:：formathtml带单位（单位，-wallet.getdebit（txin，ismine_all））+“<br>”；
        for（const ctxout&txout:wtx.tx->vout）
            如果（wallet.txoutismine（txout））。
                strhtml+=“<b>”+tr（“credit”）+“：<b>”+bitcoinUnits:：formatHTML WithUnit（Unit，wallet.getCredit（txout，ismine_all））+“<br>”；

        strhtml+=“<br><b>”+tr（“transaction”）+“：<b><br>”；
        strhtml+=guiutil:：htmlescape（wtx.tx->toString（），真）；

        strhtml+=“<br><b>”+tr（“inputs”）+“：<b>”；
        strhtml+=“<ul>”；

        用于（const ctxin&txin:wtx.tx->vin）
        {
            coutpoint prevout=txin.prevout；

            硬币；
            if（node.getunspentoutput（prevout，prev））。
            {
                {
                    strhtml+=“<li>”；
                    const ctxout&vout=prev.out；
                    CTX目标地址；
                    if（extractDestination（vout.scriptpubkey，address））。
                    {
                        std：：字符串名称；
                        if（wallet.getaddress（address，&name，/*是\mine=*/ nullptr, /* purpose= */ nullptr) && !name.empty())

                            strHTML += GUIUtil::HtmlEscape(name) + " ";
                        strHTML += QString::fromStdString(EncodeDestination(address));
                    }
                    strHTML = strHTML + " " + tr("Amount") + "=" + BitcoinUnits::formatHtmlWithUnit(unit, vout.nValue);
                    strHTML = strHTML + " IsMine=" + (wallet.txoutIsMine(vout) & ISMINE_SPENDABLE ? tr("true") : tr("false")) + "</li>";
                    strHTML = strHTML + " IsWatchOnly=" + (wallet.txoutIsMine(vout) & ISMINE_WATCH_ONLY ? tr("true") : tr("false")) + "</li>";
                }
            }
        }

        strHTML += "</ul>";
    }

    strHTML += "</font></html>";
    return strHTML;
}
