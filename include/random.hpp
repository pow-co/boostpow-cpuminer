#ifndef BOOSTMINER_LOGGER
#define BOOSTMINER_LOGGER

// Some stuff having to do with random number generators. We do not need 
// strong cryptographic random numbers for boost. It is fine to use 
// basic random number generators that you would use in a game or something. 
template <typename engine>
double random_range01(engine& gen) {
    static std::uniform_real_distribution<double> dis(0.0, 1.0);
    return dis(gen);
}

template <typename engine>
data::uint64 random_uint64(engine& gen) {
    static std::uniform_int_distribution<data::uint64> dis(
        std::numeric_limits<data::uint64>::min(),
        std::numeric_limits<data::uint64>::max()
    );
    return dis(gen);
}

template <typename engine>
data::uint32 random_uint32(engine& gen) {
    static std::uniform_int_distribution<data::uint32> dis(
        std::numeric_limits<data::uint32>::min(),
        std::numeric_limits<data::uint32>::max()
    );
    return dis(gen);
}

#endif
