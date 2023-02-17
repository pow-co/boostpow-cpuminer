#ifndef BOOSTMINER_POW_CO_API
#define BOOSTMINER_POW_CO_API

#include <data/net/asio/session.hpp>
#include <data/net/HTTP_client.hpp>
#include <gigamonkey/boost/boost.hpp>

using namespace Gigamonkey;

struct inpoint : Bitcoin::outpoint {
    using Bitcoin::outpoint::outpoint;
    bool valid () const {
        return this->Digest.valid ();
    }
    inpoint (const Bitcoin::txid &t, uint32 i) : outpoint {t, i} {}
};

struct pow_co : net::HTTP::client {
    
    pow_co (net::HTTP::caller &http, string host = "pow.co") :
        net::HTTP::client {http, net::REST {"https", host}, tools::rate_limiter {3, 1}} {}
    
    list<Boost::prevout> jobs (uint32 limit = 10);
    
    Boost::prevout job (const Bitcoin::txid &);
    Boost::prevout job (const Bitcoin::outpoint &);
    
    inpoint spends (const Bitcoin::outpoint &);
    
    void submit_proof (const bytes &);
    
    bool broadcast (const bytes &);

    void connect (net::asio::error_handler error_handler, net::interaction<const JSON &>, net::close_handler);
    
};

#endif
