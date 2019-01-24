
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
/***************************************************************
 *版权所有（c）2013-2015 Pieter Wuille*
 *根据麻省理工学院软件许可证分发，请参阅随附的*
 *文件复制或http://www.opensource.org/licenses/mit license.php.*
 ***************************************************************/


#ifndef SECP256K1_MODULE_RECOVERY_TESTS_H
#define SECP256K1_MODULE_RECOVERY_TESTS_H

static int recovery_test_nonce_function(unsigned char *nonce32, const unsigned char *msg32, const unsigned char *key32, const unsigned char *algo16, void *data, unsigned int counter) {
    (void) msg32;
    (void) key32;
    (void) algo16;
    (void) data;

    /*在第一次运行时，返回0以强制第二次运行*/
    if (counter == 0) {
        memset(nonce32, 0, 32);
        return 1;
    }
    /*在第二次运行时，返回溢出以强制第三次运行*/
    if (counter == 1) {
        memset(nonce32, 0xff, 32);
        return 1;
    }
    /*在下一次运行时，返回一个有效的nonce，但要掷硬币决定签名是否失败。*/
    memset(nonce32, 1, 32);
    return secp256k1_rand_bits(1);
}

void test_ecdsa_recovery_api(void) {
    /*设置只计算错误的上下文*/
    secp256k1_context *none = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    secp256k1_context *sign = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    secp256k1_context *vrfy = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
    secp256k1_context *both = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    secp256k1_pubkey pubkey;
    secp256k1_pubkey recpubkey;
    secp256k1_ecdsa_signature normal_sig;
    secp256k1_ecdsa_recoverable_signature recsig;
    unsigned char privkey[32] = { 1 };
    unsigned char message[32] = { 2 };
    int32_t ecount = 0;
    int recid = 0;
    unsigned char sig[74];
    unsigned char zero_privkey[32] = { 0 };
    unsigned char over_privkey[32] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    secp256k1_context_set_error_callback(none, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_error_callback(sign, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_error_callback(vrfy, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_error_callback(both, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(none, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(sign, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(vrfy, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(both, counting_illegal_callback_fn, &ecount);

    /*构造并验证相应的公钥。*/
    CHECK(secp256k1_ec_seckey_verify(ctx, privkey) == 1);
    CHECK(secp256k1_ec_pubkey_create(ctx, &pubkey, privkey) == 1);

    /*检查用于签名的错误上下文和空值*/
    ecount = 0;
    CHECK(secp256k1_ecdsa_sign_recoverable(none, &recsig, message, privkey, NULL, NULL) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_ecdsa_sign_recoverable(sign, &recsig, message, privkey, NULL, NULL) == 1);
    CHECK(ecount == 1);
    CHECK(secp256k1_ecdsa_sign_recoverable(vrfy, &recsig, message, privkey, NULL, NULL) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_ecdsa_sign_recoverable(both, &recsig, message, privkey, NULL, NULL) == 1);
    CHECK(ecount == 2);
    CHECK(secp256k1_ecdsa_sign_recoverable(both, NULL, message, privkey, NULL, NULL) == 0);
    CHECK(ecount == 3);
    CHECK(secp256k1_ecdsa_sign_recoverable(both, &recsig, NULL, privkey, NULL, NULL) == 0);
    CHECK(ecount == 4);
    CHECK(secp256k1_ecdsa_sign_recoverable(both, &recsig, message, NULL, NULL, NULL) == 0);
    CHECK(ecount == 5);
    /*这将失败或随机成功，在任何情况下都不会检查失败。*/
    secp256k1_ecdsa_sign_recoverable(both, &recsig, message, privkey, recovery_test_nonce_function, NULL);
    CHECK(ecount == 5);
    /*这些都会失败，但不会以arg_检查的方式。*/
    CHECK(secp256k1_ecdsa_sign_recoverable(both, &recsig, message, zero_privkey, NULL, NULL) == 0);
    CHECK(secp256k1_ecdsa_sign_recoverable(both, &recsig, message, over_privkey, NULL, NULL) == 0);
    /*这个会成功的。*/
    CHECK(secp256k1_ecdsa_sign_recoverable(both, &recsig, message, privkey, NULL, NULL) == 1);
    CHECK(ecount == 5);

    /*使用goofy nonce函数检查签名*/

    /*检查不正确的上下文和空值以进行恢复*/
    ecount = 0;
    CHECK(secp256k1_ecdsa_recover(none, &recpubkey, &recsig, message) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_ecdsa_recover(sign, &recpubkey, &recsig, message) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_ecdsa_recover(vrfy, &recpubkey, &recsig, message) == 1);
    CHECK(ecount == 2);
    CHECK(secp256k1_ecdsa_recover(both, &recpubkey, &recsig, message) == 1);
    CHECK(ecount == 2);
    CHECK(secp256k1_ecdsa_recover(both, NULL, &recsig, message) == 0);
    CHECK(ecount == 3);
    CHECK(secp256k1_ecdsa_recover(both, &recpubkey, NULL, message) == 0);
    CHECK(ecount == 4);
    CHECK(secp256k1_ecdsa_recover(both, &recpubkey, &recsig, NULL) == 0);
    CHECK(ecount == 5);

    /*检查空值以进行转换*/
    CHECK(secp256k1_ecdsa_sign(both, &normal_sig, message, privkey, NULL, NULL) == 1);
    ecount = 0;
    CHECK(secp256k1_ecdsa_recoverable_signature_convert(both, NULL, &recsig) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_ecdsa_recoverable_signature_convert(both, &normal_sig, NULL) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_ecdsa_recoverable_signature_convert(both, &normal_sig, &recsig) == 1);

    /*检查用于反/序列化的空值*/
    CHECK(secp256k1_ecdsa_sign_recoverable(both, &recsig, message, privkey, NULL, NULL) == 1);
    ecount = 0;
    CHECK(secp256k1_ecdsa_recoverable_signature_serialize_compact(both, NULL, &recid, &recsig) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_ecdsa_recoverable_signature_serialize_compact(both, sig, NULL, &recsig) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_ecdsa_recoverable_signature_serialize_compact(both, sig, &recid, NULL) == 0);
    CHECK(ecount == 3);
    CHECK(secp256k1_ecdsa_recoverable_signature_serialize_compact(both, sig, &recid, &recsig) == 1);

    CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(both, NULL, sig, recid) == 0);
    CHECK(ecount == 4);
    CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(both, &recsig, NULL, recid) == 0);
    CHECK(ecount == 5);
    CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(both, &recsig, sig, -1) == 0);
    CHECK(ecount == 6);
    CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(both, &recsig, sig, 5) == 0);
    CHECK(ecount == 7);
    /*签名溢出将失败，但不影响重新计数*/
    memcpy(sig, over_privkey, 32);
    CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(both, &recsig, sig, recid) == 0);
    CHECK(ecount == 7);

    /*清理*/
    secp256k1_context_destroy(none);
    secp256k1_context_destroy(sign);
    secp256k1_context_destroy(vrfy);
    secp256k1_context_destroy(both);
}

void test_ecdsa_recovery_end_to_end(void) {
    unsigned char extra[32] = {0x00};
    unsigned char privkey[32];
    unsigned char message[32];
    secp256k1_ecdsa_signature signature[5];
    secp256k1_ecdsa_recoverable_signature rsignature[5];
    unsigned char sig[74];
    secp256k1_pubkey pubkey;
    secp256k1_pubkey recpubkey;
    int recid = 0;

    /*生成随机密钥和消息。*/
    {
        secp256k1_scalar msg, key;
        random_scalar_order_test(&msg);
        random_scalar_order_test(&key);
        secp256k1_scalar_get_b32(privkey, &key);
        secp256k1_scalar_get_b32(message, &msg);
    }

    /*构造并验证相应的公钥。*/
    CHECK(secp256k1_ec_seckey_verify(ctx, privkey) == 1);
    CHECK(secp256k1_ec_pubkey_create(ctx, &pubkey, privkey) == 1);

    /*序列化/解析压缩并验证/恢复。*/
    extra[0] = 0;
    CHECK(secp256k1_ecdsa_sign_recoverable(ctx, &rsignature[0], message, privkey, NULL, NULL) == 1);
    CHECK(secp256k1_ecdsa_sign(ctx, &signature[0], message, privkey, NULL, NULL) == 1);
    CHECK(secp256k1_ecdsa_sign_recoverable(ctx, &rsignature[4], message, privkey, NULL, NULL) == 1);
    CHECK(secp256k1_ecdsa_sign_recoverable(ctx, &rsignature[1], message, privkey, NULL, extra) == 1);
    extra[31] = 1;
    CHECK(secp256k1_ecdsa_sign_recoverable(ctx, &rsignature[2], message, privkey, NULL, extra) == 1);
    extra[31] = 0;
    extra[0] = 1;
    CHECK(secp256k1_ecdsa_sign_recoverable(ctx, &rsignature[3], message, privkey, NULL, extra) == 1);
    CHECK(secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx, sig, &recid, &rsignature[4]) == 1);
    CHECK(secp256k1_ecdsa_recoverable_signature_convert(ctx, &signature[4], &rsignature[4]) == 1);
    CHECK(memcmp(&signature[4], &signature[0], 64) == 0);
    CHECK(secp256k1_ecdsa_verify(ctx, &signature[4], message, &pubkey) == 1);
    memset(&rsignature[4], 0, sizeof(rsignature[4]));
    CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rsignature[4], sig, recid) == 1);
    CHECK(secp256k1_ecdsa_recoverable_signature_convert(ctx, &signature[4], &rsignature[4]) == 1);
    CHECK(secp256k1_ecdsa_verify(ctx, &signature[4], message, &pubkey) == 1);
    /*解析压缩（具有恢复ID）并恢复。*/
    CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rsignature[4], sig, recid) == 1);
    CHECK(secp256k1_ecdsa_recover(ctx, &recpubkey, &rsignature[4], message) == 1);
    CHECK(memcmp(&pubkey, &recpubkey, sizeof(pubkey)) == 0);
    /*序列化/销毁/分析签名并再次验证。*/
    CHECK(secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx, sig, &recid, &rsignature[4]) == 1);
    sig[secp256k1_rand_bits(6)] += 1 + secp256k1_rand_int(255);
    CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rsignature[4], sig, recid) == 1);
    CHECK(secp256k1_ecdsa_recoverable_signature_convert(ctx, &signature[4], &rsignature[4]) == 1);
    CHECK(secp256k1_ecdsa_verify(ctx, &signature[4], message, &pubkey) == 0);
    /*再次恢复*/
    CHECK(secp256k1_ecdsa_recover(ctx, &recpubkey, &rsignature[4], message) == 0 ||
          memcmp(&pubkey, &recpubkey, sizeof(pubkey)) != 0);
}

/*测试几个边缘案例。*/
void test_ecdsa_recovery_edge_cases(void) {
    const unsigned char msg32[32] = {
        'T', 'h', 'i', 's', ' ', 'i', 's', ' ',
        'a', ' ', 'v', 'e', 'r', 'y', ' ', 's',
        'e', 'c', 'r', 'e', 't', ' ', 'm', 'e',
        's', 's', 'a', 'g', 'e', '.', '.', '.'
    };
    const unsigned char sig64[64] = {
        /*通过用nonce“这是我们将使用的nonce…”对上述消息进行签名生成。
         *和密钥0（无效），导致recid 0。*/

        0x67, 0xCB, 0x28, 0x5F, 0x9C, 0xD1, 0x94, 0xE8,
        0x40, 0xD6, 0x29, 0x39, 0x7A, 0xF5, 0x56, 0x96,
        0x62, 0xFD, 0xE4, 0x46, 0x49, 0x99, 0x59, 0x63,
        0x17, 0x9A, 0x7D, 0xD1, 0x7B, 0xD2, 0x35, 0x32,
        0x4B, 0x1B, 0x7D, 0xF3, 0x4C, 0xE1, 0xF6, 0x8E,
        0x69, 0x4F, 0xF6, 0xF1, 0x1A, 0xC7, 0x51, 0xDD,
        0x7D, 0xD7, 0x3E, 0x38, 0x7E, 0xE4, 0xFC, 0x86,
        0x6E, 0x1B, 0xE8, 0xEC, 0xC7, 0xDD, 0x95, 0x57
    };
    secp256k1_pubkey pubkey;
    /*签名（r，s）=（4，4），可以用所有4个Recid恢复。*/
    const unsigned char sigb64[64] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
    };
    secp256k1_pubkey pubkeyb;
    secp256k1_ecdsa_recoverable_signature rsig;
    secp256k1_ecdsa_signature sig;
    int recid;

    CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rsig, sig64, 0));
    CHECK(!secp256k1_ecdsa_recover(ctx, &pubkey, &rsig, msg32));
    CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rsig, sig64, 1));
    CHECK(secp256k1_ecdsa_recover(ctx, &pubkey, &rsig, msg32));
    CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rsig, sig64, 2));
    CHECK(!secp256k1_ecdsa_recover(ctx, &pubkey, &rsig, msg32));
    CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rsig, sig64, 3));
    CHECK(!secp256k1_ecdsa_recover(ctx, &pubkey, &rsig, msg32));

    for (recid = 0; recid < 4; recid++) {
        int i;
        int recid2;
        /*（4,4）编码在DER中。*/
        unsigned char sigbder[8] = {0x30, 0x06, 0x02, 0x01, 0x04, 0x02, 0x01, 0x04};
        unsigned char sigcder_zr[7] = {0x30, 0x05, 0x02, 0x00, 0x02, 0x01, 0x01};
        unsigned char sigcder_zs[7] = {0x30, 0x05, 0x02, 0x01, 0x01, 0x02, 0x00};
        unsigned char sigbderalt1[39] = {
            0x30, 0x25, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x04, 0x02, 0x01, 0x04,
        };
        unsigned char sigbderalt2[39] = {
            0x30, 0x25, 0x02, 0x01, 0x04, 0x02, 0x20, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
        };
        unsigned char sigbderalt3[40] = {
            0x30, 0x26, 0x02, 0x21, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x04, 0x02, 0x01, 0x04,
        };
        unsigned char sigbderalt4[40] = {
            0x30, 0x26, 0x02, 0x01, 0x04, 0x02, 0x21, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
        };
        /*（order+r，4）编码在der中。*/
        unsigned char sigbderlong[40] = {
            0x30, 0x26, 0x02, 0x21, 0x00, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xBA, 0xAE, 0xDC,
            0xE6, 0xAF, 0x48, 0xA0, 0x3B, 0xBF, 0xD2, 0x5E,
            0x8C, 0xD0, 0x36, 0x41, 0x45, 0x02, 0x01, 0x04
        };
        CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rsig, sigb64, recid) == 1);
        CHECK(secp256k1_ecdsa_recover(ctx, &pubkeyb, &rsig, msg32) == 1);
        CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigbder, sizeof(sigbder)) == 1);
        CHECK(secp256k1_ecdsa_verify(ctx, &sig, msg32, &pubkeyb) == 1);
        for (recid2 = 0; recid2 < 4; recid2++) {
            secp256k1_pubkey pubkey2b;
            CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rsig, sigb64, recid2) == 1);
            CHECK(secp256k1_ecdsa_recover(ctx, &pubkey2b, &rsig, msg32) == 1);
            /*使用（ORDER+R，4）进行验证应始终失败。*/
            CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigbderlong, sizeof(sigbderlong)) == 1);
            CHECK(secp256k1_ecdsa_verify(ctx, &sig, msg32, &pubkeyb) == 0);
        }
        /*der解析测试。*/
        /*零长度r/s。*/
        CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigcder_zr, sizeof(sigcder_zr)) == 0);
        CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigcder_zs, sizeof(sigcder_zs)) == 0);
        /*前导零。*/
        CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigbderalt1, sizeof(sigbderalt1)) == 0);
        CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigbderalt2, sizeof(sigbderalt2)) == 0);
        CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigbderalt3, sizeof(sigbderalt3)) == 0);
        CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigbderalt4, sizeof(sigbderalt4)) == 0);
        sigbderalt3[4] = 1;
        CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigbderalt3, sizeof(sigbderalt3)) == 1);
        CHECK(secp256k1_ecdsa_verify(ctx, &sig, msg32, &pubkeyb) == 0);
        sigbderalt4[7] = 1;
        CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigbderalt4, sizeof(sigbderalt4)) == 1);
        CHECK(secp256k1_ecdsa_verify(ctx, &sig, msg32, &pubkeyb) == 0);
        /*损伤特征。*/
        sigbder[7]++;
        CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigbder, sizeof(sigbder)) == 1);
        CHECK(secp256k1_ecdsa_verify(ctx, &sig, msg32, &pubkeyb) == 0);
        sigbder[7]--;
        CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigbder, 6) == 0);
        CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigbder, sizeof(sigbder) - 1) == 0);
        for(i = 0; i < 8; i++) {
            int c;
            unsigned char orig = sigbder[i];
            /*每改变一个字节*/
            for (c = 0; c < 256; c++) {
                if (c == orig ) {
                    continue;
                }
                sigbder[i] = c;
                CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigbder, sizeof(sigbder)) == 0 || secp256k1_ecdsa_verify(ctx, &sig, msg32, &pubkeyb) == 0);
            }
            sigbder[i] = orig;
        }
    }

    /*测试r/s等于零*/
    {
        /*（1,1）以der编码。*/
        unsigned char sigcder[8] = {0x30, 0x06, 0x02, 0x01, 0x01, 0x02, 0x01, 0x01};
        unsigned char sigc64[64] = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
        };
        secp256k1_pubkey pubkeyc;
        CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rsig, sigc64, 0) == 1);
        CHECK(secp256k1_ecdsa_recover(ctx, &pubkeyc, &rsig, msg32) == 1);
        CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigcder, sizeof(sigcder)) == 1);
        CHECK(secp256k1_ecdsa_verify(ctx, &sig, msg32, &pubkeyc) == 1);
        sigcder[4] = 0;
        sigc64[31] = 0;
        CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rsig, sigc64, 0) == 1);
        CHECK(secp256k1_ecdsa_recover(ctx, &pubkeyb, &rsig, msg32) == 0);
        CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigcder, sizeof(sigcder)) == 1);
        CHECK(secp256k1_ecdsa_verify(ctx, &sig, msg32, &pubkeyc) == 0);
        sigcder[4] = 1;
        sigcder[7] = 0;
        sigc64[31] = 1;
        sigc64[63] = 0;
        CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rsig, sigc64, 0) == 1);
        CHECK(secp256k1_ecdsa_recover(ctx, &pubkeyb, &rsig, msg32) == 0);
        CHECK(secp256k1_ecdsa_signature_parse_der(ctx, &sig, sigcder, sizeof(sigcder)) == 1);
        CHECK(secp256k1_ecdsa_verify(ctx, &sig, msg32, &pubkeyc) == 0);
    }
}

void run_recovery_tests(void) {
    int i;
    for (i = 0; i < count; i++) {
        test_ecdsa_recovery_api();
    }
    for (i = 0; i < 64*count; i++) {
        test_ecdsa_recovery_end_to_end();
    }
    test_ecdsa_recovery_edge_cases();
}

/*DIF/*secp256k1_模块_恢复_测试_h*/
