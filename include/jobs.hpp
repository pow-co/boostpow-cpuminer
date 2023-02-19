#ifndef BOOSTMINER_JOBS
#define BOOSTMINER_JOBS

#include <gigamonkey/boost/boost.hpp>
#include <gigamonkey/schema/keysource.hpp>

namespace BoostPOW {
    using namespace Gigamonkey;

    using uint256 = Gigamonkey::uint256;
    
    struct working : Boost::candidate {
        list<int> Workers;
        working (const Boost::candidate &x): Boost::candidate {x}, Workers {} {}
        working (): Boost::candidate {}, Workers {} {}
        
        // used to select random jobs. 
        double weight (double minimum_profitability, double tilt) const {
            if (this->profitability () < minimum_profitability) return 0;
            
            double factor = this->difficulty () / (this->difficulty () + tilt);
            
            double weight = 1;
            for (int i = 0; i < this->Workers.size (); i++) weight *= factor;
            
            return weight * (this->profitability () - minimum_profitability);
        }
    };
    
    struct jobs : std::map<digest256, working> {
        
        digest256 add_script (const Boost::output_script &z);
        void add_prevout (const digest256 &script_hash, const Boost::prevout &u);
        
        explicit operator JSON () const;
        
    };

    struct map_key_database final : key_source {
        ptr<key_source> Keys;
        uint32 MaxLookAhead;

        map<digest160, Bitcoin::secret> Past;
        list<Bitcoin::secret> Next;

        explicit map_key_database (ptr<key_source> keys, uint32 max_look_ahead = 0) :
            Keys {keys}, MaxLookAhead {max_look_ahead}, Past {}, Next {} {}

        Bitcoin::secret next () override {
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

        Bitcoin::secret operator [] (const digest160 &addr) {
            auto x = Past.contains (addr);
            if (x) return *x;

            while (data::size (Next) < MaxLookAhead) {

                Bitcoin::secret n = Keys->next ();
                digest160 a = Bitcoin::Hash160 (n.to_public ());

                Next = Next.append (n);

                if (!Past.contains (a)) {
                    Past = Past.insert (a, n);
                }
            }

            return Bitcoin::secret {};
        }

    };
    
    string write (const Bitcoin::txid &);
    string write (const Bitcoin::outpoint &);
    JSON to_JSON (const Boost::candidate::prevout &);
    JSON to_JSON (const Boost::candidate &);
}

#endif
