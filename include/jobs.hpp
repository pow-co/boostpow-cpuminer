#ifndef BOOSTMINER_JOBS
#define BOOSTMINER_JOBS

#include <gigamonkey/boost/boost.hpp>

using namespace Gigamonkey;

namespace BoostPOW {
    using uint256 = Gigamonkey::uint256;
    
    struct jobs : std::map<digest256, Boost::candidate> {
        
        digest256 add_script(const Boost::output_script &z);
        void add_prevout(const digest256 &script_hash, const Boost::prevout &u);
        
        explicit operator JSON() const;
        
    };
}

#endif
