#include <linux/module.h>
#include <asm/ptrace.h>
#include <asm/desc.h> /* for desc_ptr */
#include <linux/kallsyms.h>
#include <asm/desc.h>
#include "../../include/mod_allocator.h"
#include "../../include/mod_log.h"


#define OPCODE_CALL 0xe8




// Public api
typedef asmlinkage void (*int_handler_fn)(struct pt_regs* regs, unsigned long code );
int hook_interrupt( size_t inum, char* existing_ksym, int_handler_fn hook );
void unhook_interrupt( size_t inum );
void unhook_all_interrupts( void );
int_handler_fn get_ref_fn( size_t inum );

// idt info
static struct desc_ptr idtr;

struct idt_descr {
	uint16_t offset_1;
	uint16_t selector;
	uint8_t ist;
	uint8_t type_attr;
	uint16_t offset_2;
	uint32_t offset_3;
	uint32_t zero;
} __attribute__((packed));

/*
Provides a way for hook_interrupt to communicate how it did
the addr_call and ref_offset should be used to restore the interrupt
when unhook_interrupt is called
*/
struct int_hook_result {
	size_t inum;
	unsigned long addr_call;
	uint32_t ref_offset;
	int_handler_fn ref_fn;

	struct int_hook_result* next;
	struct int_hook_result* prev;
};

//Static functions
static void disable_page_protection(void);
static void enable_page_protection(void);
static int swap_callq(
	unsigned long addr_stub,
	unsigned long addr_curr_handler,
	unsigned long addr_new_handler,
	unsigned long* addr_call,
	uint32_t* ref_offset );
static void unhook_interrupt_res( struct int_hook_result* hook );
static unsigned long reconstruct_offset( struct idt_descr* entry );
static void push_hook_result( struct int_hook_result* res );
static struct int_hook_result* find_ihook( size_t inum );
static void remove_hook_result( struct int_hook_result* res );


//Head of the linked list, there should probably also be a mutex here
static struct int_hook_result* head = NULL;

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
static int swap_callq( 
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
		printk( KERN_INFO "mod_interrupt: found callq at %lx\n", addr_stub + i );
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
	printk( KERN_WARNING "mod_interrupt: unable to find call, the interrupt is not hooked\n" );
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


/*
The address in the idt is stored weirdly, this rebuilds
it to a single 64-bit address
*/
static unsigned long reconstruct_offset( struct idt_descr* entry ) {
	unsigned long addr = entry->offset_3;
	addr <<= 32;
	addr |= (entry->offset_2)<<16;
	addr |= entry->offset_1;
	return addr;
}

static void push_hook_result( struct int_hook_result* res ) {
	if( head ) {
		head->prev = res;
	}
	res->next = head;
	res->prev = NULL;
	head = res;
}

static struct int_hook_result* find_ihook( size_t inum ) {
	struct int_hook_result* res = head;
	for( ; res; res=res->next ) {
		if( res->inum == inum ) {
			return res;
		}
	}
	return NULL;
}

static void remove_hook_result( struct int_hook_result* res ) {
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

static void unhook_interrupt_res( struct int_hook_result* hook ) {
	disable_page_protection();
	//patch the kernel code back
	*(uint32_t*)(hook->addr_call) = hook->ref_offset;
	enable_page_protection();

	remove_hook_result( hook );
}

void unhook_interrupt( size_t inum ) {
	struct int_hook_result* res = find_ihook( inum );
	if( res == NULL ) {
		printk( KERN_WARNING "Tried to unhook interrupt %lu, but it was not found\n", inum );
		return;
	}
	unhook_interrupt_res( res );
}

void unhook_all_interrupts( void ) {
	struct int_hook_result* i;
	for( i = head; i; i = i->next ) {
		unhook_interrupt_res( i );
	}
	BUG_ON( head != NULL );
}

/*
hook an interrupt by number

Interrupts in linux are stored in the IDT and usually look like this

interrupt:
	...
	call <some setup function>
	...
	call do_interrupt
	iretq

So the routine in the IDT is just a stub that does some setup and
then farms the actual work to a real handler

The way we do interrupt hooking is to scan the actual machine code
for the call to do_interrupt, and replace it with our own function
like so

interrupt:
	...
	call <some setup function>
	...
	call our_do_interrupt
	iretq

The new handler is responsible for making sure the actual handling
gets done. Mostly this will just mean ending the handler with a call
to do_interrupt, but you're free reinvent as many wheels as you want

The results from the hooking are stored in whatever res points to
(see struct definition)

See swap_callq for documentation about how the actual bytecode swap
is done
*/
int hook_interrupt( size_t inum, char* existing_ksym, int_handler_fn hook ) {
	int swap_err;
	unsigned long addr_stub = 0;
	unsigned long addr_new_handler = (unsigned long)hook;
	unsigned long addr_ref_handler = 0;
	struct int_hook_result* res = NULL;
	struct idt_descr* idt_entry = (struct idt_descr*)(idtr.address + (inum * sizeof(*idt_entry) ) );

	addr_stub = reconstruct_offset( idt_entry );
	addr_ref_handler = kallsyms_lookup_name( existing_ksym );
	if( addr_ref_handler == 0 ) {
		printk( KERN_INFO "mod_interrupt: kernel symbol %s not found in kallsyms\n", existing_ksym );
		return -ENOENT;
	}
	printk( KERN_INFO "%s: \n\t \
		stub: %lx\n\t \
		addr_ref_handler: %lx\n\t", existing_ksym, addr_stub, addr_ref_handler );
	res = maven_alloc( sizeof( *res ) );
	if( res == NULL ) {
		return -ENOMEM;
	}
	res->inum = inum;
	res->ref_fn = (void*)addr_ref_handler;
	swap_err = swap_callq( addr_stub, addr_ref_handler, addr_new_handler, &(res->addr_call), &(res->ref_offset));
	if( swap_err ) {
		printk( KERN_INFO "mod_interrupt: failed to hook exception %ld\n", inum );
		maven_free( res );
		return -EFAULT;
	}
	printk( KERN_INFO "Interrupt %lu hooked\n", inum );
	push_hook_result( res );
	return 0;
}

int_handler_fn get_ref_fn( size_t inum ) {
	struct int_hook_result* res = find_ihook( inum );
	if( res == NULL ) {
		return NULL;
	}
	return res->ref_fn;
}

static __init int mod_init(void) {
	printk( KERN_INFO "mod_interrupt: loading\n" );

	store_idt( &idtr );
	if( idtr.address == 0 ) {
		printk( KERN_INFO "mod_interrupt: Finding IDT failed. Abort" );
		return -1;
	}
	printk( KERN_INFO "mod_interrupt: idt appears to be located at %lx\n", idtr.address );
	printk( KERN_INFO "mod_interrupt: loaded\n" );
	msg_moduleload("mod_interrupt");
	return 0;
}


static __exit void mod_exit(void) {
	msg_moduleunload("mod_interrupt");
	// Reverse the modifications
	printk( KERN_INFO "mod_interrupt: unloading\n" );
	unhook_all_interrupts( );
	printk( KERN_INFO "mod_interrupt: unloaded\n" );

}

EXPORT_SYMBOL( hook_interrupt );
EXPORT_SYMBOL( unhook_interrupt );
EXPORT_SYMBOL( unhook_all_interrupts );
EXPORT_SYMBOL( get_ref_fn );
MODULE_LICENSE("GPL");
module_init(mod_init);
module_exit(mod_exit);
