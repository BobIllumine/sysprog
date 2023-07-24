//
// Created by riseinokoe on 03.07.23.
//
#define _POSIX_C_SOURCE 200809
#include "coro_util.h"
#include "libcoro.h"
#define min(x, y) (x < y ? x : y)

uint64_t coro_gettime() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)(time.tv_sec * 1e6) + (uint64_t)(time.tv_nsec / 1000);
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

void iter_merge_sort(long arr[], long size) {
    // ctx->start_time = coro_gettime();
    for(int c_size = 1; c_size < size; c_size *= 2) {
        for(int l = 0; l < size - 1; l += 2 * c_size) {
            int m = min(l + c_size - 1, size - 1),
                r = min(l + 2 * c_size - 1, size - 1);
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
    }
    // ctx->total_time += coro_gettime() - ctx->start_time;
    // ++ctx->s_cnt;
}

void quick_sort(coro_ctx *ctx, long arr[], int l, int r) {
    if(l < r) {
        long pivot = arr[r];
        int i = l - 1;
        for(int j = l; j < r; ++j)
            if(arr[j] < pivot) {
                ++i;
                long tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
            }
        long tmp = arr[i + 1];
        arr[i + 1] = arr[r];
        arr[r] = tmp;
        ctx->start_time = coro_gettime();
        quick_sort(ctx, arr, l, i);
        ctx->start_time = coro_gettime();
        quick_sort(ctx, arr, i + 2, r);
    }
    ctx->total_time += coro_gettime() - ctx->start_time;
    ++ctx->s_cnt;
    coro_yield();
}