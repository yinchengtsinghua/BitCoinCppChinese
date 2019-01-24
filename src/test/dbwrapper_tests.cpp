
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

#include <dbwrapper.h>
#include <uint256.h>
#include <random.h>
#include <test/test_bitcoin.h>

#include <memory>

#include <boost/test/unit_test.hpp>

//测试字符串是否完全由空字符组成
static bool is_null_key(const std::vector<unsigned char>& key) {
    bool isnull = true;

    for (unsigned int i = 0; i < key.size(); i++)
        isnull &= (key[i] == '\x00');

    return isnull;
}

BOOST_FIXTURE_TEST_SUITE(dbwrapper_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(dbwrapper)
{
//执行模糊和非模糊测试。
    for (const bool obfuscate : {false, true}) {
        fs::path ph = SetDataDir(std::string("dbwrapper").append(obfuscate ? "_true" : "_false"));
        CDBWrapper dbw(ph, (1 << 20), true, false, obfuscate);
        char key = 'k';
        uint256 in = InsecureRand256();
        uint256 res;

//当obfuscate=true时，确保我们正在进行真正的模糊处理。
        BOOST_CHECK(obfuscate != is_null_key(dbwrapper_private::GetObfuscateKey(dbw)));

        BOOST_CHECK(dbw.Write(key, in));
        BOOST_CHECK(dbw.Read(key, res));
        BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
    }
}

//测试批操作
BOOST_AUTO_TEST_CASE(dbwrapper_batch)
{
//执行模糊和非模糊测试。
    for (const bool obfuscate : {false, true}) {
        fs::path ph = SetDataDir(std::string("dbwrapper_batch").append(obfuscate ? "_true" : "_false"));
        CDBWrapper dbw(ph, (1 << 20), true, false, obfuscate);

        char key = 'i';
        uint256 in = InsecureRand256();
        char key2 = 'j';
        uint256 in2 = InsecureRand256();
        char key3 = 'k';
        uint256 in3 = InsecureRand256();

        uint256 res;
        CDBBatch batch(dbw);

        batch.Write(key, in);
        batch.Write(key2, in2);
        batch.Write(key3, in3);

//在写入之前删除键3
        batch.Erase(key3);

        BOOST_CHECK(dbw.WriteBatch(batch));

        BOOST_CHECK(dbw.Read(key, res));
        BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
        BOOST_CHECK(dbw.Read(key2, res));
        BOOST_CHECK_EQUAL(res.ToString(), in2.ToString());

//钥匙3不应该被写出来
        BOOST_CHECK(dbw.Read(key3, res) == false);
    }
}

BOOST_AUTO_TEST_CASE(dbwrapper_iterator)
{
//执行模糊和非模糊测试。
    for (const bool obfuscate : {false, true}) {
        fs::path ph = SetDataDir(std::string("dbwrapper_iterator").append(obfuscate ? "_true" : "_false"));
        CDBWrapper dbw(ph, (1 << 20), true, false, obfuscate);

//这两把钥匙是特意挑选来订购的
        char key = 'j';
        uint256 in = InsecureRand256();
        BOOST_CHECK(dbw.Write(key, in));
        char key2 = 'k';
        uint256 in2 = InsecureRand256();
        BOOST_CHECK(dbw.Write(key2, in2));

        std::unique_ptr<CDBIterator> it(const_cast<CDBWrapper&>(dbw).NewIterator());

//一定要越过模糊键（如果它存在的话）
        it->Seek(key);

        char key_res;
        uint256 val_res;

        BOOST_REQUIRE(it->GetKey(key_res));
        BOOST_REQUIRE(it->GetValue(val_res));
        BOOST_CHECK_EQUAL(key_res, key);
        BOOST_CHECK_EQUAL(val_res.ToString(), in.ToString());

        it->Next();

        BOOST_REQUIRE(it->GetKey(key_res));
        BOOST_REQUIRE(it->GetValue(val_res));
        BOOST_CHECK_EQUAL(key_res, key2);
        BOOST_CHECK_EQUAL(val_res.ToString(), in2.ToString());

        it->Next();
        BOOST_CHECK_EQUAL(it->Valid(), false);
    }
}

//测试如果存在现有数据，我们不会混淆。
BOOST_AUTO_TEST_CASE(existing_data_no_obfuscate)
{
//我们将在两个包装器之间共享此fs:：path
    fs::path ph = SetDataDir("existing_data_no_obfuscate");
    create_directories(ph);

//设置一个非模糊包装器来写入一些初始数据。
    std::unique_ptr<CDBWrapper> dbw = MakeUnique<CDBWrapper>(ph, (1 << 10), false, false, false);
    char key = 'k';
    uint256 in = InsecureRand256();
    uint256 res;

    BOOST_CHECK(dbw->Write(key, in));
    BOOST_CHECK(dbw->Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());

//调用析构函数释放leveldb锁
    dbw.reset();

//现在，设置另一个包装器，使同一目录混淆
    CDBWrapper odbw(ph, (1 << 10), false, false, true);

//检查我们用未混淆的包装器编写的key/val是否存在，以及
//是可读的。
    uint256 res2;
    BOOST_CHECK(odbw.Read(key, res2));
    BOOST_CHECK_EQUAL(res2.ToString(), in.ToString());

BOOST_CHECK(!odbw.IsEmpty()); //应该有现有的数据
BOOST_CHECK(is_null_key(dbwrapper_private::GetObfuscateKey(odbw))); //键应为空字符串

    uint256 in2 = InsecureRand256();
    uint256 res3;

//检查是否可以成功写入
    BOOST_CHECK(odbw.Write(key, in2));
    BOOST_CHECK(odbw.Read(key, res3));
    BOOST_CHECK_EQUAL(res3.ToString(), in2.ToString());
}

//确保在重新索引期间开始混淆。
BOOST_AUTO_TEST_CASE(existing_data_reindex)
{
//我们将在两个包装器之间共享此fs:：path
    fs::path ph = SetDataDir("existing_data_reindex");
    create_directories(ph);

//设置一个非模糊包装器来写入一些初始数据。
    std::unique_ptr<CDBWrapper> dbw = MakeUnique<CDBWrapper>(ph, (1 << 10), false, false, false);
    char key = 'k';
    uint256 in = InsecureRand256();
    uint256 res;

    BOOST_CHECK(dbw->Write(key, in));
    BOOST_CHECK(dbw->Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());

//调用析构函数释放leveldb锁
    dbw.reset();

//通过擦除现有数据存储来模拟-reindex
    CDBWrapper odbw(ph, (1 << 10), false, true, true);

//检查我们用未混淆的包装器编写的密钥/val是否不存在
    uint256 res2;
    BOOST_CHECK(!odbw.Read(key, res2));
    BOOST_CHECK(!is_null_key(dbwrapper_private::GetObfuscateKey(odbw)));

    uint256 in2 = InsecureRand256();
    uint256 res3;

//检查是否可以成功写入
    BOOST_CHECK(odbw.Write(key, in2));
    BOOST_CHECK(odbw.Read(key, res3));
    BOOST_CHECK_EQUAL(res3.ToString(), in2.ToString());
}

BOOST_AUTO_TEST_CASE(iterator_ordering)
{
    fs::path ph = SetDataDir("iterator_ordering");
    CDBWrapper dbw(ph, (1 << 20), true, false, false);
    for (int x=0x00; x<256; ++x) {
        uint8_t key = x;
        uint32_t value = x*x;
        if (!(x & 1)) BOOST_CHECK(dbw.Write(key, value));
    }

//检查创建迭代器是否创建快照
    std::unique_ptr<CDBIterator> it(const_cast<CDBWrapper&>(dbw).NewIterator());

    for (unsigned int x=0x00; x<256; ++x) {
        uint8_t key = x;
        uint32_t value = x*x;
        if (x & 1) BOOST_CHECK(dbw.Write(key, value));
    }

    for (const int seek_start : {0x00, 0x80}) {
        it->Seek((uint8_t)seek_start);
        for (unsigned int x=seek_start; x<255; ++x) {
            uint8_t key;
            uint32_t value;
            BOOST_CHECK(it->Valid());
if (!it->Valid()) //避免关于无效迭代器的键和值的错误，以防失败。
                break;
            BOOST_CHECK(it->GetKey(key));
            if (x & 1) {
                BOOST_CHECK_EQUAL(key, x + 1);
                continue;
            }
            BOOST_CHECK(it->GetValue(value));
            BOOST_CHECK_EQUAL(key, x);
            BOOST_CHECK_EQUAL(value, x*x);
            it->Next();
        }
        BOOST_CHECK(!it->Valid());
    }
}

struct StringContentsSerializer {
//用于使两个序列化对象相同，同时允许它们具有不同的长度
//这是个糟糕的主意
    std::string str;
    StringContentsSerializer() {}
    explicit StringContentsSerializer(const std::string& inp) : str(inp) {}

    StringContentsSerializer& operator+=(const std::string& s) {
        str += s;
        return *this;
    }
    StringContentsSerializer& operator+=(const StringContentsSerializer& s) { return *this += s.str; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        if (ser_action.ForRead()) {
            str.clear();
            char c = 0;
            while (true) {
                try {
                    READWRITE(c);
                    str.push_back(c);
                } catch (const std::ios_base::failure&) {
                    break;
                }
            }
        } else {
            for (size_t i = 0; i < str.size(); i++)
                READWRITE(str[i]);
        }
    }
};

BOOST_AUTO_TEST_CASE(iterator_string_ordering)
{
    char buf[10];

    fs::path ph = SetDataDir("iterator_string_ordering");
    CDBWrapper dbw(ph, (1 << 20), true, false, false);
    for (int x=0x00; x<10; ++x) {
        for (int y = 0; y < 10; y++) {
            snprintf(buf, sizeof(buf), "%d", x);
            StringContentsSerializer key(buf);
            for (int z = 0; z < y; z++)
                key += key;
            uint32_t value = x*x;
            BOOST_CHECK(dbw.Write(key, value));
        }
    }

    std::unique_ptr<CDBIterator> it(const_cast<CDBWrapper&>(dbw).NewIterator());
    for (const int seek_start : {0, 5}) {
        snprintf(buf, sizeof(buf), "%d", seek_start);
        StringContentsSerializer seek_key(buf);
        it->Seek(seek_key);
        for (unsigned int x=seek_start; x<10; ++x) {
            for (int y = 0; y < 10; y++) {
                snprintf(buf, sizeof(buf), "%d", x);
                std::string exp_key(buf);
                for (int z = 0; z < y; z++)
                    exp_key += exp_key;
                StringContentsSerializer key;
                uint32_t value;
                BOOST_CHECK(it->Valid());
if (!it->Valid()) //避免关于无效迭代器的键和值的错误，以防失败。
                    break;
                BOOST_CHECK(it->GetKey(key));
                BOOST_CHECK(it->GetValue(value));
                BOOST_CHECK_EQUAL(key.str, exp_key);
                BOOST_CHECK_EQUAL(value, x*x);
                it->Next();
            }
        }
        BOOST_CHECK(!it->Valid());
    }
}



BOOST_AUTO_TEST_SUITE_END()
