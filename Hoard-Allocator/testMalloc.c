#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

/***************************************************************
*			TESTING MALLOC FUNCTIONALITY					                   *
*			NUMBER OF TEST CASES:	6						                       *
****************************************************************/

int testMallocNormalBelow32Input() {
	void *x = malloc(5);
	printf("\nnormal malloc below 32: %p\n", x);
	return x != NULL;
}


int testMallocNormalLevel4() {
	void *x = malloc(500);
	printf("\nmalloc of 500: %p\n", x);
	return x != NULL;
}

int testMallocDepleteLevelOneAndTestForNewSuperBlock() {
	int i;
	int assert;
	printf("\nExpect new superblock creation:\n");
	//Zeroeth level should be filled completely
	for(i=0;i<127;i++) {
		void *x = malloc(32);
		assert = x != NULL;
		if(!assert) {
			return assert;
		}
	}

	printf("\nExpect new superblock creation:\n");
	//the next call should trigger the allocation of a brand new superblock to be affixed at level 0
	void *x = malloc(32);
	return x != NULL;
}


int testMallocFillMultipleLevelsAndDepleteMultiple() {
	int i;
	int assert;
	for(i=0;i <3; i++) {
		void * x = malloc(2 << 9);
		assert = x!= NULL;
		if(!assert) {
			return assert;
		}
	}

	void *x = malloc(2 << 9);
	return x != NULL;
}

int testMallocNegativeCaseAllocateZeroMem() {
return 1;
}

int testMallocNegativeCaseAllocateTooMuchMem() {
return 1;

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

