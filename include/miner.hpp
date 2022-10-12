#ifndef BOOSTMINER_MINER
#define BOOSTMINER_MINER

#include <gigamonkey/boost/boost.hpp>
#include <whatsonchain_api.hpp>

using namespace Gigamonkey;

struct boost_prevouts {
    Boost::output_script Script;
    
    list<whatsonchain::utxo> UTXOs;
    
    Bitcoin::satoshi Value;
    
    boost_prevouts() = default;
    boost_prevouts(const Boost::output_script &script, list<whatsonchain::utxo> utxos) : 
        Script{script}, UTXOs{utxos}, Value{
            data::fold([](const Bitcoin::satoshi so_far, const whatsonchain::utxo &u) -> Bitcoin::satoshi {
                return so_far + u.Value;
            }, Bitcoin::satoshi{0}, UTXOs)} {}
    
    double difficulty() const {
        return double(work::difficulty(Script.Target));
    }
    
    double profitability() const {
        return double(Value) / difficulty();
    }
    
    bool operator==(const boost_prevouts &pp) const {
        return Script == pp.Script && UTXOs == pp.UTXOs;
    }
    
};

// select a puzzle optimally. 
boost_prevouts select(map<digest256, boost_prevouts> puzzles, double minimum_profitability = 0);

bytes mine(
    // an unredeemed Boost PoW output 
    Boost::puzzle puzzle, 
    // the address you want the bitcoins to go to once you have redeemed the boost output.
    // this is not the same as 'miner address'. This is just an address in your 
    // normal wallet and should not be the address that goes along with the key above.
    Bitcoin::address address, 
    // max time to mine before the function returns. Default is one full day. 
    double max_time_seconds = 86400);

Boost::puzzle prevouts_to_puzzle(const boost_prevouts &prevs, const Bitcoin::secret &key);

#endif

