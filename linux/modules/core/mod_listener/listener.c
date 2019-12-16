#include <linux/kernel.h>
#include <linux/module.h>
//#include <linux/kprobes.h>
//#include <linux/ptrace.h> // for regs_return_value
#include <asm/string.h>
#include <linux/netlink.h>
#include <linux/spinlock.h> //for rwlock stuff
//#include <linux/mutex.h>
//#include <linux/uuid.h>
//#include <asm/processor.h>
#include <net/net_namespace.h>
#include <net/sock.h>

//#include "api.h"

#include "../../include/common.h"
#include "../../include/mod_allocator.h"
#include "../../include/mod_log.h"
//#include "../../include/mod_syscall.h"
#include "../../include/mod_config.h"
#include "../../include/mod_utils.h"
#include "../../include/mod_listener.h"


//rwlock_t endpoint_pop;

endpoint_t * eps = 0;

rwlock_t endpoint_pop;

int endpoint_register(endpoint_t *ep){
	unsigned long spinflags;
	if(!ep) return 2;
	if(!ep->cab) return 1;
	if(!ep->name || !strlen(ep->name)) return 1;
	printk(KERN_INFO "Registering an endpoint %s\n", ep->name);
	write_lock_irqsave(&endpoint_pop, spinflags);
		ep->next = eps;
		wmb();
		eps = ep;
	write_unlock_irqrestore(&endpoint_pop, spinflags);
	return 0;
}
EXPORT_SYMBOL(endpoint_register);


int endpoint_unregister(endpoint_t *ep){
	unsigned long spinflags;
	endpoint_t *e, **eq;
	int ret = 0;
	if(!ep) return 2;
	//upgradable read locks would be nice
	printk(KERN_INFO "Unregistering an endpoint %s\n", ep->name);
	write_lock_irqsave(&endpoint_pop, spinflags);
		//find the thing
		for(e = eps, eq = &eps; e; eq = &e->next, e = e->next){
			//found it!
			if(e == ep){
				*eq = e->next;
				e->next = 0;
				wmb();
				break;
			}
		}
		ret = 1;
	write_unlock_irqrestore(&endpoint_pop, spinflags);
	return ret;
}
EXPORT_SYMBOL(endpoint_unregister);

static int _call_api(listenercallback_t c, char * args, struct api_response * resp){

	char * old_resp_msg = resp->msg;
	int old_resp_free = resp->should_free_msg;
	int old_resp_code = resp->code;

	resp->msg = 0;
	resp->should_free_msg = 0;
	resp->code = 0;

	c(args, resp);
	if(!resp->msg){
		resp->msg = "ERROR: no return message";
		resp->should_free_msg = 0;
	}

	if(old_resp_msg){
		char * cur_resp_msg = resp->msg;
		int cur_resp_free = resp->should_free_msg;

		size_t old_len = strlen(old_resp_msg);
		size_t cur_len = strlen(resp->msg);

		resp->msg = maven_alloc(cur_len + old_len + 2);	//old_message newline current_message null
		ALLOC_FAIL_CHECK(resp->msg);

		resp->should_free_msg = 1;

		memcpy(resp->msg, old_resp_msg, old_len);
		resp->msg[old_len] = '\n';
//		memcpy(resp->msg + old_len + 1, cur_resp_msg, cur_len);
//		resp->msg[old_len+cur_len+1] = 0;
		memcpy(resp->msg + old_len + 1, cur_resp_msg, cur_len+1);	//+1 copies the null for me

		if(old_resp_free){
			maven_free(old_resp_msg);
		}
		if(cur_resp_free){
			maven_free(cur_resp_msg);
		}
		if(old_resp_code && !resp->code){	//if any of the previous messages had a non 0 return code, make sure that the return code is non zero
			resp->code = old_resp_code;
		}
	}
	return 0;
}

static int _parse_api(char * msg, struct api_response * resp){
	char * args;
	endpoint_t *e;
	unsigned long spinflags;
	listenercallback_t c = 0;
	struct partial_msg * logmsg;
	if(!msg) return -1;
	args = split_on(msg, ' ');

	read_lock_irqsave(&endpoint_pop, spinflags);
		for(e = eps; e; e= e->next){
			if( streq(msg, e->name) && e->cab ){
				c = e->cab;
				_call_api(c, args, resp);
			}
		}
	read_unlock_irqrestore(&endpoint_pop, spinflags);

//	logmsg = msg_init_config_wrapper("maven.api", SEVERITY_INFO, 2);
	logmsg = msg_init("maven.api", SEVERITY_INFO, 2);
	if(c){
		msg_additem(logmsg, "Called api function", msg, TYPE_STRING);
		if(args) msg_additem(logmsg, "args", args, TYPE_STRING);
	} else {
		resp->code = -1;
		resp->msg = "Unknown command";
		resp->should_free_msg = 0;
		msg_additem(logmsg, "Invalid api function", msg, TYPE_STRING);
	}
	msg_finalize(logmsg);

	return resp->code;
}


#define DEFAULT_RISK 0

uint32_t risk = DEFAULT_RISK;

uint32_t get_risk( void ) {
	return risk;
}


static struct sock* nl_sock = NULL;

static void parse_and_respond( char* msg, pid_t send_pid ) {
	struct api_response resp = {
		.code = 0,
		.msg = NULL,
		.should_free_msg = 0,
	};
	struct sk_buff *skb_out;
	struct nlmsghdr *nl_hdr = NULL;
	size_t msg_size;
	int* data_buf;
	char* msg_buf;
	resp.should_free_msg = 0;
	_parse_api( msg, &resp );

	BUG_ON( !resp.msg );
	printk( "Response msg is %s\n", resp.msg );
	msg_size = strlen(resp.msg) + sizeof(resp.code) + 1;
	skb_out = nlmsg_new( msg_size, GFP_KERNEL );
	nl_hdr = nlmsg_put( skb_out, 0, 0, NLMSG_DONE, msg_size, 0 );
	NETLINK_CB(skb_out).dst_group = 0;
	data_buf = nlmsg_data( nl_hdr );
	msg_buf = (char*)(data_buf)+4;
	*data_buf = resp.code;

//TODO verify that there is a null terminator (use strncpy? or manually put a nully)
	memcpy( msg_buf, resp.msg, msg_size-sizeof(resp.code));
	nlmsg_unicast( nl_sock, skb_out, send_pid );


	// send response on nl_sock
	if( resp.should_free_msg ) {
		maven_free( resp.msg );
	}
	return;
}



/*
 * Netlink callback function
 */


int do_nl = 1;

static void nl_in_cb( struct sk_buff *skb ) {
	struct nlmsghdr *nl_hdr = NULL;
	uint32_t msg_len = 0;
	uint32_t payload_len = 0;
	char* msg = NULL;
	uint64_t uid = __kuid_val( current->real_cred->uid );
	if(!do_nl){
		//this might break
		if(nl_sock) netlink_kernel_release( nl_sock );
		nl_sock = 0;
		return;
	}
	if( uid != 0 && !is_containerized(current)) {
//TODO also verify that spamming the "api request from non root process" msg will not cause issues (OOM?)
//		struct partial_msg* msg = msg_init_config_wrapper( "maven.listener", SEVERITY_WARNING, 2 );
		struct partial_msg* msg = msg_init( "maven.listener", SEVERITY_WARNING, 2 );
		msg_additem( msg, "api request from non-root process", current->comm, TYPE_STRING );
		msg_additem( msg, "uid", &uid, TYPE_INT );
		msg_finalize( msg );
		return;
	}
	nl_hdr = (struct nlmsghdr*)skb->data;
	msg_len = nl_hdr->nlmsg_len;
	payload_len = msg_len - NLMSG_HDRLEN;
	msg = maven_alloc( payload_len+1 );
	if(!msg){
		//TODO respond appropriately with a specific error?
		parse_and_respond( "BROKEN", nl_hdr->nlmsg_pid );
		return;
	}
	msg[payload_len]= '\0';
	memcpy( msg, nlmsg_data(nl_hdr), payload_len );
	parse_and_respond( msg, nl_hdr->nlmsg_pid );

	maven_free( msg );

}

static struct netlink_kernel_cfg nl_cfg = {
	.groups=0,
	.flags=0,
	.input=nl_in_cb,
};


void stop_listener(char *r, struct api_response * out_api){
	struct partial_msg* msg = msg_init( "maven.listener", SEVERITY_WARNING, 2 );
	msg_additem( msg, "stopping listen api", current->comm, TYPE_STRING );
	msg_finalize( msg );
	printk(KERN_INFO "mod_listener: stopping listen api\n");
//	if(nl_sock) netlink_kernel_release( nl_sock );
//	nl_sock = 0;
	out_api->code = 0;
	out_api->msg = "stopping listen\n";
	do_nl = 0;	//postpone full NL teardown until later, so i can respond
	return;
}

endpoint_t srep = {
	"stop_listener",
	stop_listener,
	0
};

static int __init start(void) {
	printk( KERN_INFO "mod_listener: loading\n" );
	rwlock_init(&endpoint_pop);
	nl_sock = netlink_kernel_create( &init_net, NETLINK_USERSOCK, &nl_cfg );
//	transducer_init( );
	maven_allocator_info();

	endpoint_register(&srep);
	printk( KERN_INFO "mod_listener: loaded\n" );
	msg_moduleload("mod_listener");
	return 0;
}

static void __exit end(void) {
	msg_moduleunload("mod_listener");
	printk( KERN_INFO "mod_listener: unloading\n" );
	endpoint_unregister(&srep);
	if(nl_sock) netlink_kernel_release( nl_sock );
	nl_sock = 0;
	maven_allocator_info();
	printk( KERN_INFO "mod_listener: unloaded\n" );
}


MODULE_LICENSE("GPL");
module_init(start);
module_exit(end);
