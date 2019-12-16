#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include <asm/asm-offsets.h>
#include <linux/fs_struct.h>
#include <linux/fs.h>
#include <linux/path.h>
#include <linux/err.h> // For PTR_ERR and friends

#include "new_syscalls.h"
#include "file_blacklist.h"
#include "syscall_common.h"
#include "../../include/common.h"
#include "../../include/mod_listener.h"
#include "../../include/mod_log.h"
#include "../../include/mod_config.h"




#define NO_OPEN
#define NO_EXEC
//#define NO_RMMOD


/*
A mod to hook into syscalls


To hook a syscall:
hook_syscall( syscall_num, new_call ):
	Fairly self explanatory. The linux kernel provides
	#define terms __NR_<name> (e.g. __NR_read ) for all syscalls
	in the linux/syscalls.h header.

	The new call should be a function pointer with the same prototype
	as the replaced call, or things could get weird. Usually, the last
	thing new call does should be to call the reference version
	The reference versions of all hooked syscalls are available in sct_ref.

To unhook a syscall:
	Unhook one specific syscall by using unhook_syscall( num )
	Unhook all syscalls by calling unhook_all( )
*/

static void disable_page_protection(void);
static void enable_page_protection(void);


void **sys_call_table = NULL;
void* sct_ref[NR_syscalls] = {NULL};
pid_t udevd_pid = 0;


/*
 * Yell at jesse do document this
 */
int hook_syscall( size_t syscall_num, void* new_call ) {
	if( sys_call_table == NULL ) {
		printk( KERN_WARNING "sys_call_table has not been setup, cannot hook\n" );
		return 1;
	}
	if( sct_ref[syscall_num] ) {
		printk( KERN_WARNING "syscall %ld has already been hooked\n", syscall_num );
		return 1;
	}

	sct_ref[syscall_num] = (void*)sys_call_table[syscall_num];

	disable_page_protection();
	sys_call_table[syscall_num] = new_call;
	enable_page_protection();
	return 0;
}

/*
 * Yell at jesse do document this
 */
int unhook_syscall( size_t syscall_num ) {
	if( sys_call_table == NULL ) {
		printk( KERN_WARNING "sys_call_table not found, cannot unhook\n");
		return -1;
	}
	if( sct_ref[syscall_num] == NULL ) {
		printk( KERN_WARNING "syscall %ld is not hooked\n", syscall_num );
		return -1;
	}
	disable_page_protection();
	sys_call_table[syscall_num] = sct_ref[syscall_num];
	enable_page_protection();
	sct_ref[syscall_num] = NULL;
	return 0;
}

/*
 * Yell at jesse do document this
 */
void unhook_all( void ) {
	size_t i;
	for( i = 0; i < NR_syscalls; i++ ) {
		if( sct_ref[i] ) {
			printk( KERN_INFO "Unhooking syscall %lu\n", i );
			unhook_syscall( i );
		}
	}
}

static void** find_sys_call_table(void) {
	unsigned long addr_sct = kallsyms_lookup_name( "sys_call_table" );
	printk(KERN_INFO "mod_syscall: ksym syscall table at address: 0x%02lX", addr_sct);

	return (void**)addr_sct;
}


static void disable_page_protection(void) {
	write_cr0 (read_cr0 () & (~ 0x10000));
}

static void enable_page_protection(void) {
	write_cr0 (read_cr0 () | 0x10000);
}


int hook_all( void ) {
	int err;
	#ifndef NO_EXEC
	if( (err = hook_syscall( __NR_execveat, new_sys_execveat )) ) {
		return err;
	}
	if( (err = hook_syscall( __NR_execve, new_sys_execve )) ) {
		return err;
	}
	#endif
	#ifndef NO_OPEN
	if( (err = hook_syscall( __NR_openat, new_sys_openat )) ) {
		return err;
	}
	if( (err = hook_syscall( __NR_open, new_sys_open )) ) {
		return err;
	}
	#endif
#ifdef NO_RMMOD
	if( (err = hook_syscall( __NR_delete_module, new_sys_delete_module ) ) ) {
		return err;
	}
#endif
	return 0;
}

endpoint_t drep = {
	"add_trusted_daemon",
	add_trusted_daemon,
	0
};


void enable_flag(char * r, struct api_response * out_api, uint64_t flag){
	//who cares about args
	config.syscallflags |= flag;
	out_api->code = 0;
	out_api->msg = "Successfully enabled";
}

void enable_filelog(char *r, struct api_response * out_api){
	enable_flag(r, out_api, FL_PRINTALLFILES);
}
void enable_execlog(char * r, struct api_response * out_api){
	enable_flag(r, out_api, FL_PRINTALLEXEC);
}
endpoint_t flrep = {
	"enable_filelog",
	enable_filelog,
	0
};
endpoint_t elrep = {
	"enable_execlog",
	enable_execlog,
	0
};


static int __init interceptor_start(void) {
	struct api_response resp;
	printk(KERN_INFO "mod_syscall: loading\n" );
	/* Find the system call table */
	if(!(sys_call_table = find_sys_call_table())) {
		printk( KERN_WARNING "syscall: Unable to find syscall table\n" );
		/* not found, abort */
		return -1;
	}
	if( kallsyms_lookup_name( "stub_execve" ) ) {
		printk( KERN_WARNING "syscall: the kernel is using stub semantics in the sct, if you try to hook execve, you will crash\n" );
	}
	add_trusted_daemon( "systemd-udevd", &resp );
	if( resp.code ) {
		printk(KERN_WARNING "mod_syscall Unable to find systemd-udevd to make trusted");
		//todo should probably also send a message about this?
//		return -ENOENT;
	}
	blacklist_init();
//	hook_all();
	new_syscall_init();	//now handles hook_all junk
	//add a test file to the blacklist
//	add_to_blacklist("/tmp/blacklisttest");
//	add_to_blacklist("/tmp/bltest");
//	add_to_blacklist("/tmp/blacklisttest");

	endpoint_register(&drep);
	endpoint_register(&flrep);
	endpoint_register(&elrep);
	printk(KERN_INFO "mod_syscall: loaded\n" );
	msg_moduleload("mod_syscall");
	return 0;
}


static void __exit interceptor_end(void) {
	msg_moduleunload("mod_syscall");
	printk(KERN_INFO "mod_syscall: unloading\n");
	endpoint_unregister(&elrep);
	endpoint_unregister(&flrep);
	endpoint_unregister(&drep);
	/* If we don’t know what the syscall table is, don’t bother. */
	if(!sys_call_table){
		return;
	}
	/* Revert all system calls to what they were before we began. */
	unhook_all( );
	blacklist_shutdown();
	cleanup_new_syscalls();
//	maven_allocator_info();	//not used?
	printk(KERN_INFO "mod_syscall: unloaded\n");
}

EXPORT_SYMBOL( hook_syscall );
EXPORT_SYMBOL( unhook_syscall );
EXPORT_SYMBOL( unhook_all );
EXPORT_SYMBOL( hook_all );
//EXPORT_SYMBOL( add_trusted_daemon );
MODULE_LICENSE("GPL");
module_init(interceptor_start);
module_exit(interceptor_end);
