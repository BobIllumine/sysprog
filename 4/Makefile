GCC_FLAGS = -Wextra -Wall -Wno-gnu-folding-constant
UTILS = ../utils

all: build

build: test.o thread_pool.o $(UTILS)/heap_help/heap_help.c
	gcc $(GCC_FLAGS) test.o thread_pool.o $(UTILS)/heap_help/heap_help.c

test.o: test.c
	gcc $(GCC_FLAGS) -c test.c -o test.o -I $(UTILS)

thread_pool.o: thread_pool.c
	gcc $(GCC_FLAGS) -c thread_pool.c -o thread_pool.o
