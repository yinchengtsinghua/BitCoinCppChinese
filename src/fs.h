
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

#ifndef BITCOIN_FS_H
#define BITCOIN_FS_H

#include <stdio.h>
#include <string>
#if defined WIN32 && defined __GLIBCXX__
#include <ext/stdio_filebuf.h>
#endif

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

/*文件系统操作和类型*/
namespace fs = boost::filesystem;

/*将操作桥接到C stdio*/
namespace fsbridge {
    FILE *fopen(const fs::path& p, const char *mode);

    class FileLock
    {
    public:
        FileLock() = delete;
        FileLock(const FileLock&) = delete;
        FileLock(FileLock&&) = delete;
        explicit FileLock(const fs::path& file);
        ~FileLock();
        bool TryLock();
        std::string GetReason() { return reason; }

    private:
        std::string reason;
#ifndef WIN32
        int fd = -1;
#else
void* hFile = (void*)-1; //无效的句柄值
#endif
    };

    std::string get_filesystem_error_message(const fs::filesystem_error& e);

//GNU libstdc++特定于在Windows上打开utf-8路径的解决方案。
//
//在Windows上，只能通过
//`wchar_t` API，而不是'char` API。但是因为C++标准没有
//需要ifstream/ofstream的'wchar't'构造函数，而gnu库不需要
//提供它们（与微软C++库相反，参见
//https://stackoverflow.com/questions/821873/how-to-open-an-stdfstream-ofstream-or-ifstream-with-a-unicode-filename/822032 822032），网址：
//boost被强制返回到可能无法正常工作的“char”构造函数。
//
//通过在中创建流对象来解决此问题
//与`\u gnu_cxx:：stdio_filebuf`组合。此解决方案可以删除
//升级到C++ 17，其中流可以直接从
//`std:：filesystem:：path`对象。

#if defined WIN32 && defined __GLIBCXX__
    class ifstream : public std::istream
    {
    public:
        ifstream() = default;
        explicit ifstream(const fs::path& p, std::ios_base::openmode mode = std::ios_base::in) { open(p, mode); }
        ~ifstream() { close(); }
        void open(const fs::path& p, std::ios_base::openmode mode = std::ios_base::in);
        bool is_open() { return m_filebuf.is_open(); }
        void close();

    private:
        __gnu_cxx::stdio_filebuf<char> m_filebuf;
        FILE* m_file = nullptr;
    };
    class ofstream : public std::ostream
    {
    public:
        ofstream() = default;
        explicit ofstream(const fs::path& p, std::ios_base::openmode mode = std::ios_base::out) { open(p, mode); }
        ~ofstream() { close(); }
        void open(const fs::path& p, std::ios_base::openmode mode = std::ios_base::out);
        bool is_open() { return m_filebuf.is_open(); }
        void close();

    private:
        __gnu_cxx::stdio_filebuf<char> m_filebuf;
        FILE* m_file = nullptr;
    };
#else  //！（win32和glibcxx）
    typedef fs::ifstream ifstream;
    typedef fs::ofstream ofstream;
#endif //win32和glibcxx
};

#endif //比科尼亚法斯赫
