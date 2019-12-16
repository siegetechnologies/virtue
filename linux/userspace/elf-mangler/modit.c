#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "parser.h"

#include "parser.h"
#include "analyze.h"

#include "modit.h"

//this will be somewhat slowish if you are adding quite a lot of symbols
int add_symbol(analyze_section_metadata_t * symeta, size_t secnum, size_t ofs, size_t size, char * name){

	struct elf_file_metadata * elf = symeta->elf;

	struct elf_section_header_64 * section;
	get_section(elf, symeta->section, &section);

	uint8_t *indata = elf->base_ptr + section->offset;
	size_t insize = section->size;
	uint8_t * outdata = 0;
	size_t outsize = insize + sizeof(struct elf_symtab_entry);

	if(symeta->data){
		indata = symeta->data;
		insize = symeta->datasize;
		outsize = insize + sizeof(struct elf_symtab_entry);
		outdata = realloc(indata, outsize);
	} else {
		outdata = malloc(outsize);
		memcpy(outdata, indata, insize);
	}


	struct elf_symtab_entry * newsym = (struct elf_symtab_entry *)(outdata + insize);

	uint64_t bind = 0;	//local
	uint64_t type = STT_FUNC;
	//todo stringtable?
	newsym->name = add_str(&elf->metas[section->link], name);
	newsym->value = ofs;
	newsym->size = size;
	newsym->info = ELF64_ST_INFO(bind, type);
	newsym->shidx = secnum;
	newsym->other = 0;
	//todo SHN_XINDEX support
	symeta->datasize = outsize;
	symeta->data = outdata;
	return 0;
}

//this will be somewhat slowish if you are adding a LOT of relas
int add_rela(analyze_section_metadata_t * relameta, size_t symbol, size_t ofs, ssize_t addend){
	struct elf_section_header_64 * section;
	get_section(relameta->elf, relameta->section, &section);

	uint8_t *indata = relameta->elf->base_ptr + section->offset;
	size_t insize = section->size;
	uint8_t * outdata = 0;
	size_t outsize = insize + sizeof(struct elf64_rela);
	if(relameta->data){
		indata = relameta->data;
		insize = relameta->datasize;
		outsize = insize + sizeof(struct elf64_rela);
		outdata = realloc(indata, outsize);
	} else {
		outdata = malloc(outsize);
		memcpy(outdata, indata, insize);
	}
	struct elf64_rela * reloc = (struct elf64_rela *)(outdata+insize);

	uint64_t sym = symbol;
	uint64_t type = 0x02;	//todo double check that
	reloc->offset = ofs;
	reloc->info = ELF64_R_INFO(sym, type);
	reloc->addend = addend;
	relameta->datasize = outsize;
	relameta->data = outdata;
	return 0;
}

//this will be somewhat slowish if you are adding a LOT of rels
int add_rel(analyze_section_metadata_t * relmeta, size_t symbol, size_t ofs){
	struct elf_section_header_64 * section;
	get_section(relmeta->elf, relmeta->section, &section);

	uint8_t *indata = relmeta->elf->base_ptr + section->offset;
	size_t insize = section->size;
	uint8_t * outdata = 0;
	size_t outsize = insize + sizeof(struct elf64_rel);
	if(relmeta->data){
		indata = relmeta->data;
		insize = relmeta->datasize;
		outsize = insize + sizeof(struct elf64_rel);
		outdata = realloc(indata, outsize);
	} else {
		outdata = malloc(outsize);
		memcpy(outdata, indata, insize);
	}
	struct elf64_rel * reloc = (struct elf64_rel *)(outdata+insize);

	uint64_t sym = symbol;
	uint64_t type = 0x02;	//todo double check that
	reloc->offset = ofs;
	reloc->info = ELF64_R_INFO(sym, type);
	relmeta->datasize = outsize;
	relmeta->data = outdata;
	return 0;
}


size_t add_str(analyze_section_metadata_t * meta, char * name){
	//and we have to adjust our stringtable
	struct elf_section_header_64 * section;
	get_section(meta->elf, meta->section, &section);

	uint8_t *indata = meta->elf->base_ptr + section->offset;
	size_t insize = section->size;
	uint8_t * outdata = 0;

	size_t len= strlen(name);
	size_t outsize = insize + len;

	if(meta->data){
		indata = meta->data;
		insize = meta->datasize;
		outsize = insize + len;
		outdata = realloc(indata, outsize);
	} else {
		outdata = malloc(outsize);
		memcpy(outdata, indata, insize);
	}
	memcpy(outdata+insize, name, len);
	return insize;
}


//returns 0 on failure, the index of the section on success
//TODO double check that parser correctly copies over the new version
//TODO add a failure mode
size_t add_section(struct elf_file_metadata *elf, struct elf_section_header_64 sec, char * name){
	size_t indx = elf->numsechdrs;
	elf->numsechdrs++;
	if(elf->changed & ELF_METADATA_SEC_CHANGED){
		elf->sechdrs = realloc(elf->sechdrs, elf->numsechdrs * sizeof(struct elf_section_header_64));
	} else {
		struct elf_section_header_64 *nsec = malloc(elf->numsechdrs * sizeof(struct elf_section_header_64));
		memcpy(nsec, elf->sechdrs, elf->numsechdrs * sizeof(struct elf_section_header_64));
		elf->sechdrs = nsec;
	}
	//we have to update our metas as well
	elf->metas = realloc(elf->metas, elf->numsechdrs * sizeof(analyze_section_metadata_t));

	//things are resized
	struct elf_section_header_64 *thesec = elf->sechdrs + indx;

	*thesec = sec;

	thesec->name = add_str(elf->metas+elf->hdr->shstridx, name);

	//lets init that new meta
	elf->metas[indx] = analyze_meta(indx, elf);


	//todo section string table?

	elf->changed |= ELF_METADATA_SEC_CHANGED;
	return indx;
}


//lot of helper functions
//TODO more of them
size_t add_rela_section(struct elf_file_metadata *elf, size_t link, size_t info, char * name){

	struct elf_section_header_64 sec = {0};

	sec.size = 0;
	sec.type = SHT_RELA;
	sec.offset = 0;	//will be fixed with the data override
	sec.size = 0;
	sec.link = link;
	sec.info = info;
	sec.addralign = 8;
	sec.flags = 0x40;
	sec.entsize = sizeof(struct elf_section_header_64);


	return add_section(elf, sec, name);
}
