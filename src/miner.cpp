#include <miner.hpp>
#include <logger.hpp>
#include <random.hpp>

const double maximum_mining_difficulty = 2.2;
const double minimum_price_per_difficulty_sats = 1000000;

Bitcoin::satoshi calculate_fee(
    size_t inputs_size, 
    size_t pay_script_size, 
    double fee_rate) {

    return inputs_size                              // inputs
        + 4                                         // tx version
        + 1                                         // var int value 1 (number of outputs)
        + 8                                         // satoshi value size
        + Bitcoin::var_int::size(pay_script_size)   // size of output script size
        + pay_script_size                           // output script size
        + 4;                                        // locktime

}

// A cpu miner function. 
work::proof cpu_solve(const work::puzzle& p, const work::solution& initial, double max_time_seconds) {
    using uint256 = Gigamonkey::uint256;
    
    //if (initial.Share.ExtraNonce2.size() != 4) throw "Extra nonce 2 must have size 4. We will remove this limitation eventually.";
    
    uint64_big extra_nonce_2; 
    std::copy(initial.Share.ExtraNonce2.begin(), initial.Share.ExtraNonce2.end(), extra_nonce_2.begin());
    
    uint256 target = p.Candidate.Target.expand();
    if (target == 0) return {};
    //std::cout << " working " << p << std::endl;
    //std::cout << " with target " << target << std::endl;
    
    uint256 best{"0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
    
    N total_hashes{0};
    N nonce_increment{"0x0100000000"};
    uint32 display_increment = 0x00800000;
    
    work::proof pr{p, initial};
    
    uint32 begin{Bitcoin::timestamp::now()};
    
    while(true) {
        uint256 hash = pr.string().hash();
        total_hashes++;
        
        if (hash < best) {
            best = hash;

            logger::log("besthash", json {
              {"hash", best},
              {"total", uint64(total_hashes)}
            });

        } else if (pr.Solution.Share.Nonce % display_increment == 0) {
            pr.Solution.Share.Timestamp = Bitcoin::timestamp::now();
            
            if (uint32(pr.Solution.Share.Timestamp) - begin > max_time_seconds) return {};
        }
        
        if (hash < target) {
            return pr;
        }
        
        pr.Solution.Share.Nonce++;
        
        if (pr.Solution.Share.Nonce == 0) {
            extra_nonce_2++;
            std::copy(extra_nonce_2.begin(), extra_nonce_2.end(), pr.Solution.Share.ExtraNonce2.begin());
        }
    }
    
    return pr;
}

boost_prevouts select(map<digest256, boost_prevouts> puzzles, double minimum_profitability) {
    
    if (puzzles.size() == 0) return {};
    
    double total_profitability = 0;
    for (const auto &p : puzzles) if (p.Value.profitability() > minimum_profitability) 
        total_profitability += (p.Value.profitability() - minimum_profitability);
    
    double random = random_range01(data::get_random_engine()) * total_profitability;
    
    double accumulated_profitability = 0;
    for (const auto &p : puzzles) if (p.Value.profitability() > minimum_profitability) {
        accumulated_profitability += (p.Value.profitability() - minimum_profitability);
        
        if (accumulated_profitability >= random) return p.Value;
    }
    
    // shouldn't happen. 
    return {};
}

json solution_to_json(work::solution x) {

    json share {
        {"timestamp", data::encoding::hex::write(x.Share.Timestamp.Value)},
        {"nonce", data::encoding::hex::write(x.Share.Nonce)},
        {"extra_nonce_2", data::encoding::hex::write(x.Share.ExtraNonce2) }
    };
    
    if (x.Share.Bits) share["bits"] = data::encoding::hex::write(*x.Share.Bits);
    
    return json {
        {"share", share}, 
        {"extra_nonce_1", data::encoding::hex::write(x.ExtraNonce1)}
    };
}

bytes mine(
    // an unredeemed Boost PoW output 
    Boost::puzzle puzzle, 
    // the address you want the bitcoins to go to once you have redeemed the boost output.
    // this is not the same as 'miner address'. This is just an address in your 
    // normal wallet and should not be the address that goes along with the key above.
    Bitcoin::address address, 
    double max_time_seconds) {
    
    using namespace Bitcoin;
    std::cout << "mining on script " << puzzle.id() << std::endl;
    if (!puzzle.valid()) throw string{"Boost puzzle is not valid"};
    
    Bitcoin::satoshi value = puzzle.value();
    
    std::cout << "difficulty is " << puzzle.difficulty() << "." << std::endl;
    std::cout << "price per difficulty is " << puzzle.profitability() << "." << std::endl;
    
    // is the difficulty too high?
    if (puzzle.difficulty() > maximum_mining_difficulty) 
      std::cout << "warning: difficulty " << puzzle.difficulty() << " may be too high for CPU mining." << std::endl;
    
    // is the value in the output high enough? 
    if (puzzle.profitability() < minimum_price_per_difficulty_sats)
      std::cout << "warning: price per difficulty " << puzzle.profitability() << " may be too low." << std::endl;
    
    auto generator = data::get_random_engine();
    
    Stratum::session_id extra_nonce_1{random_uint32(generator)};
    uint64_big extra_nonce_2{random_uint64(generator)};
    
    work::solution initial{timestamp::now(), 0, bytes_view(extra_nonce_2), extra_nonce_1};
    
    if (puzzle.use_general_purpose_bits()) initial.Share.Bits = random_uint32(generator);
    
    work::proof proof = ::cpu_solve(work::puzzle(puzzle), initial, max_time_seconds);
    if (!proof.valid()) return {};

    bytes pay_script = pay_to_address::script(address.Digest);

    double fee_rate = 0.5;

    Bitcoin::satoshi fee = calculate_fee(puzzle.expected_size(), pay_script.size(), fee_rate);

    if (fee > value) throw string{"Cannot pay tx fee with boost output"};
    
    bytes redeem_tx = puzzle.redeem(proof.Solution, {output{value - fee, pay_script}});
    
    transaction redeem{redeem_tx};
    
    Bitcoin::txid redeem_txid = redeem.id();
    std::stringstream txid_stream;
    txid_stream << redeem_txid;

    std::cout << "redeem tx generated: " << redeem_tx << std::endl;
    
    for (const Bitcoin::input &in : redeem.Inputs) {
        
        bytes redeem_script = in.Script;
        
        std::string redeemhex = data::encoding::hex::write(redeem_script);
        
        logger::log("job.complete.redeemscript", json {
        {"solution", solution_to_json(proof.Solution)},
        {"asm", Bitcoin::ASM(redeem_script)},
        {"hex", redeemhex},
        {"fee", fee},
        {"txid", txid_stream.str().substr(7, 66)}
        });
    }

    // the transaction 
    return redeem_tx;
    
}

Boost::puzzle prevouts_to_puzzle(const boost_prevouts &prevs, const Bitcoin::secret &key) {
    Boost::output_script script = prevs.Script;
    return Boost::puzzle{data::for_each([&script](const whatsonchain::utxo &u) -> Boost::prevout {
        return {u.Outpoint, Boost::output{u.Value, script}};
    }, prevs.UTXOs), key};
}
