#ifndef BOOSTMINER_NETWORK
#define BOOSTMINER_NETWORK

#include <gigamonkey/mapi/mapi.hpp>
#include <pow_co_api.hpp>
#include <whatsonchain_api.hpp>
#include <jobs.hpp>

using namespace Gigamonkey;

namespace BoostPOW {

    struct network {
        net::HTTP::caller &Caller;
        whatsonchain WhatsOnChain;
        pow_co PowCo;
        BitcoinAssociation::MAPI Gorilla;
        
        network (net::HTTP::caller &caller, string api_host = "pow.co") :
            Caller {caller}, WhatsOnChain {caller}, PowCo {caller, api_host},
            Gorilla {caller, net::REST {"https", "mapi.gorillapool.io"}} {}
        
        BoostPOW::jobs jobs (uint32 limit = 10);
        
        bytes get_transaction (const Bitcoin::txid &);
        
        satoshi_per_byte mining_fee ();
        
        Boost::candidate job (const Bitcoin::outpoint &);
        
        struct broadcast_error {
            enum error {
                none, 
                unknown, 
                network_connection_fail, 
                insufficient_fee, 
                invalid_transaction
            };
            
            error Error;
            
            broadcast_error (error e): Error{e} {}
            
            operator bool () {
                return Error == none;
            }
        };
        
        broadcast_error broadcast (const bytes &tx);

        broadcast_error broadcast_solution (const bytes &tx);
        
    };
    
    struct fees {
        virtual double get () = 0;
        virtual ~fees () {}
    };
    
    struct given_fees : fees {
        double FeeRate;
        given_fees (double f) : FeeRate{f} {}
        double get () final override {
            return FeeRate;
        }
    };
    
    struct network_fees : fees {
        network &Net;
        double Default;
        network_fees (network &n, double d = .05) : Net {n}, Default {d} {}
        double get () final override {
            try {
                return double (Net.mining_fee ());
            } catch (std::exception &e) {
                std::cout << "Warning! Exception caught while trying to get a fee quote: " << e.what () << std::endl;
                return Default;
            }
        }
    };

    network::broadcast_error inline network::broadcast_solution (const bytes &tx) {
        PowCo.submit_proof (tx);
        return broadcast (tx);
    }
    
}

#endif
