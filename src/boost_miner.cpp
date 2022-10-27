#include <ctime>
#include <logger.hpp>
#include <network.hpp>
#include <miner.hpp>
#include <boost/program_options.hpp>
#include <gigamonkey/script/typed_data_bip_276.hpp>
#include <gigamonkey/p2p/var_int.hpp>
#include <gigamonkey/schema/hd.hpp>
#include <gigamonkey/redeem.hpp>

using namespace Gigamonkey;
namespace po = boost::program_options;

Boost::output_script read_output_script(int arg_count, char** arg_values) {
    
    string content_hash_hex{arg_values[0]};
    
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
    if (!content.valid()) throw (string{"could not read content: "} + content_hash_hex);
    
    double diff = 0;
    string difficulty_input{arg_values[1]};
    std::stringstream diff_stream{difficulty_input};
    diff_stream >> diff;
    
    // difficulty is a unit that is inversely proportional to 
    // target. One difficulty is proportional to 2^32
    // expected hash operations. 
    
    // a difficulty of 1/1000 should be easy to do on a cpu quickly. 
    // Difficulty 1 is the difficulty of the genesis block. 
    work::compact target{work::difficulty{diff}};
    if (!target.valid()) throw (std::string{"could not read difficulty: "} + difficulty_input);
    
    // Tag/topic does not need to be anything. 
    string topic{arg_values[2]};
    if (topic.size() > 20) throw string{"topic is too big: must be 20 or fewer bytes"};
    
    // additional data does not need to be anything but it 
    // can be used to provide information about a boost or
    // to add a comment. 
    string additional_data{arg_values[3]};
    
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
    bool use_general_purpose_bits = false;
    
    Boost::output_script output_script;
    bytes output_script_bytes;
    
    // If you use a bounty script, other people can 
    // compete with you to mine a boost output if you 
    // broadcast it before you broadcast the solution. 
    
    // If you use a contract script, then you are the only
    // one who can mine that boost output. 

    if (arg_count == 4) {
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
            {"difficulty", diff},
            {"content", content_hash_hex},
            {"script", {
                {"asm", Bitcoin::ASM(output_script_bytes)},
                {"hex", output_script_bytes}
            }}
        });
    } else {
        Bitcoin::address miner_address{arg_values[4]};
        if (!miner_address.valid()) throw (std::string{"could not read miner address: "} + string{arg_values[4]});
        
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
            {"difficulty", diff},
            {"content", content_hash_hex},
            {"miner", arg_values[4]},
            {"script", {
                {"asm", Bitcoin::ASM(output_script_bytes)},
                {"hex", output_script_bytes}
            }}
        });
    }
    
    return output_script;
}

int command_spend(int arg_count, char** arg_values) {
    if (arg_count < 4 || arg_count > 5) throw std::string{"invalid number of arguments; should be 4 or 5"};
    auto output_script = read_output_script(arg_count, arg_values);

    std::cout << "To spend to this job, paste into electrum-sv: \"" << 
        typed_data::write(typed_data::mainnet, output_script.write()) << "\"" << std::endl;
    
    return 0;
}

struct redeemer final : BoostPOW::redeemer, BoostPOW::multithreaded {
    redeemer(
        uint32 threads, uint64 random_seed) : 
        BoostPOW::redeemer{}, 
        BoostPOW::multithreaded{threads, random_seed}, 
        Net{} {}
    
protected:
    BoostPOW::network Net;
    
    void redeemed(const Bitcoin::transaction &redeem_tx) override {
        if (!redeem_tx.valid()) std::cout << "invalid transaction!" << std::endl;
        if (!Net.broadcast(bytes(redeem_tx))) std::cout << "broadcast failed!" << std::endl;
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
    ("script", po::value<string>(&script_string), 
        "boost output script, hex or bip 276.")
    ("value", po::value<int64>(&value), 
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
    ("fee_rate", po::value<double>(&fee_rate)->default_value(.5), 
        "Boost jobs above this difficulty will be ignored");
    
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
    
    if (threads == 0) throw string{"Need at least 1 thread."};
    if (threads > 1) throw string{"Multiple threads not supported."};
    if (fee_rate < 0) throw string{"Fee rate must be positive"};
    
    bytes *script;
    ptr<bytes> script_from_hex = encoding::hex::read(script_string);
    typed_data script_from_bip_276 = typed_data::read(script_string);
    
    if(script_from_bip_276.valid()) script = &script_from_bip_276.Data;
    else if (script_from_hex != nullptr) script = &*script_from_hex;
    else throw string{"could not read script"}; 
    
    Bitcoin::txid txid{txid_string};
    if (!txid.valid()) throw string{"could not read txid"};
    
    Bitcoin::secret key{wif_string};
    if (!key.valid()) throw string{"could not read secret key"};
    
    Bitcoin::address address;
    if (address_string == "") address = key.address();
    else address = Bitcoin::address{address_string};
    if (!address.valid()) throw string{"could not read address"};
    
    Boost::output_script boost_script{*script};
    if (!boost_script.valid()) throw string{"script is not valid"};

    logger::log("job.mine", JSON {
      {"script", script_string},
      {"value", value},
      {"txid", txid_string},
      {"vout", index},
      {"miner", key.address().write()},
      {"recipient", address.write()}
    });
    
    BoostPOW::casual_random random;
    
    Bitcoin::transaction redeem_tx = BoostPOW::mine(
        random, 
        Boost::puzzle{
            Boost::candidate{
                boost_script, 
                {Boost::candidate::prevout{
                    Bitcoin::outpoint{txid, index}, 
                    Bitcoin::satoshi{value}}}}, 
            key}, 
        address, 
        86400, // one day. 
        min_profitability, 
        max_difficulty, 
        fee_rate);
    
    bytes redeem_tx_raw = bytes(redeem_tx);
    std::string redeem_txhex = data::encoding::hex::write(redeem_tx_raw);
    
    auto redeem_txid = Bitcoin::Hash256(redeem_tx_raw);
    std::stringstream txid_stream;
    txid_stream << redeem_txid;
    
    logger::log("job.complete.transaction", JSON {
      {"txid", txid_stream.str()}, 
      {"txhex", redeem_txhex}
    });
    
    BoostPOW::network{}.broadcast(redeem_tx_raw);
    
    return 0;
}

struct manager final : BoostPOW::manager, BoostPOW::multithreaded {
    manager(
        ptr<key_source> keys, 
        ptr<address_source> addresses, 
        uint32 threads, uint64 random_seed, 
        double maximum_difficulty, 
        double minimum_profitability, 
        double fee_rate) : 
        BoostPOW::manager{keys, addresses, BoostPOW::casual_random{random_seed}, maximum_difficulty, minimum_profitability, fee_rate}, 
        BoostPOW::multithreaded{threads, random_seed} {}
    
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
    ("fee_rate", po::value<double>(&fee_rate)->default_value(.5), 
        "Boost jobs above this difficulty will be ignored");
    
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
    if (fee_rate < 0) throw string{"Fee rate must be positive"};
    
    ptr<key_source> signing_keys;
    ptr<address_source> receiving_addresses;
    
    Bitcoin::secret key{key_string};
    hd::bip32::secret hd_key{key_string};
    
    if (key.valid()) signing_keys = 
        std::static_pointer_cast<key_source>(std::make_shared<single_key_source>(key));
    else if (hd_key.valid()) signing_keys = 
        std::static_pointer_cast<key_source>(std::make_shared<hd::key_source>(hd_key));
    else throw string{"could not read signing key"};
    
    if (address_string == "") {
        if (key.valid()) receiving_addresses = 
            std::static_pointer_cast<address_source>(std::make_shared<single_address_source>(key.address()));
        else if (hd_key.valid()) receiving_addresses = 
            std::static_pointer_cast<address_source>(std::make_shared<hd::address_source>(hd_key.to_public()));
        else throw string{"could not read signing key"};
    } else {
        Bitcoin::address address{address_string};
        hd::bip32::pubkey hd_pubkey{address_string};
        
        if (address.valid()) receiving_addresses = 
            std::static_pointer_cast<address_source>(std::make_shared<single_address_source>(address));
        else if (hd_pubkey.valid()) receiving_addresses = 
            std::static_pointer_cast<address_source>(std::make_shared<hd::address_source>(hd_pubkey));
        else throw string{"could not read signing key"};
    }
    
    manager{signing_keys, receiving_addresses, threads, 
        std::chrono::system_clock::now().time_since_epoch().count() * 5090567 + 337, 
        max_difficulty, min_profitability, fee_rate}.run();
    
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
        "\n\ttopic      -- string max 20 bytes."
        "\n\tadd. data  -- string, any size."
        "\n\taddress    -- OPTIONAL. If provided, a boost contract output will be created. Otherwise it will be boost bounty."
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
        "\n\tfee_rate          -- sats per byte of the final transaction." << std::endl;
    
    return 0;
}

int main(int arg_count, char** arg_values) {
    if(arg_count == 1) return help();
    
    string function{arg_values[1]};
    
    try {
        
        if (function == "spend") return command_spend(arg_count - 2, arg_values + 2);
        if (function == "redeem") return command_redeem(arg_count - 2, arg_values + 2);
        if (function == "mine") return command_mine(arg_count - 1, arg_values + 1);
        if (function == "help") return help();
        help();
        
    } catch (const std::string x) {
        std::cout << "Error: " << x << std::endl;
        return 1;
    } catch (const std::exception &x) {
        std::cout << "Strange error: " << x.what() << std::endl;
        return 1;
    }
    
    return 0;
}

