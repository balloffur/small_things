#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void merge_sort_rec(int *a, int size, int *temp) {
    if (size <= 1) return;

    int left  = size / 2;
    int right = size - left;

    merge_sort_rec(a,        left,  temp);
    merge_sort_rec(a + left, right, temp);

    int i = 0, j = 0, k = 0;

    while (i < left && j < right) {
        if (a[i] <= a[left + j]) temp[k++] = a[i++];
        else                     temp[k++] = a[left + j++];
    }
    while (i < left)  temp[k++] = a[i++];
    while (j < right) temp[k++] = a[left + j++];

    memcpy(a, temp, (size_t)size * sizeof *a);
}

void merge_sort(int *a, int size) {
    if (!a || size <= 1) return;

    int *temp = malloc((size_t)size * sizeof *temp);
    if (!temp) return;

    merge_sort_rec(a, size, temp);
    free(temp);
}

int main(void) {
    int a[] = {5, 2, 9, 1, 5, 6, 0, -3};
    int n = (int)(sizeof a / sizeof a[0]);

    merge_sort(a, n);

    for (int i = 0; i < n; i++) printf("%d ", a[i]);
    printf("\n");
}
