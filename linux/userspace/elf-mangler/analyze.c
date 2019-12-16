#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include <inttypes.h>


#include <r_anal.h>
#include <r_asm.h>

#include "parser.h"
#include "analyze.h"
#include "cruftable.h"

#include "stackshuffle.h"
#include "registers.h"

#include "options.h"

#include "fpatch.h"	//for cleanup


RAnal * anal = 0;
RAsm * rasm = 0;
void analyze_shutdown(void){
	if(anal)r_anal_free(anal);
	if(rasm) r_asm_free(rasm);
	anal = 0;
	rasm = 0;
}
void analyze_init(void){
	if(!anal){
		anal = r_anal_new();
		r_anal_use(anal, R_SYS_ARCH);
		r_anal_set_bits(anal,64);
	}
	if(!rasm){
		rasm = r_asm_new();
		r_asm_use(rasm, R_SYS_ARCH);
		r_asm_use_assembler(rasm, "x86.nasm");	//temporary solution because of https://github.com/radare/radare2/issues/10689
		r_asm_set_bits(rasm, 64);
	}
}

char * forcegetmnemonic(analyze_section_metadata_t * meta, instr_t *ins){
	struct elf_section_header_64 * section;
	get_section(meta->elf, meta->section, &section);
	uint8_t * data = meta->elf->base_ptr + section->offset;
	if(!rasm) analyze_init();
	RAsmOp rop = {0};
	r_asm_disassemble(rasm, &rop, data + ins->loc, ins->len);
	ins->mnemonic = strdup(r_strbuf_get(&rop.buf_asm));
	return ins->mnemonic;
}

//untested
char * forceanalyze(analyze_section_metadata_t * meta, instr_t *ins){
		RAnalOp aop = {0};
		struct elf_section_header_64 * section;
		get_section(meta->elf, meta->section, &section);
		uint8_t * data = meta->elf->base_ptr + section->offset;
		uint8_t datasize = section->size;
		if(meta->data){
			data = meta->data;
			datasize = meta->datasize;
		}
		if(!anal) analyze_init();
		if(r_anal_op(anal, &aop, ins->loc, data + ins->loc, datasize-ins->loc, R_ANAL_OP_MASK_ESIL) > 0){
			ins->esil = strdup(R_STRBUF_SAFEGET(&aop.esil));

//			ins->mnemonic = strdup(R_STRBUF_SAFEGET(&aop.opex));
//			ins->mnemonic = strdup(aop.mnemonic);
			ins->id = aop.id;
			ins->type = aop.type;
			//todo more analysis?
		}
		r_anal_op_fini(&aop);
		return ins->esil;
}




//todo should i keep track of all the unknown registers?
//todo should i also use a hashtable to speed this up?
#define ISALPHA(char) (((char) >='a' && (char) <= 'z') || ((char) >= 'A' && (char) <= 'Z'))
uint64_t ins_getregs(instr_t *ins){
	if(!ins){
		printf("ERROR: null instruction!\n");
		return 0;
	}
	//try one last time to get the esil
//	if(!ins->esil) forceanalyze(meta,ins);// i dont pass meta... TODO decide if i should
	if(!ins->esil){
		printf("ERROR cant get regs for this ins, no esil!\n");
		return 0;
	}
	//two null bytes at the end for a neat little speedup
	size_t slen = strlen(ins->esil)+1;
	char * tmp = malloc(slen+1);
	memcpy(tmp, ins->esil, slen);
	tmp[slen] = 0;
	char * in, *next;
	ins->regs = 0;
	for(in = tmp; *in; in=next){
		next = strchrnul(in, ',');
		*next = 0;
		if(ISALPHA(*in)){
			registermap_t *r = registermap_find(in);
			if(r)
				ins->regs |= (uint64_t)1 << r->shift;
			else
				printf("Did not find %s register\n", in);
		}
		next++;
	}
	if(tmp)free(tmp);
	return ins->regs;
}
int symbol_compareofs(const void *a, const void *b){
	return ((symbol_t *)a)->where - ((symbol_t*)b)->where;
}
//todo sort for speed?
int findsymbols(analyze_section_metadata_t *meta){
	struct elf_file_metadata *elf = meta->elf;

	struct elf_section_header_64 * section;
	get_section(elf, meta->section, &section);


	struct elf_section_header_64 * tmpsec = NULL;
	if(meta->sym) free(meta->sym);
	meta->sym = 0;
	meta->numsym = 0;
	for(size_t i = 0; i < elf->numsechdrs; i++){
		//todo errorcheck?
		get_section(elf, i, &tmpsec);
		if(tmpsec->type != SHT_SYMTAB) continue;
		struct elf_symtab_entry *sym = (struct elf_symtab_entry*)(elf->base_ptr + tmpsec->offset);
		size_t count = tmpsec->size / sizeof(struct elf_symtab_entry);
		size_t i;

		meta->sym = realloc(meta->sym, (meta->numsym + count) * sizeof(symbol_t));

		for(i = 0; i < count; i++, sym++){
			if(sym->shidx != meta->section) continue;
			meta->sym[meta->numsym].where = sym->value;
			meta->sym[meta->numsym].size = sym->size;
			meta->sym[meta->numsym].type = ELF64_ST_TYPE(sym->info);
			meta->numsym++;
		}
	}

	if(meta->numsym) meta->sym = realloc(meta->sym, meta->numsym * sizeof(symbol_t));
	else if(meta->sym){free(meta->sym); meta->sym = 0;}
	qsort(meta->sym, meta->numsym, sizeof(symbol_t), symbol_compareofs);
	return meta->numsym;

}



//possible optimization is make sure they are sorted for faster checking, todo?
int findrelocs(analyze_section_metadata_t * meta){
	struct elf_file_metadata * elf = meta->elf;

	struct elf_section_header_64 * section;
	get_section(elf, meta->section, &section);


	struct elf_section_header_64 * tmpsec = NULL;
	size_t count = 0;
	for(size_t i = 0; i < elf->numsechdrs; i++){
		//todo errorcheck?
		get_section(elf, i, &tmpsec);
		if(tmpsec->info != meta->section) continue;
		if(tmpsec->type == SHT_RELA){
			size_t bc = count;
			count+= tmpsec->size/sizeof(struct elf64_rela);
			meta->rl = realloc(meta->rl, count * sizeof(size_t));

			struct elf64_rela * rela = (struct elf64_rela *)(elf->base_ptr + tmpsec->offset);
			for(; bc < count; bc++, rela++){
				(meta->rl)[bc].where = rela->offset;
			}
		} else if(tmpsec->type == SHT_REL){
			size_t bc = count;
			count+= tmpsec->size/sizeof(struct elf64_rel);
			meta->rl = realloc(meta->rl, count * sizeof(size_t));

			struct elf64_rel * rel = (struct elf64_rel *)(elf->base_ptr + tmpsec->offset);
			for(; bc < count; bc++, rel++){
				(meta->rl)[bc++].where = rel->offset;
			}
		}
	}
	meta->numrl = count;
	return count;

}

//simple constructor
analyze_section_metadata_t analyze_meta(size_t section, struct elf_file_metadata *elf){
	analyze_section_metadata_t ret = {0};
	ret.elf = elf;
	ret.section = section;

	struct elf_section_header_64 * sec;
	get_section(elf, section, &sec);

	ret.datasize = sec->size;
	char * secname = 0;
	get_section_name(elf, section, &secname);
	ret.section_name = strdup(secname);
	return ret;
}
int print_RRegItem(RRegItem *r, int bump){
	int i;
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("name:\t\t%s\n", r->name);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("type:\t\t%i\n", r->type);	//todo typenames
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("size:\t\t%i\n", r->size);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("offset:\t\t%i\n", r->offset);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("packed_size:\t%i\n", r->packed_size);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("is_float:\t%s\n", r->is_float ? "True" : "False");
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("flags:\t\t%s\n", r->flags);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("index:\t\t%i\n", r->index);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("arena:\t\t%i\n", r->arena);
	return 0;

}

int print_RAnalValue(RAnalValue *v, int bump){
	int i;
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("absolute:\t%i\n", v->absolute);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("memref:\t\t%i\n", v->memref);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("base:\t\t0x%llX\n", v->base);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("delta:\t\t0x%llX\n", v->delta);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("imm:\t\t0x%llX\n", v->imm);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("mul:\t\t%i\n", v->mul);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("sel:\t\t%i\n", v->sel);
	if(v->reg){
		for(i = 0; i < bump;i++) putc('\t', stdout);
		printf("Reg:\n");
		print_RRegItem(v->reg, bump+1);
	}
	if(v->regdelta){
		for(i = 0; i < bump;i++) putc('\t', stdout);
		printf("Regdelta:\n");
		print_RRegItem(v->regdelta, bump+1);
	}
	return 0;
}

int print_RAnalOp(RAnalOp* op, int bump){
	int i;
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("mnemonic:\t%s\n", op->mnemonic);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("addr:\t\t0x%llX\n", op->addr);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("type:\t\t%u\n", op->type);	//todo get the type name
	for(i = 0; i < bump;i++) putc('\t', stdout);
	//temmp disable this because issues with the format of op->prefix being changed between r2 versions
//	printf("prefix:\t\t0x%uX\n", op->prefix);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("type2:\t\t%u\n", op->type2);	//todo get type name
	//group is deprecated?
//	for(i = 0; i < bump;i++) putc('\t', stdout);
//	printf("group:\t\t%i\n", op->group);	//todo get group name
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("stackop:\t%i\n", op->stackop);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("cond:\t\t%i\n", op->cond);	//todo get cond name
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("size:\t\t%i\n", op->size);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("nopcode:\t%i\n", op->nopcode);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("cycles:\t\t%i\n", op->cycles);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("failcycles:\t%i\n", op->failcycles);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("family:\t\t%i\n", op->family);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("id:\t\t%i\n", op->id);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("eob:\t\t%s\n", op->eob?"True":"False");
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("sign:\t\t%s\n", op->sign?"True":"False");
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("delay:\t\t%i\n", op->delay);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("jump:\t\t0x%llX\n", op->jump);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("fail:\t\t0x%llX\n", op->fail);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("direction:\t%i (%s)\n", op->direction, op->direction == 1 ? "read" : op->direction == 2 ? "write" : op->direction == 4 ? "exec" : op->direction == 8 ? "reference" : "unknown/none");
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("ptr:\t\t0x%llX\n", op->ptr);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("val:\t\t0x%llX\n", op->val);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("ptrsize:\t%i\n", op->ptrsize);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("stackptr:\t0x%llX (%s%llX)\n", op->stackptr, op->stackptr < 0 ? "-" : "", op->stackptr < 0 ? -op->stackptr : op->stackptr);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("refptr:\t\t%i\n", op->refptr);
	//todo move var to its own function
	if(op->var){
		for(i = 0; i < bump;i++) putc('\t', stdout);
		printf("Var\n");
		for(i = 0; i < bump;i++) putc('\t', stdout);
		printf("\tname:\t\t%s\n", op->var->name);
		for(i = 0; i < bump;i++) putc('\t', stdout);
		printf("\ttype:\t\t%s\n", op->var->type);
		for(i = 0; i < bump;i++) putc('\t', stdout);
		printf("\tkind:\t\t%c\n", op->var->kind);	//todo full name
		//todo
	}
	int j;
	RAnalValue *v;
	for(j = 0; j < 3; j++){
		v = op->src[j];
		if(!v)continue;
		for(i = 0; i < bump;i++) putc('\t', stdout);
		printf("Src %i\n", j);
		print_RAnalValue(v, bump+1);
	}
	if(op->dst){
		for(i = 0; i < bump;i++) putc('\t', stdout);
		printf("Dst\n");
		print_RAnalValue(op->dst, bump+1);
	}
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("esil:\t\t%s\n", R_STRBUF_SAFEGET(&op->esil));
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("opex:\t\t%s\n", R_STRBUF_SAFEGET(&op->opex));
	for(i = 0; i < bump;i++) putc('\t', stdout);
	if(op->reg)printf("reg:\t\t%s\n", op->reg);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	if(op->ireg)printf("ireg:\t\t%s\n", op->ireg);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("scale:\t\t%i\n", op->scale);
	for(i = 0; i < bump;i++) putc('\t', stdout);
	printf("disp:\t0x%llX\n", op->disp);
	//TODO switch_op
	//todo hint

	return 0;
}


//analyze with r2
//possible optimization... sort badareas (twice, once for start, once for end, similar to the "double doubly linked list used in quadtrees that also keep track of height")
int analyze_r2(analyze_section_metadata_t *meta){
	struct elf_file_metadata* elf = meta->elf;

	struct elf_section_header_64 * section;
	get_section(elf, meta->section, &section);

	size_t datasize  = section->size;

	uint8_t * data = elf->base_ptr + section->offset;
	if(meta->data){
		data = meta->data;
		datasize = meta->datasize;
	}

	meta->numbs = 0;
	meta->bs = 0;


	findrelocs(meta);
	findsymbols(meta);
	if(!meta->numsym){
		printf("Unable to find a symbol in this section %s\n", meta->section_name);
		return 0; //no symbols?
	}
	if(!anal) analyze_init();

	size_t rip = 0;
	RAnalOp aop = {0};
	size_t instrsize = 0;
	size_t bssize = 0;

	size_t lvsize = meta->numleaves;
	//find a FUNC symbol

	//assumes symbols are sorted
	symbol_t *cursym = meta->sym;
	size_t symi;
	for(symi = 0;symi < meta->numsym; symi++){
		//sorted
		for(cursym = meta->sym+symi; symi < meta->numsym; symi++, cursym++)
			//todo check FUNC as well
			if(rip <= cursym->where && cursym->type == STT_FUNC) break;
		//we found a symbol that is >rip or we came to end of section
		if(symi >= meta->numsym) break;
		rip = cursym->where;// still have symbols
	//	printf("locating operations\n");
		while(rip < datasize && rip < cursym->where+cursym->size){
			aop.size = 0;
			if(r_anal_op(anal, &aop, rip, data + rip, datasize-rip, R_ANAL_OP_MASK_ESIL) > 0){
//			if(r_anal_op(anal, &aop, rip, data + rip, datasize-rip, R_ANAL_OP_MASK_ALL) > 0){


//				printf("Op %li %li\n",meta->numins, rip);
//				print_RAnalOp(&aop, 1);



//				printf("rip is %li, size is %li, hex is: ", rip, aop.size);
//				size_t i;
//				for(i = 0; i < aop.size;i++) printf("%02hhx", ((char*)data+rip)[i]);
//				printf("\n");

				//make instr, realloc array if too small
				if(meta->numins +1 > instrsize){
					instrsize = (meta->numins+1) * 2;
					meta->ins = realloc(meta->ins, instrsize * sizeof(instr_t));
				}
				instr_t *curins = &meta->ins[meta->numins];
				meta->numins++;

				curins->loc = rip;
				curins->len = aop.size;
				curins->id = aop.id;
				curins->type = aop.type;
				curins->cantcruft = 0;
				curins->isbadcause = 0;
				curins->causebad = 0;
				curins->hasreloc = 0;
				curins->hassymbol = 0;
				curins->userip = 0;
				curins->resize = 0;
				curins->bytesofs = 0;
				curins->usersp = 0;
				curins->esil = 0;
				curins->regs = 0;
				curins->memread = 0;
				curins->memwrite = 0;
				curins->esil = strdup(R_STRBUF_SAFEGET(&aop.esil));
				curins->mnemonic = 0;


				if(strstr(curins->esil, "rsp")){
					curins->usersp = 1;
				}
//				if(strstr(curins->esil, "=[")){
//					curins->memwrite = 1;
//				}

				char * tmp = curins->esil-1;
				while((tmp = strchr(tmp+1, '[')) && tmp > curins->esil && *(tmp-1) == '=') curins->memwrite = 1;
				if(tmp){	//found a [ without a = before it
					curins->memread = 1;
					//lets make sure we didnt miss any =[
					if(!curins->memwrite && strstr(tmp, "=[")) curins->memwrite = 1;
				}

				//todo optimize
				//find if a reloc points within this instruction
				size_t chkrel;
				for(chkrel = 0; chkrel < meta->numrl; chkrel++) if (meta->rl[chkrel].where >= rip && meta->rl[chkrel].where < rip+aop.size) break;
				curins->hasreloc = (chkrel != meta->numrl);

				//todo optimize
				//find if a symbol points within this instruction
				size_t chksym;
				for(chksym = 0; chksym < meta->numsym; chksym++) if (meta->sym[chksym].where >= rip && meta->sym[chksym].where < rip+aop.size) break;
				curins->hassymbol = (chksym != meta->numsym);


				if(strstr(curins->esil, "rip")){

					curins->userip = 1;

					if(options.stackshuffle){
						//lets check to see if its a ret or lret (aka function leave)
						//todo figure out WHERE in r2 these are made, and determin if there is a better way (esil)?
						//note, these are sorted, but i dont have to qsort on them since they are added in a sorted manner

						int isleave = 0;
						if(aop.type == R_ANAL_OP_TYPE_RET){	//simplest
							isleave = 1;
						} else {
							//todo classify a sibling call as a "leave"
							//these will be things that modify rip to point outside of the symbol, but aren't calls
							// point to outside of the symbol is likely reloc, but could also just be a big ol jump
							//we can do further refinements such as check that they point to the start of another symbol, but i think that is overkill, and may lead to false negatives
							if(aop.type == R_ANAL_OP_TYPE_JMP || aop.type == R_ANAL_OP_TYPE_CJMP){
								// is there a reloc here?
								if(curins->hasreloc){
									//chkrel was the index of the reloc
									//TODO, add where the reloc points to (at least the symbol) to the reloc_s structure so we can verify that this reloc doesnt just point somewhere else inside this function
									//we dont have that info, so im gonna just assume

									//we have to be a form of jump
										isleave = 1;
								} else {	// does this guy jump OOB?
									//TODO verify this <=.... can you sibling call yourself?
//									printf("Where %li, size %li, jloc %lli\n", cursym->where, cursym->size, aop.jump);
									if(aop.jump <= cursym->where || aop.jump >= cursym->where + cursym->size){
										isleave =1;
									}
								}
							}

						}

						if(isleave){
//							if(!curins->mnemonic)
//								forcegetmnemonic(meta, curins);
//							printf("Fam %i, type %i, id %i %s\n", aop.family, aop.type, aop.id, curins->mnemonic);

							//add it to the list of leaves
							if(meta->numleaves +1 > lvsize){
								lvsize = 2*(meta->numleaves+1);
								meta->leaves = realloc(meta->leaves, lvsize * sizeof(leave_t));
							}
							leave_t *lv = &meta->leaves[meta->numleaves++];
							lv->instrind = meta->numins-1;
							//i have to find its entry later in leaves_find_entry
							lv->entryind = 0;
							lv->shins = 0;
							lv->regs = 0;
							lv->shuffy = 0;
							lv->numshins = lv->nextleave = 0;

						}

					}		//end of stack reoder stuff

					if(!curins->hasreloc){
	//					printf("0x%"PRIx64":\t%s\t0x%llx 0x%llx\n", rip, curins->esil, aop.jump, aop.fail);

						if( aop.fail != 1 || aop.jump != 1){

							badsection_t bs = {0};
							//we start from the instruction
							bs.start = bs.end = rip;
							bs.causeop = meta->numins-1;
							bs.causeoploc = rip;	// might be unused?

							if(aop.jump != -1 && aop.jump < bs.start)	bs.start = aop.jump;
							if(aop.jump != -1 && aop.jump > bs.end)		bs.end = aop.jump;
							if(aop.fail != -1 && aop.fail < bs.start)	bs.start = aop.fail;
							if(aop.fail != -1 && aop.fail > bs.end)		bs.end = aop.fail;


							//could just set bs.end to rip+aop.size, but that would break the redundant RET rip mod checks that we do next line...
							if(bs.start < rip && bs.end == rip) bs.end = rip+aop.size;	//if we jump backwards, want to make sure we dont put any stuff right before this instr


							//todo can i make this any cleaner?
							curins->bytesofs = aop.size - aop.nopcode;
							void *jumpdata = data + rip + aop.nopcode;
							switch(curins->bytesofs){
								case 1: {
									char ofs = *(char*)jumpdata;
									if(ofs >= 0) bs.space = SCHAR_MAX - ofs;
									else bs.space = ofs - SCHAR_MIN;}
									break;
								case 2: {		//this should very rarely happen
									short ofs = *(short*)jumpdata;
									if(ofs >= 0) bs.space = SHRT_MAX - ofs;
									else bs.space = ofs - SHRT_MIN;}
									break;
								case 4: {
									int ofs = *(int*)jumpdata;
									if(ofs >= 0) bs.space = INT_MAX - ofs;
									else bs.space = ofs - INT_MIN;}
									break;
								default:
	//								printf("cant figure out size for this op...It is probably a ret\n");
	//								printf("0x%"PRIx64":\t%s\t0x%llx 0x%llx\n", rip, curins->esil, aop.jump, aop.fail);
									bs.space = 0;
									curins->bytesofs = 0;
								break;
							}

							//double check that it just point to itself (some rets do this)
							if(bs.start != bs.end){
								//realloc if too small
								if(meta->numbs+1 > bssize){
									bssize = (meta->numbs+1)*2;
									meta->bs = realloc(meta->bs, bssize * sizeof(badsection_t));
								}

								curins->isbadcause = 1;
								curins->causebad = meta->numbs;

								meta->bs[meta->numbs] = bs;
								meta->numbs++;
							}
						}
					}
				}
			}
			rip += aop.size;
			r_anal_op_fini(&aop);
		}
	}



	//lets squeeze any buffers we resized
	if(meta->ins)meta->ins = realloc(meta->ins, meta->numins * sizeof(instr_t));
	if(meta->bs)meta->bs = realloc(meta->bs, meta->numbs * sizeof(instr_t));
	if(meta->leaves)meta->leaves = realloc(meta->leaves, meta->numleaves * sizeof(leave_t));

	//go back through all the badzones and check if any instructions hit them
	//todo can optimize with a binary search to find the "start" instruction
	size_t baddy;
	for(baddy = 0; baddy < meta->numbs; baddy++){
		size_t bstart = meta->bs[baddy].start;
		size_t bend = meta->bs[baddy].end;
		//find start instuction
		size_t binst;
		for(binst = 0; binst < meta->numins && meta->ins[binst].loc < bstart; binst++);
		binst++; //skip the first one, since we can add stuff before it
		//find end instruction, writing them all cantcruft
		for(; binst < meta->numins && meta->ins[binst].loc < bend; binst++) meta->ins[binst].cantcruft = 1;
	}
	return 0;
}


int print_code(analyze_section_metadata_t * meta){

	struct elf_file_metadata *elf = meta->elf;

	struct elf_section_header_64 * section;
	get_section(elf, meta->section, &section);

//	uint8_t * data = elf->base_ptr + section->offset;
//	size_t datasize = section->size;
//	if(meta->data){
//		data = meta->data;
//		datasize = meta->datasize;
//	}


//	printf("\n\n\n\n");
	printf("%li badzones for this section\n", meta->numbs);

	size_t j ;
	for(j = 0; j < meta->numbs; j++){
		printf("0x%"PRIx64" -> 0x%"PRIx64"\n", meta->bs[j].start, meta->bs[j].end);
	}


	if(!meta->numrl) findrelocs(meta);
	if(!meta->numsym) findsymbols(meta);

	size_t ei = 0;
	size_t li = 0;
	//size_t ci = 0;
	size_t pi = 0;
	patch_t *p = meta->pendingpatch;
	size_t chi = 0;
	for(j=0;j < meta->numins;j++){
		instr_t * ins = &meta->ins[j];
		size_t rip = ins->loc;

		if(!ins->esil)
			forceanalyze(meta, ins);

		if(!ins->mnemonic)
			forcegetmnemonic(meta, ins);

		/*
		//print cruftables
		for(;ci < meta->numcr && meta->cr[ci].ins <= j; ci++){
			if(meta->cr[ci].ins == j) printf("Cruft %li location %li isn %li with size %li\n", ci, meta->cr[ci].location, meta->cr[ci].ins, meta->cr[ci].amount);
		}
		*/
		//print patches
		if(p){
			for(;pi < p->nummods && p->modtable[pi].insind <= j; pi++){
				if(p->modtable[pi].insind == j) printf("Mod %li location %li isn %li with size %li\n", pi, p->modtable[pi].loc, p->modtable[pi].insind, p->modtable[pi].size);
			}
			for(;chi < p->numch && p->chtable[chi].insind <= j; chi++){
				//todo more info printed here
				if(p->chtable[chi].insind == j) printf("Changeins %li location %li isn %li\n", chi, p->chtable[chi].loc, p->chtable[chi].insind);
			}
		}

		//check entry
		for(; ei < meta->numentries && meta->entries[ei].instrind < j; ei++);
		char isentry = (ei < meta->numentries && meta->entries[ei].instrind == j);
		//check leave
		for(; li < meta->numleaves && meta->leaves[li].instrind < j; li++);
		char isleave = (li < meta->numleaves && meta->leaves[li].instrind == j);


		//check in entry modification table
		size_t wl;
		for(wl = 0; wl < meta->numentries; wl++) if(meta->entries[wl].instrind <= j && meta->entries[wl].instrind+meta->entries[wl].numshins > j) break;
		char isentrymesszone = (wl != meta->numentries);

		//check in leave modification table
		for(wl = 0; wl < meta->numleaves; wl++) if((meta->leaves[wl].instrind-1) - meta->leaves[wl].numshins < j && meta->leaves[wl].instrind > j) break;
		char isleavemesszone = (wl != meta->numleaves);


		char pred = ins->cantcruft;
		char pyellow = (j+1<meta->numins && ins[1].cantcruft);
		char oncause = ins->isbadcause;
		char hassymbol = ins->hassymbol;
		char hasreloc = ins->hasreloc;
		char userip = ins->userip;
		printf("%s0x%-6"PRIx64"|%-6li%s%-30s\t%s%-40s%s" , pred ? "\033[22;31m" : (pyellow ? "\033[22;33m": ""), rip, rip, pred|pyellow ? "\033[0m" : "",
			ins->mnemonic, oncause ? "\033[22;33m": (userip? "\033[22;32m": ""), ins->esil, oncause|userip ? "\033[0m":"");
		if(hassymbol) printf("|SYMBOL");
		if(hasreloc) printf("|RELOC");
		if(isentry) printf("|ENTRY");
		if(isleave) printf("|LEAVE");
		printf("\t %s%i%s ", isentrymesszone? "\033[22;31m" : (isleavemesszone ? "\033[22;32m":"") , ins->id, isentrymesszone || isleavemesszone ? "\033[0m":"");

		//check if regs are there
		//having no regs is a valid state, but we will check again just to be sure
		if(!ins->regs)ins_getregs(ins);
		registermap_printregs(ins->regs);

		printf("\n");
		if(isentrymesszone && isleavemesszone)printf("\nWARNING, overlapping entry and leave mess zones!\n\n");
	}

	//todo verbosity options
	if(meta->numentries)printf("ENTRY PRINT\n");
	//temp hack to double check shinds
	for(j=0; j < meta->numentries; j++){
		printf("Entry %li\n", j);
		entry_t e = meta->entries[j];
		size_t h;
		for(h = 0; h < e.numshins; h++){
			shuffleinstr_t shin = e.shins[h];
			instr_t *ins = &meta->ins[shin.instrind];

			if(!ins->esil)
				forceanalyze(meta, ins);

			if(!ins->mnemonic)
				forcegetmnemonic(meta, ins);
			char pred = ins->cantcruft;
			char pyellow = (shin.instrind+1<meta->numins && ins[1].cantcruft);
			char oncause = ins->isbadcause;
			char userip = ins->userip;
			printf("%s0x%-6"PRIx64"|%-6li:%s%-30s\t%s%-40s%s" , pred ? "\033[22;31m" : (pyellow ? "\033[22;33m": ""), ins->loc, ins->loc, pred|pyellow ? "\033[0m" : "",
				ins->mnemonic, oncause ? "\033[22;33m": (userip? "\033[22;32m": ""), ins->esil, oncause|userip ? "\033[0m":"");
			printf("\tDEPS: ");
			size_t cc;
			for(cc = 0; cc < shin.numdeps; cc++) printf("%li ", shin.deps[cc]);
			printf("\n");
		}
		printf("Entry %li shuffled\n", j);
		for(h = 0; h < e.numshins; h++){
			size_t hh = e.shuffy[h];
			shuffleinstr_t shin = e.shins[hh];
			instr_t *ins = &meta->ins[shin.instrind];

			if(!ins->esil)
				forceanalyze(meta, ins);

			if(!ins->mnemonic)
				forcegetmnemonic(meta, ins);

			char pred = ins->cantcruft;
			char pyellow = (shin.instrind+1<meta->numins && ins[1].cantcruft);
			char oncause = ins->isbadcause;
			char userip = ins->userip;
			printf("%s0x%-6"PRIx64"|%-6li:%s%-30s\t%s%-40s%s" , pred ? "\033[22;31m" : (pyellow ? "\033[22;33m": ""), ins->loc, ins->loc, pred|pyellow ? "\033[0m" : "",
				ins->mnemonic, oncause ? "\033[22;33m": (userip? "\033[22;32m": ""), ins->esil, oncause|userip ? "\033[0m":"");
//			printf("\tDEPS: ");
//			size_t cc;
//			for(cc = 0; cc < shin.numdeps; cc++) printf("%li ", shin.deps[cc]);
			printf("\n");
		}
	}

	if(meta->numleaves)printf("LEAVE PRINT\n");
	//temp hack to double check shinds
	for(j=0; j < meta->numleaves; j++){
		leave_t l = meta->leaves[j];
		printf("Leave %li(entry %li)\n", j, l.entryind);
		ssize_t h;
		for(h = l.numshins-1; h >=0; h--){
			shuffleinstr_t shin = l.shins[h];
			instr_t *ins = &meta->ins[shin.instrind];
			if(!ins->esil)
				forceanalyze(meta, ins);

			if(!ins->mnemonic)
				forcegetmnemonic(meta, ins);

			char pred = ins->cantcruft;
			char pyellow = (shin.instrind+1<meta->numins && ins[1].cantcruft);
			char oncause = ins->isbadcause;
			char userip = ins->userip;
			printf("%s0x%-6"PRIx64"|%-6li:%s%-30s\t%s%-40s%s" , pred ? "\033[22;31m" : (pyellow ? "\033[22;33m": ""), ins->loc, ins->loc, pred|pyellow ? "\033[0m" : "",
				ins->mnemonic, oncause ? "\033[22;33m": (userip? "\033[22;32m": ""), ins->esil, oncause|userip ? "\033[0m":"");
			printf("\tDEPS: ");
			size_t cc;
			for(cc = 0; cc < shin.numdeps; cc++) printf("%li ", l.numshins-1 - shin.deps[cc]);
			printf("\n");
		}
		printf("Leave %li shuffled (entry %li)\n", j, l.entryind);
		for(h = l.numshins-1; h >=0; h--){
			size_t hh = l.shuffy[h];
			shuffleinstr_t shin = l.shins[hh];
			instr_t *ins = &meta->ins[shin.instrind];
			if(!ins->esil)
				forceanalyze(meta, ins);

			if(!ins->mnemonic)
				forcegetmnemonic(meta, ins);

			char pred = ins->cantcruft;
			char pyellow = (shin.instrind+1<meta->numins && ins[1].cantcruft);
			char oncause = ins->isbadcause;
			char userip = ins->userip;
			printf("%s0x%-6"PRIx64"|%-6li:%s%-30s\t%s%-40s%s" , pred ? "\033[22;31m" : (pyellow ? "\033[22;33m": ""), ins->loc, ins->loc, pred|pyellow ? "\033[0m" : "",
				ins->mnemonic, oncause ? "\033[22;33m": (userip? "\033[22;32m": ""), ins->esil, oncause|userip ? "\033[0m":"");
//			printf("\tDEPS: ");
//			size_t cc;
//			for(cc = 0; cc < shin.numdeps; cc++) printf("%li ", l.numshins-1 - shin.deps[cc]);
			printf("\n");
		}
	}


	return 0;
}



int clean_ins(instr_t *ins){
	if(ins->esil){free(ins->esil); ins->esil = 0;}
	if(ins->mnemonic){free(ins->mnemonic); ins->mnemonic = 0;}
	return 0;
}

int clean_metadata(analyze_section_metadata_t *meta){
	size_t i;
	if(meta->section_name){ free(meta->section_name); meta->section_name = 0; }
	if(meta->bs){		free(meta->bs);		meta->bs = 0;}		meta->numbs = 0;
	if(meta->ins){
		for(i = 0; i < meta->numins; i++) clean_ins(&meta->ins[i]);
		free(meta->ins); meta->ins = 0;
	}
	meta->numins = 0;
	if(meta->rl){		free(meta->rl);		meta->rl = 0;}		meta->numrl = 0;
	if(meta->sym){		free(meta->sym);		meta->sym = 0;}		meta->numsym = 0;
//	if(meta->cr){		free(meta->cr);		meta->cr = 0;}		meta->numcr = 0;
	if(meta->entries){
		for(i = 0; i < meta->numentries; i++) clean_entry(&meta->entries[i]);
		free(meta->entries); meta->entries = 0;
	}	meta->numentries = 0;
	if(meta->leaves){
		for(i = 0; i < meta->numleaves; i++) clean_leave(&meta->leaves[i]);
		free(meta->leaves); meta->leaves = 0;
	}	meta->numleaves = 0;
	if(meta->data){
		free(meta->data);
	}

	if(meta->pendingpatch){
		clean_patch(meta->pendingpatch);
		free(meta->pendingpatch);
		meta->pendingpatch = 0;
	}

	meta->datasize = 0;
	memset(meta, 0, sizeof(analyze_section_metadata_t));


	return 0;
}

int clean_metadata_notreally(analyze_section_metadata_t *meta){
	size_t i;
//	if(meta->section_name){ free(meta->section_name); meta->section_name = 0; } //we dont strdup this... probably should
	if(meta->bs){		free(meta->bs);		meta->bs = 0;}		meta->numbs = 0;
	if(meta->ins){
		for(i = 0; i < meta->numins; i++) clean_ins(&meta->ins[i]);
		free(meta->ins); meta->ins = 0;
	}
	meta->numins = 0;
	if(meta->rl){		free(meta->rl);		meta->rl = 0;}		meta->numrl = 0;
	if(meta->sym){		free(meta->sym);		meta->sym = 0;}		meta->numsym = 0;
//	if(meta->cr){		free(meta->cr);		meta->cr = 0;}		meta->numcr = 0;
	if(meta->entries){
		for(i = 0; i < meta->numentries; i++) clean_entry(&meta->entries[i]);
		free(meta->entries); meta->entries = 0;
	}	meta->numentries = 0;
	if(meta->leaves){
		for(i = 0; i < meta->numleaves; i++) clean_leave(&meta->leaves[i]);
		free(meta->leaves); meta->leaves = 0;
	}	meta->numleaves = 0;

	if(meta->pendingpatch){
		clean_patch(meta->pendingpatch);
		free(meta->pendingpatch);
		meta->pendingpatch = 0;
	}

	meta->isanalyzed = 0;
	meta->issuper = 0;
//	memset(meta, 0, sizeof(analyze_section_metadata_t));
	return 0;
}


int analyze(analyze_section_metadata_t *meta){
	analyze_r2(meta);
	meta->isanalyzed = 1;
	return 0;
}
