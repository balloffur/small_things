#pragma once
#include <cstdint>

const uint64_t DEFAULT_SEED = 0xDEADBEEFDEADBEEF;
static constexpr uint64_t pows_of_ten[] = {1ULL,10ULL,100ULL,1000ULL,10000ULL,100000ULL,1000000ULL,10000000ULL,100000000ULL,1000000000ULL,10000000000ULL,100000000000ULL,1000000000000ULL,10000000000000ULL,100000000000000ULL,1000000000000000ULL,10000000000000000ULL,100000000000000000ULL,1000000000000000000ULL,10000000000000000000ULL};

//Fast LCG+XORSHIFT 64bit generator
struct LCG64_XORSHIFT {
    uint64_t state;

    // LCG и xorshift параметры (все капсом)
    static constexpr uint64_t A = 6364136223846793005ULL;
    static constexpr uint64_t C = 1ULL;
    static const uint64_t DEFAULT_SEED = 0xDEADBEEFDEADBEEF;
    static constexpr int XS_S1 = 12;
    static constexpr int XS_S2 = 25;
    static constexpr int XS_S3 = 27;

    explicit LCG64_XORSHIFT(uint64_t seed = DEFAULT_SEED) : state(seed) {}

    // Генерация следующего числа с LCG + xorshift
    uint64_t next() {
        state = state * A + C; // LCG
        uint64_t x = state;
        // xorshift
        x ^= x >> XS_S1;
        x ^= x << XS_S2;
        x ^= x >> XS_S3;
        return x;
    }

    // Генерация числа в диапазоне [low, high]
    uint64_t next(uint64_t low, uint64_t high) {
        uint64_t r = next();
        return low + r % (high - low + 1);
    }

    // Генерация случайного нечётного числа в диапазоне [low, high]
    uint64_t next_odd(uint64_t low, uint64_t high) {
        uint64_t r = next(low, high);
        if ((r & 1ULL) == 0) r += 1;   // если чётное → прибавляем 1
        if (r > high) r -= 2;          // корректируем, если вышли за предел
        return r;
    }

    uint64_t next_digs(int digs) {
        if(digs <= 0 || digs > 19) {
            throw std::runtime_error("Wrong number of digits");
        }
        uint64_t low  = pows_of_ten[digs - 1];
        uint64_t high = pows_of_ten[digs] - 1;

        return low + next() % (high - low + 1);
    }


    static const unsigned int MAX_COUNT_CONDITION_DEFAULT = 100000; 
    template <typename Func>
    uint64_t next_cond(Func condition, unsigned int max_count = MAX_COUNT_CONDITION_DEFAULT) {
        uint64_t r = next();
        if (max_count == 0) {
            while (true) {
                r = next();
                if (condition(r)) return r;
            }
        } else {
            unsigned int count = 0;
            while (count < max_count) {
                r = next();
                if (condition(r)) return r;
                count++;
            }
        }
        return 0;
}
  
};

