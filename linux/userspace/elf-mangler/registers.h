#ifndef REGISTERHEADER
#define REGISTERHEADER

#define HASHTABLESIZE 32
typedef struct registermap_s {
	char * name;
	int shift;
} registermap_t;

extern registermap_t registermap[];

registermap_t *registermap_find(char * name);

void registermap_printregs(uint64_t regs);

int registermap_init(void);
int registermap_clean(void);
#endif
