
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//TyyFr.h格式
//版权所有（c）2011，Chris Foster[Chris42f（at）Gmail（d0t）com]
//
//Boost软件许可证-版本1.0
//
//特此免费向任何人或组织授予许可。
//获取软件的副本以及
//本许可证（“软件”）用于使用、复制、显示、分发，
//执行和传输软件，并准备
//软件，并允许向其提供软件的第三方
//执行此操作时，必须遵守以下各项：
//
//软件和本声明中的版权声明，包括
//上述许可证授予、本限制和以下免责声明，
//必须全部或部分包含在软件的所有副本中，以及
//软件的所有衍生作品，除非此类副本或衍生作品
//作品仅以机器可执行对象代码的形式生成
//源语言处理器。
//
//本软件按“原样”提供，不提供任何形式的保证、明示或
//隐含的，包括但不限于对适销性的保证，
//适用于特定目的、所有权和非侵权。在任何情况下
//著作权人或者软件发行人应当承担责任吗？
//对于任何损害赔偿或其他责任，无论是合同、侵权或其他方面，
//由软件、使用或其他
//软件交易。

//————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//TinyFormat：最小类型安全的printf替换
//
//TyyFrase.h是一个在单个C++中的类型安全的PrtfF替换库
//头文件。设计目标包括：
//
//*用户定义类型的类型安全性和可扩展性。
//*c99 printf（）兼容性，尽可能使用std:：ostream
//*简约。要包含和分发的单个头文件
//你的项目。
//*增加而不是替换标准流格式化机制
//* C++ 98支持，可选C++ 11细节
//
//
//主界面示例用法
//----------------------
//
//要将日期打印到std:：cout:
//
//std:：string weekday=“星期三”；
//const char*month=“七月”；
//尺寸t天=27；
//长小时＝14；
//int min＝44；
//
//tfm:：printf（“%s，%s%d，%.2d:%.2d\n”，工作日，月，日，小时，分钟）；
//
//这里的奇怪类型强调接口的类型安全性；它是
//可以使用“%s”转换打印std:：string，以及
//使用“%d”转换调整大小。同样的结果也可以实现。
//使用tfm:：format（）函数之一。用户提供的一份打印件
//溪流：
//
//tfm:：format（std:：cerr，“%s”，%s%d，%.2d:%.2d\n”，
//工作日、月、日、时、分）；
//
//另一个返回一个std:：string：
//
//std:：string date=tfm:：format（“%s，%s%d，%.2d:%.2d\n”，
//工作日、月、日、时、分）；
//std：：cout<<日期；
//
//这是三个主要的接口函数。还有一个
//便利函数printfln（），将换行符追加到常规结果
//用于超级简单日志记录的printf（）。
//
//
//用户定义的格式函数
//----------------------
//
//在C++ 98中模拟可变模板是非常痛苦的，因为它需要
//为每个所需数量的参数写出相同的函数。使
//这个可承受的tinyFormat附带了一组宏
//在内部生成API，但也可以在用户代码中使用。
//
//三个宏tinyFormat_ArgTypes（n）、tinyFormat_varargs（n）和
//tinyFormat_passargs（n）将生成n个参数类型的列表，
//使用整数调用时，分别为类型/名称对和参数名
//n介于1和16之间。我们可以用这些定义一个宏，它生成
//需要具有n个参数的用户定义函数。生成全部16个用户
//定义函数体时，使用宏tinyformat_foreach_argnum。对于一个
//示例，请参见源文件末尾的printf（）的实现。
//
//有时，能够通过
//到非模板函数。FormatList类作为一种方法提供
//通过以不透明的方式存储参数列表。继续
//例如，我们使用makeFormatList（）构造一个格式列表：
//
//formatListRef formatList=tfm:：makeFormatList（工作日、月、日、时、分）；
//
//格式列表现在可以传递到任何非模板函数中并使用
//通过调用vformat（）函数：
//
//tfm:：vFormat（std:：cout，“%s，%s%d，%.2d:%.2d\n”，格式列表）；
//
//
//其他API信息
//---------------------
//
//错误处理：定义tinyformat_错误以自定义错误处理
//格式字符串不受支持或格式数目错误
//说明符（默认情况下调用assert（））。
//
//用户定义的类型：默认情况下，对用户定义的类型使用运算符<。
//为更多控件重载FormatValue（）。


#ifndef TINYFORMAT_H_INCLUDED
#define TINYFORMAT_H_INCLUDED

namespace tinyformat {}
//————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//配置部分。根据您的喜好定制！

//用于鼓励简洁性的命名空间别名
namespace tfm = tinyformat;

//错误处理；默认情况下调用assert（）。
#define TINYFORMAT_ERROR(reasonString) throw tinyformat::format_error(reasonString)

//定义C++ 11个可变模板，使代码更短和更多
//将军。如果没有定义，下面将自动检测C++ 11的支持。
#define TINYFORMAT_USE_VARIADIC_TEMPLATES


//————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//实施细节。
#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>
#include <stdexcept>

#ifndef TINYFORMAT_ERROR
#   define TINYFORMAT_ERROR(reason) assert(0 && reason)
#endif

#if !defined(TINYFORMAT_USE_VARIADIC_TEMPLATES) && !defined(TINYFORMAT_NO_VARIADIC_TEMPLATES)
#   ifdef __GXX_EXPERIMENTAL_CXX0X__
#       define TINYFORMAT_USE_VARIADIC_TEMPLATES
#   endif
#endif

#if defined(__GLIBCXX__) && __GLIBCXX__ < 20080201
//std:：showpos在OSX提供的旧libstdc++上损坏。见
//http://gcc.gnu.org/ml/libstdc++/2007-11/msg00075.html
#   define TINYFORMAT_OLD_LIBSTDCPLUSPLUS_WORKAROUND
#endif

#ifdef __APPLE__
//解决方法OSX链接器警告：Xcode使用不同的默认符号
//静态libs与可执行文件的可见性（见第25期）
#   define TINYFORMAT_HIDDEN __attribute__((visibility("hidden")))
#else
#   define TINYFORMAT_HIDDEN
#endif

namespace tinyformat {

class format_error: public std::runtime_error
{
public:
    explicit format_error(const std::string &what): std::runtime_error(what) {
    }
};

//————————————————————————————————————————————————————————————————————————————————————————————————————————————————
namespace detail {

//测试T1型是否可转换为T2型
template <typename T1, typename T2>
struct is_convertible
{
    private:
//两种不同尺寸的
        struct fail { char dummy[2]; };
        struct succeed { char dummy; };
//尝试通过插入Tryconvert将T1转换为T2
        static fail tryConvert(...);
        static succeed tryConvert(const T2&);
        static const T1& makeT1();
    public:
#       ifdef _MSC_VER
//禁用TryconVert（makeT1（））中精度警告的虚假丢失
#       pragma warning(push)
#       pragma warning(disable:4244)
#       pragma warning(disable:4267)
#       endif
//标准技巧：Tryconvert的（…）版本将从
//仅当采用t2的版本不匹配时才设置重载。
//然后我们比较返回类型的大小以检查
//功能匹配。非常干净，以一种恶心的方式：）
        static const bool value =
            sizeof(tryConvert(makeT1())) == sizeof(succeed);
#       ifdef _MSC_VER
#       pragma warning(pop)
#       endif
};


//当类型不是wchar_t字符串时检测
template<typename T> struct is_wchar { typedef int tinyformat_wchar_is_not_supported; };
template<> struct is_wchar<wchar_t*> {};
template<> struct is_wchar<const wchar_t*> {};
template<int n> struct is_wchar<const wchar_t[n]> {};
template<int n> struct is_wchar<wchar_t[n]> {};


//通过强制转换为类型fmt来格式化值。此默认实现
//不应该打电话。
template<typename T, typename fmtT, bool convertible = is_convertible<T, fmtT>::value>
struct formatValueAsType
{
    /*tic void invoke（std:：ostream&/*out*/，const&/*value*/）断言（0）；
}；
//可实际转换为fmtt的类型的专用版本，如
//由“convertible”模板参数指示。
模板<typename t，typename fmtt>
结构格式值类型<t，fmtt，true>
{
    静态void调用（std:：ostream&out，const&value）
        out<<static_cast<fmtt>（value）；
}；

ifdef tinyformat_old_libstdcplusplus_解决方案
template<typename t，bool convertible=是可转换的
结构格式zerointegerworkaround
{
    静态bool调用（std:：ostream&/*/, const T& /**/) { return false; }

};
template<typename T>
struct formatZeroIntegerWorkaround<T,true>
{
    static bool invoke(std::ostream& out, const T& value)
    {
        if (static_cast<int>(value) == 0 && out.flags() & std::ios::showpos)
        {
            out << "+0";
            return true;
        }
        return false;
    }
};
#endif //TinyFormat_Old_LibstdcplusPlus_解决方案

//将任意类型转换为整数。convertible=false的版本
//引发错误。
template<typename T, bool convertible = is_convertible<T,int>::value>
struct convertToInt
{
    /*tic int invoke（const&/*值*/）
    {
        tinyFormat_错误（“tinyFormat:无法从参数类型转换为”
                         “用作可变宽度或精度的整数”）；
        返回0；
    }
}；
//在可以转换时对convertToInt进行专门化
模板<typename t>
结构ConvertToInt<t，true>
{
    static int invoke（const t&value）返回static_cast<int>（value）；
}；

//最多为给定流设置ntrunc字符的格式。
模板<typename t>
内联void格式被截断（std:：ostream&out，const&value，int ntrunc）
{
    标准：ostringstream tmp；
    TMP <值；
    std:：string result=tmp.str（）；
    out.write（result.c_str（），（std:：min）（ntrunc，static_cast<int>（result.size（）））；
}
定义tinyformat_定义_格式_截断_cstr（类型）
内联void格式被截断（std:：ostream&out，type*value，int ntrunc）\
_
    std:：streamsize len=0；\
    同时（len<ntrunc&&value[len]！=0）\
        ++len；\
    out.write（value，len）；\
}
//const char*和char*的重载。无法为有符号和无符号重载
//char也是，但为了与printf兼容，技术上不需要这些字符。
tinyformat_define_format_truncted_cstr（const char）
tinyformat_define_format_truncted_cstr（char）
undef tinyformat_define_format_truncated_cstr

//命名空间详细信息


//————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//变量格式化函数。如果
/期望。


///将值格式化为流，默认情况下委托给operator<<
//
///用户可以为自己的类型重写此项。调用此函数时，
///流标志将根据格式字符串进行修改。
///格式规范在[fmtbegin，fmtende]范围内提供。为了
///截断转换，ntrunc设置为所需的最大数目
///字符，例如“%.7s”调用ntrunc=7的formatValue。
//
///默认情况下，formatValue（）使用常规的流插入运算符
///operator<<to format the type t，with special cases for the%c and%p
///转换。
模板<typename t>
内联void格式值（std:：ostream&out，const char*/*fmtbegi*/,

                        const char* fmtEnd, int ntrunc, const T& value)
{
#ifndef TINYFORMAT_ALLOW_WCHAR_STRINGS
//由于我们不支持使用“%ls”打印wchar，请使其在
//编译时间，而不是在运行时作为空*打印。
    typedef typename detail::is_wchar<T>::tinyformat_wchar_is_not_supported DummyType;
(void) DummyType(); //使用GCC-4.8避免未使用的类型警告
#endif
//这里的混乱是支持%c和%p转换：如果这些
//转换是活动的，我们尝试将类型转换为char或const
//分别设置void*和格式，而不是设置值本身。对于
//%p转换避免取消对指针的引用是很重要的，因为
//否则在打印悬挂时可能导致崩溃（const char*）。
    const bool canConvertToChar = detail::is_convertible<T,char>::value;
    const bool canConvertToVoidPtr = detail::is_convertible<T, const void*>::value;
    if(canConvertToChar && *(fmtEnd-1) == 'c')
        detail::formatValueAsType<T, char>::invoke(out, value);
    else if(canConvertToVoidPtr && *(fmtEnd-1) == 'p')
        detail::formatValueAsType<T, const void*>::invoke(out, value);
#ifdef TINYFORMAT_OLD_LIBSTDCPLUSPLUS_WORKAROUND
    /*e if（detail:：formatzerointegerworkaround<t>：：invoke（out，value））/**/；
第二节
    否则，如果（ntrunc>=0）
    {
        //在截断转换时注意不要过度读取C字符串，例如
        //“%.4s”，最多可读取4个字符。
        detail:：formatTruncated（out，value，ntrunc）；
    }
    其他的
        外出价值；
}


//重载char类型的版本以支持作为整数打印
定义tinyformat_define_formatvalue_char（chartype）
内联void格式值（std:：ostream&out，const char*/*fmtbegi*/,  \

                        /*st char*fmtende，int/**/，chartype value）（图表类型值）
_
    开关（（（fmtende-1））\
    _
        案例“U”：案例“D”：案例“I”：案例“O”：案例“X”：案例“X”：\
            out<<static\u cast<int>（value）；break；\
        默认值：
            out<<value；break；\
    _
}
//根据3.9.1：char、signed char和unsigned char都是不同的类型
tinyformat_define_formatvalue_char（char）
tinyformat_define_formatvalue_char（有符号字符）
tinyformat_define_formatvalue_char（无符号字符）
undef tinyformat_define_formatvalue_char


//————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//用于在C++ 98中模拟可变模板的工具。这里的基本思想是
//从Boost预处理器元编程库中窃取并减少到
我们需要的只是一般的。

定义tinyformat_argtypes（n）tinyformat_argtypes_
定义tinyformat_varargs（n）tinyformat_varargs_n
定义tinyformat_passargs（n）tinyformat_passargs_n
定义tinyformat_passargs_tail（n）tinyformat_passargs_tail_

//为了尽可能保持透明，已生成以下宏
//通过优秀的cog.py代码生成脚本使用python。这避免了
//需要一系列复杂（但更一般）的预处理器技巧
//在boost.preprocessor中使用。
/ /
//若要就地重新运行代码生成，请使用'cog.py-r tinyformat.h'
//（请参阅http://nedbatchelder.com/code/cog）。或者，您可以创建
//手工制作的额外版本。

[*[COG ]
Max PARAMS＝16

def makecommaseplist（linetemplate、elemtemplate、startind=1）：
    对于范围内的j（startind，maxparams+1）：
        list=''.join（[elemtemplate%'i'：i表示范围内的i（startind，j+1）））
        cog.outl（linetemplate%'j'：j，'list'：list）

makecommaseplists（'define tinyformat_argtypes_%（j）d%（list）s'，
                  “类t%（i）d′）

输出（）
makecommaseplist（'define tinyformat_varargs_u%（j）d%（list）s'，
                  'const t%（i）d&v%（i）d'）

输出（）
makecommaseplist（“define tinyformat_passargs_%（j）d%（list）s”，“v%（i）d”）。

输出（）
cog.outl（'定义tinyformat_passargs_tail_1'）
makecommaseplists（'define tinyformat_passargs_tail_u%（j）d，%（list）s'，
                  ‘V%（I）D’，开始时间=2）

输出（）
cog.outl（'定义tinyformat_foreach_argnum（m）\\\n'+
         '.join（['m（%d）'%（j，）用于范围（1，maxparams+1）中的j）
] ]*/

#define TINYFORMAT_ARGTYPES_1 class T1
#define TINYFORMAT_ARGTYPES_2 class T1, class T2
#define TINYFORMAT_ARGTYPES_3 class T1, class T2, class T3
#define TINYFORMAT_ARGTYPES_4 class T1, class T2, class T3, class T4
#define TINYFORMAT_ARGTYPES_5 class T1, class T2, class T3, class T4, class T5
#define TINYFORMAT_ARGTYPES_6 class T1, class T2, class T3, class T4, class T5, class T6
#define TINYFORMAT_ARGTYPES_7 class T1, class T2, class T3, class T4, class T5, class T6, class T7
#define TINYFORMAT_ARGTYPES_8 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8
#define TINYFORMAT_ARGTYPES_9 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9
#define TINYFORMAT_ARGTYPES_10 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10
#define TINYFORMAT_ARGTYPES_11 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11
#define TINYFORMAT_ARGTYPES_12 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12
#define TINYFORMAT_ARGTYPES_13 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13
#define TINYFORMAT_ARGTYPES_14 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14
#define TINYFORMAT_ARGTYPES_15 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14, class T15
#define TINYFORMAT_ARGTYPES_16 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14, class T15, class T16

#define TINYFORMAT_VARARGS_1 const T1& v1
#define TINYFORMAT_VARARGS_2 const T1& v1, const T2& v2
#define TINYFORMAT_VARARGS_3 const T1& v1, const T2& v2, const T3& v3
#define TINYFORMAT_VARARGS_4 const T1& v1, const T2& v2, const T3& v3, const T4& v4
#define TINYFORMAT_VARARGS_5 const T1& v1, const T2& v2, const T3& v3, const T4& v4, const T5& v5
#define TINYFORMAT_VARARGS_6 const T1& v1, const T2& v2, const T3& v3, const T4& v4, const T5& v5, const T6& v6
#define TINYFORMAT_VARARGS_7 const T1& v1, const T2& v2, const T3& v3, const T4& v4, const T5& v5, const T6& v6, const T7& v7
#define TINYFORMAT_VARARGS_8 const T1& v1, const T2& v2, const T3& v3, const T4& v4, const T5& v5, const T6& v6, const T7& v7, const T8& v8
#define TINYFORMAT_VARARGS_9 const T1& v1, const T2& v2, const T3& v3, const T4& v4, const T5& v5, const T6& v6, const T7& v7, const T8& v8, const T9& v9
#define TINYFORMAT_VARARGS_10 const T1& v1, const T2& v2, const T3& v3, const T4& v4, const T5& v5, const T6& v6, const T7& v7, const T8& v8, const T9& v9, const T10& v10
#define TINYFORMAT_VARARGS_11 const T1& v1, const T2& v2, const T3& v3, const T4& v4, const T5& v5, const T6& v6, const T7& v7, const T8& v8, const T9& v9, const T10& v10, const T11& v11
#define TINYFORMAT_VARARGS_12 const T1& v1, const T2& v2, const T3& v3, const T4& v4, const T5& v5, const T6& v6, const T7& v7, const T8& v8, const T9& v9, const T10& v10, const T11& v11, const T12& v12
#define TINYFORMAT_VARARGS_13 const T1& v1, const T2& v2, const T3& v3, const T4& v4, const T5& v5, const T6& v6, const T7& v7, const T8& v8, const T9& v9, const T10& v10, const T11& v11, const T12& v12, const T13& v13
#define TINYFORMAT_VARARGS_14 const T1& v1, const T2& v2, const T3& v3, const T4& v4, const T5& v5, const T6& v6, const T7& v7, const T8& v8, const T9& v9, const T10& v10, const T11& v11, const T12& v12, const T13& v13, const T14& v14
#define TINYFORMAT_VARARGS_15 const T1& v1, const T2& v2, const T3& v3, const T4& v4, const T5& v5, const T6& v6, const T7& v7, const T8& v8, const T9& v9, const T10& v10, const T11& v11, const T12& v12, const T13& v13, const T14& v14, const T15& v15
#define TINYFORMAT_VARARGS_16 const T1& v1, const T2& v2, const T3& v3, const T4& v4, const T5& v5, const T6& v6, const T7& v7, const T8& v8, const T9& v9, const T10& v10, const T11& v11, const T12& v12, const T13& v13, const T14& v14, const T15& v15, const T16& v16

#define TINYFORMAT_PASSARGS_1 v1
#define TINYFORMAT_PASSARGS_2 v1, v2
#define TINYFORMAT_PASSARGS_3 v1, v2, v3
#define TINYFORMAT_PASSARGS_4 v1, v2, v3, v4
#define TINYFORMAT_PASSARGS_5 v1, v2, v3, v4, v5
#define TINYFORMAT_PASSARGS_6 v1, v2, v3, v4, v5, v6
#define TINYFORMAT_PASSARGS_7 v1, v2, v3, v4, v5, v6, v7
#define TINYFORMAT_PASSARGS_8 v1, v2, v3, v4, v5, v6, v7, v8
#define TINYFORMAT_PASSARGS_9 v1, v2, v3, v4, v5, v6, v7, v8, v9
#define TINYFORMAT_PASSARGS_10 v1, v2, v3, v4, v5, v6, v7, v8, v9, v10
#define TINYFORMAT_PASSARGS_11 v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11
#define TINYFORMAT_PASSARGS_12 v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12
#define TINYFORMAT_PASSARGS_13 v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13
#define TINYFORMAT_PASSARGS_14 v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14
#define TINYFORMAT_PASSARGS_15 v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15
#define TINYFORMAT_PASSARGS_16 v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16

#define TINYFORMAT_PASSARGS_TAIL_1
#define TINYFORMAT_PASSARGS_TAIL_2 , v2
#define TINYFORMAT_PASSARGS_TAIL_3 , v2, v3
#define TINYFORMAT_PASSARGS_TAIL_4 , v2, v3, v4
#define TINYFORMAT_PASSARGS_TAIL_5 , v2, v3, v4, v5
#define TINYFORMAT_PASSARGS_TAIL_6 , v2, v3, v4, v5, v6
#define TINYFORMAT_PASSARGS_TAIL_7 , v2, v3, v4, v5, v6, v7
#define TINYFORMAT_PASSARGS_TAIL_8 , v2, v3, v4, v5, v6, v7, v8
#define TINYFORMAT_PASSARGS_TAIL_9 , v2, v3, v4, v5, v6, v7, v8, v9
#define TINYFORMAT_PASSARGS_TAIL_10 , v2, v3, v4, v5, v6, v7, v8, v9, v10
#define TINYFORMAT_PASSARGS_TAIL_11 , v2, v3, v4, v5, v6, v7, v8, v9, v10, v11
#define TINYFORMAT_PASSARGS_TAIL_12 , v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12
#define TINYFORMAT_PASSARGS_TAIL_13 , v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13
#define TINYFORMAT_PASSARGS_TAIL_14 , v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14
#define TINYFORMAT_PASSARGS_TAIL_15 , v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15
#define TINYFORMAT_PASSARGS_TAIL_16 , v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16

#define TINYFORMAT_FOREACH_ARGNUM(m) \
    m(1) m(2) m(3) m(4) m(5) m(6) m(7) m(8) m(9) m(10) m(11) m(12) m(13) m(14) m(15) m(16)
//[ [〔结尾〕]



namespace detail {

//为format（）的参数键入opaque holder，并对其执行相关操作
//作为显式函数指针保留的类型。这允许FORMATARG用于
//要在格式列表中作为同种数组分配的每个参数
//而基于继承的幼稚实现则没有。
class FormatArg
{
    public:
        FormatArg()
             : m_value(nullptr),
             m_formatImpl(nullptr),
             m_toIntImpl(nullptr)
         { }

        template<typename T>
        explicit FormatArg(const T& value)
            : m_value(static_cast<const void*>(&value)),
            m_formatImpl(&formatImpl<T>),
            m_toIntImpl(&toIntImpl<T>)
        { }

        void format(std::ostream& out, const char* fmtBegin,
                    const char* fmtEnd, int ntrunc) const
        {
            assert(m_value);
            assert(m_formatImpl);
            m_formatImpl(out, fmtBegin, fmtEnd, ntrunc, m_value);
        }

        int toInt() const
        {
            assert(m_value);
            assert(m_toIntImpl);
            return m_toIntImpl(m_value);
        }

    private:
        template<typename T>
        TINYFORMAT_HIDDEN static void formatImpl(std::ostream& out, const char* fmtBegin,
                        const char* fmtEnd, int ntrunc, const void* value)
        {
            formatValue(out, fmtBegin, fmtEnd, ntrunc, *static_cast<const T*>(value));
        }

        template<typename T>
        TINYFORMAT_HIDDEN static int toIntImpl(const void* value)
        {
            return convertToInt<T>::invoke(*static_cast<const T*>(value));
        }

        const void* m_value;
        void (*m_formatImpl)(std::ostream& out, const char* fmtBegin,
                             const char* fmtEnd, int ntrunc, const void* value);
        int (*m_toIntImpl)(const void* value);
};


//解析并返回字符串C中的整数，作为atoi（）。
//返回时，C设置为整数末尾的1。
inline int parseIntAndAdvance(const char*& c)
{
    int i = 0;
    for(;*c >= '0' && *c <= '9'; ++c)
        i = 10*i + (*c - '0');
    return i;
}

//打印格式字符串的文本部分并返回下一个格式规范
//位置。
//
//跳过出现的任何“%”，将文本“%”打印到
//输出。下一个字符的前%字符的位置
//返回了重要的格式规范，或字符串的结尾。
inline const char* printFormatStringLiteral(std::ostream& out, const char* fmt)
{
    const char* c = fmt;
    for(;; ++c)
    {
        switch(*c)
        {
            case '\0':
                out.write(fmt, c - fmt);
                return c;
            case '%':
                out.write(fmt, c - fmt);
                if(*(c+1) != '%')
                    return c;
//对于“%%”，将尾随的%附加到下一个文本部分。
                fmt = ++c;
                break;
            default:
                break;
        }
    }
}


//分析格式字符串并相应地设置流状态。
//
//这里识别的迷你语言格式是来自c99的，
//格式为“%[标志][宽度][精度][长度]类型”。
//
//无法使用Ostream本机表示的格式选项
//状态返回到spacepadPositive（对于空格填充的正数）
//和ntrunc（用于截断转换）。如果
//必须拉出可变宽度和精度。函数返回
//指向当前格式规范结束后字符的指针。
inline const char* streamStateFromFormat(std::ostream& out, bool& spacePadPositive,
                                         int& ntrunc, const char* fmtStart,
                                         const detail::FormatArg* formatters,
                                         int& argIndex, int numFormatters)
{
    if(*fmtStart != '%')
    {
        TINYFORMAT_ERROR("tinyformat: Not enough conversion specifiers in format string");
        return fmtStart;
    }
//将流状态重置为默认值。
    out.width(0);
    out.precision(6);
    out.fill(' ');
//重置大多数标志；忽略不相关的UnitBuf&Skipws。
    out.unsetf(std::ios::adjustfield | std::ios::basefield |
               std::ios::floatfield | std::ios::showbase | std::ios::boolalpha |
               std::ios::showpoint | std::ios::showpos | std::ios::uppercase);
    bool precisionSet = false;
    bool widthSet = false;
    int widthExtra = 0;
    const char* c = fmtStart + 1;
//1）解析标志
    for(;; ++c)
    {
        switch(*c)
        {
            case '#':
                out.setf(std::ios::showpoint | std::ios::showbase);
                continue;
            case '0':
//被左对齐方式覆盖（“-”标志）
                if(!(out.flags() & std::ios::left))
                {
//使用内部填充，使数值
//格式正确，例如-00010而不是000-10
                    out.fill('0');
                    out.setf(std::ios::internal, std::ios::adjustfield);
                }
                continue;
            case '-':
                out.fill(' ');
                out.setf(std::ios::left, std::ios::adjustfield);
                continue;
            case ' ':
//被显示正号“+”标志覆盖。
                if(!(out.flags() & std::ios::showpos))
                    spacePadPositive = true;
                continue;
            case '+':
                out.setf(std::ios::showpos);
                spacePadPositive = false;
                widthExtra = 1;
                continue;
            default:
                break;
        }
        break;
    }
//2）解析宽度
    if(*c >= '0' && *c <= '9')
    {
        widthSet = true;
        out.width(parseIntAndAdvance(c));
    }
    if(*c == '*')
    {
        widthSet = true;
        int width = 0;
        if(argIndex < numFormatters)
            width = formatters[argIndex++].toInt();
        else
            TINYFORMAT_ERROR("tinyformat: Not enough arguments to read variable width");
        if(width < 0)
        {
//负宽度对应于“-”标志集
            out.fill(' ');
            out.setf(std::ios::left, std::ios::adjustfield);
            width = -width;
        }
        out.width(width);
        ++c;
    }
//3）解析精度
    if(*c == '.')
    {
        ++c;
        int precision = 0;
        if(*c == '*')
        {
            ++c;
            if(argIndex < numFormatters)
                precision = formatters[argIndex++].toInt();
            else
                TINYFORMAT_ERROR("tinyformat: Not enough arguments to read variable precision");
        }
        else
        {
            if(*c >= '0' && *c <= '9')
                precision = parseIntAndAdvance(c);
else if(*c == '-') //忽略负精度，视为零。
                parseIntAndAdvance(++c);
        }
        out.precision(precision);
        precisionSet = true;
    }
//4）忽略任何C99长度修改器
    while(*c == 'l' || *c == 'h' || *c == 'L' ||
          *c == 'j' || *c == 'z' || *c == 't')
        ++c;
//5）我们可以使用转换说明符字符。
//基于转换说明符设置流标志（由于
//Boost：：格式化类，以便在这里进行锻造）。
    bool intConversion = false;
    switch(*c)
    {
        case 'u': case 'd': case 'i':
            out.setf(std::ios::dec, std::ios::basefield);
            intConversion = true;
            break;
        case 'o':
            out.setf(std::ios::oct, std::ios::basefield);
            intConversion = true;
            break;
        case 'X':
            out.setf(std::ios::uppercase);
//Falls通过
        case 'x': case 'p':
            out.setf(std::ios::hex, std::ios::basefield);
            intConversion = true;
            break;
        case 'E':
            out.setf(std::ios::uppercase);
//Falls通过
        case 'e':
            out.setf(std::ios::scientific, std::ios::floatfield);
            out.setf(std::ios::dec, std::ios::basefield);
            break;
        case 'F':
            out.setf(std::ios::uppercase);
//Falls通过
        case 'f':
            out.setf(std::ios::fixed, std::ios::floatfield);
            break;
        case 'G':
            out.setf(std::ios::uppercase);
//Falls通过
        case 'g':
            out.setf(std::ios::dec, std::ios::basefield);
//和boost：：格式一样，让流决定浮点格式。
            out.flags(out.flags() & ~std::ios::floatfield);
            break;
        case 'a': case 'A':
            TINYFORMAT_ERROR("tinyformat: the %a and %A conversion specs "
                             "are not supported");
            break;
        case 'c':
//在FormatValue（）中作为特殊情况处理
            break;
        case 's':
            if(precisionSet)
                ntrunc = static_cast<int>(out.precision());
//使%s打印布尔值为“真”和“假”
            out.setf(std::ios::boolalpha);
            break;
        case 'n':
//不支持-将导致问题！
            TINYFORMAT_ERROR("tinyformat: %n conversion spec not supported");
            break;
        case '\0':
            TINYFORMAT_ERROR("tinyformat: Conversion spec incorrectly "
                             "terminated by end of string");
            return c;
        default:
            break;
    }
    if(intConversion && precisionSet && !widthSet)
    {
//整数的“精度”给出了最小位数（即
//左边用零填充）。这不是真正的支持
//iostreams，但是我们可以用宽度近似模拟它，如果
//宽度不作其他用途。
        out.width(out.precision() + widthExtra);
        out.setf(std::ios::internal, std::ios::adjustfield);
        out.fill('0');
    }
    return c+1;
}


//————————————————————————————————————————————————————————————————————————————————————————————————————————————————
inline void formatImpl(std::ostream& out, const char* fmt,
                       const detail::FormatArg* formatters,
                       int numFormatters)
{
//保存的流状态
    std::streamsize origWidth = out.width();
    std::streamsize origPrecision = out.precision();
    std::ios::fmtflags origFlags = out.flags();
    char origFill = out.fill();

    for (int argIndex = 0; argIndex < numFormatters; ++argIndex)
    {
//分析格式字符串
        fmt = printFormatStringLiteral(out, fmt);
        bool spacePadPositive = false;
        int ntrunc = -1;
        const char* fmtEnd = streamStateFromFormat(out, spacePadPositive, ntrunc, fmt,
                                                   formatters, argIndex, numFormatters);
        if (argIndex >= numFormatters)
        {
//在读取任何可变宽度/精度后检查参数是否保留
            TINYFORMAT_ERROR("tinyformat: Not enough format arguments");
            return;
        }
        const FormatArg& arg = formatters[argIndex];
//将参数格式化为流。
        if(!spacePadPositive)
            arg.format(out, fmt, fmtEnd, ntrunc);
        else
        {
//以下是没有直接通信的特殊情况
//在流格式和printf（）行为之间。模拟
//它通过格式化为临时字符串流和
//咀嚼生成的字符串。
            std::ostringstream tmpStream;
            tmpStream.copyfmt(out);
            tmpStream.setf(std::ios::showpos);
            arg.format(tmpStream, fmt, fmtEnd, ntrunc);
std::string result = tmpStream.str(); //分配…讨厌。
            for(size_t i = 0, iend = result.size(); i < iend; ++i)
                if(result[i] == '+') result[i] = ' ';
            out << result;
        }
        fmt = fmtEnd;
    }

//打印格式字符串的其余部分。
    fmt = printFormatStringLiteral(out, fmt);
    if(*fmt != '\0')
        TINYFORMAT_ERROR("tinyformat: Too many conversion specifiers in format string");

//还原流状态
    out.width(origWidth);
    out.precision(origPrecision);
    out.flags(origFlags);
    out.fill(origFill);
}

} //命名空间详细信息


///list of template arguments format（），以不透明的方式保存。
///
///a对formatlist（typedef'd as formatlistref）的常量引用可以是
///方便地用于将参数传递给非模板函数：all类型
///information已从参数中删除，只留下足够的
///common接口根据需要执行格式化。
class FormatList
{
    public:
        FormatList(detail::FormatArg* formatters, int N)
            : m_formatters(formatters), m_N(N) { }

        friend void vformat(std::ostream& out, const char* fmt,
                            const FormatList& list);

    private:
        const detail::FormatArg* m_formatters;
        int m_N;
};

///reference to type opaque format list for passing to vformat（）。
typedef const FormatList& FormatListRef;


namespace detail {

//为列表子类设置固定存储格式以避免动态分配
template<int N>
class FormatListN : public FormatList
{
    public:
#ifdef TINYFORMAT_USE_VARIADIC_TEMPLATES
        template<typename... Args>
        explicit FormatListN(const Args&... args)
            : FormatList(&m_formatterStore[0], N),
            m_formatterStore { FormatArg(args)... }
        { static_assert(sizeof...(args) == N, "Number of args must be N"); }
#else //C++ 98版
        void init(int) {}
#       define TINYFORMAT_MAKE_FORMATLIST_CONSTRUCTOR(n)       \
                                                               \
        template<TINYFORMAT_ARGTYPES(n)>                       \
        explicit FormatListN(TINYFORMAT_VARARGS(n))            \
            : FormatList(&m_formatterStore[0], n)              \
        { assert(n == N); init(0, TINYFORMAT_PASSARGS(n)); }   \
                                                               \
        template<TINYFORMAT_ARGTYPES(n)>                       \
        void init(int i, TINYFORMAT_VARARGS(n))                \
        {                                                      \
            m_formatterStore[i] = FormatArg(v1);               \
            init(i+1 TINYFORMAT_PASSARGS_TAIL(n));             \
        }

        TINYFORMAT_FOREACH_ARGNUM(TINYFORMAT_MAKE_FORMATLIST_CONSTRUCTOR)
#       undef TINYFORMAT_MAKE_FORMATLIST_CONSTRUCTOR
#endif

    private:
        FormatArg m_formatterStore[N];
};

//特殊的0-arg版本-MSVC表示结构中的零大小C数组是非标准的
template<> class FormatListN<0> : public FormatList
{
    public: FormatListN() : FormatList(0, 0) {}
};

} //命名空间详细信息


//————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//主要API函数

#ifdef TINYFORMAT_USE_VARIADIC_TEMPLATES

///make从模板参数列表中键入不可知格式列表。
///
///此函数的确切返回类型是实现详细信息，并且
///不应该依赖。相反，它应该存储为formatListRef:
///
/*formatListRef formatList=生成格式列表（/*…*/）；
模板<类型名…ARG>
详细信息：：formatlist n<sizeof…（args）>makeformatlist（const args&…ARGS）
{
    返回detail:：formatlistn<sizeof…（args）>（args…）；
}

第98版

内联详细信息：：FormatListn<0>MakeFormatList（））
{
    返回detail:：formatlistn<0>（）；
}
定义tinyformat_make_makeformatlist（n）
模板<tinyformat_argtypes（n）>>
详细信息：：formatlistn<n>makeformatlist（tinyformat_varargs（n））\
_
    返回详细信息：：formatlistn<n>（tinyformat_passargs（n））；\
}
TinyFormat_ForEach_Argnum（TinyFormat_Make_MakeformList）
隐藏格式制作格式列表

第二节

///根据给定的格式字符串设置流参数的格式列表。
//
///选择名称vformat（）是为了与vprintf（）的语义相似性：
///格式参数列表保存在单个函数参数中。
inline void vformat（std:：ostream&out，const char*fmt，formatListRef列表）
{
    详细信息：：formatimpl（out，fmt，list.m_formatters，list.m_n）；
}


ifdef tinyformat_使用变量模板

///根据给定的格式字符串设置流参数列表的格式。
模板<类型名…ARG>
void格式（std:：ostream&out，const char*fmt，const args&…ARGS）
{
    vformat（out，fmt，makeformatlist（args…）；
}

///根据给定的格式字符串格式化参数列表并返回
///结果为字符串。
模板<类型名…ARG>
std：：字符串格式（const char*fmt，const args&…ARGS）
{
    std:：ostringstream操作系统；
    格式（oss，fmt，args…）；
    返回oss.str（）；
}

///根据给定的格式字符串格式化std:：cout的参数列表
模板<类型名…ARG>
void printf（const char*fmt，const args&…ARGS）
{
    格式（std:：cout，fmt，args…）；
}

模板<类型名…ARG>
void printfln（const char*fmt，const args&…ARGS）
{
    格式（std:：cout，fmt，args…）；
    std：：cout<<'\n'；
}

第98版

内联void格式（std:：ostream&out，const char*fmt）
{
    vFormat（out，fmt，makeFormatList（））；
}

inline std：：字符串格式（const char*fmt）
{
    std:：ostringstream操作系统；
    格式（oss，fmt）；
    返回oss.str（）；
}

inline void printf（const char*fmt）
{
    格式（std:：cout，fmt）；
}

内联void printfln（const char*fmt）
{
    格式（std:：cout，fmt）；
    std：：cout<<'\n'；
}

定义tinyformat_make_format_funcs（n）
                                                                          \
模板<tinyformat_argtypes（n）>>
void格式（std:：ostream&out，const char*fmt，tinyformat_varargs（n））\
_
    格式（out，fmt，makeformatlist（tinyformat_passargs（n）））；\
_
                                                                          \
模板<tinyformat_argtypes（n）>>
std：：字符串格式（const char*fmt，tinyformat_varargs（n））\
_
    标准：：ostringstream oss；\
    格式（oss，fmt，tinyformat_passargs（n））；\
    返回oss.str（）；\
_
                                                                          \
模板<tinyformat_argtypes（n）>>
void printf（const char*fmt，tinyformat_varargs（n））\
_
    格式（std:：cout，fmt，tinyformat_passargs（n））；\
_
                                                                          \
模板<tinyformat_argtypes（n）>>
void printfln（const char*fmt，tinyformat_varargs（n））\
_
    格式（std:：cout，fmt，tinyformat_passargs（n））；\
    std：：cout<'\n'；\
}

TinyFormat_ForEach_Argnum（TinyFormat_Make_Format_Funcs）
隐藏格式制作格式功能

第二节

//比特币核心增加
模板<类型名…ARG>
std:：string格式（const std:：string&fmt，const args&…ARGS）
{
    std:：ostringstream操作系统；
    格式（oss，fmt.c_str（），args…）；
    返回oss.str（）；
}

//命名空间tinyFormat

定义strprintf tfm:：format

endif//包含tinyformat_h_
