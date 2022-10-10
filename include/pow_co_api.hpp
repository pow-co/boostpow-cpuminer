#ifndef BOOSTMINER_POW_CO_API
#define BOOSTMINER_POW_CO_API

#include <data/networking/HTTP_client.hpp>
#include <gigamonkey/boost/boost.hpp>

using namespace Gigamonkey;

struct pow_co : networking::HTTP_client {
    pow_co(networking::HTTP &)
    
    list<Boost::prevout> jobs();
    
}

#endif
