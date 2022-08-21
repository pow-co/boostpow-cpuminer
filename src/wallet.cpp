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

wallet::spent wallet::spend(Bitcoin::output to, satoshis_per_byte) {
    throw "unimplemented";
}

wallet wallet::add(const prevout &) {
    throw "unimplemented";
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
