#ifndef BOOSTMINER_WALLET
#define BOOSTMINER_WALLET

#include <gigamonkey/boost/boost.hpp>
#include <gigamonkey/wif.hpp>
#include <gigamonkey/schema/hd.hpp>

using namespace Gigamonkey;

struct p2pkh_prevout {
    
    Gigamonkey::digest256 TXID;
    data::uint32 Index;
    Bitcoin::satoshi Value;
    
    Gigamonkey::Bitcoin::secret Key;
    
    Bitcoin::satoshi value() const;
    
};

struct wallet {
    
    list<p2pkh_prevout> Prevouts;
    hd::bip32::secret Master;
    data::uint32 Index;
    
    Bitcoin::satoshi value() const;
    
    struct spent;
    
    spent spend(Bitcoin::output to, double satoshis_per_byte);
    wallet add(const p2pkh_prevout &);
    
    operator std::string() const;
    
    static wallet read(std::string_view);
    
};

struct wallet::spent {
    wallet Wallet;
    Bitcoin::transaction Transaction;
};

std::ostream &write_json(std::ostream &, wallet);
wallet read_json(std::istream &);

bool broadcast(const Bitcoin::transaction &t);

#endif
