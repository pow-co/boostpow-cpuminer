
#include <network.hpp>
#include <logger.hpp>

bool BoostPOW::network::broadcast(const bytes &tx) {
    return WhatsOnChain.transaction().broadcast(tx) || 
        Gorilla.submit_transaction({tx}).ReturnResult == BitcoinAssociation::MAPI::success ||
        PowCo.broadcast(tx);
}

bytes BoostPOW::network::get_transaction(const Bitcoin::txid &txid) {
    static map<Bitcoin::txid, bytes> cache;
    
    auto known = cache.contains(txid);
    if (known) return *known;
    
    bytes tx = WhatsOnChain.transaction().get_raw(txid);
    
    if (tx != bytes{}) cache = cache.insert(txid, tx);
    
    return tx;
}

BoostPOW::jobs BoostPOW::network::jobs(uint32 limit) {
    
    const list<Boost::prevout> jobs_api_call{PowCo.jobs()};
    
    std::cout << "read " << jobs_api_call.size() << " jobs from pow.co/api/v1/jobs/" << std::endl;
    
    BoostPOW::jobs Jobs{};
    uint32 count_closed_jobs = 0;
    std::map<digest256, list<Bitcoin::txid>> script_histories;
    
    auto count_closed_job = [this, &count_closed_jobs, &script_histories](const Boost::prevout &job) -> void {
        count_closed_jobs++;
        
        std::cout << " checking script redemption against pow.co/api/v1/spends/ ...";
        
        auto inpoint = PowCo.spends(job.outpoint());
        
        if (!inpoint.valid()) {
            std::cout << " No redemption found at pow.co." << std::endl;
            std::cout << " checking whatsonchain history." << std::endl;
            
            auto script_hash = job.id();
            
            auto history = script_histories.find(script_hash);
            
            if (history == script_histories.end()) {
                script_histories[script_hash] = WhatsOnChain.script().get_history(script_hash);
                history = script_histories.find(script_hash);
            }
            
            std::cout << " " << history->second.size() << " in history; " << std::endl;
            
            for (const Bitcoin::txid &history_txid : history->second) {
                Bitcoin::transaction history_tx{get_transaction(history_txid)};
                if (!history_tx.valid()) std::cout << "  could not find tx " << history_txid << std::endl;
                
                for (const Bitcoin::input &in: history_tx.Inputs) if (in.Reference == job.outpoint()) {
                    std::cout << "  Redemption found on whatsonchain.com at " << history_txid << std::endl;
                    PowCo.submit_proof(history_txid);
                    break;
                }
                
            } 
            
            std::cout << " no redemption found on whatsonchain.com " << std::endl;
            
        } else {
            std::cout << " Redemption found on pow_co; submitting proof to pow_co" << std::endl;
            PowCo.submit_proof(inpoint.Digest);
        }
        
    };
    
    for (const Boost::prevout &job : jobs_api_call) {
        digest256 script_hash = job.id();
        
        std::cout << " Checking script with hash " << script_hash << " in " << job.outpoint() << std::endl;
        
        if (auto j = Jobs.find(script_hash); j != Jobs.end()) {
            std::cout << " hash has already been found." << std::endl;
            
            bool closed_job = true;
            for (const auto &u : j->second.UTXOs) if (u.Outpoint == job.outpoint()) {
                closed_job = false;
                break;
            }
            
            if (closed_job) count_closed_job(job);
            
            continue;
        }
        
        Jobs.add_script(job.script());
        
        std::cout << " Checking on whatsonchain.com" << std::endl;
        
        auto script_utxos = WhatsOnChain.script().get_unspent(script_hash);
        
        // is the current job in the list from whatsonchain? 
        bool match_found = false;
        
        std::cout << " whatsonchain.com found " << script_utxos.size() << " utxos for script " << script_hash << std::endl;
        
        for (auto const &u : script_utxos) {
            Jobs.add_utxo(script_hash, u);
            
            if (u.Outpoint == job.outpoint()) {
                match_found = true;
                break;
            }
        }
        
        if (!match_found) {
            std::cout << " warning: " << job.outpoint() << " not found in whatsonchain utxos" << std::endl;
            
            count_closed_job(job);
        }
        
    }
    
    std::cout << "found " << Jobs.size() << " jobs that are not redeemed already" << std::endl;
    
    uint32 count_jobs_with_multiple_outputs = 0;
    uint32 count_open_jobs = 0;
    
    for (const auto &e : Jobs) {
        if (e.second.UTXOs.size() > 0) count_open_jobs++;
        if (e.second.UTXOs.size() > 1) count_jobs_with_multiple_outputs++;
    }
    
    logger::log("api.jobs.report", json {
        {"jobs_returned_by_API", jobs_api_call.size()},
        {"jobs_not_already_redeemed", count_open_jobs}, 
        {"jobs_already_redeemed", count_closed_jobs}, 
        {"jobs_with_multiple_outputs", count_jobs_with_multiple_outputs}
    });
    
    return Jobs;
    
}

