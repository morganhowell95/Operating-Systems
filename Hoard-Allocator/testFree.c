#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

/***************************************************************
*			TESTING FREE FUNCTIONALITY		   				   *
*			NUMBER OF TEST CASES:	6						   *
****************************************************************/

int testFreeBlockFromLevelZeroNoInternalFrag() {
	int *x = malloc(32);
	int *y = malloc(32);
	free(y);
	free(x);
	printf("\nTEST: Expecting superblock at level 0 to be depleted\n");
	x = malloc(32);
	y = malloc(32);
	free(x);
	free(y);
	printf("\nTEST: Expecting superblock at level 0 to be depleted\n");
	return 1;
}

int testFreeBlockFromLevelThreeWithInternalFrag() {
	int *x = malloc(250);
	int *y = malloc(250);
	free(y);
	free(x);
	printf("\nTEST: Expecting superblock at level 0 to be depleted\n");
	x = malloc(250);
	y = malloc(250);
	free(x);
	free(y);
	printf("\nTEST: Expecting superblock at level 0 to be depleted\n");
	return 1;
}

int testFreeEntireSuperBlock() {
	int *firstSuperBlock[128];
	int i;
	printf("\nExpect new superblock creation:\n");
	for(i=0;i<127;i++) {
		firstSuperBlock[i] = malloc(32);
	}
	printf("\nExpect new superblock creation:\n");
	//actually a pointer to the second super block
	firstSuperBlock[127] = malloc(32);
	//freeing this pointer should trigger an entirely empty super block
	printf("\nExpect a blank superblock:\n");
	free(firstSuperBlock[127]);
	for(i=0;i<127;i++) {
		free(firstSuperBlock[i]);
	}
	printf("\nExpect a blank superblock:\n");
	return 1;
}

int testMallocTwoSuperBlocksFullThenFreeTheEntireTwoSuperBlocks() {
	printf("\nExpect new superblock creation:\n");
	int *x = malloc(2 << 10);
	printf("\nExpect new superblock creation:\n");
	int *y = malloc(2 << 10);
	printf("\nExpect a blank superblock:\n");
	free(x);
	printf("\nExpect a blank superblock:\n");
	free(y);
	return 1;
}

int testFreeOnDifferentLevelsSimultaneously() {
	int *x = malloc(2 << 10);
	int *y = malloc(2 << 9);
	int *z = malloc(2 << 5);
	free(z);
	free(y);
	free(x);
	return 1;
}

int testFreeSomethingThatDoesntExist() {
	int *x = malloc(2 << 8);
	free(x);
	free(x);
	return 1;
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
