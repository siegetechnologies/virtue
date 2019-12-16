#ifndef MOD_NET_H
#define MOD_NET_H

struct transducer_descr;
struct api_response;

extern struct transducer_descr netfilter_transducer;
extern void log_packet_data( void );

extern void add_netrule(char * args, struct api_response * out);

#endif //MOD_NET_H
