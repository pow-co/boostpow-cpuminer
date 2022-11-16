
#include <miner.hpp>
#include <logger.hpp>
#include <mutex>

std::mutex Mutex;

BoostPOW::network::broadcast_error BoostPOW::network::broadcast(const bytes &tx) {
    std::lock_guard<std::mutex> lock(Mutex);
    std::cout << "broadcasting tx " << std::endl;
    
    bool broadcast_whatsonchain; 
    bool broadcast_gorilla;
    bool broadcast_pow_co;
    
    try {
        broadcast_pow_co = PowCo.broadcast(tx);
    } catch (networking::HTTP::exception ex) {
        std::cout << "exception caught broadcasting powco: " << ex.what() << std::endl;
        broadcast_pow_co = false;
    }
    
    try {
        broadcast_whatsonchain = WhatsOnChain.transaction().broadcast(tx); 
    } catch (networking::HTTP::exception ex) {
        std::cout << "exception caught broadcasting whatsonchain." << ex.what() << std::endl;
        broadcast_whatsonchain = false;
    }
    
    try {
        auto broadcast_result = Gorilla.submit_transaction({tx});
        broadcast_gorilla = broadcast_result.ReturnResult == BitcoinAssociation::MAPI::success;
        if (!broadcast_gorilla) std::cout << "Gorilla broadcast description: " << broadcast_result.ResultDescription << std::endl; 
    } catch (networking::HTTP::exception ex) {
        std::cout << "exception caught broadcasting gorilla: " << ex.what() << "; response code = " << ex.Response.Status << std::endl;
        broadcast_gorilla = false;
    }
    
    std::cout << "broadcast results: Whatsonchain = " << std::boolalpha << 
        broadcast_whatsonchain << "; gorilla = "<< broadcast_gorilla << "; pow_co = " << broadcast_pow_co << std::endl;
    
    // we don't count whatsonchain because that one seems to return false positives a lot. 
    return broadcast_gorilla || broadcast_pow_co ? broadcast_error::none : broadcast_error::unknown;
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
    std::lock_guard<std::mutex> lock(Mutex);
    const list<Boost::prevout> jobs_api_call{PowCo.jobs()};
    
    BoostPOW::jobs Jobs{};
    uint32 count_closed_jobs = 0;
    std::map<digest256, list<Bitcoin::txid>> script_histories;
    json::array_t redemptions;
    
    auto count_closed_job = [this, &count_closed_jobs, &script_histories, &redemptions](const Boost::prevout &job) -> void {
        count_closed_jobs++;
        
        auto inpoint = PowCo.spends(job.outpoint());
        
        if (!inpoint.valid()) {
            auto script_hash = job.id();
            
            auto history = script_histories.find(script_hash);
            
            if (history == script_histories.end()) {
                script_histories[script_hash] = WhatsOnChain.script().get_history(script_hash);
                history = script_histories.find(script_hash);
            }
            
            for (const Bitcoin::txid &history_txid : history->second) {
                Bitcoin::transaction history_tx{get_transaction(history_txid)};
                if (!history_tx.valid()) continue;
                
                uint32 ii = 0;
                for (const Bitcoin::input &in: history_tx.Inputs) if (in.Reference == job.outpoint()) {
                    
                    redemptions.push_back(json{
                        {"outpoint", write(job.outpoint())}, 
                        {"inpoint", write(Bitcoin::outpoint{history_txid, ii++})}, 
                        {"script_hash", write(script_hash)}});
                    
                    PowCo.submit_proof(history_txid);
                    break;
                }
                
            } 
            
        } else PowCo.submit_proof(inpoint.Digest);
    };
    
    for (const Boost::prevout &job : jobs_api_call) {
    
        digest256 script_hash = job.id();
        
        if (auto j = Jobs.find(script_hash); j != Jobs.end()) {
            
            bool closed_job = true;
            for (const auto &u : j->second.Prevouts.values()) if (static_cast<Bitcoin::outpoint>(u) == job.outpoint()) {
                closed_job = false;
                break;
            }
            
            if (closed_job) count_closed_job(job);
            
            continue;
        }
        
        Jobs.add_script(job.script());
        
        auto script_utxos = WhatsOnChain.script().get_unspent(script_hash);
        
        // is the current job in the list from whatsonchain? 
        bool match_found = false;
        
        for (auto const &u : script_utxos) {
            Boost::prevout p{u.Outpoint, Boost::output{u.Value, job.script()}};
            Jobs.add_prevout(script_hash, p);
            
            if (u.Outpoint == job.outpoint()) match_found = true;
        }
        
        if (!match_found) count_closed_job(job);
        
    }
    
    uint32 count_jobs_with_multiple_outputs = 0;
    uint32 count_open_jobs = 0;
    
    for (auto it = Jobs.cbegin(); it != Jobs.cend();) 
        if (it->second.Prevouts.size() == 0) it = Jobs.erase(it);
        else {
            count_open_jobs++;
            if (it->second.Prevouts.size() > 1) count_jobs_with_multiple_outputs++;
            ++it;
        }
    
    logger::log("api.jobs.report", json {
        {"jobs_returned_by_API", jobs_api_call.size()},
        {"jobs_not_already_redeemed", count_open_jobs}, 
        {"jobs_already_redeemed", count_closed_jobs}, 
        {"jobs_with_multiple_outputs", count_jobs_with_multiple_outputs}, 
        {"redemptions", redemptions}, 
        {"valid_jobs", JSON(Jobs)}
    });
    
    return Jobs;
    
}

satoshi_per_byte BoostPOW::network::mining_fee() {
    std::lock_guard<std::mutex> lock(Mutex);
    auto z = Gorilla.get_fee_quote();
    auto j = JSON(z);
    
    std::cout << "Fee quote is " << j << std::endl;
    
    return z.Fees["standard"].MiningFee;
}

Boost::candidate BoostPOW::network::job(const Bitcoin::outpoint &o) {
    // check for job at pow co. 
    Boost::candidate x{{PowCo.job(o)}};
    // check for job with whatsonchain.
    auto script_hash = x.id();
    
    auto script_utxos = WhatsOnChain.script().get_unspent(script_hash);
    
    // is the current job in the list from whatsonchain? 
    bool match_found = false;
    
    for (auto const &u : script_utxos) {
        x = x.add(Boost::prevout{u.Outpoint, Boost::output{u.Value, x.Script}});
        
        if (u.Outpoint == o) match_found = true;
    }
    
    // register job at pow co. 
    if (!match_found) {
        auto inpoint = PowCo.spends(o);
        
        if (!inpoint.valid()) {
            
            auto history = WhatsOnChain.script().get_history(script_hash);
            
            for (const auto &history_txid : history) {
                Bitcoin::transaction history_tx{get_transaction(history_txid)};
                if (!history_tx.valid()) continue;
                for (const Bitcoin::input &in: history_tx.Inputs) if (in.Reference == o) {
                    PowCo.submit_proof(history_txid);
                    break;
                }
                
            } 
            
        } else PowCo.submit_proof(inpoint.Digest);
    }
    
    return x;
}

