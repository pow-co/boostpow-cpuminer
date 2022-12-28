#include <ctime>
#include <logger.hpp>
#include <network.hpp>
#include <miner.hpp>
#include <boost/program_options.hpp>
#include <gigamonkey/script/typed_data_bip_276.hpp>
#include <gigamonkey/p2p/var_int.hpp>
#include <gigamonkey/schema/hd.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>

using namespace Gigamonkey;
namespace po = boost::program_options;

int command_spend(int arg_count, char** arg_values) {
    
    string content_hash_hex;
    double difficulty_input;
    string additional_data;
    string topic;
    uint32 script_version;
    string miner_address_string;
    
    po::options_description options_spend("options for command \"spend\"");
    
    options_spend.add_options()
    ("content", po::value<string>(&content_hash_hex), 
        "content to boost. Hexidecimal (with 0x) for a txid, hex for any other hash.")
    ("difficulty", po::value<double>(&difficulty_input), 
        "difficulty to boost")
    ("data", po::value<string>(&additional_data)->default_value(string{}), 
        "additional data.")
    ("topic", po::value<string>(&topic)->default_value(string{}), 
        "topic. (max 20 characters)")
    ("script_version", po::value<uint32>(&script_version)->default_value(1), 
        "1 or 2")
    ("miner_address", po::value<string>(&miner_address_string)->default_value(string{}), 
        "if provided, script is boost contract. Otherwise it is boost bounty.");
    
    po::positional_options_description arguments_spend;
    arguments_spend.add("content", 1);
    arguments_spend.add("difficulty", 2);
    arguments_spend.add("topic", 3);
    arguments_spend.add("data", 4);
    arguments_spend.add("miner_address", 5);
    
    po::variables_map options_map;
    try {
        po::store(po::command_line_parser(arg_count, arg_values).
          options(options_spend).positional(arguments_spend).run(), options_map);
        po::notify(options_map);
    } catch (const po::error &ex) {
        std::cout << "Input format error: " << ex.what() << std::endl;
        std::cout << options_spend << std::endl;
        return 1;
    }
    
    if (script_version < 1 || script_version > 2) throw data::exception{"invalid script version"};
    
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
    digest256 content{content_hash_hex};
    if (!content.valid()) throw data::exception{} << "could not read content: " << content_hash_hex;
    
    // difficulty is a unit that is inversely proportional to 
    // target. One difficulty is proportional to 2^32
    // expected hash operations. 
    
    // a difficulty of 1/1000 should be easy to do on a cpu quickly. 
    // Difficulty 1 is the difficulty of the genesis block. 
    work::compact target{work::difficulty{difficulty_input}};
    if (!target.valid()) throw data::exception{} << "could not read difficulty " << difficulty_input;
    
    // Tag/topic does not need to be anything. 
    if (topic.size() > 20) throw data::exception{"topic is too big: must be 20 or fewer bytes"};
    
    // additional data does not need to be anything but it 
    // can be used to provide information about a boost or
    // to add a comment. 
    
    // Category has no particular meaning. We could use it for
    // something like magic number if we wanted to imitate 21e8. 
    int32_little category = 0;
    
    // User nonce is for ensuring that no two scripts are identical. 
    // You can increase the bounty for a boost by making an identical script. 
    uint32_little user_nonce{BoostPOW::casual_random{}.uint32()};
    
    // we are using version 1 for now. 
    // we will use version 2 when we know we have Stratum extensions right. 
    
    // This has to do with whether we use boost v2 which
    // incorporates bip320 which is necessary for ASICBoost. 
    // This is not necessary for CPU mining. 
    bool use_general_purpose_bits = script_version == 2;
    
    Boost::output_script output_script;
    bytes output_script_bytes;
    
    // If you use a bounty script, other people can 
    // compete with you to mine a boost output if you 
    // broadcast it before you broadcast the solution. 
    
    // If you use a contract script, then you are the only
    // one who can mine that boost output. 
    
    std::cout << "topic: " << topic << "; data: " << additional_data << "; miner address: " << miner_address_string << std::endl;

    if (miner_address_string == "") {
        output_script = Boost::output_script::bounty(
            category, 
            content, 
            target, 
            bytes::from_string(topic), 
            user_nonce, 
            bytes::from_string(additional_data), 
            use_general_purpose_bits);
        output_script_bytes = output_script.write();

        logger::log("job.create", JSON {
            {"target", target},
            {"difficulty", difficulty_input},
            {"content", content_hash_hex},
            {"script", {
                {"asm", Bitcoin::ASM(output_script_bytes)},
                {"hex", output_script_bytes}
            }}
        });
    } else {
        Bitcoin::address miner_address{miner_address_string};
        if (!miner_address.valid()) throw data::exception{} << "could not read miner address: " << miner_address_string;
        
        output_script = Boost::output_script::contract(
            category, 
            content, 
            target, 
            bytes::from_string(topic), 
            user_nonce, 
            bytes::from_string(additional_data), 
            miner_address.Digest, 
            use_general_purpose_bits);
        output_script_bytes = output_script.write();

        logger::log("job.create", JSON {
            {"target", target},
            {"difficulty", difficulty_input},
            {"content", content_hash_hex},
            {"miner", miner_address_string},
            {"script", {
                {"asm", Bitcoin::ASM(output_script_bytes)},
                {"hex", output_script_bytes}
            }}
        });
    }

    std::cout << "To spend to this job, paste into electrum-sv: \"" << 
        typed_data::write(typed_data::mainnet, output_script.write()) << "\"" << std::endl;
    
    return 0;
}

struct redeemer final : BoostPOW::redeemer, BoostPOW::multithreaded {
    BoostPOW::network &Net;
    BoostPOW::fees &Fees;
    Bitcoin::address Address;
    
    redeemer(
        BoostPOW::network &net, 
        BoostPOW::fees &fees, 
        const Bitcoin::address &address, 
        uint32 threads, uint64 random_seed) : 
        Net{net}, Fees{fees}, Address{address},
        BoostPOW::redeemer{}, BoostPOW::multithreaded{threads, random_seed} {
        this->start_threads();
    }
    
    void submit(const std::pair<digest256, Boost::puzzle> &puzzle, const work::solution &solution) final override {
        
        double fee_rate{Fees.get()};
        
        auto value = puzzle.second.value();
        bytes pay_script = pay_to_address::script(Address.Digest);
        auto expected_inputs_size = puzzle.second.expected_size();
        auto estimated_size = BoostPOW::estimate_size(expected_inputs_size, pay_script.size());
        std::cout << "expected inputs size = " << expected_inputs_size << "; pay script size = " << pay_script.size() << std::endl;
        std::cout << "total estimated size = " << estimated_size << std::endl;
        Bitcoin::satoshi fee{int64(ceil(fee_rate * estimated_size))};
        
        std::cout << "value: " << value << "; fee rate = " << fee_rate << " proposed fee " << fee << std::endl;
        
        if (fee > value) throw string{"Cannot pay tx fee with boost output"};
        
        auto redeem_tx = BoostPOW::redeem_puzzle(puzzle.second, solution, {Bitcoin::output{value - fee, pay_script}});
        
        std::cout << "tx size " << redeem_tx.serialized_size() << "; fee rate: " << 
            ((double(value) - double(redeem_tx.sent())) / double(redeem_tx.serialized_size())) << std::endl;
        for (const auto &in : redeem_tx.Inputs) std::cout << "\tinput size: " << in.serialized_size() << std::endl;
        for (const auto &out : redeem_tx.Outputs) std::cout << "\toutput size: " << out.serialized_size() << std::endl;
        
        logger::log("job.complete.transaction", JSON {
            {"txid", BoostPOW::write(redeem_tx.id())}, 
            {"txhex", encoding::hex::write(bytes(redeem_tx))}
        });
        
        if (!Net.broadcast(bytes(redeem_tx))) std::cout << "broadcast failed!" << std::endl;
        
        std::unique_lock<std::mutex> lock(BoostPOW::redeemer::Mutex);
        Solved = true;
        Last = std::pair<digest256, Boost::puzzle>{};
        Out.notify_one();
    }
};

int command_redeem(int arg_count, char** arg_values) {
    
    string script_string;
    int64  value;
    string txid_string;
    uint32 index;
    string wif_string;
    string address_string;
    uint32 threads;
    double min_profitability;
    double max_difficulty;
    double fee_rate;
    
    po::options_description options_redeem("options for command \"redeem\"");
    
    options_redeem.add_options()
    ("script", po::value<string>(&script_string)->default_value(""), 
        "boost output script, hex or bip 276.")
    ("value", po::value<int64>(&value)->default_value(-1), 
        "value in satoshis of the output.")
    ("txid", po::value<string>(&txid_string), 
        "txid of the tx that contains this output.")
    ("index", po::value<uint32>(&index), 
        "index of the output within that tx.")
    ("wif", po::value<string>(&wif_string), 
        "private key that will be used to redeem this output.")
    ("address", po::value<string>(&address_string)->default_value(""), 
        "Your address where you will put the redeemed sats. "
        "If not provided, addresses will be generated from the key.")
    ("threads", po::value<uint32>(&threads)->default_value(1), 
        "Number of threads to mine with. Default is 1.")
    ("min_profitability", po::value<double>(&min_profitability)->default_value(0), 
        "Boost jobs with less than this sats/difficulty will be ignored.")
    ("max_difficulty", po::value<double>(&max_difficulty)->default_value(-1), 
        "Boost jobs above this difficulty will be ignored")
    ("fee_rate", po::value<double>(&fee_rate)->default_value(-1), 
        "The final transaction will have this fee per byte.");
    
    po::positional_options_description arguments_redeem;
    arguments_redeem.add("script", 1);
    arguments_redeem.add("value", 2);
    arguments_redeem.add("txid", 3);
    arguments_redeem.add("index", 4);
    arguments_redeem.add("wif", 5);
    arguments_redeem.add("address", 6);
    
    po::variables_map options_map;
    try {
        po::store(po::command_line_parser(arg_count, arg_values).
          options(options_redeem).positional(arguments_redeem).run(), options_map);
        po::notify(options_map);
    } catch (const po::error &ex) {
        std::cout << "Input format error: " << ex.what() << std::endl;
        std::cout << options_redeem << std::endl;
        return 1;
    }
    
    BoostPOW::network Net{};
    
    Boost::output_script boost_script;
    if (script_string != "") {
        
        bytes *script;
        ptr<bytes> script_from_hex = encoding::hex::read(script_string);
        typed_data script_from_bip_276 = typed_data::read(script_string);
        
        if(script_from_bip_276.valid()) script = &script_from_bip_276.Data;
        else if (script_from_hex != nullptr) script = &*script_from_hex;
        else throw data::exception{"could not read script"}; 
        
        boost_script = Boost::output_script::read(*script);
    }
    
    Bitcoin::txid txid{txid_string};
    if (!txid.valid()) throw data::exception{} << "could not read txid " << txid_string;
    
    Bitcoin::secret key{wif_string};
    if (!key.valid()) throw string{"could not read secret key"};
    
    Bitcoin::address address;
    if (address_string == "") address = key.address();
    else address = Bitcoin::address{address_string};
    if (!address.valid()) throw data::exception{} << "could not read address" << address_string;
    
    Boost::candidate Job{};
    
    if (value < 0 || script_string == "") {
        Job = Net.job(Bitcoin::outpoint{txid, index});
        
        if (value < 0) value = Job.value();
        else if (value != Job.value()) throw data::exception {"User provided value is incorrect"};
        
        if (script_string == "") script_string = typed_data::write(typed_data::mainnet, Job.Script.write());
        else if (boost_script != Job.Script) throw data::exception {"User provided script is incorrect"};
        
    } else Job = Boost::candidate{
        {Boost::prevout{
            Bitcoin::outpoint {txid, index}, 
            Boost::output {
                Bitcoin::satoshi{value}, 
                boost_script}
        }}};
    
    if (!Job.valid()) throw data::exception{"script is not valid"};
    
    BoostPOW::fees *Fees = fee_rate < 0 ? 
        (BoostPOW::fees *)(new BoostPOW::network_fees(&Net)) : 
        (BoostPOW::fees *)(new BoostPOW::given_fees(fee_rate));
    
    logger::log("job.mine", JSON {
      {"script", script_string},
      {"difficulty", double(Job.difficulty())},
      {"value", value},
      {"txid", txid_string},
      {"vout", index},
      {"miner", key.address().write()},
      {"recipient", address.write()}
    });
    
    redeemer r{Net, *Fees, address, threads, std::chrono::system_clock::now().time_since_epoch().count() * 5090567 + 337};
    
    r.mine({Job.id(), Boost::puzzle{Job, key}});
    
    r.wait_for_solution();
    
    delete Fees;
    return 0;
}

struct manager : BoostPOW::manager {
    
    struct local_redeemer final : BoostPOW::manager::redeemer, BoostPOW::channel {
        std::thread Worker;
        local_redeemer(
            manager *m, 
            uint64 random_seed, uint32 index) : 
            manager::redeemer{m}, 
            BoostPOW::channel{}, 
            Worker{std::thread{BoostPOW::mining_thread, 
                &static_cast<work::selector &>(*this), 
                new BoostPOW::casual_random{random_seed}, index}} {}
    };
        
    manager(
        BoostPOW::network &net, 
        BoostPOW::fees &f,
        key_source &keys, 
        address_source &addresses, 
        uint64 random_seed, 
        double maximum_difficulty, 
        double minimum_profitability, int threads): 
        BoostPOW::manager{net, f, keys, addresses, random_seed, maximum_difficulty, minimum_profitability} {
        
        std::cout << "starting " << threads << " threads." << std::endl;
        for (int i = 1; i <= threads; i++) 
            this->add_new_miner(ptr<BoostPOW::manager::redeemer>{new local_redeemer(this, random_seed + i, i)});
    }
};

int command_mine(int arg_count, char** arg_values) {
    
    string key_string;
    string address_string;
    uint32 threads;
    double min_profitability;
    double max_difficulty;
    double fee_rate;
    
    po::options_description options_mine("options for command \"mine\"");
    
    options_mine.add_options()
    ("key", po::value<string>(&key_string), 
        "WIF or HD private key that will be used to redeem outputs.")
    ("address", po::value<string>(&address_string)->default_value(""), 
        "Your address where you will put the redeemed sats. "
        "If not provided, addresses will be generated from the key.")
    ("threads", po::value<uint32>(&threads)->default_value(1), 
        "Number of threads to mine with. Default is 1.")
    ("min_profitability", po::value<double>(&min_profitability)->default_value(0), 
        "Boost jobs with less than this sats/difficulty will be ignored.")
    ("max_difficulty", po::value<double>(&max_difficulty)->default_value(-1), 
        "Boost jobs above this difficulty will be ignored")
    ("fee_rate", po::value<double>(&fee_rate)->default_value(-1), 
        "fee rate to be used in the final transaction. If not provided, " 
        "we dial into Gorilla Pool via MAPI to get a fee quote.");
    
    po::positional_options_description arguments_mine;
    arguments_mine.add("key", 1);
    arguments_mine.add("address", 2);
    
    po::variables_map options_map;
    try {
        po::store(po::command_line_parser(arg_count, arg_values).
          options(options_mine).positional(arguments_mine).run(), options_map);
        po::notify(options_map);
    } catch (const po::error &ex) {
        std::cout << "Input format error: " << ex.what() << std::endl;
        std::cout << options_mine << std::endl;
        return 1;
    }
    
    if (threads == 0) throw string{"Need at least 1 thread."};
    
    ptr<key_source> signing_keys;
    ptr<address_source> receiving_addresses;
    
    Bitcoin::secret key{key_string};
    HD::BIP_32::secret hd_key{key_string};
    
    if (key.valid()) signing_keys = 
        std::static_pointer_cast<key_source>(std::make_shared<single_key_source>(key));
    else if (hd_key.valid()) signing_keys = 
        std::static_pointer_cast<key_source>(std::make_shared<HD::key_source>(hd_key));
    else throw string{"could not read signing key"};

    if (address_string == "") {
        if (key.valid()) receiving_addresses = 
            std::static_pointer_cast<address_source>(std::make_shared<single_address_source>(key.address()));
        else if (hd_key.valid()) receiving_addresses = 
            std::static_pointer_cast<address_source>(std::make_shared<HD::address_source>(hd_key.to_public()));
        else throw string{"could not read receiving address"};
    } else {
        Bitcoin::address address{address_string};
        HD::BIP_32::pubkey hd_pubkey{address_string};
        
        if (address.valid()) receiving_addresses = 
            std::static_pointer_cast<address_source>(std::make_shared<single_address_source>(address));
        else if (hd_pubkey.valid()) receiving_addresses = 
            std::static_pointer_cast<address_source>(std::make_shared<HD::address_source>(hd_pubkey));
        else throw string{"could not read receiving address"};
    }
    std::cout << "about to start running" << std::endl;
    BoostPOW::network Net{};
    
    BoostPOW::fees *Fees = fee_rate < 0 ? 
        (BoostPOW::fees *)(new BoostPOW::network_fees(&Net)) : 
        (BoostPOW::fees *)(new BoostPOW::given_fees(fee_rate));
    
    manager{Net, *Fees, *signing_keys, *receiving_addresses, 
        std::chrono::system_clock::now().time_since_epoch().count() * 5090567 + 337, 
        max_difficulty, min_profitability, threads}.run();
    
    delete Fees;
    return 0;
}

int help() {

    std::cout << "input should be <function> <args>... --<option>=<value>... where function is "
        "\n\tspend      -- create a Boost output."
        "\n\tredeem     -- mine and redeem an existing boost output."
        "\n\tmine       -- call the pow.co API to get jobs to mine."
        "\nFor function \"spend\", remaining inputs should be "
        "\n\tcontent    -- hex for correct order, hexidecimal for reversed."
        "\n\tdifficulty -- a positive number."
        "\n\ttopic      -- (optional) string max 20 bytes."
        "\n\tadd. data  -- (optional) string, any size."
        "\n\taddress    -- (optional) If provided, a boost contract output will be created. Otherwise it will be boost bounty."
        "\nFor function \"redeem\", remaining inputs should be "
        "\n\tscript     -- boost output script, hex or bip 276."
        "\n\tvalue      -- value in satoshis of the output."
        "\n\ttxid       -- txid of the tx that contains this output."
        "\n\tindex      -- index of the output within that tx."
        "\n\twif        -- private key that will be used to redeem this output."
        "\n\taddress    -- (optional) your address where you will put the redeemed sats." 
        "\n\t              If not provided, addresses will be generated from the key. " 
        "\nFor function \"mine\", remaining inputs should be "
        "\n\tkey        -- WIF or HD private key that will be used to redeem outputs."
        "\n\taddress    -- (optional) your address where you will put the redeemed sats."
        "\n\t              If not provided, addresses will be generated from the key. " 
        "\noptions for functions \"redeem\" and \"mine\" are " 
        "\n\tthreads           -- Number of threads to mine with. Default is 1."
        "\n\tmin_profitability -- Boost jobs with less than this sats/difficulty will be ignored."
        "\n\tmax_difficulty    -- Boost jobs above this difficulty will be ignored."
        "\n\tfee_rate          -- sats per byte of the final transaction."
        "\n\t                     If not provided we get a fee quote from Gorilla Pool."<< std::endl;
    
    return 0;
}

int main(int arg_count, char** arg_values) {
    if(arg_count == 1) return help();
    
    string function{arg_values[1]};
    
    try {
        
        if (function == "spend") return command_spend(arg_count - 1, arg_values + 1);
        if (function == "redeem") return command_redeem(arg_count - 1, arg_values + 1);
        if (function == "mine") return command_mine(arg_count - 1, arg_values + 1);
        if (function == "help") return help();
        help();
        
    } catch (const std::string x) {
        std::cout << "Error: " << x << std::endl;
        return 1;
    } catch (const data::exception &x) {
        std::cout << "Error: " << x.what() << std::endl;
        return 1;
    } catch (const std::exception &x) {
        std::cout << "Unexpected error: " << x.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "Unknown error " << std::endl;
        return 1;
    }
    
    return 0;
}

