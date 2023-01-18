#ifndef BOOSTMINER_JOBS
#define BOOSTMINER_JOBS

#include <gigamonkey/boost/boost.hpp>

namespace BoostPOW {
    using namespace Gigamonkey;

    using uint256 = Gigamonkey::uint256;
    
    struct working : Boost::candidate {
        list<int> Workers;
        working(const Boost::candidate &x): Boost::candidate {x}, Workers {} {}
        working(): Boost::candidate {}, Workers {} {}
        
        // used to select random jobs. 
        double weight(double minimum_profitability, double tilt) const {
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
    
    string write (const Bitcoin::txid &);
    string write (const Bitcoin::outpoint &);
    JSON to_JSON (const Boost::candidate::prevout &);
    JSON to_JSON (const Boost::candidate &);
}

#endif
