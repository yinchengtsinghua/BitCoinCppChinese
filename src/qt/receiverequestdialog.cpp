
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

#include <qt/receiverequestdialog.h>
#include <qt/forms/ui_receiverequestdialog.h>

#include <qt/bitcoinunits.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <QClipboard>
#include <QDrag>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>

#if defined(HAVE_CONFIG_H)
/*clude<config/bitcoin config.h>/*用于使用
第二节

ifdef使用_qrcode
include<qrencode.h>
第二节

qrimagewidget:：qrimagewidget（qwidget*父项）：
    qlabel（父级），contextmenu（nullptr）
{
    contextmenu=新建qmenu（this）；
    qaction*saveImageAction=new qaction（tr（“&save image…”），this）；
    连接（saveimageaction，&qaction:：trigged，this，&qrimagewidget:：saveimage）；
    contextmenu->addaction（saveimageaction）；
    qaction*copyImageAction=new qaction（tr（“&copy image”），this）；
    连接（copyImageAction，&qAction:：Trigged，this，&qrimageWidget:：copyImage）；
    contextmenu->addaction（copyImageAction）；
}

qimage qrimagewidget:：exportImage（）。
{
    如果（！）PixMax（）
        返回qImage（）；
    返回pixmap（）->toImage（）；
}

void qrimagewidget:：mousePressEvent（qmouseEvent*事件）
{
    if（event->button（）==qt:：leftbutton&&pixmap（））
    {
        event->accept（）；
        qmimedata*mimedata=新的qmimedata；
        mimedata->setImageData（exportImage（））；

        qdrag*阻力=新qdrag（this）；
        拖动->setmimedata（mimedata）；
        拖动>执行（）；
    }否则{
        qlabel:：mousePressEvent（事件）；
    }
}

void qrimagewidget:：saveimage（）。
{
    如果（！）PixMax（）
        返回；
    qstring fn=guiutil:：getsavefilename（this，tr（“save qr code”），qstring（），tr（“png image（*.png）”），nullptr）；
    如果（！）fn.iSimple（）
    {
        exportImage（）.save（fn）；
    }
}

void qrimagewidget:：copyImage（）。
{
    如果（！）PixMax（）
        返回；
    qApplication:：Clipboard（）->SetImage（exportImage（））；
}

void qrimagewidget:：contextmenuevent（qContextmenuevent*事件）
{
    如果（！）PixMax（）
        返回；
    contextmenu->exec（event->globalpos（））；
}

ReceiveRequestDialog:：ReceiveRequestDialog（QWidget*父项）：
    Qdialog（父级）
    用户界面（新建用户界面：：ReceiverRequestDialog）
    模型（NulLPTR）
{
    ui->setupui（这个）；

如果要使用，请使用qrcode
    ui->btnaveas->setvisible（false）；
    ui->lblqrcode->setvisible（false）；
第二节

    连接（ui->btnaveas，&qpushbutton:：clicked，ui->lblqrcode，&qrimagewidget:：saveimage）；
}

ReceiveRequestDialog:：~ReceiveRequestDialog（）。
{
    删除用户界面；
}

void receiverequestdialog:：setmodel（walletmodel*_model）
{
    此->模型=_模型；

    If（模型）
        connect（_model->getOptionsModel（），&optionsModel:：DisplayUnitChanged，this，&receiverequestDialog:：update）；

    //必要时更新显示单元
    UpDebug（）；
}

void receiverequestdialog:：setinfo（const sendcoinsrecipient&\u info）
{
    this->info=_info；
    UpDebug（）；
}

使ReceiverRequestDialog:：Update（）无效
{
    如果（！）模型）
        返回；
    qString target=info.label；
    if（target.isEmpty（））
        目标=信息地址；
    setWindowTitle（tr（“向%1请求付款”）.arg（目标））；

    qString uri=guiutil:：formatBitcoinUri（信息）；
    ui->btnaveas->setenabled（假）；
    qStand HTML；
    html+=“<html><font face='verdana，arial，helvetica，sans-serif'>”；
    html+=“<b>”+tr（“payment information”）+“<b><br>”；
    html+=“<b>”+tr（“uri”）+“<b>：”；
    html+=“<a href=\”“+uri+”\“>”+guiutil:：htmlescape（uri）+“<a><br>”；
    html+=“<b>”+tr（“address”）+“<b>：”+guiutil:：htmlescape（info.address）+“<br>”；
    如果（信息量）
        html+=“<b>”+tr（“amount”）+“<b>：”+bitcoinUnits:：formatHTML WithUnit（model->getOptionsModel（）->getDisplayUnit（），info.amount）+“<br>”；
    如果（！）info.label.isEmpty（））
        html+=“<b>”+tr（“label”）+“<b>：”+guiutil:：htmlescape（info.label）+“<br>”；
    如果（！）info.message.isEmpty（））
        html+=“<b>”+tr（“message”）+“<b>：”+guiutil:：htmlescape（info.message）+“<br>”；
    if（model->ismultiwallet（））
        html+=“<b>”+tr（“wallet”）+“<b>：”+guiutil:：htmlescape（model->getwalletname（））+“<br>”；
    }
    ui->outuri->settext（html）；

ifdef使用_qrcode
    ui->lblqrcode->settext（“”）；
    如果（！）URI.ISUNTY（）
    {
        //限制URI长度
        if（uri.length（）>最大\u uri \u长度）
        {
            ui->lblqrcode->settext（tr（“resulting uri too long，try to reduce the text for label/message.”））；
        }否则{
            qr code*code=qr code_encodeString（uri.toutf8（）.constData（），0，qr_eclevel_l，qr_mode_8，1）；
            如果（！）代码）
            {
                ui->lblqrcode->settext（tr（“error encoding uri into qr code.”））；
                返回；
            }
            qImage qrimage=qImage（code->width+8，code->width+8，qImage:：format_rgb32）；
            填充（0xffffff）；
            无符号char*p=code->data；
            对于（int y=0；y<code->width；y++）
            {
                对于（int x=0；x<code->width；x++）
                {
                    设置像素（x+4，y+4，（*p&1）？0x0:0xffffff））；
                    P+；
                }
            }
            qrcode-free（代码）；

            qimage qraddrimage=qimage（qr_image_size，qr_image_size+20，qimage:：format_rgb32）；
            qraddrimage.fill（0xffffff）填充；
            Q油漆工（&qraddrimage）；
            painter.drawimage（0，0，qr image.scalled（qr_image_size，qr_image_size））；
            qfont font=guiutil:：fixedpitchfont（）；
            qrect paddedrect=qraddrimage.rect（）；

            //计算理想字体大小
            qreal font_size=guiutil:：calculateidealfontsize（paddedrect.width（）-20，info.address，字体）；
            font.setpointsizef（字体大小）；

            painter.setfont（字体）；
            paddedrect.setheight（qr_image_size+12）；
            painter.drawtext（paddedrect，qt:：AlignBottom qt:：AlignCenter，info.address）；
            画家。

            ui->lblqrcode->setpixmap（qpixmap:：fromimage（qraddrimage））；
            ui->btnsaveas->setenabled（真）；
        }
    }
第二节
}

void receiverequestdialog:：on_btncopyuri_clicked（））
{
    guiutil:：setclipboard（guiutil:：formatbitoinuri（info））；
}

void receiverequestdialog:：on_btncopyAddress_clicked（））
{
    guiutil:：setclipboard（信息地址）；
}
