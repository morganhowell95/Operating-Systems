#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

#define SUPERBLOCK_DEPLETE "FREE-TEST%d: Expecting superblock to be depleted\n"
#define SUPERBLOCK_CREATE "FREE-TEST%d: Expect new superblock creation:\n"
#define SUPERBLOCK_BLANK "FREE-TEST%d: Expect a blank superblock:\n"
#define SEPARATOR "--------------------------------\n"
#define RESET "\x1B[0m"
#define RED   "\x1B[31m"
#define GREEN   "\x1b[32m"
#define PASSED "\n test case passed \n"

/***************************************************************
 *			TESTING FREE FUNCTIONALITY		   				   *
 *			NUMBER OF TEST CASES:	6						   *
 ****************************************************************/

int testFreeBlockFromLevelZeroNoInternalFrag() {
	printf(RED "FREE-TEST1: Testing freeing of block from level zero with no internal fragmentation\n" RESET);
	int assert = 1;
	int *x = malloc(32);
	int *y = malloc(32);
	*x = 100;
	free(y);
	assert &= (*x==100);
	free(x);
	assert &= (*x != 100);
	printf(GREEN SUPERBLOCK_DEPLETE RESET, 1);
	x = malloc(32);
	y = malloc(32);
	free(x);
	free(y);
	printf(GREEN SUPERBLOCK_DEPLETE RESET, 1);
	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}

int testFreeBlockFromLevelThreeWithInternalFrag() {
	printf(RED "FREE-TEST2: Testing free functionality on level three with internal fragmentation\n" RESET);
	int assert = 1;
	int *x = malloc(250);
	*x = 5000;
	int *y = malloc(250);
	free(y);
	assert &= (*x==5000);
	assert(assert);
	free(x);
	assert &= (*x!=5000);
	assert(assert);
	printf(GREEN SUPERBLOCK_DEPLETE RESET, 2);

	x = malloc(250);
	*x = 6000;
	y = malloc(250);
	*y = 5000;
	assert &= (*x==6000);
	assert(assert);
	assert &= (*y==5000);
	assert(assert);
	free(x);
	assert &= (*x!=6000);
	free(y);
	assert &= (*y!=5000);
	printf(GREEN SUPERBLOCK_DEPLETE RESET, 2);
	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}

int testFreeEntireSuperBlock() {
	printf(RED "FREE-TEST3: Testing the functionality of filling an entire superblock +1, then freeing elements within \n" RESET);
	int *firstSuperBlock[128];
	int i;
	int assert = 1;

	printf(GREEN SUPERBLOCK_CREATE RESET, 3);
	for(i=0;i<127;i++) {
		firstSuperBlock[i] = malloc(32);
		*(firstSuperBlock[i]) = 782;
	}

	printf(GREEN SUPERBLOCK_CREATE RESET, 3);
	//last index is a pointer to the next super block
	firstSuperBlock[127] = malloc(32);
	*(firstSuperBlock[127]) = 986;

	//testing to make sure nothing happened with our values when making the 2nd superblock
	assert &= (*(firstSuperBlock[50]) == 782);
	assert(assert);
	assert &= (*(firstSuperBlock[127]) == 986);
	assert(assert);

	//we will first free something in the middle of the first super block and make sure the second
	//is not affected
	printf(GREEN  SUPERBLOCK_BLANK RESET, 3);
	free(firstSuperBlock[51]);
	assert &= (*(firstSuperBlock[51]) != 782);
	assert(assert);
	assert &= (*(firstSuperBlock[50]) == 782);
	assert(assert);
	assert &= (*(firstSuperBlock[127]) == 986);
	assert(assert);

	//freeing this pointer should trigger an entirely empty super block
	free(firstSuperBlock[127]);
	for(i=0;i<127;i++) {
		free(firstSuperBlock[i]);
	}

	assert &= (*(firstSuperBlock[50]) != 782);
	assert(assert);
	assert &= (*(firstSuperBlock[127]) != 986);
	assert(assert);
	printf(GREEN SUPERBLOCK_BLANK RESET, 3);

	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}

int testMallocTwoSuperBlocksFullThenFreeTheEntireTwoSuperBlocks() {
	printf(RED "FREE-TEST4: Fill two super blocks full and free - then reuse the blocks\n" RESET);
	int assert = 1;

	printf(GREEN SUPERBLOCK_CREATE RESET, 4);
	int *x = malloc(2 << 10);
	*x = 5000;

	printf(GREEN SUPERBLOCK_CREATE RESET, 4);
	int *y = malloc(2 << 10);
	*y = 4300;

	//ensuring values remain 
	assert &= (*x == 5000);
	assert(assert);
	assert &= (*y == 4300);
	assert(assert);

	printf(GREEN SUPERBLOCK_BLANK RESET, 4);
	free(x);
	assert &= (*x != 5000);
	assert(assert);
	assert &= (*y == 4300);
	assert(assert);

	printf(GREEN SUPERBLOCK_BLANK RESET, 4);
	free(y);
	assert &= (*x != 5000);
	assert(assert);
	assert &= (*y != 4300);
	assert(assert);

	//Since we went through one routine of filling and destoying the contents of the superblocks
	//we now want to make sure we can safely resuse them (as expected)
	printf(GREEN SUPERBLOCK_CREATE RESET, 4);
	x = malloc(2 << 10);
	*x = 5000;

	printf(GREEN SUPERBLOCK_CREATE RESET, 4);
	y = malloc(2 << 10);
	*y = 4300;

	//ensuring values remain 
	assert &= (*x == 5000);
	assert(assert);
	assert &= (*y == 4300);
	assert(assert);

	printf(GREEN SUPERBLOCK_BLANK RESET, 4);
	free(x);
	assert &= (*x != 5000);
	assert(assert);
	assert &= (*y == 4300);
	assert(assert);

	printf(GREEN SUPERBLOCK_BLANK RESET, 4);
	free(y);
	assert &= (*x != 5000);
	assert(assert);
	assert &= (*y != 4300);
	assert(assert);

	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}

int testFreeOnDifferentLevelsSimultaneously() {
	printf(RED "FREE-TEST5: Testing previous functionality in different levels simultaneously\n" RESET);
	int assert = 1;
	int *x = malloc(2 << 10);
	int *y = malloc(2 << 9);
	int *z = malloc(2 << 5);
	*x = 21;
	*y = 300;
	*z = 5678;

	//ensuring values remain 
	assert &= (*x == 21);
	assert(assert);
	assert &= (*y == 300);
	assert(assert);
	assert &= (*z == 5678);
	assert(assert);

	printf(GREEN SUPERBLOCK_BLANK RESET, 5);
	free(z);
	assert &= (*x == 21);
	assert(assert);
	assert &= (*y == 300);
	assert(assert);
	assert &= (*z != 5678);
	assert(assert);

	printf(GREEN SUPERBLOCK_BLANK RESET, 5);
	free(y);
	assert &= (*x == 21);
	assert(assert);
	assert &= (*y != 300);
	assert(assert);
	assert &= (*z != 5678);
	assert(assert);

	printf(GREEN SUPERBLOCK_BLANK RESET, 5);
	free(x);
	assert &= (*x != 21);
	assert(assert);
	assert &= (*y != 300);
	assert(assert);
	assert &= (*z != 5678);
	assert(assert);
	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}

int testFreeSomethingThatDoesntExist() {
	printf(RED "FREE-TEST6: Freeing NULL\n" RESET);
	int assert = 1;
	free(NULL);
	//we just want to make sure this doesn't throw an error
	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}


int main() {
	assert(testFreeBlockFromLevelZeroNoInternalFrag());
	assert(testFreeBlockFromLevelThreeWithInternalFrag());
	assert(testFreeEntireSuperBlock());
	assert(testMallocTwoSuperBlocksFullThenFreeTheEntireTwoSuperBlocks());
	assert(testFreeOnDifferentLevelsSimultaneously());
	assert(testFreeSomethingThatDoesntExist());
	return (errno);
}
