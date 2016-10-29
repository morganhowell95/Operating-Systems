all: th_alloc test

th_alloc: th_alloc.c
	gcc -fPIC -Wall -Wextra -g -shared th_alloc.c -o th_alloc.so

test: testMalloc.c testFree.c testPoison.c testFreePagesBack.c test.c
	gcc -Wall -Wextra -g testMalloc.c -o testMalloc
	gcc -Wall -Wextra -g testFree.c -o testFree
	gcc -Wall -Wextra -g testPoison.c -o testPoison
	gcc -Wall -Wextra -g testFreePagesBack.c -o testFreePagesBack
	gcc -Wall -Wextra -g test.c -o test

clean:
	rm testMalloc testFree testPoison testFreePagesBack test th_alloc.so 

