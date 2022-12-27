
#include <jobs.hpp>
#include "gtest/gtest.h"
#include <stdio.h>

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
    
    test_case test_cases[] = {{
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
    }/*, {
        "should return true for a tx with a boost output in first position"
    }, {
        "should return true for a tx with a boost output in second position"
    }*/
    };
    
    for (auto &tc : test_cases) EXPECT_TRUE(tc.result()) << tc.Explain;
    
}
    
std::string test_case::command() {
    return std::string{"./bin/DetectBoost "} + Input; 
}
    
std::optional<std::string> test_case::run() {
    
    if (FILE *fp = popen(command().c_str(), "r"); fp != nullptr) {
    
        char buf[128];
        
        std::string result;
        
        while (fgets(buf, 128, fp) != nullptr) 
            result += std::string{buf};
        
        if (pclose(fp)) return {};
        return result;
        
    } else throw data::exception{"could not open pipe"};
}
