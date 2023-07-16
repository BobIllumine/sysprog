GCC_FLAGS = -Wextra -Werror -Wall -ldl -rdynamic -Wno-gnu-folding-constant
UTILS = ../utils

all:  build

build: test.o userfs.o $(UTILS)/heap_help/heap_help.c
	gcc $(GCC_FLAGS) test.o userfs.o $(UTILS)/heap_help/heap_help.c

test.o: test.c
	gcc $(GCC_FLAGS) -c test.c -o test.o -I $(UTILS)

userfs.o: userfs.c
	gcc $(GCC_FLAGS) -c userfs.c -o userfs.o

clean:
	rm ./test.o ./userfs.o
	rm ./a.out