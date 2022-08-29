#ifndef BOOSTMINER_RANDOM
#define BOOSTMINER_RANDOM

#include <gigamonkey/types.hpp>
using namespace Gigamonkey;

// Some stuff having to do with random number generators. We do not need 
// strong cryptographic random numbers for boost. It is fine to use 
// basic random number generators that you would use in a game or something. 
template <typename engine>
double inline random_range01(engine& gen) {    
    return std::uniform_real_distribution<double>{0.0, 1.0}(gen);
}

template <typename engine>
data::uint64 random_uint64(engine& gen) {
    return std::uniform_int_distribution<data::uint64>{
        std::numeric_limits<data::uint64>::min(),
        std::numeric_limits<data::uint64>::max()
    }(gen);
}

template <typename engine>
data::uint32 random_uint32(engine& gen) {
    return std::uniform_int_distribution<data::uint32>{
        std::numeric_limits<data::uint32>::min(), 
        std::numeric_limits<data::uint32>::max()}(gen);
}

template <typename engine>
bool random_boolean(engine& gen) {
    return static_cast<bool>(std::uniform_int_distribution<data::uint32>{0, 1}(gen));
}

#endif
