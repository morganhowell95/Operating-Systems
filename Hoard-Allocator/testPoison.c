#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

#define RESET "\x1B[0m"
#define RED   "\x1B[31m"
#define GREEN   "\x1b[32m"
#define MAL_NORM "\nnormal malloc below 32: %p\n"
#define MAL_500 "\nmalloc of 500: %p\n"
#define SUPERBLOCK_DEPLETE "FREE-TEST%d: Expecting superblock to be depleted\n"
#define SUPERBLOCK_CREATE "FREE-TEST%d: Expect new superblock creation:\n"
#define SUPERBLOCK_BLANK "FREE-TEST%d: Expect a blank superblock:\n"
#define SEPARATOR "--------------------------------\n"
#define PASSED "\n test case passed \n"
#define FREE_POISON 0xab
#define ALLOC_POISON 0xcd

/***************************************************************
 *			TESTING MALLOC/FREE POISONING			                       *
 *			NUMBER OF TEST CASES:	5						                       *
 ****************************************************************/

int testMallocPoisonNormalBelow32Input() {
	printf(RED "POISON-TEST1: Malloc poison of size 5 at level 0\n" RESET);
	unsigned char *x = malloc(5);
	int i;
	int assert = 1;
	for(i=0;i<32;i++) {
		assert &= x[i] == ALLOC_POISON;
		if(!assert) {
			printf(RED "poison has failled for malloc of size 32\n" RESET);
			return assert;
		}
	}
	free(x);

	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}

int testMallocPoisonLargestBlock() {
	printf(RED "POISON-TEST2: Malloc poison of size 2048 at level 11\n" RESET);
	unsigned char *x = malloc(2 << 10);
	int i;
	int assert = 1;
	for(i=0;(i<2<<10);i++) {
		assert &= x[i] == ALLOC_POISON;
		if(!assert) {
			printf(RED "poison has failed for malloc of size 2048\n" RESET);
			return assert;
		}
	}
	free(x);

	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}

//We only want freed objects to be partially poisoned, otherwise we couldn't utilize part of their memory space
//for linkage in the super block's free list
//Taking into account the next attribute, everything but the first 8 bytes should be poisoned
int isPartiallyPoisoned(unsigned char *freedObj, int size) {
	int containsFreePoison = 1;
	int tick;
	for(tick=8;tick<size;tick++) {
		if(freedObj[tick] != FREE_POISON) {
			containsFreePoison = 0;
		}
	}
	return containsFreePoison;
}

int testFreePoisonNormalBelow32Input() {
	printf(RED "POISON-TEST3: First basic free poison test on input of size below size 32\n" RESET);
	int *x = malloc(4);
	printf(GREEN "free poison for 32 bytes at level 0\n" RESET);
	printf("malloc before set <%d>\n", *x);
	*x = 5;
	assert(*x==5);
	printf("malloc after set: %d\n", *x);
	free(x);
	printf(GREEN "testing for poison values after free call..\n" RESET);
	//testing free poison values
	int assert = 1;
	unsigned char *y = (unsigned char *) x;
	assert &= isPartiallyPoisoned(y, 32);
	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}

int testFreePoisonLargestBlock() {
	printf(RED "POISON-TEST4: Free poison for 2048 bytes at level 11\n" RESET);
	int *x = malloc(2 << 10);
	printf("malloc before set <%d>\n", *x);
	*x = 7654321;
	assert(*x==7654321);
	printf("malloc after set: %d\n", *x);
	free(x);
	int assert = 1;
	unsigned char *y = (unsigned char *) x;
	assert &= isPartiallyPoisoned(y, 2<<10);
	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}



int testFreePoisonInTheMiddleOfSuperBlock() {
	printf(RED "POISON-TEST5: Testing free poison values in the middle of a superblock\n" RESET);
	int assert=1;
	char *contiguous[5];
	int i;
	for(i=0;i<5;i++) {
		contiguous[i] = malloc(32);
	}

	free(contiguous[2]);

	//check that middle block has been poisoned
	unsigned char *y = (unsigned char *) contiguous[2];
	assert &= isPartiallyPoisoned(y, 32);
	if(!assert) {
		printf(RED "poison values for the first free call on level 0 has failed\n" RESET);
		return assert;
	}

	//check that blocks around remain unmanipulated
	y = (unsigned char *) contiguous[1];
	assert &= isPartiallyPoisoned(y, 32);
	if(assert) {
		printf(RED "objects were unintentionally poisoned\n" RESET);
		return !assert;
	} else {
		assert = 1;
	}

	y = (unsigned char *) contiguous[3];
	assert &= isPartiallyPoisoned(y, 32);
	if(assert) {
		printf(RED "objects were unintentionally poisoned\n" RESET);
		return !assert;
	} else {
		assert = 1;
	}
	free(contiguous[0]);
	free(contiguous[1]);
	free(contiguous[3]);
	free(contiguous[4]);

	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}

int main() {
	assert(testMallocPoisonNormalBelow32Input());
	assert(testMallocPoisonLargestBlock());
	assert(testFreePoisonNormalBelow32Input());
	assert(testFreePoisonLargestBlock());
	assert(testFreePoisonInTheMiddleOfSuperBlock());
	return (errno);
}

