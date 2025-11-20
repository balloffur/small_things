    #include <cstdint>

    #define SMALL_PRIMES_BORD 127
    #define SMALL_PRIMES_MAXCAP 16384

    constexpr uint64_t ODD_PRIMES_MASK = 0b100000000110110100010010100110100110010010110100110010110110111ULL;

    // биты соответствуют нечётным простым 3,5,7,...,127
    inline constexpr bool is_prime_small_bitmask(int n) {
        if (n == 2) return true;          // 2 — простое
        if (n < 2 || n % 2 == 0) return false; // исключаем меньше 2 и чётные >2
        int index = (n - 3) / 2;           // индекс бита в маске для нечётных чисел
        return (ODD_PRIMES_MASK >> index) & 1ULL;
    }

    // Делимость на простые [2,127]
    inline constexpr bool div_by_sp(int32_t n) {
    // Проверяем делимость на простые до 127
    constexpr int small_primes[] = {
        2, 3, 5, 7, 11, 13, 17, 19,  
    23, 29, 31, 37, 41, 43, 47,  
    53, 59, 61, 67, 71, 73, 79,  
    83, 89, 97, 101, 103, 107, 109, 113,  
    127
    };
    for (int p : small_primes) {
        if (n % p == 0)
            return true;
    }
    return false; 
}




    constexpr int32_t pow_mod(int64_t n, int32_t power, int64_t m) {
    int64_t result = 1;

    while (power) {
        if (power & 1)
        result = (result * n) % m;

        n = (n * n) % m;

        power >>= 1;
    }

    return (int32_t)result;
    }

    constexpr int32_t mul_mod(int32_t a, int32_t b, int32_t m) {
    return (int32_t)(((int64_t)a * b) % m);
    }


    // Тест Миллера-Раббина для int, детерминированный. Используем a= 2, 3, 5.
    // Исключаем 4 псевдопростых из диапазона (1,2^31-1)
    constexpr bool MillerRabbin(int32_t n) {

    int32_t s = __builtin_ctz(n - 1);
    int32_t t = (n - 1) >> s;

    int32_t primes[3] = {2, 3, 5};

    for (int32_t a : primes) {
        int32_t x = pow_mod(a, t, n);

        if (x == 1)
        continue;

        for (int i = 1; x != n - 1; i++) {
        if (i == s)
            return false;

        x = mul_mod(x, x, n);

        if (x == 1)
            return false;
        }
    }

    // Псевдопростые для a =2,3,5
    switch (n) {
    case 25326001:
    case 161304001:
    case 960946321:
    case 1157839381:
        return false;

    default:
        return true;
    }
    }
    
    constexpr bool is_prime(int32_t n){
        if (n<128) {return is_prime_small_bitmask(n);}
        if(div_by_sp(n)) {return false;}
        if(n<SMALL_PRIMES_MAXCAP) {return true;}
        return MillerRabbin(n);
    }
