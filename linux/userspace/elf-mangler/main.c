#define _GNU_SOURCE //needed for assert_perror

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>
#include <getopt.h>

#include "parser.h"
#include "chain.h"

#include "registers.h"	//just to initialize the HT

#include "options.h"
options_t options = {0};

int loadfile( const char* in_fn, uint8_t **out_data, size_t *out_len ) {
	if(!in_fn) return -1;		//error if no input file
	FILE *f = fopen( in_fn, "rb" );
	if( !f ) {
		printf( "Error reading from file %s\n", in_fn );
//todo actually replace this with something
//		assert_perror( errno );
		return errno;
	}
	//figure out how big the file is
	fseek( f, 0, SEEK_END );
	*out_len = ftell( f );
	fseek( f, 0, SEEK_SET );

	*out_data = calloc( *out_len, 1 );
	size_t read_actual = fread( *out_data, 1, *out_len, f );
	fclose( f );
	if( read_actual != *out_len ) {
		printf( "Expected to read %lu bytes, only read %lu\n", *out_len, read_actual );
		goto err_cleanup;
	}
	return 0;

	err_cleanup:
	free( *out_data );
	*out_data = NULL;
	*out_len = 0;
	return -1;
}

void usage(){
	printf("Usage: %s [options] input_file [output_file]\n", options.argv0);
	printf("\t-s, --shuffle\t\t\tShuffle the sections\n");
	printf("\t-b, --bumpsize SIZE\t\tAdd hlt junk onto the end of most sections, default max size is %i\n", DEFAULT_BUMPSIZE);
	printf("\t-c, --cruftsize SIZE\t\tEnable inserting crufts into executable sections, default max size is %i\n", DEFAULT_CRUFTSIZE);
	printf("\t-C, --super-cruftables\t\tEnable super-cruftables (implies -c)\n");
	printf("\t-m, --split-cruftables\t\tEnable the split cruftable method\n");
	printf("\t-M, --confused-cruftables\tEnable the confused cruftable method\n");
	printf("\t-S, --shimsize SIZE\t\tInsert stack shims, default size is %i\n", DEFAULT_SHIMSIZE);
	printf("\t-D, --dynshimsize SIZE\t\tInsert dynamic shims (can be used with or without -S), default size is %i\n", DEFAULT_DYNSHIMSIZE);
	printf("\t-p, --print\t\t\tPrint debugging ESIL output\n");
	printf("\t-r, --reorder-stack\t\tEnable re-ordering the entry and leaves of functions\n");
	//todo this should be enabled by default
	printf("\t-j, --no-jumptable-hack\t\tDisable the system that fixes issues with relative jump tables\n");
	printf("\t-I, --information\t\tPrint information about symbol locations post-modification\n");
	printf("\t-?, --help\t\t\tPrint usage (this)\n");
	printf("\t--cruftprobability PROBABILITY\tprobability at which to insert cruftables per section\n");
	printf("\t--splitprobability PROBABILITY\tprobability at which to split a cruftable (if -m or -M enabled)\n");
	printf("\t--shimalign ALIGNMENT\t\tnumber of bytes to align a shim to. Default is 16. Use 1 for no alignment. Required for functions that use certian SIMD instructions\n");
	printf("\t--seed SEED\t\t\tUse SEED instead of a time based one\n");
}

int main( int argc, char *argv[] ) {
	int c;
	options.argv0 = argv[0];
	options.cruftprobability = DEFAULT_CRUFTPROBABILITY;
	options.splitprobability = DEFAULT_SPLITPROBABILITY;
	options.shimalign = DEFAULT_SHIMALIGN;
	options.jumptablehack = 1;

	int option_index = 0;

	static struct option long_options[] = {
		{"bumpsize",		required_argument, 0, 0},
		{"cruftsize",		required_argument, 0, 0},
		{"shimsize",		required_argument, 0, 0},

		{"cruftprobability",	required_argument, 0, 0},
		{"splitprobability",	required_argument, 0, 0},
		{"shimalign",		required_argument, 0, 0},
		{"seed",		required_argument, 0, 0},
		{"dynshimsize",		required_argument, 0, 0},

		{"analyze",		no_argument,	   0, 'a'},
		{"shuffle",		no_argument,	   0, 's'},
		{"super-cruftables",	no_argument,	   0, 'C'},
		{"split-cruftables",	no_argument,	   0, 'm'},
		{"confused-cruftables",	no_argument,	   0, 'M'},
		{"print",		no_argument,	   0, 'p'},
		{"reorder-stack",	no_argument,	   0, 'r'},
		{"no-jumptable-hack",	no_argument,	   0, 'j'},
		{"information",		no_argument,	   0, 'I'},
		{"help",		no_argument,	   0, '?'},
		{0, 0, 0, 0}
	};

	while(( c = getopt_long(argc, argv, "asbcCmMprjISD", long_options, &option_index)) != -1){
		switch(c){
			case 0:
				if(long_options[option_index].flag != 0)break;
				switch(option_index){
					case 0:
						options.bumpsize = atol(optarg);
					break;
					case 1:
						options.cruftsize = atol(optarg);
					break;
					case 2:
						options.shimsize = atol(optarg);
					break;
					case 3:
						options.cruftprobability = atof(optarg);
					break;
					case 4:
						options.splitprobability = atof(optarg);
					break;
					case 5:
						options.shimalign = atol(optarg);
					break;
					case 6:
						options.seed = atol(optarg);
						options.useseed = 1;
					break;
					case 7:
						options.dynshimsize = atol(optarg);
					break;
					default:
					break;
				}

			break;
			case 'a':
				options.analyze = 1;
			break;
			case 's':
				options.shuffle = 1;
			break;
			case 'b':
				if(!options.bumpsize) options.bumpsize = DEFAULT_BUMPSIZE;
			break;
			case 'C':
				options.supercruftables = 1;
			case 'c':
				if(!options.cruftsize) options.cruftsize = DEFAULT_CRUFTSIZE;
			break;
			case 'm':
				if(options.cruftmethod<1) options.cruftmethod = 1;
			break;
			case 'M':
				if(options.cruftmethod<2) options.cruftmethod = 2;
			break;
			case 'p':
				options.printcode = 1;
			break;
			case 'r':
				options.stackshuffle = 1;
			break;
			case 'j':
				options.jumptablehack = 0;
				printf("disabling jumptablehack\n");
			break;
			case 'S':
				if(!options.shimsize)options.shimsize = DEFAULT_SHIMSIZE;
			break;
			case 'D':
				if(!options.dynshimsize)options.dynshimsize = DEFAULT_DYNSHIMSIZE;
			break;
			case 'I':
				options.printsymbols = 1;
			break;
			default :
			case '?':
				usage();
				exit(1);
			break;
		}
	}
	if(optind < argc){
		options.src = argv[optind];
	}
	optind++;
	if(optind < argc){
		options.dest = argv[optind];
	}
	if(options.cruftsize || options.printcode || options.stackshuffle || options.shimsize) options.analyze = 1;
	if(options.cruftprobability > CRUFT_MAXPROB){
		printf("Cruftable probability %f is too big!, setting to max %f\n", options.cruftprobability, CRUFT_MAXPROB);
		options.cruftprobability = CRUFT_MAXPROB;
	}
	if(options.splitprobability > SPLIT_MAXPROB){
		printf("Split probability %f is too big!, setting to max %f\n", options.cruftprobability, SPLIT_MAXPROB);
		options.splitprobability = SPLIT_MAXPROB;
	}
	if(!options.shimalign){
		printf("WARNING: Shimalign set to 0, adjusting to 1 (what you probably meant)\n");
		options.shimalign = 1;
	}
	if(!options.src){
		printf("ERROR: no input file specified\n");
		usage();
		exit(1);
	}
	int err;
	run_tests();

	if(!options.useseed){
		//BOO. If someone was able to "guess" the approximate time the file was shuffled, they would be able to re-generate it
		//srand( time( NULL ) );

		//better method, says https://wiki.sei.cmu.edu/confluence/display/c/MSC32-C.+Properly+seed+pseudorandom+number+generators

		struct timespec ts;
		if(timespec_get(&ts, TIME_UTC) == 0){
			//error, revert to time
			options.seed = time(0);
			printf("warning, low time res seed\n");
		} else {
			options.seed = (ts.tv_nsec ^ ts.tv_sec);
		}
	}
	printf("Using seed %lli\n", options.seed);
	srand(options.seed);


	registermap_init();

	uint8_t* file_data = NULL;
	size_t file_len;
	struct elf_file_metadata elf;
	struct elf_file_metadata new_elf;
//#define BENCHMARK
#ifdef BENCHMARK
	uint8_t *file_orig_data = 0;
	loadfile (options.src, &file_orig_data, &file_len);
	file_data = malloc(file_len);

	struct timeval tv1, tv2;
	gettimeofday(&tv1, NULL);
	int i;
	for(i = 0; i < 1000; i++){
		memcpy(file_data, file_orig_data, file_len);
		parse_elf(file_data, file_len, &elf);
		rejigger_elf(&elf, &new_elf);
		free_elf(&new_elf);
	}

	free_elf(&elf);
	gettimeofday(&tv2, NULL);

	fprintf(stderr, "Total time = %f seconds for 1000 itterations\n", (double)(tv2.tv_usec - tv1.tv_usec) / 1000000 + (double)(tv2.tv_sec - tv1.tv_sec));
	err= 0;
#else
	CHAIN( err,
		loadfile( options.src, &file_data, &file_len ),
		parse_elf( file_data, file_len, &elf ),
		rejigger_elf( &elf, &new_elf ),
		write_elf( &new_elf, options.dest ),
		free_elf( &new_elf ),
		free_elf( &elf ),
	);
#endif


	//really dont need this because the program ends here, and it will be freed (more efficiently) on exit
	registermap_clean();
	printf("Done! Used seed %llu\n", options.seed);
	if( err ) return err;
	return 0;
}
