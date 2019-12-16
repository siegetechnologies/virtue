#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h> //for rwlock stuff

//#include "../../include/netrule.h"

#include "../../include/common.h"
#include "../../include/mod_allocator.h"
#include "../../include/mod_log.h"

#include "../../include/mod_listener.h"	//for the api endpoints
#include "../../include/mod_config.h"


rwlock_t cleanup_pop;
typedef struct cleanup_s {
	struct cleanup_s * next;
	cleanupfunc_t cleanf;//	void (*cleanf)(config_t *);
} cleanup_t;
//lanked last
cleanup_t *cleanups = 0;

config_t config = {0};


//config_t defined in ../../include/mod_config.h

int config_addCleanup(cleanupfunc_t cleanf){
	cleanup_t *c;
	unsigned long spinflags;
	if(!cleanf){	//no
		//ya callin add with nothin ta add?
		//ya dummy
		return 1;
	}
	c = maven_alloc(sizeof(cleanup_t));
	ALLOC_FAIL_CHECK(c);
	c->cleanf = cleanf;

	//todo verify that this cleanup doesnt already exist?

	//i dont think i need any crazy stuff here
	write_lock_irqsave(&cleanup_pop, spinflags);
		c->next = cleanups;
		wmb();
		cleanups = c;
	write_unlock_irqrestore(&cleanup_pop, spinflags);

	return 0;
}
int _runcleanupallconfigs(cleanupfunc_t cleanf){
	if(!cleanf) return 1;
	cleanf(&config);
	return 0;
}
int config_removeCleanup(cleanupfunc_t cleanf){
	unsigned long spinflags;
	cleanup_t *c, **cq = &cleanups;
	if(!cleanf) return 1;
	write_lock_irqsave(&cleanup_pop, spinflags);
		//this would work nicer with an upgradable read lock
		for(c = cleanups; c; cq = &c->next, c=c->next){
			if(cleanf == c->cleanf){
				_runcleanupallconfigs(cleanf);
				*cq = c->next;
				maven_free(c);
				break;
			}
		}
	write_unlock_irqrestore(&cleanup_pop, spinflags);
	return 0;
}




static void _clean(config_t *conf){
	//TODO call a cleanup
	unsigned long spinflags;
	cleanup_t *c;
	//dont want the cleanups to be swooped out from under us
	read_lock_irqsave(&cleanup_pop, spinflags);
		for(c = cleanups; c; c= c->next){
			if(c->cleanf){
				c->cleanf(conf);
			}
		}
	read_unlock_irqrestore(&cleanup_pop, spinflags);
	if(conf->virtueid) maven_free(conf->virtueid);
	conf->id = 0;

}

void config_shutdown(void){
	_clean(&config);
}





static int __init start(void) {
	printk( KERN_INFO "mod_config: loading\n" );
//	unsigned long spinflags;

//	rwlock_init(&config_pop);
	rwlock_init(&cleanup_pop);


//	endpoint_register(&remep);
//	endpoint_register(&addep);
	//probably have a "HOST" config?
	//lets add default config?

//	read_lock_irqsave(&config_pop, spinflags);
//		p = _addpuddin("host");//?
//	read_unlock_irqrestore(&config_pop, spinflags);

	maven_allocator_info();
	printk( KERN_INFO "mod_config: loaded\n" );
	msg_moduleload("mod_config");
	return 0;
}


static void __exit end(void) {
	msg_moduleunload("mod_config");
	printk( KERN_INFO "mod_config: unloading\n" );
//	endpoint_unregister(&addep);
//	endpoint_unregister(&remep);
	config_shutdown();
	maven_allocator_info();
	printk( KERN_INFO "mod_config: unloaded\n" );
}

//struct partial_msg * msg_init_config_wrapper(const char * tag, uint64_t severity, size_t kv_pairs){
//	//uuid_t u = {0};
//	char * vname = config.virtueid;
//	struct partial_msg * ret =  msg_init(tag, severity, vname ? kv_pairs+1 : kv_pairs);
//	if(vname){
//		msg_additem(ret, "container_virtueid", vname, TYPE_STRING);
//	}
//	return ret;
//}


EXPORT_SYMBOL(config);
//EXPORT_SYMBOL(msg_init_config_wrapper);
EXPORT_SYMBOL(config_addCleanup);
EXPORT_SYMBOL(config_removeCleanup);
MODULE_LICENSE("GPL");
module_init(start);
module_exit(end);
