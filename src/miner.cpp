#include <gigamonkey/script/typed_data_bip_276.hpp>
#include <miner.hpp>
#include <logger.hpp>

namespace BoostPOW {

    // A cpu miner function. 
    work::proof cpu_solve(const work::puzzle& p, const work::solution& initial, double max_time_seconds) {
        using uint256 = Gigamonkey::uint256;
        
        uint32 initial_time = initial.Share.Timestamp.Value;
        uint32 local_initial_time = Bitcoin::timestamp::now().Value;
        
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
                pr.Solution.Share.Timestamp.Value = initial_time + uint32(Bitcoin::timestamp::now().Value - local_initial_time);
                
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

    prevouts select(random &r, const jobs &j, double minimum_profitability) {
        
        if (j.size() == 0) return {};
        
        double total_profitability = 0;
        for (const auto &p : j) if (p.second.profitability() > minimum_profitability) 
            total_profitability += (p.second.profitability() - minimum_profitability);
        
        double random = r.range01() * total_profitability;
        
        double accumulated_profitability = 0;
        for (const auto &p : j) if (p.second.profitability() > minimum_profitability) {
            accumulated_profitability += (p.second.profitability() - minimum_profitability);
            
            if (accumulated_profitability >= random) return p.second;
        }
        
        // shouldn't happen. 
        return {};
    }

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

    Bitcoin::transaction mine(
        random &r, 
        // an unredeemed Boost PoW output 
        const Boost::puzzle &puzzle, 
        // the address you want the bitcoins to go to once you have redeemed the boost output.
        // this is not the same as 'miner address'. This is just an address in your 
        // normal wallet and should not be the address that goes along with the key above.
        const Bitcoin::address &address, 
        double max_time_seconds, 
        double minimum_price_per_difficulty_sats, 
        double maximum_mining_difficulty) {
        
        using namespace Bitcoin;
        std::cout << "mining on script " << puzzle.id() << std::endl;
        if (!puzzle.valid()) throw string{"Boost puzzle is not valid"};
        
        Bitcoin::satoshi value = puzzle.value();
        
        std::cout << "difficulty is " << puzzle.difficulty() << "." << std::endl;
        std::cout << "price per difficulty is " << puzzle.profitability() << "." << std::endl;
        
        // is the difficulty too high?
        if (maximum_mining_difficulty > 0, puzzle.difficulty() > maximum_mining_difficulty) 
            std::cout << "warning: difficulty " << puzzle.difficulty() << " may be too high for CPU mining." << std::endl;
        
        // is the value in the output high enough? 
        if (puzzle.profitability() < minimum_price_per_difficulty_sats)
            std::cout << "warning: price per difficulty " << puzzle.profitability() << " may be too low." << std::endl;
        
        Stratum::session_id extra_nonce_1{r.uint32()};
        uint64_big extra_nonce_2{r.uint64()};
        
        work::solution initial{timestamp::now(), 0, bytes_view(extra_nonce_2), extra_nonce_1};
        
        if (puzzle.use_general_purpose_bits()) initial.Share.Bits = r.uint32();
        
        work::proof proof = BoostPOW::cpu_solve(work::puzzle(puzzle), initial, max_time_seconds);
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
        return redeem;
        
    }

    Boost::puzzle prevouts::to_puzzle(const Bitcoin::secret &key) const {
        Boost::output_script script = Script;
        
        return Boost::puzzle{data::for_each([&script](const utxo &u) -> Boost::prevout {
            return {u.Outpoint, Boost::output{u.Value, script}};
        }, UTXOs), key};
    }

    prevouts::operator json() const {
        std::stringstream value_stream;
        value_stream << int64(Value);
        
        json::array_t arr;
        for (const auto &u : UTXOs) arr.push_back(json(u));
        
        return json {
            {"script", typed_data::write(typed_data::mainnet, Script.write())}, 
            {"UTXOs", arr}, 
            {"value", value_stream.str()}};
    }

    jobs::operator json() const {
        json::object_t puz;
        
        for (const auto &j : *this) {
            std::stringstream ss;
            ss << j.first;
            puz[ss.str()] = json(j.second);
        }
        
        return puz;
    }

}
