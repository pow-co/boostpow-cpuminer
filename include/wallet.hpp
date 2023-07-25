#ifndef BOOSTMINER_WALLET
#define BOOSTMINER_WALLET

#include <gigamonkey/boost/boost.hpp>
#include <gigamonkey/wif.hpp>
#include <gigamonkey/schema/hd.hpp>

using namespace Gigamonkey;

struct p2pkh_prevout {
    
    Bitcoin::outpoint Outpoint;
    Bitcoin::satoshi Value;
    
    Gigamonkey::Bitcoin::secret Key;
    
    explicit operator Bitcoin::prevout() const;
    
};

std::ostream inline &operator<<(std::ostream &o, const p2pkh_prevout &p) {
    return o << "p2pkh_prevout{" << p.Outpoint << ", Value: " << p.Value << ", Key: " << p.Key;
}

struct wallet {
    
    static constexpr double default_fee_rate = 0.5;
    
    list<p2pkh_prevout> Prevouts;
    HD::BIP_32::secret Master;
    data::uint32 Index;
    
    Bitcoin::satoshi value() const;
    
    struct spent;
    
    spent spend(Bitcoin::output to, double satoshis_per_byte = default_fee_rate) const;
    wallet insert(const p2pkh_prevout &) const;
    
    operator std::string() const;
    
    static wallet read(std::string_view);
    
};

std::ostream &operator<<(std::ostream &, wallet &);

struct wallet::spent {
    wallet Wallet;
    Bitcoin::transaction Transaction;
};

std::ostream &write_json(std::ostream &, const wallet&);
wallet read_json(std::istream &);

std::ostream inline &operator<<(std::ostream &o, wallet &w) {
    return write_json(o, w);
}

void write_to_file(const wallet &w, const std::string &filename);

wallet read_wallet_from_file(const std::string &filename);

wallet restore(const HD::BIP_32::secret &master, uint32 max_look_ahead);

#endif
