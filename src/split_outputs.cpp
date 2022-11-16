
#include <random.hpp>
#include <wallet.hpp>
#include <logger.hpp>
#include <miner.hpp>
#include <network.hpp>
#include <data/io/exception.hpp>
#include <gigamonkey/schema/bip_44.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>
#include <gigamonkey/fees.hpp>
#include <data/io/wait_for_enter.hpp>

const uint32 max_index_lookahead = 30000;
const Bitcoin::satoshi max_sats_per_output = 12345678;

struct derived_key {
    uint32 Index;
    Bitcoin::secret Secret;
    
    bool operator==(const derived_key &k) const {
        return Index == k.Index && Secret == k.Secret;
    }
};

struct addresses {
    HD::BIP_44::master_secret Master;
    uint32 MaxIndex;
    map<digest160, derived_key> Addresses;
    
    void derive_next() {
        auto k = Master.receive(MaxIndex);
        Addresses = Addresses.insert(k.address().Digest, derived_key {MaxIndex++, k});
    }
    
    list<p2pkh_prevout> find_my_outputs(map<Bitcoin::txid, Bitcoin::transaction> txs) {
        std::cout << "finding my outputs in " << txs.size() << ". Number of pregenerated keys is " << Addresses.size() << std::endl;
        list<p2pkh_prevout> mine;
        for (const data::entry<Bitcoin::txid, Bitcoin::transaction> &e : txs) {
            std::cout << "checking transaction " << e.Key << std::endl;
            uint32 index = 0;
            bool found_output = false;
            for (const Bitcoin::output &o : e.Value.Outputs) {
                Bitcoin::outpoint outpoint{e.Key, index++};
                
                pay_to_address script{o.Script};
                if (!script.valid()) continue;
                Bitcoin::address address{Bitcoin::address::main, script.Address};
                std::cout << "    output " << outpoint.Index << " has address " << address << std::endl;
                auto z = Addresses.contains(script.Address);
                if (!z) continue;
                found_output = true;
                std::cout << "    found address " << z->Secret.address() << " at index " << z->Index << ". max index is " << MaxIndex << std::endl;
                
                mine = mine << p2pkh_prevout {outpoint, o.Value, z->Secret};
                
                while (MaxIndex - z->Index < max_index_lookahead) derive_next();
            }
            if (!found_output) std::cout << "warning: no output found for tx " << e.Key << std::endl;
        }
        std::cout << "found " << mine.size() << " outputs." << std::endl;
        return mine;
    }
    
    addresses(const HD::BIP_44::master_secret &m) : Master{m}, MaxIndex{0}, Addresses{} {
        for (uint32 u = 0; u < max_index_lookahead; u++) derive_next();
    }
};

struct program_input {
    string Keyphrase;
    string Master;
    list<digest256> Transactions;
    
    program_input(const JSON &j) {
        
        if (!j.contains("keyphrase") || !j.contains("master") || !j.contains("txs")) 
            throw data::exception {} << "invalid input format: " << j;
        
        const JSON &keyphrase = j["keyphrase"];
        const JSON &master = j["master"];
        const JSON &txs = j["txs"];
        
        if (!keyphrase.is_string() || !master.is_string() || !txs.is_array())
            throw data::exception {} << "invalid input format: " << j;
        
        list<digest256> transactions;
        for (const JSON &txid : txs) {
            if (!txid.is_string()) throw data::exception {} << "invalid txid: " << txid;
            digest256 t{string(txid)};
            if (!t.valid()) throw data::exception {} << "invalid txid: " << txid;
            transactions = transactions << t;
        }
        
        Keyphrase = keyphrase;
        Master = master;
        Transactions = transactions;
    }
};

HD::BIP_44::master_secret make_wallet(string keyphrase, string master) {
    HD::BIP_32::secret hd_master{master};
    
    if (!hd_master.valid()) throw data::exception{"can't read master key"};
    
    HD::BIP_44::master_secret from_master{hd_master};
    //HD::BIP_44::secret from_master{hd_master.derive({HD::BIP_44::purpose, HD::BIP_44::coin_type_Bitcoin_Cash})};
    /*
    HD::BIP_44::secret from_keyphrase = HD::BIP_44::electrum_sv_wallet(keyphrase);
    
    if (!from_keyphrase.valid()) throw data::exception{"can't read keyphrase"};
    
    if (from_master != from_keyphrase) throw data::exception{"inconsistency between keyphrase and master key"};
    */
    return from_master;
}

struct split_outputs {
    BoostPOW::network Net;
    BoostPOW::casual_random Random;
    map<Bitcoin::txid, Bitcoin::transaction> get_txs(const list<Bitcoin::txid> &txids) {
        map<Bitcoin::txid, Bitcoin::transaction> m{};
        
        for (const auto &txid : txids) {
            Bitcoin::transaction tx{Net.get_transaction(txid)};
            if (!tx.valid()) throw data::exception {} << "could not get a transaction for txid " << txid;
            m = m.insert(txid, tx);
        }
        
        return m;
    }
    
    split_outputs(const program_input &p, const digest160 &addr): Net{}, Random{} {
        addresses my_addrs{make_wallet(p.Keyphrase, p.Master)};
        auto my_outputs = my_addrs.find_my_outputs(get_txs(p.Transactions));
        
        auto z = my_addrs.Addresses.contains(addr);
        if (!z) throw data::exception {} << "can't find begin address";
        
        uint32 latest_index = z->Index;
        
        std::cout << "about to generate txs" << std::endl;
        
        for (const p2pkh_prevout &p : my_outputs) {
            
            // What is the value of this output? 
            std::cout << "    value of output at " << p.Outpoint << " is " << p.Value << std::endl;
            std::cout << "    address is " << p.Key.address() << std::endl;
            
            bool matched = false;
            for (const auto &u : Net.WhatsOnChain.address().get_unspent(p.Key.address())) 
                if (u.Outpoint == p.Outpoint) matched = true;
            
            if (!matched) {
                std::cout << "   output is spent " << std::endl;
                wait_for_enter();
                continue;
            }
            
            uint32 num_new_outputs = ceil(double(int64(p.Value)) / double(max_sats_per_output));
            
            // How many new outputs will we make with it? 
            std::cout << "    would be divided into " 
                << num_new_outputs << " outputs. " << std::endl;
            
            uint32 expected_tx_size = 
                4 + 
                4 + 
                1 + 40 +
                Bitcoin::signature::MaxSize + 33 + 2 +
                Bitcoin::var_int::size(num_new_outputs) + 
                num_new_outputs * (8 + 26);
                
            // How big will the tx be? 
            std::cout << "    tx size will be " << expected_tx_size << std::endl;
            
            int64 expected_fee = ceil(double(expected_tx_size) * double(50) / 1000.);
            
            // How much will that cost? 
            std::cout << "    cost at 50 stats per 1000 bytes is " << expected_fee << std::endl;
            int64 remainder = (int64(p.Value) - expected_fee);
            std::cout << "    leaving " << remainder << std::endl;
            
            int64 sats_per_output = ceil(double(remainder) / double(num_new_outputs));
            std::cout << "    sats per output is " << sats_per_output << std::endl;
            
            list<Bitcoin::output> outputs{};
            
            for (int i = 0; i < num_new_outputs; i++) {
                int64 value = sats_per_output < remainder ? sats_per_output : remainder;
                if (remainder == 0) throw data::exception {} << "math error index " << i;
                remainder -= value;
                bytes script = pay_to_address::script(my_addrs.Master.receive(latest_index++).address().Digest);
                outputs = outputs << Bitcoin::output{Bitcoin::satoshi{value}, script};
            }
            
            transaction_design design{2, 
                {transaction_design::input{Bitcoin::prevout(p), 107}}, 
                shuffle(outputs, Random.Engine), 0};
            
            std::cout << "    tx expected size is " << design.expected_size() << std::endl;
            std::cout << "    expected fee rate is " << design.fee_rate() << std::endl;
            
            auto documents = design.documents();
            
            if (documents.size() != 1) throw data::exception {} << "invalid design doc list generated";
            
            auto tx = design.complete({pay_to_address::redeem(
                p.Key.sign(design.documents().first()), 
                p.Key.to_public())});
            
            if (!tx.valid()) throw data::exception {"invalid transaction"};
            
            auto tx_size = tx.serialized_size();
            std::cout << "    final tx size is " << tx_size << std::endl;
            auto sent = tx.sent();
            std::cout << "    final tx sent is " << sent << std::endl;
            auto spent = tx.spent();
            std::cout << "    final tx spent is " << spent << std::endl;
            auto fee = spent - tx.sent();
            std::cout << "    final tx fee is " << fee << std::endl;
            double fee_rate = double(fee) / double(tx_size);
            std::cout << "    final tx fee rate is " << fee_rate << std::endl;
            std::cout << "    txid is " << tx.id() << std::endl;
            
            if (fee_rate < (50./1000.) || fee_rate > (60./1000.)) throw data::exception {} << "invalid fee rate " << fee_rate;
            
            std::cout << "    broadcasting tx " << tx.id() << std::endl;
            wait_for_enter();
            auto success = Net.broadcast(bytes(tx));
            if (success) std::cout << "    successfully broadcast tx " << tx.id() << std::endl;
            else std::cout << "    failed to broadcast tx " << tx.id() << std::endl;
        } 
    }
};

void split_outputs_main(int arg_count, char** arg_values) {
    if (arg_count != 3) throw data::exception {} << "expect two arguments";
    
    std::string filename{arg_values[1]};
    std::string next_address_string{arg_values[2]};
    
    std::cout << "checking file " << filename << " and address " << next_address_string << std::endl;
    
    Bitcoin::address next_address{next_address_string};
    if (!next_address.valid()) throw data::exception {} << "could not read address" << next_address_string;
    
    std::fstream my_file;
    my_file.open(filename, std::ios::in);
    if (!my_file) throw data::exception("could not open file"); 
    
    json j = json::parse(my_file);
    
    std::cout << "JSON read: " << j << std::endl;
    
    try {
        split_outputs(program_input{j}, next_address.Digest);
    } catch (JSON::exception &e) {
        throw data::exception {} << "could not read json " << j;
    }
    std::cout << "program complete" << std::endl;
}

int main(int arg_count, char** arg_values) {
    try {
        split_outputs_main(arg_count, arg_values);
    } catch (const data::exception &e) {
        std::cout << "Error: " << e.message() << std::endl;
        return 1;
    } catch (const std::exception &e) {
        std::cout << "Internal error: " << e.what() << std::endl;
        return 2;
    }
    
    return 0;
}

