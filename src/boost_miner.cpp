#include <boost/program_options.hpp>
#include <gigamonkey/script/typed_data_bip_276.hpp>
#include <gigamonkey/p2p/var_int.hpp>
#include <network.hpp>
#include <ctime>
#include <wallet.hpp>
#include <logger.hpp>
#include <random.hpp>
#include <keys.hpp>
#include <miner.hpp>

using namespace Gigamonkey;

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

        logger::log("job.create", json {
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

        logger::log("job.create", json {
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

int command_redeem(int arg_count, char** arg_values) {
    if (arg_count != 6) throw std::string{"invalid number of arguments; should be 6"};
    
    string arg_script{arg_values[0]};
    string arg_value{arg_values[1]};
    string arg_txid{arg_values[2]};
    string arg_index{arg_values[3]};
    string arg_wif{arg_values[4]};
    string arg_address{arg_values[5]};
    
    bytes *script;
    ptr<bytes> script_from_hex = encoding::hex::read(string{arg_script});
    typed_data script_from_bip_276 = typed_data::read(arg_script);
    
    if(script_from_bip_276.valid()) script = &script_from_bip_276.Data;
    else if (script_from_hex != nullptr) script = &*script_from_hex;
    else throw string{"could not read script"}; 
    
    int64 value;
    std::stringstream{arg_value} >> value;
    
    Bitcoin::txid txid{arg_txid};
    if (!txid.valid()) throw string{"could not read txid"};
    
    uint32 index;
    std::stringstream{arg_index} >> index;
    
    Bitcoin::address address{arg_address};
    if (!address.valid()) throw string{"could not read address"};
    
    Bitcoin::secret key{arg_wif};
    if (!key.valid()) throw string{"could not read secret key"};
    
    Boost::output_script boost_script{*script};
    if (!boost_script.valid()) throw string{"script is not valid"};

    logger::log("job.mine", json {
      {"script", arg_values[0]},
      {"value", arg_values[1]},
      {"txid", arg_values[2]},
      {"vout", arg_values[3]},
      {"miner", key.address().write()},
      {"recipient", address.write()}
    });
    
    BoostPOW::casual_random random;
    
    const double maximum_mining_difficulty = 2.2;
    const double minimum_price_per_difficulty_sats = 100000;
    
    Bitcoin::transaction redeem_tx = BoostPOW::mine(
        random, 
        Boost::puzzle{{
            Boost::prevout{
                Bitcoin::outpoint{txid, index}, 
                Boost::output{Bitcoin::satoshi{value}, boost_script}}
        }, key}, address, 86400, // one day. 
        minimum_price_per_difficulty_sats, maximum_mining_difficulty);
    
    bytes redeem_tx_raw = bytes(redeem_tx);
    std::string redeem_txhex = data::encoding::hex::write(redeem_tx_raw);
    
    auto redeem_txid = Bitcoin::Hash256(redeem_tx_raw);
    std::stringstream txid_stream;
    txid_stream << redeem_txid;
    
    logger::log("job.complete.transaction", json {
      {"txid", txid_stream.str()}, 
      {"txhex", redeem_txhex}
    });
    
    BoostPOW::network{}.broadcast(redeem_tx_raw);
    
    return 0;
}

int command_mine(int arg_count, char** arg_values) {
    namespace po = boost::program_options;
    
    string key_string;
    string address_string;
    uint32 count_threads;
    double min_profitability;
    double max_difficulty;
    
    po::options_description options_mine("options for command \"mine\"");
    
    options_mine.add_options()
    ("key", po::value<string>(&key_string), 
        "WIF or HD private key that will be used to redeem outputs.")
    ("address", po::value<string>(&address_string)->default_value(""), 
        "Your address where you will put the redeemed sats. "
        "If not provided, addresses will be generated from the key.")
    ("threads", po::value<uint32>(&count_threads)->default_value(1), 
        "Number of threads to mine with. Default is 1. (does not work yet)")
    ("min_profitability", po::value<double>(&min_profitability)->default_value(0), 
        "Boost jobs with less than this sats/difficulty will be ignored.")
    ("max_difficulty", po::value<double>(&max_difficulty)->default_value(-1), 
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
    }
    
    ptr<key_generator> signing_keys;
    ptr<address_generator> receiving_addresses;
    
    Bitcoin::secret key{key_string};
    hd::bip32::secret hd_key{key_string};
    
    if (key.valid()) signing_keys = 
        std::static_pointer_cast<key_generator>(std::make_shared<single_key_generator>(key));
    else if (hd_key.valid()) signing_keys = 
        std::static_pointer_cast<key_generator>(std::make_shared<hd_key_generator>(hd_key));
    else throw string{"could not read signing key"};
    
    if (address_string == "") {
        if (key.valid()) receiving_addresses = 
            std::static_pointer_cast<address_generator>(std::make_shared<single_address_generator>(key.address()));
        else if (hd_key.valid()) receiving_addresses = 
            std::static_pointer_cast<address_generator>(std::make_shared<hd_address_generator>(hd_key.to_public()));
        else throw string{"could not read signing key"};
    } else {
        Bitcoin::address address{address_string};
        hd::bip32::pubkey hd_pubkey{address_string};
        
        if (address.valid()) receiving_addresses = 
            std::static_pointer_cast<address_generator>(std::make_shared<single_address_generator>(address));
        else if (hd_pubkey.valid()) receiving_addresses = 
            std::static_pointer_cast<address_generator>(std::make_shared<hd_address_generator>(hd_pubkey));
        else throw string{"could not read signing key"};
    }
    
    BoostPOW::network net{};
    BoostPOW::casual_random rand;
    
    try {
        while(true) {
            BoostPOW::jobs Jobs{net.jobs(100)};
            auto count_jobs = Jobs.size();
            if (count_jobs == 0) return 0;
            
            std::cout << "About to start mining!" << std::endl;
            
            // remove jobs that are two difficult. 
            if (max_difficulty > 0) {
                for (auto it = Jobs.cbegin(); it != Jobs.cend();) 
                    if (it->second.difficulty() > max_difficulty) 
                        it = Jobs.erase(it);
                    else ++it;
                
                std::cout << (count_jobs - Jobs.size()) << " jobs removed due to high difficulty." << std::endl;
            }
            
            Bitcoin::transaction redeem_tx = 
                BoostPOW::mine(rand, 
                    BoostPOW::select(rand, Jobs, min_profitability).to_puzzle(signing_keys->next()), 
                    receiving_addresses->next(), 
                    // 15 minutes.
                    900, min_profitability, max_difficulty); 
            
            if (redeem_tx.valid()) net.broadcast(bytes(redeem_tx));
            else std::cout << "proceeding to call the API again" << std::endl;
            
        }
    } catch (const networking::HTTP::exception &exception) {
        std::cout << "API problem: " << exception.what() << std::endl;
    }
    
    return 0;
}

int help() {

    std::cout << "input should be <function> <args>... --<option>=<value>... where function is "
        "\n\t  spend      -- create a Boost output."
        "\n\t  redeem     -- mine and redeem an existing boost output."
        "\n\t  mine       -- call the pow.co API to get jobs to mine."
        "\nFor function \"spend\", remaining inputs should be "
        "\n\t  content    -- hex for correct order, hexidecimal for reversed."
        "\n\t  difficulty -- a positive number."
        "\n\t  topic      -- string max 20 bytes."
        "\n\t  add. data  -- string, any size."
        "\n\t  address    -- OPTIONAL. If provided, a boost contract output will be created. Otherwise it will be boost bounty."
        "\nFor function \"redeem\", remaining inputs should be "
        "\n\t  script     -- boost output script, hex or bip 276."
        "\n\t  value      -- value in satoshis of the output."
        "\n\t  txid       -- txid of the tx that contains this output."
        "\n\t  index      -- index of the output within that tx."
        "\n\t  wif        -- private key that will be used to redeem this output."
        "\n\t  address    -- your address where you will put the redeemed sats." 
        "\n\t  threads    -- (optional) number of threads to mine with. Default is 1." 
        "\nFor function \"mine\", remaining inputs should be "
        "\n\t  key        -- WIF or HD private key that will be used to redeem outputs."
        "\n\t  address    -- (optional) your address where you will put the redeemed sats."
        "\n\t              If not provided, addresses will be generated from the key. " 
        "\n\toptions for function \"mine\" are " 
        "\n\t  threads           -- Number of threads to mine with. Default is 1. (does not work yet)"
        "\n\t  min_profitability -- Boost jobs with less than this sats/difficulty will be ignored."
        "\n\t  max_difficulty    -- Boost jobs above this difficulty will be ignored." << std::endl;
    
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
        
    } catch (std::string x) {
        std::cout << "Error: " << x << std::endl;
        return 1;
    }
    
    return 0;
}

