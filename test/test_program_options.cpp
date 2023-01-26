#include <miner_options.hpp>
#include <gigamonkey/schema/hd.hpp>
#include "gtest/gtest.h"
#include <stdio.h>

namespace BoostPOW {

    int help () {
        return 0;
    }

    int version () {
        return 0;
    }

    int spend (const script_options &) {
        return 0;
    }

    int redeem (const Bitcoin::outpoint &, const Boost::output_script &, int64_t, const mining_options &) {
        return 0;
    }

    int mine (double min_profitability, double max_difficulty, const mining_options &) {
        return 0;
    }

    struct test_case {
        bool ExpectValid;
        stack<string> Input;
        int ArgCount;
        char ** ArgValues;

        test_case (bool expect_valid, stack<string> in) :
            ExpectValid {expect_valid}, Input {in}, ArgCount {in.size ()}, ArgValues {new char *[ArgCount]} {
            int i = 0;
            for (const string &w : Input) ArgValues[i++] = const_cast<char*> (w.data ());
        }

        void run () const {
            bool valid = BoostPOW::run (argh::parser (ArgCount, ArgValues), help, version, spend, redeem, mine) == 0;
            EXPECT_EQ (valid, ExpectValid) << "failure on input " << Input << "; expected " << std::boolalpha << ExpectValid;
        }
    };

    TEST (ProgramOptionsTest, TestProgramOptions) {

        Bitcoin::secret k {Bitcoin::secret::main, secp256k1::secret {1}, true};
        HD::BIP_32::secret kkk {secp256k1::secret {1}, data::bytes (32), HD::BIP_32::main};

        for (auto &tx : stack<test_case> {
            test_case {true,  {"BoostMiner", "help"}},
            test_case {true,  {"BoostMiner", "--help"}},
            test_case {true,  {"BoostMiner", "version"}},
            test_case {true,  {"BoostMiner", "--version"}},
            // commands without arguments.
            test_case {false, {"BoostMiner", "spend"}},
            test_case {false, {"BoostMiner", "redeem"}},
            test_case {false, {"BoostMiner", "mine"}},
            // minimal spend command with options
            test_case {true,  {"BoostMiner", "spend", "--content=0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "--difficulty=.5"}},
            // minimal spend with options in a different place.
            test_case {true,  {"BoostMiner", "--content=0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "--difficulty=.5", "spend"}},
            test_case {true,  {"BoostMiner", "spend", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", ".5"}},
            // alternate hash format
            test_case {true,  {"BoostMiner", "spend", "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", ".5"}},
            // invalid hash
            test_case {false, {"BoostMiner", "spend", "0x", ".5"}},
            // invalid difficulty
            test_case {false, {"BoostMiner", "spend", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", "-1"}},
            // topic and additional data
            test_case {true,  {"BoostMiner", "spend", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                ".5", "hi.topic"}},
            test_case {true,  {"BoostMiner", "spend", "--topic=hi.topic",
                "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", ".5"}},
            test_case {true,  {"BoostMiner", "spend", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", ".5",
                "hi.topic", "wah.wah.wah"}},
            // topic and data with spaces inside the string. This is how it gets turned to the program when you use quotes.
            test_case {true,  {"BoostMiner", "spend", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                ".5", "hi topic", "wah wah wah"}},
            test_case {true,  {"BoostMiner", "spend", "--topic=hi topic", "--data=wah wah wah",
                "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", ".5"}},
            test_case {true,  {"BoostMiner", "spend", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", ".5 "
                "hi.topic", "wah.wah.wah", "19pEUX5s6aNoE1oyjtzUkKTLENZyAYSJC8"}},
            test_case {true,  {"BoostMiner", "spend", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", ".5 "
                "--address=19pEUX5s6aNoE1oyjtzUkKTLENZyAYSJC8", "hi.topic", "wah.wah.wah"}},
            test_case {true,  {"BoostMiner", "spend", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                ".5", "--address=19pEUX5s6aNoE1oyjtzUkKTLENZyAYSJC8"}},
            // invalid address
            test_case {false,  {"BoostMiner", "spend", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                ".5", "--address=19pEUX5s6aNoE1oyjtzUkKTLENZyAYSJC"}},
            // no secret key
            test_case {false, {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", "0"}},
            // minimal redeem command.
            test_case {true,  {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "--secret=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            test_case {true,  {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "--wif=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            // minimal mine command.
            test_case {true,  {"BoostMiner", "mine", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            test_case {true,  {"BoostMiner", "mine", "--secret=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            test_case {true,  {"BoostMiner", "mine", "--wif=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            // invalid keys.
            test_case {false, {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "--secret=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYq"}},
            test_case {false, {"BoostMiner", "mine", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYq"}},
            // HD keys.
            test_case {true,  {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", "0",
                "--secret=xprvDtLp33sh6a9oZ927HKUzSxGxsZwxPsEs4TNo43X1oKdb6hgXNWNwUH6H6kxi1cR5UxKoFny2XeMPqx4SgMHW19v5NPyZ6q6y3FvtKihA6Ev"}},
            test_case {true,  {"BoostMiner", "mine",
                "xprvDtLp33sh6a9oZ927HKUzSxGxsZwxPsEs4TNo43X1oKdb6hgXNWNwUH6H6kxi1cR5UxKoFny2XeMPqx4SgMHW19v5NPyZ6q6y3FvtKihA6Ev"}},
            // invalid HD keys.
            test_case {false, {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", "0",
                "--secret=xprvDtLp33sh6a9oZ927HKUzSxGxsZwxPsEs4TNo43X1oKdb6hgXNWNwUH6H6kxi1cR5UxKoFny2XeMPqx4SgMHW19v5NPyZ6q6y3FvtKihA6E"}},
            test_case {false, {"BoostMiner", "mine",
                "xprvDtLp33sh6a9oZ927HKUzSxGxsZwxPsEs4TNo43X1oKdb6hgXNWNwUH6H6kxi1cR5UxKoFny2XeMPqx4SgMHW19v5NPyZ6q6y3FvtKihA6E"}},
            // invalid threads value.
            test_case {false, {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "--secret=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ", "--threads=0"}},
            test_case {false, {"BoostMiner", "mine", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ", "--threads=0"}},
            // valid threads value.
            test_case {true,  {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "--secret=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ", "--threads=1"}},
            test_case {true,  {"BoostMiner", "mine", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ", "--threads=1"}}
        }) tx.run ();
    }

}
