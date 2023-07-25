#ifndef BOOSTMINER_RANDOM
#define BOOSTMINER_RANDOM

#include <gigamonkey/types.hpp>
#include <chrono>
#include <mutex>

namespace BoostPOW {
    using namespace Gigamonkey;

    // Some stuff having to do with random number generators. We do not need 
    // strong cryptographic random numbers for boost. It is fine to use 
    // basic random number generators that you would use in a game or something. 
    
    struct random {
        
        virtual double range01 () = 0;

        virtual data::uint64 uint64 (data::uint64 max) = 0;

        virtual data::uint32 uint32 (data::uint32 max) = 0;

        data::uint64 uint64 () {
            return uint64 (std::numeric_limits<data::uint64>::max ());
        }

        data::uint32 uint32 () {
            return uint32 (std::numeric_limits<data::uint32>::max ());
        }

        virtual bool boolean () = 0;
        
        virtual ~random () {}
        
    };
    
    template <typename engine>
    struct std_random : random {
    
        static double range01 (engine& gen) {
            return std::uniform_real_distribution<double> {0.0, 1.0} (gen);
        }

        static data::uint64 uint64 (engine& gen, data::uint64 max) {
            return std::uniform_int_distribution<data::uint64> {
                std::numeric_limits<data::uint64>::min (), max}(gen);
        }

        static data::uint32 uint32 (engine& gen, data::uint32 max) {
            return std::uniform_int_distribution<data::uint32> {
                std::numeric_limits<data::uint32>::min (), max} (gen);
        }

        static bool boolean (engine& gen) {
            return static_cast<bool> (std::uniform_int_distribution<data::uint32> {0, 1} (gen));
        }
        
        engine Engine;
        
        double range01 () override {
            return range01 (Engine);
        }

        data::uint64 uint64 (data::uint64 max = std::numeric_limits<data::uint64>::max ()) override {
            return uint64 (Engine, max);
        }

        data::uint32 uint32 (data::uint32 max = std::numeric_limits<data::uint32>::max ()) override {
            return uint32 (Engine, max);
        }

        bool boolean () override {
            return boolean (Engine);
        }
        
        std_random () : std_random {std::chrono::system_clock::now ().time_since_epoch ().count ()} {}
        std_random (data::uint64 seed);

    };

    using casual_random = std_random<std::default_random_engine>;
    
    template <typename engine>
    class random_threadsafe : random {
        std_random<engine> Random;
        std::mutex Mutex;
        
        double range01 () override {
            std::lock_guard<std::mutex> Lock (Mutex);
            return Random.range01 ();
        }

        data::uint64 uint64 (data::uint64 max) override {
            std::lock_guard<std::mutex> Lock (Mutex);
            return Random.uint64 (max);
        }

        data::uint32 uint32 (data::uint32 max) override {
            std::lock_guard<std::mutex> Lock (Mutex);
            return Random.uint32 (max);
        }

        bool boolean () override {
            std::lock_guard<std::mutex> Lock (Mutex);
            return Random.boolean ();
        }
        
        random_threadsafe () : random {} {}
        random_threadsafe (data::uint64 seed) : random {seed} {}

    };
    
    using casual_random_threadsafe = random_threadsafe<std::default_random_engine>;

    template <> inline std_random<std::default_random_engine>::std_random (data::uint64 seed) : Engine {} {
        Engine.seed (seed);
    }
    
}

#endif
