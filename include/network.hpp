#ifndef BOOSTMINER_NETWORK
#define BOOSTMINER_NETWORK

#include <gigamonkey/mapi/mapi.hpp>
#include <pow_co_api.hpp>
#include <whatsonchain_api.hpp>
#include <jobs.hpp>

using namespace Gigamonkey;

namespace BoostPOW {

    struct network {
        boost::asio::io_context IO;
        networking::HTTP HTTP;
        whatsonchain WhatsOnChain;
        pow_co PowCo;
        BitcoinAssociation::MAPI Gorilla;
        
        network() : IO{}, HTTP{IO}, WhatsOnChain{HTTP}, PowCo{HTTP}, 
            Gorilla{HTTP, networking::REST{"https", "mapi.gorillapool.io"}} {}
        
        bool broadcast(const bytes &tx);
        
        BoostPOW::jobs jobs(uint32 limit = 10);
        
        bytes get_transaction(const Bitcoin::txid &);
        
    };
    
}

#endif
