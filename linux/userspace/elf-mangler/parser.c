#define _GNU_SOURCE	//for assert_perrror

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h> //for memcpy


#include "parser.h"

#include "analyze.h"
#include "cruftable.h"
#include "chain.h"
#include "stackshuffle.h"
#include "stackshim.h"
#include "fpatch.h"

#include "options.h"

void run_tests( void ) {
	#ifndef NODEBUG
	assert( sizeof( struct elf_prog_header_64 ) == 0x38 );
	assert( sizeof( struct elf_section_header_64 ) == 0x40 );
	assert( sizeof( struct elf_symtab_entry ) == 24 );
	printf( "All tests passed\n" );
	#endif
}

static int is_elf64( const struct elf_header *elf ) {
	return elf->bitmode == 2
	    && elf->machine == 0x3e
	    && sizeof( *elf ) == 64;
}

static int is_good_elf( const struct elf_header *elf ) {
	return elf->magic[0] == 0x7f
	    && elf->magic[1] == 'E'
	    && elf->magic[2] == 'L'
	    && elf->magic[3] == 'F'
	    && elf->endianness == 1
	    && elf->ident_version == 1
	    ;
}

static int is_object_elf( const struct elf_header *elf ) {
	return elf->phnum == 0;
}

static int check_proghdrsz( const struct elf_file_metadata *elf ) {
	struct elf_header *hdr = elf->hdr;
	if( elf->total_sz < (unsigned long)(hdr->phoff) + (hdr->phentsize*hdr->phnum) ) {
		printf( "Elf file is too smal for number of ph entries\n" );
		return -2;
	}
	return 0;
}
static int check_proghdrentries( const struct elf_file_metadata *elf ) {
	for( size_t i = 0; i < elf->numproghdrs; i++ ) {
		struct elf_prog_header_64 tmp = elf->proghdrs[i];
		if( elf->total_sz < tmp.offset + tmp.filesz ) { //should this be memsz?, probably not
			printf( "Elf file is to small for program header table to be correct\n" );
			return -1;
		}
	}
	return 0;
}
static int check_sechdrsz( const struct elf_file_metadata *elf ) {
	struct elf_header *hdr = elf->hdr;
	if( elf->total_sz < (unsigned long)(hdr->shoff) + (hdr->shentsize*hdr->shnum) ) {
		printf( "Elf file is too small for number of sh entries\n" );
		return -2;
	}
	return 0;
}
static int check_sechdrentries( const struct elf_file_metadata *elf ) {
	for( size_t i = 0; i < elf->numsechdrs; i++ ) {
		struct elf_section_header_64 tmp = elf->sechdrs[i];
		if( elf->total_sz < tmp.offset + tmp.size ) {
			printf( "Elf file is to small for program header table to be correct\n" );
			return -1;
		}
	}
	return 0;
}

int parse_elf( uint8_t *in_data, size_t in_len, struct elf_file_metadata *out_parsed ) {
	struct elf_header *hdr = (struct elf_header*)in_data;
	int err;
	ANDCHAIN( err,
		is_elf64( hdr ),
		is_good_elf( hdr ),
		is_object_elf( hdr ),
	);
	if(!err) {
		printf( "Bad object file\n" );
		return -1;
	}
	//this should all be safe
	out_parsed->base_ptr = in_data;
	out_parsed->total_sz = in_len;
	out_parsed->hdr = (struct elf_header*)(in_data);
	out_parsed->numproghdrs = hdr->phnum;
	out_parsed->proghdrs= (struct elf_prog_header_64*)(in_data + (unsigned long)(hdr->phoff));
	out_parsed->numsechdrs = hdr->shnum;
	out_parsed->sechdrs= (struct elf_section_header_64*)(in_data + (unsigned long)(hdr->shoff));
	out_parsed->changed = 0;
	out_parsed->metas = 0;
	// check file size to make sure everything fits
	CHAIN( err,
		check_proghdrsz( out_parsed ),
		check_proghdrentries( out_parsed),
		check_sechdrsz( out_parsed ),
		check_sechdrentries( out_parsed),
		printf_chainable( "Got %lu program table entries\n", out_parsed->numproghdrs),
		printf_chainable( "Got %lu section table entries\n", out_parsed->numsechdrs)
	);
	if( err ) {
		return err;
	}
	return 0;
}
static int valid_doublemap( size_t *m1, size_t *m2, size_t sz ) {
	for( size_t i = 0; i < sz; i++ ) {
		size_t tmp = m1[i];
		assert( tmp < sz );
		assert( m2[tmp] == i );
	}
	return 1;
}


int get_num(const struct elf_file_metadata *elf, struct elf_section_header_64 * in_sec){
	return (int)(((void*)in_sec - (void*)elf->sechdrs) / sizeof(struct elf_section_header_64));
}


int get_section( const struct elf_file_metadata *elf, size_t secnum, struct elf_section_header_64 **out_sec ) {
	if( secnum >= elf->numsechdrs ) {
		return -ENOENT;
	}
	*out_sec = &(elf->sechdrs[secnum]);
	return 0;
}

int get_section_name( const struct elf_file_metadata *elf, size_t secnum, char **out_name ) {
	int err;
	struct elf_section_header_64 *shstrtab;
	struct elf_section_header_64 *tmp;
	CHAIN( err,
		get_section( elf, elf->hdr->shstridx, &shstrtab ),
		get_section( elf, secnum, &tmp )
	);
	if( err ) return err;

	char* names = elf->base_ptr + shstrtab->offset;
	*out_name = names + elf->sechdrs[secnum].name;
	return 0;
}



int rejigger_elf( struct elf_file_metadata *in_elf, struct elf_file_metadata *out_elf ) {
	int err = 0;
	struct elf_section_header_64 *tmpsec;
//	size_t bytes_exec = 0, bytes_nonexec = 0;
	size_t section_bytes = 0;
	//size_t num_exec, num_nonexec = 0;

	//zeroing the output struct
	memset( out_elf, 0, sizeof( *out_elf ) );

	size_t * bmp_size_list = 0;
	if(options.bumpsize) bmp_size_list = calloc(in_elf->numsechdrs, sizeof(size_t));

	if(!in_elf->metas) in_elf->metas = calloc(in_elf->numsechdrs, sizeof(analyze_section_metadata_t));
	//initialize the metas, maybe print
	for( size_t i = 0; i < in_elf->numsechdrs; i++ ) {
		CHAIN( err,
			get_section( in_elf, i, &tmpsec ),
		);
		if( err ) return err;

		//todo we are changing this up.....
		//TODO fix secname... maybe STRDUP
		if(!in_elf->metas[i].elf) in_elf->metas[i] = analyze_meta(i, in_elf);
		//we replace notes with SHT_NULL sections, so the byte count will be 0
		if( tmpsec->type == SHT_NOTE || !(tmpsec->flags & SHF_EXECINSTR)) {
			continue;
		}
		//initial analyze?
		if(options.analyze){
			printf("analyzing %li aka %s\n", i, in_elf->metas[i].section_name);
			analyze(&in_elf->metas[i]);
		}
		if(options.printcode){
			printf("Printing %li aka %s\n", i, in_elf->metas[i].section_name);
			print_code(&in_elf->metas[i]);
		}
	}
	//do stack junk
	for( size_t i = 0; i < in_elf->numsechdrs; i++){
		CHAIN( err,
			get_section( in_elf, i, &tmpsec ),
		);
		if( err ) return err;
		if( tmpsec->type == SHT_NOTE || !(tmpsec->flags & SHF_EXECINSTR)) {
			continue;
		}
		if(options.shimsize){
			printf("Calculating and inserting stack shims for %li aka %s\n", i, in_elf->metas[i].section_name);
			if(!in_elf->metas[i].isanalyzed){
				clean_metadata_notreally(&in_elf->metas[i]);
				printf("analyzing %li aka %s\n", i, in_elf->metas[i].section_name);
				analyze(&in_elf->metas[i]);
			}
			//todo adjust so it dynamically adjusts the analysis, so i dont have to re-analyze it
			applystackshim(&in_elf->metas[i]);
			apply_and_fixup_patches(in_elf, in_elf->metas);	///uh... gonna lazy do this for testing
		}
		if(options.stackshuffle){
			printf("Calculating stack reorder for %li aka %s\n", i, in_elf->metas[i].section_name);
			if(!in_elf->metas[i].isanalyzed){
				clean_metadata_notreally(&in_elf->metas[i]);
				printf("analyzing %li aka %s\n", i, in_elf->metas[i].section_name);
				analyze(&in_elf->metas[i]);
			}
			calcentireshuffle(&in_elf->metas[i]);
			applyshuffle(&in_elf->metas[i]);
		}
	}


	//do dyn shims
	if(options.dynshimsize){
		for( size_t i = 0; i < in_elf->numsechdrs; i++){
			CHAIN( err,
				get_section( in_elf, i, &tmpsec ),
			);
			if( err ) return err;
			if( tmpsec->type == SHT_NOTE || !(tmpsec->flags & SHF_EXECINSTR)) {
				continue;
			}
			//have to re-analyze?
			if(!in_elf->metas[i].isanalyzed){
				clean_metadata_notreally(&in_elf->metas[i]);
				printf("analyzing %li aka %s\n", i, in_elf->metas[i].section_name);
				analyze(&in_elf->metas[i]);
			}
//			size_t res = generate_cruftables_wrapper(&in_elf->metas[i]);
			generate_dynstackshim(&in_elf->metas[i]);
			printf("stack shims dyn\n");
		}
		apply_and_fixup_patches(in_elf, in_elf->metas);	///uh... gonna lazy do this for testing
	}

	//do crufts
	if(options.cruftsize){
		for( size_t i = 0; i < in_elf->numsechdrs; i++){
			CHAIN( err,
				get_section( in_elf, i, &tmpsec ),
			);
			if( err ) return err;
			if( tmpsec->type == SHT_NOTE || !(tmpsec->flags & SHF_EXECINSTR)) {
				continue;
			}
			//have to re-analyze?
			if(!in_elf->metas[i].isanalyzed){
				clean_metadata_notreally(&in_elf->metas[i]);
				printf("analyzing %li aka %s\n", i, in_elf->metas[i].section_name);
				analyze(&in_elf->metas[i]);
			}
			size_t res = generate_cruftables_wrapper(&in_elf->metas[i]);
			printf("\tgenerated %li crufts (patch)\n", res);
		}
		apply_and_fixup_patches(in_elf, in_elf->metas);	///uh... gonna lazy do this for testing
	}


	//grab the final total size, also compute a bumpsize
	for( size_t i = 0; i < in_elf->numsechdrs; i++ ) {
		CHAIN( err,
			get_section( in_elf, i, &tmpsec ),
		);
		if( err ) return err;
		if(in_elf->metas[i].data){
			section_bytes += in_elf->metas[i].datasize;
		} else section_bytes += tmpsec->size;

		section_bytes += tmpsec->addralign; // just to be safe

		if(bmp_size_list){
			if(tmpsec->type != SHT_RELA && tmpsec->type != SHT_REL && tmpsec->type != SHT_STRTAB && tmpsec->type != SHT_SYMTAB){
				//lousy hack to make sure it isnt .eh_frame
				char * secname = 0;
				get_section_name(in_elf, i, &secname);
				//the '.' checks for anything kernel related, such as __ksymtab (shows up as progbits?)

				if(secname[0] == '.' && strcmp(secname, ".eh_frame")
					&& strncmp(secname, ".orc_", 5)
					&& strcmp(secname, ".parainstructions")
					&& strcmp(secname, ".rela.parainstructions")
					&& strcmp(secname, ".altinstructions")
					&& strcmp(secname, ".rela.altinstructions")
				){
					//update size to include bmp_size
					bmp_size_list[i] = rand()%options.bumpsize;
				}
			}
			section_bytes += bmp_size_list[i];
		}

	}

	//Now, we will fill in the output struct
	out_elf->numsechdrs = in_elf->numsechdrs;
	size_t shsize = (out_elf->numsechdrs) * (sizeof ( struct elf_section_header_64) );
	out_elf->total_sz = sizeof( struct elf_header ) + shsize + section_bytes;


	printf("totasize %li\n", out_elf->total_sz);

	out_elf->base_ptr = calloc( 1, out_elf->total_sz );
	out_elf->hdr = out_elf->base_ptr;
	size_t writeoffset = sizeof(struct elf_header);
	//copy over the section headers
	out_elf->sechdrs = out_elf->base_ptr + writeoffset;
	writeoffset += shsize;

	// copy the header data, most won't change
	memcpy( out_elf->hdr, in_elf->hdr, sizeof( struct elf_header ) );
	out_elf->hdr->shoff = (void*)((void*)out_elf->sechdrs - out_elf->base_ptr);

	out_elf->hdr->shnum = out_elf->numsechdrs;


//i dont use this yet
//	out_elf->hdr->phnum = out_elf->numproghdrs;

	//mapping from out_elf sections to in_elf sections.
	// There will always be the same number of each
	size_t *shuffle_arr = calloc( sizeof( *shuffle_arr), in_elf->numsechdrs );
	//and the reverse
	size_t *shuffle_arr_back = calloc( sizeof( *shuffle_arr_back), in_elf->numsechdrs );

	//build an array with [0,1,2,...,in_elf->numsechdrs]
	for( size_t i = 0; i < in_elf->numsechdrs; i++ ) {
		shuffle_arr[i] = i;
		shuffle_arr_back[i] = i;
	}

	if(options.shuffle){
		//use a simple fisher-yates to shuffle the array. Leave arr[0]=0
		for( size_t i = 1; i < in_elf->numsechdrs; i++ ) {
			size_t swap,swap_idx;
			swap_idx = (rand() % (in_elf->numsechdrs - i)) + i;
			assert( swap_idx < in_elf->numsechdrs );
			assert( swap_idx >= i );
			swap = shuffle_arr[swap_idx];
			shuffle_arr[swap_idx] = shuffle_arr[i];
			shuffle_arr[i] = swap;
			shuffle_arr_back[swap] = i; //build the backwards map at the same time
		}
	}
	assert( valid_doublemap( shuffle_arr, shuffle_arr_back, in_elf->numsechdrs ) );
	//I don't even think we need this
	assert( valid_doublemap( shuffle_arr_back, shuffle_arr, in_elf->numsechdrs ) );

	// we can fill this in right away
	out_elf->hdr->shstridx = shuffle_arr_back[in_elf->hdr->shstridx];


	// again, we start at 1, since the SHT_NULL section should already be zeroed
	for( size_t i = 1; i < in_elf->numsechdrs; i++ ) {
		size_t shufsec = shuffle_arr[i];
		err = get_section( in_elf, shufsec, &tmpsec);
		if( err ) goto cleanup;
		if( tmpsec->type == SHT_NOTE ) continue;



		char * nm = 0;
		get_section_name(in_elf, shufsec, &nm);
//		printf("doing final shuffle-copy on section %li->%li aka %s\n", shufsec, i, nm);


		//todo one one offset?
//		size_t* offset_ptr = ( tmpsec->flags & SHF_EXECINSTR ) ? &exec_writeoffset : &nonexec_writeoffset;
		size_t* offset_ptr = &writeoffset;

		// if it needs to be aligned, align it with some bit hacking
		// ex. to align by 8, add 7 and zero the 3 lsbs
		if( tmpsec->addralign != 0 ) {
			*offset_ptr += tmpsec->addralign-1;
			*offset_ptr &= ~(tmpsec->addralign-1);
		}


		struct elf_section_header_64* newsec = &(out_elf->sechdrs[i]);
		assert( sizeof(*newsec) == sizeof( *tmpsec ) );
		// copy the section header
		memcpy( newsec, tmpsec, sizeof( *tmpsec ) );

		newsec->offset = *offset_ptr;


		// copy the section (not the section header) from in to out
//			printf("copy section number %li->%li aka %s\n", shufsec, i, nm);
		if(in_elf->metas[shufsec].data){
			newsec->size = in_elf->metas[shufsec].datasize;
			memcpy(out_elf->base_ptr + newsec->offset, in_elf->metas[shufsec].data, newsec->size);
		} else {
			memcpy( out_elf->base_ptr + newsec->offset, in_elf->base_ptr + tmpsec->offset, newsec->size );
		}

		if(bmp_size_list){
			size_t bmp_size = bmp_size_list[shuffle_arr[i]];
			//also put a bunch of HLT instructions after it, up to bmp_size
			memset(out_elf->base_ptr + newsec->offset + newsec->size, BMP_HLT_BYTE, bmp_size);
			newsec->size +=bmp_size;	//adjust size of section to match the bump (if applicable)
		}

		//re-do the rel and sym section indexes
		if(options.shuffle){
			switch( newsec->type ) {
			//if this is a RELA entry, we need update sh_link and sh_info
			case SHT_REL:
			case SHT_RELA:
				newsec->link = shuffle_arr_back[tmpsec->link];
				newsec->info = shuffle_arr_back[tmpsec->info];
				break;
			// each entry in the symbol table needs to be updated
			case SHT_SYMTAB:
				newsec->link = shuffle_arr_back[newsec->link];
				assert( (newsec->size) % sizeof( struct elf_symtab_entry ) == 0 );
				// for each entry, change the shidx
				struct elf_symtab_entry *symtab = (struct elf_symtab_entry*)(out_elf->base_ptr + newsec->offset);
				for( size_t j = 0; j < tmpsec->size/(sizeof (struct elf_symtab_entry)); j++ ) {
					struct elf_symtab_entry* tmp = &(symtab[j]);
					if( tmp->shidx == 0 || (tmp->shidx >= SHN_LORESERVE && tmp->shidx <= SHN_HIRESERVE ) ) {
						(void)0; // do nothing
					} else {
						assert( tmp->shidx < in_elf->numsechdrs );
						tmp->shidx = shuffle_arr_back[tmp->shidx]; //map from in_elf to out_elf
					}
				}
				break;
			default:
				break;
			}
		}
		newsec->offset = *offset_ptr;
		//i change newsec->size when i copy it
		*offset_ptr += newsec->size;
	}

	if(options.printsymbols){
		//todo move this to a more general area/function
		//find the strtable
		size_t i;
		printf("Symbol locations\n");
		for(i = 0; i < out_elf->numsechdrs; i++){
			struct elf_section_header_64 * sec = 0;
			get_section(out_elf, i, &sec);
			if(sec->type != SHT_SYMTAB) continue;

			struct elf_section_header_64 * strtab = 0;
			int err = get_section(out_elf, sec->link, &strtab);
			if(err) continue;


			char * nm = 0;
			get_section_name(out_elf, i, &nm);
			printf("\tSymbol locations for %s\n", nm);
			struct elf_symtab_entry *sym = (struct elf_symtab_entry*)(out_elf->base_ptr + sec->offset);
			size_t count = sec->size / sizeof(struct elf_symtab_entry);
			size_t j;
			for(j = 0; j < count; j++, sym++){
				if(sym->shidx < 0 || sym->shidx >= out_elf->numsechdrs) continue;
				//grab my name
				char * name = "NONAME";
				if(strtab && sym->name < strtab->size && *(char*)(out_elf->base_ptr + strtab->offset + sym->name)) name = out_elf->base_ptr + strtab->offset + sym->name;
				struct elf_section_header_64 * dec;
				get_section(out_elf, sym->shidx, &dec);
				//location is sym->value + dec->base_ptr
				printf("\t%s: %li\n", name, sym->value+dec->offset);
			}
		}
	}
	cleanup:
		if(bmp_size_list)	free( bmp_size_list );
		if(shuffle_arr) 	free( shuffle_arr );
		if(shuffle_arr_back)	free( shuffle_arr_back );
		analyze_shutdown();
		if( err ) {
			free( out_elf->base_ptr );
			out_elf->base_ptr = NULL;
		}
		return err;
}

int write_elf( struct elf_file_metadata *elf, char* fn ) {
	if(!fn) return 0;	//gracefully ignore null output files (usually because an output file was not specified)
	FILE *f = fopen( fn, "wb" );
	if( f ) {
		fwrite( elf->base_ptr, 1, elf->total_sz, f);
		int err = fclose( f );
		return err;
	}
	printf("Error writing to file %s\n", fn);
	//todo actually replace this with something
//	assert_perror(errno);
	return errno;
}
int free_elf( struct elf_file_metadata *elf ) {
	if(elf->metas){
		for(size_t i = 0; i < elf->numsechdrs; i++)clean_metadata(&elf->metas[i]);
		free(elf->metas);
	}
	if(elf->changed & ELF_METADATA_HDR_CHANGED){
		free(elf->hdr);
	}
	if(elf->changed & ELF_METADATA_PROG_CHANGED){
		free(elf->proghdrs);
	}
	if(elf->changed & ELF_METADATA_SEC_CHANGED){
		free(elf->sechdrs);
	}
	free(elf->base_ptr );
	elf->base_ptr = NULL;
	return 0;
}
