#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>


#include <r_anal.h> // i guess?

#include "parser.h"

#include "analyze.h"

#include "options.h"

#include "cruftable.h"
#include "fpatch.h"



char *jmpshortopcodes[] = {
	"\xEB", //standard old jump
	"\x70", "\x71", "\x72", "\x73",		//conditional jumps
	"\x74", "\x75", "\x76", "\x77",
	"\x78", "\x79", "\x7A", "\x7B",
	"\x7C", "\x7D", "\x7E", "\x7F",
	/*"\xE3"*/	//JECXZ, we dont want to use that...
};
char *jmpnearopcodes[] = {
	"\xE9",	//standard old jump
	"\x0f\x80","\x0f\x81","\x0f\x82","\x0f\x83",	//conditional jumps
	"\x0f\x84","\x0f\x85","\x0f\x86","\x0f\x87",
	"\x0f\x88","\x0f\x89","\x0f\x8A","\x0f\x8B",
	"\x0f\x8C","\x0f\x8D","\x0f\x8E","\x0f\x8F"
};


int changeins_getopcodeindx(size_t len, size_t operandbytes, void * data){
	size_t i;
	char ** cmpdat = (operandbytes == 1 ? jmpshortopcodes : jmpnearopcodes);
	for(i = 0; i < 17; i++){
		char * cmpd = cmpdat[i];
		//strncmp on an opcode!
		//TODO predefined table on this?
		if(!strncmp(cmpd, data, len-operandbytes)) return i;
	}
	printf("getopcodeindx WARNING: Unknown jump opcode: \033[22;31m");
	for(i = 0; i < len-operandbytes;i++) printf("%02hhx", ((char*)data)[i]);
	printf("\033[0m");
	for(; i < len;i++) printf("%02hhx", ((char*)data)[i]);
	printf("\n");
	return -1;
}

//get the len of the current bytes_current and opcodeindx
//assume no invalid arguments
//yes it uses strlen, but thats probably the fastest method without resorting to a predefined table
//TODO make a predefined table
size_t changeins_getlen(changeins_t *ch){
	if(ch->opcodeindx<0) return 0;
	if(ch->bytes_current ==1){
		return strlen(jmpshortopcodes[ch->opcodeindx]) + 1;
	} else if(ch->bytes_current == 2 || ch->bytes_current == 4){
		return strlen(jmpnearopcodes[ch->opcodeindx]) + 4;
	}
	return 0;
}



int addchangeins(analyze_section_metadata_t *meta, patch_t *p, size_t op){
	if(!meta || !p->chtable || op > meta->numins){
		printf("addchangeins ERROR: bad arguments\n");
		return -1;
	}

	struct elf_section_header_64 * section;
	get_section(meta->elf, meta->section, &section);
	void *data = meta->elf->base_ptr + section->offset;
	if(meta->data) data = meta->data;

	instr_t *ins = meta->ins+op;

	void * insdata = data + ins->loc;
	changeins_t * ch = p->chtable + p->numch;
	p->numch++; //"add it"

	ch->loc = ins->loc;
	ch->bsize = 0;
	ch->insind = op;

	ch->bytes_current = ch->bytes_orig = ins->bytesofs;
	ch->len_orig = ins->len;
	ch->len_new = ins->len;	//only change it if it gets modified

	ch->opcodeindx = changeins_getopcodeindx(ch->len_orig, ch->bytes_orig, insdata);

	ch->towhere_new = 0;
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
	return 0;
}

int bumpchangeins(analyze_section_metadata_t * meta, patch_t *p, size_t op, ssize_t bumpsize){
	if(!meta || op >meta->numins){
		printf("bumpchangeins ERROR: bad arguments\n");
		return -1;
	}
	//find the changeins if its arleady in there
	size_t i;
	for(i = 0; i < p->numch; i++){
		if(p->chtable[i].insind == op) break;
	}
	//its not there, we have to add it
	if(i == p->numch){
		int err = addchangeins(meta, p, op);
		if(err)return(err);
	}
	//bump the size
	p->chtable[i].bsize += bumpsize;
	return 0;
}


void print_patch(patch_t *p){
	size_t i;
	printf("Patch table\n");
	printf("\tindex|\tinsind:\tloc,\tsize,\tflags,\treplacesize,\tdata\n");
	for(i = 0; i < p->nummods; i++){
		mod_t *m = p->modtable +i;
		printf("\t%li|\t%li:\t%li,\t%li,\t%i,\t%li,\t%p\n", i, m->insind, m->loc, m->size, m->flags, m->replacesize, m->data);
		if(m->replacesize > m->size){
			printf("%s:%i ", __FILE__, __LINE__);
			printf("Warning: replacesize > size! %li > %li\n", m->replacesize, m->size);
		}
	}
	//todo print the changeins
}

//used for qsort
int mod_compareofs(const void * a, const void * b){
	mod_t * am = ((mod_t*)a);
	mod_t * bm = ((mod_t*)b);
	//todo
	if(am->loc == bm->loc){		//todo determin if this is a good idea...
		if(am->replacesize) return 1;
		if(am->replacesize) return -1;
	}
	return am->loc - bm->loc;
}

int changeins_compareofs(const void * a, const void * b){
	changeins_t * am = ((changeins_t*)a);
	changeins_t * bm = ((changeins_t*)b);
	//todo
	return am->insind - bm->insind;
}



//todo modify this to not requite meta (dont really need to grab ins)
//might not need meta, might only need ins?
size_t modify_ins(analyze_section_metadata_t *meta, changeins_t *ch, void * out, void *in, size_t *outofs, size_t *inofs){
	//check if we need to adjust size
	instr_t * ins = &meta->ins[ch->insind];
	//bytesize remains the same
	if(ch->bytes_orig == ch->bytes_current || (ch->bytes_orig == 4 && ch->bytes_current == 2) || (ch->bytes_orig == 2 && ch->bytes_current == 4)){
		//copy the orig
		memcpy(out, in, ins->len);
		//only need to change value actually
		void * jumpdata = out + ins->len - ins->bytesofs;
		switch(ins->bytesofs){
			case 1:
				*(char*)jumpdata = ch->towhere_new;
			break;
			case 2:
				*(short*)jumpdata = ch->towhere_new;
			break;
			case 4:
				*(int*)jumpdata = ch->towhere_new;
			break;
			default:
				printf("ERROR unknown jump operand size %i\n", ins->bytesofs);
			break;
		}
		//that was easy...
		*outofs += ins->len;
		*inofs += ins->len;
		return ins->len;
	} else {	//bytesize changes
		//not doing any checks here for opcodeindx, etc, because invalid ones should've errored earlier
		char * oppy = ch->bytes_current == 1 ? jmpshortopcodes[ch->opcodeindx] : jmpnearopcodes[ch->opcodeindx];
		if(!ch->len_new){
			printf("modify_ins ERROR no len_new?\n");
			return 0;
		}
		size_t oplen = strlen(oppy);	//ehhhhhh TODO change to a table
		memcpy(out, oppy, oplen);
		void * jumpdata = out + oplen;
		//write my 4 bits
		//todo switch statement?
		*(int*)jumpdata = ch->towhere_new;

		*outofs	+= ch->len_new;
		*inofs	+= ch->len_orig;
		return ch->len_new;
	}
	return 0;
}

size_t patch_genfixups(patch_t *p){
	//lets calculate our fixups now
	//copy them over sorted
	p->numfixups = p->nummods + p->numch;
	p->fixups = malloc(p->numfixups * sizeof(fixup_tracker_t));

	size_t i = 0;
	size_t j = 0;
	size_t k = 0;
	while(j < p->nummods && k < p->numch){
		for(;j < p->nummods && p->modtable[j].loc <= p->chtable[k].loc; j++, i++){
			p->fixups[i].loc = p->modtable[j].loc;
//			p->fixups[i].size= p->modtable[j].size;
			p->fixups[i].size= p->modtable[j].size - p->modtable[j].replacesize;
		}
		if(!(j < p->nummods))break;
		for(;k < p->numch && p->chtable[k].loc < p->modtable[j].loc; k++){
			if(!(p->chtable[k].len_new - p->chtable[k].len_orig))continue;  //ignore ones that dont change size
			p->fixups[i].size= p->chtable[k].len_new - p->chtable[k].len_orig;
			p->fixups[i].loc = p->chtable[k].loc;
			i++;
		}
	}
	//copy the rest of em
	for(;j < p->nummods; j++, i++){
		p->fixups[i].loc = p->modtable[j].loc;
		p->fixups[i].size= p->modtable[j].size - p->modtable[j].replacesize;
	}
	for(;k < p->numch; k++){
		if(!(p->chtable[k].len_new - p->chtable[k].len_orig))continue;  //ignore ones that dont change size
		p->fixups[i].size= p->chtable[k].len_new - p->chtable[k].len_orig;
		p->fixups[i].loc = p->chtable[k].loc;
		i++;
	}
	if(p->numfixups != i){
		p->numfixups = i;
		p->fixups = realloc(p->fixups, p->numfixups * sizeof(fixup_tracker_t));
	}
	return p->numfixups;
}


int calculate_patch(analyze_section_metadata_t *meta, patch_t *p){
	if(!p->nummods) return 0;
	if(meta->pendingpatch && meta->pendingpatch != p){
		clean_patch(meta->pendingpatch);
		free(meta->pendingpatch);
		meta->pendingpatch = 0;
	}

	struct elf_section_header_64 * section;
	get_section(meta->elf, meta->section, &section);

	size_t i;
	void *id = meta->elf->base_ptr + section->offset;
	if(meta->data) id = meta->data;

	//patch is now complete, so we can crunch it down
	if(p->nummods != p->sizemods){
		p->sizemods = p->nummods;
		p->modtable = realloc(p->modtable, p->sizemods * sizeof(patch_t));
	}

	//just go through and calc the locs (if the person adding didnt)
	for(i = 0; i < p->nummods; i++){
		p->modtable[i].loc = meta->ins[p->modtable[i].insind].loc;
	}

	//lets sort it too
	qsort(p->modtable, p->nummods, sizeof(mod_t), mod_compareofs);



	if(p->chtable)free(p->chtable);	//oops?
	p->numch = 0;
	p->chtable = malloc(meta->numbs * sizeof(changeins_t));	//allocate space for jump instructions that we have to change. There cant be more than meta->numbs of them, as there is only one changeins per badsection (the causeop)

	p->addsz = 0;	//just keep track of the total size we add in (todo make sure it happens for the second pass as well)


	//go through mods, see if we need to bump anything
	for(i = 0; i < p->nummods; i++){
		mod_t *m = p->modtable + i;
		if(m->size == m->replacesize) continue;	//size wont change, dont bother

		//TODO do i need to also add for the changeins?
		p->addsz += m->size;
		p->addsz -= m->replacesize;
		instr_t * ins = &meta->ins[m->insind];
		//early out for badzone check
		if(!ins->cantcruft) continue;

		//loop through badzones, modify ones we are in
		size_t baddy;
		//todo optimize this (see analyze.c for ideas)
		//could just BINARY SEARCH, maybe
		for(baddy = 0; baddy < meta->numbs; baddy++){
			badsection_t *bs = &meta->bs[baddy];
			//check if we touch
			if(!(bs->start < ins->loc && bs->end > ins->loc)) continue;	//todo make sure this isnt an off by one? Likely not, this has been in the code forever
			//well, we touch, add or bump it
			int err = bumpchangeins(meta, p, bs->causeop, m->size - m->replacesize);
			if(err){
				if(p->chtable) free(p->chtable);
				p->chtable = 0;
				return -1;
			}
		}
	}

	//end of first pass, now have to process the changeins we added

	int modded = 1;
	while(modded){
		modded = 0;
		//lets go through, adjust changeins towhere_new and see if its too big
		for(i = 0; i < p->numch; i++){
			changeins_t * ch = p->chtable+i;
			instr_t *ins = meta->ins + ch->insind;
			//calculate towhere_new
			ch->towhere_new = ch->towhere_orig + ( ch->towhere_orig > 0 ? ch->bsize : -ch->bsize);
			//figure out the size we need for it
			size_t bytes_new = 1;
			if(ch->towhere_new > SCHAR_MAX || ch->towhere_new < SCHAR_MIN){
				bytes_new = 2;//todo make sure its not 2 ever?
				if(ch->towhere_new > SHRT_MAX || ch->towhere_new < SHRT_MIN){
					bytes_new = 4;
				}
			}
			//if we need to adjust our size (we dont fit anymore!)
			if(bytes_new > ch->bytes_current){
				printf("BUMPING %li, %li: %li -> %li\n", ch->towhere_orig, ch->towhere_new,ch->bytes_current, bytes_new);
				ch->bytes_current = bytes_new;
				//check to make sure we recognize the opcode
				if(ch->opcodeindx < 0){
					char * insdata = ins->loc + id;
					printf("calculate_patch 2nd pass ERROR unknown opcode!");
					size_t j;
					for(j=0; j < ins->len; j++) printf("%x", insdata[j]);
					printf("\n");
//					if(p->chtable) free(p->chtable);
//					p->chtable = 0;
					return -1;
				}
				//so we are going to need to update our size now
				ch->len_new = changeins_getlen(ch);
				if(!ch->len_new){
					printf("calculate_patch 2nd pass ERROR unknown len_new?\n");
//					if(p->chtable) free(p->chtable);
//					p->chtable = 0;
					return -1;
				}
				size_t incbytes = ch->len_new - ch->len_orig;
				p->addsz += incbytes;
				//ok, we really need to update our size

				//early out for badzone check
				if(!ins->cantcruft) continue;

				//loop through badzones, modify ones we are in
				size_t baddy;
				//todo optimize this (see analyze.c for ideas)
				//could just BINARY SEARCH, maybe
				for(baddy = 0; baddy < meta->numbs; baddy++){
					badsection_t *bs = &meta->bs[baddy];
					//check if we touch
					if(!(bs->start < ins->loc && bs->end > ins->loc)) continue;//todo check to make sure this works for stuff after the ins?
					modded = 1;	//because we are modifying something,.make sure to loop again
					//well, we touch, add or bump it
					int err = bumpchangeins(meta, p, bs->causeop, incbytes);
					if(err){
						if(p->chtable) free(p->chtable);
						p->chtable = 0;
						return -1;
					}
				}
			}
		}
	}
	//need to sort the changeins
	if(p->chtable){
		qsort(p->chtable, p->numch, sizeof(changeins_t), changeins_compareofs);
		//and crunch them
		p->chtable = realloc(p->chtable, p->numch * sizeof(changeins_t));
	}
	patch_genfixups(p);

	meta->pendingpatch = p;
	return p->addsz;
}
int apply_patch(analyze_section_metadata_t *meta){

	patch_t *p = meta->pendingpatch;
	if(!p || !p->nummods){
		return 0;
	}
	meta->isanalyzed = 0;	//we are modifying the instructions, so we invalidate the analysis

	struct elf_section_header_64 * section;
	get_section(meta->elf, meta->section, &section);

	void *id = meta->elf->base_ptr + section->offset;
	if(meta->data) id = meta->data;
	size_t insize = (meta->data ? meta->datasize : section->size);

	//do a copy yo
	size_t outsize = p->addsz + insize;


	void * outd = malloc(outsize);

	size_t k = 0;
	size_t j = 0;

	size_t inofs = 0;
	size_t outofs = 0;


	while(inofs < insize){
		size_t inend = insize;
		//find next mod
		for(; k < p->nummods; k++)if(p->modtable[k].loc >= inofs) break;
		if(k < p->nummods && inend > p->modtable[k].loc) inend = p->modtable[k].loc;
		//find next changeins
		for(; j < p->numch; j++)if( p->chtable[j].loc >= inofs) break;
		if(j < p->numch && inend > p->chtable[j].loc) inend = p->chtable[j].loc;

//		printf("Debugging %li/%li %li/%li\n", k, p->nummods, j, p->numch);
		//copy data over
		if(inofs < inend){
			memcpy(outd + outofs, id +inofs, inend-inofs);
			outofs += (inend-inofs);
			inofs = inend;
		}
		//do we write mod(s)
		while(k < p->nummods && p->modtable[k].loc == inofs){
			mod_t *cm = p->modtable + k;
				//write it
			if(!cm->data)
				write_cruftable_wrapper(outd + outofs, cm->size);
			else
				memcpy(outd + outofs, cm->data, cm->size);
			outofs += cm->size;
			inofs += cm->replacesize;
			//todo figure out best way to sort when replacesize exists
			//inc
			k++;
		}
		//do we write a changeins?
		if(j < p->numch){
			changeins_t *cmi = p->chtable + j;
			if(inofs == cmi->loc){
//				printf("len new is %li\n", cmi->len_new);
				modify_ins(meta, cmi, outd + outofs, id + inofs, &outofs, &inofs);
//				memset(outd + outofs, 0x90,  cmi->len_new);
//				outofs += cmi->len_new;
//				inofs += cmi->len_new;
				j++;
			}
		}
	}
	if(outofs < outsize){
		printf("fpatch apply_patch WARNING: outofs < outsize! %li < %li\n", outofs, outsize);
	}

	//we are going to have to fixup rel and sym soon

	if(id == meta->data) free(id);
	meta->data = outd;
	meta->datasize = outsize;

//	clean_patch(p);
//	free(p);
//	meta->pendingpatch = 0;


//	if(p->chtable) free(p->chtable);
//	p->chtable = 0;
	return 0;

}


int clean_mod(mod_t *m){
	if(m->data && ! m->flags &MOD_DONTFREE){
		free(m->data);
		m->data = 0;
	}
	return 0;
}

int clean_patch(patch_t *p){
	size_t i;
	if(p->modtable){
		for(i = 0; i < p->nummods; i++){
			clean_mod(p->modtable + i);
		}
		free(p->modtable);
	}
	if(p->chtable) free(p->chtable);
	if(p->fixups) free(p->fixups);
	p->modtable = 0;
	p->nummods = 0;
	p->chtable = 0;
	p->numch = 0;
	p->fixups = 0;
	p->numfixups = 0;
	return 0;
}



int patch_addmod(patch_t *p, mod_t m){
	if(p->nummods+1 > p->sizemods){
		p->sizemods = (p->nummods+1) * 2;
		p->modtable = realloc(p->modtable, p->sizemods * sizeof(mod_t));
	}
	p->modtable[p->nummods] = m;
	p->nummods++;
	return 0;
}
int patch_addpatch(patch_t *orig, patch_t *n){
	if(orig->nummods + n->nummods > orig->sizemods){
		//todo could align this nicer
		orig->sizemods  = (orig->nummods + n->nummods) * 2;
		orig->modtable = realloc(orig->modtable, orig->sizemods * sizeof(mod_t));
	}
	memcpy(orig->modtable + orig->nummods, n->modtable, n->nummods * sizeof(mod_t));
	orig->nummods += n->nummods;

	size_t i;
	//im going to go through and make sure to mark the mods in n as dontfree
	for(i = 0; i < n->nummods; i++){
		n->modtable[i].flags |= MOD_DONTFREE;
	}
	//another method could be to make copies... todo decide if this is better
	return 0;
}




//these next few functions are very similar to the cruftables ones
size_t patch_fixup_symtab(struct elf_section_header_64 * section, struct elf_file_metadata * elf, analyze_section_metadata_t * metas){
	struct elf_symtab_entry *sym = (struct elf_symtab_entry*)(elf->base_ptr + section->offset);
	size_t count = section->size / sizeof(struct elf_symtab_entry);
	size_t i;
	//todo support special cases of SHT_SYMDAB_SHNDX
	for(i = 0; i < count; i++, sym++){
		//check if it points to a section we modified
		if(sym->shidx < 0 || sym->shidx >= elf->numsechdrs || !metas[sym->shidx].section)continue;
		analyze_section_metadata_t * meta = &metas[sym->shidx];
		patch_t *p = meta->pendingpatch;
		if(!p || !p->numfixups) continue;
		size_t ofscounter = 0;
		size_t szcounter = 0;
		size_t j;
		//we can early out since meta-> is sorted
		for(j = 0; j < p->numfixups && p->fixups[j].loc < sym->value; j++) ofscounter += p->fixups[j].size;
		for(; j < p->numfixups && p->fixups[j].loc < sym->value+sym->size; j++) szcounter += p->fixups[j].size;
		if(ofscounter || szcounter){
			//todo verbosity options
			printf("\t adjusting symbol offset 0x%"PRIx64" to 0x%"PRIx64" (%li total)\tsize %li to %li (%li total)\n", sym->value, sym->value+ofscounter, ofscounter, sym->size, sym->size+szcounter, szcounter);
			sym->value += ofscounter;
			sym->size += szcounter;
		}
	}
	return i;
}

size_t patch_fixup_rela(struct elf_section_header_64 * section, analyze_section_metadata_t *meta){
	patch_t *p = meta->pendingpatch;
	if(!p || !p->numfixups) return 0;
	struct elf_file_metadata *elf = meta->elf;
	struct elf64_rela * reloc = (struct elf64_rela *)(elf->base_ptr + section->offset);
	size_t count = section->size / sizeof(struct elf64_rela);
	size_t i;
	for(i = 0; i < count; i++, reloc++){
		size_t ofscounter = 0;
		size_t j;
		//early out since fixups are sorted
		for(j = 0; j < p->numfixups && p->fixups[j].loc < reloc->offset; j++) ofscounter += p->fixups[j].size;
		if(ofscounter){
			printf("\t adjusting reloc (rela) 0x%"PRIx64" to 0x%"PRIx64" (%li total)\n", reloc->offset, reloc->offset+ ofscounter, ofscounter);
			reloc->offset += ofscounter;
		}
	}
	return i;
}
size_t patch_fixup_rel(struct elf_section_header_64 * section, analyze_section_metadata_t *meta){
	patch_t *p = meta->pendingpatch;
	if(!p || !p->numfixups) return 0;
	struct elf_file_metadata *elf = meta->elf;
	struct elf64_rel * reloc = (struct elf64_rel *)(elf->base_ptr + section->offset);
	size_t count = section->size / sizeof(struct elf64_rel);
	size_t i;
	for(i = 0; i < count; i++, reloc++){
		size_t ofscounter = 0;
		size_t j;
		//early out since fixups are sorted
		for(j = 0; j < p->numfixups && p->fixups[j].loc < reloc->offset; j++) ofscounter += p->fixups[j].size;
		if(ofscounter){
			printf("\t adjusting reloc (rela) 0x%"PRIx64" to 0x%"PRIx64" (%li total)\n", reloc->offset, reloc->offset+ ofscounter, ofscounter);
			reloc->offset += ofscounter;
		}
	}
	return i;
}

//this is gross and probably could be optimized/data cached
size_t jumphack_get_ofs(struct elf_file_metadata * elf, size_t secnum, size_t loc){
	//why even bother...
	if(loc == 0){
		return 0;
	}

	size_t minofs = loc;
	//find all relocs
	size_t i;
	for(i = 0; i < elf->numsechdrs; i++){
		struct elf_section_header_64 * relsec = 0;
		//todo err
		get_section(elf, i, &relsec);
		if(relsec->type != SHT_REL && relsec->type != SHT_RELA) continue;	//probably only need to look for relas
		//check if this reloc is for an executable section
		struct elf_section_header_64 * tmpsec = 0;
		get_section(elf, relsec->info, &tmpsec);
		if(!(tmpsec->flags & SHF_EXECINSTR)) continue;

		//grab symbol table
		struct elf_section_header_64 *symtab = 0;
		get_section(elf, relsec->link, &symtab);
		size_t symcount = symtab->size / sizeof(struct elf_symtab_entry);
		struct elf_symtab_entry *symt =(struct elf_symtab_entry*)(elf->base_ptr + symtab->offset);

		//alright lets roll though
		size_t j;

		if(tmpsec->type == SHT_REL){
			struct elf64_rel * reloc = (struct elf64_rel *)(elf->base_ptr + relsec->offset);
			size_t count = relsec->size / sizeof(struct elf64_rel);
			for(j = 0; j < count; j++, reloc++){
				//make sure the reloc is of the right type
				//todo if this breaks stuff fix it
				if( ELF64_R_TYPE(reloc->info) != 0x02) continue;
				//grab the symbol
				size_t sind = ELF64_R_SYM(reloc->info);
				if(sind >= symcount)continue;
				struct elf_symtab_entry *sym = symt+sind;
				//make sure the symbol points to my section
				//todo support SHT_SYMTAB_SHNDX
				if(sym->shidx != secnum) continue;
				//make sure that my loc falls within the symbol's range
				//some valid symbols have a size of 0 (really gcc?), so i just ignore size
				if(sym->value > loc) continue;
				//ok, we point to this symbol, do we point before it?
				ssize_t addend = 0;	//reloc lol
				if(sym->value + addend <= loc){
					size_t ofs = loc-sym->value+addend;
					if(ofs < minofs) minofs = ofs;
				}
			}
		} else { //rela
			//nearly the same code as above, so i removed most of the comments
			struct elf64_rela * reloc = (struct elf64_rela *)(elf->base_ptr + relsec->offset);
			size_t count = relsec->size / sizeof(struct elf64_rela);
			for(j = 0; j < count; j++, reloc++){
				if( ELF64_R_TYPE(reloc->info) != 0x02) continue;
				size_t sind = ELF64_R_SYM(reloc->info);
				if(sind >= symcount)continue;
				struct elf_symtab_entry *sym = symt+sind;
				if(sym->shidx != secnum) continue;
				if(sym->value > loc) continue;
				ssize_t addend = reloc->addend+4;	//+4 because relative to the end of the instruction sorta junk
				if(sym->value + addend <= loc){
					size_t ofs = loc-(sym->value+addend);
					if(ofs < minofs) minofs = ofs;
				}
			}
		}
	}
	if(minofs != loc)printf("minofs %li vs fakeofs %li\n", minofs, loc);
	return minofs;
}

//very similar to the cruftable one
//todo a lot of cleanup here (as well as optimization... binary search?)
size_t patch_fixup_rela_addend(struct elf_section_header_64 *section, struct elf_file_metadata *elf, analyze_section_metadata_t *metas){
	size_t i;
	struct elf_section_header_64 *symtab = 0;
	//find the symbol table
	get_section(elf, section->link, &symtab);
	size_t symcount = symtab->size / sizeof(struct elf_symtab_entry);
	struct elf_symtab_entry *symt =(struct elf_symtab_entry*)(elf->base_ptr + symtab->offset);

	//find the section where the relocations are applied (eg, .text for .rela.text
	analyze_section_metadata_t * applymeta = &metas[section->info];


	struct elf64_rela * reloc = (struct elf64_rela *)(elf->base_ptr + section->offset);
	size_t count = section->size / sizeof(struct elf64_rela);
	for(i =0 ; i < count; i++, reloc++){
		if(!reloc->addend) continue;
		//grab the symol responsible
		size_t sind = ELF64_R_SYM(reloc->info);
		if(sind >= symcount)continue;
		struct elf_symtab_entry *sym = symt+sind;
		//find meta responsible
		if(sym->shidx < 0 || sym->shidx >= elf->numsechdrs) continue;
		analyze_section_metadata_t *meta = &metas[sym->shidx];			//where the reloactions point to
		patch_t *p =meta->pendingpatch;
		if(!p || !p->numfixups) continue;

		//find start of crufts after symbol location

		ssize_t ofscounter = 0;
		size_t j;

		//those -4 cases
		if(reloc->addend < 0){
			if(reloc->addend == -4 && ELF64_ST_TYPE(sym->info) == STT_FUNC)	//its likely a 32 bit relative call. It will point 4 bytes before the actual place it jumps to because x86 jump logic
				continue;
			//we can early out since meta->cr is sorted
			for(j = 0; j < p->numfixups && p->fixups[j].loc < sym->value+reloc->addend; j++);
			for(;j < p->numfixups && p->fixups[j].loc < sym->value; j++){
				ofscounter -= p->fixups[j].size;
				printf("Fixup %li got me! %li, i am %li, %li\n", j, p->fixups[j].loc, sym->value, reloc->addend);
			}
		} else {
			//we can early out since fixups is sorted
			for(j = 0; j < p->numfixups && p->fixups[j].loc < sym->value; j++);
			//todo make a better check for this
			//better hack, find the symbol that the reloc is "in", find any relas that point to that symbol, find the one with the largest addend that points before me, Use the difference between that point before me to be the weird adjust
			//basically look for the RELOC that references the jump table, calculate the required offset based on it
			if(options.jumptablehack && ELF64_R_TYPE(reloc->info) == 0x02 && strcmp(applymeta->section_name, ".eh_frame")){		//the strcmp is a hack to keep the jumptable hack from breaking eh_frame sections.	///TODO may be needed to use on paravirt sections?
				//oldhack
//				for(;j < meta->numcr && meta->cr[j].location < sym->value+reloc->addend-reloc->offset; j++) ofscounter += meta->cr[j].amount;
				//newhack
				size_t ofs = jumphack_get_ofs(elf, section->info, reloc->offset);
				for(;j < p->numfixups && p->fixups[j].loc < sym->value+reloc->addend-ofs; j++) ofscounter += p->fixups[j].size;
													//^big-ol-hack right there for the weird jump table math that whatever compiler does
			} else {
				for(;j < p->numfixups && p->fixups[j].loc < sym->value+reloc->addend; j++) ofscounter += p->fixups[j].size;
			}
		}
		if(ofscounter){
			//todo verbosity options
			//TODO this first %s appears to be wrong? compare to the output of readelf -r
			// its like it just strips the .rela from the front of .rela
			//dunno, it looks weird
			printf("\t %s adjusting reloc ADDEND 0x%"PRIx64"(%li) to 0x%"PRIx64"(%li) (%li total)\n", applymeta->section_name, reloc->addend, reloc->addend, reloc->addend+ ofscounter, reloc->addend+ofscounter, ofscounter);
			reloc->addend += ofscounter;
		}
	}

	return i;
}
//generic function that "applies" patches entirely
size_t apply_and_fixup_patches(struct elf_file_metadata *elf, analyze_section_metadata_t * metas){
	size_t err;
	size_t i;
	for(i = 0; i < elf->numsechdrs; i++){
		analyze_section_metadata_t *m = &metas[i];
		if(m->section && m->pendingpatch);
		apply_patch(m);
	}
	for(i = 0; i < elf->numsechdrs; i++){
		struct elf_section_header_64 * sec = 0;
		err = get_section( elf, i, &sec);
		if(err) continue;
		char * nm = 0;
		get_section_name(elf, i, &nm);
		//printf("Fixing symbols and rels on %li aka %s\n",i, nm);
		switch( sec->type ) {
			case SHT_REL:
				printf("apply_and_fixup_patches SHT_REL on section %s\n", nm);
				//todo 32 rel and rela?
				if(metas[sec->info].section && metas[sec->info].pendingpatch){
					printf("rel %li fixing up looking at %u\n", i, sec->info);
					patch_fixup_rel(sec, &metas[sec->info]);
				}
			break;
			case SHT_RELA:
				printf("apply_and_fixup_patches SHT_RELA on section %s\n", nm);
				if(metas[sec->info].section && metas[sec->info].pendingpatch){
					printf("rela %li fixing up looking at %u\n", i, sec->info);
					patch_fixup_rela(sec, &metas[sec->info]);
				}
				//todo optimize this so it can do all at once?
				printf("rela addend %li fixing up\n", i);
				patch_fixup_rela_addend(sec, elf, metas);
			break;
			default:
			break;
		}
	}

	//need to fixup symtab AFTER the rela and reloc, because fixup_rela_addend relies on it being in the right spot
	for(i = 0; i < elf->numsechdrs; i++){
		struct elf_section_header_64 * sec = 0;
		err = get_section( elf, i, &sec);
		if(err) continue;
		if(sec->type == SHT_SYMTAB){
			char * nm = 0;
			get_section_name(elf, i, &nm);
			printf("symtab %li fixing up\n", i);
			patch_fixup_symtab(sec, elf, metas);
		}
	}

	for(i = 0; i < elf->numsechdrs; i++){
		analyze_section_metadata_t *m = &metas[i];
		if(m->pendingpatch){
			clean_patch(m->pendingpatch);
			free(m->pendingpatch);
			m->pendingpatch = 0;
		}
	}
	return 0;
}
