
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
/*版权所有（c）2011 LevelDB作者。版权所有。
  此源代码的使用受可以
  在许可证文件中找到。有关参与者的名称，请参阅作者文件。

  C级绑定。可能作为一个稳定的ABI有用
  由保存在共享库中的leveldb的程序使用，或
  一个JNI API。

  不支持：
  . 选项类型的getter
  . 实现键缩短的自定义比较器
  . 自定义ITER、DB、ENV、仅使用C绑定的缓存实现

  一些惯例：

  （1）我们只向客户机公开不透明的结构指针和函数。
  这允许我们在不必更改内部表示的情况下
  重新编译客户端。

  （2）为了简单起见，没有等价于切片类型的。相反，
  调用方必须单独传递指针和长度
  争论。

  （3）错误由以空结尾的C字符串表示。无效的
  意味着没有错误。传递所有可能引发错误的操作
  最后一个参数是“char**errptr”。必须满足以下条件之一
  输入时为真：
     ＊ErrpTr= =空
     *errptr指向malloc（）ed NULL终止错误消息
       （在Windows上，*errptr必须已被此库malloc（）编辑。）
  成功后，leveldb例程保持*errptr不变。
  失败时，leveldb释放*errptr的旧值，并
  将*errptr设置为malloc（）ed错误消息。

  （4）bool的类型为unsigned char（0==false；rest==true）

  （5）所有指针参数必须为非空。
**/


#ifndef STORAGE_LEVELDB_INCLUDE_C_H_
#define STORAGE_LEVELDB_INCLUDE_C_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/*导出类型*/

typedef struct leveldb_t               leveldb_t;
typedef struct leveldb_cache_t         leveldb_cache_t;
typedef struct leveldb_comparator_t    leveldb_comparator_t;
typedef struct leveldb_env_t           leveldb_env_t;
typedef struct leveldb_filelock_t      leveldb_filelock_t;
typedef struct leveldb_filterpolicy_t  leveldb_filterpolicy_t;
typedef struct leveldb_iterator_t      leveldb_iterator_t;
typedef struct leveldb_logger_t        leveldb_logger_t;
typedef struct leveldb_options_t       leveldb_options_t;
typedef struct leveldb_randomfile_t    leveldb_randomfile_t;
typedef struct leveldb_readoptions_t   leveldb_readoptions_t;
typedef struct leveldb_seqfile_t       leveldb_seqfile_t;
typedef struct leveldb_snapshot_t      leveldb_snapshot_t;
typedef struct leveldb_writablefile_t  leveldb_writablefile_t;
typedef struct leveldb_writebatch_t    leveldb_writebatch_t;
typedef struct leveldb_writeoptions_t  leveldb_writeoptions_t;

/*数据库操作*/

extern leveldb_t* leveldb_open(
    const leveldb_options_t* options,
    const char* name,
    char** errptr);

extern void leveldb_close(leveldb_t* db);

extern void leveldb_put(
    leveldb_t* db,
    const leveldb_writeoptions_t* options,
    const char* key, size_t keylen,
    const char* val, size_t vallen,
    char** errptr);

extern void leveldb_delete(
    leveldb_t* db,
    const leveldb_writeoptions_t* options,
    const char* key, size_t keylen,
    char** errptr);

extern void leveldb_write(
    leveldb_t* db,
    const leveldb_writeoptions_t* options,
    leveldb_writebatch_t* batch,
    char** errptr);

/*如果找不到，则返回空值。否则是一个malloc（）ed数组。
   以*vallen格式存储数组的长度。*/

extern char* leveldb_get(
    leveldb_t* db,
    const leveldb_readoptions_t* options,
    const char* key, size_t keylen,
    size_t* vallen,
    char** errptr);

extern leveldb_iterator_t* leveldb_create_iterator(
    leveldb_t* db,
    const leveldb_readoptions_t* options);

extern const leveldb_snapshot_t* leveldb_create_snapshot(
    leveldb_t* db);

extern void leveldb_release_snapshot(
    leveldb_t* db,
    const leveldb_snapshot_t* snapshot);

/*如果属性名未知，则返回空值。
   else返回指向malloc（）-ed以NULL结尾的值的指针。*/

extern char* leveldb_property_value(
    leveldb_t* db,
    const char* propname);

extern void leveldb_approximate_sizes(
    leveldb_t* db,
    int num_ranges,
    const char* const* range_start_key, const size_t* range_start_key_len,
    const char* const* range_limit_key, const size_t* range_limit_key_len,
    uint64_t* sizes);

extern void leveldb_compact_range(
    leveldb_t* db,
    const char* start_key, size_t start_key_len,
    const char* limit_key, size_t limit_key_len);

/*管理操作*/

extern void leveldb_destroy_db(
    const leveldb_options_t* options,
    const char* name,
    char** errptr);

extern void leveldb_repair_db(
    const leveldb_options_t* options,
    const char* name,
    char** errptr);

/*迭代器*/

extern void leveldb_iter_destroy(leveldb_iterator_t*);
extern unsigned char leveldb_iter_valid(const leveldb_iterator_t*);
extern void leveldb_iter_seek_to_first(leveldb_iterator_t*);
extern void leveldb_iter_seek_to_last(leveldb_iterator_t*);
extern void leveldb_iter_seek(leveldb_iterator_t*, const char* k, size_t klen);
extern void leveldb_iter_next(leveldb_iterator_t*);
extern void leveldb_iter_prev(leveldb_iterator_t*);
extern const char* leveldb_iter_key(const leveldb_iterator_t*, size_t* klen);
extern const char* leveldb_iter_value(const leveldb_iterator_t*, size_t* vlen);
extern void leveldb_iter_get_error(const leveldb_iterator_t*, char** errptr);

/*写入批处理*/

extern leveldb_writebatch_t* leveldb_writebatch_create();
extern void leveldb_writebatch_destroy(leveldb_writebatch_t*);
extern void leveldb_writebatch_clear(leveldb_writebatch_t*);
extern void leveldb_writebatch_put(
    leveldb_writebatch_t*,
    const char* key, size_t klen,
    const char* val, size_t vlen);
extern void leveldb_writebatch_delete(
    leveldb_writebatch_t*,
    const char* key, size_t klen);
extern void leveldb_writebatch_iterate(
    leveldb_writebatch_t*,
    void* state,
    void (*put)(void*, const char* k, size_t klen, const char* v, size_t vlen),
    void (*deleted)(void*, const char* k, size_t klen));

/*选项*/

extern leveldb_options_t* leveldb_options_create();
extern void leveldb_options_destroy(leveldb_options_t*);
extern void leveldb_options_set_comparator(
    leveldb_options_t*,
    leveldb_comparator_t*);
extern void leveldb_options_set_filter_policy(
    leveldb_options_t*,
    leveldb_filterpolicy_t*);
extern void leveldb_options_set_create_if_missing(
    leveldb_options_t*, unsigned char);
extern void leveldb_options_set_error_if_exists(
    leveldb_options_t*, unsigned char);
extern void leveldb_options_set_paranoid_checks(
    leveldb_options_t*, unsigned char);
extern void leveldb_options_set_env(leveldb_options_t*, leveldb_env_t*);
extern void leveldb_options_set_info_log(leveldb_options_t*, leveldb_logger_t*);
extern void leveldb_options_set_write_buffer_size(leveldb_options_t*, size_t);
extern void leveldb_options_set_max_open_files(leveldb_options_t*, int);
extern void leveldb_options_set_cache(leveldb_options_t*, leveldb_cache_t*);
extern void leveldb_options_set_block_size(leveldb_options_t*, size_t);
extern void leveldb_options_set_block_restart_interval(leveldb_options_t*, int);

enum {
  leveldb_no_compression = 0,
  leveldb_snappy_compression = 1
};
extern void leveldb_options_set_compression(leveldb_options_t*, int);

/*比较器*/

extern leveldb_comparator_t* leveldb_comparator_create(
    void* state,
    void (*destructor)(void*),
    int (*compare)(
        void*,
        const char* a, size_t alen,
        const char* b, size_t blen),
    const char* (*name)(void*));
extern void leveldb_comparator_destroy(leveldb_comparator_t*);

/*过滤策略*/

extern leveldb_filterpolicy_t* leveldb_filterpolicy_create(
    void* state,
    void (*destructor)(void*),
    char* (*create_filter)(
        void*,
        const char* const* key_array, const size_t* key_length_array,
        int num_keys,
        size_t* filter_length),
    unsigned char (*key_may_match)(
        void*,
        const char* key, size_t length,
        const char* filter, size_t filter_length),
    const char* (*name)(void*));
extern void leveldb_filterpolicy_destroy(leveldb_filterpolicy_t*);

extern leveldb_filterpolicy_t* leveldb_filterpolicy_create_bloom(
    int bits_per_key);

/*读取选项*/

extern leveldb_readoptions_t* leveldb_readoptions_create();
extern void leveldb_readoptions_destroy(leveldb_readoptions_t*);
extern void leveldb_readoptions_set_verify_checksums(
    leveldb_readoptions_t*,
    unsigned char);
extern void leveldb_readoptions_set_fill_cache(
    leveldb_readoptions_t*, unsigned char);
extern void leveldb_readoptions_set_snapshot(
    leveldb_readoptions_t*,
    const leveldb_snapshot_t*);

/*写入选项*/

extern leveldb_writeoptions_t* leveldb_writeoptions_create();
extern void leveldb_writeoptions_destroy(leveldb_writeoptions_t*);
extern void leveldb_writeoptions_set_sync(
    leveldb_writeoptions_t*, unsigned char);

/*隐藏物*/

extern leveldb_cache_t* leveldb_cache_create_lru(size_t capacity);
extern void leveldb_cache_destroy(leveldb_cache_t* cache);

/*埃恩*/

extern leveldb_env_t* leveldb_create_default_env();
extern void leveldb_env_destroy(leveldb_env_t*);

/*效用*/

/*免费通话（PTR）。
   要求：ptr是malloc（）-ed，由某个例程返回
   在此文件中。请注意，在某些情况下（通常在Windows上），您
   可能需要调用此例程而不是释放（ptr）来释放
   malloc（）-此库返回的ed内存。*/

extern void leveldb_free(void* ptr);

/*返回此版本的主要版本号。*/
extern int leveldb_major_version();

/*返回此版本的次要版本号。*/
extern int leveldb_minor_version();

#ifdef __cplusplus
/*/*端部外部“C”*/
第二节

endif/*存储级别包括*/

