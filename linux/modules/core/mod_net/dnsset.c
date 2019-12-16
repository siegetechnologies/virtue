#include <linux/module.h>
#include <linux/rbtree.h>
#include <asm/string.h>
#include <linux/hashtable.h>
#include <linux/uuid.h>
#include <linux/ipv6.h>
#include <linux/spinlock.h>

#include "../../include/mod_allocator.h"
#include "../../include/mod_log.h"
#include "../../include/mod_config.h"	//for msg_in0twrapper

#include "dnsset.h"
//#define DEBUGPRINT
#define MAX_CACHE_SIZE 128

/*
DECLARE_HASHTABLE( pervirtue, 8 );
//struct mutex table_mut;

//had to switch to a spinny to avoid issues
struct spinlock table_lock;

size_t host_entries_added = 0;
size_t ip_entries_added = 0;
size_t ipv6_entries_added = 0;
size_t host_entries_freed = 0;
size_t ip_entries_freed = 0;
size_t ipv6_entries_freed = 0;
*/
//todo global stats

dnscache_t db = {0};

struct dnshdr {
	__be16 id;
	#if defined( __LITTLE_ENDIAN_BITFIELD )
	uint16_t
		//
		ra:1,
		z:1,
		ad:1,
		cd:1,
		rcode:4,
		qr:1,
		opcode:4,
		aa:1,
		tc:1,
		rd:1;
	#elif defined (__BIG_ENDIAN_BITFIELD )
	uint16_t
		qr:1,
		opcode:4,
		aa:1,
		tc:1,
		rd:1,
		ra:1,
		z:1,
		ad:1,
		cd:1,
		rcode:4;
	#else
	#error "No bitfield or not little endian"
	#endif
	__be16 qcount;
	__be16 acount;
	__be16 nscount;
	__be16 arcount;
};

struct rbnode {
//	dnscache_t * db;	//required to remove?
	struct rbnode * older; //doubley linked list for LRU cache
	struct rbnode * newer;
	struct rb_node node;
	size_t ref;
	char type;				// 1 for ip, 2 for ipv6, 3 for dns
	union {
		uint32_t ip;
		uint128_t ipv6;
		struct {
			uint32_t hosthash;	//possible to collide (unlikely), so we still check hostname
			char * hostname;
		};
	};
};


#define NODECHECK 	do{ 	if(!n){ printk(KERN_WARNING "%s:%i NODECHECK TRIGGERED by %s!\n", __FILE__, __LINE__, __func__); return 1; } }while(0);
#ifdef DEBUGPRINT
	#define OLDESTCHECK 	do { if(!oldestnode){ printk(KERN_WARNING "%s:%i OLDESTCHECK TRIGGERED by %s!\n", __FILE__, __LINE__, __func__); return 1; } }while(0);
	#define OLDESTCHECKNORET do { if(!oldestnode){ printk(KERN_WARNING "%s:%i OLDESTCHECK TRIGGERED by %s!\n", __FILE__, __LINE__, __func__); } }while(0);
	#define PRINTNODE 	do{ 	printk(KERN_WARNING "%s:%i %s called on %p! ->\n", __FILE__, __LINE__, __func__, n); \
					printk("newer: %p, older: %p, hostname: %p %s\n", n->newer, n->older, n->hostname, n->hostname); \
				}while(0);
	#define PRINTCACHE	do {	printk(KERN_WARNING "%s:%i %s cachesize: %li newestnode: %p oldestnode: %p\n", __FILE__, __LINE__, __func__, db.cachesize, db.newestnode, db.yoldestnode); }while(0);
#else
	#define PRINTNODE
	#define PRINTCACHE
	#define OLDESTCHECK
	#define OLDESTCHECKNORET
#endif
static inline int checkpow2(uint64_t in){
	//bit jank
	//ex 0b00010000 & 0b00001111 == 0, but 0b00010100 & 0b00010011 == 0b00010000
	return !(in & (in-1));
}

//send a message containing an update for a entry. This is usually called on pow2 of the entry.
static int send_entry_update_message(struct rbnode *n){
			struct partial_msg* msg;
			uint64_t conv;
			uint128_t conv6;
			NODECHECK
			PRINTNODE
//			msg = msg_init_config_wrapper( "maven.net", SEVERITY_INFO, 2 );
			msg = msg_init( "maven.net", SEVERITY_INFO, 2 );
			switch(n->type){
				case 1:	//ipv4
					conv = (uint64_t)ntohl( n->ip );
					msg_additem( msg, "update_ip_destination", &conv, TYPE_INT );
				break;
				case 2:	//ipv6
					conv6 = n->ipv6;	//todo actually convert?
					msg_additem( msg, "update_ipv6_destination", &conv6, TYPE_UUID );
				break;
				case 3:	//dns
					msg_additem( msg, "new_dns_query", n->hostname, TYPE_STRING );
				break;
				default:
					printk(KERN_WARNING "unknown rbnode type %i in dnsset\n", n->type);
					msg_additem( msg, "unknown_ip_type", &n->type, TYPE_INT );
				break;
			}

			msg_additem( msg, "refcount", &n->ref, TYPE_INT );
			//msg_additem( msg, "uuid", uuid, TYPE_UUID );
			msg_finalize( msg );
			return 0;
}
//send a message on a new entry.
static int send_entry_new_message(struct rbnode *n){
			struct partial_msg* msg;
			uint64_t conv;
			uint128_t conv6;
			NODECHECK
			PRINTNODE
//			msg = msg_init_config_wrapper( "maven.net", SEVERITY_INFO, 2 );
			msg = msg_init( "maven.net", SEVERITY_INFO, 2 );
			switch(n->type){
				case 1:	//ipv4
					conv = (uint64_t)ntohl( n->ip );
					msg_additem( msg, "new_ip_destination", &conv, TYPE_INT );
				break;
				case 2:	//ipv6
					conv6 = n->ipv6;	//todo actually convert?
					msg_additem( msg, "new_ipv6_destination", &conv6, TYPE_UUID );
				break;
				case 3:	//dns
					msg_additem( msg, "new_dns_query", n->hostname, TYPE_STRING );
				break;
				default:
					printk(KERN_WARNING "unknown rbnode type %i in dnsset\n", n->type);
					msg_additem( msg, "unknown_ip_type", &n->type, TYPE_INT );
				break;
			}
			//msg_additem( msg, "uuid", uuid, TYPE_UUID );
			msg_finalize( msg );
			return 0;
}

static int entry_cache_remove(struct rbnode * n){
	OLDESTCHECKNORET
	NODECHECK
	PRINTNODE
	PRINTCACHE
	//link neighbors
	if(n->older) n->older->newer = n->newer; //unlikely, as usually the oldest is removed
	else db.oldestnode = n->newer;
	if(n->newer) n->newer->older = n->older; //very likely
	else db.newestnode = n->older;

	db.cachesize--;
	PRINTCACHE
	PRINTNODE
	OLDESTCHECKNORET
	return 0;
}
static int entry_cache_move_newest(struct rbnode *n){
	OLDESTCHECKNORET
	NODECHECK
//	PRINTNODE


	if(n->older) n->older->newer = n->newer; //unlikely, as usually the oldest is removed
	else db.oldestnode = n->newer;
	if(n->newer) n->newer->older = n->older; //very likely
	else db.newestnode = n->older;



	n->newer = 0;
	n->older = db.newestnode;
	if(n->older) n->older->newer = n;
	else db.oldestnode = n;
	db.newestnode = n;

	OLDESTCHECKNORET
	return 0;
}

static int delete_entry(struct rbnode *n){
	OLDESTCHECKNORET
	NODECHECK
	PRINTNODE
	PRINTCACHE
	send_entry_update_message(n);
	entry_cache_remove(n);
	switch(n->type){
		case 1:
			rb_erase(&n->node, &db.ips);
			db.ip_entries_freed++;
		break;
		case 2:
			rb_erase(&n->node, &db.ipv6s);
			db.ipv6_entries_freed++;
		break;
		case 3:
			rb_erase(&n->node, &db.hosts);
			db.host_entries_freed++;
		break;
	}
	PRINTCACHE
	PRINTNODE
	maven_free(n);
	OLDESTCHECKNORET
	return 0;
}

static int entry_cache_add_and_evict( struct rbnode *n){
	int retval = 0;
	OLDESTCHECKNORET
	NODECHECK
	PRINTNODE
	PRINTCACHE
	db.cachesize++;
	while(db.cachesize > MAX_CACHE_SIZE){
		OLDESTCHECKNORET
		retval = 1;
		if(delete_entry(db.oldestnode)) break;
	}
	n->newer = 0;
	n->older = db.newestnode;
	if(n->older) n->older->newer = n;
	else db.oldestnode = n;
	db.newestnode = n;

	PRINTCACHE
	PRINTNODE
	OLDESTCHECKNORET
	return 0;
}


//case insensitive!
static unsigned int hash_hostname_case(char *nm){
	unsigned int rethash = 0;
	for(; *nm; nm++) rethash  = rethash * 31 + (*nm & 0b11011111);;
	return rethash;
}

typedef int nodecmp( struct rbnode*, struct rbnode* );
static struct rbnode *rbtree_search( struct rb_root *root, struct rbnode* key, nodecmp cmp );
static int rbtree_insert( struct rb_root *root, struct rbnode* key, nodecmp cmp );
static const char* dns_query_host( const char* inp, int max_len );



//if they do collide, that is fine, since we strcmp the source strings as well as compare hashes
//all child nodes will collide, so they will all compare strcmp
static int dnscmp( struct rbnode *n1, struct rbnode* n2 ) {
	if(n1->hosthash != n2->hosthash) return n1->hosthash - n2->hosthash;
	return strcasecmp(
		n1->hostname,
		n2->hostname );
}
static int ipcmp( struct rbnode *n1, struct rbnode *n2 ) {
	return n1->ip - n2->ip;
}

static int ipv6cmp( struct rbnode *n1, struct rbnode *n2 ) {
	if(n1->ipv6 == n2->ipv6) return 0;
	if((int)(n1->ipv6 - n2->ipv6)) return (int)(n1->ipv6 - n2->ipv6);
	return n1->ipv6 > n2->ipv6 ? 1 : -1;	//they are different, but overflow in such a way that when truncated to 32 bits, it equals 0 (which would mean they are the same), so i return -1 or +1
}

/*
static unsigned inline int uuid_key( uuid_t *uuid ) {
	return *((unsigned int*)uuid);
}
static void print_uuid( uuid_t *uuid ) {
	int j;
	for( j = 0; j < UUID_SIZE/sizeof(uint64_t); j++ ) {
		printk( "%llx", ((uint64_t*)(uuid->b))[j] );
	}
	printk( "\n" );
}
static struct hashtable_entry *get_by_uuid( uuid_t *uuid ) {
	struct hashtable_entry *cursor = NULL;
	unsigned int key = uuid_key( uuid );
	hash_for_each_possible( pervirtue, cursor, node, key ) {
		if( uuid_equal( uuid, &(cursor->uuid) ) ) {
			return cursor;
		}
	}
	return NULL;
}

static void __attribute__((unused)) print_hash_keys( void ) {
	int i;
	struct hashtable_entry* ptr = NULL;
	printk( "Printing hashtable entries\n" );
	hash_for_each( pervirtue, i, ptr, node ) {
		printk( "Key: %x\nuuid:", uuid_key( &(ptr->uuid) ) );
		print_uuid( &(ptr->uuid) );
	}
}

static struct hashtable_entry* add_uuid( uuid_t *uuid ) {
	unsigned int key = uuid_key( uuid );
	struct partial_msg* msg;
	struct hashtable_entry *new = maven_alloc( sizeof (*new ) );
	ALLOC_FAIL_CHECK( new );
	msg = msg_init_config_wrapper( "maven.net", SEVERITY_INFO, 1 );
	msg_additem( msg, "msg", "new network activity", TYPE_STRING );
	msg_finalize( msg );

	uuid_copy( &(new->uuid), uuid );
	new->hosts = RB_ROOT;
	new->ips = RB_ROOT;
	new->ipv6s = RB_ROOT;
	hash_add( pervirtue, &(new->node), key );
	//print_hash_keys();
	return new;
}
static struct hashtable_entry *find_or_make_uuid( uuid_t *uuid ) {
	struct hashtable_entry *ret = get_by_uuid( uuid );
	if( ret ) {return ret;}
	return add_uuid( uuid );
}

*/
//todo double check this? There is no +1?
static void __attribute__((unused)) dns_to_string( char* dnsquery ) {
	int tmp = *dnsquery;
	while( tmp ) {
		*dnsquery = '.';
		dnsquery += tmp;
		tmp = *dnsquery;
	}
}

//prints, but unlike dns_to_string, does not overwrite the source pointer
void prettyprint_dns(const char * host, int num){
	size_t i;
	size_t l = strlen(host)+1;
	char p[l];
	memcpy(p, host, l);

	for( i = 0 ;i < l  && p[i] ;  ) {
		size_t next = p[i]+1;
		p[i] = '.';
		i += next;
	}
	printk(KERN_INFO  "Added dns %s %i\n", p+1, num);
}
/*
 * add_dns_ref
 * increase the count for the number of times a dns address has been seen
 * If this is the first time, a log message will be emitted
 * the input can be pulled directly from the dns query data
 * returns the incremented count (minimum 1)
 */
int add_dns_ref( const char* host) {
	int ret;
	unsigned long table_spinflags;
	spin_lock_irqsave( &db.table_lock, table_spinflags);
	{
		struct rbnode key = {
			.hostname = (char*)host,
			.hosthash = hash_hostname_case((char*)host),
		};
		struct rbnode *tmp = NULL;
		struct rb_root *dnsroot;
		dnsroot = &(db.hosts);
		if( (tmp = rbtree_search( dnsroot, &key, dnscmp ) ) ) {
			ret = ++(tmp->ref);
			if(checkpow2(ret) && ret > 2) send_entry_update_message(tmp);
			entry_cache_move_newest(tmp);	//mark as most recently used
		} else {
			// new dns
			size_t namelen = strlen( host ) +1;
			size_t nodelen = sizeof(struct rbnode);
			//see netdb_free for why we need alignment
			struct rbnode *new = maven_alloc_aligned(nodelen+namelen, 4);
			ALLOC_FAIL_CHECK( new );
			db.host_entries_added++;
			//teeny little trick to save mallocs
			new->hostname = (char*)new + nodelen;
			memcpy( new->hostname, host, namelen );


			new->type = 3;
			new->ref = 1;
			new->hosthash = key.hosthash;
			rbtree_insert( dnsroot, new, dnscmp );
			send_entry_new_message(new);
			ret = new->ref;
			entry_cache_add_and_evict(new);	//add to the cache
		}
	}
	spin_unlock_irqrestore( &db.table_lock, table_spinflags );
//	printk(KERN_INFO "Added dns %s %i\n", host, ret);
	#ifdef DEBUGPRINT
	prettyprint_dns(host, ret);
	#endif
	return ret;
}


/*
 * add_ip_ref
 * increase the count for the number of times an ip address has been seen
 * If this is the first time, a log message will be emitted
 * the input can be pulled directly from the iphdr struct
 * returns the incremented count (minimum 1)
 */
int add_ip_ref( __be32 ip) {
	int ret;
	unsigned long table_spinflags;
					//this is in a macro, so this actually gets set
	spin_lock_irqsave( &db.table_lock, table_spinflags);
	{
		struct rbnode key = {
			.ip = ip,
		};
		struct rbnode *tmp = NULL;
		struct rb_root *iproot = &(db.ips);
		if( (tmp = rbtree_search( iproot, &key, ipcmp ) ) ) {
			ret = ++(tmp->ref);
			if(checkpow2(ret) && ret > 2) send_entry_update_message(tmp);
			entry_cache_move_newest(tmp);
		} else {
			struct rbnode *new = maven_alloc_aligned(sizeof(*new), 4);
			ALLOC_FAIL_CHECK( new );

			db.ip_entries_added++;
			new->type = 1;
			new->ip = ip;
			new->ref = 1;

			send_entry_new_message(new);

			rbtree_insert( iproot, new, ipcmp );
			ret = new->ref;
			entry_cache_add_and_evict( new);
		}
	}
	spin_unlock_irqrestore( &db.table_lock, table_spinflags);
	return ret;
}
/*
 * add_ipv6_ref
 * increase the count for the number of times an ip address has been seen
 * If this is the first time, a log message will be emitted
 * the input can be pulled directly from the iphdr struct
 * returns the incremented count (minimum 1)
 */
int add_ipv6_ref( uint128_t ipv6 ) {
	int ret;
	unsigned long table_spinflags;
	spin_lock_irqsave( &db.table_lock, table_spinflags);
	{
		struct rbnode key = {
			.ipv6 = ipv6,
		};
		struct rbnode *tmp = NULL;
//		struct hashtable_entry *entry = find_or_make_uuid( uuid );
		struct rb_root *ip6root = &(db.ipv6s);
		if( (tmp = rbtree_search( ip6root, &key, ipv6cmp ) ) ) {
			ret = ++(tmp->ref);
			if(checkpow2(ret) && ret > 2) send_entry_update_message(tmp);
			entry_cache_move_newest(tmp);
		} else {
			struct rbnode *new = maven_alloc_aligned(sizeof(*new), 4);
			ALLOC_FAIL_CHECK( new );

			db.ipv6_entries_added++;
			new->type = 2;
			new->ipv6 = ipv6;
			new->ref = 1;

			send_entry_new_message(new);

			rbtree_insert( ip6root, new, ipv6cmp );
			ret = new->ref;
			entry_cache_add_and_evict( new);
		}
	}
	spin_unlock_irqrestore( &db.table_lock, table_spinflags );
	return ret;
}

static struct rbnode *rbtree_search( struct rb_root *root, struct rbnode* key, nodecmp cmp) {
	struct rb_node *node = root->rb_node;
	while( node ) {
		struct rbnode *visit = container_of( node, struct rbnode, node );
		int result = cmp( key, visit );
		if( result < 0 ) {
			node = node->rb_left;
		} else if (result > 0) {
			node = node->rb_right;
		} else {
			return visit;
		}
	}
	return NULL;
}

static int rbtree_insert( struct rb_root *root, struct rbnode* key, nodecmp cmp ) {
	struct rb_node **new = &(root->rb_node), *parent = NULL ;
//	printk(KERN_INFO "inserting %p\n", key);
	while( *new ) {
		struct rbnode *visit = container_of( *new, struct rbnode, node );
		int result = cmp( key, visit );
		parent = *new;
		if( result < 0 ) {
			new = &((*new)->rb_left);
		} else if ( result > 0 ) {
			new = &((*new)->rb_right );
		} else {
			return 1;
		}
	}
	rb_link_node( &key->node, parent, new );
	rb_insert_color( &key->node, root );
	return 0;
}

static const char* dns_query_host( const char* inp, int max_len ) {
	size_t i = 0;
	size_t strlen;
	while( 1 ) {
		if( i >= max_len ) { //need to fail if i==max_len, since we will lookahead one char anyway
			return NULL;
		}
		if( !inp[i] ) { // if 0, end of record
			return inp;
		}

		//otherwise, go to next string
		strlen = inp[i];
//		inp[i] = '.';
		i += strlen+1;
	}
}

int handle_dns_packet( uint8_t* packet_data, size_t packet_data_len ) {
	struct dnshdr *dns_header = NULL;
	if( packet_data_len < sizeof( *dns_header ) ) {
		printk( "Packet is not large enough to be a dns query\n" );
		return 1;
	}

	dns_header = (struct dnshdr *)packet_data;
	if( ntohs(dns_header->qcount) < 1 ) {
		printk("No dns queries in this packet" );
	} else {
		const char* hostname = dns_query_host( (char*)dns_header+sizeof(*dns_header), packet_data_len - sizeof( *dns_header ) );
		if( hostname ) {
			add_dns_ref( hostname);
		}
	}
	return 0;
}



int netdb_init( void ){
//	struct partial_msg* msg;
	memset(&db, 0, sizeof(dnscache_t));
	spin_lock_init(&db.table_lock);

	db.hosts = RB_ROOT;
	db.ips = RB_ROOT;
	db.ipv6s = RB_ROOT;


//since this calles the registry_wrapper, it tries to grab a read lock.... this no worky because we are in a write_lock here
//could just not use the wrapper, which would still work as intended.
//but do we really need this message?
//	msg = msg_init_config_wrapper( "maven.net", SEVERITY_INFO, 1 );
//	msg_additem( msg, "msg", "new network activity", TYPE_STRING );
//	msg_finalize( msg );

	return 0;
}


int netdb_cleanup ( void ){
	struct rbnode *cursor;
	struct rbnode *tmp;
	//rbtree nodes need to be aligned to 4 for this to not crash... it uses metadata from pointers to save the color
	rbtree_postorder_for_each_entry_safe( cursor, tmp, &(db.hosts), node ) {
		//not freeing cursor->hostname here, because we do a trick where we allocate more than we need for cursor, and just append the hostname to the end of it
		maven_free( cursor );
		db.host_entries_freed++;
	}
	rbtree_postorder_for_each_entry_safe( cursor, tmp, &(db.ips), node ) {
		maven_free( cursor );
		db.ip_entries_freed++;
	}
	rbtree_postorder_for_each_entry_safe( cursor, tmp, &(db.ipv6s), node ) {
		maven_free( cursor );
		db.ipv6_entries_freed++;
	}
	printk( KERN_INFO "\tips added: %lu\nips freed: %lu\n", db.ip_entries_added, db.ip_entries_freed );
	printk( KERN_INFO "\tipv6s added: %lu\nipv6s freed: %lu\n", db.ipv6_entries_added, db.ipv6_entries_freed );
	printk( KERN_INFO "\thosts added: %lu\nhosts freed: %lu\n", db.host_entries_added, db.host_entries_freed );
	return 0;
}
