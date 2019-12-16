//really simple thing
//only takes one command, doesnt spit out the output (yet)
// referenced https://gist.github.com/arunk-s/c897bb9d75a6c98733d6
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h>

//#include <linux/netlink.h>	//having apline's out of date linux-headers package installed breaks a lot of things
//so we just copy-pase
#define NLM_F_REQUEST		0x01
#define NETLINK_USERSOCK	2	/* Reserved for user mode socket protocols 	*/
#define NLMSG_ALIGNTO	4U
#define NLMSG_ALIGN(len) ( ((len)+NLMSG_ALIGNTO-1) & ~(NLMSG_ALIGNTO-1) )
#define NLMSG_HDRLEN	 ((int) NLMSG_ALIGN(sizeof(struct nlmsghdr)))
#define NLMSG_LENGTH(len) ((len) + NLMSG_HDRLEN)
#define NLMSG_SPACE(len) NLMSG_ALIGN(NLMSG_LENGTH(len))
#define NLMSG_DATA(nlh)  ((void*)(((char*)nlh) + NLMSG_LENGTH(0)))
typedef unsigned short __kernel_sa_family_t;
typedef unsigned int __u32;
typedef unsigned short __u16;
struct sockaddr_nl {
	__kernel_sa_family_t	nl_family;	/* AF_NETLINK	*/
	unsigned short	nl_pad;		/* zero		*/
	__u32		nl_pid;		/* port ID	*/
       	__u32		nl_groups;	/* multicast groups mask */
};
struct nlmsghdr {
	__u32		nlmsg_len;	/* Length of message including header */
	__u16		nlmsg_type;	/* Message content */
	__u16		nlmsg_flags;	/* Additional flags */
	__u32		nlmsg_seq;	/* Sequence number */
	__u32		nlmsg_pid;	/* Sending process port ID */
};


int sockky;
struct sockaddr_nl src_addr = {0};
struct sockaddr_nl dest_addr = {0};

int get_sockky(void){
	sockky = socket(PF_NETLINK, SOCK_RAW, NETLINK_USERSOCK);
	if(sockky < 0){
		return sockky;
	}
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0;
	dest_addr.nl_groups = 0;
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid();

	return bind(sockky, (struct sockaddr*)&src_addr, sizeof(src_addr));
	return 0;
}


int send_data (char * data){
	struct nlmsghdr *nlh = NULL;
	size_t len = strlen(data) + 1;
	size_t nllen = NLMSG_SPACE(len);
	nlh = (struct nlmsghdr *)malloc(nllen);
	memset(nlh, 0, nllen);
	nlh->nlmsg_len = nllen;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_pid = getpid();
	memcpy(NLMSG_DATA(nlh), data, len);

	send(sockky, nlh, nllen, 0);


	nlh = realloc(nlh, sizeof(struct nlmsghdr));
	recv(sockky, nlh, sizeof(struct nlmsghdr), MSG_PEEK);
	nlh = realloc(nlh, nlh->nlmsg_len);
	printf("header %i\n", nlh->nlmsg_len);
	recv(sockky,nlh,nlh->nlmsg_len, 0);
	printf("Response code: %i, Response data: %s\n", *(int*)NLMSG_DATA(nlh), (char*)NLMSG_DATA(nlh)+4);
	free(nlh);
	return 0;
}

int main(int argc, char ** argv){

	if(argc < 2){
		printf("Usage: %s command\n", argv[0]);
		return 1;
	}

	if(get_sockky()){
		printf("Error getting socket, must be run as root\n");
		return 2;
	}
	send_data(argv[1]);
	return 0;
}

