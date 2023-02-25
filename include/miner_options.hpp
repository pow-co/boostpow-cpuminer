
#ifndef BOOSTPOW_CPUMINER_PROGRAM_OPTIONS
#define BOOSTPOW_CPUMINER_PROGRAM_OPTIONS

#include <string>
#include <optional>
#include <argh.h>
#include <gigamonkey/boost/boost.hpp>
#include <gigamonkey/schema/keysource.hpp>

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
        std::optional<Bitcoin::address> MinerAddress {};

        std::optional<uint32> UserNonce {};
        std::optional<uint32> Category {};
    };

    struct mining_options {

        ptr<key_source> SigningKeys {};

        ptr<address_source> ReceivingAddresses {};

        uint32 Threads {1};

        // if not provided, look up fee rate from GorillaPool MAPI.
        std::optional<double> FeeRate {};

        // Where to call the Boost API.
        // If not provided, use pow.co.
        std::optional<string> APIHost {};
    };

    // validate options and call the appropriate function.
    int run (const argh::parser &,
        int (*help) (),
        int (*version) (),
        int (*spend) (const script_options &),
        int (*redeem) (const Bitcoin::outpoint &, const Boost::output_script &, int64, const mining_options &),
        int (*mine) (double min_profitability, double max_difficulty, const mining_options &));
}

#endif
