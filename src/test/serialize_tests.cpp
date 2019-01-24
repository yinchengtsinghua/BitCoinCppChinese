
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2012-2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。

#include <serialize.h>
#include <streams.h>
#include <hash.h>
#include <test/test_bitcoin.h>

#include <stdint.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(serialize_tests, BasicTestingSetup)

class CSerializeMethodsTestSingle
{
protected:
    int intval;
    bool boolval;
    std::string stringval;
    char charstrval[16];
    CTransactionRef txval;
public:
    CSerializeMethodsTestSingle() = default;
    CSerializeMethodsTestSingle(int intvalin, bool boolvalin, std::string stringvalin, const char* charstrvalin, const CTransactionRef& txvalin) : intval(intvalin), boolval(boolvalin), stringval(std::move(stringvalin)), txval(txvalin)
    {
        memcpy(charstrval, charstrvalin, sizeof(charstrval));
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(intval);
        READWRITE(boolval);
        READWRITE(stringval);
        READWRITE(charstrval);
        READWRITE(txval);
    }

    bool operator==(const CSerializeMethodsTestSingle& rhs)
    {
        return  intval == rhs.intval && \
                boolval == rhs.boolval && \
                stringval == rhs.stringval && \
                strcmp(charstrval, rhs.charstrval) == 0 && \
                *txval == *rhs.txval;
    }
};

class CSerializeMethodsTestMany : public CSerializeMethodsTestSingle
{
public:
    using CSerializeMethodsTestSingle::CSerializeMethodsTestSingle;
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(intval, boolval, stringval, charstrval, txval);
    }
};

BOOST_AUTO_TEST_CASE(sizes)
{
    BOOST_CHECK_EQUAL(sizeof(char), GetSerializeSize(char(0), 0));
    BOOST_CHECK_EQUAL(sizeof(int8_t), GetSerializeSize(int8_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(uint8_t), GetSerializeSize(uint8_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(int16_t), GetSerializeSize(int16_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(uint16_t), GetSerializeSize(uint16_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(int32_t), GetSerializeSize(int32_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(uint32_t), GetSerializeSize(uint32_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(int64_t), GetSerializeSize(int64_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(uint64_t), GetSerializeSize(uint64_t(0), 0));
    BOOST_CHECK_EQUAL(sizeof(float), GetSerializeSize(float(0), 0));
    BOOST_CHECK_EQUAL(sizeof(double), GetSerializeSize(double(0), 0));
//bool序列化为char
    BOOST_CHECK_EQUAL(sizeof(char), GetSerializeSize(bool(0), 0));

//StudieChestGETSerialZeSeI化与C++类型匹配
    BOOST_CHECK_EQUAL(GetSerializeSize(char(0), 0), 1U);
    BOOST_CHECK_EQUAL(GetSerializeSize(int8_t(0), 0), 1U);
    BOOST_CHECK_EQUAL(GetSerializeSize(uint8_t(0), 0), 1U);
    BOOST_CHECK_EQUAL(GetSerializeSize(int16_t(0), 0), 2U);
    BOOST_CHECK_EQUAL(GetSerializeSize(uint16_t(0), 0), 2U);
    BOOST_CHECK_EQUAL(GetSerializeSize(int32_t(0), 0), 4U);
    BOOST_CHECK_EQUAL(GetSerializeSize(uint32_t(0), 0), 4U);
    BOOST_CHECK_EQUAL(GetSerializeSize(int64_t(0), 0), 8U);
    BOOST_CHECK_EQUAL(GetSerializeSize(uint64_t(0), 0), 8U);
    BOOST_CHECK_EQUAL(GetSerializeSize(float(0), 0), 4U);
    BOOST_CHECK_EQUAL(GetSerializeSize(double(0), 0), 8U);
    BOOST_CHECK_EQUAL(GetSerializeSize(bool(0), 0), 1U);
}

BOOST_AUTO_TEST_CASE(floats_conversion)
{
//选择明确映射到二进制浮点的值以避免
//编译器端的舍入问题。
    BOOST_CHECK_EQUAL(ser_uint32_to_float(0x00000000), 0.0F);
    BOOST_CHECK_EQUAL(ser_uint32_to_float(0x3f000000), 0.5F);
    BOOST_CHECK_EQUAL(ser_uint32_to_float(0x3f800000), 1.0F);
    BOOST_CHECK_EQUAL(ser_uint32_to_float(0x40000000), 2.0F);
    BOOST_CHECK_EQUAL(ser_uint32_to_float(0x40800000), 4.0F);
    BOOST_CHECK_EQUAL(ser_uint32_to_float(0x44444444), 785.066650390625F);

    BOOST_CHECK_EQUAL(ser_float_to_uint32(0.0F), 0x00000000U);
    BOOST_CHECK_EQUAL(ser_float_to_uint32(0.5F), 0x3f000000U);
    BOOST_CHECK_EQUAL(ser_float_to_uint32(1.0F), 0x3f800000U);
    BOOST_CHECK_EQUAL(ser_float_to_uint32(2.0F), 0x40000000U);
    BOOST_CHECK_EQUAL(ser_float_to_uint32(4.0F), 0x40800000U);
    BOOST_CHECK_EQUAL(ser_float_to_uint32(785.066650390625F), 0x44444444U);
}

BOOST_AUTO_TEST_CASE(doubles_conversion)
{
//选择明确映射到二进制浮点的值以避免
//编译器端的舍入问题。
    BOOST_CHECK_EQUAL(ser_uint64_to_double(0x0000000000000000ULL), 0.0);
    BOOST_CHECK_EQUAL(ser_uint64_to_double(0x3fe0000000000000ULL), 0.5);
    BOOST_CHECK_EQUAL(ser_uint64_to_double(0x3ff0000000000000ULL), 1.0);
    BOOST_CHECK_EQUAL(ser_uint64_to_double(0x4000000000000000ULL), 2.0);
    BOOST_CHECK_EQUAL(ser_uint64_to_double(0x4010000000000000ULL), 4.0);
    BOOST_CHECK_EQUAL(ser_uint64_to_double(0x4088888880000000ULL), 785.066650390625);

    BOOST_CHECK_EQUAL(ser_double_to_uint64(0.0), 0x0000000000000000ULL);
    BOOST_CHECK_EQUAL(ser_double_to_uint64(0.5), 0x3fe0000000000000ULL);
    BOOST_CHECK_EQUAL(ser_double_to_uint64(1.0), 0x3ff0000000000000ULL);
    BOOST_CHECK_EQUAL(ser_double_to_uint64(2.0), 0x4000000000000000ULL);
    BOOST_CHECK_EQUAL(ser_double_to_uint64(4.0), 0x4010000000000000ULL);
    BOOST_CHECK_EQUAL(ser_double_to_uint64(785.066650390625), 0x4088888880000000ULL);
}
/*
生成以下哈希的python代码：

    def反向_hex（x）：
        返回binascii.hexlify（“'.join（reversed（x）））
    DEF DHA256（x）：
        返回hashlib.sha256（hashlib.sha256（x.digest（））.digest（）。

    反向_hex（dsha256（''.join（struct.pack（'<f'，x）for x in range（01000）））='8e8b4cf3e4df8b332057e3e23af42ebc663b61e04955d5e7e32d85099d7f3fe0c'
    反向_hex（dsha256（''.join（struct.pack（'<d'，x）for x in range（01000）））='43d0c82591953c4eafe114590d39266a1585d25d43557f0d78b23f96'
**/

BOOST_AUTO_TEST_CASE(floats)
{
    CDataStream ss(SER_DISK, 0);
//编码
    for (int i = 0; i < 1000; i++) {
        ss << float(i);
    }
    BOOST_CHECK(Hash(ss.begin(), ss.end()) == uint256S("8e8b4cf3e4df8b332057e3e23af42ebc663b61e0495d5e7e32d85099d7f3fe0c"));

//译码
    for (int i = 0; i < 1000; i++) {
        float j;
        ss >> j;
        BOOST_CHECK_MESSAGE(i == j, "decoded:" << j << " expected:" << i);
    }
}

BOOST_AUTO_TEST_CASE(doubles)
{
    CDataStream ss(SER_DISK, 0);
//编码
    for (int i = 0; i < 1000; i++) {
        ss << double(i);
    }
    BOOST_CHECK(Hash(ss.begin(), ss.end()) == uint256S("43d0c82591953c4eafe114590d392676a01585d25b25d433557f0d7878b23f96"));

//译码
    for (int i = 0; i < 1000; i++) {
        double j;
        ss >> j;
        BOOST_CHECK_MESSAGE(i == j, "decoded:" << j << " expected:" << i);
    }
}

BOOST_AUTO_TEST_CASE(varints)
{
//编码

    CDataStream ss(SER_DISK, 0);
    CDataStream::size_type size = 0;
    for (int i = 0; i < 100000; i++) {
        ss << VARINT(i, VarIntMode::NONNEGATIVE_SIGNED);
        size += ::GetSerializeSize(VARINT(i, VarIntMode::NONNEGATIVE_SIGNED), 0);
        BOOST_CHECK(size == ss.size());
    }

    for (uint64_t i = 0;  i < 100000000000ULL; i += 999999937) {
        ss << VARINT(i);
        size += ::GetSerializeSize(VARINT(i), 0);
        BOOST_CHECK(size == ss.size());
    }

//译码
    for (int i = 0; i < 100000; i++) {
        int j = -1;
        ss >> VARINT(j, VarIntMode::NONNEGATIVE_SIGNED);
        BOOST_CHECK_MESSAGE(i == j, "decoded:" << j << " expected:" << i);
    }

    for (uint64_t i = 0;  i < 100000000000ULL; i += 999999937) {
        uint64_t j = std::numeric_limits<uint64_t>::max();
        ss >> VARINT(j);
        BOOST_CHECK_MESSAGE(i == j, "decoded:" << j << " expected:" << i);
    }
}

BOOST_AUTO_TEST_CASE(varints_bitpatterns)
{
    CDataStream ss(SER_DISK, 0);
    ss << VARINT(0, VarIntMode::NONNEGATIVE_SIGNED); BOOST_CHECK_EQUAL(HexStr(ss), "00"); ss.clear();
    ss << VARINT(0x7f, VarIntMode::NONNEGATIVE_SIGNED); BOOST_CHECK_EQUAL(HexStr(ss), "7f"); ss.clear();
    ss << VARINT((int8_t)0x7f, VarIntMode::NONNEGATIVE_SIGNED); BOOST_CHECK_EQUAL(HexStr(ss), "7f"); ss.clear();
    ss << VARINT(0x80, VarIntMode::NONNEGATIVE_SIGNED); BOOST_CHECK_EQUAL(HexStr(ss), "8000"); ss.clear();
    ss << VARINT((uint8_t)0x80); BOOST_CHECK_EQUAL(HexStr(ss), "8000"); ss.clear();
    ss << VARINT(0x1234, VarIntMode::NONNEGATIVE_SIGNED); BOOST_CHECK_EQUAL(HexStr(ss), "a334"); ss.clear();
    ss << VARINT((int16_t)0x1234, VarIntMode::NONNEGATIVE_SIGNED); BOOST_CHECK_EQUAL(HexStr(ss), "a334"); ss.clear();
    ss << VARINT(0xffff, VarIntMode::NONNEGATIVE_SIGNED); BOOST_CHECK_EQUAL(HexStr(ss), "82fe7f"); ss.clear();
    ss << VARINT((uint16_t)0xffff); BOOST_CHECK_EQUAL(HexStr(ss), "82fe7f"); ss.clear();
    ss << VARINT(0x123456, VarIntMode::NONNEGATIVE_SIGNED); BOOST_CHECK_EQUAL(HexStr(ss), "c7e756"); ss.clear();
    ss << VARINT((int32_t)0x123456, VarIntMode::NONNEGATIVE_SIGNED); BOOST_CHECK_EQUAL(HexStr(ss), "c7e756"); ss.clear();
    ss << VARINT(0x80123456U); BOOST_CHECK_EQUAL(HexStr(ss), "86ffc7e756"); ss.clear();
    ss << VARINT((uint32_t)0x80123456U); BOOST_CHECK_EQUAL(HexStr(ss), "86ffc7e756"); ss.clear();
    ss << VARINT(0xffffffff); BOOST_CHECK_EQUAL(HexStr(ss), "8efefefe7f"); ss.clear();
    ss << VARINT(0x7fffffffffffffffLL, VarIntMode::NONNEGATIVE_SIGNED); BOOST_CHECK_EQUAL(HexStr(ss), "fefefefefefefefe7f"); ss.clear();
    ss << VARINT(0xffffffffffffffffULL); BOOST_CHECK_EQUAL(HexStr(ss), "80fefefefefefefefe7f"); ss.clear();
}

BOOST_AUTO_TEST_CASE(compactsize)
{
    CDataStream ss(SER_DISK, 0);
    std::vector<char>::size_type i, j;

    for (i = 1; i <= MAX_SIZE; i *= 2)
    {
        WriteCompactSize(ss, i-1);
        WriteCompactSize(ss, i);
    }
    for (i = 1; i <= MAX_SIZE; i *= 2)
    {
        j = ReadCompactSize(ss);
        BOOST_CHECK_MESSAGE((i-1) == j, "decoded:" << j << " expected:" << (i-1));
        j = ReadCompactSize(ss);
        BOOST_CHECK_MESSAGE(i == j, "decoded:" << j << " expected:" << i);
    }
}

static bool isCanonicalException(const std::ios_base::failure& ex)
{
    std::ios_base::failure expectedException("non-canonical ReadCompactSize()");

//what（）返回的字符串对于不同的平台可能不同。
//不是直接将ex.what（）与预期的字符串进行比较，
//创建一个异常实例，以查看ex what（）是否匹配
//异常实例返回的预期解释字符串。
    return strcmp(expectedException.what(), ex.what()) == 0;
}


BOOST_AUTO_TEST_CASE(noncanonical)
{
//编写一些非规范的CompactSize编码，以及
//确保在回读时引发异常。
    CDataStream ss(SER_DISK, 0);
    std::vector<char>::size_type n;

//用三个字节对零进行编码：
    ss.write("\xfd\x00\x00", 3);
    BOOST_CHECK_EXCEPTION(ReadCompactSize(ss), std::ios_base::failure, isCanonicalException);

//用三个字节编码的0xfc：
    ss.write("\xfd\xfc\x00", 3);
    BOOST_CHECK_EXCEPTION(ReadCompactSize(ss), std::ios_base::failure, isCanonicalException);

//用三个字节编码的0xfd正常：
    ss.write("\xfd\xfd\x00", 3);
    n = ReadCompactSize(ss);
    BOOST_CHECK(n == 0xfd);

//用五个字节对零进行编码：
    ss.write("\xfe\x00\x00\x00\x00", 5);
    BOOST_CHECK_EXCEPTION(ReadCompactSize(ss), std::ios_base::failure, isCanonicalException);

//0xffff用五个字节编码：
    ss.write("\xfe\xff\xff\x00\x00", 5);
    BOOST_CHECK_EXCEPTION(ReadCompactSize(ss), std::ios_base::failure, isCanonicalException);

//用九个字节对零进行编码：
    ss.write("\xff\x00\x00\x00\x00\x00\x00\x00\x00", 9);
    BOOST_CHECK_EXCEPTION(ReadCompactSize(ss), std::ios_base::failure, isCanonicalException);

//0x01ffffff用九个字节编码：
    ss.write("\xff\xff\xff\xff\x01\x00\x00\x00\x00", 9);
    BOOST_CHECK_EXCEPTION(ReadCompactSize(ss), std::ios_base::failure, isCanonicalException);
}

BOOST_AUTO_TEST_CASE(insert_delete)
{
//测试插入/删除字节。
    CDataStream ss(SER_DISK, 0);
    BOOST_CHECK_EQUAL(ss.size(), 0U);

    ss.write("\x00\x01\x02\xff", 4);
    BOOST_CHECK_EQUAL(ss.size(), 4U);

    char c = (char)11;

//在开始/结束/中间插入：
    ss.insert(ss.begin(), c);
    BOOST_CHECK_EQUAL(ss.size(), 5U);
    BOOST_CHECK_EQUAL(ss[0], c);
    BOOST_CHECK_EQUAL(ss[1], 0);

    ss.insert(ss.end(), c);
    BOOST_CHECK_EQUAL(ss.size(), 6U);
    BOOST_CHECK_EQUAL(ss[4], (char)0xff);
    BOOST_CHECK_EQUAL(ss[5], c);

    ss.insert(ss.begin()+2, c);
    BOOST_CHECK_EQUAL(ss.size(), 7U);
    BOOST_CHECK_EQUAL(ss[2], c);

//从开始/结束/中间删除
    ss.erase(ss.begin());
    BOOST_CHECK_EQUAL(ss.size(), 6U);
    BOOST_CHECK_EQUAL(ss[0], 0);

    ss.erase(ss.begin()+ss.size()-1);
    BOOST_CHECK_EQUAL(ss.size(), 5U);
    BOOST_CHECK_EQUAL(ss[4], (char)0xff);

    ss.erase(ss.begin()+1);
    BOOST_CHECK_EQUAL(ss.size(), 4U);
    BOOST_CHECK_EQUAL(ss[0], 0);
    BOOST_CHECK_EQUAL(ss[1], 1);
    BOOST_CHECK_EQUAL(ss[2], 2);
    BOOST_CHECK_EQUAL(ss[3], (char)0xff);

//确保getandclear做了正确的事情：
    CSerializeData d;
    ss.GetAndClear(d);
    BOOST_CHECK_EQUAL(ss.size(), 0U);
}

BOOST_AUTO_TEST_CASE(class_methods)
{
    int intval(100);
    bool boolval(true);
    std::string stringval("testing");
    const char charstrval[16] = "testing charstr";
    CMutableTransaction txval;
    CTransactionRef tx_ref{MakeTransactionRef(txval)};
    CSerializeMethodsTestSingle methodtest1(intval, boolval, stringval, charstrval, tx_ref);
    CSerializeMethodsTestMany methodtest2(intval, boolval, stringval, charstrval, tx_ref);
    CSerializeMethodsTestSingle methodtest3;
    CSerializeMethodsTestMany methodtest4;
    CDataStream ss(SER_DISK, PROTOCOL_VERSION);
    BOOST_CHECK(methodtest1 == methodtest2);
    ss << methodtest1;
    ss >> methodtest4;
    ss << methodtest2;
    ss >> methodtest3;
    BOOST_CHECK(methodtest1 == methodtest2);
    BOOST_CHECK(methodtest2 == methodtest3);
    BOOST_CHECK(methodtest3 == methodtest4);

    CDataStream ss2(SER_DISK, PROTOCOL_VERSION, intval, boolval, stringval, charstrval, txval);
    ss2 >> methodtest3;
    BOOST_CHECK(methodtest3 == methodtest4);
}

BOOST_AUTO_TEST_SUITE_END()
