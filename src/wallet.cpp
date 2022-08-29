#include <wallet.hpp>
#include <gigamonkey/fees.hpp>
#include <gigamonkey/incomplete.hpp>
#include <gigamonkey/script/machine.hpp>
#include <math.h>
#include <data/io/wait_for_enter.hpp>
#include <fstream>

std::ostream &write_json(std::ostream &o, const p2pkh_prevout &p) {
    return o << "{\"wif\": \"" << p.Key << "\", \"txid\": \"" << p.TXID.Value << "\", \"index\": " << p.Index << ", \"value\": " << int64(p.Value) << "}";
}

std::ostream &write_json(std::ostream &o, const wallet &w) {
    
    o << "{\"prevouts\": [";
    
    list<p2pkh_prevout> p = w.Prevouts;
    
    if (!data::empty(p)) while (true) {
        write_json(o, data::first(p));
        p = data::rest(p);
        if (data::empty(p)) break;
        o << ", ";
    }
    
    return o << "], \"master\": \"" << w.Master << "\", \"index\": " << w.Index << "}";
    
}

using nlohmann::json;

p2pkh_prevout read_prevout(const json &j) {
    
    return p2pkh_prevout {
        
        Gigamonkey::digest256{string(j["txid"])}, 
        data::uint32(j["index"]), 
        Bitcoin::satoshi(int64(j["value"])), 
        
        Gigamonkey::Bitcoin::secret{string(j["wif"])}
    };
    
}

wallet read_json(std::istream &i) {
    json j = json::parse(i);
    
    if (j.size() != 3 || !j.contains("prevouts") || !j.contains("master") || !j.contains("index")) throw std::string{"invalid wallet format"};
    
    auto pp = j["prevouts"];
    list<p2pkh_prevout> prevouts;
    for (const json p: pp) prevouts <<= read_prevout(p);
    
    return wallet{prevouts, hd::bip32::secret{string(j["master"])}, data::uint32(j["index"])};
}

Bitcoin::satoshi wallet::value() const {
    return data::fold([](Bitcoin::satoshi x, const p2pkh_prevout &p) -> Bitcoin::satoshi {
        return x + p.Value;
    }, Bitcoin::satoshi{0}, Prevouts);
}

wallet wallet::add(const p2pkh_prevout &p) const {
    return wallet{
        Prevouts << p, 
        Master, 
        Index
    };
}

p2pkh_prevout::operator Bitcoin::prevout() const {
    return Bitcoin::prevout{Bitcoin::outpoint{TXID, Index}, Bitcoin::output{Value, pay_to_address::script(Key.address().Digest)}};
}

uint64 redeem_script_size(bool compressed_pubkey) {
    return (compressed_pubkey ? 33 : 65) + 2 + Bitcoin::signature::MaxSignatureSize;
}

constexpr uint64 p2sh_script_size = 24;

constexpr int64 dust = 500;

wallet::spent wallet::spend(Bitcoin::output to, double satoshis_per_byte) const {
    wallet w = *this;
    
    // generate change script
    auto new_secret = Bitcoin::secret(w.Master.derive(w.Index));
    w.Index++;
    
    bytes change_script = Gigamonkey::pay_to_address::script(new_secret.address().Digest);
    
    // we create a transaction design without inputs and with an empty change output. 
    // we will figure out what these parameters are. 
    Gigamonkey::transaction_design tx{1, {}, {to, Bitcoin::output{0, change_script}}, 0};
    
    // the keys that we will use to sign the tx. 
    list<Bitcoin::secret> signing_keys{};
    
    Bitcoin::satoshi fee;
    
    // find sufficient funds 
    while (true) {
        // minimum fee required for this tx.
        fee = std::ceil(tx.expected_size() * satoshis_per_byte);
        
        // we have enough funds to cover the amount sent with fee without leaving a dust input. 
        if (tx.spent() - tx.sent() - fee > dust) break;
        
        if (data::empty(w.Prevouts)) throw std::string{"insufficient funds"};
        
        auto p = w.Prevouts.first();
        w.Prevouts = w.Prevouts.rest();
        
        tx.Inputs <<= transaction_design::input(Bitcoin::prevout(p), redeem_script_size(p.Key.Compressed));
        signing_keys <<= p.Key;
        
    }
    
    // randomize change index. 
    uint32 change_index = std::uniform_int_distribution<data::uint32>(0, 1)(get_random_engine());
    
    Bitcoin::satoshi change_value = tx.spent() - tx.sent() - fee;
    Bitcoin::output change{change_value, change_script};
    
    // replace outputs with a randomized list containing the correct change amount. 
    tx.Outputs = change_index == 0 ? list<Bitcoin::output>{change, to} : list<Bitcoin::output>{to, change};
    
    Bitcoin::incomplete::transaction incomplete(tx);
    
    list<Bitcoin::sighash::document> documents = tx.documents();
    
    Bitcoin::transaction complete = incomplete.complete(
        data::map_thread([](const Bitcoin::secret &k, const Bitcoin::sighash::document &doc) -> bytes {
            return pay_to_address::redeem(k.sign(doc), k.to_public());
        }, signing_keys, documents));
    
    uint32 index = 0;
    list<Bitcoin::result> results = data::map_thread(
        [&index, &incomplete](const transaction_design::input &inp, const Bitcoin::input &inb) -> Bitcoin::result {
            return Bitcoin::evaluate(inb.Script, inp.Prevout.script(), Bitcoin::redemption_document{inp.Prevout.value(), incomplete, index++});
        }, tx.Inputs, complete.Inputs);
    
    return spent{w.add(p2pkh_prevout{complete.id(), change_index, change_value, new_secret}), complete};
}

bool broadcast(const Bitcoin::transaction &t) {
    std::cout << "please broadcast this transaction: " << bytes(t) << std::endl;
    data::wait_for_enter();
    return true;
}

void write_to_file(const wallet &w, const std::string &filename) {
    std::fstream my_file;
    my_file.open(filename, std::ios::out);
    if (!my_file) throw std::string{"could not open file"};
    write_json(my_file, w);
    my_file.close();
}

wallet read_wallet_from_file(const std::string &filename) {
    std::fstream my_file;
    my_file.open(filename, std::ios::in);
    if (!my_file) throw std::string{"could not open file"};    
    return read_json(my_file);
}
