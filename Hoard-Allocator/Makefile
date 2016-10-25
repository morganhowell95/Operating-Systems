all: th_alloc test

th_alloc: th_alloc.c
	gcc -fPIC -pedantic -Wall -Wextra -g -shared th_alloc.c -o th_alloc.so

test: test.c
	gcc -pedantic -Wall -Wextra -g test.c -o test

clean:
	rm test th_alloc.so

