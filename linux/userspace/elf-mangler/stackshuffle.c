#include <stdio.h>
#include <stdlib.h>



#include <r_anal.h>

#include <string.h>

#include "parser.h"
#include "analyze.h"

#include "stackshuffle.h"

//find the stack regs that the leave either reads or writes to
uint64_t leave_getstackregs(analyze_section_metadata_t * meta, leave_t *l){
	//basically look for push/pops, or together their regs
	uint64_t regs = 0;
	size_t i;
	for(i = 0; i < l->numshins; i++){
		instr_t ins = meta->ins[(l->instrind-1)-i];
		if(ins.id == 566 || ins.id ==588)
			regs |= ins.regs;
	}
	l->regs = regs;
	return regs;
}

//find the stack regs that the entry either reads or writes to
uint64_t entry_getstackregs(analyze_section_metadata_t * meta, entry_t *e){
	//basically look for push/pops, or together their regs
	uint64_t regs = 0;
	size_t i;
	for(i = 0; i < e->numshins; i++){
		instr_t ins = meta->ins[e->instrind+i];
		if(ins.id == 566 || ins.id ==588)
			regs |= ins.regs;
	}
	e->regs = regs;
	return regs;
}

//compare an entry to its leave(s), truncate entry if it overlaps
//if i have push/pops on registers that the leaves dont, truncate to match the leaves
//returns if it truncated
//todo verbosity options
int entry_truncate(analyze_section_metadata_t * meta, entry_t *e){
	uint64_t leaveregs = 0;
	size_t lind;
	leave_t *l;
	for(lind = e->leavelist; lind; lind = l->nextleave){
//		printf("leaf %li\n", lind-1);
		l = &meta->leaves[lind-1];
		//is possible to have 0 for regs, but unlikely and in that case, probably extremely easy to calculate (just nops)
		if(!l->regs) leave_getstackregs(meta, l);
		if(lind == e->leavelist) leaveregs = l->regs;
		else leaveregs &= l->regs;
		//all of the leaves have to have them, whats why we & instead of |
	}
	//printregs(leaveregs);
//	printf("\n");
	uint64_t badregs = ~leaveregs; //the registers we arent allowed to have



	uint64_t tregs = 0;	//used for adjusting regs after i truncate
	size_t i;
	for(i = 0; i < e->numshins; i++){
		instr_t ins = meta->ins[e->instrind+i];
		if(ins.id == 566 || ins.id ==588){
			if(ins.regs & badregs)break;
			tregs |= ins.regs;
		}
	}
	if(i == e->numshins) return 0;	//all set
	//truncate it
	e->numshins = i;
	e->shins = realloc(e->shins, e->numshins * sizeof(shuffleinstr_t));
	//reset my regs
	e->regs = tregs;

	return 1;
}

//compare a leave to its entry, truncate leave if it has too much
//if i have push/pops on registers that the entry does not, truncate to match
//returns if it truncated
int leave_truncate(analyze_section_metadata_t * meta, leave_t *l){
	entry_t *e = &meta->entries[l->entryind];
	//todo an optimization could be cache these somewhere
	if(!e->regs) entry_getstackregs(meta, e);
	uint64_t badregs = ~e->regs;	//the registers we arent allowed to have

	uint64_t tregs = 0;		//used for adjusting regs after I truncate
	size_t i;
	for(i = 0; i < l->numshins; i++){
		instr_t ins = meta->ins[l->instrind-1-i];
		if(ins.id == 566 || ins.id ==588){
			if(ins.regs & badregs)break;
			tregs |= ins.regs;
		}
	}
	if(i == l->numshins) return 0;	//all set
	//truncate it
	l->numshins = i;
	l->shins = realloc(l->shins, l->numshins * sizeof(shuffleinstr_t));
	//reset my regs (quicker than doing the whole getstackregs on it)
	l->regs = tregs;

	return 1;
}


//run the two above truncate functions until both of them return no changes
int dotrunc(analyze_section_metadata_t *meta){
	size_t i;
	for(i = 0; i < meta->numentries; i++){
//		printf("doing entry %li\n", i);
		entry_t *e = &meta->entries[i];
		int tresult = 1;
		while(tresult){
			tresult = entry_truncate(meta, e);
			size_t leaveind;
			leave_t *l;
			for(leaveind = e->leavelist; leaveind; leaveind = l->nextleave){
				l = &meta->leaves[leaveind-1];
				tresult |= leave_truncate(meta, l);
			}
		}
	}
	//if we had any untruncated leaves by this point, that would be some sort of error...(eg, there exists a leave that is not in an entry's leavelist) TODO look into this?
	return 0;
}

//calculate dependencies for the instructions in an entry
//start at the last instruction, work backwards
int entry_calcdeps(analyze_section_metadata_t *meta, entry_t *e){
	ssize_t i;
	for(i = e->numshins-1; i >= 1; i--){
		shuffleinstr_t *shin = &e->shins[i];
		instr_t ins = meta->ins[shin->instrind];
		//we ignore rsp (7) here becase we dont include RSP modifying things aside from push and pops in our ranges
		uint64_t dregs = ins.regs & ~(1<<7);
		if(!dregs) continue;		//no registers touched? (nop maybe?)
		shin->deps = calloc(i, sizeof(size_t));
		ssize_t j;	//signed because >= 0
		for(j = i-1; j >= 0; j--){
			shuffleinstr_t sham = e->shins[j];
			instr_t bl = meta->ins[sham.instrind];
			if(dregs & bl.regs)	//some of our registers overlap, depended
				shin->deps[shin->numdeps++] = j;
		}
		//truncate down list
		if(shin->numdeps)
			shin->deps = realloc(shin->deps, shin->numdeps * sizeof(size_t));
		else {
			free(shin->deps); shin->deps = 0;
		}
	}
	return 0;
}

//calculate dependencies for the instructions in a leave
//start at the last instruction (right before the reg in the leave) and work backwards
int leave_calcdeps(analyze_section_metadata_t *meta, leave_t *l){
	ssize_t i;
	for(i = 0; i+1 < l->numshins; i++){
		shuffleinstr_t *shin = &l->shins[i];
		//regs should have been set up when we truncated
		instr_t ins = meta->ins[shin->instrind];
		//we ignore rsp (7) here becase we dont include RSP modifying things aside from push and pops in our ranges
		uint64_t dregs = ins.regs & ~(1<<7);
		if(!dregs) continue;		//no registers touched? (nop maybe?)
		shin->deps = calloc(l->numshins-1-i, sizeof(size_t));
		ssize_t j; //signed because consistency
		for(j = i+1; j < l->numshins; j++){
			shuffleinstr_t sham = l->shins[j];
			instr_t bl = meta->ins[sham.instrind];
			if(dregs & bl.regs)	//some of our registers overlap, depended
				shin->deps[shin->numdeps++] = j;
		}
		//truncate down list
		if(shin->numdeps)
			shin->deps = realloc(shin->deps, shin->numdeps * sizeof(size_t));
		else {
			free(shin->deps); shin->deps = 0;
		}
	}
	return 0;
}

//little ease-of-use function for the two calcdeps above
int calcdeps(analyze_section_metadata_t *meta){
	size_t i;
	for(i = 0; i < meta->numentries; i++){
		entry_calcdeps(meta, &meta->entries[i]);
	}
	for(i = 0; i < meta->numleaves; i++){
		leave_calcdeps(meta, &meta->leaves[i]);
	}
	return 0;
}



//used for qsort
int entries_comparelocs(const void * a, const void *b){
	return ((entry_t *)a)->instrind - ((entry_t*)b)->instrind;
}
int leaves_comparelocs(const void * a, const void *b){
	return ((leave_t *)a)->instrind - ((leave_t*)b)->instrind;
}



//find all symbol references to an instruction in this section, add it to the entrylist
//todo (maybe) adjust this so it does all sections at once, and maaybe checks if the symbol is referenced by a reloc from RIP modifying ins?
//todo could use some cleanup and optimization (one optimization is do all sections at once)
size_t findentries(analyze_section_metadata_t *meta){
	struct elf_file_metadata *elf = meta->elf;
	struct elf_section_header_64 * section;
	get_section(elf, meta->section, &section);

	struct elf_section_header_64 * tmpsec = NULL;
	size_t i;
	size_t sz = meta->numentries;
	for(i = 0; i < elf->numsechdrs; i++){
		//todo errorcheck?
		get_section(elf, i, &tmpsec);
		if(tmpsec->type != SHT_SYMTAB) continue;

		struct elf_symtab_entry *sym = (struct elf_symtab_entry *)(elf->base_ptr + tmpsec->offset);
		size_t count = tmpsec->size / sizeof(struct elf_symtab_entry);
		size_t j;
		for(j = 0; j < count; j++, sym++){
			if(sym->shidx != meta->section) continue;
			//we have a location in our section, gotta find the instruction that corresponds to it
			size_t loc = sym->value;
			//yes, 3 nested for loop, kinda gross
			//todo potential optimization is binary search
			size_t k;
			for(k = 0; k < meta->numins && meta->ins[k].loc != loc; k++);
			if(k == meta->numins){
			//todo verbosity levels
//				printf("Findentries: found a symbol that doesn't correspond to an instruction\n");
				continue;
			}
			//make sure there isnt already an entry at this spot
			//cant really optimize based on sorting, unless we use an insertion sort to sort on the fly
			size_t h;
			for(h = 0; h < meta->numentries && meta->entries[h].instrind != k; h++);
			if(h != meta->numentries) continue; ///already exits an entry here
			//do the whole realloc thing
			if(meta->numentries+1 > sz){
				sz = 2*(meta->numentries+1);
				meta->entries = realloc(meta->entries, sz * sizeof(entry_t));
			}
			entry_t *ent = &meta->entries[meta->numentries++];
			ent->instrind = k;
			ent->shins = 0;
			ent->regs = 0;
			ent->shuffy = 0;
			ent->numshins = ent->leavelist = 0;
		}
	}
	//compact
	meta->entries = realloc(meta->entries, meta->numentries * sizeof(entry_t));
	//sort (todo may change to an insertion sort that happens as we enter them)
	qsort(meta->entries, meta->numentries, sizeof(entry_t), entries_comparelocs);
	return meta->numentries;
}


//go through the list of leaves and associate each leave to an entry
//right now it just chooses the closest entry that comes before the leave
//this should cover just about anything generated by a compiler. This wont work if a function is handcoded such that its ret is BEFORE the function entry
//todo (maybe) a more advanced solution would be follow jumps unconditionally and find which rets they hit
int leaves_find_entry(analyze_section_metadata_t *meta){
	size_t li = 0;
	size_t ei = 0;
	leave_t *l = meta->leaves;
	entry_t * e = meta->entries;
	for(li = 0; li < meta->numleaves; li++, l++){
		for(;ei< meta->numentries && e->instrind < l->instrind; ei++, e++);
		l->entryind = ei > 0 ? ei-1: 0;
		//add myself to the leavelist
		entry_t *e = &meta->entries[l->entryind];
		l->nextleave = e->leavelist;
		e->leavelist=li+1;
	}
	return 0;
}


//early way to find the instructions that we have the ability to shuffle (no badzone edges, no rip modifiers, etc)
int findleaveshufflerange(analyze_section_metadata_t *meta, leave_t *l){
	size_t instrofs = l->instrind-1;
	size_t collide = 0; //so technically we cant get a leave that goes all the way to an entry

	entry_t *e = &meta->entries[l->entryind];
	//find collisions (entry, other leaves)
	if(e->instrind+e->numshins > collide && e->instrind < instrofs) collide = e->instrind + e->numshins;
	size_t lcind;
	leave_t lc;
	for(lcind = e->leavelist; lcind; lcind = lc.nextleave){
		leave_t *lcp = &meta->leaves[lcind-1];
		lc = *lcp;
		if(lcp == l) continue; //dont collide with self
		if(lc.instrind > collide && lc.instrind-lc.numshins < instrofs) collide = lc.instrind;
	}

	for(; instrofs > collide; instrofs--){
		instr_t *ins = &meta->ins[instrofs];
		//lets calculate the regs while we are here
		if(!ins->regs)ins_getregs(ins);
		//loop through badsections, find if this instruction is on the edge
		//gross and unoptimized
		size_t k;										//the +1 is ok, because we start one before our ret inst
		for(k = 0; k < meta->numbs; k++) if(meta->bs[k].start == ins->loc || meta->bs[k].end == (ins+1)->loc) break;
		if(k != meta->numbs) break;//STOP

		//check if i reference rip
		if(ins->userip) break;//STOP
		//todo use ins->regs to do this


		//if i have a reloc to me
		//todo in the future adjust relocs with shuffles
		if(ins->hasreloc) break;

		//memory writes that arent stack ones
		if(ins->memwrite && !ins->usersp) break;

		//not a pop, but uses the stack
		if(ins->type != R_ANAL_OP_TYPE_POP && ins->usersp)break;//STOP
	}
	//make sure that we dont try to allocate -1 (unsigned) TODO does this ever happen? this check might not be needed now
	l->numshins = l->instrind-1 > instrofs ? (l->instrind-1) - (instrofs) : 0;

//	allocate my shins
	if(l->numshins)l->shins = calloc(l->numshins, sizeof(shuffleinstr_t));
	size_t i;
	for(i=0; i < l->numshins; i++){
		shuffleinstr_t *sh = &l->shins[i];
		sh->instrind = (l->instrind-1)-i;
	}
	return 0;
}


//early way to find the instructions that we have the ability to shuffle (no badzones, no rip modifiers, etc)
int findentryshufflerange(analyze_section_metadata_t *meta, entry_t *e){
	size_t instrofs = e->instrind;
	size_t collide = meta->numins;
	//find collisions (leaves)
	size_t lcind;
	leave_t lc;
	for(lcind = e->leavelist; lcind; lcind = lc.nextleave){
		lc = meta->leaves[lcind-1];
		if(lc.instrind-lc.numshins < collide && lc.instrind > instrofs) collide = lc.instrind-lc.numshins;
	}

	for(; instrofs < collide; instrofs++){
		instr_t *ins = &meta->ins[instrofs];
		if(!ins->regs)ins_getregs(ins);
		//loop through badsections, find if this instruction is on the edge
		//gross and unoptimized
		size_t k;
		for(k = 0; k < meta->numbs; k++) if(meta->bs[k].start == ins->loc || meta->bs[k].end == ins->loc) break;
		if(k != meta->numbs) break;//STOP

		//check if i reference rip
		if(ins->userip) break;//STOP
		//if i have a reloc to me
		//todo in the future adjust relocs with shuffles
		if(ins->hasreloc) break;
		//memory writes that arent stack ones
		if(ins->memwrite && !ins->usersp) break;

		//not a push, but uses the stack
		if(ins->type != R_ANAL_OP_TYPE_PUSH && ins->type != R_ANAL_OP_TYPE_RPUSH && ins->type != R_ANAL_OP_TYPE_UPUSH && ins->usersp)break;//STOP
	}
	e->numshins = instrofs - e->instrind;

//	allocate my shins
	if(e->numshins) e->shins = calloc(e->numshins, sizeof(shuffleinstr_t));
	size_t i;
	for(i=0; i < e->numshins; i++){
		shuffleinstr_t *sh = &e->shins[i];
		sh->instrind = e->instrind+i;
	}

	return 0;
}


//must be done before the other two
int findentryshuffleranges(analyze_section_metadata_t *meta){
	size_t i;
	for(i=0; i < meta->numentries; i++){
		findentryshufflerange(meta, &meta->entries[i]);
	}
	return 0;
}

int findleaveshuffleranges(analyze_section_metadata_t *meta){
	size_t i;
	for(i=0; i < meta->numleaves; i++){
		findleaveshufflerange(meta, &meta->leaves[i]);
	}
	return 0;
}



//shuffleinstr_t destructor
int clean_shin(shuffleinstr_t * shin){
	if(shin->deps){ free(shin->deps); shin->deps = 0;} shin->numdeps = 0;
	return 0;
}
//leave_t destructor
int clean_leave(leave_t *l){
	if(l->shins){
		size_t i;
		for(i = 0; i < l->numshins; i++) clean_shin(&l->shins[i]);
		free(l->shins);
		l->shins = 0;
		l->numshins = 0;
	}
	if(l->shuffy){free(l->shuffy); l->shuffy = 0;}
	return 0;
}
//etnry_t destructor
int clean_entry(entry_t *e){
	if(e->shins){
		size_t i;
		for(i = 0; i < e->numshins; i++) clean_shin(&e->shins[i]);
		free(e->shins);
		e->shins = 0;
		e->numshins = 0;
	}
	if(e->shuffy){free(e->shuffy); e->shuffy = 0;}
	return 0;
}


//i should be ready to shuffle
int shuffle_entry(analyze_section_metadata_t *meta, entry_t *e){
	if(e->shuffy)free(e->shuffy);
	e->shuffy = calloc(e->numshins, sizeof(size_t));
	size_t *cantshuf = calloc(e->numshins, sizeof(size_t));
	size_t *shufbuf = calloc(e->numshins, sizeof(size_t));
	size_t i;
	size_t stackloc = 1; // 0 means it doesnt touch stack
	for(i = 0; i < e->numshins;){
		size_t shufbufsize = 0;
		//rebuild shufbuf
		size_t j;
		for(j = 0; j < e->numshins; j++){
			if(cantshuf[j])continue;
			shuffleinstr_t *shin = &e->shins[j];
			//check deps
			size_t d;
			for(d = 0; d < shin->numdeps; d++){
				size_t dep = shin->deps[d];
				size_t k;
				for(k =0; k < i && dep != e->shuffy[k]; k++);
				if(k ==i) break;//got all the way through without satisfying a dep
			}
			if(d != shin->numdeps) continue;	//was not able to satisfy everything
			//looks like this guy is doable
			shufbuf[shufbufsize++] = j;
		}
		if(!shufbufsize){
			printf("Fatal error, exhausted shufbuf!\n");
			// FATAL error none able to be entered!
			//i guess restart and try again?
			memset(cantshuf, 0, e->numshins * sizeof(size_t));
			i=0;
			stackloc = 0;
			continue;
		}
		//choose a random one!
		size_t indx = shufbuf[rand()%shufbufsize];
		cantshuf[indx] = 1;
		e->shuffy[i] = indx;
		//set stackindx
		//todo could also just loop through afterwards if needed
		shuffleinstr_t *shin = &e->shins[indx];
		instr_t in = meta->ins[shin->instrind];		//todo change this to arithmatic?
		if(in.id == 566 || in.id ==588){
			shin->stackorder = stackloc++;
		}
//		printf("%li ", e->shuffy[i]);
		i++;
	}
	if(cantshuf)free(cantshuf);
	if(shufbuf)free(shufbuf);


//	printf("\n");
	return 0;
}


//todo do i need to check that there is only one stack op per register in a leave/entry?
int shuffle_leave(analyze_section_metadata_t *meta, leave_t *l){
	if(l->shuffy)free(l->shuffy);
	l->shuffy = calloc(l->numshins, sizeof(size_t));
	size_t *cantshuf = calloc(l->numshins, sizeof(size_t));
	size_t *shufbuf = calloc(l->numshins, sizeof(size_t));


	size_t maxstackorder = 0;
	//"copy" over my stack order for the associated push and pop
	entry_t *e = &meta->entries[l->entryind];
	size_t li;
	size_t ei;
	for(li = 0, ei = 0; li < l->numshins && ei < e->numshins;li++, ei++){
		//if not a stackthing, just go ahead a little
		while(ei < e->numshins){
			instr_t in = meta->ins[e->shins[ei].instrind];		//todo change this to arithmatic?
			if(in.id == 566 || in.id ==588)break;		//stop on a stackop
			ei++;
		}
		while(li < l->numshins){
			instr_t in = meta->ins[l->shins[li].instrind];		//todo change this to arithmatic?
			if(in.id == 566 || in.id ==588)break;		//stop on a stackop
			li++;
		}
		if(li >= l->numshins || ei >= e->numshins) break;	//error? or expected?
		//ok, copy the stack order value (both ei and li are on stack ops)
		l->shins[li].stackorder = e->shins[ei].stackorder;
		if(maxstackorder < l->shins[li].stackorder) maxstackorder = l->shins[li].stackorder;
	}



	ssize_t i;
	//have to do this backwards
	for(i = l->numshins-1; i >= 0;){
		size_t shufbufsize = 0;
		//rebuild shufbuf
		size_t j;
		for(j = 0; j < l->numshins; j++){
			if(cantshuf[j])continue;
			shuffleinstr_t *shin = &l->shins[j];
			//check stackorder
			if(shin->stackorder && shin->stackorder != maxstackorder) continue;
			//check deps
			size_t d;
			for(d = 0; d < shin->numdeps; d++){
				size_t dep = shin->deps[d];
				ssize_t k;
				for(k = l->numshins-1; k > i && dep != l->shuffy[k]; k--);
				if(k == i) break;//got all the way through without satisfying a dep
			}
			if(d != shin->numdeps) continue;	//was not able to satisfy everything
			//looks like this guy is doable
			shufbuf[shufbufsize++] = j;
		}
		if(!shufbufsize){
			printf("Fatal error, exhausted shufbuf!\n");
			// FATAL error none able to be entered!
			//i guess restart and try again?
			memset(cantshuf, 0, l->numshins * sizeof(size_t));
			i=0;
			continue;
		}
		//choose a random one!
		size_t indx = shufbuf[rand()%shufbufsize];
		cantshuf[indx] = 1;
		l->shuffy[i] = indx;
		if(l->shins[indx].stackorder) maxstackorder--;	//this instruction is a stack one, we have to "pop" the stack

//		printf("%li ", l->shuffy[i]);
		i--;
	}
	if(cantshuf)free(cantshuf);
	if(shufbuf)free(shufbuf);


	printf("\n");
	return 0;
}



int apply_entry(analyze_section_metadata_t * meta, entry_t *e){
	if(!e->numshins) return -1;

	struct elf_section_header_64 * section;
	get_section(meta->elf, meta->section, &section);

	//todo fiogure this out for good
	instr_t te = meta->ins[e->instrind];
	size_t start = te.loc;
	size_t end = te.loc+te.len;

	size_t j;
	for(j = 0; j < e->numshins; j++){
		te = meta->ins[e->shins[j].instrind];
		if(te.loc < start) start = te.loc;
		if(te.loc+te.len > end) end = te.loc+te.len;
	}


	size_t outofs = start;
	uint8_t *outdata = meta->elf->base_ptr + section->offset;
	if(meta->data) outdata = meta->data;
	uint8_t *indata = malloc(end-start);
	memcpy(indata, outdata+outofs, end-start);

	ssize_t inofs = -start;
	//copy it over in the order we shuffled
	size_t i;
	for(i = 0; i < e->numshins; i++){
		instr_t in = meta->ins[e->shins[e->shuffy[i]].instrind];
		memcpy(outdata + outofs, (indata + in.loc) + inofs, in.len);
		outofs += in.len;
	}

	//free the temp
	if(indata)free(indata);
	return 0;
}

//does not work? not sure
int fixup_entry(analyze_section_metadata_t * meta, entry_t *e){
	if(!e->numshins) return -1;
	//fix up the ins we shuffled
	instr_t *tins = calloc(e->numshins, sizeof(instr_t));
	memcpy(tins, meta->ins + e->shins[0].instrind, e->numshins * sizeof(instr_t));
	size_t i;
	for(i = 0; i < e->numshins; i++){
		instr_t * to = &meta->ins[e->shins[e->shuffy[i]].instrind];
		instr_t * from = &tins[i];
		*to = *from;
	}
	if(tins)free(tins);
	//lightly clean up the thing now
	return 0;
}


int apply_leave(analyze_section_metadata_t * meta, leave_t *l){
	if(!l->numshins) return -1;

	struct elf_section_header_64 * section;
	get_section(meta->elf, meta->section, &section);

	//todo fiogure this out for good
	instr_t te = meta->ins[l->instrind];
	size_t start = te.loc;
	size_t end = te.loc+te.len;

	size_t j;
	for(j = 0; j < l->numshins; j++){
		te = meta->ins[l->shins[j].instrind];
		if(te.loc < start) start = te.loc;
		if(te.loc+te.len > end) end = te.loc+te.len;
	}

	size_t outofs = start;
	uint8_t *outdata = meta->elf->base_ptr + section->offset;
	if(meta->data) outdata = meta->data;
	uint8_t *indata = malloc(end-start);
	memcpy(indata, outdata+outofs, end-start);

	ssize_t inofs = -start;
	//copy it over in the order we shuffled
	ssize_t i;
	for(i = l->numshins-1; i >= 0; i--){
		instr_t in = meta->ins[l->shins[l->shuffy[i]].instrind];
		memcpy(outdata + outofs, indata + in.loc + inofs, in.len);
		outofs += in.len;
	}


	//free temp
	if(indata)free(indata);
	return 0;
}


//does not work? not sure
int fixup_leave(analyze_section_metadata_t *meta, leave_t *l){
	if(!l->numshins) return -1;
	//fix up the ins we shuffled
	instr_t *tins = calloc(l->numshins, sizeof(instr_t));
	memcpy(tins, meta->ins + l->shins[l->numshins-1].instrind, l->numshins * sizeof(instr_t));
	size_t i;
	for(i = 0; i < l->numshins; i++){
		instr_t * to = &meta->ins[l->shins[l->shuffy[i]].instrind];
		instr_t * from = &tins[l->numshins-1-i];
		*to = *from;
	}
	if(tins)free(tins);
	return 0;
}



int calcshuffle(analyze_section_metadata_t *meta){
	size_t i;
	for(i = 0; i < meta->numentries; i++){
		printf("calculating shuffle for entry %li\n", i);
		shuffle_entry(meta, &meta->entries[i]);
	}
	for(i = 0; i < meta->numleaves; i++){
		printf("shuffling shuffle for leave %li\n", i);
		shuffle_leave(meta, &meta->leaves[i]);
	}
	return 0;
}

int applyshuffle(analyze_section_metadata_t *meta){
	size_t i;
	for(i = 0; i < meta->numentries; i++){
		printf("applying shuffle entry %li\n", i);
		apply_entry(meta, &meta->entries[i]);
	}
	for(i = 0; i < meta->numleaves; i++){
		printf("applying shuffle leave %li\n", i);
		apply_leave(meta, &meta->leaves[i]);
	}
	meta->isanalyzed = 0;	//we are modifying the instructions, so we invalidate the analysis
	//todo only set it if we actually modified something
/*
	for(i = 0; i < meta->numentries; i++){
		printf("fixing shuffle entry %li\n", i);
		fixup_entry(meta, &meta->entries[i]);
	}
	for(i = 0; i < meta->numleaves; i++){
		printf("fixing shuffle leave %li\n", i);
		fixup_leave(meta, &meta->leaves[i]);
	}
*/
	return 0;
}


int calcentireshuffle(analyze_section_metadata_t *meta){
	findentries(meta);
	leaves_find_entry(meta);
	findentryshuffleranges(meta);
	findleaveshuffleranges(meta);
	dotrunc(meta);
	calcdeps(meta);
	calcshuffle(meta);
	return 0;
}
