# heap-alloc


## Background
This is a dynamic heap allocator implemented in C. As you might recall, memory allocation can happen on the stack or the heap. Stack allocation is somewhat simpler, as it does not require explicit managing by the programmer - a stack frame of a function is "discarded" as the function returns, and with it, all local variables and parameters. The freed space can then be used by other processes. The disadvantage of that is a program must know at compile-time what resources will be used, and how much. For example - how large should an array be? Often times, this information isn't known until runtime - hence the benefits of dynamic memory allocation at runtime.

## Requirements
A heap memory allocator must be able to handle arbitrary requests, requests of all sizes, alignment requirements, and not modify any previously-allocated blocks. It also must be efficient in allocating memory, both in terms of speed (how quickly a allocation request is satisfied), and the usage of resources (how to avoid external fragmentation?).

## Implementation
A next-fit placement policy is selected for this project - the allocator will search through memory, looking for a free block of memory, and upon completion of a request, the next request will pick up from the previous. This has multiple advantages over a first-fit policy. Looping through the entire list from start each time is likely to produce uneven allocation - blocks at the front of the list will be used up, and ones at the end will be left unused. Further, large blocks of free memory tend to be broken up into smaller allocated chunks, causing external fragmentation. A next-fit policy reduces the likelihood of such issues.

Our alignment policy is 8-byte (double word) boundaries. Large blocks will be split to satisfy smaller requests, in order to avoid internal fragmentation. Adjacent free blocks, when they do appear, are coalesced into a single, larger block. 

## Final Note
In practice, functions like `malloc` are used to allocate/free memory explicitly by using `mmap` and `munmap` functions, or by using the `sbrk` function to grow/shrink the heap.
