#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "registers.h"


registermap_t registermap[] = {

	//rax
	{"rax",	0},
	{"eax",	0},
	{"ax",	0},
	{"al",	0},
	{"ah",	0},
	//ebx
	{"rbx",	1},
	{"ebx",	1},
	{"bx",	1},
	{"bl",	1},
	{"bh",	1},
	//ecx
	{"rcx",	2},
	{"ecx",	2},
	{"cx",	2},
	{"cl",	2},
	{"ch",	2},
	//edx
	{"rdx",	3},
	{"edx",	3},
	{"dx",	3},
	{"dl",	3},
	{"dh",	3},
	//rbp
	{"rbp",	4},
	{"ebp",	4},
	{"bp",	4},
	{"bpl",	4},
	//rsi
	{"rsi",	5},
	{"esi",	5},
	{"si",	5},
	{"sil",	5},
	//rdi
	{"rdi",	6},
	{"edi",	6},
	{"di",	6},
	{"dil",	6},
	//rsp
	{"rsp",	7},
	{"esp",	7},
	{"sp",	7},
	{"spl",	7},
	//r8
	{"r8",	8},
	{"r8d",	8},
	{"r8w",	8},
	{"r8b",	8},
	//r9
	{"r9",	9},
	{"r9d",	9},
	{"r9w",	9},
	{"r9b",	9},
	//r10
	{"r10",	10},
	{"r10d",10},
	{"r10w",10},
	{"r10b",10},
	//r11
	{"r11",	11},
	{"r11d",11},
	{"r11w",11},
	{"r11b",11},
	//r12
	{"r12",	12},
	{"r12d",12},
	{"r12w",12},
	{"r12b",12},
	//r13
	{"r13",	13},
	{"r13d",13},
	{"r13w",13},
	{"r13b",13},
	//r14
	{"r14",	14},
	{"r14d",14},
	{"r14w",14},
	{"r14b",14},
	//r15
	{"r15",	15},
	{"r15d",15},
	{"r15w",15},
	{"r15b",15},


//TODO all the float ones, flag ones, etc
	//the xmms
	{"xmm0",16},
	{"xmm1",17},
	{"xmm2",18},
	{"xmm3",19},
	{"xmm4",20},
	{"xmm5",21},
	{"xmm6",22},
	{"xmm7",23},
	//flags (all one register, but can be tracked one by one)
	{"cs",	24},
	{"ds",	25},
	{"ss",	26},
	{"es",	27},
	{"fs",	28},
	{"gs",	29},
	//todo rest of eflags?
	//status flags
	{"cf", 30},
	{"pf", 31},
	{"af", 32},
	{"zf", 33},
	{"sf", 34},
	{"of", 35},

	//todo fpu, mmx, bounds, ymm, mxcsr

	//rip
	{"rip",	63},
	{"eip",	63},
	{"ip",	63},
	{"ipl",	63},	//does this even exist?
	{0}
};

unsigned int hash_string(char * st){
	unsigned int rethash = 0;
	for(;*st; st++) rethash = rethash*31 + *st;
	return rethash;
}
unsigned int hash_string_case(char * st){
	unsigned int rethash = 0;
	for(;*st; st++) rethash = rethash*31 + (*st & 0b11011111);	//hash it as lowercase
	return rethash;
}

typedef struct hashbucket_s {
	registermap_t *reg;
	unsigned int fullhash;
	struct hashbucket_s * next;
} hashbucket_t;


hashbucket_t ht[HASHTABLESIZE] = {0};
int register_addtohashmap(registermap_t *rm){
	//get hash
	unsigned int hash = hash_string_case(rm->name);
	int bucket = hash % HASHTABLESIZE;
	hashbucket_t *hb = &ht[bucket];
	//is first bucket empty?
	if(!hb->reg){
		hb->reg = rm;
		hb->fullhash = hash;
		return 0;
	}
	//insert
	hashbucket_t *nhb = malloc(sizeof(hashbucket_t));
	nhb->reg = rm;
	nhb->fullhash = hash;
	nhb->next = hb->next;
	hb->next = nhb;
	return 0;
}

int registermap_init(void){
	registermap_t *rm;
	for(rm = registermap; rm->name; rm++){
		register_addtohashmap(rm);
	}
	return 0;
}
int registermap_clean(void){
	size_t i;
	for(i = 0; i < HASHTABLESIZE; i++){
		hashbucket_t *hb, *ohb;
		for(hb = ht[i].next; (ohb=hb);){
			hb=hb->next;
			free(ohb);
		}
	}
	return 0;
}

registermap_t * registermap_find(char * name){
	unsigned int hash = hash_string_case(name);
	int bucket = hash%HASHTABLESIZE;
	hashbucket_t *hb = &ht[bucket];
	for(; hb; hb = hb->next){
		if(hb->fullhash != hash) continue;
		if(!hb->reg || strcasecmp(name, hb->reg->name))continue;
		//we found it
		return hb->reg;
	}
	return 0;//found nothing
}


void registermap_printregs(uint64_t regs){
	uint64_t modregs = regs;
	registermap_t *r;
	for(r = registermap; r->name; r++) if((uint64_t)1<<r->shift & modregs){
		modregs &= ~(1 << r->shift);
		printf("%s ", r->name);
	}
}
