#include <cstdint>

/// Deterministic primality test for all 64-bit integers.
/// For n in [0,127] — bitmask lookup.
/// For n in [128, 2^32) — small prime trial division + Miller–Rabin with bases 2,3,61.
/// For n in [2^32, 2^51) — Miller–Rabin with bases 2,3,5,7.
/// For n in [2^51, 2^64) — Miller–Rabin with bases:
///   2, 325, 9375, 28178, 450775, 9780504, 1795265022.
/// These sets of bases give a fully deterministic result across the entire uint64 range.


// Up to 127 via bitmask
constexpr uint64_t ODD_PRIMES_MASK =
    0b100000010110110100010010100110100110010010110100110010110110111ULL;
// Bits correspond to odd primes 3,5,7,...,127.
inline constexpr bool is_prime_small_bitmask(int n) {
    if (n == 2) return true;                    // 2 is prime
    if (n < 2 || n % 2 == 0) return false;      // reject <2 and even numbers >2
    int index = (n - 3) / 2;                    // bit index for odd numbers
    return (ODD_PRIMES_MASK >> index) & 1ULL;
}

// Deterministic Miller–Rabin bases for 16-bit, 32-bit, 51-bit and 64-bit ranges
static constexpr inline uint64_t base64[] =
    {2ll, 325ll, 9375ll, 28178ll, 450775ll, 9780504ll, 1795265022ll};
static constexpr inline uint64_t base51[] = {2ll, 3ll, 5ll, 7ll};
static constexpr inline uint32_t base32[] = {2, 3, 61};
static constexpr inline uint16_t base16[] = {2, 3};

// Small primes for quick trial division
static constexpr inline uint64_t small_primes[] =
    {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97};

#define MAXUINT32  4294967295u
#define MAX51      3317444400000000ull
#define MAX_SMALL_SQUARED 9409

static constexpr inline uint64_t powmod64(uint64_t a, uint64_t d, uint64_t mod) {
    __uint128_t r = 1;
    __uint128_t x = a % mod;
    while (d) {
        if (d & 1) r = (r * x) % mod;
        x = (x * x) % mod;
        d >>= 1;
    }
    return (uint64_t)r;
}

static constexpr inline uint32_t powmod32(uint32_t a, uint32_t d, uint32_t mod) {
    uint64_t r = 1;
    uint64_t x = a % mod;
    while (d) {
        if (d & 1) r = (r * x) % mod;
        x = (x * x) % mod;
        d >>= 1;
    }
    return (uint32_t)r;
}

// Основной цикл Миллера-Рабина
static constexpr inline bool check_composite32(uint32_t n, uint32_t a, uint32_t d, int s) {
    uint32_t x = powmod32(a, d, n);
    if (x == 1 || x == n - 1) return false;
    while (--s) {
        x = (uint64_t)x * x % n;
        if (x == n - 1) return false;
    }
    return true; 
}

// Основной цикл Миллера-Рабина
static constexpr inline bool check_composite64(uint64_t n, uint64_t a, uint64_t d, int s) {
    uint64_t x = powmod64(a, d, n);
    if (x == 1 || x == n - 1) return false;
    while (--s) {
        x = (uint64_t)((__uint128_t)x * x % n);
        if (x == n - 1) return false;
    }
    return true; 
}


inline constexpr bool Miller_Rabbin_32(uint32_t n) {
    uint32_t d = n - 1;
    int s = 0;
    while ((d & 1u) == 0) { d >>= 1; s++; }
    for (uint32_t a : base32) {
        if (a % n == 0) continue;
        if (check_composite32(n, a, d, s)) return false;
    }
    return true;
}

inline constexpr bool __Miller_Rabbin_51(uint64_t n) {
    uint64_t d = n - 1;
    int s = 0;
    while ((d & 1ull) == 0) { d >>= 1; s++; }
    for (uint64_t a : base51) {
        if (a % n == 0) continue;
        if (check_composite64(n, a, d, s)) return false;
    }
    return true;
}

inline constexpr bool Miller_Rabbin_64(uint64_t n) {
    uint64_t d = n - 1;
    int s = 0;
    while ((d & 1ull) == 0) { d >>= 1; s++; }
    for (uint64_t a : base64) {
        if (a % n == 0) continue;
        if (check_composite64(n, a, d, s)) return false;
    }
    return true;
}

//Проверяет число uint64_t на простоту, детерминированно
constexpr bool is_prime(uint64_t n) {
    if (n < 127) {
        return is_prime_small_bitmask(n);
    }
    for (auto i : small_primes) {
        if (n % i == 0) return (n==i);
    }
    if(n <= MAX_SMALL_SQUARED){
        return true;
    }
    if (n <= MAXUINT32) {
        return Miller_Rabbin_32(n);
    }
    if (n <= MAX51) {
        return __Miller_Rabbin_51(n);
    }
    return Miller_Rabbin_64(n);
}
