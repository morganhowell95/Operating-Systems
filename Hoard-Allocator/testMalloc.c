#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

#define SUPERBLOCK_DEPLETE "MALLOC-TEST%d: Expecting superblock to be depleted\n"
#define SUPERBLOCK_CREATE "MALLOC-TEST%d: Expect new superblock creation:\n"
#define SUPERBLOCK_BLANK "MALLOC-TEST%d: Expect a blank superblock:\n"
#define SENT_MALLOC_VAL 879
#define SEPARATOR "--------------------------------\n"
#define RESET "\x1B[0m"
#define RED   "\x1B[31m"
#define GREEN   "\x1b[32m"
#define PASSED "\n test case passed \n"
/***************************************************************
*			TESTING MALLOC FUNCTIONALITY					                   *
*			NUMBER OF TEST CASES:	6						                       *
****************************************************************/

int testMallocNormalBelow32Input() {
	printf(RED "MALLOC-TEST1: Very basic malloc test of single level 0 element\n" RESET);
	int *x = malloc(5);
	*x = SENT_MALLOC_VAL;
	int assert = (x != NULL) && (*x == SENT_MALLOC_VAL);
	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}


int testMallocNormalLevel4() {
	printf(RED "MALLOC-TEST2: Very basic malloc test of single level 4 element\n" RESET);
	int *x = malloc(500);
	*x = SENT_MALLOC_VAL;
	int assert = (x != NULL) && (*x == SENT_MALLOC_VAL);
	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}

int testMallocDepleteLevelOneAndTestForNewSuperBlock() {
	printf(RED "MALLOC-TEST3: Creating two new superblocks son level 0\n" RESET);
	int i;
	int assert = 1;
	printf(GREEN SUPERBLOCK_CREATE RESET, 3);
	int *levelZeroValues[128];

	//Zeroeth level should be filled completely
	for(i=0;i<127;i++) {
		levelZeroValues[i] = malloc(32);
		*(levelZeroValues[i]) = SENT_MALLOC_VAL;
	}

	printf(GREEN SUPERBLOCK_CREATE RESET, 3);
	//the next call should trigger the allocation of a brand new superblock to be affixed at level 0
	levelZeroValues[127] = malloc(32);
	*(levelZeroValues[127]) = SENT_MALLOC_VAL + 23;

	for(i=0;i<127;i++) {
		assert &= (levelZeroValues[i]!=NULL) && (*(levelZeroValues[i])==SENT_MALLOC_VAL);
	}
	assert &= (levelZeroValues[127]!=NULL) && (*(levelZeroValues[127])==SENT_MALLOC_VAL+23);

	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}


int testMallocFillMultipleLevelsAndDepleteMultiple() {
	printf(RED "MALLOC-TEST4: filling multiple levels \n" RESET);
	int i;
	int assert=1;
	int *level10[3];

	for(i=0;i <3; i++) {
		level10[i] = malloc(2 << 9);
		*(level10[i]) = SENT_MALLOC_VAL;
	}
	int *x = malloc(2 << 8);
	*x = SENT_MALLOC_VAL;

	for(i=0;i <3; i++) {
		assert &= (*(level10[i]) == SENT_MALLOC_VAL);
	}
	assert &= (*x == SENT_MALLOC_VAL);

	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}

int testMallocNegativeCaseAllocateZeroMem() {
	printf(RED "MALLOC-TEST5: Mallocing 0\n" RESET);
	void *x = malloc(0);
	int assert = (x==NULL);
	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}

int testMallocNegativeCaseAllocateTooMuchMem() {
	printf(RED "MALLOC-TEST6: Mallocing above max val\n" RESET);
	void *x = malloc(99999999999);
	int assert = (x==NULL);
	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}

int main() {
		assert(testMallocNormalBelow32Input());
		assert(testMallocNormalLevel4());
		assert(testMallocDepleteLevelOneAndTestForNewSuperBlock());
		assert(testMallocFillMultipleLevelsAndDepleteMultiple());
		assert(testMallocNegativeCaseAllocateZeroMem());
		assert(testMallocNegativeCaseAllocateTooMuchMem());
		return (errno);
}

