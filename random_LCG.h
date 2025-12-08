#pragma once
#include <cstdint>
#include <cstring>
#include <chrono>

#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
    #include <concepts>
    #include <type_traits>
    #define LCG64_HAS_CONCEPTS 1
#else
    #define LCG64_HAS_CONCEPTS 0
#endif

/// Default seed value used when no seed is provided.
const uint64_t DEFAULT_SEED = 0xDEADBEEFDEADBEEF;

/// Precomputed powers of ten used by uint64_digs().
static constexpr uint64_t pows_of_ten[] = {
    1ULL,10ULL,100ULL,1000ULL,10000ULL,100000ULL,1000000ULL,10000000ULL,
    100000000ULL,1000000000ULL,10000000000ULL,100000000000ULL,
    1000000000000ULL,10000000000000ULL,100000000000000ULL,
    1000000000000000ULL,10000000000000000ULL,100000000000000000ULL,
    1000000000000000000ULL,10000000000000000000ULL
};

/**
 * @brief Fast 64-bit PRNG based on a combined LCG + XorShift step.
 *
 * Compact (64-bit) internal state, deterministic for fixed seed.
 * Not cryptographically secure — intended for simulation, Monte-Carlo,
 * procedural generation, sampling, randomized algorithms etc.
 */
struct PRNG64 {

    uint64_t state;

    static constexpr uint64_t A  = 6364136223846793005ULL;
    static constexpr uint64_t C  = 1ULL;
    static const     uint64_t DEFAULT_SEED = 0xDEADBEEFDEADBEEF;
    static constexpr int      XS_S1 = 12;
    static constexpr int      XS_S2 = 25;
    static constexpr int      XS_S3 = 27;


    /**
     * @brief Default deterministic seed constructor.
     */
    PRNG64(){
        state = DEFAULT_SEED;
    }

    /**
     * @brief Construct PRNG from raw double bits.
     */
    explicit PRNG64(double seed){
        static_assert(sizeof(double) == 8, "unexpected double size");
        std::memcpy(&state, &seed, sizeof(seed));
    }

#if LCG64_HAS_CONCEPTS
    /**
     * @brief Construct from any type convertible to uint64_t.
     */
    template<typename T>
        requires (std::convertible_to<T, uint64_t> &&
                  !std::same_as<std::remove_cvref_t<T>, PRNG64>)
    explicit PRNG64(T seed){
        state = static_cast<uint64_t>(seed);
    }
#else
    explicit PRNG64(uint64_t seed){
        state = seed;
    }
#endif

    /**
     * @brief Create a PRNG seeded from system time (non-deterministic).
     */
    static PRNG64 time_seed(){
        using namespace std::chrono;
        uint64_t t     = high_resolution_clock::now().time_since_epoch().count();
        uint64_t stack = reinterpret_cast<uint64_t>(&t);
        uint64_t seed  = t ^ (stack * 0x9E3779B97F4A7C15ULL);
        return PRNG64(seed);
    }

    /**
     * @brief Next random 64-bit value (LCG + XorShift).
     */
    uint64_t uint64(){
        state = state * A + C;
        uint64_t x = state;
        x ^= x >> XS_S1;
        x ^= x << XS_S2;
        x ^= x >> XS_S3;
        return x;
    }

    /**
     * @brief Random integer in range [low, high].
     */
    uint64_t uint64(uint64_t low, uint64_t high){
        uint64_t r = uint64();
        return low + r % (high - low + 1);
    }

    /**
     * @brief Random integer in [0, high).
     */
    uint64_t uint64_exclusive(uint64_t high){
        if(high == 0){
            return 0;
        }
        if(high == UINT64_MAX){
            return uint64();
        }
        return uint64() % high;
    }

    /**
     * @brief Bernoulli(p) — fair 50/50 bit.
     */
    bool bit(){
        return uint64() & 1;
    }

    /**
     * @brief Bernoulli(p) — biased coin flip returning true ~p.
     */
    bool bit(double p){
        if(p <= 0.0){
            return false;
        }
        if(p >= 1.0){
            return true;
        }
        return real() < p;
    }

    /**
     * @brief Integer with exactly `digs` decimal digits.
     */
    uint64_t uint64_digs(int digs){
        if(digs <= 0 || digs > 19){
            return 0;
        }
        uint64_t low  = pows_of_ten[digs - 1];
        uint64_t high = pows_of_ten[digs] - 1;
        return low + uint64() % (high - low + 1);
    }


    static const unsigned int MAX_COUNT_CONDITION_DEFAULT = 100000;

    /**
     * @brief Draw values until predicate passes.
     */
    template<typename Func>
    uint64_t uint64_cond(Func condition, unsigned int max_count = MAX_COUNT_CONDITION_DEFAULT){
        uint64_t r = uint64();
        if(max_count == 0){
            while(true){
                r = uint64();
                if(condition(r)){
                    return r;
                }
            }
        }else{
            unsigned int count = 0;
            while(count < max_count){
                r = uint64();
                if(condition(r)){
                    return r;
                }
                count++;
            }
        }
        return 0;
    }

    /**
     * @brief Uniform double in [0,1).
     */
    double real(){
        uint64_t r = uint64();
        return (r >> 11) * (1.0 / (1ULL << 53));
    }

    /**
     * @brief Uniform double in [low, high).
     */
    double real(double low, double high){
        return low + real() * (high - low);
    }
};
