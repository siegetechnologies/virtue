#ifndef FILEBLACKLISTHEADER
#define FILEBLACKLISTHEADER

int add_to_blacklist(const char * abspath);
char * check_blacklist(struct path p, struct kstat s);
int blacklist_init(void);
int blacklist_shutdown(void);

#endif
