#ifndef MEMPOOL_H
#define MEMPOOL_H


//#define NO_SIMEMINFO
#define MAVEN_DOUBLEFREE_CHECK

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <asm/types.h>

//  PTR_ALIGN in kernel.h
// the ol align with your evil twin type
#define ptr_round_down(p, a) ((uintptr_t)(p) & ~((uintptr_t)(a) - 1))
//new (new) alloc structure, looks like
/*
        pool
     ___/  \___
    /          \
    v          v
 mempool   free_list

where mempool is a contiguous chunk of memory split up into
	n blocks of j size, and
free_list is a n sized array of indices, treated as a stack
	of indexes for free blocks of the mempool

*/

struct mvn_mempool {
	size_t mempool_order;	// log2 the size of the region (entire size... log2(num_blocks * block_size))
	size_t block_order;	// log2 the block size
	size_t num_free;

	// indices of free blocks in the pool. Should be constant ptr after being set up
	size_t * free_stack;

	// Address of the main memory pool. Should be constant after being set up
	uint8_t* mempool;


	// Set to zero before initialization. Set to one once init is complete
	int initialized;

	spinlock_t mut;

	// pointer to next pool. Can be NULL
	struct mvn_mempool* next;
};

static inline size_t mvn_pool_block_size (struct mvn_mempool * pool){
	return (1ULL << pool->block_order);
}

static inline size_t mvn_pool_num_blocks (struct mvn_mempool * pool){
	return (1 << (pool->mempool_order - pool->block_order));
}


struct mvn_mempool* mvn_meminit( size_t nblocks_order, size_t block_order );

void* mvn_alloc_aligned( struct mvn_mempool* data, size_t sz, size_t alignment);

void mvn_memfree( struct mvn_mempool* data, void* ptr );

void mvn_memdestroy( struct mvn_mempool* data );

int mvn_addrinpool( struct mvn_mempool* pool, void* ptr );

#endif // MEMPOOL_H
