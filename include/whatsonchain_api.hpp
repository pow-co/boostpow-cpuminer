#ifndef BOOSTMINER_WHATSONCHAIN_API
#define BOOSTMINER_WHATSONCHAIN_API

#include <data/networking/HTTP_client.hpp>
#include <gigamonkey/address.hpp>

using namespace Gigamonkey;

struct whatsonchain : networking::HTTP_client {
    whatsonchain(networking::HTTP &http) : 
        networking::HTTP_client{http, tools::rate_limiter{3, 1}, 
            networking::REST{"https", "api.whatsonchain.com"}} {}
    
    struct addresses {
        struct balance {
            Bitcoin::satoshi Confirmed;
            Bitcoin::satoshi Unconfirmed;
        };
        
        balance get_balance(const Bitcoin::address &);
        
        struct transaction_info {
            Bitcoin::txid TxHash;
            uint32 Height;
        };
        
        list<transaction_info> get_history(const Bitcoin::address &);
        
        struct unspent_output_info {
            
            Bitcoin::txid TxHash;
            uint32 Index;
            Bitcoin::satoshi Value;
            uint32 Height;
            
        };
        
        list<unspent_output_info> get_unspent_transactions(const Bitcoin::address &);
        
        whatsonchain &API;
    };
    
    addresses address();
    
    struct transactions {
        
        bool broadcast(const bytes& tx);
        
        ptr<bytes> get_raw(const Bitcoin::txid&);
        
        whatsonchain &API;
    };
    
    transactions transaction();
    
};

whatsonchain::addresses inline whatsonchain::address() {
    return addresses {*this};
}

whatsonchain::transactions inline whatsonchain::transaction() {
    return transactions {*this};
}

#endif
