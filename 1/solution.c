#include <stdio.h>
#include <string.h>
#include "libcoro.h"
#include "coro_util.h"
#include <limits.h>
#include "heap_help.h"
#include <stdlib.h>

/**
 * You can compile and run this code using the commands:
 *
 * $> gcc solution.c libcoro.c coro_util.c
 * $> ./a.out
 */


/**
 * Coroutine body. This code is executed by all the coroutines. Here you
 * implement your solution, sort each individual file.
 */
static int
coroutine_func_f(void *context)
{
	/* IMPLEMENT SORTING OF INDIVIDUAL FILES HERE. */
	// struct coro *this = coro_this(); 
	coro_arg *arg = (coro_arg*) context;
    char *cur_filename = NULL;
    if(*arg->q_size > 0)
        cur_filename = arg->queue[--(*arg->q_size)];
    arg->ctx->start_time = coro_gettime();
    while(cur_filename) {
        FILE *file = fopen(cur_filename, "r");
        if(!file) {
            free(cur_filename);
            return -1;
        } 
        long arr_cnt = 0, raw_size = 10;
        long *raw_arr = (long *) malloc(sizeof(long) * 10);
        while(!feof(file)) {
            if(arr_cnt == raw_size - 1)
                raw_size *= 2, raw_arr = realloc(raw_arr, sizeof(long) * raw_size);
            fscanf(file, "%ld", &raw_arr[arr_cnt++]);
        }
        fclose(file);
        arg->arrays[(*arg->q_size)] = (long*) malloc(sizeof(long) * arr_cnt);
        arg->arr_sizes[(*arg->q_size)] = arr_cnt;
        for(int i = 0; i < arr_cnt; ++i)
            arg->arrays[(*arg->q_size)][i] = raw_arr[i];
        free(raw_arr);
        // Quick sort is much more quicker, but it is too painful to measure recursive function's working time
        iter_merge_sort(arg->arrays[(*arg->q_size)], arr_cnt);
        // quick_sort(arg->ctx, arg->arrays[(*arg->q_size)], 0, arr_cnt - 1);
        uint64_t w_time = coro_gettime() - arg->ctx->start_time;
        // printf("%s, #%d, %lu ms, %lu ms.\n", cur_filename, arg->ctx->id, arg->ctx->timeout, w_time);
        if(w_time > arg->ctx->timeout) {
            arg->ctx->total_time += w_time;
            ++arg->ctx->s_cnt;
            coro_yield();
            arg->ctx->start_time = coro_gettime();
        }
        if(!*arg->q_size)
            break;
        cur_filename = arg->queue[--(*arg->q_size)];
    }
    printf("Coroutine %lu: total working time - %lu mcs, switch count - %d\n", (unsigned long)arg->ctx->id, (unsigned long)arg->ctx->total_time, arg->ctx->s_cnt);
    free(arg->ctx);
    free(arg);
	/* This will be returned from coro_status(). */
	return 0;
}

int
main(int argc, char **argv)
{
    if(argc < 2) {
        printf("Please provide more args.\n");
        exit(EXIT_SUCCESS);
    }

	/* Initialize our coroutine global cooperative scheduler. */
	coro_sched_init();
    int opt,
        coro_num = 1;

    uint64_t timeout = INT_MAX,
            main_start = coro_gettime();

    while((opt = getopt(argc, argv, "hn:T:")) != -1) {
        switch (opt) {
            case 'h':
                printf("Use: <PROGRAM_PATH> [-h] [-n] <CORO_NUM> [-T] <TARGET_LATENCY> <FILE1> <FILE2> ...\n");
                printf("Options: \n");
                printf("[-h]: Help message\n");
                printf("[-n]: Numbers of coroutines\n");
                printf("[-T]: Target latency for coroutines (in mсs)\n");
                exit(EXIT_SUCCESS);
            case 'n':
                coro_num = atoi(optarg);
                break;
            case 'T':
                timeout = atoi(optarg);
                break;
            default:
                break;
        }
    }
    int filenames_size = argc - optind,
        f_size_copy = filenames_size;
    char **filenames = calloc(filenames_size, sizeof(char *));
    for(int i = 0; optind < argc; ++optind, ++i)
        filenames[i] = argv[optind];
    long **all_arrays = (long **) malloc(sizeof(long *) * filenames_size);
    long *all_sizes = calloc(filenames_size, sizeof(long));
	/* Start several coroutines. */
	for (int i = 0; i < coro_num; ++i) {
        coro_ctx *new_ctx = (coro_ctx *) malloc(sizeof(coro_ctx));
        *new_ctx = (coro_ctx) {
            .id = i,
            .start_time = 0,
            .total_time = 0,
            .timeout = timeout / coro_num,
            .s_cnt = 0
        };
        coro_arg *new_arg = (coro_arg *) malloc(sizeof(coro_arg));
        *new_arg = (coro_arg) {
            .ctx = new_ctx,
            .q_size = &filenames_size,
            .queue = filenames,
            .arrays = all_arrays,
            .arr_sizes = all_sizes
        };
		coro_new(coroutine_func_f, new_arg);
	}
    printf("Coroutine creation time - %lu mcs.\n", (coro_gettime() - main_start));
	/* Wait for all the coroutines to end. */
	struct coro *c;
	while ((c = coro_sched_wait()) != NULL) {
		/*
		 * Each 'wait' returns a finished coroutine with which you can
		 * do anything you want. Like check its exit status, for
		 * example. Don't forget to free the coroutine afterwards.
		 */
		printf("Finished %d\n", coro_status(c));
		coro_delete(c);
	}
	/* All coroutines have finished. */
    free(filenames);    
	/* IMPLEMENT MERGING OF THE SORTED ARRAYS HERE. */
    uint64_t merge_s_time = coro_gettime();
    FILE *output = fopen("result.txt", "w");
    long pivot = all_sizes[0];
    long *sorted_arr = (long *) malloc(sizeof(long) * pivot);
    memcpy(sorted_arr, all_arrays[0], pivot * sizeof(long));
    for(int i = 1; i < f_size_copy; ++i) {
        // Reallocate some more memory
        sorted_arr = realloc(sorted_arr, (all_sizes[i] + pivot) * sizeof(long));
        if(!sorted_arr) {
            printf("Not enough memory\n");
            return -1;
        }
        // Concatenate arrays
        for(int j = 0; j < all_sizes[i]; ++j)
            sorted_arr[pivot + j] = all_arrays[i][j];
        // Merge them
        merge(sorted_arr, 0, pivot - 1, pivot + all_sizes[i] - 1);
        // Update total length
        pivot += all_sizes[i];
    }
    // Output
    for(int i = 0; i < pivot; ++i)
        fprintf(output, "%ld ", sorted_arr[i]);
    // printf("%d.\n", f_size_copy);
    for(int i = 0; i < f_size_copy; ++i)
        free(all_arrays[i]);
    free(sorted_arr);
    free(all_arrays);
    free(all_sizes);
    fclose(output);
    printf("Program working time - %lu mcs, merging time - %lu mcs.\n", (unsigned long)(coro_gettime() - main_start), (unsigned long)(coro_gettime() - merge_s_time));
	return 0;
}
