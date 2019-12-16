#ifndef DNSSETHEADER
#define DNSSETHEADER
typedef unsigned __int128 uint128_t;	//for ipv6s



typedef struct dnscache_s {
	struct spinlock table_lock;
	size_t host_entries_added;
	size_t ip_entries_added;
	size_t ipv6_entries_added;
	size_t host_entries_freed;
	size_t ip_entries_freed;
	size_t ipv6_entries_freed;

	size_t cachesize;
	struct rbnode * oldestnode;
	struct rbnode * newestnode;

	struct rb_root hosts;
	struct rb_root ips;
	struct rb_root ipv6s;
	struct hlist_node node;
} dnscache_t;

int add_dns_ref( const char* host );
int add_ip_ref( __be32 ip);
int add_ipv6_ref( uint128_t ipv6);
int handle_dns_packet( uint8_t* packet_data, size_t packet_data_len );


int netdb_init( void );

//doesnt unallocate memory for *d
int netdb_cleanup( void );


#endif
