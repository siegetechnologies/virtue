#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pid_namespace.h>

//#include <linux/spinlock.h> //for rwlock stuff

//#include "../../include/netrule.h"

#include "../../include/common.h"
//#include "../../include/mod_allocator.h"
//#include "../../include/mod_log.h"

//#include "../../include/mod_listener.h"	//for the api endpoints
#include "../../include/mod_utils.h"



static inline int pid_namespace_inum( struct task_struct * ts ) {
	struct pid_namespace *pns = task_active_pid_ns( ts );
	return pns ? pns->ns.inum : 0;
}


int is_containerized(struct task_struct *ts){
	int root_ns_inum = 0;
	int my_ns_inum = pid_namespace_inum(ts);
	struct pid_namespace * parent = task_active_pid_ns(ts);
	while(parent->parent)
		parent=parent->parent;
	root_ns_inum = parent->ns.inum;
	return (root_ns_inum != my_ns_inum);
}

static int __init start(void) {
	//this is so simple, not gonna bother with the msg
	printk( KERN_INFO "mod_utils: loading\n" );
	printk( KERN_INFO "mod_utils: loaded\n" );
	return 0;
}


static void __exit end(void) {
	printk( KERN_INFO "mod_utils: unloading\n" );
//	maven_allocator_info();	//not currently used?
	printk( KERN_INFO "mod_utils: unloaded\n" );

}

EXPORT_SYMBOL(is_containerized);

MODULE_LICENSE("GPL");
module_init(start);
module_exit(end);
