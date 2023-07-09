#include <miner_options.hpp>
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

    int redeem (const Bitcoin::outpoint &, const bytes &, int64_t, const redeeming_options &) {
        return 0;
    }

    int mine (const mining_options &) {
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
            // minimal redeem command.
            test_case {true,  {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "--key=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            test_case {true,  {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "--wif=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            test_case {true,  {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            test_case {true,  {"BoostMiner", "redeem", "--txid=0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "--index=0", "--key=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            // no secret key
            test_case {false, {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", "0"}},
            // invalid txid
            test_case {false, {"BoostMiner", "redeem", "0x00112233445566778899aabbcddeeff00112233445566778899aabbccddeeff",
                "0", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            // invalid index
            test_case {true,  {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "-3", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            // minimal mine command.
            test_case {true,  {"BoostMiner", "mine", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            test_case {true,  {"BoostMiner", "mine", "--key=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            test_case {true,  {"BoostMiner", "mine", "--wif=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            // invalid keys.
            test_case {false, {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "--key=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYq"}},
            test_case {false, {"BoostMiner", "mine", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYq"}},
            // HD keys.
            test_case {true,  {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", "0",
                "--key=xprvDtLp33sh6a9oZ927HKUzSxGxsZwxPsEs4TNo43X1oKdb6hgXNWNwUH6H6kxi1cR5UxKoFny2XeMPqx4SgMHW19v5NPyZ6q6y3FvtKihA6Ev"}},
            test_case {true,  {"BoostMiner", "mine",
                "xprvDtLp33sh6a9oZ927HKUzSxGxsZwxPsEs4TNo43X1oKdb6hgXNWNwUH6H6kxi1cR5UxKoFny2XeMPqx4SgMHW19v5NPyZ6q6y3FvtKihA6Ev"}},
            // invalid HD keys.
            test_case {false, {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", "0",
                "--key=xprvDtLp33sh6a9oZ927HKUzSxGxsZwxPsEs4TNo43X1oKdb6hgXNWNwUH6H6kxi1cR5UxKoFny2XeMPqx4SgMHW19v5NPyZ6q6y3FvtKihA6E"}},
            test_case {false, {"BoostMiner", "mine",
                "xprvDtLp33sh6a9oZ927HKUzSxGxsZwxPsEs4TNo43X1oKdb6hgXNWNwUH6H6kxi1cR5UxKoFny2XeMPqx4SgMHW19v5NPyZ6q6y3FvtKihA6E"}},
            // invalid threads value.
            test_case {false, {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "--key=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ", "--threads=0"}},
            test_case {false, {"BoostMiner", "mine", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ", "--threads=0"}},
            // valid threads value.
            test_case {true,  {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "--key=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ", "--threads=1"}},
            test_case {true,  {"BoostMiner", "mine", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ", "--threads=1"}},
            // with script and sats
            test_case {true,  {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ",
                "bitcoin-script:010108626f6f7374706f7775040000000020ffeeddccbbaa99887766554433221100ff"
                    "eeddccbbaa9988776655443322110004feff011d00042133c96c007e7c557a766b7e52796b567a8254887e"
                    "567a820120a1697e7c7eaa7c6b7e6b04ff1f00e076836b847c6c84856c7e7c8254887e6c7e7c8254887eaa"
                    "01007e816c825488537f7681530121a5696b768100a0691d00000000000000000000000000000000000000"
                    "000000000000000000007e6c539458959901007e819f6976a96c88acd2e72ac8", "1"}},
            test_case {true,  {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "--script=bitcoin-script:010108626f6f7374706f7775040000000020ffeeddccbbaa99887766554433221100ff"
                    "eeddccbbaa9988776655443322110004feff011d00042133c96c007e7c557a766b7e52796b567a8254887e"
                    "567a820120a1697e7c7eaa7c6b7e6b04ff1f00e076836b847c6c84856c7e7c8254887e6c7e7c8254887eaa"
                    "01007e816c825488537f7681530121a5696b768100a0691d00000000000000000000000000000000000000"
                    "000000000000000000007e6c539458959901007e819f6976a96c88acd2e72ac8",
                "--value=1", "--key=KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            // hex script
            test_case {true,  {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ",
                    "08626f6f7374706f7775040000000020ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766"
                    "55443322110004feff011d00040590c213007e7c557a766b7e52796b567a8254887e567a820120a1697e"
                    "7c7eaa7c6b7e6b04ff1f00e076836b847c6c84856c7e7c8254887e6c7e7c8254887eaa01007e816c8254"
                    "88537f7681530121a5696b768100a0691d00000000000000000000000000000000000000000000000000"
                    "000000007e6c539458959901007e819f6976a96c88ac",
                "1"}},
            // invalid script
            test_case {false, {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ", "bitcoin-script", "1"}},
            // invalid sats
            test_case {false, {"BoostMiner", "redeem", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "bitcoin-script:010108626f6f7374706f7775040000000020ffeeddccbbaa99887766554433221100ff"
                    "eeddccbbaa9988776655443322110004feff011d00042133c96c007e7c557a766b7e52796b567a8254887e"
                    "567a820120a1697e7c7eaa7c6b7e6b04ff1f00e076836b847c6c84856c7e7c8254887e6c7e7c8254887eaa"
                    "01007e816c825488537f7681530121a5696b768100a0691d00000000000000000000000000000000000000"
                    "000000000000000000007e6c539458959901007e819f6976a96c88acd2e72ac8",
                "0", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}},
            // backwards compatible input variant with the output first.
            test_case {true,  {"BoostMiner", "redeem", "bitcoin-script:010108626f6f7374706f7775040000000020ffeeddccbbaa99887766554433221100ff"
                    "eeddccbbaa9988776655443322110004feff011d00042133c96c007e7c557a766b7e52796b567a8254887e"
                    "567a820120a1697e7c7eaa7c6b7e6b04ff1f00e076836b847c6c84856c7e7c8254887e6c7e7c8254887eaa"
                    "01007e816c825488537f7681530121a5696b768100a0691d00000000000000000000000000000000000000"
                    "000000000000000000007e6c539458959901007e819f6976a96c88acd2e72ac8",
                "1", "0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                "0", "KwFevqMbSXhGxNWuVc6vuERwdXq7aDQtiLNkjPVokF87RsGMBYqZ"}}
        }) tx.run ();
    }

}
