#ifndef OPTIONSHEADER
#define OPTIONSHEADER

#define DEFAULT_BUMPSIZE		1024
#define DEFAULT_CRUFTSIZE		1024
#define DEFAULT_CRUFTPROBABILITY	0.9
#define DEFAULT_SPLITPROBABILITY	0.5
#define DEFAULT_SHIMSIZE		1024 //thats really big!
#define DEFAULT_DYNSHIMSIZE		256
#define DEFAULT_SHIMALIGN		16 //for compat with sse instructions that are aligned... TODO autodetect

#define CRUFT_MAXPROB	0.9999d
#define SPLIT_MAXPROB	0.9999d


typedef struct options_s {
	char *argv0;
	char * src;
	char * dest;
	char shuffle;
	char supercruftables;
	char cruftmethod;
	char printcode;
	char analyze;
	char stackshuffle;
	char jumptablehack;
//	char verbose;		//todo
	char printsymbols;

//	int maxcruftables;	//todo?
	size_t shimsize;
	size_t cruftsize;
	size_t bumpsize;
	size_t shimalign;
	size_t dynshimsize;

	long long unsigned int seed;
	char useseed;

	double cruftprobability;
	double splitprobability;
//todo option for setting random seed
} options_t;

options_t options;	//in main

#endif
