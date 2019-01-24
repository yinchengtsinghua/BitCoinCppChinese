
//此源码被清华学神尹成大魔王专业翻译分析并修改
//尹成QQ77025077
//尹成微信18510341407
//尹成所在QQ群721929980
//尹成邮箱 yinc13@mails.tsinghua.edu.cn
//尹成毕业于清华大学,微软区块链领域全球最有价值专家
//https://mvp.microsoft.com/zh-cn/PublicProfile/4033620
//版权所有（c）2018比特币核心开发者
//根据MIT软件许可证分发，请参见随附的
//文件复制或http://www.opensource.org/licenses/mit-license.php。
#include <test/gen/crypto_gen.h>

#include <key.h>

#include <rapidcheck/gen/Arbitrary.h>
#include <rapidcheck/Gen.h>
#include <rapidcheck/gen/Predicate.h>
#include <rapidcheck/gen/Container.h>

/*生成1到20个用于op_checkmultisig的键*/
rc::Gen<std::vector<CKey>> MultisigKeys()
{
    return rc::gen::suchThat(rc::gen::arbitrary<std::vector<CKey>>(), [](const std::vector<CKey>& keys) {
        return keys.size() >= 1 && keys.size() <= 15;
    });
};
