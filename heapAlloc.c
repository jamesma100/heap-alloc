///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019-2020 Jim Skrentny
// Posting or sharing this file is prohibited, including any changes/additions.
//
//////////////////////////////////////////////////////////////////////////////
// Main File:        heapAlloc.c
// This File:        heapAlloc.c
// Other Files:      (name of all other files if any)
// Semester:         CS 354 Spring 2020
//
// Author:           James Ma
// Email:            yma255@wisc.edu
// CS Login:         jamesm
//
/////////////////////////// OTHER SOURCES OF HELP //////////////////////////////
//                   fully acknowledge and credit all sources of help,
//                   other than Instructors and TAs.
//
// Persons:          Identify persons by name, relationship to you, and email.
//                   Describe in detail the the ideas and help they provided.
//
// Online sources:   avoid web searches to solve your problems, but if you do
//                   search, be sure to include Web URLs and description of
//                   of any information you find.
/////////////////////////////////////////////////////////////////////////////////
 
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include "heapAlloc.h"
 
/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block but only containing size.
 */
typedef struct blockHeader {           
    int size_status;
    /*
    * Size of the block is always a multiple of 8.
    * Size is stored in all block headers and free block footers.
    *
    * Status is stored only in headers using the two least significant bits.
    *   Bit0 => least significant bit, last bit
    *   Bit0 == 0 => free block
    *   Bit0 == 1 => allocated block
    *
    *   Bit1 => second last bit 
    *   Bit1 == 0 => previous block is free
    *   Bit1 == 1 => previous block is allocated
    * 
    * End Mark: 
    *  The end of the available memory is indicated using a size_status of 1.
    * 
    * Examples:
    * 
    * 1. Allocated block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 27
    *      If the previous block is free, size_status should be 25
    * 
    * 2. Free block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 26
    *      If the previous block is free, size_status should be 24
    *    Footer:
    *      size_status should be 24
    */
} blockHeader;         

/* Global variable - DO NOT CHANGE. It should always point to the first block,
 * i.e., the block at the lowest address.
 */
blockHeader *heapStart = NULL;

/* Size of heap allocation padded to round to nearest page size.
 */
int allocsize;

/*
 * Additional global variables may be added as needed below
 */
blockHeader *currBlock = NULL; //current block
static blockHeader* nextBlockHeader = NULL;
static int heapSize = 0;

/*
 * Helper functions declared below
 */

/*
 * Gets size of block by omitting two least significant bits
 * Argument block: pointer to header of block
 * Returns: size of the block pointed to by block
 */
int getBlockSize(blockHeader* block){
	int blockSize;
	blockSize = block->size_status >> 2; //right shift by 2
	blockSize = blockSize << 2; //left shift by 2
	return blockSize;
}

/*
 * Gets a_bit of a block (last bit)
 * Argument block: pointer to header of the block
 * Returns: an integer a bit of 1 or 0
 */
int getABit(blockHeader* block){
	int sizeStatus = block->size_status;
	int aBit = sizeStatus & 1;
	return aBit;
}

/*
 * Gets  p_bit of a block (second last bit)
 * Argument block: pointer to header of the block
 * Returns: an integer p bit of 1 or 0
 */
int getPBit(blockHeader* block){
	int sizeStatus = block->size_status;
	int pBit = sizeStatus & 2;
	pBit = pBit >> 1; //divide result by 2 to get 1 or 0
	return pBit;
}

/*
 * Gets pointer to the next block header
 * Argument: currBlock: a pointer to header of block
 * Returns: a pointer to the next block
 */
void* getNextBlock(void* currBlock){
	return (void*)currBlock + getBlockSize(currBlock);
}

/*
 * Gets pointer to a block's header given pointer to its payload
 * Argument ptrToPayload: pointer to the payload of a block
 * Returns: a pointer to the header of the block
 */
void* getHeaderFromPayload(void* ptrToPayload){
	void* ptrToHeader = (void*)((int)ptrToPayload - sizeof(blockHeader));
	return ptrToHeader;
}

/*
 * Gets footer of current block
 * Argument currBlock: pointer to header of a block
 * Returns: pointer to the footer of the block
 */
void* getFooter(void* currBlock){
    return getNextBlock(currBlock) - sizeof(blockHeader);
}

/*
 * Sets footer of a block given its block header
 * Argument currBlock: pointer to the header of the block
 * Returns: void
 */
void setFooter(void* currBlock){
	void* footerAddress;
	footerAddress = getFooter(currBlock);
	((blockHeader*)footerAddress)->size_status = getBlockSize(currBlock);
}

/*
 * Gets footer of next block
 * Argument currBlock: pointer to the header of a block
 * Returns: pointer to the footer of the next block
 */
void* getNextBlockFooter(void* currBlock){
	return getFooter(getNextBlock(currBlock));
}

/* 
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block on success.
 * Returns NULL on failure.
 * This function should:
 * - Check size - Return NULL if not positive or if larger than heap space.
 * - Determine block size rounding up to a multiple of 8 and possibly adding padding as a result.
 * - Use NEXT-FIT PLACEMENT POLICY to chose a free block
 * - Use SPLITTING to divide the chosen free block into two if it is too large.
 * - Update header(s) and footer as needed.
 * Tips: Be careful with pointer arithmetic and scale factors.
 */
void* allocHeap(int size) {
	int padding;
	int blockSize;

	void* ptrToPayload = NULL; //pointer to the payload of the allocated block
	blockHeader* nextBlock = NULL; //pointer to next block after current block is allocated	 
	blockHeader* tempPointer = NULL; //tracker blockHeader* variable used for looping

	int tempPointerBlockSize;
	int allocSuccess = 0; //0 is failed, 1 is success

	//check that size is positive
	if (size < 1){
		return NULL;
	}
	
	//gets required padding size, header size, and final block size
	padding = (size + sizeof(blockHeader)) % 8;
	padding = (8 - padding) % 8;
	blockSize = (size + sizeof(blockHeader)) + padding;

	//if this is the first block being allocated, then set pointer to current
	//block to the pointer to start of heap heapStart
	if (nextBlockHeader == NULL){
		heapSize = getBlockSize(heapStart);
		if (blockSize > heapSize){
			return NULL;
		}

		currBlock = heapStart;
		currBlock->size_status = blockSize + 3; //sets currBlock to allocated
		nextBlock = getNextBlock(currBlock);
		nextBlockHeader = (blockHeader*)nextBlock;

		//next block size is remaining heap space
		nextBlockHeader->size_status = heapSize - getBlockSize(currBlock);
		nextBlockHeader->size_status += 2; //sets p bit of next block to allocated
	        heapSize -= blockSize;	
	}
	
	//if this is not the first block being allocated, loop through heap space
	//to find appropriate spot
	else{
		if (blockSize > heapSize){
			return NULL;
		}
		tempPointer = nextBlockHeader;
	
		while (tempPointer->size_status != 1){
			tempPointerBlockSize = getBlockSize(tempPointer);
			
			//if we find free block that is big enough
			if (tempPointerBlockSize > blockSize && getABit(tempPointer) == 0){
				currBlock = tempPointer;
				currBlock->size_status -= getBlockSize(currBlock);
			       	currBlock->size_status += blockSize +  1; //currBlock is now allocated
				nextBlock = getNextBlock(currBlock);
				nextBlockHeader = (blockHeader*)nextBlock;

				//split blocks if space found larger than requested
				if (tempPointerBlockSize > blockSize){
					nextBlockHeader->size_status = tempPointerBlockSize - blockSize;
					
				}
				heapSize -= blockSize;
				nextBlockHeader->size_status += 2; //sets next block's p bit to 1
				allocSuccess = 1;
				break;
			}
			//if temp block not big enough or is allocated, continue iteration
			else{
				tempPointer = (void*)tempPointer + getBlockSize(tempPointer);
				tempPointer = (blockHeader*)tempPointer;
			}
		}
		if (allocSuccess == 0){		
			//reach end of heap, now wrap around
			tempPointer = heapStart;
			while (tempPointer != nextBlockHeader){
				tempPointerBlockSize = getBlockSize(tempPointer);

				//if we find free block big enough
				if (tempPointerBlockSize > blockSize && getABit(tempPointer) == 0){
					currBlock = tempPointer;
					currBlock->size_status -= getBlockSize(currBlock);
					currBlock->size_status += blockSize + 1; //currBlock is now allocated
					nextBlock = getNextBlock(currBlock);
					nextBlockHeader = (blockHeader*)nextBlock;

					//split blocks if space found larger than requested
					if (tempPointerBlockSize > blockSize){
						nextBlockHeader->size_status = tempPointerBlockSize - blockSize;
					}
					heapSize -= blockSize;
					nextBlockHeader->size_status += 2;
					allocSuccess = 1;
					break;
				} else {
					tempPointer = (void*)tempPointer + getBlockSize(tempPointer);
					tempPointer = (blockHeader*)tempPointer;
				}
			}
		}

		//fail to allocate memory
		if (allocSuccess == 0){
			return NULL;
		}

		nextBlock = getNextBlock(currBlock);
		nextBlockHeader = (blockHeader*)nextBlock;
	}
	
	//if next block is free, update its footer
	if (getABit(nextBlockHeader) == 0){
		((blockHeader*)getFooter(nextBlockHeader))->size_status = getBlockSize(nextBlockHeader);
	}

	ptrToPayload = (void*)currBlock + sizeof(blockHeader); //payload is 4 + block pointer
	return ptrToPayload;
} 
 
/* 
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function should:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - USE IMMEDIATE COALESCING if one or both of the adjacent neighbors are free.
 * - Update header(s) and footer as needed.
 */                    
int freeHeap(void *ptr) {
	blockHeader* currBlockHeader = NULL;
	blockHeader* prevBlockHeader = NULL; //new block header after coalescing
					     //with previous block
	int newHeapSpace; //freed up heap space that is now available

	//checks that ptr is not NULL	
	if (ptr == NULL){
		return -1;
	}	

	//checks that ptr address is a multiple of 8
	if ((int)ptr % 8 != 0){
		return -1;
	}
	
	//checks that ptr is within heap space	
	if ((int)ptr > (int)(heapStart + allocsize)){
		return -1;
	}
	
	//sets currBlockHeader var to pointer to block header of block
	currBlockHeader = (blockHeader*)getHeaderFromPayload(ptr);

	//if current block is already freed
	if (getABit(currBlockHeader) == 0){
		return -1;
	}

	newHeapSpace = getBlockSize(currBlockHeader);

	//update current block's status to freed
	currBlockHeader->size_status -= 1;
	heapSize += newHeapSpace;

	//sets next block's p bit to freed
	((blockHeader*)(getNextBlock(currBlockHeader)))->size_status -= 2;

	//sets footer
	setFooter(currBlockHeader);

	nextBlockHeader = currBlockHeader;
		

	//coalesce with next block, if next block is free
	if (getABit((blockHeader*)getNextBlock(currBlockHeader)) == 0){

		//update size of block
		currBlockHeader->size_status += getBlockSize((blockHeader*)getNextBlock(currBlockHeader));

		//sets block footer
		setFooter(currBlockHeader);
	}

	//coalesce with previous block, if previous block is free
	int prevBlockFooterValue = ((blockHeader*)((void*)currBlockHeader - sizeof(blockHeader)))->size_status;

	if (prevBlockFooterValue != 0){
		prevBlockHeader = (blockHeader*)((void*)currBlockHeader - prevBlockFooterValue);
		prevBlockHeader->size_status += currBlockHeader->size_status;
		setFooter(prevBlockHeader);
		nextBlockHeader = prevBlockHeader;
	}
    	return 0;
} 
 
/*
 * Function used to initialize the memory allocator.
 * Intended to be called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */                    
int initHeap(int sizeOfRegion) {    
 
    static int allocated_once = 0; //prevent multiple initHeap calls
 
    int pagesize;  // page size
    int padsize;   // size of padding when heap size not a multiple of page size
    void* mmap_ptr; // pointer to memory mapped area
    int fd;

    blockHeader* endMark;
  
    if (0 != allocated_once) {
        fprintf(stderr, 
        "Error:mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion 
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    allocsize = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, allocsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }
  
    allocated_once = 1;

    // for double word alignment and end mark
    allocsize -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heapStart = (blockHeader*) mmap_ptr + 1;

    // Set the end mark
    endMark = (blockHeader*)((void*)heapStart + allocsize);
    endMark->size_status = 1;

    // Set size in header
    heapStart->size_status = allocsize;

    // Set p-bit as allocated in header
    // note a-bit left at 0 for free
    heapStart->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader*) ((void*)heapStart + allocsize - 4);
    footer->size_status = allocsize;
  
    return 0;
} 
                  
/* 
 * Function to be used for DEBUGGING to help you visualize your heap structure.
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block 
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts) 
 * t_End    : address of the last byte in the block 
 * t_Size   : size of the block as stored in the block header
 */                     
void dumpMem() {     
 
    int counter;
    char status[5];
    char p_status[5];
    char *t_begin = NULL;
    char *t_end   = NULL;
    int t_size;
    int footer;

    blockHeader *current = heapStart;
    counter = 1;

    int used_size = 0;
    int free_size = 0;
    int is_used   = -1;

    fprintf(stdout, "************************************Block list***\
                    ********************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\t\tFooter Val\n");
    fprintf(stdout, "-------------------------------------------------\
                    --------------------------------\n");
  
    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;
    
        if (t_size & 1) {
            // LSB = 1 => used block
            strcpy(status, "used");
            is_used = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "Free");
            is_used = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "used");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "Free");
        }

        if (is_used) 
            used_size += t_size;
        else 
            free_size += t_size;

        t_end = t_begin + t_size - 1;
	footer = ((blockHeader*)(getFooter(current)))->size_status;
    
        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%d\t\t%i\n", counter, status, 
        p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size, footer);
    
        current = (blockHeader*)((char*)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, "---------------------------------------------------\
                    ------------------------------\n");
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fprintf(stdout, "Total used size = %d\n", used_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", used_size + free_size);
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fflush(stdout);

    return;  
} 
