
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
/***************************************************************
 *版权所有（c）2014-2015 Pieter Wuille*
 *根据麻省理工学院软件许可证分发，请参阅随附的*
 *文件复制或http://www.opensource.org/licenses/mit license.php.*
 ***************************************************************/


#include "include/secp256k1.h"
#include "include/secp256k1_recovery.h"
#include "util.h"
#include "bench.h"

typedef struct {
    secp256k1_context *ctx;
    unsigned char msg[32];
    unsigned char sig[64];
} bench_recover_t;

void bench_recover(void* arg) {
    int i;
    bench_recover_t *data = (bench_recover_t*)arg;
    secp256k1_pubkey pubkey;
    unsigned char pubkeyc[33];

    for (i = 0; i < 20000; i++) {
        int j;
        size_t pubkeylen = 33;
        secp256k1_ecdsa_recoverable_signature sig;
        CHECK(secp256k1_ecdsa_recoverable_signature_parse_compact(data->ctx, &sig, data->sig, i % 2));
        CHECK(secp256k1_ecdsa_recover(data->ctx, &pubkey, &sig, data->msg));
        CHECK(secp256k1_ec_pubkey_serialize(data->ctx, pubkeyc, &pubkeylen, &pubkey, SECP256K1_EC_COMPRESSED));
        for (j = 0; j < 32; j++) {
            /*a->sig[j+32]=data->msg[j]；/*将以前的消息移到s.*/
            data->msg[j]=data->sig[j]；/*将former r移到message。*/

            /*a->sig[j]=pubkeyc[j+1]；/*将恢复的pubkey x坐标移动到r（必须是有效的x坐标）。*/
        }
    }
}

void bench_recover_setup（void*arg）
    INTI；
    工作台恢复数据=（工作台恢复数据）arg；

    对于（i=0；i<32；i++）
        data->msg[i]=1+i；
    }
    对于（i=0；i<64；i++）
        data->sig[i]=65+i；
    }
}

利息主（空）
    工作台恢复数据；

    data.ctx=secp256k1_context_create（secp256k1_context_verify）；

    运行_benchmark（“ecdsa_recover”，Bench_recover，Bench_recover_setup，null，&data，10，20000）；

    secp256k1_context_destroy（data.ctx）；
    返回0；
}
