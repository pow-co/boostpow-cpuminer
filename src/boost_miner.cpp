
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
    if (arg_count < 4 || arg_count > 5) throw "invalid number of arguments; should be 4 or 5";
    
    std::cout << "To spend to this job, paste into electrum-sv: \"" << 
        typed_data::write(typed_data::mainnet, read_output_script(arg_count, arg_values).write()) << "\"" << std::endl;
    
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
    
    Bitcoin::transaction redeem_tx = miner{}.mine(
        Boost::puzzle{{
            Boost::prevout{
                Bitcoin::outpoint{txid, index}, 
                Boost::output{Bitcoin::satoshi{value}, boost_script}}
        }, key}, address);
    
    bytes redeem_tx_raw = bytes(redeem_tx);
    std::string redeem_txhex = data::encoding::hex::write(redeem_tx_raw);
    
    auto redeem_txid = Bitcoin::Hash256(redeem_tx_raw);
    std::stringstream txid_stream;
    txid_stream << redeem_txid;
    
    logger::log("job.complete.transaction", json {
      {"txid", txid_stream.str()}, 
      {"txhex", redeem_txhex}
    });
    
    network{}.broadcast(redeem_tx_raw);
    
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
        
        if (address.valid()) receiving_addresses = 
            std::static_pointer_cast<address_generator>(std::make_shared<single_address_generator>(address));
        else if (hd_pubkey.valid()) receiving_addresses = 
            std::static_pointer_cast<address_generator>(std::make_shared<hd_address_generator>(hd_pubkey));
        else throw string{"could not read signing key"};
    }
    
    network net{};
    miner Miner{};
    
    try {
        while(true) {
            list<Boost::prevout> jobs = net.PowCo.jobs();
            std::cout << "read " << jobs.size() << " jobs from pow.co/api/v1/jobs/" << std::endl;
            
            set<digest256> script_hashes;
            map<digest256, boost_prevouts> prevouts;
            uint32 jobs_with_duplicates = 0;
            
            for (const auto &job : jobs) {
                digest256 script_hash = job.id();
                
                if (script_hashes.contains(script_hash)) {
                    std::cout << " Script " << script_hash << " has already been detected." << std::endl;
                    continue;
                }
                
                script_hashes = script_hashes.insert(script_hash);
                
                std::cout << " Checking script with hash " << script_hash << " in \n\t" << 
                    job.outpoint() << " on whatsonchain.com" << std::endl;
                
                auto script_utxos = net.WhatsOnChain.script().get_unspent(script_hash);
                
                // is the current job in the list from whatsonchain? 
                bool match_found = false;
                
                std::cout << " whatsonchain.com found " << script_utxos.size() << " utxos for script " << script_hash << std::endl;
                
                if (script_utxos.size() > 1) jobs_with_duplicates++;
                
                for (auto const &utxo : script_utxos) {
                    if (utxo.Outpoint == job.outpoint()) {
                        match_found = true;
                        break;
                    }
                }
                
                if (!match_found) {
                    std::cout << " warning: " << job.outpoint() << " not found in whatsonchain utxos" << std::endl;
                
                    std::cout << " checking script redeption against pow.co/api/v1/spends/ ...";
                    
                    auto inpoint = net.PowCo.spends(job.outpoint());
                    if (!inpoint.valid()) {
                        std::cout << " No redemption found at pow.co." << std::endl;
                        std::cout << " checking whatsonchain history." << std::endl;
                        auto script_history = net.WhatsOnChain.script().get_history(script_hash);
                        
                        std::cout << " " << script_history.size() << " transactions returned; " << std::endl;
                        
                        for (const Bitcoin::txid &history_txid : script_history) {
                            Bitcoin::transaction history_tx{net.WhatsOnChain.transaction().get_raw(history_txid)};
                            if (!history_tx.valid()) std::cout << "  could not find tx " << history_txid << std::endl;
                            
                            for (const Bitcoin::input &in: history_tx.Inputs) if (in.Reference == job.outpoint()) {
                                std::cout << "  Redemption found on whatsonchain.com at " << history_txid << std::endl;
                                net.PowCo.submit_proof(history_txid);
                                goto redemption_found;
                            }
                            
                        } 
                        
                        std::cout << " no redemption found on whatsonchain.com " << std::endl;
                        
                    } else {
                        std::cout << " Redemption found" << std::endl;
                        net.PowCo.submit_proof(inpoint.Digest);
                    }
                }
                
                redemption_found:
                
                if (script_utxos.size() == 0) continue;
                
                prevouts = prevouts.insert(script_hash, boost_prevouts{job.script(), script_utxos});
                
                std::cout << " jobs found so far: " << prevouts.keys() << std::endl;
                
            }
            
            std::cout << "found " << jobs.size() << " jobs that are not redeemed already" << std::endl;
            std::cout << "found " << jobs_with_duplicates << " boost scripts that appear in multiple outputs." << std::endl;
            
            std::cout << "About to start mining!" << std::endl;
            
            Bitcoin::transaction redeem_tx = 
                Miner.mine(prevouts_to_puzzle(Miner.select(prevouts), signing_keys->next()), receiving_addresses->next(), 900);
            if (!redeem_tx.valid()) {
                std::cout << "proceeding to call the API again" << std::endl;
                continue;
            }
            
            net.broadcast(bytes(redeem_tx));
            
        }
    } catch (const networking::HTTP::exception &exception) {
        std::cout << "API problem: " << exception.what() << std::endl;
    }
    
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
        "\n\t              If not provided, addresses will be generated from the key. " << std::endl;
    
    return 0;
}

int main(int arg_count, char** arg_values) {
    if(arg_count == 1) return help();
    //if (arg_count != 5) return help();
    
    string function{arg_values[1]};
    
    try {
        if (function == "spend") return command_spend(arg_count - 2, arg_values + 2);
        if (function == "redeem") return command_redeem(arg_count - 2, arg_values + 2);
        if (function == "mine") return command_mine(arg_count - 2, arg_values + 2);
        if (function == "help") return help();
        help();
    } catch (std::string x) {
        std::cout << "Error: " << x << std::endl;
        return 1;
    }
    
    return 0;
}

