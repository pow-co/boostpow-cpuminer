#include <miner.hpp>

namespace BoostPOW {
    
    digest256 jobs::add_script(const Boost::output_script &z) {
        auto script_hash = SHA2_256(z.write());
        auto script_location = this->find(script_hash);
        if (script_location == this->end()) (*this)[script_hash] = Boost::candidate{z, {}};
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
}
