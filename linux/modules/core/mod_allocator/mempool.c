#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <asm/types.h>
#include <linux/random.h>
#include <linux/spinlock.h>

#include "mempool.h"

static size_t mempool_size( struct mvn_mempool* pool ) {
	return 1ULL << pool->mempool_order;
}



static uint8_t* block_idx_to_addr( struct mvn_mempool *pool, size_t idx) {
	return pool->mempool + (idx << pool->block_order);
}


//assumes addr is aligned to block size?
//probably doesnt actually need to be aligned to block size
static size_t addr_to_block_idx( struct mvn_mempool* pool, uint8_t* addr ) {
	return (addr - pool->mempool) >> pool->block_order;
}

// range is inclusive. randomR( 0, 2 ) will return 0, 1, or 2 with approximately equal probability
// may not be really random when min-max is a significant fraction of MAX_SIZE_T
// min <= ret <= max


//Unused?
#if 0
static size_t randomR( size_t min, size_t max ) {
	size_t random;
	BUG_ON( min > max );
	get_random_bytes( &random, sizeof( random ) );
	random = random % (max-min+1);
	random += min;
	BUG_ON( random < min );
	BUG_ON( random > max );
	return random;
}
#endif

//similar to above, but min is 0
//0<= ret <= max (inclusive)
static inline size_t randomMax(size_t max){
	size_t random;
	get_random_bytes( &random, sizeof( random ) );
	random = random % (max+1);
//	BUG_ON( random > max );	//not gonna happen anway
	return random;
}

//similar to above, but 0 <= ret < max (exclusive)
static inline size_t randomMaxE(size_t max){
	size_t random;
	get_random_bytes( &random, sizeof( random ) );
	random = random % (max);
//	BUG_ON( random > max );	//not gonna happen anway
	return random;
}

//TODO?
/*
static int pg_order_from_size( size_t size ) {
	unsigned int num_pages;
	if( size == 0 ) {
		printk( KERN_INFO "Allocator: Warning, called order_from_sz with 0 " );
		return 0;
	}

	// ceil( size / PAGE_SIZE )
	num_pages = (size + (PAGE_SIZE-1)) / PAGE_SIZE;

	// ceil( log2( num_pages ) )
	return fls( num_pages-1 );
}
*/
/*
Initialize a pool of memory for the maven memory manager.

@nblocks_order maximum number of memory blocks the user will be able to allocate, represented as log2(num_blocks)
@block_order maximum size of allocation in thie pool, represented as log2(max_alloc_size)

#deprecated @num_blocks The maximum number of memory blocks the user will be able to allocate
#deprecated @max_alloc_size The largest size chunk of memory the user will be able to allocate from this pool (in fact, the only size)

*/
struct mvn_mempool* mvn_meminit( size_t nblocks_order, size_t block_order ) {
	//TODO replace with alloc_pages or something similar
	size_t i;
	size_t * free_stack;
	void * mempool;

	size_t nblocks = 1ULL << nblocks_order;
	size_t mempool_order = nblocks_order + block_order;

	struct mvn_mempool* pool = kzalloc( sizeof( *pool ), GFP_KERNEL );
	if( !pool ) {
		goto clean_0;
	}


	//not required anymore, since we dont store metadata in unused blocks
//	BUG_ON( 1ULL << block_order < sizeof(size_t));	//minimum size of blocks 8 bytes

	mempool = (void*)__get_free_pages(GFP_KERNEL, mempool_order - PAGE_SHIFT);

	if( !mempool ) {
		printk( KERN_INFO  "Could not alloc mempool\n" );
		goto clean_1;
	}

//	memset(mempool, 0 , 1ULL << mempool_order);	//we zero on each alloc anyway
	free_stack = kzalloc(nblocks * sizeof(size_t), GFP_KERNEL);

	if( !free_stack ) {
		printk( KERN_INFO  "Could not alloc free_stack\n" );
		goto clean_2;
	}

	for( i = 0; i < nblocks; i++ ) {
		free_stack[i] = i;
	}

	spin_lock_init( &(pool->mut) );
	pool->mempool_order = mempool_order;
	pool->block_order = block_order;
	pool->mempool = mempool;
	pool->free_stack = free_stack;
	pool->num_free = nblocks;
	pool->initialized = 1;
	return pool;


	//cleanups in case of error
	kfree( free_stack );
	clean_2:
	free_pages( (unsigned long)(mempool), mempool_order - PAGE_SHIFT);
	clean_1:
	kfree( pool );
	clean_0:
	return NULL;
}


/*
Delete a pool of memory
@pool the result of a mvn_meminit call
*/
void mvn_memdestroy( struct mvn_mempool* pool ) {
	//FIXME: TODO: should this be atomic?
	//only happens on module unload?
	pool->initialized = 0;
	memset( pool->mempool, 0, mempool_size( pool ) );
	free_pages( (unsigned long)(pool->mempool), pool->mempool_order - PAGE_SHIFT );
	kfree( pool->free_stack );
	kfree( pool );
}

/*
Allocate a chuck of memory from an initialized pool
All allocated chunks are the same size

@pool: a pool of memory obtained from a mvn_meminit call
@sz: the desired size of the memory. Even though all chunks are the same size,
	this parameter is still present for sanity checking that sz <= max_alloc_sz
@alignment: align the address to bytes. 1 is dont align

returns a pointer to no less than sz bytes of memory, or NULL
It is very possible for this function to fail, please be sure to check return value
*/
void* mvn_alloc_aligned( struct mvn_mempool* pool, size_t sz, size_t alignment) {

	unsigned long spinflags;
	uint8_t *block;
	size_t rndindx;
	size_t psize = mvn_pool_block_size(pool);
	BUG_ON( !pool->initialized );
	if( sz > psize ) {
		printk( KERN_INFO  "Cannot allocate a block that large. Max size is %lu\n", mvn_pool_block_size(pool) );
		return NULL;
	}
	alignment = alignment ? alignment: 8;

	spin_lock_irqsave( &(pool->mut), spinflags );
	if( pool->num_free == 0 ) {
		printk( KERN_INFO  "Cannot allocate, no free blocks in this pool\n" );
		spin_unlock_irqrestore( &(pool->mut), spinflags );
		return NULL;
	}

	//choose a random block to grab from the free_stack
	rndindx = randomMaxE(pool->num_free);

	block = block_idx_to_addr( pool, pool->free_stack[rndindx]);
	if((uintptr_t)block % alignment){
		//block is not aligned, cant use.
		//hopefully next pool has an aligned block
		spin_unlock_irqrestore( &(pool->mut), spinflags );
		return NULL;
	}

	//ok, we have decided on this block

	//lets remove it from the free stack
	pool->num_free--;
	pool->free_stack[rndindx] = pool->free_stack[pool->num_free];

	//todo, if i wanted to keep track of "used blocks", i could do an actual swap instead of half swap
	//then the free stack would look like this
	//0  num_free   max_size(16)
	//|      |        |
	//fffffffuuuuuuuuu
	//where f is a free block indx and u is a used block indx
	//havent gotten a use case for it yet, but it is a potential thing to do


	//todo we might be able to convert to an atomic?
	spin_unlock_irqrestore( &(pool->mut), spinflags );

	// they should mostly be zeros already
	memset( block, 0, psize );

	// offset randomly into the block, to make injection attacks harder
	//but then align to the alignment spec required
	return (void *)ptr_round_down(block + randomMax(psize - sz), alignment);
}

/*
Free a chunk of memory allocated with mvn_alloc
@pool: a mvn_mempool obtained from a mvn_meminit
@ptr: a pointer obtained from a call to mvn_alloc

If ptr is not in the mempool, the a BUG() call will be generated
if pool is not a valid mempool, the results are undefined, but probably bad
ptr is expected to be aligned to the top of the block
*/
void mvn_memfree( struct mvn_mempool* pool, void* ptr ) {
	size_t indx;
#ifdef MAVEN_DOUBLEFREE_CHECK
	size_t i;
#endif
	unsigned long spinflags;
	BUG_ON( !pool->initialized );
	BUG_ON( !mvn_addrinpool( pool, ptr ) );

	BUG_ON( pool->num_free >= mvn_pool_num_blocks(pool));	//will only trigger during cases of double free
								//can remove if we add more in depth double free checks?


	//not going to change by other users, can be outside of critical section
	indx = addr_to_block_idx(pool, ptr);


	spin_lock_irqsave( &(pool->mut), spinflags );


	//double_free_check
#ifdef MAVEN_DOUBLEFREE_CHECK
	//potential improvement, loop over the entire free_stack list (even after num_free) to have consistent timing
	//would require code in mvn_alloc_aligned to make sure to set everything above num_free to -1, so no false positives (or code in loop to check i < num_free
	for(i = 0; i < pool->num_free; i++){
		if(indx == pool->free_stack[i]){	//indx has already been freed
			printk( KERN_ERR  "DOUBLE FREE\n" );
			spin_unlock_irqrestore( &(pool->mut), spinflags );
			BUG_ON(1);	//can comment out if we want to fail nicely
		}
	}
#endif
	//one possible "security" thing could be veryify a double free by checking to make sure the block isnt in the free list
	// that would have unpredictable timing, however

	//push it onto free stack
	pool->free_stack[pool->num_free] = indx;
	pool->num_free++;
	//ptr is aligned to block

	//needs to be in critical section in case another user allocates it
	memset( ptr, 0, mvn_pool_block_size(pool) ); // zero the pool

	spin_unlock_irqrestore( &(pool->mut), spinflags );
}

/**
  * Returns true iff the pointer is inside the memory pool
  * and also aligned, i.e., it is the start of a block
  */
int mvn_addrinpool( struct mvn_mempool* pool, void* ptr ) {
	int below_start = ptr >= (void*)pool->mempool;
	int above_end = ptr < (void*)pool->mempool+ mempool_size( pool );
	int aligned = (unsigned long)ptr % mvn_pool_block_size(pool) == 0;
	return below_start && above_end && aligned;
}
