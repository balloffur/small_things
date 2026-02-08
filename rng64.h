#ifndef LCG64_RANDOM_GENERATOR
#define LCG64_RANDOM_GENERATOR

#include <cstdint>
#include <cstring>
#include <chrono>
#include <limits>
#include <type_traits>
#include <iterator>
#include <utility>
#include <sys/random.h>

namespace rng64 {



/// Default seed value used when no seed is provided.
inline constexpr std::uint64_t DEFAULT_SEED = 0xDEADBEEFDEADBEEFULL;



class PRNG64 {
public:
    /// Default deterministic seed constructor.
    constexpr PRNG64();

    /// Construct PRNG from raw double bits.
    explicit PRNG64(double seed_bits);

    /// Construct from any integral type (except bool).
    template<typename T,
             typename std::enable_if<
                 std::is_integral<T>::value &&
                 !std::is_same<typename std::remove_cv<T>::type, bool>::value,
                 int
             >::type = 0>
    explicit constexpr PRNG64(T seed);

    /// Create a PRNG seeded from system time (non-deterministic).
    static PRNG64 time_seed();

    /// Create a PRNG seeded from getrandom.
    static PRNG64 getrandom_seed();

    /// Next random 64-bit value (LCG + XorShift).
    std::uint64_t uint64();

    /// Random integer in range [low, high] (inclusive).
    std::uint64_t uint64(std::uint64_t low, std::uint64_t high);

    /// Random integer in [0, high) (exclusive).
    std::uint64_t uint64_exclusive(std::uint64_t high);

    /// Bernoulli(0.5) — fair 50/50 bit.
    bool bit();

    /// Bernoulli(p) — biased coin flip returning true ~p.
    bool bit(double p);

    /// Draw values until predicate passes. If max_count==0 => infinite loop until success.
    template<typename Func>
    std::uint64_t uint64_cond(Func condition, unsigned int max_count = MAX_COUNT_CONDITION_DEFAULT);

    /// Uniform double in [0,1).
    double real();

    /// Uniform double in [low, high).
    double real(double low, double high);

    /// Uniform int in [INT_MIN, INT_MAX].
    int integer();

    /// Uniform int in [low, high] (inclusive).
    int integer(int low, int high);


    // Random shuffle
    template<typename RandomIt>
    void shuffle(RandomIt first, RandomIt last);


     // Adaptors for STL integration

    using result_type = std::uint64_t;
    static constexpr std::uint64_t min() { return 0; }
    static constexpr std::uint64_t max() { return std::numeric_limits<std::uint64_t>::max(); }
    std::uint64_t operator()() { return uint64(); }

private:

    std::uint64_t state;

    static constexpr std::uint64_t A = 6364136223846793005ULL;
    static constexpr std::uint64_t C = 1ULL;
    static constexpr std::uint64_t DEFAULT_SEED_LOCAL = DEFAULT_SEED;

    // constexpr to avoid ODR/linker issues in header-only
    static constexpr unsigned int MAX_COUNT_CONDITION_DEFAULT = 100000;

    static constexpr int XS_S1 = 12;
    static constexpr int XS_S2 = 25;
    static constexpr int XS_S3 = 27;

    /// unbiased helper: [0, n)
    std::uint64_t _next_exclusive(std::uint64_t n);




   
};


inline constexpr PRNG64::PRNG64() : state(DEFAULT_SEED_LOCAL) {}


inline PRNG64::PRNG64(double seed_bits) {
    static_assert(sizeof(double) == 8, "unexpected double size");
    std::memcpy(&state, &seed_bits, sizeof(seed_bits));
}



// Template constructor from integral types without bool. No concepts!  
template<typename T,
         typename std::enable_if<
             std::is_integral<T>::value &&
             !std::is_same<typename std::remove_cv<T>::type, bool>::value,
             int
         >::type>
inline constexpr PRNG64::PRNG64(T seed) : state(static_cast<std::uint64_t>(seed)) {}

inline PRNG64 PRNG64::time_seed() {
    using namespace std::chrono;
    std::uint64_t t = static_cast<std::uint64_t>(
        high_resolution_clock::now().time_since_epoch().count()
    );
    std::uint64_t stack = reinterpret_cast<std::uint64_t>(&t);
    std::uint64_t seed = t ^ (stack * 0x9E3779B97F4A7C15ULL);
    return PRNG64(seed);
}


inline PRNG64 PRNG64::getrandom_seed() {
    std::uint64_t entropy = 0;

    ssize_t r = getrandom(&entropy, sizeof(entropy), 0);
    if (r != sizeof(entropy)) {
        throw std::runtime_error("getrandom failed");
    }
    std::uint64_t stack = reinterpret_cast<std::uint64_t>(&entropy);
    std::uint64_t seed =
        entropy ^ (stack * 0x9E3779B97F4A7C15ULL);

    return PRNG64(seed);
}

inline std::uint64_t PRNG64::uint64() {
    state = state * A + C;
    std::uint64_t x = state;
    x ^= x >> XS_S1;
    x ^= x << XS_S2;
    x ^= x >> XS_S3;
    return x;
}

inline std::uint64_t PRNG64::_next_exclusive(std::uint64_t n) {
    if (n <= 1) return 0;
    if ((n & (n - 1)) == 0)
        return uint64() & (n - 1);

    std::uint64_t threshold = (-n) % n;
    while (true) {
        std::uint64_t r = uint64();
        if (r >= threshold)
            return r % n;
    }
}

inline std::uint64_t PRNG64::uint64(std::uint64_t low, std::uint64_t high) {
    if (low > high) return low;
    std::uint64_t range = high - low + 1;
    if (range == 0) return low; // overflow when low=high=UINT64_MAX
    return low + _next_exclusive(range);
}

inline std::uint64_t PRNG64::uint64_exclusive(std::uint64_t high) {
    return _next_exclusive(high);
}

inline bool PRNG64::bit() {
    return (uint64() & 1ULL) != 0ULL;
}

inline bool PRNG64::bit(double p) {
    if (p <= 0.0) return false;
    if (p >= 1.0) return true;
    return real() < p;
}


template<typename Func>
inline std::uint64_t PRNG64::uint64_cond(Func condition, unsigned int max_count) {
    std::uint64_t r;
    if (max_count == 0) {
        do { r = uint64(); } while (!condition(r));
        return r;
    } else {
        for (unsigned int i = 0; i < max_count; ++i) {
            r = uint64();
            if (condition(r)) return r;
        }
        return 0;
    }
}

inline double PRNG64::real() {
    return (uint64() >> 11) * 0x1.0p-53;
}

inline double PRNG64::real(double low, double high) {
    return low + real() * (high - low);
}

inline int PRNG64::integer() {
    constexpr std::int64_t low  = static_cast<std::int64_t>(std::numeric_limits<int>::min());
    constexpr std::int64_t high = static_cast<std::int64_t>(std::numeric_limits<int>::max());
    const std::uint64_t range = static_cast<std::uint64_t>(high - low) + 1ULL;
    return static_cast<int>(low + static_cast<std::int64_t>(_next_exclusive(range)));
}

inline int PRNG64::integer(int low, int high) {
    if (low > high) return low;

    const std::int64_t lo = static_cast<std::int64_t>(low);
    const std::int64_t hi = static_cast<std::int64_t>(high);
    const std::uint64_t range = static_cast<std::uint64_t>(hi - lo) + 1ULL;

    return static_cast<int>(lo + static_cast<std::int64_t>(_next_exclusive(range)));
}



template<class RandomIt>
inline void PRNG64::shuffle(RandomIt first, RandomIt last) {
    using diff_t = typename std::iterator_traits<RandomIt>::difference_type;

    diff_t n = last - first;
    if (n <= 1) return;

    // Fisher–Yates: for i = n-1..1 swap(i, j), j in [0..i]
    for (diff_t i = n - 1; i > 0; --i) {
        const std::uint64_t j = _next_exclusive(static_cast<std::uint64_t>(i) + 1ULL);
        std::iter_swap(first + i, first + static_cast<diff_t>(j));
    }
}








} // namespace RNG

#endif // LCG64_RANDOM_GENERATOR
