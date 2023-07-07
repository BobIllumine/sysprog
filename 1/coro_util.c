//
// Created by riseinokoe on 03.07.23.
//

#include "coro_util.h"


uint64_t coro_gettime() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)(time.tv_sec * 1000000) + (uint64_t)(time.tv_nsec / 1000);
}

void merge(long arr[], int l, int m, int r) {
    int size_l = m - l + 1,
            size_r = r - m;

    // Allocating memory dynamically because stack size is too small
    long *arr_l = (long *)malloc(sizeof(long) * size_l),
            *arr_r = (long *)malloc(sizeof(long) * size_r);

    for(int i = 0; i < size_l; ++i)
        arr_l[i] = arr[l + i];
    for(int j = 0; j < size_r; ++j)
        arr_r[j] = arr[m + j + 1];

    int i = 0, j = 0, k = l;
    while(i < size_l && j < size_r) {
        if(arr_l[i] <= arr_r[j]) {
            arr[k] = arr_l[i];
            ++i;
        }
        else {
            arr[k] = arr_r[j];
            ++j;
        }
        ++k;
    }
    while(i < size_l) {
        arr[k] = arr_l[i];
        ++i, ++k;
    }

    while(j < size_r) {
        arr[k] = arr_r[j];
        ++j, ++k;
    }
    free(arr_l);
    free(arr_r);
}

void merge_sort(coro_ctx *ctx, long arr[], int l, int r) {

}