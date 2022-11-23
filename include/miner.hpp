#ifndef BOOSTMINER_MINER
#define BOOSTMINER_MINER

#include <gigamonkey/schema/keysource.hpp>
#include <gigamonkey/work/solver.hpp>
#include <random.hpp>
#include <network.hpp>
#include <thread>
#include <condition_variable>
#include <mutex>

namespace BoostPOW {
    
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

    void mining_thread(work::selector *, random *, uint32);
    
    struct channel : virtual work::selector, virtual work::solver {
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
        work::puzzle select() final override {
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
    
    struct redeemer : virtual work::selector, virtual work::solver {
        redeemer() : work::selector{}, Mutex{}, Out{}, Current{} {}
        
        void mine(const std::pair<digest256, Boost::puzzle> &p);
        
        void wait_for_solution() {
            std::unique_lock<std::mutex> lock(Mutex);
            if (Solved) return;
            Out.wait(lock);
        }
        
    protected:
        std::mutex Mutex;
        std::condition_variable Out;
        
        std::pair<digest256, Boost::puzzle> Current;
        std::pair<digest256, Boost::puzzle> Last;
        
        bool Solved;
        
        void solved(const work::solution &) override;
        
        virtual void submit(const std::pair<digest256, Boost::puzzle> &, const work::solution &) = 0;
    };
    
    struct manager {
        
        struct redeemer final : BoostPOW::redeemer, BoostPOW::channel {
            std::thread Worker;
            manager *Manager;
            redeemer(
                manager *m, 
                uint64 random_seed, uint32 index) : 
                BoostPOW::redeemer{}, 
                BoostPOW::channel{}, 
                Worker{std::thread{mining_thread, 
                    &static_cast<work::selector &>(*this), 
                    new casual_random{random_seed}, index}}, Manager{m} {}
            
            void submit(const std::pair<digest256, Boost::puzzle> &puzzle, const work::solution &solution) final override {
                Manager->submit(puzzle, solution);
            }
        };
        
        manager(
            network &net, 
            fees &f,
            key_source &keys, 
            address_source &addresses, 
            uint64 random_seed, 
            double maximum_difficulty, 
            double minimum_profitability, int threads);
        
        void update_jobs(const BoostPOW::jobs &j);
        
        ~manager();
        
        void submit(const std::pair<digest256, Boost::puzzle> &, const work::solution &);
        
    private:
        std::mutex Mutex;
        
        network &Net;
        fees &Fees;
        
        key_source &Keys;
        address_source &Addresses;
        
        casual_random Random;
        
        double MaxDifficulty;
        double MinProfitability;
        
        jobs Jobs;
        
        uint32 Threads;
        redeemer **Redeemers;
        
        void select_job(int i);
        
    };
    
}

#endif

