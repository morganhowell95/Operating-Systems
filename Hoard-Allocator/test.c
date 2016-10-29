#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

#define SUPERBLOCK_DEPLETE "%s%d: Expecting superblock to be depleted\n"
#define SUPERBLOCK_CREATE "%s%d: Expect new superblock creation:\n"
#define SUPERBLOCK_BLANK "%s%d: Expect a blank superblock:\n"
#define SEPARATOR "--------------------------------\n"
#define RESET "\x1B[0m"
#define RED   "\x1B[31m"
#define GREEN   "\x1b[32m"
#define PASSED "\n test case passed \n"
#define SENT_MALLOC_VAL 879
#define FREE_POISON 0xab
#define ALLOC_POISON 0xcd
#define SUPER_BLOCK_SIZE 4096
#define SUPER_BLOCK_MASK (~(SUPER_BLOCK_SIZE-1))

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
	printf(GREEN SUPERBLOCK_CREATE RESET, "MALLOC-TEST" , 3);
	int *levelZeroValues[128];

	//Zeroeth level should be filled completely
	for(i=0;i<127;i++) {
		levelZeroValues[i] = malloc(32);
		*(levelZeroValues[i]) = SENT_MALLOC_VAL;
	}

	printf(GREEN SUPERBLOCK_CREATE RESET, "MALLOC-TEST" , 3);
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
	printf(GREEN SUPERBLOCK_DEPLETE RESET,"FREE-TEST", 1);
	x = malloc(32);
	y = malloc(32);
	free(x);
	free(y);
	printf(GREEN SUPERBLOCK_DEPLETE RESET,"FREE-TEST", 1);
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
	printf(GREEN SUPERBLOCK_DEPLETE RESET,"FREE-TEST", 2);

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
	printf(GREEN SUPERBLOCK_DEPLETE RESET,"FREE-TEST", 2);
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

	printf(GREEN SUPERBLOCK_CREATE RESET, "FREE-TEST", 3);
	for(i=0;i<127;i++) {
		firstSuperBlock[i] = malloc(32);
		*(firstSuperBlock[i]) = 782;
	}

	printf(GREEN SUPERBLOCK_CREATE RESET, "FREE-TEST", 3);
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
	printf(GREEN  SUPERBLOCK_BLANK RESET,"FREE-TEST", 3);
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
	printf(GREEN SUPERBLOCK_BLANK RESET,"FREE-TEST", 3);

	if(assert) {
		printf(GREEN PASSED RESET);
	}
	printf(SEPARATOR);
	return assert;
}

int testMallocTwoSuperBlocksFullThenFreeTheEntireTwoSuperBlocks() {
	printf(RED "FREE-TEST4: Fill two super blocks full and free - then reuse the blocks\n" RESET);
	int assert = 1;

	printf(GREEN SUPERBLOCK_CREATE RESET, "FREE-TEST", 4);
	int *x = malloc(2 << 10);
	*x = 5000;

	printf(GREEN SUPERBLOCK_CREATE RESET, "FREE-TEST", 4);
	int *y = malloc(2 << 10);
	*y = 4300;

	//ensuring values remain 
	assert &= (*x == 5000);
	assert(assert);
	assert &= (*y == 4300);
	assert(assert);

	printf(GREEN SUPERBLOCK_BLANK RESET,"FREE-TEST", 4);
	free(x);
	assert &= (*x != 5000);
	assert(assert);
	assert &= (*y == 4300);
	assert(assert);

	printf(GREEN SUPERBLOCK_BLANK RESET,"FREE-TEST", 4);
	free(y);
	assert &= (*x != 5000);
	assert(assert);
	assert &= (*y != 4300);
	assert(assert);

	//Since we went through one routine of filling and destoying the contents of the superblocks
	//we now want to make sure we can safely resuse them (as expected)
	printf(GREEN SUPERBLOCK_CREATE RESET,"FREE-TEST", 4);
	x = malloc(2 << 10);
	*x = 5000;

	printf(GREEN SUPERBLOCK_CREATE RESET,"FREE-TEST", 4);
	y = malloc(2 << 10);
	*y = 4300;

	//ensuring values remain 
	assert &= (*x == 5000);
	assert(assert);
	assert &= (*y == 4300);
	assert(assert);

	printf(GREEN SUPERBLOCK_BLANK RESET,"FREE-TEST", 4);
	free(x);
	assert &= (*x != 5000);
	assert(assert);
	assert &= (*y == 4300);
	assert(assert);

	printf(GREEN SUPERBLOCK_BLANK RESET,"FREE-TEST", 4);
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

	printf(GREEN SUPERBLOCK_BLANK RESET,"FREE-TEST", 5);
	free(z);
	assert &= (*x == 21);
	assert(assert);
	assert &= (*y == 300);
	assert(assert);
	assert &= (*z != 5678);
	assert(assert);

	printf(GREEN SUPERBLOCK_BLANK RESET,"FREE-TEST", 5);
	free(y);
	assert &= (*x == 21);
	assert(assert);
	assert &= (*y != 300);
	assert(assert);
	assert &= (*z != 5678);
	assert(assert);

	printf(GREEN SUPERBLOCK_BLANK RESET, "FREE-TEST", 5);
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


/* Object: One return from malloc/input to free. */
struct __attribute__((packed)) object {
  union {
    struct object *next; // For free list (when not in use)
    char * raw; // Actual data
  };
};

/* Super block bookeeping; one per superblock.  "steal" the first
 * object to store this structure
 */
struct __attribute__((packed)) superblock_bookkeeping {
  struct superblock_bookkeeping * next; // next super block
  struct object *free_list;
  // Free count in this superblock
  uint8_t free_count; // Max objects per superblock is 128-1, so a byte is sufficient
  uint8_t level;
};

/* Superblock: a chunk of contiguous virtual memory.
 * Subdivide into allocations of same power-of-two size. */
struct __attribute__((packed)) superblock {
  struct superblock_bookkeeping bkeep;
  void *raw;  // Actual data here
};

/***************************************************************
*			TESTING RETURN FREE PAGES FUNCTIONALITY	           *
*			NUMBER OF TEST CASES:	2	                       *
****************************************************************/

static inline
struct superblock_bookkeeping * obj2bkeep (void *ptr) {
	  uint64_t addr = (uint64_t) ptr;
	    addr &= SUPER_BLOCK_MASK;
		  return (struct superblock_bookkeeping *) addr;
}

int testNormalCaseAtLevel11() {
	printf(RED "RETURN-OS-TEST1:  Filling five super blocks full at level 11\n" RESET);
	int* contiguous[5];
	int i;
	for(i=0;i<5;i++) {
		contiguous[i] = malloc(2<<10);
	}
	*(contiguous[0]) = 123456;
	for(i=4;i>0;i--) {
		free(contiguous[i]);
	}

	int retentionValueAfterFree = *(contiguous[0]);
	//ensure that at least two of the blocks are no longer mapped in memory
	int noMemCount = 0;
	for(i=0;i<5;i++) {
		struct superblock_bookkeeping *bkeep = obj2bkeep(contiguous[i]);
		msync(bkeep, SUPER_BLOCK_SIZE, 0);
		if(errno == ENOMEM) {
			noMemCount++;
		}
		errno = 0;
	}

	free(contiguous[0]);
	if(noMemCount < 2) {
		printf(RED "\n ERROR-RETURN-OS-TEST1: Memory is still mapped that should be unmapped \n" RESET);
		return 0;
	} else if(noMemCount > 2 || retentionValueAfterFree != 123456) {
		printf(RED "\n ERROR-RETURN-OS-TEST1: Memory has been unmapped that should still be mapped\n" RESET);
		return 0;
	} else {
		printf(GREEN PASSED RESET);
		printf(SEPARATOR);
		return 1;
	}
}


//covers the case of two free superblocks that are separated by a block in use
//before: FREE->USED->FREE after: USED
int testReturnPagesThatAreSeparatedByUsedBlock() {
	printf(RED "RETURN-OS-TEST2:  Filling five super blocks full at level 11 and emptying disparate columns\n" RESET);
	int *contiguous[10];
	int i;
	for(i=0;i<10;i++) {
		contiguous[i] = malloc(2<<10);
	}
	//stud values that will be tested for interference
	*(contiguous[0]) = 654321;
	*(contiguous[3]) = 654321;
	*(contiguous[7]) = 654321;
	*(contiguous[9]) = 654321;

	free(contiguous[2]);
	free(contiguous[4]);
	free(contiguous[6]);
	free(contiguous[8]);

	//test to make sure values were not manipulated after the calls to free
	int retentionVals[4] = {0, 3, 7, 9};
	int hasRetention = 1;

	for(i=0;i<4;i++){
		if(*(contiguous[retentionVals[i]]) != 654321) {
			hasRetention = 0;
		}
	}

	//ensure that at least two of the blocks are no longer mapped in memory
	int noMemCount = 0;
	for(i=0;i<10;i++) {
		struct superblock_bookkeeping *bkeep = obj2bkeep(contiguous[i]);
		msync(bkeep, SUPER_BLOCK_SIZE, 0);
		if(errno == ENOMEM) {
			noMemCount++;
		}
		errno = 0;
	}
	
	if(noMemCount < 2) {
		printf(RED "ERROR-RETURN-OS-TEST2: Memory is still mapped that should be unmapped \n" RESET);
		return 0;
	} else if(noMemCount > 2 || !hasRetention) {
		printf(RED "ERROR-RETURN-OS-TEST2: Memory has been unmapped that should still be mapped\n" RESET);
		return 0;
	} else {
		printf(GREEN PASSED RESET);
		printf(SEPARATOR);
		return 1;
	}
	
}

int main() {
	assert(testMallocNormalBelow32Input());
	assert(testMallocNormalLevel4());
	assert(testMallocDepleteLevelOneAndTestForNewSuperBlock());
	assert(testMallocFillMultipleLevelsAndDepleteMultiple());
	assert(testMallocNegativeCaseAllocateZeroMem());
	assert(testMallocNegativeCaseAllocateTooMuchMem());
	assert(testFreeBlockFromLevelZeroNoInternalFrag());
	assert(testFreeBlockFromLevelThreeWithInternalFrag());
	assert(testFreeEntireSuperBlock());
	assert(testMallocTwoSuperBlocksFullThenFreeTheEntireTwoSuperBlocks());
	assert(testFreeOnDifferentLevelsSimultaneously());
	assert(testFreeSomethingThatDoesntExist());
	assert(testMallocPoisonNormalBelow32Input());
	assert(testMallocPoisonLargestBlock());
	assert(testFreePoisonNormalBelow32Input());
	assert(testFreePoisonLargestBlock());
	assert(testFreePoisonInTheMiddleOfSuperBlock());
	assert(testNormalCaseAtLevel11());
	assert(testReturnPagesThatAreSeparatedByUsedBlock());
	printf(GREEN "\n\b ALL TEST CASES PASSED!!! \n" RESET);
	return (errno);
}

