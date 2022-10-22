#ifndef BOOSTMINER_MINER
#define BOOSTMINER_MINER

#include <gigamonkey/boost/boost.hpp>
#include <random.hpp>
#include <keys.hpp>
#include <whatsonchain_api.hpp>
#include <thread>

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
    std::pair<digest256, prevouts> select(random &, const jobs &, double minimum_profitability = 0);
    
    Bitcoin::transaction mine(
        random &, 
        // an unredeemed Boost PoW output 
        const Boost::puzzle &puzzle, 
        // the address you want the bitcoins to go to once you have redeemed the boost output.
        // this is not the same as 'miner address'. This is just an address in your 
        // normal wallet and should not be the address that goes along with the key above.
        const Bitcoin::address &address, 
        // max time to mine before the function returns. 
        double fee_rate, 
        double max_time_seconds, 
        double minimum_price_per_difficulty_sats, 
        double maximum_mining_difficulty = -1);
    
    struct channel_outer {
        virtual void update(const work::puzzle &) = 0;
        virtual work::solution wait(uint32 wait_time_seconds) = 0;
    };

    struct channel_inner {
        // get latest job. If there is no job yet, block. 
        // pointer will be null if the thread is supposed to stop. 
        virtual const work::puzzle latest() = 0;
        
        virtual void solved(const work::solution &) = 0;
    };

    void mining_thread(channel_inner *, random *, uint32);
    
    struct channel : channel_outer, channel_inner {
        std::mutex Mutex;
        std::condition_variable In;
        std::condition_variable Out;
        
        work::puzzle Puzzle;
        bool Set;
        
        work::solution Solution;
        bool Solved;
        
        void update(const work::puzzle &p) final override {
            std::unique_lock<std::mutex> lock(Mutex);
            Puzzle = p;
            Solved = false;
            Set = true;
            In.notify_all();
        }
        
        // get latest job. If there is no job yet, block. 
        // pointer will be null if the thread is supposed to stop. 
        const work::puzzle latest() final override {
            std::unique_lock<std::mutex> lock(Mutex);
            if (!Set) In.wait(lock);
            return Puzzle;
        }
        
        void solved(const work::solution &x) final override {
            std::lock_guard<std::mutex> lock(Mutex);
            Solution = x;
            Solved = true;
            Out.notify_one();
        }
        
        work::solution wait(uint32 wait_time_seconds) final override {
            std::unique_lock<std::mutex> lock(Mutex);
            Out.wait_for(lock, std::chrono::seconds(wait_time_seconds));
            return Solution;
        }
        
        channel() : Mutex{}, In{}, Out{}, Puzzle{}, Set{false}, Solution{}, Solved{false} {}
        
    };
    
    struct miner {
        miner(
            ptr<key_generator> keys, 
            ptr<address_generator> addresses, 
            uint32 threads, uint64 random_seed, 
            double minimum_profitability, 
            double fee_rate) :
            Keys{keys}, Addresses{addresses}, Threads{threads}, Seed{random_seed}, 
            MinProfitability{minimum_profitability}, FeeRate{fee_rate}, 
            Channel{}, Random{Seed}, Jobs{}, Selected{}, Current{}, Workers{} {}
        ~miner();
        
        void update(const jobs &j);
        Bitcoin::transaction wait(uint32 wait_time_seconds);
        
    private:
        ptr<key_generator> Keys;
        ptr<address_generator> Addresses;
        uint32 Threads; 
        uint64 Seed;
        double MinProfitability;
        double FeeRate;
        
        casual_random Random;
        channel Channel;
        
        jobs Jobs;
        std::pair<digest256, prevouts> Selected;
        Boost::puzzle Current;
        
        // start threads if not already. 
        void start();
        
        void select_and_update_job();
        
        std::vector<std::thread> Workers;
        
    };
    
    string write(const Bitcoin::txid &);
    string write(const Bitcoin::outpoint &);

}

#endif

