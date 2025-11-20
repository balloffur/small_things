#include <vector>
#include "primetest_int32.h"

void factor(int n, std::vector<int>& factors){
    if(n==1 || n==0){
        factors.push_back(n);
        return;
    }
    factors.reserve(16);
    constexpr int list_of_primes[] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53};
    for(auto i : list_of_primes){
        while(n % i == 0){
            n /= i;
            factors.push_back(i);
        }
        if(n == 1) {
            return;
        }
    }

    if(MillerRabbin(n)){
        factors.push_back(n);
        return;
    }

    for(int i = 59; i*i <= n; i += 6){
        while(n % i == 0){
            n /= i;
            factors.push_back(i);
        }
        while(n % (i+2) == 0){
            n /= (i+2);
            factors.push_back(i+2);
        }
    }

    if(n > 1){
        factors.push_back(n);
    }
}
