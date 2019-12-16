#define _GNU_SOURCE //memmem
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <time.h>





char * rops[] = {"\x5b\xc3", "\x5b\x5d\xc3", 0};
size_t ropstops[]= {0, 0, 0};

typedef struct roploc_s {
	int ropnum;
	size_t location;
} roploc_t;
typedef struct result_s {
	roploc_t * locs;
	size_t numrops;
	size_t sizerops;
	unsigned int hash;	//for caching
} result_t;



#define MAXHASHBUCKETS 256
//im going to use a hashtable to do a quick check for an exact duplicate
typedef struct hashbucket_s {
	size_t resi;
	struct hashbucket_s * next;
} hashbucket_t;


hashbucket_t ht[MAXHASHBUCKETS] ={0};

static inline unsigned int hash_result(result_t *res){
	unsigned long rethash = 0;
	size_t i;
	for(i = 0; i < res->numrops; i++){
		rethash = rethash * 31 + res->locs[i].location + res->locs[i].ropnum;
	}
	res->hash = rethash;
	return rethash;
}
static inline int compare_results_ez(result_t *a, result_t *b){
	if(a->hash != b->hash) return 3;
	if(a->numrops != b->numrops) return 2;
	size_t i;
	for(i = 0; i < a->numrops; i++) if(a->locs[i].location != b->locs[i].location || a->locs[i].ropnum != b->locs[i].ropnum) return 1;
	return 0;
}
int result_checkandaddtohashtable(result_t *rest, size_t resi){
	if(!rest) return -1;
	if(!rest[resi].hash) hash_result(rest+resi);	//yes its possible to havve a hash of 0... but unlikely
	unsigned int bucket = rest[resi].hash %MAXHASHBUCKETS;
//	printf("Bucket %i\n", bucket);
//	printf("hash %i\n", rest[resi].hash);
	hashbucket_t *hb = &ht[bucket];
	for(;hb->next; hb = hb->next) if(!compare_results_ez(rest+(hb->resi-1), rest+resi)) return 1;
	if(hb->resi){ // need to create a new one
		if(!compare_results_ez(rest+(hb->resi-1), rest+resi)) return 1;
		hb->next = malloc(sizeof(hashbucket_t));
		hb=hb->next;
		hb->next = 0;
	}
//	printf("Bucket %i\n", bucket);
	hb->resi = resi+1;
	return 0;
}
//todo optimize to insert into the first space instead of following LL through
int result_addtohashtable(result_t *rest, size_t resi){
	if(!rest) return -1;
	if(!rest[resi].hash) hash_result(rest+resi);	//yes its possible to havve a hash of 0... but unlikely
	int bucket = rest[resi].hash %MAXHASHBUCKETS;
	hashbucket_t *hb = &ht[bucket];
	for(;hb->next; hb = hb->next);
	if(hb->resi){ // need to create a new one
		hb->next = malloc(sizeof(hashbucket_t));
		hb=hb->next;
		hb->next = 0;
	}
	hb->resi =resi+1;
	return 0;
}

int result_removefromhashtable(result_t *rest, size_t resi){
	if(!rest) return -1;
	if(!rest[resi].hash) hash_result(rest+resi);
	int bucket = rest[resi].hash %MAXHASHBUCKETS;
	hashbucket_t *hbt = 0;
	hashbucket_t *hb = &ht[bucket];
	for(;hb->next; hbt = hb, hb = hb->next) if(hb->resi == resi+1) break;
	if(hb->resi != resi+1) return -2;	//did not find one with us
	hb->resi = 0;
	if(hbt){
		hbt->next = hb->next;
		free(hb); //we arent the first
	} else if(hb->next){
		//we are the first
		hbt = hb->next;
		hb->next = hbt->next;
		hb->resi = hbt->resi;
		free(hbt);
	} else {
		hb->resi = 0;
	}
	return 0;
}
void hashtable_clean(void){
	size_t i;
	hashbucket_t * hb;
	for(i = 0; i < MAXHASHBUCKETS; i++)for(hb = ht[i].next; hb; hb=hb->next)free(hb);
}



int compare_roplocs(const void *a, const void *b){
//	if( ((roploc_t *)a)->ropnum == ((roploc_t *)b)->ropnum )
//		return ((roploc_t *)a)->ropnum - ((roploc_t *)b)->ropnum;
	return ((roploc_t *)a)->location - ((roploc_t *)b)->location;
}

void clean_result_nohash(result_t *res){
//	size_t i;
//	for(i = 0; i < res->numrops; res++){
//		clean_rop(res->locs+i);
//	}
	if(res->locs)free(res->locs);
	res->locs     =0;
	res->numrops  =0;
	res->sizerops =0;
	res->hash =0;
}
void clean_result(result_t *rest, size_t resi){
//	size_t i;
//	for(i = 0; i < res->numrops; res++){
//		clean_rop(res->locs+i);
//	}
	result_t *res = rest+resi;
	result_removefromhashtable(rest, resi);
	if(res->locs)free(res->locs);
	res->locs     =0;
	res->numrops  =0;
	res->sizerops =0;
	res->hash =0;
}



int add_roploc(result_t *res, int ropnum, size_t loc){
	if(!res) return -1;
	if(res->numrops+1 > res->sizerops){
		res->sizerops = (res->numrops+1)*2;
		res->locs = realloc(res->locs, res->sizerops*sizeof(roploc_t));
	}
	res->locs[res->numrops].ropnum = ropnum;
	res->locs[res->numrops].location = loc;
	res->numrops++;
	return 0;
}

inline static void compress_result(result_t *res){
	if(res->numrops){
		res->locs = realloc(res->locs, res->numrops * sizeof(roploc_t));
		res->sizerops = res->numrops;
	} else {
		if(res->locs)free(res->locs);
		res->locs = 0;
		res->numrops = res->sizerops = 0;
	}
}


int print_result(result_t *res){
	if(!res) return -1;
	printf("Result::: %li numrops\n", res->numrops);
	size_t i;
	for(i =0; i < res->numrops; i++){
		printf("\tRop %i at %li\n", res->locs[i].ropnum, res->locs[i].location);
	}
	return 0;
}


int main(int argc, char ** argv){
	struct timespec ts;
	if(timespec_get(&ts, TIME_UTC) ==0){
		srand(time(0));
	} else {
		srand(ts.tv_nsec ^ ts.tv_sec);
	}
	char fifoname[100];
	do {
		sprintf(fifoname, "/tmp/entropyfifo%i", rand());
	} while(access(fifoname, F_OK) != -1);
	mkfifo(fifoname, 0666);
	printf("Using %s as a fifo\n", fifoname);



	char cmdstring[256];
	sprintf(cmdstring, "../elf-mangler -sbrCMj ../elftest/testsuite.o %s", fifoname);
	printf("Using %s as a cmd\n", cmdstring);


	result_t *res = calloc(1000000, sizeof(result_t));

	int i;
	for(i = 0; i < 1000000; i++){
		FILE *d = popen(cmdstring, "r");
		FILE *f = fopen(fifoname, "r");
		if(!f){
			printf("Error opening fifo?\n");
			return 1;
		}



//		printf("we run time\n");
		memset(ropstops, 0, sizeof(ropstops));

	//get a shuffle
		size_t loc = 0;
		int c;
		for(loc = 0;(c=fgetc(f)) != EOF; loc++){
//			printf("%i\n", c);
			int j;
			for(j = 0; rops[j]; j++){
				//todo overlapping junk?
				//spaghettti
				t1:
				if(rops[j][
					ropstops[j]
				] == (char)c){
					ropstops[j]++;
					if(!rops[ropstops[j]]){
						//successfully found a string
						//add it to the list
						add_roploc(&res[i], j, 1+loc-ropstops[j]);
						ropstops[j] = 0;
					}
				} else if(ropstops[j]){
					ropstops[j] = 0;
					goto t1;	//hey, its better than a j-- continue...
				} else ropstops[j] = 0;
			}
		}
//		print_result(&res[i]);
		if(result_checkandaddtohashtable(res, i)){	//its already there
			printf("We got a dupe! (possibly)\n");
			if(res[i].numrops){printf("Dupe:\n"); print_result(&res[i]);}
			clean_result_nohash(res+i);
			i--;
		}
		compress_result(&res[i]);
		//its usually already sorted
		qsort(res[i].locs, res[i].numrops, sizeof(roploc_t), compare_roplocs);

//		if(!(i%100))print_result(&res[i]);
//		cleanup_result(&res);
		pclose(d);
		fclose(f);
	}
	//todo clean?
	remove(fifoname);
	return 0;
}
