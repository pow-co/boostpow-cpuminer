
#ifndef BOOSTPOW_CPUMINER_PROGRAM_OPTIONS
#define BOOSTPOW_CPUMINER_PROGRAM_OPTIONS

#include <string>
#include <optional>
#include <argh.h>
#include "jobs.hpp"

namespace BoostPOW {
    using namespace Gigamonkey;

    struct script_options {
        // Content is what is to be boosted. Could be a hash or
        // could be text that's 32 bytes or less. There is a
        // BIG PROBLEM with the fact that hashes in Bitcoin are
        // often displayed reversed. This is a convention that
        // got started long ago because people were stupid.

        // For average users of boost, we need to ensure that
        // the hash they are trying to boost actually exists. We
        // should not let them paste in hashes to boost; we should
        // make them select content to be boosted.

        // In my library, we read the string backwards by putting
        // an 0x at the front.
        digest256 Content {};

        // difficulty is a unit that is inversely proportional to
        // target. One difficulty is proportional to 2^32
        // expected hash operations.

        // a difficulty of 1/1000 should be easy to do on a cpu quickly.
        // Difficulty 1 is the difficulty of the genesis block.
        double Difficulty {0};

        // Tag/topic does not need to be anything.
        string Topic {};

        // additional data does not need to be anything but it
        // can be used to provide information about a boost or
        // to add a comment.
        string Data {};

        // script version can be 1 or 2.
        uint32 Version {2};

        // if provided, a contract script will be created.
        // Otherwise a bounty script will be created.
        maybe<digest160> MinerPubkeyHash {};

        maybe<uint32> UserNonce {};
        maybe<uint32> Category {};
    };

    struct redeeming_options {

        ptr<map_key_database> SigningKeys {};

        ptr<address_source> ReceivingAddresses {};

        uint32 Threads {1};

        // if not provided, look up fee rate from GorillaPool MAPI.
        maybe<double> FeeRate {};

        // Where to call the Boost API.
        // If not provided, use pow.co.
        maybe<string> APIHost {};
    };

    struct mining_options : redeeming_options {
        double MinProfitability {0};
        double MaxDifficulty {-1};
        int64 MinValue {300};
        bool Websockets {false};
        uint32 RefreshInterval {90};
    };

    // validate options and call the appropriate function.
    int run (const argh::parser &,
        int (*help) (),
        int (*version) (),
        int (*spend) (const script_options &),
        int (*redeem) (const Bitcoin::outpoint &, const bytes &script, int64 value, const redeeming_options &),
        int (*mine) (const mining_options &));
}

#endif
