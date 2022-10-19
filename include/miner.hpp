#ifndef BOOSTMINER_MINER
#define BOOSTMINER_MINER

#include <gigamonkey/boost/boost.hpp>
#include <random.hpp>
#include <keys.hpp>
#include <whatsonchain_api.hpp>

namespace BoostPOW {

struct prevouts {
    Boost::output_script Script;
    
    list<utxo> UTXOs;
    
    Bitcoin::satoshi Value;
    
    prevouts() = default;
    prevouts(const Boost::output_script &script, list<utxo> utxos) : 
        Script{script}, UTXOs{utxos}, Value{
            data::fold([](const Bitcoin::satoshi so_far, const utxo &u) -> Bitcoin::satoshi {
                return so_far + u.Value;
            }, Bitcoin::satoshi{0}, UTXOs)} {}
    
    double difficulty() const {
        return double(work::difficulty(Script.Target));
    }
    
    double profitability() const {
        return double(Value) / difficulty();
    }
    
    bool operator==(const prevouts &pp) const {
        return Script == pp.Script && UTXOs == pp.UTXOs;
    }
    
    void add(const utxo &u) {
        UTXOs = UTXOs << u;
        Value += u.Value;
    }
    
    Boost::puzzle to_puzzle(const Bitcoin::secret &key) const;
    
    explicit operator json() const;
    
};

struct jobs : std::map<digest256, prevouts> {
    
    digest256 add_script(const Boost::output_script &z) {
        auto script_hash = SHA2_256(z.write());
        auto script_location = this->find(script_hash);
        if (script_location == this->end()) (*this)[script_hash] = prevouts{z, {}};
        return script_hash;
    }
    
    void add_utxo(const digest256 &script_hash, const utxo &u) {
        auto script_location = this->find(script_hash);
        if (script_location != this->end()) script_location->second.add(u);
    }
    
    explicit operator json() const;
    
};
    
// select a puzzle optimally. 
prevouts select(random &, const jobs &, double minimum_profitability = 0);

Bitcoin::transaction mine(
    random &, 
    // an unredeemed Boost PoW output 
    const Boost::puzzle &puzzle, 
    // the address you want the bitcoins to go to once you have redeemed the boost output.
    // this is not the same as 'miner address'. This is just an address in your 
    // normal wallet and should not be the address that goes along with the key above.
    const Bitcoin::address &address, 
    // max time to mine before the function returns. 
    double max_time_seconds, 
    double minimum_price_per_difficulty_sats, 
    double maximum_mining_difficulty = -1);

}

/*
struct miner_outer {work::puzzle &
    virtual void update_job(int, const work::puzzle &) = 0;
};

struct miner_inner {
    // get latest job. If there is no job yet, block. 
    // pointer will be null if the thread is supposed to stop. 
    virtual const data::entry<int, work::puzzle> *latest_job() = 0;
    virtual void solved(int, const work::solution &) = 0;
};

void mining_thread(miner_inner const *);

struct miner : miner_outer, miner_inner {
    std::shared_mutex Mutex;
    std::list<data::entry<int, work::puzzle>> Puzzles;
    int Max;
};*/

#endif

