#ifndef BOOSTMINER_MINER
#define BOOSTMINER_MINER

#include <gigamonkey/schema/keysource.hpp>
#include <random.hpp>
#include <network.hpp>
#include <thread>
#include <condition_variable>
#include <mutex>

namespace BoostPOW {
    
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
    
    uint64 estimate_size(size_t inputs_size, size_t pay_script_size);
    Bitcoin::transaction redeem_puzzle(const Boost::puzzle &puzzle, const work::solution &solution, list<Bitcoin::output> pay);
    
    struct miner : virtual work::challenger {
        // get latest job. If there is no job yet, block. 
        // pointer will be null if the thread is supposed to stop. 
        virtual work::puzzle latest() = 0;
        
        virtual ~miner() {}
    };

    void mining_thread(miner *, random *, uint32);
    
    struct channel : virtual miner {
        std::mutex Mutex;
        std::condition_variable In;
        
        work::puzzle Puzzle;
        bool Set;
        
        void pose(const work::puzzle &p) final override {
            std::unique_lock<std::mutex> lock(Mutex);
            Puzzle = p;
            Set = true;
            
            In.notify_all();
        }
        
        // get latest job. If there is no job yet, block. 
        // pointer will be null if the thread is supposed to stop. 
        work::puzzle latest() final override {
            std::unique_lock<std::mutex> lock(Mutex);
            if (!Set) In.wait(lock);
            return Puzzle;
        }
        
        channel() : Mutex{}, In{}, Puzzle{}, Set{false} {}
        
    };
    
    struct multithreaded : channel {
        multithreaded(
            uint32 threads, uint64 random_seed) :
            Threads{threads}, Seed{random_seed}, Workers{} {}
        
        virtual ~multithreaded();
        
        uint32 Threads; 
        uint64 Seed;
        
        // start threads if not already. 
        void start_threads();
        
    private:
        std::vector<std::thread> Workers;
    };
    
    struct redeemer : virtual miner {
        redeemer() : Mutex{}, Current{}, RedeemAddress{}, FeeRate{} {}
        
        void mine(const Boost::puzzle &, const Bitcoin::address &, double fee_rate);
        
    private:
        std::mutex Mutex;
        
        Boost::puzzle Current;
        Bitcoin::address RedeemAddress;
        double FeeRate;
        
        virtual void redeemed(const Bitcoin::transaction &) = 0;
        void solved(const work::solution &) override;
        
        std::vector<std::thread> Workers;
    };
    
    struct manager : redeemer {
        manager(
            network &net, 
            ptr<key_source> keys, 
            ptr<address_source> addresses, 
            casual_random random,  
            double maximum_difficulty, 
            double minimum_profitability, 
            double fee_rate) : redeemer{}, 
            Net{net}, Keys{keys}, Addresses{addresses}, 
            MaxDifficulty{maximum_difficulty}, MinProfitability{minimum_profitability}, 
            FeeRate{fee_rate}, Random{random}, Jobs{}, Selected{}, Workers{} {}
        
        void run();
        
    private:
        network &Net;
        ptr<key_source> Keys;
        ptr<address_source> Addresses;
        
        casual_random Random;
        
        double MaxDifficulty;
        double MinProfitability;
        double FeeRate;
        
        jobs Jobs;
        std::pair<digest256, Boost::candidate> Selected;
        
        std::vector<std::thread> Workers;
        
        void redeemed(const Bitcoin::transaction &redeem_tx) override;
        
        void update_jobs(const jobs &j);
        void select_job();
        
    };
    
    string write(const Bitcoin::txid &);
    string write(const Bitcoin::outpoint &);
    JSON to_JSON(const Boost::candidate::prevout &);
    JSON to_JSON(const Boost::candidate &);
    
}

#endif

