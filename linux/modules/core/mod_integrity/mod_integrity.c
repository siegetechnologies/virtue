#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hashtable.h>
#include <crypto/sha256_base.h>
#include <crypto/sha.h>
#include <linux/string.h>
#include <linux/hash.h>

#include "../../include/mod_allocator.h"
#include "../../include/mod_log.h"

//for detecting if stringhash is in this kernel
#include <linux/version.h>
#if defined OLDHASH || LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0)
	#ifdef OLDHASH
		#pragma message "Forced ignoring <linux/stringhash.h> and using full_name_hash2 because OLDHASH is defined"
	#endif
	#define OLDHASH
	#define FULL_NAME_HASH full_name_hash2
#else
	#pragma message "Using <linux/stringhash.h> for full_name_hash... this still needs a bit more testing, but is considered usable."
	#pragma message "You can manually force to go to old fake full_name_hash2 by defining OLDHASH"
	#pragma message "Once verified, remove this warning please."
	#include <linux/stringhash.h>
	#define FULL_NAME_HASH full_name_hash
#endif


/*
A module for verifiying system integrity


3 functions are exported from this module
maven_store_sig( name, addr, mem_len, flags ):
	creates an entry in the integrity table using known good memory at addr
	name is a unique human readable name. Something like "syscall table"
	addr can be a direct memory address, i.e. 0xffffffffabcdef00
		- in this case the memory starting at that address and continuing for
			mem_len bytes is considered the good memory
		- an example use case is to create a signature using the address of the sys_call_table
	addr can also be the address of a function that satisfies the mem_fn typedef (below)
		- in this case, the F_INDIRECT flag must be given
		- the function must fill the preallocated buf field with exactly len bytes.
			len will always be equal to the mem_len field
		- the filled buf array is used to generate and check signatures
		- an example use case is given by sidt_example at the bottom of the file
			which loads the contents of the idtr register into the buffer
	mem_len is the length of the memory section to check. If addr is a function pointer,
		mem_len should be the length of the buffer that the function will fill
	flags is a bitmap of options. Currently, the only supported option is F_INDIRECT
		which indicates that addr is a function pointer

maven_check_sig( name )
	verifies that the signature stored using <name> has not changed
	returns 0 if the sig is valid
	returns nonzero if the sig is invalid, or the name is not known
		This may or may not indicate system compromise

maven_check_system( )
	Checks all known signatures.
	Returns 0 if all signatures are valid
	otherwise, returns non-zero error code

*/

#define TABLE_BITS 8 //2^8=256 slots in our hastable
#define F_INDIRECT 0x1

struct integrity_data {
	char* name;
	unsigned long addr;
	uint8_t ref_digest[SHA256_DIGEST_SIZE];
	size_t mem_len;
	uint32_t flags;

	struct hlist_node node;
};

typedef void (*mem_fn)(uint8_t* buf, size_t len );


//helpers and stuff
static int gen_digest( uint8_t* mem, size_t mem_len, uint8_t* digest );
static int check_digest( struct integrity_data* data );
static void dealloc_table( void );
static int update_buf( size_t sz );

//these should be exported
int maven_check_sig( char* name );
int maven_check_system( void );
int maven_store_sig( char* name, unsigned long addr, size_t mem_len, uint32_t flags );

static DECLARE_HASHTABLE( integrity, TABLE_BITS );
struct crypto_shash* tfm;

// Global buffer to reduce calls to alloc and free

static struct mutex bufmut;
uint8_t* buf = NULL;
size_t bufsz = 0;


/*
Update buf to make sure it is at least sz bytes

If buf is NULL, allocates a new buffer of size sz
If buff is alloc'ed, but too small, it is realloc'ed to be large enough
If buf is already large enough, has no effect
*/
static int update_buf( size_t sz ) {
	uint8_t* tmp;
	int ret = 0;
	mutex_lock( &bufmut );
	if( buf == NULL || bufsz < sz ) {
		tmp = maven_alloc( sz );
		if( tmp != NULL ) {
			memcpy( tmp, buf, bufsz );
			maven_free( buf );
			buf = tmp;
			bufsz = sz;
			ret = 0;
		} else {
			printk( KERN_WARNING "Integrity: update_buf failed\n" );
			//Make sure that if this fails, everything fails unless we take care of it
			maven_free( buf );
			buf = NULL;
			ret = -ENOMEM;
		}
	}
	mutex_unlock( &bufmut );
	return ret;
}

/*
Checks to see if the memory signature hinted by data
is still valid
*/
static int check_digest( struct integrity_data* data ) {
	uint8_t computed_digest[SHA256_DIGEST_SIZE];
	int err;
	size_t i;
	uint8_t* digest_target;

	if( data->flags & F_INDIRECT ) {
		err = update_buf( data->mem_len );
		if( err ) {
			return err;
		}

		((mem_fn)data->addr)( buf, data->mem_len );
		digest_target = buf;
	} else {
		digest_target = (uint8_t*)data->addr;
	}
	err = gen_digest( digest_target, data->mem_len, computed_digest );
	if( err ) {
		printk( KERN_INFO "Integrity: Failed to compute digest: err %d\n", err );
		return err;
	}
	for( i = 0; i < SHA256_DIGEST_SIZE; i++ ) {
		if( computed_digest[i] != data->ref_digest[i] ) {
			printk( KERN_WARNING "Integrity: Invalid hash for %s. Integrity may have been compromised\n", data->name );
			return -EFAULT;
		}
	}
	printk( KERN_INFO "Integrity: Hash for %s valid\n", data->name );
	return 0;
}

/*
generate a sha256 digest of the data given by mem
saves the memory in the prealloc'd digest field
returns 0 on success, crypto_shash error code on failure
*/
static int gen_digest( uint8_t* mem, size_t mem_len, uint8_t* digest ) {
	int error;
	SHASH_DESC_ON_STACK(desc, tfm );

	desc->tfm = tfm;
	desc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;
	error = crypto_shash_init( desc );
	if( error ) {
		printk( KERN_INFO "Integrity: Error in shash_init\n" );
		return error;
	}
	error = crypto_shash_update( desc, mem, mem_len );
	if( error ) {
		printk( KERN_INFO "Integrity: Error in shash_update\n" );
		return error;
	}

	error = crypto_shash_final( desc, digest );
	if( error ) {
		printk( KERN_INFO "Integrity: Error on shash_final\n" );
		return error;
	}
	return 0;
}

#ifdef OLDHASH
static uint64_t quick_name_hash( char* name, size_t len ) {
	uint64_t ret = 0;
	uint64_t acc = 0;
	size_t counter = 0;
	size_t rem = 0;
	unsigned long sz = sizeof( uint64_t );
	while( sz*(counter+1) <= len ) {
		ret ^= hash_64( ((uint64_t*)name)[counter], sz*8);
		counter++;
	}
	rem = len % sz;
	counter *= sz;
	while( rem ) {
		acc |= ((uint64_t)name[counter])<<((--rem)*8);
		counter++;
	}
	BUG_ON( counter != len );
	ret ^= hash_64( acc, sz*8 );
	printk( "Final hash of %s is %llx\n", name, ret );
	return ret;

}




/*
 * This is not intended to be cryptographically secure at all.
 * when <linux/stringhash.h> get's ported to this kernel, remove this
 */
//currently preprocessor'd out on newer kernels
static uint64_t full_name_hash2( void* salt, char* name, size_t len ) {
	uint64_t namehash = quick_name_hash( name, len );
	uint64_t salthash = 0;
	if( salt ) {
		salthash = hash_ptr( salt, sizeof( salt ) );
	}
	return namehash ^ salthash;
}
#endif	//OLDHASH

/*
Lookup a name in the integrity hashtable
and validate the digest

Returns 0 if the sig is valid
returns non-zero if the name is not present or
	the digest has changed (indicating tampering)
	Specifically, returns -err, where err is some error code in errno.h

*/
int maven_check_sig( char* name ) {
	uint64_t name_hash;
	struct integrity_data* data;
	int err;

	//NOTE, full_name_hash only returns a 32 bit result. the fake full_name_hash2 will return a 64 bit one
	//official full_name_hash uses hash_fold to fold the salt together with the hash. fake full_name_hash2 uses an xor.
	//unless i've missed something, it should still be OK to use the official full_name_hash though
	name_hash = FULL_NAME_HASH( NULL, name, strlen( name ) );

	printk( KERN_INFO "Integrity: Searching for %s with hash %llx\n", name, name_hash );
	hash_for_each_possible( integrity, data, node, name_hash ) {
		if( data == NULL ) {
			continue; //or break?
		}
		if( strcmp( data->name, name ) == 0 ) {
			err = check_digest( data );
			if( err ) {
				// log_event?
				return err;
			} else {
				return 0;
			}
		}
	}
	printk( KERN_WARNING "Integrity: symbol %s not found in integrity table\n", name );
	return -ENOENT;

}

/*
Validate all known memory signatures
*/
int maven_check_system( void ) {
	int i;
	int err;
	struct integrity_data* data;
	hash_for_each( integrity, i, data, node ) {
		printk( KERN_INFO "Integrity: Checking %s\n", data->name );
		err = check_digest( data );
		if( err ) {
			// log event?
			return err;
		}
	}
	return 0;
}

/*
generate a signature for the given address
See top level documentation for an in depth description of the fields
Returns 0 on success, nonzero on failure
*/
int maven_store_sig( char* name, unsigned long addr, size_t mem_len, uint32_t flags ) {
	struct integrity_data* data;
	size_t name_len;
	int err;
	unsigned int name_hash;
	uint8_t* digest_target;


	name_len = strlen( name );
	name_hash = FULL_NAME_HASH( NULL, name, name_len );

	data = maven_alloc( sizeof(*data) );
	if( data == NULL ) {
		goto err_mem1;
	}

	data->name = maven_alloc( name_len + 1 );
	if( data->name == NULL ) {
		goto err_mem2;
	}
	memcpy( data->name, name, name_len + 1 );
	data->addr = addr;
	data->flags = flags;
	data->mem_len = mem_len;

	// Sometimes we get the address of a function instead of a memory address
	// used for stuff like checking that the result of sidt hasn't changed
	// See typedef of mem_fn
	if( flags & F_INDIRECT ) {
		err = update_buf( data->mem_len );
		if( err ) {
			goto err_mem3;
		}
		((mem_fn)addr)( buf, data->mem_len );
		digest_target = buf;
	} else {
		digest_target = (uint8_t*)data->addr;
	}
	err = gen_digest( digest_target, data->mem_len, data->ref_digest );
	if( err ) {
		printk( KERN_INFO "Integrity: gen_digest for %s failed\n", data->name );
		return err;
	}

	printk( KERN_INFO "Integrity: Added %s with hash %x\n", data->name, name_hash );
	hash_add( integrity, &(data->node), name_hash );

	return 0;

	//If any call to update buff or maven_alloc fails, we'll wind up here
	err_mem3:
	maven_free( data->name );
	err_mem2:
	maven_free( data );
	err_mem1:
	return -ENOMEM;
}

/*
Cleanup stuff
*/
static void dealloc_table( void ) {
	int i;
	struct integrity_data* data;
	struct hlist_node* tmp;

	hash_for_each_safe( integrity, i, tmp, data, node ) {
		hash_del( &(data->node) );
		maven_free( data->name );
		maven_free( data );
	}
}


static int __init interceptor_start(void) {
	printk(KERN_INFO "mod_integrity: loading\n");
	hash_init( integrity );
	tfm = crypto_alloc_shash( "sha256", 0 , CRYPTO_ALG_ASYNC );
	if( IS_ERR( tfm ) ) {
		int error = PTR_ERR(tfm);
		printk( KERN_INFO "Integrity: Failed to setup profile for hashing with error %d\n", error );
		return error;
	}
	maven_allocator_info();
	printk(KERN_INFO "mod_integrity: loaded\n");
	msg_moduleload("mod_integrity");
	return 0;
}


static void __exit interceptor_end(void) {
	msg_moduleunload("mod_integrity");
	printk(KERN_INFO "mod_integrity: unloading\n");
	dealloc_table();
	crypto_free_shash( tfm );
	if( buf != NULL ) {
		maven_free( buf );
	}
	maven_allocator_info();
	printk(KERN_INFO "mod_integrity: unloaded\n");
}

EXPORT_SYMBOL( maven_check_sig );
EXPORT_SYMBOL( maven_check_system );
EXPORT_SYMBOL( maven_store_sig );
MODULE_LICENSE("GPL");
module_init(interceptor_start);
module_exit(interceptor_end);
