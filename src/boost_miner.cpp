#include <gigamonkey/boost/boost.hpp>
#include <gigamonkey/ledger.hpp>
#include <ctime>
#include <wallet.hpp>
#include <logger.hpp>
#include <random.hpp>

using namespace Gigamonkey;

Bitcoin::satoshi calculate_fee(Bitcoin::prevout p, bytes pay_script, double fee_rate) {
    
    Boost::output_script output_script{p.script()};
    
    data::uint32 estimated_tx_size
        = 4                  // tx version
        + 1                  // var int value 1 (to say how many inputs there are)
        + 36                 // outpoint
                            // input script size with signature max size
        + Boost::input_script::expected_size(output_script.Type, output_script.UseGeneralPurposeBits)
        + 4                  // sequence number
        + 1                  // var int value 1 (number of outputs)
        + 8                  // satoshi value size
        + pay_script.size()  // output script size
        + 4;                 // locktime
    
    Bitcoin::satoshi spent = p.Value.Value;
    
    Bitcoin::satoshi fee = ceil(estimated_tx_size * fee_rate);
    
    if (fee > spent) throw std::string{"Cannot pay tx fee with boost output"};
    
    return fee;
}

// A cpu miner function. 
work::proof cpu_solve(const work::puzzle& p, const work::solution& initial) {
    using uint256 = Gigamonkey::uint256;
    
    uint64_big extra_nonce_2; 
    std::copy(initial.Share.ExtraNonce2.begin(), initial.Share.ExtraNonce2.end(), extra_nonce_2.begin());
    
    uint256 target = p.Candidate.Target.expand();
    if (target == 0) return {};
    
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
    ptr<bytes> topic = encoding::hex::read(string{arg_values[2]});
    if (topic == nullptr || topic->size() > 20) throw (std::string{"could not read topic: "} + string{arg_values[2]});
    
    // additional data does not need to be anything but it 
    // can be used to provide information about a boost or
    // to add a comment. 
    ptr<bytes> additional_data = encoding::hex::read(string{arg_values[3]});
    if (additional_data == nullptr) throw (std::string{"could not read additional_data: "} + string{arg_values[3]});
    
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
            *topic, 
            user_nonce, 
            *additional_data, 
            use_general_purpose_bits);
      logger::log("job.create", json {
          {"target", target},
          {"difficulty", diff},
          {"content", content_hash_hex},
          {"script", {
            {"asm",Bitcoin::ASM(output_script.write())},
            {"hex",output_script.write()}
          }}
      });
    } else {
        Bitcoin::address miner_address{arg_values[4]};
        if (!miner_address.valid()) throw (std::string{"could not read miner address: "} + string{arg_values[4]});
        
        output_script = Boost::output_script::contract(
            category, 
            content, 
            target, 
            *topic, 
            user_nonce, 
            *additional_data, 
            miner_address.Digest, 
            use_general_purpose_bits);

      logger::log("job.create", json {
          {"target", target},
          {"difficulty", diff},
          {"content", content_hash_hex},
          {"miner", arg_values[4]},
          {"script", {
            {"asm",Bitcoin::ASM(output_script.write())},
            {"hex",output_script.write()}
          }}
      });
    }
    
    return output_script;
}

int command_spend(int arg_count, char** arg_values) {
    if (arg_count < 4 || arg_count > 5) throw std::string{"invalid number of arguments; should be 4 or 5"};
    read_output_script(arg_count, arg_values);
    return 0;
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

Bitcoin::transaction mine(
    // an unredeemed Boost PoW output 
    const Bitcoin::prevout &prev, 
    // The private key that you will use to redeem the boost output. This key 
    // corresponds to 'miner address' in the Boost PoW protocol. 
    const Bitcoin::secret &private_key, 
    // the address you want the bitcoins to go to once you have redeemed the boost output.
    // this is not the same as 'miner address'. This is just an address in your 
    // normal wallet and should not be the address that goes along with the key above.
    const Bitcoin::address &address) {
    using namespace Bitcoin;
    
    // Is this a boost output? 
    Boost::output_script output_script{prev.script()}; 
    if (!output_script.valid()) throw std::string{"Not a valid Boost output script"};
    
    // If this is a contract script, we need to check that the key we have been given corresponds 
    // to the miner address in the script. 
    if (output_script.Type == Boost::contract && output_script.MinerAddress != private_key.address().Digest)
        throw "Incorrect key provided to mine this output.";
        
    // is the difficulty too high?
    if (output_script.Target.difficulty() > 1.01) {
      //std::cout << "warning: difficulty may be too high for CPU mining." << std::endl;
    }
    
    // is the value in the output high enough? 
    Boost::puzzle boost_puzzle{output_script, private_key};
    
    auto generator = data::get_random_engine();
    
    Stratum::session_id extra_nonce_1{random_uint32(generator)};
    uint64_big extra_nonce_2{random_uint64(generator)};
    
    work::solution initial{timestamp::now(), 0, bytes_view(extra_nonce_2), extra_nonce_1};
    
    if (output_script.UseGeneralPurposeBits) initial.Share.Bits = random_uint32(generator);
    
    work::proof proof = ::cpu_solve(work::puzzle(boost_puzzle), initial);

    bytes pay_script = pay_to_address::script(address.Digest);
    
    Bitcoin::satoshi fee { calculate_fee(prev, pay_script, wallet::default_fee_rate) };
    
    // the incomplete transaction 
    incomplete::transaction incomplete{ 
        {incomplete::input{prev.outpoint()}}, // one incomplete input 
        {output{prev.value() - fee, pay_script}}}; // one output 
    
    // signature
    Bitcoin::signature signature = private_key.sign( 
        sighash::document{
            prev.value(),   // output being redeemed
            prev.script(),
            incomplete,              // the incomplete tx
            0});                     // index of input that will contain this signature
    
    Boost::input_script input_script = Boost::input_script(
            signature, private_key.to_public(), proof.Solution, output_script.Type, output_script.UseGeneralPurposeBits);
    
    data::bytes redeemhex = input_script.write();
    
    std::string redeemhexstring = data::encoding::hex::write(redeemhex);
    
    logger::log("job.complete.redeemscript", json {
      {"solution", solution_to_json(proof.Solution)},
      {"asm", ASM(input_script.write())},
      {"hex", redeemhexstring },
      {"fee", fee}
    });
    
    // the transaction 
    return incomplete.complete({input_script.write()});
    
}

int command_redeem(int arg_count, char** arg_values) {
    if (arg_count != 6) throw std::string{"invalid number of arguments; should be 6"};
    
    string arg_script{arg_values[0]};
    string arg_value{arg_values[1]};
    string arg_txid{arg_values[2]};
    string arg_index{arg_values[3]};
    string arg_wif{arg_values[4]};
    string arg_address{arg_values[5]};
    
    ptr<bytes> script = encoding::hex::read(arg_script);
    if (script == nullptr) throw std::string{"could not read script"}; 
    
    int64 value;
    std::stringstream{arg_value} >> value;
    
    Bitcoin::txid txid{arg_txid};
    if (!txid.valid()) throw std::string{"could not read txid"};
    
    uint32 index;
    std::stringstream{arg_index} >> index;
    
    Bitcoin::address address{arg_address};
    if (!address.valid()) throw std::string{"could not read address"};
    
    Bitcoin::secret key{arg_wif};
    if (!key.valid()) throw std::string{"could not read secret key"};

    logger::log("job.mine", json {
      {"script", arg_values[0]},
      {"value", arg_values[1]},
      {"txid", arg_values[2]},
      {"vout", arg_values[3]},
      {"miner", key.address().write()},
      {"recipient", address.write()}
    });
    
    Bitcoin::transaction tx = mine(
        Bitcoin::prevout{
            Bitcoin::outpoint{txid, index}, 
            Bitcoin::output{Bitcoin::satoshi{value}, *script}}, 
        key, address);
    
    std::string txhex = data::encoding::hex::write(bytes(tx));

    logger::log("job.complete.transaction", json {
      {"txhex", txhex}
    });
    
    return 0;
}

int command_boost(int arg_count, char** arg_values) {
    
    if (arg_count < 4 || arg_count > 7) throw std::string{"invalid number of arguments; should at least 4 and at most 7"};
    
    std::string filename{arg_values[0]};
    auto w = read_wallet_from_file(filename);
    
    string arg_value{arg_values[1]};
    
    int64 satoshi_value;
    std::stringstream{arg_value} >> satoshi_value;
    
    Bitcoin::satoshi value = satoshi_value;
    
    if (value > w.value()) throw "insufficient funds";
    
    string arg_content{arg_values[2]};
    
    digest256 content{arg_content};
    if (!content.valid()) throw (string{"could not read content: "} + arg_content);
    
    double diff = 0;
    string difficulty_input{arg_values[3]};
    std::stringstream diff_stream{difficulty_input};
    diff_stream >> diff;
    
    work::compact target{work::difficulty{diff}};
    if (!target.valid()) throw (std::string{"could not read difficulty: "} + difficulty_input);
    
    bytes topic{};
    bytes additional_data{};
    
    if (arg_count > 4) {
    
        ptr<bytes> topic_read = encoding::hex::read(string{arg_values[4]});
        if (topic_read == nullptr || topic_read->size() > 20) throw (std::string{"could not read topic: "} + string{arg_values[4]});
        topic = *topic_read;
    }
    
    if (arg_count > 5) {
        ptr<bytes> add_data = encoding::hex::read(string{arg_values[5]});
        if (add_data == nullptr) throw (std::string{"could not read additional_data: "} + string{arg_values[5]});
        additional_data = *add_data;
    }
    
    Boost::type type = Boost::bounty;
    
    if (arg_count > 6) {
        string boost_type{arg_values[6]};
        if (boost_type == "contract") type = Boost::contract;
        else if (boost_type != "bounty") throw (std::string{"could not boost type: "} + string{arg_values[6]});
        
    }
    
    if (type == Boost::contract) throw std::string{"We do not do boost contract yet"};
    
    Boost::output_script boost_output_script = Boost::output_script::bounty(
        0, content, target, topic, 
        random_uint32(get_random_engine()), 
        additional_data, false);
    
    Bitcoin::output bitcoin_output{value, boost_output_script.write()};
    
    auto spend = w.spend(bitcoin_output);
    
    uint32 boost_output_index = 0;
    // where is the output script? 
    for (const Bitcoin::output &o : spend.Transaction.Outputs) {
        if (o == bitcoin_output) break;
        boost_output_index++;
    }
    
    if (boost_output_index == spend.Transaction.Outputs.size()) throw "could not find boost output index";
    
    broadcast(spend.Transaction);
    
    write_to_file(spend.Wallet, filename);
    
    w = spend.Wallet;
    
    Bitcoin::secret miner_key = Bitcoin::secret(w.Master.derive(w.Index));
    Bitcoin::secret spend_key = Bitcoin::secret(w.Master.derive(w.Index + 1));
    
    w.Index += 2;
    
    Bitcoin::transaction redeem = mine(
        Bitcoin::prevout{
            Bitcoin::outpoint{spend.Transaction.id(), boost_output_index}, 
            bitcoin_output}, miner_key, spend_key.address());
    
    broadcast(redeem);
    
    w = w.add(p2pkh_prevout{
        redeem.id(), 0, 
        redeem.Outputs[0].Value,
        spend_key});
    
    write_to_file(w, filename);
    
    return 0;
}

int help() {

    std::cout << "input should be \"function\" \"args\"... where function is "
        "\n\tspend      -- create a Boost output."
        "\n\tredeem     -- mine and redeem an existing boost output."
        "\nFor function \"spend\", remaining inputs should be "
        "\n\tcontent    -- hex for correct order, hexidecimal for reversed."
        "\n\tdifficulty -- "
        "\n\ttopic      -- string max 20 bytes."
        "\n\tadd. data  -- string, any size."
        "\n\taddress    -- OPTIONAL. If provided, a boost contract output will be created. Otherwise it will be boost bounty."
        "\nFor function \"redeem\", remaining inputs should be "
        "\n\tscript     -- boost output script."
        "\n\tvalue      -- value in satoshis of the output."
        "\n\ttxid       -- txid of the tx that contains this output."
        "\n\tindex      -- index of the output within that tx."
        "\n\twif        -- private key that will be used to redeem this output."
        "\n\taddress    -- your address where you will put the redeemed sats."
        "\nFor function \"boost\", remaining inputs should be "
        "\n\tfilename   -- wallet file name. "
        "\n\tvalue      -- value in satoshis to pay for boost."
        "\n\tcontent    -- hex for correct order, hexidecimal for reversed."
        "\n\tdifficulty -- "
        "\n\ttopic      -- OPTIONAL: string max 20 bytes."
        "\n\tadd. data  -- OPTIONAL: string, any size."
        "\n\ttype       -- OPTIONAL: may be either 'bounty' or 'contract'. Default is 'bounty'" << std::endl;
    
    return 0;
}

int main(int arg_count, char** arg_values) {
	if(arg_count == 1) return help();
    //if (arg_count != 5) return help();
    
    string function{arg_values[1]};
    
    try {
        if (function == "spend") return command_spend(arg_count - 2, arg_values + 2);
        if (function == "redeem") return command_redeem(arg_count - 2, arg_values + 2);
        if (function == "boost") return command_boost(arg_count - 2, arg_values + 2);
        if (function == "help") return help();
        help();
    } catch (std::string x) {
        std::cout << "Error: " << x << std::endl;
        return 1;
    }
    
    return 0;
}

