#include <stdio.h>
#include <string.h>
#include "libcoro.h"
#include "coro_util.h"
#include <limits.h>
#include "heap_help.h"

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

	struct coro *this = coro_this();
	coro_arg *arg = (coro_arg*) context;
	/* This will be returned from coro_status(). */
	return 0;
}

int
main(int argc, char **argv)
{
    if(argc < 2) {
        printf("Please provide more args");
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
    char **filenames = (char **) malloc(sizeof(char *) * (argc - optind));
    for(int i = 0; optind < argc; ++optind, ++i) {
        filenames[i] = argv[optind];
    }

	/* Start several coroutines. */
	for (int i = 0; i < coro_num; ++i) {
        coro_ctx new_ctx;
        new_ctx.id = i;
        new_ctx.start_time = 0;
        new_ctx.total_time = 0;
        new_ctx.timeout = timeout / coro_num;
        coro_arg new_arg;
        new_arg.ctx = &new_ctx;
		/*
		 * I have to copy the name. Otherwise, all the coroutines would
		 * have the same name when they finally start.
		 */
		coro_new(coroutine_func_f, &new_arg);
	}
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
	return 0;
}
