
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

#ifndef BITCOIN_STREAMS_H
#define BITCOIN_STREAMS_H

#include <support/allocators/zeroafterfree.h>
#include <serialize.h>

#include <algorithm>
#include <assert.h>
#include <ios>
#include <limits>
#include <map>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <utility>
#include <vector>

template<typename Stream>
class OverrideStream
{
    Stream* stream;

    const int nType;
    const int nVersion;

public:
    OverrideStream(Stream* stream_, int nType_, int nVersion_) : stream(stream_), nType(nType_), nVersion(nVersion_) {}

    template<typename T>
    OverrideStream<Stream>& operator<<(const T& obj)
    {
//序列化到此流
        ::Serialize(*this, obj);
        return (*this);
    }

    template<typename T>
    OverrideStream<Stream>& operator>>(T&& obj)
    {
//从此流中取消序列化
        ::Unserialize(*this, obj);
        return (*this);
    }

    void write(const char* pch, size_t nSize)
    {
        stream->write(pch, nSize);
    }

    void read(char* pch, size_t nSize)
    {
        stream->read(pch, nSize);
    }

    int GetVersion() const { return nVersion; }
    int GetType() const { return nType; }
    size_t size() const { return stream->size(); }
};

/*覆盖和/或附加到现有字节向量的最小流
 *
 *参考向量将根据需要增长。
 **/

class CVectorWriter
{
 public:

/*
 *@param[in]类型化序列化类型
 *@param[in]nversionin序列化版本（包括任何标志）
 *@param[in]vchdatain引用字节向量覆盖/追加
 *@param[in]nposin起始位置。开始写入的向量索引。矢量最初
 *根据需要增长到最大值（nposin，vec.size（））。所以要附加，请使用vec.size（）。
**/

    CVectorWriter(int nTypeIn, int nVersionIn, std::vector<unsigned char>& vchDataIn, size_t nPosIn) : nType(nTypeIn), nVersion(nVersionIn), vchData(vchDataIn), nPos(nPosIn)
    {
        if(nPos > vchData.size())
            vchData.resize(nPos);
    }
/*
 *（其他参数同上）
 *@param[in]参数是从nposin开始要序列化的项的列表。
**/

    template <typename... Args>
    CVectorWriter(int nTypeIn, int nVersionIn, std::vector<unsigned char>& vchDataIn, size_t nPosIn, Args&&... args) : CVectorWriter(nTypeIn, nVersionIn, vchDataIn, nPosIn)
    {
        ::SerializeMany(*this, std::forward<Args>(args)...);
    }
    void write(const char* pch, size_t nSize)
    {
        assert(nPos <= vchData.size());
        size_t nOverwrite = std::min(nSize, vchData.size() - nPos);
        if (nOverwrite) {
            memcpy(vchData.data() + nPos, reinterpret_cast<const unsigned char*>(pch), nOverwrite);
        }
        if (nOverwrite < nSize) {
            vchData.insert(vchData.end(), reinterpret_cast<const unsigned char*>(pch) + nOverwrite, reinterpret_cast<const unsigned char*>(pch) + nSize);
        }
        nPos += nSize;
    }
    template<typename T>
    CVectorWriter& operator<<(const T& obj)
    {
//序列化到此流
        ::Serialize(*this, obj);
        return (*this);
    }
    int GetVersion() const
    {
        return nVersion;
    }
    int GetType() const
    {
        return nType;
    }
private:
    const int nType;
    const int nVersion;
    std::vector<unsigned char>& vchData;
    size_t nPos;
};

/*通过引用从现有向量读取的最小流
 **/

class VectorReader
{
private:
    const int m_type;
    const int m_version;
    const std::vector<unsigned char>& m_data;
    size_t m_pos = 0;

public:

    /*
     *@param[in]类型序列化类型
     *@param[in]版本序列化版本（包括任何标志）
     *@param[in]要覆盖/追加的数据引用字节向量
     *@param[in]pos起始位置。读取应该开始的向量索引。
     **/

    VectorReader(int type, int version, const std::vector<unsigned char>& data, size_t pos)
        : m_type(type), m_version(version), m_data(data), m_pos(pos)
    {
        if (m_pos > m_data.size()) {
            throw std::ios_base::failure("VectorReader(...): end of data (m_pos > m_data.size())");
        }
    }

    /*
     *（其他参数同上）
     *@param[in]参数从pos开始反序列化的项目列表。
     **/

    template <typename... Args>
    VectorReader(int type, int version, const std::vector<unsigned char>& data, size_t pos,
                  Args&&... args)
        : VectorReader(type, version, data, pos)
    {
        ::UnserializeMany(*this, std::forward<Args>(args)...);
    }

    template<typename T>
    VectorReader& operator>>(T& obj)
    {
//从此流中取消序列化
        ::Unserialize(*this, obj);
        return (*this);
    }

    int GetVersion() const { return m_version; }
    int GetType() const { return m_type; }

    size_t size() const { return m_data.size() - m_pos; }
    bool empty() const { return m_data.size() == m_pos; }

    void read(char* dst, size_t n)
    {
        if (n == 0) {
            return;
        }

//从缓冲区开始读取
        size_t pos_next = m_pos + n;
        if (pos_next > m_data.size()) {
            throw std::ios_base::failure("VectorReader::read(): end of data");
        }
        memcpy(dst, m_data.data() + m_pos, n);
        m_pos = pos_next;
    }
};

/*双端缓冲区，结合向量和类流接口。
 *
 *>>and<<read and write unmatted data using the above serialization templates.
 *以线性时间填充数据；某些Stringstream实现需要n^2时间。
 **/

class CDataStream
{
protected:
    typedef CSerializeData vector_type;
    vector_type vch;
    unsigned int nReadPos;

    int nType;
    int nVersion;
public:

    typedef vector_type::allocator_type   allocator_type;
    typedef vector_type::size_type        size_type;
    typedef vector_type::difference_type  difference_type;
    typedef vector_type::reference        reference;
    typedef vector_type::const_reference  const_reference;
    typedef vector_type::value_type       value_type;
    typedef vector_type::iterator         iterator;
    typedef vector_type::const_iterator   const_iterator;
    typedef vector_type::reverse_iterator reverse_iterator;

    explicit CDataStream(int nTypeIn, int nVersionIn)
    {
        Init(nTypeIn, nVersionIn);
    }

    CDataStream(const_iterator pbegin, const_iterator pend, int nTypeIn, int nVersionIn) : vch(pbegin, pend)
    {
        Init(nTypeIn, nVersionIn);
    }

    CDataStream(const char* pbegin, const char* pend, int nTypeIn, int nVersionIn) : vch(pbegin, pend)
    {
        Init(nTypeIn, nVersionIn);
    }

    CDataStream(const vector_type& vchIn, int nTypeIn, int nVersionIn) : vch(vchIn.begin(), vchIn.end())
    {
        Init(nTypeIn, nVersionIn);
    }

    CDataStream(const std::vector<char>& vchIn, int nTypeIn, int nVersionIn) : vch(vchIn.begin(), vchIn.end())
    {
        Init(nTypeIn, nVersionIn);
    }

    CDataStream(const std::vector<unsigned char>& vchIn, int nTypeIn, int nVersionIn) : vch(vchIn.begin(), vchIn.end())
    {
        Init(nTypeIn, nVersionIn);
    }

    template <typename... Args>
    CDataStream(int nTypeIn, int nVersionIn, Args&&... args)
    {
        Init(nTypeIn, nVersionIn);
        ::SerializeMany(*this, std::forward<Args>(args)...);
    }

    void Init(int nTypeIn, int nVersionIn)
    {
        nReadPos = 0;
        nType = nTypeIn;
        nVersion = nVersionIn;
    }

    CDataStream& operator+=(const CDataStream& b)
    {
        vch.insert(vch.end(), b.begin(), b.end());
        return *this;
    }

    friend CDataStream operator+(const CDataStream& a, const CDataStream& b)
    {
        CDataStream ret = a;
        ret += b;
        return (ret);
    }

    std::string str() const
    {
        return (std::string(begin(), end()));
    }


//
//向量子集
//
    const_iterator begin() const                     { return vch.begin() + nReadPos; }
    iterator begin()                                 { return vch.begin() + nReadPos; }
    const_iterator end() const                       { return vch.end(); }
    iterator end()                                   { return vch.end(); }
    size_type size() const                           { return vch.size() - nReadPos; }
    bool empty() const                               { return vch.size() == nReadPos; }
    void resize(size_type n, value_type c=0)         { vch.resize(n + nReadPos, c); }
    void reserve(size_type n)                        { vch.reserve(n + nReadPos); }
    const_reference operator[](size_type pos) const  { return vch[pos + nReadPos]; }
    reference operator[](size_type pos)              { return vch[pos + nReadPos]; }
    void clear()                                     { vch.clear(); nReadPos = 0; }
    iterator insert(iterator it, const char x=char()) { return vch.insert(it, x); }
    void insert(iterator it, size_type n, const char x) { vch.insert(it, n, x); }
    value_type* data()                               { return vch.data() + nReadPos; }
    const value_type* data() const                   { return vch.data() + nReadPos; }

    void insert(iterator it, std::vector<char>::const_iterator first, std::vector<char>::const_iterator last)
    {
        if (last == first) return;
        assert(last - first > 0);
        if (it == vch.begin() + nReadPos && (unsigned int)(last - first) <= nReadPos)
        {
//有空间时在前面插入的特殊情况
            nReadPos -= (last - first);
            memcpy(&vch[nReadPos], &first[0], last - first);
        }
        else
            vch.insert(it, first, last);
    }

    void insert(iterator it, const char* first, const char* last)
    {
        if (last == first) return;
        assert(last - first > 0);
        if (it == vch.begin() + nReadPos && (unsigned int)(last - first) <= nReadPos)
        {
//有空间时在前面插入的特殊情况
            nReadPos -= (last - first);
            memcpy(&vch[nReadPos], &first[0], last - first);
        }
        else
            vch.insert(it, first, last);
    }

    iterator erase(iterator it)
    {
        if (it == vch.begin() + nReadPos)
        {
//从前面擦除的特殊情况
            if (++nReadPos >= vch.size())
            {
//每当我们到达终点，我们就抓住机会清除缓冲区。
                nReadPos = 0;
                return vch.erase(vch.begin(), vch.end());
            }
            return vch.begin() + nReadPos;
        }
        else
            return vch.erase(it);
    }

    iterator erase(iterator first, iterator last)
    {
        if (first == vch.begin() + nReadPos)
        {
//从前面擦除的特殊情况
            if (last == vch.end())
            {
                nReadPos = 0;
                return vch.erase(vch.begin(), vch.end());
            }
            else
            {
                nReadPos = (last - vch.begin());
                return last;
            }
        }
        else
            return vch.erase(first, last);
    }

    inline void Compact()
    {
        vch.erase(vch.begin(), vch.begin() + nReadPos);
        nReadPos = 0;
    }

    bool Rewind(size_type n)
    {
//如果缓冲区尚未压缩，则倒带N个字符
        if (n > nReadPos)
            return false;
        nReadPos -= n;
        return true;
    }


//
//流子集
//
    bool eof() const             { return size() == 0; }
    CDataStream* rdbuf()         { return this; }
    int in_avail() const         { return size(); }

    void SetType(int n)          { nType = n; }
    int GetType() const          { return nType; }
    void SetVersion(int n)       { nVersion = n; }
    int GetVersion() const       { return nVersion; }

    void read(char* pch, size_t nSize)
    {
        if (nSize == 0) return;

//从缓冲区开始读取
        unsigned int nReadPosNext = nReadPos + nSize;
        if (nReadPosNext > vch.size()) {
            throw std::ios_base::failure("CDataStream::read(): end of data");
        }
        memcpy(pch, &vch[nReadPos], nSize);
        if (nReadPosNext == vch.size())
        {
            nReadPos = 0;
            vch.clear();
            return;
        }
        nReadPos = nReadPosNext;
    }

    void ignore(int nSize)
    {
//从缓冲区开始忽略
        if (nSize < 0) {
            throw std::ios_base::failure("CDataStream::ignore(): nSize negative");
        }
        unsigned int nReadPosNext = nReadPos + nSize;
        if (nReadPosNext >= vch.size())
        {
            if (nReadPosNext > vch.size())
                throw std::ios_base::failure("CDataStream::ignore(): end of data");
            nReadPos = 0;
            vch.clear();
            return;
        }
        nReadPos = nReadPosNext;
    }

    void write(const char* pch, size_t nSize)
    {
//写入缓冲区结尾
        vch.insert(vch.end(), pch, pch + nSize);
    }

    template<typename Stream>
    void Serialize(Stream& s) const
    {
//特殊情况：stream<<stream concatenates like stream+=stream
        if (!vch.empty())
            s.write((char*)vch.data(), vch.size() * sizeof(value_type));
    }

    template<typename T>
    CDataStream& operator<<(const T& obj)
    {
//序列化到此流
        ::Serialize(*this, obj);
        return (*this);
    }

    template<typename T>
    CDataStream& operator>>(T&& obj)
    {
//从此流中取消序列化
        ::Unserialize(*this, obj);
        return (*this);
    }

    void GetAndClear(CSerializeData &d) {
        d.insert(d.end(), begin(), end());
        clear();
    }

    /*
     *用某个键对该流的内容执行XOR。
     *
     *@param[in]key用于异或此流中数据的键。
     **/

    void Xor(const std::vector<unsigned char>& key)
    {
        if (key.size() == 0) {
            return;
        }

        for (size_type i = 0, j = 0; i != size(); i++) {
            vch[i] ^= key[j++];

//这可能会对很多字节的数据起作用，因此
//重要的是我们要计算“j”，也就是说，这里的“key”索引
//方法而不是做百分比，这实际上是一个部门
//对于每个字节，xor'd——比需要的慢得多。
            if (j == key.size())
                j = 0;
        }
    }
};

template <typename IStream>
class BitStreamReader
{
private:
    IStream& m_istream;

///buffered从输入流中读取的字节。一个新的字节被读取到
///当m_偏移量达到8时缓冲。
    uint8_t m_buffer{0};

///前一个已返回的m_缓冲区中的高位位数
///read（）调用。下一个要返回的位位于
///最有效位位置。
    int m_offset{8};

public:
    explicit BitStreamReader(IStream& istream) : m_istream(istream) {}

    /*从流中读取指定的位数。返回数据
     *在64位uint的nbits最低有效位中。
     **/

    uint64_t Read(int nbits) {
        if (nbits < 0 || nbits > 64) {
            throw std::out_of_range("nbits must be between 0 and 64");
        }

        uint64_t data = 0;
        while (nbits > 0) {
            if (m_offset == 8) {
                m_istream >> m_buffer;
                m_offset = 0;
            }

            int bits = std::min(8 - m_offset, nbits);
            data <<= bits;
            data |= static_cast<uint8_t>(m_buffer << m_offset) >> (8 - bits);
            m_offset += bits;
            nbits -= bits;
        }
        return data;
    }
};

template <typename OStream>
class BitStreamWriter
{
private:
    OStream& m_ostream;

///等待写入输出流的缓冲字节。字节是
//调用m_offset达到8或flush（）时的/written buffer。
    uint8_t m_buffer{0};

///M_缓冲区中已由上一个写入的高位位数
///write（）调用，但尚未刷新到流中。下一位是
///written to与最高有效位的偏移量。
    int m_offset{0};

public:
    explicit BitStreamWriter(OStream& ostream) : m_ostream(ostream) {}

    ~BitStreamWriter()
    {
        Flush();
    }

    /*将64位int的nbits最低有效位写入输出
     *流。数据被缓冲，直到它完成一个八位字节。
     **/

    void Write(uint64_t data, int nbits) {
        if (nbits < 0 || nbits > 64) {
            throw std::out_of_range("nbits must be between 0 and 64");
        }

        while (nbits > 0) {
            int bits = std::min(8 - m_offset, nbits);
            m_buffer |= (data << (64 - nbits)) >> (64 - 8 + m_offset);
            m_offset += bits;
            nbits -= bits;

            if (m_offset == 8) {
                Flush();
            }
        }
    }

    /*将任何未写入的位刷新到输出流，并用0填充到
     *下一个字节边界。
     **/

    void Flush() {
        if (m_offset == 0) {
            return;
        }

        m_ostream << m_buffer;
        m_buffer = 0;
        m_offset = 0;
    }
};



/*文件*的未引用raii包装
 *
 *如果文件不为空，它将在超出范围时自动关闭。
 *如果要返回文件指针，请返回file.release（）。
 *如果需要提前关闭文件，请使用file.fclose（）而不是fclose（file）。
 **/

class CAutoFile
{
private:
    const int nType;
    const int nVersion;

    FILE* file;

public:
    CAutoFile(FILE* filenew, int nTypeIn, int nVersionIn) : nType(nTypeIn), nVersion(nVersionIn)
    {
        file = filenew;
    }

    ~CAutoFile()
    {
        fclose();
    }

//不允许拷贝
    CAutoFile(const CAutoFile&) = delete;
    CAutoFile& operator=(const CAutoFile&) = delete;

    void fclose()
    {
        if (file) {
            ::fclose(file);
            file = nullptr;
        }
    }

    /*获得包装文件*并转移所有权。
     *@注意，这将使cautoFile对象无效，并使其成为调用方的责任。
     *用于清除返回的文件*。
     **/

    FILE* release()             { FILE* ret = file; file = nullptr; return ret; }

    /*在不转移所有权的情况下获得包装文件*。
     *@注意，文件*的所有权将保留在此类中。仅当
     *cautofile异常使用传递的指针。
     **/

    FILE* Get() const           { return file; }

    /*如果包装文件*为nullptr，则返回true，否则返回false。
     **/

    bool IsNull() const         { return (file == nullptr); }

//
//流子集
//
    int GetType() const          { return nType; }
    int GetVersion() const       { return nVersion; }

    void read(char* pch, size_t nSize)
    {
        if (!file)
            throw std::ios_base::failure("CAutoFile::read: file handle is nullptr");
        if (fread(pch, 1, nSize, file) != nSize)
            throw std::ios_base::failure(feof(file) ? "CAutoFile::read: end of file" : "CAutoFile::read: fread failed");
    }

    void ignore(size_t nSize)
    {
        if (!file)
            throw std::ios_base::failure("CAutoFile::ignore: file handle is nullptr");
        unsigned char data[4096];
        while (nSize > 0) {
            size_t nNow = std::min<size_t>(nSize, sizeof(data));
            if (fread(data, 1, nNow, file) != nNow)
                throw std::ios_base::failure(feof(file) ? "CAutoFile::ignore: end of file" : "CAutoFile::read: fread failed");
            nSize -= nNow;
        }
    }

    void write(const char* pch, size_t nSize)
    {
        if (!file)
            throw std::ios_base::failure("CAutoFile::write: file handle is nullptr");
        if (fwrite(pch, 1, nSize, file) != nSize)
            throw std::ios_base::failure("CAutoFile::write: write failed");
    }

    template<typename T>
    CAutoFile& operator<<(const T& obj)
    {
//序列化到此流
        if (!file)
            throw std::ios_base::failure("CAutoFile::operator<<: file handle is nullptr");
        ::Serialize(*this, obj);
        return (*this);
    }

    template<typename T>
    CAutoFile& operator>>(T&& obj)
    {
//从此流中取消序列化
        if (!file)
            throw std::ios_base::failure("CAutoFile::operator>>: file handle is nullptr");
        ::Unserialize(*this, obj);
        return (*this);
    }
};

/*围绕一个文件*的非refcounted raii包装器，该文件*实现一个
 *从反序列化。它保证能够倒带给定数量的字节。
 *
 *如果文件不为空，它将在超出范围时自动关闭。
 *如果需要提前关闭文件，请使用file.fclose（）而不是fclose（file）。
 **/

class CBufferedFile
{
private:
    const int nType;
    const int nVersion;

FILE *src;            //源文件
uint64_t nSrcPos;     //从源读取了多少字节
uint64_t nReadPos;    //从中读取了多少字节
uint64_t nReadLimit;  //我们可以读到哪个位置
uint64_t nRewind;     //我们保证倒带多少字节
std::vector<char> vchBuf; //缓冲器

protected:
//从源读取数据以填充缓冲区
    bool Fill() {
        unsigned int pos = nSrcPos % vchBuf.size();
        unsigned int readNow = vchBuf.size() - pos;
        unsigned int nAvail = vchBuf.size() - (nSrcPos - nReadPos) - nRewind;
        if (nAvail < readNow)
            readNow = nAvail;
        if (readNow == 0)
            return false;
        size_t nBytes = fread((void*)&vchBuf[pos], 1, readNow, src);
        if (nBytes == 0) {
            throw std::ios_base::failure(feof(src) ? "CBufferedFile::Fill: end of file" : "CBufferedFile::Fill: fread failed");
        } else {
            nSrcPos += nBytes;
            return true;
        }
    }

public:
    CBufferedFile(FILE *fileIn, uint64_t nBufSize, uint64_t nRewindIn, int nTypeIn, int nVersionIn) :
        nType(nTypeIn), nVersion(nVersionIn), nSrcPos(0), nReadPos(0), nReadLimit(std::numeric_limits<uint64_t>::max()), nRewind(nRewindIn), vchBuf(nBufSize, 0)
    {
        src = fileIn;
    }

    ~CBufferedFile()
    {
        fclose();
    }

//不允许拷贝
    CBufferedFile(const CBufferedFile&) = delete;
    CBufferedFile& operator=(const CBufferedFile&) = delete;

    int GetVersion() const { return nVersion; }
    int GetType() const { return nType; }

    void fclose()
    {
        if (src) {
            ::fclose(src);
            src = nullptr;
        }
    }

//检查我们是否在源文件的末尾
    bool eof() const {
        return nReadPos == nSrcPos && feof(src);
    }

//读取字节数
    void read(char *pch, size_t nSize) {
        if (nSize + nReadPos > nReadLimit)
            throw std::ios_base::failure("Read attempted past buffer limit");
        if (nSize + nRewind > vchBuf.size())
            throw std::ios_base::failure("Read larger than buffer size");
        while (nSize > 0) {
            if (nReadPos == nSrcPos)
                Fill();
            unsigned int pos = nReadPos % vchBuf.size();
            size_t nNow = nSize;
            if (nNow + pos > vchBuf.size())
                nNow = vchBuf.size() - pos;
            if (nNow + nReadPos > nSrcPos)
                nNow = nSrcPos - nReadPos;
            memcpy(pch, &vchBuf[pos], nNow);
            nReadPos += nNow;
            pch += nNow;
            nSize -= nNow;
        }
    }

//返回当前读取位置
    uint64_t GetPos() const {
        return nReadPos;
    }

//倒带到给定的阅读位置
    bool SetPos(uint64_t nPos) {
        nReadPos = nPos;
        if (nReadPos + nRewind < nSrcPos) {
            nReadPos = nSrcPos - nRewind;
            return false;
        } else if (nReadPos > nSrcPos) {
            nReadPos = nSrcPos;
            return false;
        } else {
            return true;
        }
    }

    bool Seek(uint64_t nPos) {
        long nLongPos = nPos;
        if (nPos != (uint64_t)nLongPos)
            return false;
        if (fseek(src, nLongPos, SEEK_SET))
            return false;
        nLongPos = ftell(src);
        nSrcPos = nLongPos;
        nReadPos = nLongPos;
        return true;
    }

//防止读数超过某个位置
//没有参数可以移除限制
    bool SetLimit(uint64_t nPos = std::numeric_limits<uint64_t>::max()) {
        if (nPos < nReadPos)
            return false;
        nReadLimit = nPos;
        return true;
    }

    template<typename T>
    CBufferedFile& operator>>(T&& obj) {
//从此流中取消序列化
        ::Unserialize(*this, obj);
        return (*this);
    }

//在流中搜索给定的字节，并保持在该字节上
    void FindByte(char ch) {
        while (true) {
            if (nReadPos == nSrcPos)
                Fill();
            if (vchBuf[nReadPos % vchBuf.size()] == ch)
                break;
            nReadPos++;
        }
    }
};

#endif //比特币流
