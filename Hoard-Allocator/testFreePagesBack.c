#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

#define RESET "\x1B[0m"
#define RED   "\x1B[31m"
#define GREEN   "\x1b[32m"
#define MAL_NORM "\nnormal malloc below 32: %p\n"
#define MAL_500 "\nmalloc of 500: %p\n"
#define SUPERBLOCK_CREATE "\nTEST: Expect new superblock creation:\n"
#define BORDER "\n--------------------------------------------------\n"
#define PASSED "\n test case passed \n"
#define FREE_POISON 0xab
#define ALLOC_POISON 0xcd
#define SUPER_BLOCK_SIZE 4096
#define SUPER_BLOCK_MASK (~(SUPER_BLOCK_SIZE-1))

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
		printf(BORDER);
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
		printf(BORDER);
		return 1;
	}
	
}

int main() {
	assert(testNormalCaseAtLevel11());
	assert(testReturnPagesThatAreSeparatedByUsedBlock());
	return(errno);
}

