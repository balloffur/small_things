#include <cstdint>

/// Детерминированный тест простоты для всех чисел из диапазона [0,2^64]
/// Для чисел из [0,127] -- битмаска
/// Для чисел из [128,2^32] -- малые простые + Миллер-Раббин с базами 2,3,61
/// Для чисел из [2^32,2^51] -- Миллер-Раббин с базами 2,3,5,7
/// Для чисел из [2^51,2^64] -- Миллер-Раббин с базами 2 325 9375 28178 450775 9780504 1795265022ll



// До 127 через битмаску
constexpr uint64_t ODD_PRIMES_MASK = 0b100000010110110100010010100110100110010010110100110010110110111ULL;
// биты соответствуют нечётным простым 3,5,7,...,127
inline constexpr bool is_prime_small_bitmask(int n) {
    if (n == 2) return true;          // 2 — простое
    if (n < 2 || n % 2 == 0) return false; // исключаем меньше 2 и чётные >2
    int index = (n - 3) / 2;           // индекс бита в маске для нечётных чисел
    return (ODD_PRIMES_MASK >> index) & 1ULL;
}
// Базы Детерминированного миллера раббина для 16,32 и 64
static constexpr inline uint64_t base64[]={2ll, 325ll, 9375ll, 28178ll, 450775ll, 9780504ll, 1795265022ll};
static constexpr inline uint64_t base51[]={2ll,3ll,5ll,7ll};
static constexpr inline uint32_t base32[]={2,3,61};
static constexpr inline uint16_t base16[]={2,3};

// Малые простые 
static constexpr inline uint64_t small_primes[]={2,3,5,7,11,13,17,19,23};

#define MAXUINT32  4294967295
#define MAX51 3317444400000000


static inline uint64_t powmod64(uint64_t a, uint64_t d, uint64_t mod) {
    __uint128_t r = 1;
    __uint128_t x = a % mod;
    while (d) {
        if (d & 1) r = (r * x) % mod;
        x = (x * x) % mod;
        d >>= 1;
    }
    return (uint64_t)r;
}

static inline uint32_t powmod32(uint32_t a, uint32_t d, uint32_t mod) {
    uint64_t r = 1;
    uint64_t x = a % mod;
    while (d) {
        if (d & 1) r = (r * x) % mod;
        x = (x * x) % mod;
        d >>= 1;
    }
    return (uint32_t)r;
}


static inline bool check_composite32(uint32_t n, uint32_t a, uint32_t d, int s) {
    uint32_t x = powmod32(a, d, n);
    if (x == 1 || x == n - 1) return false;
    while (--s) {
        x = (uint64_t)x * x % n;
        if (x == n - 1) return false;
    }
    return true; // composite
}

static inline bool check_composite64(uint64_t n, uint64_t a, uint64_t d, int s) {
    uint64_t x = powmod64(a, d, n);
    if (x == 1 || x == n - 1) return false;
    while (--s) {
        x = (uint64_t)((__uint128_t)x * x % n);
        if (x == n - 1) return false;
    }
    return true; // composite
}



inline bool Miller_Rabbin_32(uint32_t n) {
    uint32_t d = n - 1;
    int s = 0;
    while ((d & 1u) == 0) { d >>= 1; s++; }
    for (uint32_t a : base32) {
        if (a % n == 0) continue;
        if (check_composite32(n, a, d, s)) return false;
    }
    return true;
}

inline bool Miller_Rabbin_51(uint64_t n) {
    uint64_t d = n - 1;
    int s = 0;
    while ((d & 1ull) == 0) { d >>= 1; s++; }
    for (uint64_t a : base51) {
        if (a % n == 0) continue;
        if (check_composite64(n, a, d, s)) return false;
    }
    return true;
}

inline bool Miller_Rabbin_64(uint64_t n) {
    uint64_t d = n - 1;
    int s = 0;
    while ((d & 1ull) == 0) { d >>= 1; s++; }
    for (uint64_t a : base64) {
        if (a % n == 0) continue;
        if (check_composite64(n, a, d, s)) return false;
    }
    return true;
}


bool is_prime(uint64_t n){
    if(n<127){
        return is_prime_small_bitmask(n);
    }
    for(auto i:small_primes){
        if(n%i==0) return false;
    }
    if(n<MAXUINT32){
        return Miller_Rabbin_32(n);
    } 
    if(n<MAX51){
        return Miller_Rabbin_51(n);
    }
    
    return Miller_Rabbin_64(n);
}

