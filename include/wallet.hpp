#ifndef BOOSTMINER_WALLET
#define BOOSTMINER_WALLET

#include <gigamonkey/boost/boost.hpp>

using namespace Gigamonkey;
using nlohmann::json;

struct wallet {

    struct prevout {
        
        Gigamonkey::digest256 TXID;
        data::uint32 Index;
        Bitcoin::satoshi Value;
        
        Gigamonkey::Bitcoin::wif Key;
        
        Bitcoin::satoshi value() const;
    
        operator json() const;
        
        static wallet read(const json&);
        
    };
    
    list<prevout> Prevouts;
    hd::bip32::secret Master;
    data::uint32 Index;
    
    Bitcoin::satoshi value() const;
    
    struct spent;
    
    spent spend(Bitcoin::output to, satoshis_per_byte);
    wallet add(const prevout &);
    
    operator json() const;
    
    static wallet read(const json&);
    
};

struct wallet::spent {
    wallet Wallet
    Bitcoin::transaction Transaction;
};

bool broadcast(const Bitcoin::transaction &t);

wallet read_from_file(const std::string &filename);

bool write_to_file(const wallet &, const std::string &filename);

#endif
