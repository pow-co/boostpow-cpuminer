#ifndef BOOSTMINER_WHATSONCHAIN_API
#define BOOSTMINER_WHATSONCHAIN_API

#include <data/net/HTTP_client.hpp>
#include <gigamonkey/address.hpp>

using namespace Gigamonkey;

struct UTXO {
    
    Bitcoin::outpoint Outpoint;
    Bitcoin::satoshi Value;
    uint32 Height;
    
    UTXO ();
    UTXO (const JSON &);
    bool valid () const;
    
    bool operator == (const UTXO &u) const {
        return Outpoint == u.Outpoint && Value == u.Value && Height == u.Height;
    }
    
    explicit operator JSON () const;
    
};

std::ostream inline &operator << (std::ostream &o, const UTXO &u) {
    return o << "UTXO {" << u.Outpoint << ", " << u.Value << ", " << u.Height << "}" << std::endl;
}

struct whatsonchain : net::HTTP::client_blocking {
    whatsonchain (ptr<net::HTTP::SSL> ssl) :
        net::HTTP::client_blocking {ssl, net::HTTP::REST {"https", "api.whatsonchain.com"}, tools::rate_limiter {3, 1}} {}
    whatsonchain (): net::HTTP::client_blocking {net::HTTP::REST {"https", "api.whatsonchain.com"}, tools::rate_limiter {3, 1}} {}
    
    struct addresses {
        struct balance {
            Bitcoin::satoshi Confirmed;
            Bitcoin::satoshi Unconfirmed;
        };
        
        balance get_balance (const Bitcoin::address &);
        
        struct transaction_info {
            Bitcoin::txid TxHash;
            uint32 Height;
        };
        
        list<transaction_info> get_history (const Bitcoin::address &);
        
        list<UTXO> get_unspent (const Bitcoin::address &);
        
        whatsonchain &API;
    };
    
    addresses address ();
    
    struct transactions {
        
        bool broadcast (const bytes& tx);
        
        bytes get_raw (const Bitcoin::txid&);

        JSON tx_data (const Bitcoin::txid&);

        whatsonchain &API;
    };
    
    transactions transaction ();
    
    struct scripts {
        
        list<UTXO> get_unspent (const digest256& script_hash);
        list<Bitcoin::txid> get_history (const digest256& script_hash);
        
        whatsonchain &API;
        
    };
    
    scripts script ();
    
};

whatsonchain::addresses inline whatsonchain::address () {
    return addresses {*this};
}

whatsonchain::transactions inline whatsonchain::transaction () {
    return transactions {*this};
}

whatsonchain::scripts inline whatsonchain::script () {
    return scripts {*this};
}

bool inline UTXO::valid () const {
    return Value != 0;
}

#endif
