#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include <asm/asm-offsets.h>

#include <linux/path.h>

#include <linux/err.h>
#include <linux/namei.h>

#include "../../include/common.h"
#include "../../include/mod_allocator.h"



#include "../../include/mod_log.h"
#include "../../include/mod_listener.h"	//registering endpoints






extern void ** sys_call_table;

typedef struct blacklistentry_s {
	dev_t dev;
	unsigned long ino;
	struct path path;

	struct blacklistentry_s *next;
	char blname[];
} blacklistentry_t;

static struct mutex blacklist_mutex;

//todo make this fast(er) with a hashtable?
blacklistentry_t * blacklist = 0;

int add_to_blacklist(char __user * abspath){

	size_t len;
	blacklistentry_t *e;
	struct kstat stat = {0};
	struct path path;
	struct partial_msg *msg = 0;
	blacklistentry_t ** lastbl = &blacklist;

	int err = kern_path(abspath, LOOKUP_FOLLOW | LOOKUP_AUTOMOUNT, &path);
	if(err){
		printk(KERN_WARNING "%s:%i kern_path err is %i when attempting to add %s to blacklist\n", __FILE__, __LINE__, err, abspath);
		return 1;
	}


	//also compare ino and dev and etc
	err = vfs_getattr(&path, &stat, STATX_BASIC_STATS, LOOKUP_FOLLOW | LOOKUP_AUTOMOUNT);
	if(err){
		printk(KERN_WARNING "%s:%i vfs_getattr err is %i when attempting to add %s to blacklist\n", __FILE__, __LINE__, err, abspath);
		return 1;
	}

	len = strlen(abspath)+1;
	printk("Adding %s to blacklist\n", abspath);
	msg = msg_init( "maven.syscall", SEVERITY_WARNING, 2 );
	msg_additem( msg, "adding to file blacklist", abspath, TYPE_STRING );
	msg_finalize( msg );
	e = maven_alloc(len+sizeof(blacklistentry_t));
	//TODO null alloc result!
	e->next = 0;
	e->path = path;
	e->ino = stat.ino;
	e->dev = stat.dev;
	memcpy(&e->blname, abspath, len);


	//checks to see if already in blacklist (have to check in critical section as well)

//commented out because it is just more complexity, and the add_function isnt called often
//	//find end of blacklist, also check if already exits
//	for(;*lastbl; lastbl = &(*lastbl)->next){
//		if(path_equal(&(*lastbl)->path, &path) || (stat.dev == (*lastbl)->dev && stat.ino == (*lastbl)->ino)) goto exists;
//	}


	mutex_lock(&blacklist_mutex);
	//just double check end (in case someone added another when i was waiting for the lock)
	for(;*lastbl; lastbl = &(*lastbl)->next){
		if(path_equal(&(*lastbl)->path, &path) || (stat.dev == (*lastbl)->dev && stat.ino == (*lastbl)->ino)){
			mutex_unlock(&blacklist_mutex);
			goto exists;
		}
	}
	//add it in
	*lastbl = e;
	mutex_unlock(&blacklist_mutex);

	return 0;
exists:
	printk(KERN_WARNING "%s already exists in the blacklist\n", abspath);
	if(e) maven_free(e);
	return -EEXIST;

}

void add_filerule(char *r, struct api_response * out_api){
	int res;
	char * abspath;
	abspath = r;
	if(!abspath){
		goto usage;
	}
	res = add_to_blacklist(abspath);
	out_api->code = 0;
	out_api->msg = "Successfully added blacklist";
	if(res){
		if(res == -EEXIST){
			out_api->msg = "Successfully added blacklist... already exists";
		} else {
			out_api->msg = "Error adding blacklist";
			out_api->code = 1;
		}
	}
	return;
	usage:
		out_api->code = 1;
		out_api->msg = "Usage: add_filerule <PATH>";
		return;
}
endpoint_t frep = {
	"add_filerule",
	add_filerule,
	0
};



int blacklist_init(void){
	mutex_init(&blacklist_mutex);
	endpoint_register(&frep);
	return 0;
}


//returns string to name of blacklist entry if found
//returns 0 otherwise
char * check_blacklist(struct path p, struct kstat s){
	blacklistentry_t *bl;
	for(bl = blacklist; bl; bl = bl->next)
		if(path_equal(&bl->path, &p) ||
		(s.dev == bl->dev && s.ino == bl->ino)
		) return bl->blname;
	return 0;
}
int blacklist_shutdown(void){
	blacklistentry_t *bl, *next = 0;
	endpoint_unregister(&frep);
	//todo verify no race conditions here
	for(bl = blacklist; bl; bl = next){
		next = bl->next;
		maven_free(bl);
	}
	return 0;
}



EXPORT_SYMBOL(add_to_blacklist);
MODULE_LICENSE("GPL");
