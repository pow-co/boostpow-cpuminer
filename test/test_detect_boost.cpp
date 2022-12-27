
#include <jobs.hpp>
#include "gtest/gtest.h"
#include <stdio.h>

std::optional<std::string> run(std::string command);

struct test_case {
    std::string Input;
    bool ExpectedSuccess;
    std::string ExpectedOutput;
    std::string Explain;
    
    std::string command();
    
    std::optional<std::string> run();
    
    bool result() {
        auto r = run();
        return bool(r) ? ExpectedSuccess && *r == ExpectedOutput : !ExpectedSuccess;
    }
};

TEST(DetectBoostTest, TestDetectBoost) {
    
    std::vector<test_case> test_cases{{
        "", false, "", "should fail with no inputs."
    }, {
        "a b", false, "", "should fail with two inputs"
    }, {
        "hi", false, "", "should fail with an input that is not hex"
    }, {
        "ab", false, "", "should fail with an input that is not a Bitcoin transaction"
    }, {
        "0100000001fc9b6c8e58016f9e36d29b2a20536bb3202dc304aeb1785ffa015de4f3732cc50e"
        "0000006b483045022100e8eb4b41bd348201521b311a5048a7b192b8209db2c5bfbd1cbd0a6b"
        "a541302c022044a80e8b8023e114d85acceeeca9757c9f048102de6ea7e210d61d919895e34e"
        "412103af3ead8a3ab792225bf22262f0b81a72e5070788d363ee717c5868421b75a62dffffff"
        "ff01000000000000000040006a0a6d793263656e74732c201c716e557631774f7a4d77505876"
        "556e626e3169517054336344476e32152c20302e303134303735373231303439383133343600000000", 
        true, "", "should return false for an input with no boost outputs"
    }, {
        "0100000001248692b3f474fcff28b86d92acf3df1103fabc760aec4a7d7c569865461fb3f501000"
        "0006b483045022100be05adadc04b424e705f12741483f1d382268104b68b247d986a03d10e8bc7"
        "590220540be19473bf8603d283b058d0c9617fd734f70afc39672f9ecefba1d94decfd41210314d"
        "1417fbfc2dc5a585f5277d1b58d6b8671539e6bd4bc1cc0e3dcc83c062bc3ffffffff0240420f00"
        "00000000ad08626f6f7374706f77750400000000205aefde4557660b75e7ec7c17c66df64d1ce28"
        "71fa843c7c880ab5972491949ca046beb191c000433713660007e7c557a766b7e52796b557a8254"
        "887e557a8258887e7c7eaa7c6b7e7e7c8254887e6c7e7c8254887eaa01007e816c825488537f768"
        "1530121a5696b768100a0691d000000000000000000000000000000000000000000000000000000"
        "00007e6c539458959901007e819f6976a96c88ac18651400000000001976a9143317e65148b23e1"
        "43cbf43bbc69809a4413acbe788ac64c60b00", 
        true, "e1b7e7cce975dd41cc9024b94034e149808dc07ba319ebd18dba946aa4525a82\n", 
        "should return true for a tx with a boost output in first position"
    }, {
        "0100000001825a52a46a94ba8dd1eb19a37bc08d8049e13440b92490cc41dd75e9cce7b7e10100000"
        "06a4730440220784165f05a587a4a12c79fa31be40441e70d12c62cf5202db6a4a3a69e1bf0cc0220"
        "0ef07e6fb14a6c91be74165ee4461219c70e53905a81a5f09afc0ce209f30c76412103f39a7a1151c"
        "adff68ac9331ac9af6fbf2d57b5d6b350675bee58c641237c1ec6ffffffff02102205000000000019"
        "76a9144356fab9a6c00c08ef8855ff2c852906edabba3488ac40420f0000000000b308626f6f73747"
        "06f777504000000002002ea51cde1d853c91fce958f0b106a109260448beb6bf0a84ce698e131cfdd"
        "30040055551c0666616d696c79049ae3717f007e7c557a766b7e52796b557a8254887e557a8258887"
        "e7c7eaa7c6b7e7e7c8254887e6c7e7c8254887eaa01007e816c825488537f7681530121a5696b7681"
        "00a0691d00000000000000000000000000000000000000000000000000000000007e6c53945895990"
        "1007e819f6976a96c88ac64c60b00", 
        true, "6a85e7d909a6c6d28f18b051bf2c6d6f175c970c69468ff6a4aa850825098984\n", 
        "should return true for a tx with a boost output in second position"
    }};
    
    for (auto &tc : test_cases) EXPECT_TRUE(tc.result()) << tc.Explain;
    
}

std::optional<std::string> run(std::string command) {
    if (FILE *fp = popen(command.c_str(), "r"); fp != nullptr) {
    
        char buf[128];
        
        std::string result;
        
        while (fgets(buf, 128, fp) != nullptr) 
            result += std::string{buf};
        
        if (pclose(fp)) return {};
        return result;
        
    } else throw data::exception{"could not open pipe"};
}
    
std::string test_case::command() {
    return std::string{"../bin/DetectBoost "} + Input; 
}
    
std::optional<std::string> test_case::run() {
    return ::run(command());
}
