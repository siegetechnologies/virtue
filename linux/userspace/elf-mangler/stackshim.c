#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>

#include <r_anal.h>
#include <r_asm.h>

#include "parser.h"
#include "analyze.h"

#include "options.h"
#include "fpatch.h"
#include "stackshim.h"
#include "modit.h"
#include "frame_fix.h"


//sub rsp 4 bytes
//4881ec OFFSET
//add rsp 4 bytes
//4881c4 OFFSET

//sub rsp 1 byte
//4883ec OFFSET
//add rsp 1 byte
//4883c4 OFFSET


#include "stackshimasm.hex.h"





int shim_comparestart(const void * a, const void *b){
	return((shim_t *)a)->start - ((shim_t*)b)->start;
}


//mostly a copy of applystackshim at this point
int generate_dynstackshim(analyze_section_metadata_t * meta){

	struct elf_file_metadata * elf = meta->elf;
	struct elf_section_header_64 * section;
	get_section(elf, meta->section, &section);

	uint8_t * data = elf->base_ptr + section->offset;
	if(meta->data) data = meta->data;


	//lets find my reloc section
	struct elf_section_header_64 * relasec;

	size_t relaindx;
	for(relaindx = 0; relaindx < elf->numsechdrs; relaindx++){
		get_section(elf, relaindx, &relasec);
		if(relasec->type == SHT_RELA && relasec->info == meta->section) break;
	}
	if(relaindx >= elf->numsechdrs){
		//gotta make our own i guess!
		//TODO
	}

	patch_t *p = calloc(1, sizeof(patch_t));


	//for each executable section

	instr_t * ins = meta->ins;
	size_t insi = 0;
	symbol_t *cursym;
	size_t symi;
	//symbols are sorted
	shim_t * shims = 0;
	size_t sizeshims = 0;
	for(symi = 0, cursym = meta->sym; symi < meta->numsym; symi++, cursym++){
		if(cursym->type != STT_FUNC) continue;
		//find the instr that i correspond to
		//instrs are sorted
		for(; insi < meta->numins; insi++, ins++) if(cursym->where <= ins->loc) break;
		if(insi >= meta->numins || cursym->where != ins->loc) continue;



		size_t numshims = 0;
		//i line up
		//go through, find any of type 333 (sub rsp) or 8 (add rsp), make sure that every add has at least a sub and vice versa.
		instr_t *in = ins;
		size_t insb = insi;
		for(; insb < meta->numins && in->loc < cursym->where+cursym->size; insb++, in++){
			//also assumes 64 bit mode, otherwise it would be 3
			if(in->len < 4) continue;	//cant be either
			uint8_t *opcode = data + in->loc;
			//we are going to assume 64 bit mode, so the 48 is required
			//todo support not 64 bit mode
			if(opcode[0] != 0x48) continue;
			//has to be sub or add 8 or 32 from r32
			if(opcode[1] != 0x81 && opcode[1] != 0x83) continue;
			//has to operate on rsp
			if(opcode[2] != 0xec && opcode[2] != 0xc4) continue;
			//found one
			//figure out its number
			size_t num = opcode[1] == 0x81 ? *(unsigned int*)(&opcode[3]) : opcode[3];
			//search through the shims we already have to find a matching
			size_t shimmysham;
			for(shimmysham = 0; shimmysham < numshims; shimmysham++) if(shims[shimmysham].refnumber == num) break;
			shim_t * shimmy = shims+shimmysham;
			if(shimmysham >= numshims){	//add a new shim
				if(numshims+1 >sizeshims){
					sizeshims = (numshims+1)*2;
					shims = realloc(shims, sizeshims * sizeof(shim_t));
				}
				shimmy = shims+shimmysham;
				numshims++;
				shimmy->refnumber = num;
				shimmy->hasadd = 0;
				shimmy->hassub = 0;
				shimmy->opbytes = opcode[1] == 0x81 ? 4: 1;
				shimmy->numinslocs = 0;
				shimmy->inslocs = 0;
				shimmy->start = insb;
			}
			//add my location to the shim
			shimmy->numinslocs++;
			shimmy->inslocs = realloc(shimmy->inslocs, shimmy->numinslocs* sizeof(size_t));
			shimmy->inslocs[shimmy->numinslocs-1] = insb;
			if(insb < shimmy->start) shimmy->start = insb;	//update start
			//adjust hasadd or hassub
			if(opcode[2] == 0xc4)
				shimmy->hasadd = 1;
			else
				shimmy->hassub = 1;
			//adjust opbytes if needed, only lower
			if(opcode[1] == 0x83) shimmy->opbytes = 1;
		}

		//lets sort them
		qsort(shims, numshims, sizeof(shim_t), shim_comparestart);

		//we should have a list of shims, lets make some patches from them
		size_t shimmysham;
		shim_t *shim;
		for(shim = shims, shimmysham = 0; shimmysham < numshims; shimmysham++, shim++){
			//TODO have some printfs here for debugging/verbose
			//some checks
			if(!shim->hasadd || !shim->hassub || !shim->numinslocs || !shim->inslocs){
				if(shim->inslocs) free(shim->inslocs);
				continue;
			}

			//loop through ops, adjusting them
			size_t j;
			for(j = 0; j < shim->numinslocs; j++){
				size_t insind = shim->inslocs[j];
				instr_t *in = ins + insind;
				uint8_t *opcode = data + in->loc;

			//lets insert a buncha nops right before the instr

				mod_t m = {insind, in->loc, 0, 1, 0, NULL};
				if(opcode[2] == 0xc4){
					//todo double check that there is space...
					instr_t *in2 = in+1;
					m.insind = insind+1;
					m.loc = in2->loc;
					m.size = sizeof(hex_text_remove_shim)-1;
					m.data = hex_text_remove_shim;
					//ok, now we need to add a rela in there
//					add_rela(&elf->metas[relaindx], 0, in->loc + 20, -4);
//todo figure out how to gt relas INTO the middle of a mod_t...
//probably could insert some list into the mod_t of relocs to add in...
//or i could add a list of "adjustments" to make to various relocs, set the reloc to be at in->loc or wharever, and then it would run through the list post reloc_fixup and add or subtract to the reloc offset
					printf("Add %li\n", m.size);
				} else {
					m.size = sizeof(hex_text_insert_shim)-1;
					m.data = hex_text_insert_shim;
					printf("Sub %li \n", m.size);
				}
				patch_addmod(p, m);


			}
			//clean up allocated space
			if(shim->inslocs) free(shim->inslocs);
		}
	}
	//clean up;
	if(shims) free(shims);
	shims = 0;
	if(!p->nummods){
		clean_patch(p);
		free(p);
		return 0;
	}
	if(calculate_patch(meta, p) <0){
		printf("generate dynshims failed at patch calculation\n");
		clean_patch(p);
		free(p);
		return -1;
	}

	return 0;

}




//todo possilbe improvement is the ability to change a stack shim instruction to allow more shimming
//would also require support in fpatch, im not sure
int applystackshim(analyze_section_metadata_t *meta){
	struct elf_file_metadata *elf = meta->elf;
	struct elf_section_header_64 * section;
	get_section(elf, meta->section, &section);

	size_t datasize = section->size;
	uint8_t * data = elf->base_ptr + section->offset;
	if(meta->data) data = meta->data;
	if(meta->data) datasize = meta->datasize;

	patch_t *bigpatch = calloc(1, sizeof(patch_t));


	patch_t p = {0};
	//for each executable section

	instr_t *ins = meta->ins;
	size_t insi = 0;
	symbol_t *cursym;
	size_t symi;
	//symbols are sorted
	shim_t * shims = 0;
	size_t sizeshims = 0;
	for(symi = 0, cursym = meta->sym; symi < meta->numsym; symi++, cursym++){
		if(cursym->type != STT_FUNC) continue;
		//find the instr that i correspond to
		//instrs are sorted
		for(; insi < meta->numins; insi++, ins++) if(cursym->where <= ins->loc) break;
		if(insi >= meta->numins || cursym->where != ins->loc) continue;

		size_t symstart = insi;


		size_t numshims = 0;
		//i line up
		//go through, find any of type 333 (sub rsp) or 8 (add rsp), make sure that every add has at least a sub and vice versa.
		instr_t *in = ins;
		size_t insb = insi;
		for(; insb < meta->numins && in->loc < cursym->where+cursym->size; insb++, in++){
			//also assumes 64 bit mode, otherwise it would be 3
			if(in->len < 4) continue;	//cant be either
			uint8_t *opcode = data + in->loc;
			//we are going to assume 64 bit mode, so the 48 is required
			//todo support not 64 bit mode
			if(opcode[0] != 0x48) continue;
			//has to be sub or add 8 or 32 from r32
			if(opcode[1] != 0x81 && opcode[1] != 0x83) continue;
			//has to operate on rsp
			if(opcode[2] != 0xec && opcode[2] != 0xc4) continue;
			//found one
			//figure out its number
			size_t num = opcode[1] == 0x81 ? *(unsigned int*)(&opcode[3]) : opcode[3];
			//search through the shims we already have to find a matching
			size_t shimmysham;
			for(shimmysham = 0; shimmysham < numshims; shimmysham++) if(shims[shimmysham].refnumber == num) break;
			shim_t * shimmy = shims+shimmysham;
			if(shimmysham >= numshims){	//add a new shim
				if(numshims+1 >sizeshims){
					sizeshims = (numshims+1)*2;
					shims = realloc(shims, sizeshims * sizeof(shim_t));
				}
				shimmy = shims+shimmysham;
				numshims++;
				shimmy->refnumber = num;
				shimmy->hasadd = 0;
				shimmy->hassub = 0;
				shimmy->opbytes = opcode[1] == 0x81 ? 4: 1;
				shimmy->numinslocs = 0;
				shimmy->inslocs = 0;
				shimmy->start = insb;
			}
			//add my location to the shim
			shimmy->numinslocs++;
			shimmy->inslocs = realloc(shimmy->inslocs, shimmy->numinslocs* sizeof(size_t));
			shimmy->inslocs[shimmy->numinslocs-1] = insb;
			if(insb < shimmy->start) shimmy->start = insb;	//update start
			//adjust hasadd or hassub
			if(opcode[2] == 0xc4)
				shimmy->hasadd = 1;
			else
				shimmy->hassub = 1;
			//adjust opbytes if needed, only lower
			if(opcode[1] == 0x83) shimmy->opbytes = 1;
		}
		//lets sort them
		qsort(shims, numshims, sizeof(shim_t), shim_comparestart);

		//we should have a list of shims, lets apply them
		size_t shimmysham;
		shim_t *shim;
		for(shim = shims, shimmysham = 0; shimmysham < numshims; shimmysham++, shim++){
			size_t j;
			//TODO have some printfs here for debugging/verbose
			//some checks
			if(!shim->hasadd || !shim->hassub || !shim->numinslocs || !shim->inslocs){
				if(shim->inslocs) free(shim->inslocs);
				shim->numinslocs = 0;
				continue;
			}


			//find how much to bump
			ssize_t bcount = count_pushbeforesave(meta, symstart, shim->start);


			//lets fixup the stack accesses in this shim
			size_t endins = insb;
			if(shimmysham+1 < numshims)
				endins = shim[1].start;	//make sure we dont run into the next shim!


			int res = fixup_frame(&p, meta, shim->start, endins, shim->refnumber + bcount);
			if(res<0){	//unable to fix it up
				printf("Error fixing up the stack frame, not putting a shim here!\n");
				clean_patch(&p);
				continue;
			}
			//fixup things BEFORE the shim happens (gcc will sometimes reorder oddly)
			res = fixup_frame_before(&p, meta, symstart, shim->start, shim->refnumber + bcount);
			if(res<0){	//unable to fix it up
				printf("Error fixing up the stack frame before, not putting a shim here!\n");
				clean_patch(&p);
				continue;
			}
			//todo alternative method to just modify them based on static shim size


			//calculate a random shim amount
			//TODO see if
//TODO figure out what i meant by ^^

			//TODO change this to a replacement patch
			size_t shim_max = (shim->opbytes == 4 ? INT_MAX : SCHAR_MAX) - shim->refnumber;
			if(shim_max > options.shimsize) shim_max = options.shimsize;
			size_t shim_amount = (rand() % shim_max);
			shim_amount -= shim_amount%options.shimalign;
			size_t shim_new = shim->refnumber + shim_amount;
			//loop through ops, adjusting them
			for(j = 0; j < shim->numinslocs; j++){
				instr_t *in = meta->ins + shim->inslocs[j];
				uint8_t *opcode = data + in->loc;
				if(in->len < 4){
					printf("%s:%i", __FILE__, __LINE__);
					printf(" ERROR shim is broken? shimi: %li, insind: %li, numinsind: %li, datasize: %li, loc:%li, len:%i\n", j, shim->inslocs[j], meta->numins, datasize, in->loc, in->len);
					continue;
				}
				if(opcode[1] == 0x81) *(unsigned int*)(&opcode[3]) = shim_new;
				else opcode[3] = shim_new;
			}
			//clean up allocated space
			if(shim->inslocs) free(shim->inslocs);
			//append the patch
			if(p.nummods)print_patch(&p);
			patch_addpatch(bigpatch, &p);
			clean_patch(&p);
		}
	}
	//clean up;
	if(shims) free(shims);
	shims = 0;


	clean_patch(&p);

	if(bigpatch->nummods)print_patch(bigpatch);
	if(!bigpatch->nummods){
		clean_patch(bigpatch);
		free(bigpatch);
		return 0;
	}
	if(calculate_patch(meta, bigpatch) <0){
		printf("generate shims failed at patch calculation\n");
//		clean_patch(bigpatch);
//		free(bigpatch);
		return -1;
	}
	return 0;
}
