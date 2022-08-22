#include <wallet.hpp>

wallet::prevout::operator json() const {
    return json{
        {"wif", string(Key)}, 
        {"txid", string(TXID)}, 
        {"index", index}, 
        {"value", int64(value)}};
}
        
prevout wallet::prevout::read(const json&) {
    if (j.size() != 4 || !j.contains("wif") || !j.contains("txid") || !j.contains("index") || !j.contains("value")) throw "invalid prevout format";
    
    return prevout {
        
        Gigamonkey::digest256{string(j["txid"])}, 
        data::uint32(j["index"]), 
        Bitcoin::satoshi(int64(j["value"])), 
        
        Gigamonkey::Bitcoin::wif{j["wif"]};
    };
    
}

Bitcoin::satoshi wallet::value() const {
    data::fold([](Bitcoin::satoshi x, const prevout &p) -> Bitcoin::satoshi {
        return x + p.Value;
    }, Bitcoin::satoshi{0}, Prevouts);
}

uint64 redeem_script_size(bool compressed_pubkey) {
    return compressed_pubkey ? 33 : 65;
}

constexpr uint64 p2sh_script_size = 24;

constexpr int64 dust = 500;

wallet::spent wallet::spend(Bitcoin::output to, double satoshis_per_byte) {
    wallet w = *this;
    
    uint64 expected_size = 
        to.serialized_size() + // size of this output
        // size of change output.
        p2sh_script_size + Gigamonkey::Bitcoin::var_int::size(p2sh_script_size) + 8 + 
        10; // version, locktime, and number of outputs and inputs.
    
    int64 amount_spent = 0;
    int64 amount_sent = to.Value;
    
    // select prevouts 
    list<prevout> prevouts{};
    
    do {
        if (data::empty(w.Prevouts)) throw "insufficient funds";
        
        number_of_inputs = prevouts.size();
        
        amount_spent += w.Prevouts.first().Value;
        
        prevouts <<= w.Prevouts.first();
        w.Prevouts = w.Prevouts.rest();
        
        expected_size += redeem_script_size - 
            Gigamonkey::Bitcoin::var_int::size(number_of_inputs) + 
            Gigamonkey::Bitcoin::var_int::size(number_of_inputs + 1);
        
    } while (amount_sent + satoshis_per_byte * expected_size < amount_spent + dust);
    
    // generate change script
    auto xpriv = w.Master.derive(w.Index);
    bytes change_script = Gigamonkey::pay_to_address::script(xpriv.address())};
    w.Index++:
    
    // calculate fee 
    
    // make signatures 
    
    // create tx. 
    
    // update wallet with new prevout
    
    return spent{w, tx};
}

wallet wallet::add(const prevout &p) {
    return wallet{
        Prevouts << p, 
        Master, 
        Index
    };
}

wallet::operator json() const {
    json::array_t prevouts;
    
    for (const prevout &p : Prevouts) {prevouts.push(json(p))};
    
    return json{
        {"prevouts", prevouts}, 
        {"master", string(Master)}, 
        {"index", Index}};
}
    
static wallet wallet::read(const json &j) {
    if (j.size() != 3 || !j.contains("prevouts") || !j.contains("master") || !j.contains("index")) throw "invalid wallet format";
    
    auto pp = j["prevouts"];
    list<prevout> prevouts;
    for (const json p: pp) prevouts <<= prevout::read(p);
    
    return wallet{prevouts, hd::bip32::secret{string(j["master"])}, data::uint32(j["index"]);
}

bool broadcast(const Bitcoin::transaction &t) {
    std::cout << "Please broadcast this transaction: " << data::encoding::hex::write(t.write());
    std::cout << "How did it go? Y/N" << std::endl;
    
    char response;
    
    while (true) {
        std::cin >> response;
        if (response == 'y' || response == 'Y') return true;
        if (response == 'n' || response == 'N') return false;
    }
}

wallet read_from_file(const std::string &filename) {
    std::ifstream myfile{filename};
    std::string contents;
    
    if ( !myfile.is_open() ) throw std::string{"could not open file"} + filename;
    
    myfile >> contents;  
    
    return wallet::read(json::parse(mystring));
}

bool write_to_file(const wallet &w, const std::string &filename) {
    ofstream myfile {filename}; //open is the method of ofstream
    if ( !myfile.is_open() ) throw std::string{"could not open file"} + filename;
    o << json(w);
    o.close();
}
