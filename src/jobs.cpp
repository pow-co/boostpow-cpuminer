#include <miner.hpp>
#include <gigamonkey/script/typed_data_bip_276.hpp>

namespace BoostPOW {

    double working::weight (double minimum_profitability, double tilt) const {
        if (this->profitability () < minimum_profitability) return 0;

        double factor = this->difficulty () / (this->difficulty () + tilt);

        double weight = 1;
        for (int i = 0; i < this->Workers.size (); i++) weight *= factor;

        return weight * (this->profitability () - minimum_profitability);
    }
    
    digest256 jobs::add_script (const Boost::output_script &z) {
        auto script_hash = SHA2_256 (z.write ());
        auto script_location = Jobs.find (script_hash);
        if (script_location == Jobs.end ()) Jobs[script_hash] = Boost::candidate {z};
        return script_hash;
    }

    void jobs::add_prevout (const Boost::prevout &u) {
        auto script_location = Jobs.find (u.id ());
        if (script_location != Jobs.end ()) script_location->second = script_location->second.add (u);
        else Jobs[u.id ()] = Boost::candidate {{u}};
        Scripts[u.Key] = u.id ();
    }

    uint32 jobs::remove (function<bool (const working &)> f) {
        uint32 removed = 0;
        for (auto it = Jobs.cbegin (); it != Jobs.cend ();)
            if (f (it->second)) {
                for (const auto &p : it->second.Prevouts.values ()) {
                    auto x = Scripts.find (static_cast<const Bitcoin::outpoint &> (p));
                    if (x != Scripts.end ()) Scripts.erase (x);
                }

                removed++;
                it = Jobs.erase (it);
            }

            else ++it;

        return removed;
    }

    jobs::operator JSON () const {
        JSON::object_t puz;
        
        for (const auto &j : Jobs) puz[write (j.first)] = to_JSON (j.second);
        
        return puz;
    }

    std::map<digest256, working>::iterator jobs::random_select (random &r, double minimum_profitability) {

        if (Jobs.size () == 0) return {};

        double normalization = 0;
        for (const auto &p : Jobs) normalization += p.second.weight (minimum_profitability, .025);
        if (normalization == 0) return Jobs.end ();
        double random = r.range01 () * normalization;

        double accumulated_profitability = 0;
        for (auto it = Jobs.begin (); it != Jobs.end (); it++) {
            accumulated_profitability += it->second.weight (minimum_profitability, .025);

            if (accumulated_profitability >= random) return it;
        }

        throw exception {"Warning: random_select failed to select job. "};
    }
    
    string write (const Bitcoin::txid &txid) {
        std::stringstream txid_stream;
        txid_stream << txid;
        string txid_string = txid_stream.str ();
        if (txid_string.size () < 73) throw string {"warning: txid string was "} + txid_string;
        return txid_string.substr (7, 66);
    }
    
    string write (const Bitcoin::outpoint &o) {
        std::stringstream ss;
        ss << write (o.Digest) << ":" << o.Index;
        return ss.str ();
    }
    
    JSON to_JSON (const Boost::candidate::prevout &p) {
        return JSON {
            {"output", write (static_cast<Bitcoin::outpoint> (p))},
            {"value", int64 (p.Value)}};
    }
    
    JSON to_JSON (const Boost::candidate &c) {
        
        JSON::array_t arr;
        auto prevouts = c.Prevouts.values ();
        while (!data::empty (prevouts)) {
            arr.push_back (to_JSON (prevouts.first ()));
            prevouts = prevouts.rest ();
        }
        
        return JSON {
            {"script", typed_data::write (typed_data::mainnet, c.Script.write ())},
            {"prevouts", arr}, 
            {"value", int64 (c.value ())},
            {"profitability", c.profitability ()},
            {"difficulty", c.difficulty ()}
        };
    }

    Bitcoin::secret map_key_database::next () {
        if (data::size (Next) != 0) {
            auto n = data::first (Next);
            Next = data::rest (Next);
            return n;
        }

        Bitcoin::secret n = Keys->next ();
        digest160 a = Bitcoin::Hash160 (n.to_public ());

        if (!Past.contains (a)) Past = Past.insert (a, n);

        return n;
    }

    Bitcoin::secret map_key_database::operator [] (const digest160 &addr) {
        auto x = Past.contains (addr);
        if (x) return *x;

        while (data::size (Next) < MaxLookAhead) {
            Bitcoin::secret n = Keys->next ();
            digest160 a = Bitcoin::Hash160 (n.to_public ());

            Next = Next.append (n);

            if (!Past.contains (a)) Past = Past.insert (a, n);
        }

        return Bitcoin::secret {};
    }
}
