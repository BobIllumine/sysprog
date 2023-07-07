//
// Created by riseinokoe on 03.07.23.
//

#ifndef SYSPROG_CORO_UTIL_H
#define SYSPROG_CORO_UTIL_H
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>

typedef struct {
    uint64_t id;
    uint64_t start_time;
    uint64_t total_time;
    uint64_t timeout;
} coro_ctx;

uint64_t coro_gettime();

typedef struct {
    coro_ctx *ctx;
    char *filename;
} coro_arg;

static void merge(long arr[], int l, int m, int r);

static void merge_sort(coro_ctx *ctx, long arr[], int l, int r);

#endif //SYSPROG_CORO_UTIL_H
