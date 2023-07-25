
#include <random.hpp>
#include <wallet.hpp>
#include <logger.hpp>
#include <miner.hpp>
#include <network.hpp>
#include <data/io/exception.hpp>

int command_generate (int arg_count, char** arg_values) {
    if (arg_count != 1) throw data::exception {"invalid number of arguments; one expected."};
    
    std::string filename {arg_values[0]};
    
    std::cout << "Type random characters and we will generate a wallet for you. Press enter when you think you have enough." << std::endl;
    
    std::string user_input {};
    
    while (true) {
        char x = std::cin.get ();
        if (x == '\n') break;
        user_input.push_back (x);
    } 
    
    digest512 bits = SHA2_512 (user_input);
    
    secp256k1::secret secret;
    
    HD::chain_code chain_code (32);
    
    std::copy (bits.begin (), bits.begin () + 32, secret.Value.begin ());
    std::copy (bits.begin () + 32, bits.end (), chain_code.begin ());
    
    HD::BIP_32::secret master {secret, chain_code, HD::BIP_32::main};
    
    write_to_file (wallet {{}, master, 0}, filename);
    return 0;
}

int command_receive (int arg_count, char** arg_values) {
    if (arg_count != 1) throw data::exception {"invalid number of arguments; one expected."};
    
    std::string filename{arg_values[0]};
    auto w = read_wallet_from_file (filename);
    
    Bitcoin::secret new_key = Bitcoin::secret (w.Master.derive (w.Index));
    
    w.Index += 1;
    
    std::cout << new_key << " " << new_key.address () << std::endl;
    
    write_to_file (w, filename);
    return 0;
}

int command_import(int arg_count, char** arg_values) {
    if (arg_count != 5) throw data::exception {"invalid number of arguments; five expected."};
    
    std::string filename {arg_values[0]};
    
    string arg_txid {arg_values[1]};
    string arg_index {arg_values[2]};
    string arg_value {arg_values[3]};
    string arg_wif {arg_values[4]};
    
    digest256 txid {arg_txid};
    
    uint32 index;
    std::stringstream {arg_index} >> index;
    
    int64 value;
    std::stringstream {arg_value} >> value;
    
    Bitcoin::secret key {arg_wif};
    if (!key.valid ()) throw data::exception {"could not read secret key"};
    
    auto w = read_wallet_from_file (filename);
    
    w = w.insert (p2pkh_prevout {txid, index, Bitcoin::satoshi {value}, key});
    
    write_to_file (w, filename);
    return 0;
}

int command_send (int arg_count, char** arg_values) {
    if (arg_count != 3) throw data::exception {"invalid number of arguments; three expected."};
    
    std::string filename {arg_values[0]};
    auto w = read_wallet_from_file (filename);
    
    write_to_file (w, filename);
    return 0;
}

int command_value (int arg_count, char** arg_values) {
    if (arg_count != 1) throw data::exception {"invalid number of arguments; one expected."};
    
    std::string filename{arg_values[0]};
    std::cout << read_wallet_from_file (filename).value () << std::endl;
    
    return 0;
}

int command_boost (int arg_count, char** arg_values) {
    
    if (arg_count < 4 || arg_count > 7) data::exception {"invalid number of arguments; should at least 4 and at most 7"};
    
    std::string filename {arg_values[0]};
    auto w = read_wallet_from_file (filename);
    
    string arg_value {arg_values[1]};
    
    int64 satoshi_value;
    std::stringstream {arg_value} >> satoshi_value;
    
    Bitcoin::satoshi value = satoshi_value;
    
    if (value > w.value ()) throw data::exception {"insufficient funds"};
    
    string arg_content {arg_values[2]};
    
    digest256 content {arg_content};
    if (!content.valid ()) throw data::exception {} << "could not read content: " << arg_content;
    
    double diff = 0;
    string difficulty_input {arg_values[3]};
    std::stringstream diff_stream {difficulty_input};
    diff_stream >> diff;
    
    work::compact target {work::difficulty {diff}};
    if (!target.valid ()) throw data::exception {} << "could not read difficulty: " << difficulty_input;
    
    bytes topic {};
    bytes additional_data {};
    
    if (arg_count > 4) {
    
        maybe<bytes> topic_read = encoding::hex::read (string {arg_values[4]});
        if (!bool (topic_read) || topic_read->size () > 20) throw data::exception {} << "could not read topic: " << arg_values[4];
        topic = *topic_read;
    }
    
    if (arg_count > 5) {
        maybe<bytes> add_data = encoding::hex::read (string {arg_values[5]});
        if (! bool (add_data)) throw data::exception {} << "could not read additional_data: " << arg_values[5];
        additional_data = *add_data;
    }
    
    Boost::type type = Boost::bounty;
    
    if (arg_count > 6) {
        string boost_type {arg_values[6]};
        if (boost_type == "contract") type = Boost::contract;
        else if (boost_type != "bounty") throw data::exception {} << "could not boost type: " << arg_values[6];
    }
    
    if (type == Boost::contract) throw data::exception {"We do not do boost contract yet"};
    
    Boost::output_script boost_output_script = Boost::output_script::bounty (
        0, content, target, topic, 
        BoostPOW::casual_random {}.uint32 (),
        additional_data, false);
    
    Bitcoin::output boost_output {value, boost_output_script.write ()};
    auto bitcoin_output = Bitcoin::output (boost_output);
    
    auto spend = w.spend (bitcoin_output);
    
    uint32 boost_output_index = 0;
    // where is the output script? 
    for (const Bitcoin::output &o : spend.Transaction.Outputs) {
        if (o == bitcoin_output) break;
        boost_output_index++;
    }
    
    if (boost_output_index == spend.Transaction.Outputs.size ()) throw data::exception {"could not find boost output index"};
    
    BoostPOW::network net {};
    
    net.broadcast (bytes (spend.Transaction));
    
    write_to_file (spend.Wallet, filename);
    /*
    w = spend.Wallet;
    
    Bitcoin::secret miner_key = Bitcoin::secret(w.Master.derive(w.Index));
    Bitcoin::secret spend_key = Bitcoin::secret(w.Master.derive(w.Index + 1));
    
    w.Index += 2;
    
    Bitcoin::transaction redeem_tx = mine(
        Boost::puzzle{{Boost::prevout{
            Bitcoin::outpoint{spend.Transaction.id(), boost_output_index}, 
            boost_output}}, miner_key}, spend_key.address());
    
    bytes redeem_tx_bytes = bytes(redeem_tx);
    
    net.broadcast(redeem_tx_bytes);
    
    w = w.add(p2pkh_prevout{
        Bitcoin::Hash256(redeem_tx_bytes), 0, 
        redeem_tx.Outputs[0].Value,
        spend_key});*/
    
    write_to_file (w, filename);
    return 0;
}

int command_restore (int arg_count, char** arg_values) {
    if (arg_count > 3) throw data::exception {"invalid number of arguments; two or three expected."};
    
    std::string filename {arg_values[0]};
    std::string master_human {arg_values[1]};
    
    HD::BIP_32::secret master {master_human};
    if (!master.valid ()) throw data::exception {"could not read HD private key"};
    
    uint32 max_look_ahead = 25;
    if (arg_count == 3) {
        std::string arg_max_look_ahead {arg_values[2]};
        std::stringstream {arg_max_look_ahead} >> max_look_ahead;
    }
    
    auto w = restore (master, max_look_ahead);
    std::cout << "wallet is " << w << std::endl;
    
    write_to_file (w, filename);
    
    return 0;
}

int help () {

    std::cout << "input should be \"function\" \"args\"... where function is "
        "\n\tgenerate   -- create a new wallet."
        "\n\tvalue      -- print the total value in the wallet."
        "\n\treceive    -- generate a new address."
        "\n\timport     -- add a utxo to this wallet."
        "\n\tsend       -- send to an address or script."
        "\n\tboost      -- boost content."
        "\n\tboost      -- restore a wallet."
        "\nFor function \"generate\", remaining inputs should be "
        "\n\tfilename   -- wallet file name. "
        "\nFor function \"value\", remaining inputs should be "
        "\n\tfilename   -- wallet file name. "
        "\nFor function \"address\", remaining inputs should be "
        "\n\tfilename   -- wallet file name. "
        "\nFor function \"import\", remaining inputs should be "
        "\n\tfilename   -- wallet file name. "
        "\n\ttxid       -- txid of the tx that contains the output to be added to the wallet."
        "\n\tindex      -- index of the output within that tx."
        "\n\tvalue      -- value in satoshis of the output."
        "\n\twif        -- private key that will be used to redeem this output."
        "\nFor function \"send\", remaining inputs should be "
        "\n\tfilename   -- wallet file name. "
        "\n\tvalue      -- value in satoshis to send."
        "\n\taddress    -- address or script (bip 276 encoded) to send them to." 
        "\nFor function \"boost\", remaining inputs should be "
        "\n\tfilename   -- wallet file name. "
        "\n\tvalue      -- value in satoshis to pay for boost."
        "\n\tcontent    -- hex for correct order, hexidecimal for reversed."
        "\n\tdifficulty -- "
        "\n\ttopic      -- OPTIONAL: string max 20 bytes."
        "\n\tadd. data  -- OPTIONAL: string, any size."
        "\n\ttype       -- OPTIONAL: may be either 'bounty' or 'contract'. Default is 'bounty'"
        "\nFor function \"restore\", remaining inputs should be "
        "\n\tfilename   -- wallet file name. "
        "\n\tmaster     -- HD master key" << std::endl;
    
    return 0;
}

int main (int arg_count, char** arg_values) {
    
    if (arg_count == 1) return help ();
    //if (arg_count != 5) return help();
    
    string function {arg_values[1]};
    
    try {
        
        if (function == "generate") return command_generate (arg_count - 2, arg_values + 2);
        if (function == "import") return command_import (arg_count - 2, arg_values + 2);
        if (function == "value") return command_value (arg_count - 2, arg_values + 2);
        if (function == "receive") return command_receive (arg_count - 2, arg_values + 2);
        if (function == "send") return command_send (arg_count - 2, arg_values + 2);
        if (function == "boost") return command_boost (arg_count - 2, arg_values + 2);
        if (function == "restore") return command_restore (arg_count - 2, arg_values + 2);
        if (function == "help") return help ();
        help ();
        
    } catch (data::exception x) {
        std::cout << "Error: " << x.what () << std::endl;
        return 1;
    }
    
    return 0;
}

