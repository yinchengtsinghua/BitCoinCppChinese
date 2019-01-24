
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
/***************************************************************
 *版权所有（c）2015 Andrew Poelstra*
 *根据麻省理工学院软件许可证分发，请参阅随附的*
 *文件复制或http://www.opensource.org/licenses/mit license.php.*
 ***************************************************************/


#ifndef SECP256K1_MODULE_ECDH_TESTS_H
#define SECP256K1_MODULE_ECDH_TESTS_H

void test_ecdh_api(void) {
    /*只计算错误的安装上下文*/
    secp256k1_context *tctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    secp256k1_pubkey point;
    unsigned char res[32];
    unsigned char s_one[32] = { 0 };
    int32_t ecount = 0;
    s_one[31] = 1;

    secp256k1_context_set_error_callback(tctx, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(tctx, counting_illegal_callback_fn, &ecount);
    CHECK(secp256k1_ec_pubkey_create(tctx, &point, s_one) == 1);

    /*检查是否检测到所有空值*/
    CHECK(secp256k1_ecdh(tctx, res, &point, s_one) == 1);
    CHECK(ecount == 0);
    CHECK(secp256k1_ecdh(tctx, NULL, &point, s_one) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_ecdh(tctx, res, NULL, s_one) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_ecdh(tctx, res, &point, NULL) == 0);
    CHECK(ecount == 3);
    CHECK(secp256k1_ecdh(tctx, res, &point, s_one) == 1);
    CHECK(ecount == 3);

    /*清理*/
    secp256k1_context_destroy(tctx);
}

void test_ecdh_generator_basepoint(void) {
    unsigned char s_one[32] = { 0 };
    secp256k1_pubkey point[2];
    int i;

    s_one[31] = 1;
    /*当基点是生成器时，检查是否创建了pubkey*/
    for (i = 0; i < 100; ++i) {
        secp256k1_sha256_t sha;
        unsigned char s_b32[32];
        unsigned char output_ecdh[32];
        unsigned char output_ser[32];
        unsigned char point_ser[33];
        size_t point_ser_len = sizeof(point_ser);
        secp256k1_scalar s;

        random_scalar_order(&s);
        secp256k1_scalar_get_b32(s_b32, &s);

        /*用ECDH函数计算*/
        CHECK(secp256k1_ec_pubkey_create(ctx, &point[0], s_one) == 1);
        CHECK(secp256k1_ecdh(ctx, output_ecdh, &point[0], s_b32) == 1);
        /*“显式”计算*/
        CHECK(secp256k1_ec_pubkey_create(ctx, &point[1], s_b32) == 1);
        CHECK(secp256k1_ec_pubkey_serialize(ctx, point_ser, &point_ser_len, &point[1], SECP256K1_EC_COMPRESSED) == 1);
        CHECK(point_ser_len == sizeof(point_ser));
        secp256k1_sha256_initialize(&sha);
        secp256k1_sha256_write(&sha, point_ser, point_ser_len);
        secp256k1_sha256_finalize(&sha, output_ser);
        /*比较*/
        CHECK(memcmp(output_ecdh, output_ser, sizeof(output_ser)) == 0);
    }
}

void test_bad_scalar(void) {
    unsigned char s_zero[32] = { 0 };
    unsigned char s_overflow[32] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
        0xba, 0xae, 0xdc, 0xe6, 0xaf, 0x48, 0xa0, 0x3b,
        0xbf, 0xd2, 0x5e, 0x8c, 0xd0, 0x36, 0x41, 0x41
    };
    unsigned char s_rand[32] = { 0 };
    unsigned char output[32];
    secp256k1_scalar rand;
    secp256k1_pubkey point;

    /*创建随机点*/
    random_scalar_order(&rand);
    secp256k1_scalar_get_b32(s_rand, &rand);
    CHECK(secp256k1_ec_pubkey_create(ctx, &point, s_rand) == 1);

    /*试着用坏值乘以它*/
    CHECK(secp256k1_ecdh(ctx, output, &point, s_zero) == 0);
    CHECK(secp256k1_ecdh(ctx, output, &point, s_overflow) == 0);
    /*…还有一个不错的*/
    s_overflow[31] -= 1;
    CHECK(secp256k1_ecdh(ctx, output, &point, s_overflow) == 1);
}

void run_ecdh_tests(void) {
    test_ecdh_api();
    test_ecdh_generator_basepoint();
    test_bad_scalar();
}

/*DIF/*secp256k1_module_ecdh_tests_h*/
