#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include <asm/asm-offsets.h>
#include <linux/fs_struct.h>
#include <linux/fs.h>
#include <linux/pid_namespace.h>
#include <linux/path.h>
#include <linux/pid.h>
#include <linux/err.h> // For PTR_ERR and friends
#include <linux/sched/signal.h>
#include <linux/sched/task.h>

#include "new_syscalls.h"

#include "file_blacklist.h"
#include "syscall_common.h"

#include "../../include/common.h"
#include "../../include/mod_allocator.h"
#include "../../include/mod_log.h"
#include "../../include/mod_config.h"
#include "../../include/mod_utils.h"

#include <linux/namei.h>

#include <linux/binfmts.h>

int (*ref_do_execveat)(int fd, struct filename *filename,const char __user *const __user *__argv,const char __user *const __user *__envp,int flags) = 0;
struct filename *(*ref_getname_flags)(const char __user *filename, int flags, int *empty) = 0;
long (*ref_do_sys_open)(int dfd, const char __user *filename, int flags, umode_t mode);


//prototypes for static helper functions, definitions at bottom
static int has_approved_ancestor( struct task_struct * ts );
static int find_process_pid( char* process, pid_t *pid );

pid_t *pidlist = NULL;
size_t pidlist_len = 0;
size_t pidlist_cap = 0;

void add_trusted_daemon( char* dname, struct api_response *out ) {
	int err;
	if( !dname ) {
		out->code = 1;
		out->msg = "Invalid or missing arguments";
		out->should_free_msg = 0;
		return;
	}
	if ( pidlist_len+1 > pidlist_cap ) {
		// powers of 2 are nice, but we start at 0, so if we don't have the +1 we fail
		// size will go 0->1->3->7->15->...
		pid_t* newpidlist = maven_alloc( (1+(2*pidlist_cap)) * sizeof( pid_t ) );
		memcpy( newpidlist, pidlist, pidlist_len );
		pidlist_cap *= 2;
		pidlist_cap++;
		if( pidlist ) maven_free( pidlist );
		pidlist = newpidlist;
	}
	err = find_process_pid( dname, &(pidlist[pidlist_len]) );
	if( err ) {
		out->code = ENOENT;
		out->msg = "Process not found";
		out->should_free_msg = 0;
		return;
	}
	pidlist_len++;
	out->code = 0;
	out->msg = "OK";
	out->should_free_msg = 0;
	return;
}


extern void ** sys_call_table;
extern void* sct_ref[NR_syscalls];

/*
 * new_sys_delete
 * prevents removal of maven_master
 * final version should prevent removal of anything
 */
asmlinkage long new_sys_delete_module( const char __user *name_user,
                                       unsigned int flags)
{
//	uuid_t uuid;
	//TODO null check
	long (*ref_sys_delete_module)(const char __user*, unsigned int) = sct_ref[__NR_delete_module];
//	struct partial_msg *msg = msg_init_config_wrapper( "maven.syscall", SEVERITY_ERROR, 2 );
	struct partial_msg *msg = msg_init( "maven.syscall", SEVERITY_ERROR, 2 );
//	declare_uuid_for_task( uuid, current );

	(void)ref_sys_delete_module;	//what?
	msg_additem( msg, "call to sys_delete_module", name_user, TYPE_STRING );
	//TODO add an int here describing weather the caller was containerized
	//msg_additem( msg, "container uuid", &uuid, TYPE_UUID );
	msg_finalize( msg );
	printk( KERN_INFO "sys_delete_module %s\n", name_user );
	return -1;
}



//takes a null terminated list of strings and concatenates them into space-seperated single string
// (aka "argv" to "space seperated string"
// will return an empty string ("") if there is nothing in argv
//will return null on a failure (to allocate)
// returns 1 char*, which must be eventually maven_free'd
char * argv_to_string(const char __user * const __user * argv){
		char * argv_string = 0;
		size_t argv_len = 0;
		char * argv_ptr;
		size_t i;
		for(i = 0; argv[i]; i++){
			argv_len += strlen(argv[i]) + 1;
		}
		argv_string = maven_alloc(argv_len + 1);
		//alloc_check_fail?
		if(!argv_string) return 0;
		argv_ptr = argv_string;
		for(i = 0; argv[i]; i++){
			size_t lenny = strlen(argv[i]);
			memcpy(argv_ptr, argv[i], lenny);
			argv_ptr[lenny] = ' ';
			argv_ptr += lenny + 1;
		}
		argv_ptr[0] = 0;	//add the finial null
		if(argv_ptr > argv_string && argv_ptr[-1] == ' ') argv_ptr[-1] = 0;	//remove trailing whitespace
	return argv_string;
}

//new_sys_execveat
asmlinkage long new_sys_execveat( int fd, const char __user *filename,
                                const char __user *const __user *argv,
                                const char __user *const __user *envp,
				int flags)
{

	int lookup_flags;


//	uuid_t uuid;
//	struct task_struct *ts = current;
//	unsigned int res = get_uuid_for_ns_inum( inum_for_task(ts), &uuid );
//	if( res ) {
//	if(!grab_my_uuid(&uuid)){
//		char * argvstr;
//		struct partial_msg *msg = msg_init_config_wrapper( "maven.syscall", SEVERITY_WARNING, 2 );
//		msg_additem( msg, "Error", "unknown container", TYPE_STRING );
//		msg_additem( msg, "command", filename, TYPE_STRING );
//		argvstr = argv_to_string(argv);
//		if(argvstr)
//			msg_additem( msg, "argv", argvstr, TYPE_STRING );
//		msg_finalize( msg );
//		if(argvstr)
//			maven_free(argvstr);
//		if( maven_global_risk > 1 ) {
//			return -1;
//		}
//	}
	if( FLAG_PRINTALLEXEC(config.syscallflags) ){
		char * argvstr;
		argvstr = argv_to_string(argv);
		struct partial_msg *msg = msg_init("maven.syscall", SEVERITY_WARNING, 2 );
		msg_additem(msg, "exec_access", filename, TYPE_STRING);
		if(argvstr)
			msg_additem(msg, "exec_argv", argvstr, TYPE_STRING);
		msg_finalize(msg);
		if(argvstr)
			maven_free(argvstr);
	}

	if( !is_containerized(current) ) {
		goto ref_syscall;
	}

	/*
	if( strncmp( filename, "/home/", 6 ) == 0 ) {
		printk( KERN_INFO "executing from home directories is forbidden\n" );
		if( maven_global_risk > 0 ) {
			return -1;
		}
	}
	if( filename[0] != '/' ) {
		printk( KERN_INFO "disallowing relative path command %s\n", filename );
		if( maven_global_risk > 0 ) {
			return -1;
		}
	}
	*/
	if( current->cred->uid.val == 0 ) {
		if( has_approved_ancestor( current ) ) {
			goto ref_syscall;
		}
		{
			char * argvstr;
//			struct partial_msg *msg = msg_init_config_wrapper( "maven.syscall", SEVERITY_WARNING, 1 );
			struct partial_msg *msg = msg_init( "maven.syscall", SEVERITY_WARNING, 1 );
			msg_additem( msg, "unapproved root process", filename, TYPE_STRING );
			argvstr = argv_to_string(argv);
			if(argvstr)
				msg_additem( msg, "argv", argvstr, TYPE_STRING );
			msg_finalize( msg );
			if(argvstr)
				maven_free(argvstr);

		}
		if( maven_global_risk > 0 ) {
			printk( "Would error here, panic" );
			goto ref_syscall;
			return -EPERM;
		}
	}

	ref_syscall:

	lookup_flags = (flags & AT_EMPTY_PATH) ? LOOKUP_EMPTY : 0;

	return ref_do_execveat(fd,
			   ref_getname_flags(filename, lookup_flags, NULL),
			   argv, envp, flags);
//	return ref_sys_execveat( fd, filename, argv, envp, flags );
}

//in fs/exec.c, this is basically what happens anyway
//wrapper for execveat
asmlinkage long new_sys_execve( const char __user *filename,
                                const char __user *const __user *argv,
                                const char __user *const __user *envp)
{
	return new_sys_execveat(AT_FDCWD, filename, argv, envp, 0);
}

asmlinkage long new_sys_openat( int dirfd, const char __user *filename,
                              int flags,
                              umode_t mode )
{
	/*
		Convert the ref pointer to the type of the syscall
		TODO null check
	*/
	struct partial_msg * msg;
	struct path p;
	struct kstat s;
	char fn_buf[512]; // max is supposedly 256. I don't trust that
	char* abspath;
	char* pname;
	int res;
	char * blname;
	if(dirfd == AT_FDCWD){
		abspath = d_path( &(current->fs->pwd), fn_buf, 512 );
	} else {
		//TODO?
		abspath = fn_buf;
		abspath[0] = 0;
	}


	pname = current->comm;
	res = user_path_at(dirfd, filename, LOOKUP_FOLLOW | LOOKUP_AUTOMOUNT, &p);
	if(!res)
		res = vfs_getattr(&p, &s, STATX_BASIC_STATS, LOOKUP_FOLLOW | LOOKUP_AUTOMOUNT);

	//print into mod_log if set to do so
	if( FLAG_PRINTALLFILES(config.syscallflags) ){
		msg = msg_init("maven.syscall", SEVERITY_WARNING, 2 );
		msg_additem(msg, "file_access", abspath, TYPE_STRING);
		msg_additem(msg, "file_entered", filename, TYPE_STRING);
		msg_additem(msg, "file_flags", &flags, TYPE_INT);
		msg_additem(msg, "file_process", pname, TYPE_STRING);
		msg_finalize(msg);
	}

	if(!res && (blname = check_blacklist(p, s))){
		printk(KERN_WARNING "Access blacklisted file!\n");
		//todo add a flag to say weather the thing is containerized or not?
		printk( "openat() by %s is accessing %s from %s (blacklisted file %s)\n", pname, filename, abspath, blname);
//		msg = msg_init_config_wrapper("maven.syscall", SEVERITY_WARNING, 2 );
		msg = msg_init("maven.syscall", SEVERITY_WARNING, 2 );
		msg_additem(msg, "blacklist_access", blname, TYPE_STRING);
		msg_finalize(msg);
		return -EACCES;
	}


	if (force_o_largefile())
		flags |= O_LARGEFILE;
	return ref_do_sys_open(dirfd, filename, flags, mode);

}
asmlinkage long new_sys_open( const char __user *filename,
                              int flags,
                              umode_t mode )
{
	return new_sys_openat(AT_FDCWD, filename, flags, mode);
}

/**
 * static helper functions
 **/



static int has_approved_ancestor( struct task_struct * ts ) {
	struct task_struct *p;
	int i;
	for( p = ts;; p = p->real_parent ) {
		for( i = 0; i < pidlist_len; i++ ) {
			if( p->tgid == pidlist[i] ) {
				return 1;
			}
		}
		if( p->pid == 1 ) {
			break;
		}
	}
	return 0;
}


static int find_process_pid( char* process, pid_t *pid ) {
	struct task_struct *p = NULL;
	for_each_process( p ) {
		if( streq( p->comm, process ) ) {
			*pid = p->tgid;
			return 0;
		}
	}
	return 1;
}


void cleanup_new_syscalls() {
	if( pidlist ) {
		maven_free( pidlist );
	}
}

extern int hook_syscall(size_t, void*);

int new_syscall_init(void){
	int err;

	if( !(ref_getname_flags = (void*)kallsyms_lookup_name("getname_flags")) ) {
		printk(KERN_WARNING "Couldn't lookup do_execveat, not hooking execve(at)\n");
	} else if( !(ref_do_execveat = (void*)kallsyms_lookup_name("do_execveat")) ) {
		printk(KERN_WARNING "Couldn't lookup do_execveat, not hooking execve(at)\n");
	} else if( (err = hook_syscall( __NR_execveat, new_sys_execveat )) ) {
		printk(KERN_WARNING "Couldn't hook execveat, err %i\n", err);
	} else if( (err = hook_syscall( __NR_execve, new_sys_execve )) ) {
		printk(KERN_WARNING "Couldn't hook execve (but got execveat?), err %i\n", err);
	}

	if( !(ref_do_sys_open = (void*)kallsyms_lookup_name("do_sys_open")) ) {
		printk(KERN_WARNING "Couldn't lookup do_sys_open, not hooking open(at)\n");
	} else if( (err = hook_syscall( __NR_openat, new_sys_openat )) ) {
		printk(KERN_WARNING "Couldn't hook openat, err %i\n", err);
	} else if( (err = hook_syscall( __NR_open, new_sys_open )) ) {
		printk(KERN_WARNING "Couldn't hook open (but got openat?), err %i\n", err);
	}


	return 0;
}
