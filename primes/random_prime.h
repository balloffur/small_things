#pragma once
#include <cstdint>
#include "random_LCG.h"
#include "prime_test.h"

static constexpr uint64_t primorial = 2*3*5*7;

static constexpr uint64_t primorial_free[] = {11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 121, 127, 131, 137, 139, 143, 149, 151, 157, 163, 167, 169, 173, 179, 181, 187, 191, 193, 197, 199, 209};
static constexpr uint16_t primorial_free_size=46;

uint64_t random_prime(uint64_t seed=DEFAULT_SEED){
    LCG64_XORSHIFT gen(seed);
    while(true){
    uint64_t r=gen.next();
    r=(r/primorial)*primorial+primorial_free[r%primorial_free_size];
    if(is_prime(r))
        return r;
    }
    return 0;
}

static constexpr uint64_t digits1[]={2,3,5,7};
static constexpr uint64_t digits2[]={11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97};
uint64_t random_prime_digs(int digits = 19, uint64_t seed = DEFAULT_SEED) {
    LCG64_XORSHIFT gen(seed);

    if(digits == 1){
        return digits1[gen.next() % (sizeof(digits1)/sizeof(digits1[0]))];
    }
    if(digits == 2){
        return digits2[gen.next() % (sizeof(digits2)/sizeof(digits2[0]))];
    }

    uint64_t high = (digits < 19) ? pows_of_ten[digits+1] : UINT64_MAX;

    while(true){
        uint64_t r = gen.next_digs(digits);
        r = (r / primorial) * primorial + primorial_free[r % primorial_free_size];
        if(is_prime(r) && r < high){
            return r;
        }
    }
}

    
