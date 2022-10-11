#ifndef BOOSTMINER_WHATSONCHAIN_API
#define BOOSTMINER_WHATSONCHAIN_API

#include <data/networking/HTTP_client.hpp>
#include <gigamonkey/address.hpp>

using namespace Gigamonkey;

struct whatsonchain : networking::HTTP_client {
    whatsonchain(networking::HTTP &http) : 
        networking::HTTP_client{http, tools::rate_limiter{3, 1}, 
            networking::REST{"https", "api.whatsonchain.com"}} {}
    
    struct utxo {
        
        Bitcoin::outpoint Outpoint;
        Bitcoin::satoshi Value;
        uint32 Height;
        
        utxo();
        utxo(const json &);
        bool valid() const;
        
    };
    
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
        
        list<utxo> get_unspent(const Bitcoin::address &);
        
        whatsonchain &API;
    };
    
    addresses address();
    
    struct transactions {
        
        bool broadcast(const bytes& tx);
        
        ptr<bytes> get_raw(const Bitcoin::txid&);
        
        whatsonchain &API;
    };
    
    transactions transaction();
    
    struct scripts {
        
        list<utxo> get_unspent(const digest256& script_hash);
        
        whatsonchain &API;
        
    };
    
    scripts script();
    
};

whatsonchain::addresses inline whatsonchain::address() {
    return addresses {*this};
}

whatsonchain::transactions inline whatsonchain::transaction() {
    return transactions {*this};
}

whatsonchain::scripts inline whatsonchain::script() {
    return scripts {*this};
}

bool inline whatsonchain::utxo::valid() const {
    return Value != 0;
}

#endif
