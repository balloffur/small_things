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

/// Precomputed powers of ten, used by uint64_digs().
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
 * Compact (64-bit) internal state, deterministic sequences for the same seed.
 * Not cryptographically secure — suitable for simulation, procedural
 * generation, Monte-Carlo, sampling etc.
 *
 * Key API:
 *  - uint64()            → full 64-bit random value
 *  - uint64(low, high)   → bounded integer
 *  - bit()               → random bool
 *  - real()              → uniform double in [0, 1)
 *  - real(low, high)     → uniform double in [low, high)
 *  - uint64_digs(n)      → integer with exactly n decimal digits
 *  - uint64_cond(pred)   → draw values until predicate returns true
 */
struct PRNG64 {
    /// Internal generator state.
    uint64_t state;

    // Algorithm parameters
    static constexpr uint64_t A  = 6364136223846793005ULL;
    static constexpr uint64_t C  = 1ULL;
    static const     uint64_t DEFAULT_SEED = 0xDEADBEEFDEADBEEF;
    static constexpr int      XS_S1 = 12;
    static constexpr int      XS_S2 = 25;
    static constexpr int      XS_S3 = 27;

    /**
     * @brief Default constructor.
     *
     * Initializes the generator using DEFAULT_SEED.
     * Produces the same sequence each run.
     */
    PRNG64() : state(DEFAULT_SEED) {}

    /**
     * @brief Construct from a double seed.
     *
     * Uses the 64-bit binary representation of the double
     * as the initial PRNG state.
     *
     * Determinism is guaranteed only for this specific runtime
     * and floating-point format.
     */
    explicit PRNG64(double seed) {
        static_assert(sizeof(double) == 8, "unexpected double size");
        std::memcpy(&state, &seed, sizeof(seed));
    }

#if LCG64_HAS_CONCEPTS
    /**
     * @brief Construct from any type implicitly convertible to uint64_t.
     *
     * All bits of the converted value define the generator's seed.
     * Deterministic reproducibility is ensured when seeding from
     * uint64_t directly.
     */
    template <typename T>
        requires (std::convertible_to<T, uint64_t> &&
                  !std::same_as<std::remove_cvref_t<T>, PRNG64>)
    explicit PRNG64(T seed)
        : state(static_cast<uint64_t>(seed)) {}
#else
    /**
     * @brief Construct from a raw 64-bit seed (fallback for non-concept compilers).
     */
    explicit PRNG64(uint64_t seed) : state(seed) {}
#endif

    /**
     * @brief Construct a generator seeded from system time and ASLR entropy.
     *
     * Produces different sequences across program runs.
     * For reproducible output, seed manually using uint64_t.
     */
    static PRNG64 time_seed() {
        using namespace std::chrono;
        uint64_t t     = high_resolution_clock::now().time_since_epoch().count();
        uint64_t stack = reinterpret_cast<uint64_t>(&t);
        uint64_t seed  = t ^ (stack * 0x9E3779B97F4A7C15ULL);
        return PRNG64(seed);
    }

    /**
     * @brief Generate a random boolean value.
     */
    bool bit() {
        return uint64() & 1;
    }

    /**
     * @brief Bernoulli trial with probability p of returning true.
     *
     * @param p Probability in [0, 1].
     * @return true with probability ~p, false otherwise.
     */
    bool bit(double p) {
        if (p <= 0.0) return false;
        if (p >= 1.0) return true;
        return real() < p;
}

    /**
     * @brief Generate the next 64-bit pseudorandom value.
     *
     * Advances the LCG state and applies XorShift scrambling.
     */
    uint64_t uint64() {
        state = state * A + C;
        uint64_t x = state;
        x ^= x >> XS_S1;
        x ^= x << XS_S2;
        x ^= x >> XS_S3;
        return x;
    }

    /**
     * @brief Generate an integer uniformly in [low, high].
     *
     * Uses modular reduction; slight statistical bias exists
     * when the interval length does not divide 2^64.
     */
    uint64_t uint64(uint64_t low, uint64_t high) {
        uint64_t r = uint64();
        return low + r % (high - low + 1);
    }

    /**
     * @brief Generate a random number with exactly `digs` decimal digits.
     *
     * Range = [10^(digs-1), 10^digs - 1].
     *
     * @return 0 if digs is out of [1,19].
     */
    uint64_t uint64_digs(int digs) {
        if (digs <= 0 || digs > 19)
            return 0;
        uint64_t low  = pows_of_ten[digs - 1];
        uint64_t high = pows_of_ten[digs] - 1;
        return low + uint64() % (high - low + 1);
    }

    /// Default retry limit for uint64_cond().
    static const unsigned int MAX_COUNT_CONDITION_DEFAULT = 100000;

    /**
     * @brief Draw values until `condition(value)` succeeds.
     *
     * @param condition Predicate receiving uint64_t; returns true when satisfied.
     * @param max_count Maximum attempts; 0 means unbounded search.
     * @return A matching value, or 0 if no match within attempts.
     */
    template <typename Func>
    uint64_t uint64_cond(Func condition, unsigned int max_count = MAX_COUNT_CONDITION_DEFAULT) {
        uint64_t r = uint64();
        if (max_count == 0) {
            while (true) {
                r = uint64();
                if (condition(r)) return r;
            }
        } else {
            unsigned int count = 0;
            while (count < max_count) {
                r = uint64();
                if (condition(r)) return r;
                count++;
            }
        }
        return 0;
    }

    /**
     * @brief Generate a uniform double in [0, 1).
     *
     * Uses the upper 53 bits of uint64() to fill the mantissa.
     */
    double real() {
        uint64_t r = uint64();
        return (r >> 11) * (1.0 / (1ULL << 53));
    }

    /**
     * @brief Generate a uniform double in [low, high).
     */
    double real(double low, double high) {
        return low + real() * (high - low);
    }

    
};
