#include <gigamonkey/script/typed_data_bip_276.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>
#include <sv/uint256.h>
#include <miner.hpp>
#include <logger.hpp>
#include <math.h>

namespace BoostPOW {
    using uint256 = Gigamonkey::uint256;

    // A cpu miner function. 
    work::proof cpu_solve (const work::puzzle& p, const work::solution& initial, double max_time_seconds) {
        
        uint32 initial_time = initial.Share.Timestamp.Value;
        uint32 local_initial_time = Bitcoin::timestamp::now ().Value;
        
        uint64_big extra_nonce_2; 
        std::copy (initial.Share.ExtraNonce2.begin (), initial.Share.ExtraNonce2.end (), extra_nonce_2.begin ());
        
        uint256 target = p.Candidate.Target.expand ();
        if (target == 0) return {};
        
        N total_hashes {0};
        N nonce_increment {"0x0100000000"};
        uint32 display_increment = 0x00800000;
        
        work::proof pr {p, initial};
        
        uint32 begin {Bitcoin::timestamp::now ()};
        
        while(true) {
            uint256 hash = pr.string ().hash ();
            total_hashes++;
            
            if (pr.Solution.Share.Nonce % display_increment == 0) {
                pr.Solution.Share.Timestamp.Value = initial_time + uint32 (Bitcoin::timestamp::now ().Value - local_initial_time);
                
                if (uint32 (pr.Solution.Share.Timestamp) - begin > max_time_seconds) return {};
            }
            
            if (hash < target) return pr;
            
            pr.Solution.Share.Nonce++;
            
            if (pr.Solution.Share.Nonce == 0) {
                extra_nonce_2++;
                std::copy (extra_nonce_2.begin (), extra_nonce_2.end (), pr.Solution.Share.ExtraNonce2.begin ());
            }
        }
        
        return pr;
    }
    
    std::map<digest256, working>::iterator random_select (random &r, jobs &j, double minimum_profitability) {
        
        if (j.size () == 0) return {};
        
        double normalization = 0;
        for (const auto &p : j) normalization += p.second.weight (minimum_profitability, .025);
        if (normalization == 0) return j.end ();
        double random = r.range01 () * normalization;
        
        double accumulated_profitability = 0;
        for (auto it = j.begin (); it != j.end (); it++) {
            accumulated_profitability += it->second.weight (minimum_profitability, .025);
            
            if (accumulated_profitability >= random) return it;
        }
        
        throw exception {"Warning: random_select failed to select job. "};
    }
    
    JSON solution_to_JSON (work::solution x) {
        
        JSON share {
            {"timestamp", data::encoding::hex::write (x.Share.Timestamp.Value)},
            {"nonce", data::encoding::hex::write (x.Share.Nonce)},
            {"extra_nonce_2", data::encoding::hex::write (x.Share.ExtraNonce2) }
        };
        
        if (x.Share.Bits) share["bits"] = data::encoding::hex::write (*x.Share.Bits);
        
        return JSON {
            {"share", share}, 
            {"extra_nonce_1", data::encoding::hex::write (x.ExtraNonce1)}
        };
    }
    
    work::proof solve (random &r, const work::puzzle& p, double max_time_seconds) {
        
        Stratum::session_id extra_nonce_1 {r.uint32 ()};
        uint64_big extra_nonce_2 {r.uint64 ()};
        
        work::solution initial {Bitcoin::timestamp::now (), 0, bytes_view (extra_nonce_2), extra_nonce_1};
        
        if (p.Mask != -1) initial.Share.Bits = r.uint32 ();
        
        return cpu_solve (p, initial, max_time_seconds);
        
    }
    
    Bitcoin::transaction redeem_puzzle (const Boost::puzzle &puzzle, const work::solution &solution, list<Bitcoin::output> pay) {
        bytes redeem_tx = puzzle.redeem (solution, pay);
        if (redeem_tx == bytes {}) return {};
        
        Bitcoin::transaction redeem {redeem_tx};
        
        Bitcoin::txid redeem_txid = redeem.id ();
        
        std::cout << "redeem tx generated: " << redeem_tx << std::endl;
        
        for (const Bitcoin::input &in : redeem.Inputs) {
            
            bytes redeem_script = in.Script;
            
            std::string redeemhex = data::encoding::hex::write (redeem_script);
            
            logger::log ("job.complete.redeemscript", JSON {
                {"solution", solution_to_JSON (solution)},
                {"asm", Bitcoin::ASM (redeem_script)},
                {"hex", redeemhex},
                {"txid", write (redeem_txid)}
            });
        }

        // the transaction 
        return redeem;
    }
    
    Bitcoin::transaction mine (
        random &r, 
        // an unredeemed Boost PoW output 
        const Boost::puzzle &puzzle, 
        // the address you want the bitcoins to go to once you have redeemed the boost output.
        // this is not the same as 'miner address'. This is just an address in your 
        // normal wallet and should not be the address that goes along with the key above.
        const digest160 &address,
        double fee_rate, 
        double max_time_seconds, 
        double minimum_price_per_difficulty_sats, 
        double maximum_mining_difficulty) {
        
        using namespace Bitcoin;
        std::cout << "mining on script " << puzzle.id () << std::endl;
        if (!puzzle.valid ()) throw string {"Boost puzzle is not valid"};
        
        Bitcoin::satoshi value = puzzle.value ();
        
        std::cout << "difficulty is " << puzzle.difficulty () << "." << std::endl;
        std::cout << "price per difficulty is " << puzzle.profitability () << "." << std::endl;
        
        // is the difficulty too high?
        if (maximum_mining_difficulty > 0, puzzle.difficulty () > maximum_mining_difficulty)
            std::cout << "warning: difficulty " << puzzle.difficulty () << " may be too high for CPU mining." << std::endl;
        
        // is the value in the output high enough? 
        if (puzzle.profitability () < minimum_price_per_difficulty_sats)
            std::cout << "warning: price per difficulty " << puzzle.profitability () << " may be too low." << std::endl;
        
        work::proof proof = solve (r, work::puzzle (puzzle), max_time_seconds);
        if (!proof.valid()) return {};
        
        bytes pay_script = pay_to_address::script (address);
        
        Bitcoin::satoshi fee {int64 (ceil (fee_rate * estimate_size (puzzle.expected_size (), pay_script.size ())))};
        
        if (fee > value) throw string {"Cannot pay tx fee with boost output"};
        
        return redeem_puzzle (puzzle, proof.Solution, {output {value - fee, pay_script}});
        
    }
    
    void mining_thread (work::selector *m, random *r, uint32 thread_number) {
        logger::log ("begin thread", JSON (thread_number));
        try {
            work::puzzle puzzle {};
            
            while (true) {
                puzzle = m->select ();
                if (!puzzle.valid ()) break;
                
                work::proof proof = solve (*r, puzzle, 10);
                if (proof.valid ()) {
                    logger::log ("solution found in thread", JSON (thread_number));
                    m->solved (proof.Solution);
                    logger::log ("solution submitted", JSON (thread_number));
                }
            }
        } catch (const std::exception &x) {
            std::cout << "Error " << x.what () << std::endl;
        }
        
        logger::log ("end thread", JSON (thread_number));
        delete r;
    }
    
    void multithreaded::start_threads () {
        if (Workers.size() != 0) return;
        std::cout << "starting " << Threads << " threads." << std::endl;
        for (int i = 1; i <= Threads; i++) 
            Workers.emplace_back (&mining_thread,
                &static_cast<work::selector &> (*this),
                new casual_random {Seed + i}, i);
    }
    
    multithreaded::~multithreaded () {
        pose ({});
        
        for (auto &thread : Workers) thread.join ();
    }
    
    void redeemer::solved (const work::solution &solution) {
        // shouldn't happen
        if (!solution.valid ()) return;
        
        if (work::proof {work::puzzle (Current.second), solution}.valid ()) submit(Current, solution);
        
        else if (work::proof {work::puzzle (Last.second), solution}.valid ()) submit(Last, solution);
    }
    
    void redeemer::mine (const std::pair<digest256, Boost::puzzle> &p) {
        std::unique_lock<std::mutex> lock (Mutex);
        if (Last == std::pair<digest256, Boost::puzzle> {}) Last = Current;
        Current = p;
        if (Current.second.valid ()) this->pose (work::puzzle (Current.second));
        else this->pose (work::puzzle {});
    }
    
    manager::manager (
        network &net, fees &f,
        const map_key_database &keys,
        address_source &addresses,
        uint64 random_seed, 
        double maximum_difficulty, 
        double minimum_profitability) : Mutex {},
        Net {net}, Fees {f}, Keys {keys}, Addresses {addresses},
        MaxDifficulty {maximum_difficulty}, MinProfitability {minimum_profitability},
        Random {random_seed}, Jobs {}, Redeemers {} {}
        
    int manager::add_new_miner (ptr<redeemer> r) {
        Redeemers.push_back (r);
        return Redeemers.size ();
    }
    
    void manager::run() {    
        while(true) {
            std::cout << "calling API" << std::endl;
            try {
                update_jobs (Net.jobs (100));
            } catch (const networking::HTTP::exception &exception) {
                std::cout << "API problem: " << exception.what () <<
                    "\n\tcall: " << exception.Request.Method << " " << exception.Request.Port << 
                    "://" << exception.Request.Host << exception.Request.Path << 
                    "\n\theaders: " << exception.Request.Headers << 
                    "\n\tbody: \"" << exception.Request.Body << "\"" << std::endl;
            }
            
            std::this_thread::sleep_for (std::chrono::seconds (90));
            
        }
    }
    
    void manager::select_job (int i) {
        
        if (Jobs.size () == 0) throw exception {"Warning: jobs is empty"};
        auto selected = random_select (Random, Jobs, MinProfitability);
        if (selected == Jobs.end ()) {

            logger::log ("worker.resting", JSON {
                {"thread", JSON (i)}
            });

            Redeemers[i - 1]->mine (std::pair<digest256, Boost::puzzle> {});

        } else {
            selected->second.Workers = selected->second.Workers << i;

            logger::log ("job.selected", JSON {
                {"thread", JSON (i)},
                {"script_hash", BoostPOW::write (selected->first)},
                {"value", int64 (selected->second.value ())},
                {"profitability", selected->second.profitability ()},
                {"difficulty", selected->second.difficulty ()}
            });

            Redeemers[i - 1]->mine (std::pair<digest256, Boost::puzzle>
                {selected->first, Boost::puzzle {selected->second,
                    selected->second.Script.Type == Boost::bounty ? Keys.next () : Keys[selected->second.Script.MinerPubkeyHash] }});
        }
        
    }
    
    void manager::update_jobs (const BoostPOW::jobs &j) {
        std::unique_lock<std::mutex> lock (Mutex);
        
        Jobs = j;
        uint32 count_jobs = Jobs.size ();
        if (count_jobs == 0) return;
        
        // remove jobs that are too difficult. 
        if (MaxDifficulty > 0) {
            for (auto it = Jobs.cbegin (); it != Jobs.cend ();)
                if (it->second.difficulty () > MaxDifficulty)
                    it = Jobs.erase (it);
                else ++it;
            
            std::cout << (count_jobs - Jobs.size ()) << " jobs removed due to high difficulty." << std::endl;
        }
        
        uint32 unprofitable_jobs = 0;
        for (auto it = Jobs.cbegin (); it != Jobs.cend ();)
            if (it->second.profitability () < MinProfitability) {
                unprofitable_jobs++;
                it = Jobs.erase (it);
            } else it++;
        
        uint32 profitable_jobs = Jobs.size ();
        std::cout << "found " << unprofitable_jobs << " unprofitable jobs. " << profitable_jobs << " jobs remaining " << std::endl;

        if (profitable_jobs == 0) return;

        uint32 contract_jobs = 0;
        uint32 impossible_contract_jobs = 0;
        for (auto it = Jobs.cbegin (); it != Jobs.cend ();)
            if (it->second.Script.Type == Boost::contract) {
                contract_jobs++;
                if (!Keys[it->second.Script.MinerPubkeyHash].valid ()) {
                    impossible_contract_jobs++;
                    it = Jobs.erase (it);
                } else it++;
            } else it++;

        std::cout << "of these, " << contract_jobs << " are contract jobs. Of those, "
            << (contract_jobs - impossible_contract_jobs) << " are jobs that we know how to work on, leaving "
            << (profitable_jobs - impossible_contract_jobs) << " total jobs available." << std::endl;

        if (profitable_jobs - impossible_contract_jobs == 0) return;
        
        // select a new job for all mining threads. 
        for (int i = 1; i <= Redeemers.size (); i++) select_job (i);
    }
    
    void manager::submit(const std::pair<digest256, Boost::puzzle> &puzzle, const work::solution &solution) {
        
        std::unique_lock<std::mutex> lock (Mutex);
        double fee_rate {Fees.get ()};
        
        auto value = puzzle.second.value ();
        bytes pay_script = pay_to_address::script (Addresses.next ().Digest);
        auto expected_inputs_size = puzzle.second.expected_size ();
        auto estimated_size = BoostPOW::estimate_size (expected_inputs_size, pay_script.size ());
        
        Bitcoin::satoshi fee {int64 (ceil (fee_rate * estimated_size))};
        
        if (fee > value) throw string {"Cannot pay tx fee with boost output"};
        
        auto redeem_tx = BoostPOW::redeem_puzzle (puzzle.second, solution, {Bitcoin::output{value - fee, pay_script}});
        
        auto w = Jobs.find (puzzle.first);
        if (w != Jobs.end ()) {

            auto redeem_bytes = bytes (redeem_tx);
            
            logger::log ("job.complete.transaction", JSON {
                {"txid", BoostPOW::write (redeem_tx.id ())},
                {"txhex", encoding::hex::write (redeem_bytes)}
            });
        
            if (!Net.broadcast_solution (redeem_bytes)) std::cout << "broadcast failed!" << std::endl;
            
            auto workers = w->second.Workers;
            Jobs.erase (w);
            for (int i : workers) select_job (i);
        }
        
    }
    
}
