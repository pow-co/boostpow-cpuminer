#ifndef BOOSTMINER_POW_CO_API
#define BOOSTMINER_POW_CO_API

#include <data/networking/HTTP_client.hpp>
#include <gigamonkey/boost/boost.hpp>

using namespace Gigamonkey;

struct inpoint : Bitcoin::outpoint {
    using Bitcoin::outpoint::outpoint;
    bool valid () const {
        return this->Digest.valid ();
    }
    inpoint (const Bitcoin::txid &t, uint32 i) : outpoint {t, i} {}
};

struct pow_co : networking::HTTP_client {
    
    pow_co (networking::HTTP &http, string host = "pow.co") :
        networking::HTTP_client {http, networking::REST {"https", host}, tools::rate_limiter {3, 1}} {}
    
    list<Boost::prevout> jobs (uint32 limit = 10);
    
    Boost::prevout job (const Bitcoin::txid &);
    Boost::prevout job (const Bitcoin::outpoint &);
    
    inpoint spends (const Bitcoin::outpoint &);
    
    void submit_proof (const bytes &);
    
    bool broadcast (const bytes &);
    
};

#endif
