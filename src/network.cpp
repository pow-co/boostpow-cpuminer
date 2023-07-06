
#include <miner.hpp>
#include <logger.hpp>
#include <mutex>
#include <iomanip>

std::mutex Mutex;

BoostPOW::network::broadcast_error BoostPOW::network::broadcast (const bytes &tx) {
    std::lock_guard<std::mutex> lock (Mutex);
    std::cout << "broadcasting tx " << std::endl;
    
    bool broadcast_whatsonchain; 
    bool broadcast_gorilla;
    bool broadcast_pow_co;
    
    try {
        broadcast_pow_co = PowCo.broadcast (tx);
    } catch (net::HTTP::exception ex) {
        std::cout << "exception caught broadcasting powco: " << ex.what () << std::endl;
        broadcast_pow_co = false;
    }
    
    try {
        broadcast_whatsonchain = WhatsOnChain.transaction ().broadcast (tx);
    } catch (net::HTTP::exception ex) {
        std::cout << "exception caught broadcasting whatsonchain." << ex.what () << std::endl;
        broadcast_whatsonchain = false;
    }
    
    try {
        auto broadcast_result = Gorilla.submit_transaction({tx});
        broadcast_gorilla = broadcast_result.ReturnResult == BitcoinAssociation::MAPI::success;
        if (!broadcast_gorilla) std::cout << "Gorilla broadcast description: " << broadcast_result.ResultDescription << std::endl; 
    } catch (net::HTTP::exception ex) {
        std::cout << "exception caught broadcasting gorilla: " << ex.what() << "; response code = " << ex.Response.Status << std::endl;
        broadcast_gorilla = false;
    }
    
    std::cout << "broadcast results: Whatsonchain = " << std::boolalpha << 
        broadcast_whatsonchain << "; gorilla = "<< broadcast_gorilla << "; pow_co = " << broadcast_pow_co << std::endl;
    
    // we don't count whatsonchain because that one seems to return false positives a lot. 
    return broadcast_gorilla || broadcast_pow_co ? broadcast_error::none : broadcast_error::unknown;
}

bytes BoostPOW::network::get_transaction (const Bitcoin::txid &txid) {
    static map<Bitcoin::txid, bytes> cache;
    
    auto known = cache.contains (txid);
    if (known) return *known;
    
    bytes tx = WhatsOnChain.transaction ().get_raw (txid);
    
    if (tx != bytes {}) cache = cache.insert (txid, tx);
    
    return tx;
}

// transactions by txid
map<Bitcoin::txid, bytes> Transaction;

// script histories by script hash
map<digest256, list<Bitcoin::txid>> History;

BoostPOW::jobs BoostPOW::network::jobs (uint32 limit, double max_difficulty, int64 min_value) {
    
    std::lock_guard<std::mutex> lock (Mutex);
    const list<Boost::prevout> jobs_api_call {PowCo.jobs (limit, max_difficulty)};
    
    BoostPOW::jobs Jobs {};
    
    uint32 count_closed_jobs = 0;
    uint32 count_jobs_with_multiple_outputs = 0;
    uint32 count_open_jobs = 0;
    uint32 count_low_value_jobs = 0;
    
    json::array_t redemptions;
    
    std::map<digest256, list<Boost::prevout>> prevouts;
    
    std::cout << "Jobs returned from API: " << jobs_api_call.size () << std::endl;
    
    // organize all jobs in terms of script hash. 
    for (const Boost::prevout &job : jobs_api_call) {
        digest256 script_hash = job.id ();
        if (auto j = prevouts.find (script_hash); j != prevouts.end ()) j->second <<= job; 
        else prevouts[script_hash] = list<Boost::prevout> {job};
    }
    
    std::cout << "found " << prevouts.size () << " separate scripts." << std::endl;
    
    // check on jobs that have been closed and are incorrectly being returned.
    auto count_closed_job = [this, &count_closed_jobs, &redemptions] (const Boost::prevout &job) -> void {
        std::cout << "  reporting closed job " << job << std::endl;
        count_closed_jobs++;
        
        inpoint in;

        // this usually doesn't work.
        try {
            in = PowCo.spends (job.outpoint ());
        } catch (const net::HTTP::exception &exception) {
            // continue if this call fails, as it is not essential. 
            std::cout << "API problem: " << exception.what () <<
                "\n\tcall: " << exception.Request.Method << " " << exception.Request.URL <<
                "\n\theaders: " << exception.Request.Headers << 
                "\n\tbody: \"" << exception.Request.Body << "\"" << std::endl;
        }
        
        // if it fails, use whatsonchain.
        if (!in.valid ()) {
            auto script_hash = job.id ();
            
            auto history = History.contains (script_hash);
            
            if (!history) {
                History = History.insert (script_hash, WhatsOnChain.script ().get_history (script_hash));
                history = History.contains (script_hash);
            }
            
            for (const Bitcoin::txid &redeem_txid : *history) {
                Bitcoin::transaction redeem_tx;

                auto tx = Transaction.contains (redeem_txid);
                if (!tx) {
                    redeem_tx = Bitcoin::transaction (get_transaction (redeem_txid));
                    Transaction = Transaction.insert (redeem_txid, bytes (redeem_tx));
                } else redeem_tx = Bitcoin::transaction (*tx);

                if (!redeem_tx.valid ()) continue;
                
                uint32 ii = 0;
                for (const Bitcoin::input &in: redeem_tx.Inputs) if (in.Reference == job.outpoint ()) {

                    bytes spend_tx;

                    auto tx = Transaction.contains (in.Reference.Digest);
                    if (!tx) {
                        spend_tx = get_transaction (in.Reference.Digest);
                        Transaction = Transaction.insert (in.Reference.Digest, spend_tx);
                    } else spend_tx = *tx;
                    
                    std::cout << "spend tx found: " << redeem_txid << std::endl;
                    
                    redemptions.push_back (json {
                        {"outpoint", write (job.outpoint ())},
                        {"inpoint", write (Bitcoin::outpoint {redeem_txid, ii++})},
                        {"script_hash", write (script_hash)}/*,
                        {"spend", encoding::hex::write (spend_tx)},
                        {"redeem", encoding::hex::write (bytes (redeem_tx))}*/
                    });
                    
                    PowCo.submit_proof (bytes (redeem_tx));
                    break;
                }
                
            } 
            
        }
    };
    
    int i = 0;
    for (const auto &pair : prevouts) {
        auto script_hash = pair.first;
        
        std::cout << "  checking script " << i << " of " << prevouts.size () << " with hash " << pair.first << std::endl;
        i++;
        
        list<UTXO> script_utxos = WhatsOnChain.script ().get_unspent (script_hash);
        std::cout << "  got unspent scripts " << script_utxos << std::endl;
        list<Boost::prevout> unspent;
        
        for (const Boost::prevout &p : pair.second) {
            std::cout << "    " << p.outpoint () << std::endl;
            bool closed = true;
            
            for (const UTXO &u : script_utxos) if (u.Outpoint == p.outpoint ()) {
                closed = false;
                continue;
            }
            
            if (closed) count_closed_job (p);
            else unspent <<= p;
        }
        
        if (!data::empty (unspent)) {
            Jobs.add_script (unspent.first ().script ());
            
            for (const Boost::prevout &p : unspent) {
                count_open_jobs++;
                
                if (p.value () < min_value) count_low_value_jobs++;
                else Jobs.add_prevout (p);
            } 
            
            if (data::size (unspent) > 1) count_jobs_with_multiple_outputs++;
        }
        
    }
    
    logger::log ("api.jobs.report", json {
        {"jobs_returned_by_API", jobs_api_call.size ()},
        {"jobs_not_already_redeemed", count_open_jobs}, 
        {"jobs_already_redeemed", count_closed_jobs}, 
        {"jobs_with_multiple_outputs", count_jobs_with_multiple_outputs}, 
        {"jobs_with_low_value", count_low_value_jobs}, 
        {"redemptions", redemptions}, 
        {"valid_jobs", JSON (Jobs)}
    });
    
    return Jobs;
    
}

satoshi_per_byte BoostPOW::network::mining_fee () {
    std::lock_guard<std::mutex> lock (Mutex);
    auto z = Gorilla.get_fee_quote ();
    if (!z.valid ()) throw exception {} << "invalid fee quote response received: " << string (JSON (z));
    auto j = JSON (z);
    
    return z.Fees["standard"].MiningFee;
}

Boost::candidate BoostPOW::network::job (const Bitcoin::outpoint &o) {
    // check for job at pow co. 
    Boost::candidate x {{PowCo.job (o)}};
    // check for job with whatsonchain.
    auto script_hash = x.id ();
    
    auto script_utxos = WhatsOnChain.script ().get_unspent (script_hash);
    
    // is the current job in the list from whatsonchain? 
    bool match_found = false;
    
    for (auto const &u : script_utxos) {
        x = x.add (Boost::prevout {u.Outpoint, Boost::output {u.Value, x.Script}});
        
        if (u.Outpoint == o) match_found = true;
    }
    
    // register job at pow co. 
    if (!match_found) {
        auto inpoint = PowCo.spends (o);
        
        if (!inpoint.valid ()) {
            
            auto history = WhatsOnChain.script ().get_history (script_hash);
            
            for (const auto &history_txid : history) {
                Bitcoin::transaction history_tx {get_transaction (history_txid)};
                if (!history_tx.valid ()) continue;
                for (const Bitcoin::input &in: history_tx.Inputs) if (in.Reference == o) {
                    PowCo.submit_proof (bytes (history_tx));
                    break;
                }
                
            } 
            
        }
    }
    
    return x;
}

double BoostPOW::network::price (tm time) {

    std::stringstream ss;
    ss << time.tm_mday << "-" << (time.tm_mon + 1) << "-" << (time.tm_year + 1900);

    string date = ss.str ();

    std::cout << "   about to get BSV price in USD at date " << date << std::endl;

    auto request = CoinGecko.REST.GET ("/api/v3/coins/bitcoin-cash-sv/history", {
        entry<data::UTF8, data::UTF8> {"date", date },
        entry<data::UTF8, data::UTF8> {"localization", "false" }
    });

    // the rate limitation for this call is hard to understand.
    // If it doesn't work we wait 30 seconds.
    while (true) {

        auto response = CoinGecko (request);

        if (response.Status == net::HTTP::status::ok) {
            JSON info = JSON::parse (response.Body);
            return info["market_data"]["current_price"]["usd"];
        }

        net::asio::io_context io {};
        net::asio::steady_timer {io, net::asio::chrono::seconds (30)}.wait ();

    }
}
