#include <gigamonkey/script/typed_data_bip_276.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>
#include <sv/uint256.h>
#include <miner.hpp>
#include <logger.hpp>
#include <math.h>


#include <data/net/websocket.hpp>

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
        if (!puzzle.valid ()) throw data::exception {"Boost puzzle is not valid"};
        
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
        
        if (fee > value) throw data::exception {"Cannot pay tx fee with boost output"};
        
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
        Random {random_seed}, Jobs {}, Redeemers {}, Mining {false} {}
        
    int manager::add_new_miner (ptr<redeemer> r) {
        Redeemers.push_back (r);
        return Redeemers.size ();
    }

    void manager::select_job (int i) {

        if (Jobs.Jobs.size () == 0) {
            Mining = false;
            return;
        };

        auto selected = Jobs.random_select (Random, MinProfitability);
        if (selected == Jobs.Jobs.end ()) {

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
    
    void manager::run () {
        boost::asio::steady_timer timer (Net.IO);
        int count = 0;

        // we will call the API every few minutes.
        function<void (boost::system::error_code)> periodically =
            [self = this->shared_from_this (), &periodically, &timer, &count]
            (boost::system::error_code err) {
            if (err) throw exception {} << "unknown error: " << err;

            if (count % 30 == 0) {

                std::cout << "About to call jobs API " << std::endl;
                try {
                    self->update_jobs (self->Net.jobs (100));
                } catch (const net::HTTP::exception &exception) {
                    std::cout << "API problem: " << exception.what () <<
                        "\n\tcall: " << exception.Request.Method << " " << exception.Request.URL.port () <<
                        "://" << exception.Request.URL.host () << exception.Request.URL.path () <<
                        "\n\theaders: " << exception.Request.Headers <<
                        "\n\tbody: \"" << exception.Request.Body << "\"" << std::endl;
                } catch (const std::exception &exception) {
                    std::cout << "Problem: " << exception.what () << std::endl;
                } catch (...) {
                    std::cout << "something went wrong: " << std::endl;
                    return;
                }

                std::cout << "about to wait another 15 minutes" << std::endl;
            } else {
                self->select_job (self->Random.uint32 (self->Redeemers.size () - 1));
            }

            count++;
            timer.expires_after (boost::asio::chrono::seconds (30));
            timer.async_wait (periodically);
        };

        struct handlers : pow_co::websockets_protocol_handlers {
            manager &Manager;

            handlers (manager &m) : Manager {m} {}

            void job_created (const Boost::prevout &p) {
                std::cout << "new boost prevout received via websockets: " << p << std::endl;
                Manager.new_job (p);
            };
        };

        try {
            std::cout << "making initial jobs call " << std::endl;

            // get started.
            periodically (boost::system::error_code {});

            std::cout << "set up websockets..." << std::endl;

            // set up websockets.
/*
            Net.PowCo.connect ([] (boost::system::error_code err) {
                    throw exception {} << "websockets error " << err;
                },
                [] () {},
                [self = this->shared_from_this ()] (ptr<net::session<const JSON &>> o) {
                    std::cout << "setting up handlers for websockets" << std::endl;
                    return std::static_pointer_cast<pow_co::websockets_protocol_handlers> (
                        ptr<handlers> {new handlers {*self}});
                });*/

            net::websocket::open (Net.IO,
                net::URL {net::protocol::WS, "5201", "pow.co", "/"},
                nullptr,
                [] (boost::system::error_code err) {
                    throw exception {} << "websockets error " << err;
                }, [] () {},
                [self = this->shared_from_this ()] (ptr<net::session<const string &>> o) {
                    return [self] (string_view x) {
                        auto j = JSON::parse (x);
                        std::cout << "read websockets message " << j << std::endl;
                        if (!pow_co::websockets_protocol_message::valid (j))
                            std::cout << "invalid websockets message received: " << j << std::endl;
                        else if (j["type"] == "boostpow.job.created") {
                            if (auto prevout = pow_co::websockets_protocol_message::job_created (j["content"]);
                                bool (prevout)) self->new_job (*prevout);
                            else std::cout << "could not read websockets message " << j["content"] << std::endl;
                        } else if (j["type"] == "boostpow.proof.created") {
                            if (auto outpoint = pow_co::websockets_protocol_message::proof_created (j["content"]);
                                bool (outpoint)) self->solved_job (*outpoint);
                            else std::cout << "could not read websockets message " << j["content"] << std::endl;
                        } else std::cout << "unknown message received: " << j << std::endl;
                    };
                });

            Net.IO.run ();
        } catch (const std::exception &e) {
            std::cout << "caught exception: " << e.what () << std::endl;
        } catch (...) {
            std::cout << "caught some kind of other thing" << std::endl;
        }

    }

    void manager::new_job (const Boost::prevout &p) {
        std::unique_lock<std::mutex> lock (Mutex);

        if (p.difficulty () > MaxDifficulty) return;

        if (p.profitability () < MinProfitability) {
            if (auto it = Jobs.Jobs.find (p.id ()); it == Jobs.Jobs.end()) return;
        }

        Jobs.add_prevout (p);
        std::cout << "new job added" << std::endl;

        if (Mining = false) {
            Mining = true;

            // select a new job for all mining threads.
            for (int i = 1; i <= Redeemers.size (); i++) select_job (i);
        }
    }

    void manager::solved_job (const Bitcoin::outpoint &o) {
        std::unique_lock<std::mutex> lock (Mutex);

        if (auto x = Jobs.Scripts.find (o); x != Jobs.Scripts.end ()) {
            if (auto w = Jobs.Jobs.find (x->second); w != Jobs.Jobs.end ()) {
                if (data::size (w->second.Prevouts) == 1) {
                    auto workers = w->second.Workers;
                    Jobs.Jobs.erase (w);
                    for (int i : workers) select_job (i);
                } else {
                    set<Boost::candidate::prevout> new_prevouts {};
                    for (const auto &p : w->second.Prevouts.values ())
                        if (static_cast<Bitcoin::outpoint> (p) != o) new_prevouts = new_prevouts.insert (p);
                    w->second.Prevouts = new_prevouts;
                }

            }

            Jobs.Scripts.erase (x);
        }

    }
    
    void manager::update_jobs (const BoostPOW::jobs &j) {
        std::unique_lock<std::mutex> lock (Mutex);
        
        Jobs = j;
        uint32 total_jobs = Jobs.Jobs.size ();
        if (total_jobs == 0) return;

        uint32 difficult_jobs = 0;
        std::cout << "updating jobs" << std::endl;
        
        // remove jobs that are too difficult. 
        if (MaxDifficulty > 0) difficult_jobs = Jobs.remove (
            [MaxDifficulty = this->MaxDifficulty]
            (const BoostPOW::working &x) -> bool {
            return x.difficulty () > MaxDifficulty;
        });

        std::cout << difficult_jobs << " jobs removed due to high difficulty." << std::endl;

        // remove jobs whose value is too low.
        uint32 tiny_jobs = difficult_jobs = Jobs.remove (
            [] (const BoostPOW::working &x) -> bool {
            return x.value () < 100;
        });

        std::cout << tiny_jobs << " jobs removed due to low value." << std::endl;

        uint32 unprofitable_jobs = Jobs.remove (
            [MinProfitability = this->MinProfitability]
            (const BoostPOW::working &x) -> bool {
            return x.profitability () < MinProfitability;
        });
        
        uint32 profitable_jobs = Jobs.Jobs.size ();
        std::cout << "found " << unprofitable_jobs << " unprofitable jobs. " << profitable_jobs << " jobs remaining " << std::endl;

        if (profitable_jobs == 0) return;

        uint32 contract_jobs = 0;
        uint32 impossible_contract_jobs = 0;
        for (auto it = Jobs.Jobs.cbegin (); it != Jobs.Jobs.cend ();)
            if (it->second.Script.Type == Boost::contract) {
                contract_jobs++;
                if (!Keys[it->second.Script.MinerPubkeyHash].valid ()) {
                    impossible_contract_jobs++;
                    it = Jobs.Jobs.erase (it);
                } else it++;
            } else it++;

        std::cout << "of these, " << contract_jobs << " are contract jobs. Of those, "
            << (contract_jobs - impossible_contract_jobs) << " are jobs that we know how to work on, leaving "
            << (profitable_jobs - impossible_contract_jobs) << " total jobs available." << std::endl;

        if (profitable_jobs - impossible_contract_jobs == 0) return;
        
        Mining = true;

        // select a new job for all mining threads. 
        for (int i = 1; i <= Redeemers.size (); i++) select_job (i);
    }

    void manager::submit (const std::pair<digest256, Boost::puzzle> &puzzle, const work::solution &solution) {
        
        std::unique_lock<std::mutex> lock (Mutex);
        double fee_rate {Fees.get ()};
        
        auto value = puzzle.second.value ();
        bytes pay_script = pay_to_address::script (Addresses.next ().Digest);
        auto expected_inputs_size = puzzle.second.expected_size ();
        auto estimated_size = BoostPOW::estimate_size (expected_inputs_size, pay_script.size ());
        std::cout << "redeeming tx; fee rate is " << fee_rate << "; value is " <<
            value << "; estimated size is " << estimated_size <<  std::endl;
        if (fee_rate <= .0001) throw "error: fee rate too small";
        Bitcoin::satoshi fee {int64 (ceil (fee_rate * estimated_size))};
        
        if (fee > value) throw data::exception {"Cannot pay tx fee with boost output"};
        
        auto redeem_tx = BoostPOW::redeem_puzzle (puzzle.second, solution, {Bitcoin::output {value - fee, pay_script}});
        
        auto w = Jobs.Jobs.find (puzzle.first);
        if (w != Jobs.Jobs.end ()) {

            auto redeem_bytes = bytes (redeem_tx);
            
            logger::log ("job.complete.transaction", JSON {
                {"txid", BoostPOW::write (redeem_tx.id ())},
                {"txhex", encoding::hex::write (redeem_bytes)}
            });
        
            if (!Net.broadcast_solution (redeem_bytes)) std::cout << "broadcast failed!" << std::endl;
            
            auto workers = w->second.Workers;
            Jobs.Jobs.erase (w);
            for (int i : workers) select_job (i);
        }
        
    }
    
}
