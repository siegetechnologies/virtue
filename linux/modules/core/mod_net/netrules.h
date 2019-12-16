#ifndef NETRULESHEADER
#define NETRULESHEADER
typedef unsigned __int128 uint128_t;

#define NETRULE_INVALID	0
#define NETRULE_BLACK	1
#define NETRULE_WHITE	2

typedef struct netruler_s {
	size_t num_netrules;
	struct netrule_s *netrules;
	size_t num_netrulesipv6;
	struct netruleipv6_s * netrulesipv6;    //stretchy buffers
	rwlock_t lock;
	rwlock_t ipv6lock;
} netruler_t;

struct api_response;

uint8_t lookup_netrule_ipv4(struct netruler_s *r, uint32_t ip);
uint8_t lookup_netrule_ipv6(struct netruler_s *r, uint128_t ip);
void add_netrule(char *r, struct api_response * out);
void netrules_free(struct netruler_s *r);
void netrules_init(struct netruler_s *r);

void nr_startup(void);
void nr_shutdown(void);

#endif
