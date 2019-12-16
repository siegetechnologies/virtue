#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/keyboard.h>
#include <linux/netfilter.h>
#include <linux/netfilter_defs.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/version.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/uuid.h>

#include "../../include/common.h"
#include "../../include/mod_net.h"
#include "../../include/mod_allocator.h"
#include "../../include/mod_log.h"
#include "../../include/mod_config.h"

#include "dnsset.h"
#include "netrules.h"
/*
A networking module for the MAVEN microvisor
currently, logs IPV4 and IPV6 connections, DNS requests, and has a simple rule based system to block or accept packets.
*/


/*
These values are in netfilter_ipv4.h, but are only exposed to user programs
 I'm sure redefining them and using them in the kernel is bad or whatever
*/
#define NF_IP_PRE_ROUTING 0
#define NF_IP_LOCAL_IN 1
#define NF_IP_LOCAL_OUT 3

#define PROTO_UDP	(17)
#define PROTO_TCP	(6)
#define PROTO_ICMP	(1)
#define PROTO_ICMPV6	(58)
#define PROTO_IGMP	(2)

/*
typedef struct netoptions_s {
	char dnslogenabled;
	char iplogenabled;
//TODO
	char netruleenabled;
} netoptions_t;
*/

static int netfilter_enabled = 0;
//todo do these need to be made atomic?
static uint64_t num_tcp = 0;
static uint64_t num_udp = 0;
static uint64_t num_icmp = 0;
static uint64_t num_igmp = 0;
static uint64_t num_other = 0;



//TODO throw this on a timer
void log_packet_data( void ) {
//	struct partial_msg *msg = msg_init_config_wrapper( "maven.net", SEVERITY_INFO, 5 );
	struct partial_msg *msg = msg_init( "maven.net", SEVERITY_INFO, 5 );
	if( !netfilter_enabled ) {
		return;
	}
	msg_additem( msg, "tcp packets", &num_tcp, TYPE_INT );
	msg_additem( msg, "udp packets", &num_udp, TYPE_INT );
	msg_additem( msg, "icmp packets", &num_icmp, TYPE_INT );
	msg_additem( msg, "igmp packets", &num_igmp, TYPE_INT );
	msg_additem( msg, "other packets", &num_other, TYPE_INT );
	msg_finalize( msg );
	num_tcp = 0;
	num_udp = 0;
	num_icmp = 0;
	num_igmp = 0;
	num_other = 0;
}
EXPORT_SYMBOL( log_packet_data );


//returns 1 if tcp, 2 if udp, 0 if anything else, but you already knew that
static int read_transport(int protocol, struct sk_buff *skb, size_t *header_len, uint16_t *dest, uint16_t *source){
	switch(protocol){
		case PROTO_UDP: {
			struct udphdr* udp_header = udp_hdr( skb );
			num_udp++;
			*dest = ntohs(udp_header->dest);
			*source = ntohs(udp_header->source);
			*header_len = sizeof( *udp_header );
			return 2;
		} break;
		case PROTO_TCP: {
			struct tcphdr* tcp_header = tcp_hdr( skb );
			num_tcp++;
			*dest = ntohs(tcp_header->dest);
			*source = ntohs(tcp_header->source);
			*header_len = (tcp_header->doff)*4;
			return 1;
		} break;

		//"others"
		case PROTO_ICMPV6:
		case PROTO_ICMP:
			num_icmp++;
		break;
		case PROTO_IGMP:
			num_igmp++;
		break;
		default:
			printk( "Unknown protocol number %d\n", protocol );
			num_other++;
		break;
	}
	return 0;
}

static int handle_packet( struct sk_buff *skb) {
	struct iphdr *ip_header = ip_hdr(skb);
	uint8_t* packet_data;
	size_t packet_data_len;
	size_t header_len;
	uint16_t dest;
	uint16_t source;
	//ignore if it isnt a udp or tcp packet
	if(!read_transport(ip_header->protocol, skb, &header_len, &dest, &source)) return 0;
//	printk("dest/src is %i/%i\n", dest, source);	//a very "verbose" print
	if( dest == 53 ) {
		packet_data = (uint8_t*)(skb_transport_header(skb)+header_len);
		packet_data_len = ntohs(ip_header->tot_len) + sizeof( *ip_header ) + header_len;
		handle_dns_packet( packet_data, packet_data_len );
	}
	return 0;
}
static int handle_packet_ipv6( struct sk_buff *skb ) {
	struct ipv6hdr *ipv6_header = ipv6_hdr(skb);
	uint8_t* packet_data;
	size_t packet_data_len;
	size_t header_len;
	uint16_t dest;
	uint16_t source;
	//ipv6 nexthdr field is analagous to ipv4 protocal field
	//ignore if it isnt a udp or tcp packet
	if(!read_transport(ipv6_header->nexthdr, skb, &header_len, &dest, &source)) return 0;
//	printk("dest/src is %i/%i\n", dest, source);
	if( dest == 53 ) {
		packet_data = (uint8_t*)(skb_transport_header(skb)+header_len);
		//we dont work with jumbopackets yet... TODO?
		if(!ipv6_header->payload_len){
			printk(KERN_WARNING "mod_net: ipv6 dns jumbopacket, ignoring\n");
			return -1;
		}
		packet_data_len = ntohs(ipv6_header->payload_len) + sizeof( *ipv6_header ) + header_len;
		handle_dns_packet( packet_data, packet_data_len );
	}
	return 0;
}

unsigned int hook_func( void* priv, struct sk_buff* skb, const struct nf_hook_state* state) {

	struct iphdr* ip_header = ip_hdr(skb);

	if(ip_header->version == 4){
		handle_packet( skb );
		add_ip_ref(ip_header->daddr);
	} else if(ip_header->version == 6){
		struct ipv6hdr* ipv6_header = ipv6_hdr(skb);
		handle_packet_ipv6(skb);
		add_ipv6_ref(*((uint128_t *)&ipv6_header->daddr) );
	} else {
		printk(KERN_WARNING "Unknown ip header version %i!\n", ip_header->version);
	}

	if(!config.netrules){
		// do nothin, no netrules made yet
	} else if(ip_header->version == 4){
		if(lookup_netrule_ipv4(config.netrules, ip_header->daddr) == NETRULE_BLACK) return NF_DROP;
		if(lookup_netrule_ipv4(config.netrules, ip_header->saddr) == NETRULE_BLACK) return NF_DROP;
	} else if(ip_header->version == 6){
		struct ipv6hdr* ipv6_header = ipv6_hdr(skb);
		if(lookup_netrule_ipv6(config.netrules, *((uint128_t *)&ipv6_header->daddr)) == NETRULE_BLACK) return NF_DROP;
		if(lookup_netrule_ipv6(config.netrules, *((uint128_t *)&ipv6_header->saddr)) == NETRULE_BLACK) return NF_DROP;
	}
//	accept:
	return NF_ACCEPT;
}
static struct nf_hook_ops hookipv4 = {
	.hook = hook_func,
	.hooknum = NF_INET_POST_ROUTING,
//	.hooknum = NF_IP_LOCAL_IN,	//only hooks incoming
	.pf = PF_INET,
	.priority = NF_IP_PRI_FIRST,
};
static struct nf_hook_ops hookipv6 = {
	.hook = hook_func,
	.hooknum = NF_INET_POST_ROUTING,
//	.hooknum = NF_IP_LOCAL_OUT,	//only hooks outgoing
	.pf = PF_INET6,
	.priority = NF_IP_PRI_FIRST,
};
static int set_netfilter_enabled( int on ) {
	if( on ) {
		if( !netfilter_enabled ) {
			#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
			  nf_register_net_hook(&init_net, &hookipv4);
			  nf_register_net_hook(&init_net, &hookipv6);
			#else
			  nf_register_hook(&hookipv4);
			  nf_register_hook(&hookipv6);
			#endif
			netfilter_enabled = 1;
		}
	} else {
		if( netfilter_enabled ) {
			#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
			  nf_unregister_net_hook(&init_net, &hookipv4);
			  nf_unregister_net_hook(&init_net, &hookipv6);
			#else
			  nf_unregister_hook(&hookipv4);
			  nf_unregister_hook(&hookipv6);
			#endif
			netfilter_enabled = 0;
		}
	}
	return netfilter_enabled;
}

static int get_netfilter_enabled( void ) {
	return netfilter_enabled;
}

struct transducer_descr netfilter_transducer = {
	.name = "Netfilter",
	.get_enabled = get_netfilter_enabled,
	.set_enabled = set_netfilter_enabled,
};
EXPORT_SYMBOL( netfilter_transducer );



static int __init interceptor_start(void) {
	printk(KERN_INFO "mod_net: loading\n");
//	register_netcallbacks(netobject_make, netobject_delete);
	nr_startup();

	netdb_init();
	set_netfilter_enabled( 1 );


	maven_allocator_info();
	printk(KERN_INFO "mod_net: loaded\n");
	msg_moduleload("mod_net");
	return 0;
}


static void __exit interceptor_end(void) {
	msg_moduleunload("mod_net");
	printk(KERN_INFO "mod_net: unloading\n");
	set_netfilter_enabled( 0 );
	netdb_cleanup();
	nr_shutdown();
//	register_netcallbacks(NULL, NULL);
	//todo trigger mod_registry to remove all of em!
//	netrules_free();

	maven_allocator_info();
	printk(KERN_INFO "mod_net: unloaded\n");

}

EXPORT_SYMBOL( add_netrule );
MODULE_LICENSE("GPL");
module_init(interceptor_start);
module_exit(interceptor_end);
