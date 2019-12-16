#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>

#include <r_anal.h>
#include <r_asm.h>


#include "parser.h"

#include "analyze.h"

#include "options.h"

#include "fpatch.h"



#define ISWHITESPACE(char) (!(char)||(char)==' '||(char)=='\r'||(char)=='\n'||(char) =='\t')
#define ISNUM(char) ((char) >= '0' && (char) <= '9')
#define ISALPHA(char) ( ((char) >= 'a' && (char) <= 'z') || ((char) >= 'A' && (char) <= 'Z') )
#define ISALPHANUM(char) ( ISALPHA((char)) || ISNUM((char)) )



extern RAsm * rasm;

//somewhat of a hack to count the difference between the "sub rsp 0x20" and the pushes before it
ssize_t count_pushbeforesave(analyze_section_metadata_t *meta, size_t start, size_t end){
	//find where i mov rbp, rsp
	for(; start < end; start++){
		instr_t *in = meta->ins + start;
		if(in->id == 449) break;
	}
//todo support things that arent 8 bytes
	ssize_t cnt = 0;
	for(; start < end; start++){
		instr_t *in = meta->ins + start;
		if(in->id == 588) cnt+=8;		//todo change this to look at stackop
		else if (in->id == 566) cnt -= 8;	//TODO is 147 right? or is it 566	//todo change this to look at stackop
	}
	return cnt;
}


#define FIXUP_STACKOP_DONTPUT -1
//TODO we need to check that rbp exists

//currently kindof a hack.... dissassembles the instruction, checks and changes it, and reassembles it
//this is because more than just movs can access relative to the stack. Almost any instruction can (you will see a lot of cmp [rsp+something], rax in code)
//todo need to count the pushes between mov rbp, rsp and the sub rsp to adjust my rspofs
//TODO make sure that stack reodering also does something with this? rbp+ addressing will be broken if rbp is a different place
char strabuf[0x1000];	//allocated a lot because im lazy and dont want to do it dynamic
//patch_t fixup_stackop(analyze_section_metadata_t * meta, int insind, size_t rspofs){
int fixup_stackop_putatplace(patch_t *p, analyze_section_metadata_t * meta, int insind, size_t rspofs, int putplace){
	int mod = 0;
	struct elf_file_metadata * elf = meta->elf;
	struct elf_section_header_64 * section;
	get_section(elf, meta->section, &section);

	uint8_t * data = elf->base_ptr + section->offset;
	if(meta->data) data = meta->data;


	//not using forcegetmnemonic because i dont want overhead of keeping the strs around?
	instr_t *in = meta->ins+insind;
	//lets disass it
	if(!rasm) analyze_init();
	RAsmOp rop ={0};
	r_asm_disassemble(rasm, &rop, data + in->loc, in->len);
//	printf("disas %s | %s\n", rop.buf_asm, rop.buf_hex);
	char * buf_asm = r_strbuf_get(&rop.buf_asm);
	char * memastart = buf_asm;
	char * memaend = memastart;
	char * inwrit = memastart;
	char * outwrit = strabuf;
	*strabuf = 0;
	while( (memastart = strchr(memaend, '[')) ){
		memaend = strchr(memastart, ']');
		if(!memaend) break;	//unlikely to get a [ without a ] following... but we will check anyway
		//lets see if they need changing
		char * rsp = strcasestr(memastart, "rsp");
		if(rsp >= memaend) rsp = 0;
		char * rbp = strcasestr(memastart, "rbp");
		if(rbp >= memaend) rsp = 0;
		if(rsp && rbp){
			printf("Fixup_stackop WARNING: found both a rbp and a rsp in a memory access? skipping\n");
			continue;
		}
		char * reg = rsp ? rsp : rbp;
		if(!reg) continue;
		//find a + or -
		char * op = strchr(reg, '+');
		if(op >= memaend) op = 0;
		if(!op){
			op = strchr(reg, '-');
			if(op >= memaend) op = 0;
		}
		//todo grab the num directly from in?
		if(!op) continue;			//no + or -
		char * end = 0;
		long int num = strtol(op+1, &end, 0);
		long int trickynum = *op == '-' ? -num : num;
		if(rsp){
			//we are looking for numbers that are too large
			if(trickynum < rspofs) continue;	//all good!
			mod =1;
			//ok, rsp + too much
			//lets calculate the rbp + versions
			long int newnum = trickynum - rspofs;
			if(newnum < 0){
				//todo errors!
				printf("Newnum is <0, something weird is going on!\n");
				return -1;
			}
			//copy over the old stuff
			memcpy(strabuf, inwrit, reg-inwrit);
			outwrit += reg-inwrit;
			//copy over new stuff
			sprintf(outwrit, "rbp + %li", newnum);
			outwrit += strlen(outwrit);
			inwrit = memaend;
		} else if(rbp){
			//we are looking for negative numbers
			if(trickynum >= 0) continue;		//all good!
			mod =1;
			//ok, rbp -
			//lets calculate the rsp + versions
			long int newnum = rspofs + trickynum;
			if(newnum < 0){
				//todo errors!
				printf("Newnum is <0, something weird is going on!\n");
				return -1;
			}
//			printf("%li + %li = %li\n", rspofs, trickynum, newnum);
			//copy over the old stuff
			memcpy(strabuf, inwrit, reg-inwrit);
			outwrit += reg-inwrit;
			//copy over new stuff
			sprintf(outwrit, "rsp + %li", newnum);
			outwrit += strlen(outwrit);
			inwrit = memaend;
		}
	}
	if(mod){
		//make sure to copy over the rest
		strcpy(outwrit, inwrit);
		printf("%s -> %s\n", buf_asm, strabuf);
		//assemble the new instruction
		int res = r_asm_assemble(rasm, &rop, strabuf);
		if(!res) return -1;	//error assembling
		//add it as a replacement mod


		//FIXUP_STACKOP_DONTPUT
		if(putplace < 0){
			mod_t m = {insind, in->loc, rop.size, 0, in->len, NULL};
			//small hack because i havent figured out that bug yet
			if(m.size < m.replacesize){
				m.size = m.replacesize;
			}
			m.data = malloc(m.size);
			memcpy(m.data, r_strbuf_getbin(&rop.buf, NULL), rop.size);
			//more of a small hack, insert nops till it works
			memset(m.data+rop.size, 0x90, m.size-rop.size);
			patch_addmod(p, m);
		} else {		//we have a valid putplace
			mod_t m = {insind, in->loc, in->len, 0, in->len, NULL};
			//small hack because i havent figured out that bug yet
			m.data = malloc(m.size);
			//more of a small hack, insert nops till it works
			memset(m.data, 0x90, m.size);
			patch_addmod(p, m);
			//add another patch in our putplace for the real one
			//calcualte should fix up our loc anyway later
			mod_t m2 = {putplace, meta->ins[putplace].loc, rop.size, 0, 0, NULL};
			m2.data = malloc(m2.size);
			memcpy(m2.data, r_strbuf_getbin(&rop.buf, NULL), rop.size);
			patch_addmod(p, m2);
		}
	}

	return mod;
}
//little ease of use wrapper for above
int fixup_stackop(patch_t *p, analyze_section_metadata_t * meta, int insind, size_t rspofs){
	return fixup_stackop_putatplace(p, meta, insind, rspofs, FIXUP_STACKOP_DONTPUT);
}


int fixup_frame(patch_t *p, analyze_section_metadata_t *meta, size_t start, size_t end, size_t bump){
	size_t i;
	//loop through all instructions, check if they need a fixup
	for(i = start; i < end; i++){
		instr_t *in = meta->ins+i;
		//basically we want an op that uses a fixed offset from rbp or rsp
		if(	//read memory	//write			//lea
			(in->memread || in->memwrite || in->type == R_ANAL_OP_TYPE_LEA) &&
			(in->type != R_ANAL_OP_TYPE_POP
			&& in->type != R_ANAL_OP_TYPE_PUSH && in->type != R_ANAL_OP_TYPE_UPUSH && in->type != R_ANAL_OP_TYPE_RPUSH
			&& in->type != R_ANAL_OP_TYPE_RET
			&& in->type != R_ANAL_OP_TYPE_CALL)	//not a push, pop, ret, or call
			&& (in->usersp || strstr(in->esil, "rbp"))		//touches either rbp or rsp
		){
			int res = fixup_stackop(p, meta, i, bump);
			if(res < 0){
				//cant fix
				return -1;
			}
			//todo attempt other methods (if static shims)
		}

	}
	return 0;
}

int fixup_frame_before(patch_t *p, analyze_section_metadata_t *meta, size_t start, size_t shimloc, size_t bump){
	printf("fixup_frame-before!\n");
	size_t i;
	//loop through all instructions, check if they need a fixup
	for(i = start; i < shimloc; i++){
		instr_t *in = meta->ins+i;
		if(	//read memory	//write			//lea
			(in->memread || in->memwrite || in->id == 322) &&
			(in->id != 566 && in->id != 588 && in->id != 147 && in->id != 56) &&	//not a push, pop, ret, or call
			(in->usersp || strstr(in->esil, "rbp"))		//touches either rbp or rsp
		){
			int res = fixup_stackop_putatplace(p, meta, i, bump, shimloc+1);
			if(res < 0){
				//cant fix
				return -1;
			}
			//todo attempt other methods (if static shims)
		}

	}
	return 0;
}




//this function is to update rbp+ offsets, as reordering the stack might result in the mov rbp, rsp being re-arranged, and therefore being at a different offset from the arguments

/*
int fixup_reorder(patch_t *p, analyze_section_metadata_t *meta, size_t start, size_t end, size_t bump){
	size_t i;
	for(i = start; i < end; i++){
		instr_t *in = meta->ins+i;
		if(
			(in->memread || in->memwrite) &&
			(in->id != 566 && in->id != 588 && in->id != 147 && in->id != 56) &&	//not a push, pop, ret, or call
			(strstr(in->esil, "rbp"))		//touches rbp
		){
			int res = fixup_reoder_stackop(p, meta, i, bump);
			if(res < 0){
				//cant fix
				return -1;
			}
			//todo attempt other methods (if static shims)
	}
}
*/
