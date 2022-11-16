#include <miner.hpp>
#include <gigamonkey/script/typed_data_bip_276.hpp>

namespace BoostPOW {
    
    digest256 jobs::add_script(const Boost::output_script &z) {
        auto script_hash = SHA2_256(z.write());
        auto script_location = this->find(script_hash);
        if (script_location == this->end()) (*this)[script_hash] = Boost::candidate{z};
        return script_hash;
    }

    void jobs::add_prevout(const digest256 &script_hash, const Boost::prevout &u) {
        auto script_location = this->find(script_hash);
        if (script_location != this->end()) {
            script_location->second = script_location->second.add(u);
        }
    }

    jobs::operator JSON() const {
        JSON::object_t puz;
        
        for (const auto &j : *this) puz[write(j.first)] = to_JSON(j.second);
        
        return puz;
    }
    
    string write(const Bitcoin::txid &txid) {
        std::stringstream txid_stream;
        txid_stream << txid;
        string txid_string = txid_stream.str();
        if (txid_string.size() < 73) throw string {"warning: txid string was "} + txid_string;
        return txid_string.substr(7, 66);
    }
    
    string write(const Bitcoin::outpoint &o) {
        std::stringstream ss;
        ss << write(o.Digest) << ":" << o.Index;
        return ss.str();
    }
    
    JSON to_JSON(const Boost::candidate::prevout &p) {
        return JSON {
            {"output", write(static_cast<Bitcoin::outpoint>(p))}, 
            {"value", int64(p.Value)}};
    }
    
    JSON to_JSON(const Boost::candidate &c) {
        
        JSON::array_t arr;
        auto prevouts = c.Prevouts.values();
        while (!data::empty(prevouts)) {
            arr.push_back(to_JSON(prevouts.first()));
            prevouts = prevouts.rest();
        }
        
        return JSON {
            {"script", typed_data::write(typed_data::mainnet, c.Script.write())}, 
            {"prevouts", arr}, 
            {"value", int64(c.value())}, 
            {"profitability", c.profitability()}, 
            {"difficulty", c.difficulty()}
        };
    }
}
