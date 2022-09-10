
#include <wallet.hpp>
#include <logger.hpp>
#include <random.hpp>

int command_generate(int arg_count, char** arg_values) {
    if (arg_count != 1) throw "invalid number of arguments; one expected.";
    
    std::string filename{arg_values[0]};
    
    std::cout << "Type random characters and we will generate a wallet for you. Press enter when you think you have enough." << std::endl;
    
    std::string user_input{};
    
    while(true) {
        char x = std::cin.get();
        if (x == '\n') break;
        user_input.push_back(x);
    } 
    
    digest512 bits = SHA2_512(user_input);
    
    secp256k1::secret secret;
    
    hd::chain_code chain_code(32);
    
    std::copy(bits.begin(), bits.begin() + 32, secret.Value.begin());
    std::copy(bits.begin() + 32, bits.end(), chain_code.begin());
    
    hd::bip32::secret master{secret, chain_code, hd::bip32::main};
    
    write_to_file(wallet{{}, master, 0}, filename);
    return 0;
}

int command_receive(int arg_count, char** arg_values) {
    if (arg_count != 1) throw "invalid number of arguments; one expected.";
    
    std::string filename{arg_values[0]};
    auto w = read_wallet_from_file(filename);
    
    Bitcoin::secret new_key = Bitcoin::secret(w.Master.derive(w.Index));
    
    w.Index += 1;
    
    std::cout << new_key << " " << new_key.address() << std::endl;
    
    write_to_file(w, filename);
    return 0;
}

int command_import(int arg_count, char** arg_values) {
    if (arg_count != 5) throw std::string{"invalid number of arguments; five expected."};
    
    std::string filename{arg_values[0]};
    
    string arg_txid{arg_values[1]};
    string arg_index{arg_values[2]};
    string arg_value{arg_values[3]};
    string arg_wif{arg_values[4]};
    
    digest256 txid{arg_txid};
    
    uint32 index;
    std::stringstream{arg_index} >> index;
    
    int64 value;
    std::stringstream{arg_value} >> value;
    
    Bitcoin::secret key{arg_wif};
    if (!key.valid()) throw "could not read secret key";
    
    auto w = read_wallet_from_file(filename);
    
    w = w.add(p2pkh_prevout{txid, index, Bitcoin::satoshi{value}, key});
    
    write_to_file(w, filename);
    return 0;
}

int command_send(int arg_count, char** arg_values) {
    if (arg_count != 3) throw std::string{"invalid number of arguments; three expected."};
    
    std::string filename{arg_values[0]};
    auto w = read_wallet_from_file(filename);
    
    write_to_file(w, filename);
    return 0;
}

int help() {

    std::cout << "input should be \"function\" \"args\"... where function is "
        "\n\tgenerate   -- create a new wallet."
        "\n\treceive    -- generate a new address."
        "\n\timport     -- add a utxo to this wallet."
        "\n\tsend       -- mine and redeem an existing boost output."
        "\nFor function \"generate\", remaining inputs should be "
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
        "\n\taddress    -- address to send them to." << std::endl;
    
    return 0;
}

int main(int arg_count, char** arg_values) {
    
    if(arg_count == 1) return help();
    //if (arg_count != 5) return help();
    
    string function{arg_values[1]};
    
    try {
        if (function == "generate") return command_generate(arg_count - 2, arg_values + 2);
        if (function == "import") return command_import(arg_count - 2, arg_values + 2);
        if (function == "receive") return command_receive(arg_count - 2, arg_values + 2);
        if (function == "send") return command_send(arg_count - 2, arg_values + 2);
        if (function == "help") return help();
        help();
    } catch (std::string x) {
        std::cout << "Error: " << x << std::endl;
        return 1;
    }
    
    return 0;
}

