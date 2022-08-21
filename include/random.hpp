#ifndef BOOSTMINER_RANDOM
#define BOOSTMINER_RANDOM

#include <gigamonkey/types.hpp>
#include <chrono>

using namespace Gigamonkey;

namespace BoostPOW {

    // Some stuff having to do with random number generators. We do not need 
    // strong cryptographic random numbers for boost. It is fine to use 
    // basic random number generators that you would use in a game or something. 

    template <typename engine>
    struct random {

        static double inline range01(engine& gen) {    
            return std::uniform_real_distribution<double>{0.0, 1.0}(gen);
        }

        static data::uint64 uint64(engine& gen) {
            return std::uniform_int_distribution<data::uint64>{
                std::numeric_limits<data::uint64>::min(),
                std::numeric_limits<data::uint64>::max()
            }(gen);
        }

        static data::uint32 uint32(engine& gen) {
            return std::uniform_int_distribution<data::uint32>{
                std::numeric_limits<data::uint32>::min(), 
                std::numeric_limits<data::uint32>::max()}(gen);
        }

        static bool boolean(engine& gen) {
            return static_cast<bool>(std::uniform_int_distribution<data::uint32>{0, 1}(gen));
        }
        
        engine Engine;
        
        double inline range01() {    
            return range01(Engine);
        }

        data::uint64 uint64() {
            return uint64(Engine);
        }

        data::uint32 uint32() {
            return uint32(Engine);
        }

        bool boolean() {
            return boolean(Engine);
        }
        
        random() : random{std::chrono::system_clock::now().time_since_epoch().count()} {}
        random(data::uint64 seed);

    };

    template <> inline random<std::default_random_engine>::random(data::uint64 seed) : Engine{} {
        Engine.seed(seed);
    }

    using casual_random = random<std::default_random_engine>;

}

#endif
