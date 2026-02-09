#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void LSD_radix_u32(int *a, size_t n) {  
    static_assert(sizeof(int)==4,"int must be 32");
    if (n <= 1) return;

    int *b = (int*)malloc((size_t)n * sizeof(int));
    if (!b) return;
    

    int *src = a;
    int *dst = b;

    for (int pass = 0; pass < 4; ++pass) {
        int c[256] = {0};
        int shift = pass * 8;

        // count
        for (int i = 0; i < n; ++i) {
            int x = (int)src[i];
            int d = (int)((x >> shift) & 255u);
            // Для кореектной сортировки отрицательных
            if(pass==3){
                d ^=128;
            }
            c[d]++;
        }

        // prefix sums
        for (int i = 1; i < 256; ++i) c[i] += c[i - 1];

        // stable scatter (с конца!)
        for (int i = n - 1; i >= 0; --i) {
            int x = (int)src[i];
            int d = (int)((x >> shift) & 255u);
            // Для кореектной сортировки отрицательных
            if(pass==3){
                d ^=128;
            }
            dst[--c[d]] = src[i];
        }

        // swap
        int *tmp = src;
        src = dst;
        dst = tmp;
    }

    // 4 прохода => результат снова в a, копировать не надо
    free(b);
}


int main(){
    int a[12]={1,24,5,2,34,-5,2,3,4,-212,2,2};
    LSD_radix_u32(a,12);
    for(int i=0;i<12;i++){
        printf("%d ",a[i]);
    }
}
