#ifndef BOOSTMINER_NETWORK
#define BOOSTMINER_NETWORK

#include <gigamonkey/mapi/mapi.hpp>
#include <pow_co_api.hpp>
#include <whatsonchain_api.hpp>

using namespace Gigamonkey;

struct network {
    boost::asio::io_context IO;
    networking::HTTP HTTP;
    whatsonchain WhatsOnChain;
    pow_co PowCo;
    BitcoinAssociation::MAPI Gorilla;
    
    network() : IO{}, HTTP{IO}, WhatsOnChain{HTTP}, PowCo{HTTP}, 
        Gorilla{HTTP, networking::REST{"https", "mapi.gorillapool.io"}} {}
    
    bool broadcast(const bytes &tx);
};

#endif
