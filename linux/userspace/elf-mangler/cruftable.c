#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>

#include "parser.h"
#include "analyze.h"

#include "cruftable.h"


#include "limits.h"
#include "options.h"


#include "fpatch.h"



char *confusedshortopcodes[] = {
"\x70", "\x71", "\x72", "\x73",
"\x74", "\x75", "\x76", "\x77",
"\x78", "\x79", "\x7A", "\x7B",
"\x7C", "\x7D", "\x7E", "\x7F",
/*"\xE3"*/ // JECXZ, we dont want o use that
};

char *confusednearopcodes[] = {
"\x0f\x80","\x0f\x81","\x0f\x82","\x0f\x83",
"\x0f\x84","\x0f\x85","\x0f\x86","\x0f\x87",
"\x0f\x88","\x0f\x89","\x0f\x8A","\x0f\x8B",
"\x0f\x8C","\x0f\x8D","\x0f\x8E","\x0f\x8F"
};

//modified split_cruftable
void write_confused_cruftable(void * out, size_t sz){
	if(!sz) return;
	if(sz > 1 && (double)rand()/(double)RAND_MAX < options.splitprobability){	//do i split?
		size_t splitpnt = rand()%sz;
		write_confused_cruftable(out, splitpnt);
		write_confused_cruftable(out+splitpnt, sz-splitpnt);
	} else {
		if(rand()%2){	//write a confused one
			if(sz < 3){			//only have enough room for nops
				memset(out, 0x90, sz);
			} else if(sz-2 < SCHAR_MAX){	//jump relative 8
				int op = rand()%(sizeof(confusedshortopcodes)/sizeof(char*));
				((char*)out)[0] = confusedshortopcodes[op][0];
				char bmpmax = sz-2;
				((char*)(out+1))[0] = bmpmax;
				write_confused_cruftable(out+2, sz-2);
			} else if(sz-6 < INT_MAX){	//jump relative 32 (16 is the same opcode... so its really 32)
				//choose random opcode
				int op = rand()%(sizeof(confusednearopcodes)/sizeof(char*));
				((char*)out)[0] = confusednearopcodes[op][0];
				((char*)out)[1] = confusednearopcodes[op][1];
				int bmpmax = sz-6;
				((int*)(out+2))[0] = bmpmax; // endian agnostic as long as this is being run on the same endianness as the object file is messing with
				write_confused_cruftable(out+6, sz-6);

			}
		} else {	//write normal cruftable out
			if(sz < 3){			//only have enough room for nops
				// -fno-optimize-sibling-calls will "fix"
				memset(out, 0x90, sz);	//This line right here + an optimization from GCC breaks selfhosting with -r. GCC will pop everything from the stack, like its about to ret, but then JMP (not call) memset. Memset will then ret itself.
			} else if(sz-2 < SCHAR_MAX){	//jump relative 8
				memset(out, 0xF1, sz);
				char bmpmax = sz-2;
				((char*)out)[0] = 0xEB;
				((char*)(out+1))[0] = bmpmax;
			} else if(sz-5 < INT_MAX){	//jump relative 32 (16 is the same opcode... so its really 32)
				memset(out, 0xF1, sz);
				int bmpmax = sz-5;
				((char*)out)[0] = 0xE9;
				((int*)(out+1))[0] = bmpmax; // endian agnostic as long as this is being run on the same endianness as the object file is messing with
			}
		}
	}
}


//simple recursive function to randomly split and write a cruftable
void write_split_cruftable(void * out, size_t sz){
	if(!sz) return;
	if(sz > 1 && (double)rand()/(double)RAND_MAX < options.splitprobability){	//do i split?
		size_t splitpnt = rand()%sz;
		write_split_cruftable(out, splitpnt);
		write_split_cruftable(out+splitpnt, sz-splitpnt);
	} else {
		if(sz < 3){			//only have enough room for nops
			memset(out, 0x90, sz); //this may also cause the weird ret issue that plauges selfhosting confused cruftables.
		} else if(sz-2 < SCHAR_MAX){	//jump relative 8
			memset(out, 0xF1, sz);
			char bmpmax = sz-2;
			((char*)out)[0] = 0xEB;
			((char*)(out+1))[0] = bmpmax;
		} else if(sz-5 < INT_MAX){	//jump relative 32 (16 is the same opcode... so its really 32)
			memset(out, 0xF1, sz);
			int bmpmax = sz-5;
			((char*)out)[0] = 0xE9;
			((int*)(out+1))[0] = bmpmax; // endian agnostic as long as this is being run on the same endianness as the object file is messing with
		}
	}
}

size_t write_cruftable_wrapper(void * out, size_t sz){
	switch(options.cruftmethod){
		default:
		case 0:
			if(sz < 3){			//only have enough room for nops
				memset(out, 0x90, sz);
			} else if(sz-2 < SCHAR_MAX){	//jump relative 8
				memset(out, 0xF1, sz);
				char bmpmax = sz-2;
				((char*)out)[0] = 0xEB;
				((char*)(out+1))[0] = bmpmax;
			} else if(sz-5 < INT_MAX){	//jump relative 32 (16 is the same opcode... so its really 32)
				memset(out, 0xF1, sz);
				int bmpmax = sz-5;
				((char*)out)[0] = 0xE9;
				((int*)(out+1))[0] = bmpmax; // endian agnostic as long as this is being run on the same endianness as the object file is messing with
			}
		break;
		case 1:
			write_split_cruftable(out, sz);
		break;
		case 2:
			write_confused_cruftable(out, sz);
		break;
	}
	return sz;
}

//these enabled will make it faster
#define CRUFTABLES_MANUALLYCALC
#define CRUFTABLES_MANUALLYBUMP

size_t generate_cruftables_patch(analyze_section_metadata_t *meta){
	if(!meta) return 0;
	if(meta->numins < 1) return 0;
#ifdef CRUFTABLES_MANUALLYBUMP
	struct elf_section_header_64 * section;
	get_section(meta->elf, meta->section, &section);
	void * data = meta->elf->base_ptr + section->offset;
	if(meta->data) data = meta->data;
#endif

	printf(!options.supercruftables ? "choosing cruftables for %s using a patch\n" : "choosing cruftables and super-cruftables for %s using a patch\n", meta->section_name);
	patch_t *p = calloc(1, sizeof(patch_t));

	size_t *bsindcache = malloc(meta->numbs * sizeof(size_t));
#ifdef CRUFTABLES_MANUALLYCALC
	p->chtable = malloc(meta->numbs * sizeof(changeins_t));
	p->numch = 0;
	p->addsz = 0;
	//optimization so i dont have to run over all cruftables twice
#endif
	while((double)rand()/(double)RAND_MAX < options.cruftprobability){
		//choose random instruction to insert the cruftable before
		size_t insind = rand()%meta->numins;
		instr_t * ins = &meta->ins[insind];
		//choose amount
		size_t amount = 1+rand()%options.cruftsize;
		if(ins->cantcruft){
			if(!options.supercruftables) continue;		//super cruftables disabled, cant insert into a badsection (inside a relative jump)
			size_t numinbsindcache = 0;
			//find all the baddies that i touch
			//todo optimize finding which cruftables hit(see analyze.c for some ideas)
			size_t baddy;
			for(baddy = 0; baddy < meta->numbs; baddy++){
				badsection_t *bs = &meta->bs[baddy];
				//check if i touch
				if(!(bs->start < ins->loc && bs->end > ins->loc))continue; //todo make sure this isnt an off-by-one
				//check if i fit
				if(amount > bs->space) break;
				//well i fit and i hit it, so add to the list (for optimization)
				bsindcache[numinbsindcache++] = baddy;
			}
			if(baddy != meta->numbs) continue; //one of the badsections that i touch was too small, cant place anything here
			//ok, im in a bs, andi fit in all of them

			//go through list of bs i touched
			for(baddy = 0; baddy < numinbsindcache; baddy++){
				badsection_t *bs = &meta->bs[bsindcache[baddy]];
				//modify space
				bs->space -= amount;
				//add a changeop
#ifdef CRUFTABLES_MANUALLYCALC
#ifndef CRUFTABLES_MANUALLYBUMP
				bumpchangeins(meta, p, bs->causeop, amount);
#else
				instr_t *ins = &meta->ins[bs->causeop];
				size_t j;
				for(j =0; j < p->numch; j++){
					if(p->chtable[j].insind == bs->causeop) break;
				}
				changeins_t *ch = p->chtable + j;
				if(j == p->numch){
					p->numch++;
					ch->insind = bs->causeop;
					ch->loc = ins->loc;
					ch->bytes_current = ch->bytes_orig = ins->bytesofs;
					ch->opcodeindx = 0;
					ch->len_orig = ins->len;
					ch->len_new = ins->len;
					ch->towhere_new = 0;
					ch->bsize = 0;
//					printf("boits %li\n", ch->bytes_orig);

					void * insdata = data + ins->loc;
					void * operanddata = insdata + ins->len - ch->bytes_orig;
					switch(ch->bytes_orig){
						case 1:
							ch->towhere_orig = *(char*)operanddata;
						break;
						case 2:
							ch->towhere_orig = *(short*)operanddata;
						break;
						case 4:
							ch->towhere_orig = *(int*)operanddata;
						break;
						default:
							printf("addchangeins ERROR: operand length is odd? %li\n", ch->bytes_orig);
							return -1;
						break;
					}
				}
				ch->bsize += amount;
#endif
#endif
			}
		}
		mod_t m = {insind, ins->loc, amount, 1, 0, NULL};
		patch_addmod(p, m);
		p->addsz += amount;
	}

	if(bsindcache){ free(bsindcache); bsindcache = 0;}
	if(!p->nummods){
		clean_patch(p);
		free(p);
		return 0;
	}

#ifndef CRUFTABLES_MANUALLYCALC
	calculate_patch(meta,p);
#else



	//TODO really could just abuse the calc function..... It would be slower, but less "hacky"

	size_t i;
	//we have to manualy apply our changeins_new, since the calculate is supposed to do that
	for(i = 0; i < p->numch; i++){
		changeins_t *ch = p->chtable+i;
		ch->towhere_new = ch->towhere_orig + ( ch->towhere_orig > 0 ? ch->bsize : -ch->bsize);
//		printf("towhere %li -> %li\n", ch->towhere_orig, ch->towhere_new);
	}

	meta->pendingpatch = p;
	//patch is done, lets clean it up a bit
	if(p->nummods != p->sizemods){
		p->sizemods = p->nummods;
		p->modtable = realloc(p->modtable, p->sizemods * sizeof(patch_t));
	}
	//dont need to calc out locs since we do it on insertion
	//sort it up
	qsort(p->modtable, p->nummods, sizeof(mod_t), mod_compareofs);
	if(p->chtable){
		qsort(p->chtable, p->numch, sizeof(changeins_t), changeins_compareofs);
		p->chtable = realloc(p->chtable, p->numch * sizeof(changeins_t));
	}
	patch_genfixups(p);

#endif
	//should be set to go!
	return p->nummods;
}

int generate_cruftables_anywhere(analyze_section_metadata_t *meta){
	if(!meta) return 0;
	if(meta->numins < 1) return 0;

	patch_t *p = calloc(1, sizeof(patch_t));
	p->numch = 0;
	p->addsz = 0;
	while((double)rand()/(double)RAND_MAX < options.cruftprobability){
		//choose random instruction to insert the cruftable before
		size_t insind = rand()%meta->numins;
		instr_t * ins = &meta->ins[insind];
		//choose amount
		size_t amount = 1+rand()%options.cruftsize;
		mod_t m = {insind, ins->loc, amount, 1, 0, NULL};
		patch_addmod(p, m);
	}
	if(!p->nummods){
		clean_patch(p);
		free(p);
		return 0;
	}
	//apply the patch
	if(calculate_patch(meta, p) <0){
		clean_patch(p);
		free(p);
		return -1;
	}

	return p->nummods;
}

size_t generate_cruftables_wrapper(analyze_section_metadata_t *meta){
	//attempt anywhere, if that fails, fall back to the size check method
	if(options.supercruftables){
		int res = generate_cruftables_anywhere(meta);
		if(res >=0){
			return res;
		}
		printf("Unable to do anywhere cruftables, doing them normally\n");
	}


	return generate_cruftables_patch(meta);
}
