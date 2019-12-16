#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/types.h>
#include <linux/mm.h>
#include <linux/random.h>	//wait_for_random_bytes
#include <linux/slab.h>

#include "mempool.h"


/*
 * Simple, constant time, thread safe allocator for the MAVEN microvisor
 * This is the public facing api, most of the allocator logic is in mempool.c
 * exports 3 functions, maven_alloc, maven_free, and allocator_pinfo,
 * use the first two to manage memory in other maven modules
 *
 * NOTE: The memory pools are allocated using kmalloc (really get_free_pages)
 *   at startup, when we know we can trust the system.
 *   Any allocations after this may be malicious, so
 *   we won't make them. Thus, once we are out of memory, we won't acquire any
 *   more.
 */


struct allocator {
	uint64_t total_mem;
	struct mvn_mempool* base_pool;
	// Bookkeeping/debugging stuff. This may not be left in upon release
	atomic64_t num_allocs;
	atomic64_t num_frees;
	atomic64_t total_mem_allocd;
	atomic64_t total_mem_freed;
};

static struct allocator *global_alloc = NULL;


/*
 * due to the way __alloc_pages works under the hood, we want each of our
 * pools to be PAGE_SIZE*2^x bytes, for same value of x
 * since the block size is one larger than the max alloc size, we'll make our
 * max alloc sizes 2^n-1 bytes
 * The number of pools,blocks,etc is pretty much arbitrary other than this,
 * so it can be adjusted if MAVEN finds itself running out of memory
 */
#define NUM_POOLS 6
#define FROM_ORDER(x) ( 1 << x )
#define POOL_SIZE(p) FROM_ORDER(p->block_order)
#define MAX_SIZE (FROM_ORDER( pool_alloc_orders[NUM_POOLS-1]))


const static size_t pool_alloc_orders[NUM_POOLS] =	{6,  8, 10, 12, 14, 16};
const static size_t pool_num_block_orders[NUM_POOLS] =	{10, 8,  6,  5,  4,  2};


// a macro for iterating through pools
// This is a safe loop, the items can be changed/deleted/etc in the loop body
// this causes a warning in C89, since there are variable decls in the for loop
#define for_each_pool(allocator, p) \
	struct mvn_mempool *p,*__pnext;\
	for( p = allocator->base_pool, __pnext = p ? p->next : NULL;\
		p != NULL;\
		p = __pnext, __pnext = __pnext ? __pnext->next : NULL )

//returns the 
static struct mvn_mempool* maven_pool_for_alloc( struct allocator* alloc, void * addr){
	struct mvn_mempool *ret = NULL;

	for_each_pool( alloc, p ) {
		//Only works with pow2 as block size
		void * aligned_ptr = (void *) ptr_round_down(addr, mvn_pool_block_size(p) );
		if(mvn_addrinpool(p, aligned_ptr )) {
			ret = p;
			//dont early out for more consistent timing
			//break;
		}
	}
	return ret;
}

void *maven_alloc_aligned_ng( struct allocator* alloc, size_t size, size_t alignment ) {
	void* ptr = NULL;
	if( size == 0 ) {
		printk( KERN_INFO "Allocator: warning, calling maven_malloc with 0, probably a bug\n" );
		return NULL;
	}
	if( size > MAX_SIZE ) {
		printk( KERN_INFO "Allocator: warning, cannot allocate blocks larger than %lu bytes. Tried to alloc %li", (size_t)MAX_SIZE, size);
		return NULL;
	}
	{
		for_each_pool( alloc, p ) {
			size_t psize = POOL_SIZE(p);
			if( size <= psize ) {
				ptr = mvn_alloc_aligned( p, size, alignment);
				if( ptr ) {
					atomic64_inc(&(alloc->num_allocs));
					atomic64_add(psize, &(alloc->total_mem_allocd));
					return ptr;
				} else {
					printk( KERN_INFO "Allocator: warning, no pools of size %lu remain (or ones that dont match alignment), will try next size", psize );
				}
			} else {
				//that pool was too small, check the next one
				// intentional pass to next iteration of for loop
			}
		}
	}
	printk( KERN_INFO "Allocator: Warning, all suitable memory chunks full" );
	return NULL;


}
/*
 * Allocates a block of memory of at least size bytes, aligned to alignment bytes
 * Returns a pointer to he memory on success, NULL on failure
 * maven_alloc_aligned attempts to follow the same conventions as malloc
 */
void* maven_alloc_aligned( size_t size, size_t alignment) {
	return maven_alloc_aligned_ng( global_alloc, size, alignment );
}
/*
debug wrapper of above
*/
void *maven_alloc_aligned_debug(size_t size, size_t alignment, const char * caller, const char * file, const int line){
	printk( KERN_INFO "maven_alloc_aligned called with args %li %li from %s %s:%i %p", size, alignment, caller, file, line, __builtin_return_address(1));
	return maven_alloc_aligned(size, alignment);
}


/*
 * Allocates a block of memory of at least size bytes
 * Returns a pointer to he memory on success, NULL on failure
 * maven_alloc attempts to follow the same conventions as malloc
 * essentially a wrapper for maven_alloc_aligned
 */
//defined in the header file as an inline

/*
void * maven_alloc(size_t size){
	return maven_alloc_aligned(size, 1);
}
*/
void maven_free_ng( struct allocator* alloc, void* addr ) {
	struct mvn_mempool *p = NULL;
	if( addr == NULL ) {
		// something something standard malloc behavior?
		return;
	}
	p = maven_pool_for_alloc( alloc, addr);
	if(p == NULL){
		printk( KERN_WARNING "Unable to free address %p, are you sure it is a valid pointer?\n", addr );
	} else {
		size_t psize = mvn_pool_block_size(p);
		void * aligned_ptr = (void *) ptr_round_down(addr, psize);
		mvn_memfree( p, aligned_ptr );
		atomic64_inc(&(alloc->num_frees));
		atomic64_add(psize, &(alloc->total_mem_freed));
	}
}

/*
 * Frees an address that was previously allocated with a call to maven_malloc
 * if addr is NULL, no operation is performed
 * if addr was not allocated, or has already been freed, the result is undefined
 */
void maven_free( void* addr ) {
	maven_free_ng( global_alloc, addr );
}
/*
debug wrapper of above
*/
void maven_free_debug(void * addr, const char * caller, const char * file, const int line){
	printk( KERN_INFO "maven_free called with args %p from %s %s:%i %p", addr, caller, file, line, __builtin_return_address(1));
	return maven_free(addr);
}

void * maven_realloc_aligned_ng( struct allocator *alloc, void * oldaddr, size_t newsize, size_t alignment){
	size_t ofs;
	size_t oldsz;
	void * newdata = 0;
	struct mvn_mempool * pool = 0;
	void * aligned_ptr = oldaddr;
	size_t psize;

	if(oldaddr == NULL) return maven_alloc_aligned_ng( alloc, newsize, alignment);	//realloc(null, size) just is malloc(size);
	//find pool that oldaddr came from
	pool = maven_pool_for_alloc( alloc, oldaddr);
	if(pool == NULL){
		printk( KERN_WARNING "Unable to realloc address %p, are you sure it is a valid pointer?\n", oldaddr );
		return NULL;
	}


	//maven_realloc() defaults to 8
	alignment = alignment ? alignment : 8;	//slightly faster if no optimization to use alignemtnt?:8, but its a gnu extension;

	psize = mvn_pool_block_size(pool);

	//calculate aligned_ptr
	aligned_ptr = (void *) ptr_round_down(oldaddr, psize);
	ofs = oldaddr - aligned_ptr;

	//attempt to do an in_block realloc
	//"in_block realloc" just checks if it will fit
	//also check alignment. If it isnt aligned, we will allocate a new one
	if( !((uintptr_t)oldaddr % alignment) && ofs + newsize < psize ){
		return oldaddr;
	}

	//so at this point we have to actually re-allocate

	//we dont know the size of the block, but we can assume that it is the rest of the block (max that would fit given that input pointer, cant possibly be any larger, so we dont need to copy any larger)
	oldsz = psize - ofs;
	//we dont need to check if oldsz > newsize, as the "in_block realloc" would've covered it

	//simple, malloc-copy-free
	newdata = maven_alloc_aligned_ng( alloc, newsize, alignment);
	if(newdata){
		memcpy(newdata, oldaddr, oldsz);
	} else {
		printk( KERN_WARNING "Unable to Allocate a new block for realloc\n");
		return NULL;
	}

	mvn_memfree( pool, aligned_ptr );

	atomic64_inc(&(alloc->num_frees));
	atomic64_add(psize, &(alloc->total_mem_freed));

	return newdata;
}

/*
 * maven version of realloc
 * on failure, returns a null pointer
 * if given null as oldaddr, just calls maven_alloc
 * will do an in-block realloc if it can, otherwise does an alloc-copy-free style realloc
 * Note, not a constant time (uses memcpy if it needs to move pools)
 */

void * maven_realloc_aligned(void * oldaddr, size_t newsize, size_t alignment){
	return maven_realloc_aligned_ng( global_alloc, oldaddr, newsize, alignment );
}
/*
debug wrapper of above
*/
void *maven_realloc_aligned_debug(void * oldaddr, size_t size, size_t alignment, const char * caller, const char * file, const int line){
	printk( KERN_INFO "maven_realloc_aligned called with args %p %li %li from %s %s:%i %p", oldaddr, size, alignment, caller, file, line, __builtin_return_address(1));
	return maven_realloc_aligned(oldaddr, size, alignment);
}


//wrapper for the above
//defined as inline in header file
/*
void * maven_realloc(void * oldaddr, size_t newsize){
	return maven_realloc_aligned(oldaddr, newsize, 1);
}
*/



void get_allocator_data( uint64_t *allocs, uint64_t *frees, uint64_t *used, uint64_t *free ) {
	uint64_t alc = atomic64_read(&(global_alloc->total_mem_allocd));
	uint64_t fre = atomic64_read(&(global_alloc->total_mem_freed));
	*allocs = atomic64_read(&(global_alloc->num_allocs));
	*frees = atomic64_read(&(global_alloc->num_frees));
	*used = (alc - fre);
	*free = (global_alloc->total_mem - alc);
}

static void allocator_free( struct allocator* allocator ) {
	if(!allocator){
		printk( KERN_ERR "Error, deleting a null allocator?" );
		return;
	}
	printk( "Deleting an allocator" );
	printk( "%lu allocs, %lu frees\n", atomic64_read(&(allocator->num_allocs)), atomic64_read(&(allocator->num_frees))  );
	{
		for_each_pool( allocator, p ) {
			mvn_memdestroy( p );
		}
	}
	kfree( allocator );
}

struct allocator* allocator_init( void ) {
	struct allocator* a = kzalloc( sizeof( *a ), GFP_KERNEL );
	struct mvn_mempool* last_pool;
	size_t i;
	a->base_pool = mvn_meminit( pool_num_block_orders[0], pool_alloc_orders[0] );
	last_pool = a->base_pool;
	for( i = 1; i < NUM_POOLS; i++ ) {
		last_pool->next = mvn_meminit( pool_num_block_orders[i], pool_alloc_orders[i] );
		if( last_pool->next == NULL ) {
			printk( KERN_WARNING "Failed to allocate pool %ld\n", i );
			allocator_free( a );
			a = NULL;
			return NULL;
		}
		a->total_mem += 1 << (pool_num_block_orders[i] + pool_alloc_orders[i]);
		last_pool = last_pool->next;
	}
	return a;
}

static __init int mod_init( void ) {
#ifndef NO_SIMEMINFO
	struct sysinfo tmp;
	si_meminfo( &tmp );
#endif
	printk(KERN_INFO "mod_allocator: loading\n");
	//make sure prng is seeded
	if(wait_for_random_bytes()){
		printk(KERN_INFO "Allocator: wait_for_random_bytes failed\n");
		return -EAGAIN;
	}
	global_alloc = allocator_init();
	if(!global_alloc){
		printk(KERN_INFO "Allocator: unable to create global allocator\n");
		return -ENOMEM;
	}


#ifndef NO_SIMEMINFO
	printk( "Total system is %lu bytes (~%lu pages)\n", tmp.totalram, (tmp.totalram)/PAGE_SIZE);
#endif
	printk( "Using %llu bytes (~%llu pages)\n", global_alloc->total_mem, global_alloc->total_mem/PAGE_SIZE);
	/*
	char* tmp;
	for( i = 0; i < pool_num_blocks[0]; i++ ) {
		tmp = maven_alloc( 1 );
		if(
	}
	printk( maven_alloc( 1 ) );
	*/
	printk(KERN_INFO "mod_allocator: loaded\n");
	return 0;
}


//the following is fairly new
void maven_allocator_info(void){
	if(!global_alloc){
		printk(KERN_ERR "Error, info on null global_alloc\n");
	}
	printk( "%lu allocs, %lu frees\n", atomic64_read(&(global_alloc->num_allocs)), atomic64_read(&(global_alloc->num_frees))  );
}
EXPORT_SYMBOL( maven_allocator_info );

//end of fairly new

static __exit void mod_exit( void ) {
	printk(KERN_INFO "mod_allocator: unloading\n");
	allocator_free( global_alloc );
	global_alloc = 0;
	printk(KERN_INFO "mod_allocator: unloaded\n");
}



EXPORT_SYMBOL( allocator_init );
EXPORT_SYMBOL( allocator_free );

EXPORT_SYMBOL( maven_alloc_aligned);
EXPORT_SYMBOL( maven_alloc_aligned_ng);
EXPORT_SYMBOL( maven_alloc_aligned_debug);
//EXPORT_SYMBOL( maven_alloc );
EXPORT_SYMBOL( maven_free );
EXPORT_SYMBOL( maven_free_ng );
EXPORT_SYMBOL( maven_free_debug );
EXPORT_SYMBOL( maven_realloc_aligned);
EXPORT_SYMBOL( maven_realloc_aligned_ng);
EXPORT_SYMBOL( maven_realloc_aligned_debug);
//EXPORT_SYMBOL( maven_realloc );
EXPORT_SYMBOL( get_allocator_data );
MODULE_LICENSE( "GPL" );
module_init( mod_init );
module_exit( mod_exit );
