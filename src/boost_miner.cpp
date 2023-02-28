#include <ctime>
#include <logger.hpp>
#include <network.hpp>
#include <miner.hpp>
#include <miner_options.hpp>
#include <gigamonkey/p2p/var_int.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>
#include <gigamonkey/script/typed_data_bip_276.hpp>
#include <gigamonkey/schema/hd.hpp>

using namespace Gigamonkey;

int spend (const BoostPOW::script_options &options) {

    work::compact target {work::difficulty {options.Difficulty}};
    if (!target.valid ()) throw data::exception {} << "could not read difficulty " << options.Difficulty;
    
    // Category has no particular meaning. We could use it for
    // something like magic number if we wanted to imitate 21e8. 
    int32_little category {options.Category ? *options.Category : 0};
    
    // User nonce is for ensuring that no two scripts are identical. 
    // You can increase the bounty for a boost by making an identical script. 
    uint32_little user_nonce {options.UserNonce ? *options.UserNonce : BoostPOW::casual_random {}.uint32 ()};
    
    // we are using version 1 for now. 
    // we will use version 2 when we know we have Stratum extensions right. 
    
    // This has to do with whether we use boost v2 which
    // incorporates bip320 which is necessary for ASICBoost. 
    // This is not necessary for CPU mining. 
    bool use_general_purpose_bits = options.Version == 2;
    
    Boost::output_script output_script;
    bytes output_script_bytes;
    
    // If you use a bounty script, other people can 
    // compete with you to mine a boost output if you 
    // broadcast it before you broadcast the solution. 
    
    // If you use a contract script, then you are the only
    // one who can mine that boost output. 
    
    std::cout << "topic: " << options.Topic << "; data: " << options.Data << std::endl;

    if (options.MinerPubkeyHash) {
        std::cout << "miner address: " << *options.MinerPubkeyHash << std::endl;

        output_script = Boost::output_script::contract (
            category, options.Content, target,
            bytes::from_string (options.Topic),
            user_nonce,
            bytes::from_string (options.Data),
            *options.MinerPubkeyHash,
            use_general_purpose_bits);

        output_script_bytes = output_script.write ();

        logger::log ("job.create", JSON {
            {"target", target},
            {"difficulty", options.Difficulty},
            {"content", BoostPOW::write (options.Content)},
            {"miner", Bitcoin::address (Bitcoin::address::main, *options.MinerPubkeyHash)},
            {"script", {
                {"asm", Bitcoin::ASM (output_script_bytes)},
                {"hex", encoding::hex::write (output_script_bytes)}
            }}
        });
    } else {
        output_script = Boost::output_script::bounty (
            category, options.Content, target,
            bytes::from_string (options.Topic),
            user_nonce,
            bytes::from_string (options.Data),
            use_general_purpose_bits);

        output_script_bytes = output_script.write ();

        logger::log ("job.create", JSON {
            {"target", target},
            {"difficulty", options.Difficulty},
            {"content", BoostPOW::write (options.Content)},
            {"script", {
                {"asm", Bitcoin::ASM (output_script_bytes)},
                {"hex", encoding::hex::write (output_script_bytes)}
            }}
        });
    }

    std::cout << "To spend to this job, paste into electrum-sv: \"" << 
        typed_data::write (typed_data::mainnet, output_script_bytes) << "\"" << std::endl;
    
    return 0;
}

struct redeemer final : BoostPOW::redeemer, BoostPOW::multithreaded {
    BoostPOW::network &Net;
    BoostPOW::fees &Fees;
    digest160 Address;
    
    redeemer (
        BoostPOW::network &net, 
        BoostPOW::fees &fees, 
        const digest160 &address,
        uint32 threads, uint64 random_seed) : 
        Net {net}, Fees {fees}, Address {address},
        BoostPOW::redeemer {}, BoostPOW::multithreaded {threads, random_seed} {
        this->start_threads ();
    }
    
    void submit (const std::pair<digest256, Boost::puzzle> &puzzle, const work::solution &solution) final override {
        
        double fee_rate {Fees.get ()};
        
        auto value = puzzle.second.value ();
        bytes pay_script = pay_to_address::script (Address);
        auto expected_inputs_size = puzzle.second.expected_size ();
        auto estimated_size = BoostPOW::estimate_size (expected_inputs_size, pay_script.size ());

        Bitcoin::satoshi fee {int64 (ceil (fee_rate * estimated_size))};
        
        if (fee > value) throw data::exception {"Cannot pay tx fee with boost output"};
        
        auto redeem_tx = BoostPOW::redeem_puzzle (puzzle.second, solution, {Bitcoin::output {value - fee, pay_script}});
        
        logger::log ("job.complete.transaction", JSON {
            {"txid", BoostPOW::write (redeem_tx.id ())},
            {"txhex", encoding::hex::write (bytes (redeem_tx))}
        });
        
        if (!Net.broadcast (bytes (redeem_tx))) std::cout << "broadcast failed!" << std::endl;
        
        std::unique_lock<std::mutex> lock (BoostPOW::redeemer::Mutex);
        Solved = true;
        Last = std::pair<digest256, Boost::puzzle> {};
        std::cout << "about to close channel" << std::endl;
        this->close ();

        Out.notify_one ();
    }
};

int redeem (const Bitcoin::outpoint &outpoint, const Boost::output_script &script, int64 value, const BoostPOW::mining_options &options) {

    BoostPOW::network Net = (options.APIHost) ?
        BoostPOW::network {*options.APIHost} :
        BoostPOW::network {};

    Boost::candidate Job {};

    Boost::output_script boost_script {};
    
    if (value <= 0 || !script.valid ()) {
        Job = Net.job (outpoint);
        
        if (value <= 0) value = Job.value ();
        else if (value != Job.value ()) throw data::exception {"User provided value is incorrect"};
        
        if (!script.valid ()) boost_script = Job.Script;
        else {
            if (script != Job.Script) throw data::exception {"User provided script is incorrect"};
            boost_script = script;
        }
        
    } else {
        boost_script = script;
        Job = Boost::candidate {
            {Boost::prevout {
                outpoint,
                Boost::output {
                    Bitcoin::satoshi {value},
                    script}
            }}};
    }
    
    if (!Job.valid ()) throw data::exception {"script is not valid"};
    
    BoostPOW::fees *Fees = bool (options.FeeRate) ?
        (BoostPOW::fees *) (new BoostPOW::given_fees (*options.FeeRate)) :
        (BoostPOW::fees *) (new BoostPOW::network_fees (Net));

    auto key = options.SigningKeys->next ();
    auto address = options.ReceivingAddresses->next ();
    
    logger::log ("job.mine", JSON {
      {"script", typed_data::write (typed_data::mainnet, boost_script.write ())},
      {"difficulty", double (Job.difficulty ())},
      {"value", value},
      {"outpoint", BoostPOW::write (outpoint)},
      {"miner", key.address ()},
      {"recipient", string (address)}
    });
    
    redeemer r {Net, *Fees, address.Digest, options.Threads,
        std::chrono::system_clock::now ().time_since_epoch ().count () * 5090567 + 337};
    
    r.mine ({Job.id (), Boost::puzzle {Job, key}});
    
    r.wait_for_shutdown ();
    std::cout << "shut down... deleting fees " << std::endl;
    delete Fees;
    std::cout << "fees deleted " << std::endl;
    return 0;
}

struct manager : BoostPOW::manager {
    
    struct local_redeemer final : BoostPOW::manager::redeemer, BoostPOW::channel {
        std::thread Worker;
        local_redeemer (
            manager *m, 
            uint64 random_seed, uint32 index) : 
            manager::redeemer {m},
            BoostPOW::channel {},
            Worker {std::thread {BoostPOW::mining_thread,
                &static_cast<work::selector &> (*this),
                new BoostPOW::casual_random {random_seed}, index}} {}
    };
        
    manager (
        BoostPOW::network &net, 
        BoostPOW::fees &f,
        const BoostPOW::map_key_database &keys,
        address_source &addresses,
        uint64 random_seed, 
        double maximum_difficulty, 
        double minimum_profitability, int threads): 
        BoostPOW::manager {net, f, keys, addresses, random_seed, maximum_difficulty, minimum_profitability} {
        
        std::cout << "starting " << threads << " threads." << std::endl;
        for (int i = 1; i <= threads; i++) 
            this->add_new_miner (ptr<BoostPOW::manager::redeemer> {new local_redeemer (this, random_seed + i, i)});
    }
};

int mine (double min_profitability, double max_difficulty, const BoostPOW::mining_options &options) {
    
    std::cout << "about to start running" << std::endl;

    BoostPOW::network Net = (options.APIHost) ?
        BoostPOW::network {*options.APIHost} :
        BoostPOW::network {};

    BoostPOW::fees *Fees = bool (options.FeeRate) ?
        (BoostPOW::fees *) (new BoostPOW::given_fees (*options.FeeRate)) :
        (BoostPOW::fees *) (new BoostPOW::network_fees (Net));
    
    std::make_shared<manager> (Net, *Fees, *options.SigningKeys, *options.ReceivingAddresses,
        std::chrono::system_clock::now ().time_since_epoch ().count () * 5090567 + 337,
        max_difficulty, min_profitability, options.Threads)->run ();
    
    delete Fees;
    return 0;
}

const char version_string[] = "BoostMiner 0.2.5";

int version () {
    std::cout << version_string << std::endl;
    return 0;
}

int help () {

    std::cout << "Welcome to " << version_string << "!" << std::endl;
    std::cout << "Input should be \n\t<method> <args>... --<option>=<value>... \nwhere method is "
        "\n\tspend      -- create a Boost script."
        "\n\tredeem     -- mine and redeem an existing boost output."
        "\n\tmine       -- call the pow.co API to get jobs to mine."
        "\nFor method \"spend\" provide the following as options or as arguments in order "
        "\n\tcontent    -- hex for correct order, hexidecimal for reversed."
        "\n\tdifficulty -- a positive number."
        "\n\ttopic      -- (optional) string max 20 bytes."
        "\n\tdata       -- (optional) string, any size."
        "\n\taddress    -- (optional) If provided, a boost contract output will be created. Otherwise it will be boost bounty."
        "\nFor method \"redeem\", provide the following as options or as arguments in order "
        "\n\ttxid       -- txid of the tx that contains this output."
        "\n\tindex      -- index of the output within that tx."
        "\n\twif        -- private key that will be used to redeem this output."
        "\n\tscript     -- (optional) boost output script, hex or bip 276."
        "\n\tvalue      -- (optional) value in satoshis of the output."
        "\n\taddress    -- (optional) your address where you will put the redeemed sats." 
        "\n\t              If not provided, addresses will be generated from the key. " 
        "\nFor method \"mine\", provide the following as options or as arguments in order"
        "\n\tkey        -- WIF or HD private key that will be used to redeem outputs."
        "\n\taddress    -- (optional) your address where you will put the redeemed sats."
        "\n\t              If not provided, addresses will be generated from the key. " 
        "\nadditional available options are "
        "\n\tapi_host          -- Host to call for Boost API. Default is pow.co"
        "\n\tthreads           -- Number of threads to mine with. Default is 1."
        "\n\tmin_profitability -- Boost jobs with less than this sats/difficulty will be ignored."
        "\n\tmax_difficulty    -- Boost jobs above this difficulty will be ignored."
        "\n\tfee_rate          -- Sats per byte of the final transaction."
        "\n\t                     If not provided we get a fee quote from Gorilla Pool." << std::endl;

    return 0;
}

int main (int arg_count, char** arg_values) {
    return BoostPOW::run (argh::parser (arg_count, arg_values), help, version, spend, redeem, mine);
}

