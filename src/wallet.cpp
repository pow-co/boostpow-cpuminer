#include <wallet.hpp>
#include <gigamonkey/fees.hpp>

std::ostream &write_json(std::ostream &o, const p2pkh_prevout &p) {
    return o << "{\"wif\": \"" << Key << "\", \"txid\": \"" << TXID << "\", \"index\": " << index << ", \"value\": " << int64(value) << "}";
}

std::ostream &write_json(std::ostream &o, const wallet &p) {
    
    o << "{\"prevouts\": [";
    
    list<p2pkh_prevout> p = Prevouts;
    
    if (!data::empty(p)) while (true) {
        write_json(o, data::first(p));
        
        if (data::empty(p)) break;
        o << ", ";
    }
    
    return o << "], \"master\": \"" << Master << "\", \"index\": " << Index << "}";
    
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
    
    if (j.size() != 3 || !j.contains("prevouts") || !j.contains("master") || !j.contains("index")) throw "invalid wallet format";
    
    auto pp = j["prevouts"];
    list<prevout> prevouts;
    for (const json p: pp) prevouts <<= read_prevout(p);
    
    return wallet{prevouts, Bitcoin::hd::bip32::secret{string(j["master"])}, data::uint32(j["index"])};
}

Bitcoin::satoshi wallet::value() const {
    return data::fold([](Bitcoin::satoshi x, const prevout &p) -> Bitcoin::satoshi {
        return x + p.Value;
    }, Bitcoin::satoshi{0}, Prevouts);
}

wallet wallet::add(const p2pkh_prevout &p) {
    return wallet{
        Prevouts << p, 
        Master, 
        Index
    };
}

uint64 redeem_script_size(bool compressed_pubkey) {
    return compressed_pubkey ? 33 : 65;
}

constexpr uint64 p2sh_script_size = 24;

constexpr int64 dust = 500;

wallet::spent wallet::spend(Bitcoin::output to, double satoshis_per_byte) {
    
    list<p2pkh_prevout> prevouts = Prevouts;
    
    Gigamonkey::transaction_design {
        
    };
    
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
