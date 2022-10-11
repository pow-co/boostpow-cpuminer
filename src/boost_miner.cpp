#include <gigamonkey/boost/boost.hpp>
#include <gigamonkey/script/typed_data_bip_276.hpp>
#include <gigamonkey/p2p/var_int.hpp>
#include <ctime>
#include <logger.hpp>
#include <random.hpp>
#include <keys.hpp>
#include <pow_co_api.hpp>
#include <whatsonchain_api.hpp>

using namespace Gigamonkey;
using nlohmann::json;

const double maximum_mining_difficulty = 2.2;
const double minimum_price_per_difficulty_sats = 1000000;

Bitcoin::satoshi calculate_fee(
    size_t inputs_size, 
    size_t pay_script_size, 
    double fee_rate) {

    return inputs_size                              // inputs
        + 4                                         // tx version
        + 1                                         // var int value 1 (number of outputs)
        + 8                                         // satoshi value size
        + Bitcoin::var_int::size(pay_script_size)   // size of output script size
        + pay_script_size                           // output script size
        + 4;                                        // locktime

}

// A cpu miner function. 
work::proof cpu_solve(const work::puzzle& p, const work::solution& initial) {
    using uint256 = Gigamonkey::uint256;
    
    //if (initial.Share.ExtraNonce2.size() != 4) throw "Extra nonce 2 must have size 4. We will remove this limitation eventually.";
    
    uint64_big extra_nonce_2; 
    std::copy(initial.Share.ExtraNonce2.begin(), initial.Share.ExtraNonce2.end(), extra_nonce_2.begin());
    
    uint256 target = p.Candidate.Target.expand();
    if (target == 0) return {};
    //std::cout << " working " << p << std::endl;
    //std::cout << " with target " << target << std::endl;
    
    uint256 best{"0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
    
    N total_hashes{0};
    N nonce_increment{"0x0100000000"};
    uint32 display_increment = 0x00800000;
    
    work::proof pr{p, initial};
    
    while(true) {
        uint256 hash = pr.string().hash();
        total_hashes++;
        
        if (hash < best) {
            best = hash;

            logger::log("besthash", json {
              {"hash", best},
              {"total", uint64(total_hashes)}
            });

        } else if (pr.Solution.Share.Nonce % display_increment == 0) {
            pr.Solution.Share.Timestamp = Bitcoin::timestamp::now();
        }
        
        if (hash < target) {
            return pr;
        }
        
        pr.Solution.Share.Nonce++;
        
        if (pr.Solution.Share.Nonce == 0) {
            extra_nonce_2++;
            std::copy(extra_nonce_2.begin(), extra_nonce_2.end(), pr.Solution.Share.ExtraNonce2.begin());
        }
    }
    
    return pr;
}

json solution_to_json(work::solution x) {

    json share {
        {"timestamp", data::encoding::hex::write(x.Share.Timestamp.Value)},
        {"nonce", data::encoding::hex::write(x.Share.Nonce)},
        {"extra_nonce_2", data::encoding::hex::write(x.Share.ExtraNonce2) }
    };
    
    if (x.Share.Bits) share["bits"] = data::encoding::hex::write(*x.Share.Bits);
    
    return json {
        {"share", share}, 
        {"extra_nonce_1", data::encoding::hex::write(x.ExtraNonce1)}
    };
}

bytes mine(
    // an unredeemed Boost PoW output 
    Boost::puzzle puzzle, 
    // the address you want the bitcoins to go to once you have redeemed the boost output.
    // this is not the same as 'miner address'. This is just an address in your 
    // normal wallet and should not be the address that goes along with the key above.
    Bitcoin::address address) {
    using namespace Bitcoin;
    
    if (!puzzle.valid()) throw string{"Boost puzzle is not valid"};
    
    Bitcoin::satoshi value = puzzle.value();
    
    // is the difficulty too high?
    if (puzzle.difficulty() > maximum_mining_difficulty) 
      std::cout << "warning: difficulty may be too high for CPU mining." << std::endl;
    
    // is the value in the output high enough? 
    if (puzzle.profitability() < minimum_price_per_difficulty_sats)
      std::cout << "warning: price per difficulty may be too low." << std::endl;
    
    auto generator = data::get_random_engine();
    
    Stratum::session_id extra_nonce_1{random_uint32(generator)};
    uint64_big extra_nonce_2{random_uint64(generator)};
    
    work::solution initial{timestamp::now(), 0, bytes_view(extra_nonce_2), extra_nonce_1};
    
    if (puzzle.use_general_purpose_bits()) initial.Share.Bits = random_uint32(generator);
    
    work::proof proof = ::cpu_solve(work::puzzle(puzzle), initial);

    bytes pay_script = pay_to_address::script(address.Digest);

    double fee_rate = 0.5;

    Bitcoin::satoshi fee = calculate_fee(puzzle.expected_size(), pay_script.size(), fee_rate);

    if (fee > value) throw string{"Cannot pay tx fee with boost output"};
    
    bytes redeem_tx = puzzle.redeem(proof.Solution, {output{value - fee, pay_script}});
    
    bytes redeem_script = transaction{redeem_tx}.Inputs[0].Script;

    std::string redeemhex = data::encoding::hex::write(redeem_script);

    //std::cout << "job.complete.redeemscript " << redeemhexstring << std::endl;
    
    logger::log("job.complete.redeemscript", json {
      {"solution", solution_to_json(proof.Solution)},
      {"asm", Bitcoin::ASM(redeem_script)},
      {"hex", redeemhex},
      {"fee", fee}
    });

    // the transaction 
    return redeem_tx;
    
}

int command_spend(int arg_count, char** arg_values) {
    if (arg_count < 4 || arg_count > 5) throw "invalid number of arguments; should be 4 or 5";
    
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
    if (!target.valid()) throw (string{"could not read difficulty: "} + difficulty_input);
    
    //std::cout << "target: " << target << std::endl;
    
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
    uint32_little user_nonce{random_uint32(get_random_engine())};
    
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

    std::cout << "To spend to this job, paste into electrum-sv: \"" << 
        typed_data::write(typed_data::mainnet, output_script_bytes) << "\"" << std::endl;
    
    return 0;
}

int command_redeem(int arg_count, char** arg_values) {
    if (arg_count != 6) throw "invalid number of arguments; should be 6";
    
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
    
    bytes redeem_tx = mine(
        Boost::puzzle{{
            Boost::prevout{
                Bitcoin::outpoint{txid, index}, 
                Boost::output{Bitcoin::satoshi{value}, boost_script}}
        }, key}, address);
    
    std::string redeem_txhex = data::encoding::hex::write(redeem_tx);
    
    auto redeem_txid = Bitcoin::Hash256(redeem_tx);
    std::stringstream txid_stream;
    txid_stream << redeem_txid;
    
    logger::log("job.complete.transaction", json {
      {"txid", txid_stream.str()}, 
      {"txhex", redeem_txhex}
    });
    
    networking::HTTP http;
    whatsonchain whatsonchain_API{http};
    
    whatsonchain_API.transaction().broadcast(redeem_tx);
    
    return 0;
}

int command_mine(int arg_count, char** arg_values) {
    if (arg_count > 2 || arg_count < 1) throw "invalid number of arguments; should be 1 or 2";
    
    ptr<key_generator> signing_keys;
    ptr<address_generator> receiving_addresses;
    
    Bitcoin::secret key{string(arg_values[0])};
    hd::bip32::secret hd_key{string(arg_values[0])};
    
    if (key.valid()) signing_keys = 
        std::static_pointer_cast<key_generator>(std::make_shared<single_key_generator>(key));
    else if (hd_key.valid()) signing_keys = 
        std::static_pointer_cast<key_generator>(std::make_shared<hd_key_generator>(hd_key));
    else throw string{"could not read signing key"};
    
    if (arg_count == 1) {
        if (key.valid()) receiving_addresses = 
            std::static_pointer_cast<address_generator>(std::make_shared<single_address_generator>(key.address()));
        else if (hd_key.valid()) receiving_addresses = 
            std::static_pointer_cast<address_generator>(std::make_shared<hd_address_generator>(hd_key.to_public()));
        else throw string{"could not read signing key"};
    } else {
        Bitcoin::address address{string(arg_values[1])};
        hd::bip32::pubkey hd_pubkey{string(arg_values[1])};
        
        if (key.valid()) receiving_addresses = 
            std::static_pointer_cast<address_generator>(std::make_shared<single_address_generator>(address));
        else if (hd_key.valid()) receiving_addresses = 
            std::static_pointer_cast<address_generator>(std::make_shared<hd_address_generator>(hd_pubkey));
        else throw string{"could not read signing key"};
    }
    
    networking::HTTP http;
    pow_co pow_co_API{http};
    whatsonchain whatsonchain_API{http};
    
    list<Boost::prevout> jobs = pow_co_API.jobs();
    list<Boost::puzzle> puzzles;
    
    for (const auto &job : jobs) {
        auto script_hash = job.Value.ID;
        std::cout << "Checking script " << script_hash << " in " << job.outpoint() << std::endl;
        
        auto script_utxos = whatsonchain_API.script().get_unspent(script_hash);
        
        if (data::empty(script_utxos)) std::cout << "warning: no utxos found for script " << script_hash << std::endl;
        
        // is the current job in the list from whatsonchain? 
        bool match_found = false;
        
        for (auto const &utxo : script_utxos) if (utxo.Outpoint == job.outpoint()) {
            match_found = true;
            break;
        }
        
        if (!match_found) {
            std::cout << "warning: no utxos found with script " << job.outpoint();
            continue;
        }
        
        if (script_utxos.size() > 1) {
            std::cout << "warning: more than one output found with script " << job.outpoint();
            continue;
        }
        
        puzzles = puzzles << Boost::puzzle{{job}, signing_keys->next()};
        
    }
    
    for (const auto &puzzle : puzzles) 
        whatsonchain_API.transaction().broadcast(mine(puzzle, receiving_addresses->next()));
    
    return 0;
}

int help() {

    std::cout << "input should be \"function\" \"args\"... where function is "
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
        "\n\taddress    -- your address where you will put the redeemed sats." 
        "\nFor function \"mine\", remaining inputs should be "
        "\n\tkey        -- WIF or HD private key that will be used to redeem outputs."
        "\n\taddress    -- (optional) your address where you will put the redeemed sats."
        "\n\t              If not provided, addresses will be generated from the key. "<< std::endl;
    
    return 0;
}

int main(int arg_count, char** arg_values) {
	if(arg_count ==1) return help();
    //if (arg_count != 5) return help();
    
    string function{arg_values[1]};
    
    try {
        if (function == "spend") return command_spend(arg_count - 2, arg_values + 2);
        if (function == "redeem") return command_redeem(arg_count - 2, arg_values + 2);
        if (function == "mine") return command_mine(arg_count - 2, arg_values + 2);
        if (function == "help") return help();
        help();
    } catch (string x) {
        std::cout << "Error: " << x << std::endl;
    }
    
    return 1;
}

