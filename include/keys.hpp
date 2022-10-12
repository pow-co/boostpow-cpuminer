#ifndef BOOSTMINER_KEYS
#define BOOSTMINER_KEYS

#include <gigamonkey/schema/hd.hpp>

using namespace Gigamonkey;

struct address_generator {
    virtual Bitcoin::address next() = 0;
};

struct key_generator {
    virtual Bitcoin::secret next() = 0;
};

struct single_address_generator : address_generator {
    Bitcoin::address Address;
    single_address_generator(const Bitcoin::address &addr) : Address{addr} {}
    Bitcoin::address next() override {
        return Address;
    }
};

struct single_key_generator : key_generator {
    Bitcoin::secret Key;
    single_key_generator(const Bitcoin::secret &key) : Key{key} {}
    Bitcoin::secret next() override {
        return Key;
    }
};

struct hd_address_generator : address_generator {
    hd::bip32::pubkey Key;
    uint32 Index;
    hd_address_generator(const hd::bip32::pubkey &key) : Key{key}, Index{0} {}
    hd_address_generator(const hd::bip32::pubkey &key, uint32 i) : Key{key}, Index{i} {}
    Bitcoin::address next() override {
        return Key.derive(Index++).address();
    }
};

struct hd_key_generator : key_generator {
    hd::bip32::secret Key;
    uint32 Index;
    hd_key_generator(const hd::bip32::secret &key) : Key{key}, Index{0} {}
    hd_key_generator(const hd::bip32::secret &key, uint32 i) : Key{key}, Index{i} {}
    Bitcoin::secret next() override {
        return Bitcoin::secret(Key.derive(Index++));
    }
};

#endif

