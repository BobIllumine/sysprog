GCC_FLAGS = -Wextra -Wall -Wno-gnu-folding-constant -ldl -rdynamic
FILES = test1.txt test2.txt test3.txt test4.txt test5.txt test6.txt
OPTIONS = -n 15 -T 300

all: libcoro.c solution.c coro_util.c ../utils/heap_help/heap_help.c
	gcc $(GCC_FLAGS) libcoro.c solution.c coro_util.c ../utils/heap_help/heap_help.c -I ../utils/heap_help
	./a.out $(OPTIONS) $(FILES)

clean:
	rm a.out
