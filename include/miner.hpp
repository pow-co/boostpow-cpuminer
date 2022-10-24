#ifndef BOOSTMINER_MINER
#define BOOSTMINER_MINER

#include <gigamonkey/boost/boost.hpp>
#include <gigamonkey/schema/keysource.hpp>
#include <random.hpp>
#include <whatsonchain_api.hpp>
#include <thread>
#include <condition_variable>

namespace BoostPOW {
    
    struct jobs : std::map<digest256, Boost::candidate> {
        
        digest256 add_script(const Boost::output_script &z);
        void add_prevout(const digest256 &script_hash, const Boost::prevout &u);
        
        explicit operator json() const;
        
    };
    
    // select a puzzle optimally. 
    std::pair<digest256, Boost::candidate> select(random &, const jobs &, double minimum_profitability = 0);
    
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
        virtual void update(const std::pair<digest256, work::puzzle> &) = 0;
        virtual work::solution wait(uint32 wait_time_seconds) = 0;
    };

    struct channel_inner {
        // get latest job. If there is no job yet, block. 
        // pointer will be null if the thread is supposed to stop. 
        virtual std::pair<digest256, work::puzzle> latest() = 0;
        
        virtual void solved(const work::solution &) = 0;
    };

    void mining_thread(channel_inner *, random *, uint32);
    
    struct channel : channel_outer, channel_inner {
        std::mutex Mutex;
        std::condition_variable In;
        std::condition_variable Out;
        
        std::pair<digest256, work::puzzle> Puzzle;
        bool Set;
        
        work::solution Solution;
        bool Solved;
        
        void update(const std::pair<digest256, work::puzzle> &p) final override {
            std::unique_lock<std::mutex> lock(Mutex);
            Puzzle = p;
            Solved = false;
            Set = true;
            In.notify_all();
        }
        
        // get latest job. If there is no job yet, block. 
        // pointer will be null if the thread is supposed to stop. 
        std::pair<digest256, work::puzzle> latest() final override {
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
            ptr<key_source> keys, 
            ptr<address_source> addresses, 
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
        ptr<key_source> Keys;
        ptr<address_source> Addresses;
        uint32 Threads; 
        uint64 Seed;
        double MinProfitability;
        double FeeRate;
        
        casual_random Random;
        channel Channel;
        
        jobs Jobs;
        std::pair<digest256, Boost::candidate> Selected;
        Boost::puzzle Current;
        
        // start threads if not already. 
        void start();
        
        void select_and_update_job();
        
        std::vector<std::thread> Workers;
        
    };
    
    string write(const Bitcoin::txid &);
    string write(const Bitcoin::outpoint &);
    json to_json(const Boost::candidate::prevout &);
    json to_json(const Boost::candidate &);

}

#endif

