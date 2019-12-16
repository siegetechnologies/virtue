#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/string.h>
#include <linux/bug.h>
#include <linux/inet.h>
#include <linux/spinlock.h>


#include "../../include/common.h"
#include "../../include/mod_net.h"
#include "../../include/mod_allocator.h"
#include "../../include/mod_log.h"	//maven_panic
#include "../../include/mod_config.h"
#include "../../include/mod_listener.h"	//registering endpoints
#include "netrules.h"

#define NETRULE_INVALID 0
#define NETRULE_BLACK 1
#define NETRULE_WHITE 2


typedef struct netrule_s {
	uint8_t type;
	uint32_t ip;
	uint32_t netmask;
} netrule_t;


typedef struct netruleipv6_s {
	uint8_t type;
	uint128_t ip;
	uint128_t netmask;
} netruleipv6_t;

//decided on tracking netrules seperately from puddins



// i am using an array for speed of parsing.
//size_t num_netrules = 0;
//netrule_t * netrules = 0;

//size_t num_netrulesipv6 = 0;
//netruleipv6_t * netrulesipv6 = 0;

endpoint_t nrep = {
	"add_netrule",
	add_netrule,
	0
};



void nr_conf_cleanup(config_t *c){
	if(c->netrules){
		netrules_free(c->netrules);
		maven_free(c->netrules);
	}
	c->netrules = 0;
}

void nr_startup(void){
	config_addCleanup(nr_conf_cleanup);
	endpoint_register(&nrep);
}
void nr_shutdown(void){
	config_removeCleanup(nr_conf_cleanup);
	endpoint_unregister(&nrep);
}



void netrules_free(struct netruler_s * r){
	unsigned long spinflags = 0;
	unsigned long spinflags2 = 0;
	write_lock_irqsave(&r->lock, spinflags);
		if(r->netrules) maven_free(r->netrules);
	write_lock_irqsave(&r->ipv6lock, spinflags2);
		if(r->netrulesipv6) maven_free(r->netrulesipv6);
		memset(r, 0, sizeof(struct netruler_s));
	write_unlock_irqrestore(&r->ipv6lock, spinflags2);
	write_unlock_irqrestore(&r->lock, spinflags);
}
void netrules_init(struct netruler_s *r){
	rwlock_init(&r->lock);
	rwlock_init(&r->ipv6lock);
	r->num_netrules = 0;
	r->num_netrulesipv6 = 0;
	r->netrules = 0;
	r->netrulesipv6 = 0;
}

//size_t num_netruleipv6 = 0;
//netruleipv6_t * netrules = 0;
uint8_t netrule_default = NETRULE_WHITE;	//todo add support for changing
uint8_t netrule_defaultipv6 = NETRULE_WHITE;	//todo add support for changing

static inline int track_netrule(struct netruler_s *r, netrule_t n){
	unsigned long spinflags = 0;
	write_lock_irqsave(&r->lock, spinflags);
		r->netrules = maven_realloc(r->netrules, (r->num_netrules+1) * sizeof(netrule_t));
		ALLOC_FAIL_CHECK(r->netrules);
		r->netrules[r->num_netrules] = n;
		r->num_netrules++;
	write_unlock_irqrestore(&r->lock, spinflags);
	return 0;
}

static inline int track_netruleipv6(struct netruler_s *r, netruleipv6_t n){
	unsigned long spinflags = 0;
	write_lock_irqsave(&r->ipv6lock, spinflags);
		r->netrulesipv6 = maven_realloc(r->netrulesipv6, (r->num_netrulesipv6+1) * sizeof(netruleipv6_t));
		ALLOC_FAIL_CHECK(r->netrulesipv6);
		r->netrulesipv6[r->num_netrulesipv6] = n;
		r->num_netrulesipv6++;
	write_unlock_irqrestore(&r->ipv6lock, spinflags);
	return 0;
}

uint8_t lookup_netrule_ipv4(struct netruler_s *r, uint32_t ip){
	size_t i;
	unsigned long spinflags = 0;
	read_lock_irqsave(&r->lock, spinflags);
	for(i = 0; i < r->num_netrules; i++){
		//netmask is pre-applied to each rule
		if(r->netrules[i].ip == (ip & r->netrules[i].netmask)
		&& r->netrules[i].type != NETRULE_INVALID){
			read_unlock_irqrestore(&r->lock, spinflags);
			return r->netrules[i].type;
		}
	}
	//ip did not match any rules
	read_unlock_irqrestore(&r->lock, spinflags);
	return netrule_default;
}

uint8_t lookup_netrule_ipv6(struct netruler_s *r, uint128_t ip){
	size_t i;
	unsigned long spinflags = 0;
	read_lock_irqsave(&r->ipv6lock, spinflags);
	for(i = 0; i < r->num_netrulesipv6; i++){
		//netmask is pre-applied to each rule
		if(r->netrulesipv6[i].ip == (ip & r->netrulesipv6[i].netmask)
		&& r->netrulesipv6[i].type != NETRULE_INVALID){
			read_unlock_irqrestore(&r->ipv6lock, spinflags);
			return r->netrulesipv6[i].type;
		}
	}
	//ip did not match any rules
	read_unlock_irqrestore(&r->ipv6lock, spinflags);
	return netrule_defaultipv6;
}

void add_netrule(char * r, struct api_response * out_api) {
	//a lot of todo here

	int res;
	char * addr;
	char * type;
	char * netmask;
	int choice = 0;

	netrule_t new = {0};
	netruleipv6_t newipv6 = {0};

	type = r;
	if(!type) goto usage;
	addr = split_on(type, ' ');
	if(!addr) goto usage;
	// split along / for netmask
	netmask = split_on(addr, '/');

	type = strim(type);
	addr = strim(addr);
	if(netmask) netmask = strim(netmask);

	if(!type) goto usage;
	if(!addr) goto usage;



	if(!strcasecmp(type, "invalid")){
		new.type = newipv6.type =  NETRULE_INVALID;
//	} else if(!strcasecmp(r, "white") || !strcasecmp(r, "whitelist")){
	} else if(!strncasecmp(type, "white", 5)){
		new.type = newipv6.type = NETRULE_WHITE;
	} else if(!strncasecmp(type, "black", 5)){
		new.type = newipv6.type = NETRULE_BLACK;
	} else {
		out_api->code = 1;
		out_api->msg = "Invalid type";
		return;
	}
	//ok, all args seem to be OK, lets check

	//detect if it is an ipv6
	printk(KERN_INFO "|%s|%s|%s|", r, addr, netmask);
	if(strchr(addr, ':')){
		res = in6_pton(addr, -1, (uint8_t*)&newipv6.ip, -1, NULL);
		if(netmask){
			res &= in6_pton(netmask, -1, (uint8_t*)&newipv6.netmask, -1, NULL);
			newipv6.ip &= newipv6.netmask;
		} else {
			newipv6.netmask = ~0;
		}
		if(!res){
			out_api->code = 1;
			out_api->msg = "Invalid ipv6 address or netmask";
			return;
		}
		choice = 1;
//		track_netruleipv6(newipv6);
//		printk(KERN_ERR "In progress");
	} else {	//ipv4
		res = in4_pton(addr, -1, (uint8_t*)&new.ip, -1, NULL);
		if(netmask){
			res &= in4_pton(netmask, -1, (uint8_t*)&new.netmask, -1, NULL);
			new.ip &= new.netmask;
		} else {
			new.netmask = ~0;
		}
		if(!res){
			out_api->code = 1;
			out_api->msg = "Invalid ipv4 address or netmask";
			return;
		}
		choice = 2;
//		track_netrule(new);
//		printk(KERN_ERR "In progress");
	}
	printk(KERN_INFO "%s %s %s", r, addr, netmask);

	//so our netrule is all set up, lets track it

	//if netrules hasnt been set up, set it up!
	if(!config.netrules){
		config.netrules= maven_alloc(sizeof(netruler_t));
		ALLOC_FAIL_CHECK(config.netrules);	//force it
		netrules_init(config.netrules);
	}

	//ok, lets adderit

	if(choice == 1){
		track_netruleipv6(config.netrules, newipv6);
	} else if(choice == 2){
		track_netrule(config.netrules, new);
	}

	out_api->code = 0;
	out_api->msg = "Sucessfully added rule";

	return;

	usage:
		//TODO idea, if no ip address, set default?
		out_api->code = 1;
		//todo multiple args?
		out_api->msg = "Usage: add_netrule <type> <ip_address>[/netmask]";
		return;
}


