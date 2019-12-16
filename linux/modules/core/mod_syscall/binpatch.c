#include <linux/module.h>
#include <asm/ptrace.h>
#include <asm/desc.h> /* for desc_ptr */
#include <linux/kallsyms.h>
#include <asm/desc.h>
#include "../../include/mod_allocator.h"


#define OPCODE_CALL 0xe8
static void enable_page_protection(void);
static void disable_page_protection(void);

struct patch_result {
	size_t inum;
	unsigned long addr_call;
	uint32_t ref_offset;

	struct patch_result* next;
	struct patch_result* prev;
};

int swap_callq(
	unsigned long addr_stub,
	unsigned long addr_curr_handler,
	unsigned long addr_new_handler,
	unsigned long* addr_call,
	uint32_t* ref_offset );
static void push_hook_result( struct patch_result* res );
static struct patch_result* find_ihook( size_t inum );
static void remove_hook_result( struct patch_result* res );


//Head of the linked list, there should probably also be a mutex here
static struct patch_result* head = NULL;

#define HOOK_RES_UNINITIALIZED { \
	.inum=-1,\
	.addr_call=0,\
	.ref_offset=0,\
	.next=NULL,\
	.prev=NULL,\
};



/*
Scan memory starting at addr_stub for the call to ref_handler
replace the call to ref_handler with a call to new_handler
cry silently

return 0 on success
returns nonzero or SEGFAULTs on failure
*/
int swap_callq(
	unsigned long addr_stub,
	unsigned long addr_curr_handler,
	unsigned long addr_new_handler,
	unsigned long* addr_call,
	uint32_t* ref_offset ) {

	/* relative call uses a 32 bit offset, need to truncate */
	uint32_t ref_stub32 = addr_stub & 0xFFFFFFFF;
	uint32_t curr_handler32 = addr_curr_handler & 0xFFFFFFFF;
	uint32_t new_handler32 = addr_new_handler & 0xFFFFFFFF;

	uint32_t i = 0; //tracks number of bytes into the function

	uint32_t target_offset,actual_offset,new_offset;

	uint32_t calls = 0;

	// Arbitrary max to limit scanning. In reality, it's usually 2
	for( calls = 0; calls < 5; calls++ ) {
		/* we know we're looking for a call command */
		while( *(uint8_t*)(addr_stub + i) != OPCODE_CALL ) {
			i++;
		}
		printk( KERN_INFO "Trapper: found callq at %lx\n", addr_stub + i );
		i++;

		/*
		figure out where the call is going to.
		if it's going to the curr_handler, we will rewrite it

		first, compute the target offset we would need to get to curr_handler

		addr_stub + i + 4 + <offset> = handler
		<offset> = handler - addr_stub - i - 4

		The +4 is because the offset is computed from the addr of the
		 next instruction
		*/
		target_offset = curr_handler32 - ref_stub32 - i - 4;
		actual_offset = *(uint32_t*)(addr_stub + i );

		if( target_offset == actual_offset ) {
			// bingo!

			//where are we jumping again? See math above, but with new handler
			new_offset = new_handler32 - ref_stub32 - i - 4;

			// save the address and offset, so we don't need to do this again
			// when we unhook
			*addr_call = addr_stub + i;
			*ref_offset = actual_offset;

			disable_page_protection();
			*(uint32_t*)(addr_stub + i) = new_offset;
			enable_page_protection();
			return 0;
		}
	}
	printk( KERN_WARNING "Trapper: unable to find call, the interrupt is not hooked\n" );
	return -1;
}


/*
   Turn off a bit 16 in cr0 to disable page protection
 */
static void disable_page_protection(void) {
	write_cr0 (read_cr0 () & (~ 0x10000));
}

/*
	Turn the protection bit back on
 */
static void enable_page_protection(void) {
	write_cr0 (read_cr0 () | 0x10000);
}


static void push_hook_result( struct patch_result* res ) {
	if( head ) {
		head->prev = res;
	}
	res->next = head;
	res->prev = NULL;
	head = res;
}

static struct patch_result* find_ihook( size_t inum ) {
	struct patch_result* res = head;
	for( ; res; res=res->next ) {
		if( res->inum == inum ) {
			return res;
		}
	}
	return NULL;
}

static void remove_hook_result( struct patch_result* res ) {
	if( res->prev ) {
		res->prev->next = res->next;
		if( res->next ) {
			res->next->prev = res->prev;
		}

	} else if( res->next ) {
		res->next->prev = res->prev;
		head = res->next;

	} else {
		head = NULL;
	}
	res->next = NULL;
	res->prev = NULL;
	maven_free( res );
}

static void unhook_interrupt_res( struct patch_result* hook ) {
	disable_page_protection();
	//patch the kernel code back
	*(uint32_t*)(hook->addr_call) = hook->ref_offset;
	enable_page_protection();

	remove_hook_result( hook );
}

void unhook_interrupt( size_t inum ) {
	struct patch_result* res = find_ihook( inum );
	if( res == NULL ) {
		printk( KERN_WARNING "Tried to unhook interrupt %lu, but it was not found\n", inum );
		return;
	}
	unhook_interrupt_res( res );
}

void unhook_all_interrupts( void ) {
	struct patch_result* i;
	for( i = head; i; i = i->next ) {
		unhook_interrupt_res( i );
	}
	BUG_ON( head != NULL );
}



MODULE_LICENSE("GPL");
