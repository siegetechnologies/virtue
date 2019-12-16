struct allocator;

extern struct allocator* allocator_init( void );
extern void allocator_free( struct allocator* alloc );

extern void maven_allocator_info( void );

extern void* maven_alloc_aligned( size_t size, size_t alignemnt );
extern void* maven_alloc_aligned_ng( struct allocator* alloc, size_t size, size_t alignemnt );
extern void* maven_alloc_aligned_debug( size_t size, size_t alignemnt, const char * caller, const char * file, const int line);

extern void maven_free( void* addr );
extern void maven_free_ng( struct allocator* alloc, void* addr );
extern void maven_free_debug( void *addr, const char * caller, const char * file, const int line);

extern void* maven_realloc_aligned( void * oldaddr, size_t size , size_t alignment);
extern void* maven_realloc_aligned_ng( struct allocator* alloc, void * oldaddr, size_t size , size_t alignment);
extern void* maven_realloc_aligned_debug( void * oldaddr, size_t size, size_t alignemnt, const char * caller, const char * file, const int line);

extern void get_allocator_data( uint64_t *num_allocs, uint64_t *num_frees, uint64_t *total_used, uint64_t *total_free );

//#define MAVEN_ALLOCATOR_DEBUG
#ifdef MAVEN_ALLOCATOR_DEBUG
#define maven_alloc_aligned( s, al ) maven_alloc_aligned_debug((s), (al), __func__, __FILE__, __LINE__)
#define maven_realloc_aligned( ol, s, al ) maven_realloc_aligned_debug((ol), (s), (al), __func__, __FILE__, __LINE__)
#define maven_free( ol ) maven_free_debug((ol), __func__, __FILE__, __LINE__)
#endif

/*
//inline wrapers for ease of use and to be similar to malloc and realloc
static inline void* maven_realloc( void * oldaddr, size_t size ){
	return maven_realloc_aligned(oldaddr, size, 1);
}
static inline void* maven_alloc( size_t size ){
	return maven_alloc_aligned(size, 1);
}
*/

//inline wrappers wornt working with debug (__func__ displayed the definition of the inline....)
//0 for alignment forces the default alignment. (this is currently set to 8 in the functions)
#define maven_realloc( old, sz ) maven_realloc_aligned( (old), (sz), 0 )
#define maven_alloc( sz ) maven_alloc_aligned( (sz), 0 )
#define maven_alloc_ng( alloc, sz) maven_alloc_aligned_ng( alloc, (sz), 0 )
#define maven_realloc_ng( a, old, sz ) maven_realloc_aligned_ng( a, (old), (sz), 0 )

#define ALLOC_FAIL_CHECK(ptr) if( !ptr) { \
	maven_panic( "out of memory", __FILE__, __LINE__ );\
	BUG();\
} while( 0 );
