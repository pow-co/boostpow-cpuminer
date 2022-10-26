#ifndef BOOSTMINER_MINER
#define BOOSTMINER_MINER

#include <gigamonkey/boost/boost.hpp>
#include <gigamonkey/schema/keysource.hpp>
#include <random.hpp>
#include <thread>
#include <condition_variable>
#include <mutex>

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
    
    
    Bitcoin::satoshi calculate_fee(size_t inputs_size, size_t pay_script_size, double fee_rate);
    Bitcoin::transaction redeem_puzzle(const Boost::puzzle &puzzle, const work::solution &solution, list<Bitcoin::output> pay);
    
    struct channel_outer {
        virtual void update(const std::pair<digest256, work::puzzle> &) = 0;
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
        
        std::pair<digest256, work::puzzle> Puzzle;
        
        void update(const std::pair<digest256, work::puzzle> &p) final override {
            std::unique_lock<std::mutex> lock(Mutex);
            Puzzle = p;
            In.notify_all();
        }
        
        // get latest job. If there is no job yet, block. 
        // pointer will be null if the thread is supposed to stop. 
        std::pair<digest256, work::puzzle> latest() final override {
            std::unique_lock<std::mutex> lock(Mutex);
            if (!Puzzle.first.valid()) In.wait(lock);
            return Puzzle;
        }
        
        channel() : Mutex{}, In{}, Puzzle{} {}
        
    };
    
    struct miner : channel {
        miner(
            uint32 threads, uint64 random_seed) :
            Threads{threads}, Seed{random_seed}, Workers{} {}
        ~miner();
        
        uint32 Threads; 
        uint64 Seed;
        
        // start threads if not already. 
        void start();
        
    private:
        std::vector<std::thread> Workers;
        
    };
    
    string write(const Bitcoin::txid &);
    string write(const Bitcoin::outpoint &);
    json to_json(const Boost::candidate::prevout &);
    json to_json(const Boost::candidate &);

}

#endif

