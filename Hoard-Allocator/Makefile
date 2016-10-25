all: th_alloc test

th_alloc: th_alloc.c
	gcc -fPIC -Wall -Wextra -g -shared th_alloc.c -o th_alloc.so

test: test.c
	gcc -Wall -Wextra -g test.c -o test
	gcc -Wall -Wextra -g testMalloc.c -o testMalloc
	gcc -Wall -Wextra -g testFree.c -o testFree

clean:
	rm test th_alloc.so

