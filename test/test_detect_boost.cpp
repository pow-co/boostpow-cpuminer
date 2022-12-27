
#include <jobs.hpp>
#include "gtest/gtest.h"
#include <stdio.h>

struct test_case {
    std::string Input;
    bool ExpectedSuccess;
    std::string ExpectedOutput;
    std::string Explain;
    
    std::string command();
    
    std::string run();
    
    bool result() {
        std::string r = run();
        return ExpectedSuccess ? r == ExpectedOutput : r == "";
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
    }/*, {
        "should return false for an input with no boost outputs"
    }, {
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
    
std::string test_case::run() {
    
    if (FILE *fp = popen(command().c_str(), "r"); fp != nullptr) {
    
        char buf[128];
        
        std::string result;
        
        while (fgets(buf, 128, fp) != NULL) {
            // Do whatever you want here...
            result += std::string{buf};
        }
        
        pclose(fp);
        return result;
        
    } else throw data::exception{"could not open pipe"};
}
