#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hashtable.h>
#include <crypto/sha256_base.h>
#include <crypto/sha.h>
#include <linux/string.h>
#include <linux/hash.h>
#include <linux/kallsyms.h>
#include <linux/syscalls.h>
#include <asm/syscall.h>
#include <asm/desc.h>

#include "hashutils.h"
#include "../../include/mod_allocator.h"


struct crypto_shash* tfm = NULL;
void **sct = NULL;

int init_crypto(void){
	tfm = crypto_alloc_shash( "sha256", 0 , 0 );
	if(!tfm) return 1;
	return 0;
}
int cleanup_crypto(void){
	crypto_free_shash( tfm );
	return 0;
}


//todo verify that this is thread safe?
int hash_sct( sha_buf *addrbuf, sha_buf *textbuf ) {
//	struct crypto_shash* tfm = crypto_alloc_shash( "sha256", 0 , 0 );
	int error = 0;
	int i;
	SHASH_DESC_ON_STACK(addrs, tfm );
	SHASH_DESC_ON_STACK(code, tfm );
	if( !sct ) {
		sct = (void*)kallsyms_lookup_name( "sys_call_table" );
	}
	textbuf->sz = 0;
	addrbuf->sz = 0;

	addrs->tfm = tfm;
	addrs->flags = CRYPTO_TFM_REQ_MAY_SLEEP;
	code->tfm = tfm;
	code->flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	//first we hash the addresses in the table
	if( (error = crypto_shash_init(addrs) ) ) {
		printk( KERN_INFO "Integrity: Error in shash_init\n" );
		goto cleanup;
	}
	crypto_shash_update( addrs, (uint8_t*)sct, (__NR_syscall_max*sizeof( void* )) );
	error = crypto_shash_final( addrs, addrbuf->buf );
	if( error ) {
		printk( KERN_INFO "Integrity: Error on shash_final\n" );
		goto cleanup;
	}

	//now we hash the bytecode at each address
	if( (error = crypto_shash_init(code) ) ) {
		printk( KERN_INFO "Integrity: Error in shash_init\n" );
		goto cleanup;
	}

	for( i = 0; i < __NR_syscall_max; i++ ) {
		BUG_ON( sct[i] == NULL );
		crypto_shash_update( code, (uint8_t*)sct[i], 64 );
	}

	error = crypto_shash_final( code, textbuf->buf );
	if( error ) {
		printk( KERN_INFO "Integrity: Error on shash_final\n" );
		goto cleanup;
	}
	textbuf->sz = 32;
	addrbuf->sz = 32;

	cleanup:
//	crypto_free_shash( tfm );
	return error;
}

//todo verify that this is thread safe?
int hash_idt( sha_buf *idt_vec_buf, sha_buf *idt_code_buf ) {
//	struct crypto_shash* tfm = crypto_alloc_shash( "sha256", 0 , 0 );
	int error = 0;
	size_t i;
	struct desc_ptr idtptr;
	SHASH_DESC_ON_STACK(entries, tfm );
	SHASH_DESC_ON_STACK(code, tfm );
	size_t idt_max = 0;
	size_t idt_sz_actual;

	entries->tfm = tfm;
	entries->flags = CRYPTO_TFM_REQ_MAY_SLEEP;
	code->tfm = tfm;
	code->flags = CRYPTO_TFM_REQ_MAY_SLEEP;
	store_idt( &idtptr );

	idt_max = idtptr.size / sizeof( gate_desc );
	idt_max = idt_max > 256 ? 256 : idt_max; 
	/*
	 * From OSDev: There are 256 interrupts, so IDT should have 256 entries ... more entries are ignored
	 * For some reason, loading kvm convinces the kernel that there are more
	 * this may be considered a hack
	 */


	if( (error = crypto_shash_init(entries) ) ) {
		printk( KERN_INFO "Integrity: Error in shash_init\n" );
		goto cleanup;
	}
	if( (error = crypto_shash_init(code) ) ) {
		printk( KERN_INFO "Integrity: Error in shash_init\n" );
		goto cleanup;
	}
	idt_sz_actual = idt_max * sizeof( gate_desc);
	goto cleanup;
	
	crypto_shash_update( entries, (uint8_t*)idtptr.address, idt_sz_actual );
	for( i = 0; i < idt_max; i++ ) {
		gate_desc* tmp = &(((gate_desc*)idtptr.address)[i]);
		volatile unsigned long gate_addr = (tmp->offset_high);
		gate_addr <<= 32;
		gate_addr |= (tmp->offset_middle)<<16;
		gate_addr |= tmp->offset_low;

		// hash the machine code (or the first 64 bytes )
		crypto_shash_update( code, (uint8_t*)gate_addr, 64 );
	}

	error = crypto_shash_final( entries, idt_vec_buf->buf );
	if( error ) {
		printk( KERN_INFO "Integrity: Error on shash_final\n" );
		goto cleanup;
	}
	idt_vec_buf->sz = 32;
	error = crypto_shash_final( code, idt_code_buf->buf );
	if( error ) {
		printk( KERN_INFO "Integrity: Error on shash_final\n" );
		goto cleanup;
	}
	idt_code_buf->sz = 32;



	cleanup:
//	crypto_free_shash( tfm );
	return error;
}
