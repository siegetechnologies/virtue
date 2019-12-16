#ifndef PARSER_H
#define PARSER_H

#define BMP_HLT_BYTE 0xF1 	//halt opcode to insert after sections (todo different architechtures)
/*
 *may already exist in kernel space
 *https://en.wikipedia.org/wiki/Executable_and_Linkable_Format#File header
 */



	#define ELF32_R_SYM(i)	((i)>>8)
	#define ELF32_R_TYPE(i)   ((unsigned char)(i))
	#define ELF32_R_INFO(s,t) (((s)<<8)+(unsigned char)(t))

	#define ELF64_R_SYM(i)    ((i)>>32)
	#define ELF64_R_TYPE(i)   ((i)&0xffffffffL)
	#define ELF64_R_INFO(s,t) (((s)<<32)+((t)&0xffffffffL))

	#define ELF32_ST_BIND(i)   ((i)>>4)
	#define ELF32_ST_TYPE(i)   ((i)&0xf)
	#define ELF32_ST_INFO(b,t) (((b)<<4)+((t)&0xf))

	#define ELF64_ST_BIND(i)   ((i)>>4)
	#define ELF64_ST_TYPE(i)   ((i)&0xf)
	#define ELF64_ST_INFO(b,t) (((b)<<4)+((t)&0xf))


#define STT_NOTYPE	0
#define STT_OBJECT	1
#define STT_FUNC	2
#define STT_SECTION	3
#define STT_FILE	4
#define STT_COMMON	5
#define STT_TLS		6
#define STT_LOOS	10
#define STT_HIOS	12
#define STT_LOPROC	13
#define STT_HIPROC	15

struct elf_header {
	uint8_t magic[4]; //0x7f 'E' 'L' 'F'
	uint8_t bitmode; // 2 for 64 bit format
	uint8_t endianness; //expect 1
	uint8_t ident_version; //should be 1
	uint8_t abi; //hope this is 3
	uint8_t abiversion; //ignore this
	uint8_t padding[7];
	uint16_t type; //{1:relocatable,2:exec,3:shared,4:core}
	uint16_t machine; //expect 0x3e for x86_64
	uint32_t version; //expect 1
	void* entry; //address of entry point
	void* phoff; // start of program header table; usually 0x40
	void* shoff; //section header table offset
	uint32_t flags; //arch dependent
	uint16_t ehsize; //probably 64 bytes
	uint16_t phentsize; //program header entry size
	uint16_t phnum; //program header entry count
	uint16_t shentsize; //section header entry size
	uint16_t shnum; //section header entry count
	uint16_t shstridx; //index of section header table containing names
} __attribute__((packed));


#define PT_NULL    0x0
#define PT_LOAD    0x1
#define PT_DYNAMIC 0x2
#define PT_INTERP  0x3
#define PT_NOTE    0x4
#define PT_SHLIB   0x5
#define PT_PHDR    0x6

struct elf_prog_header_64 {
	uint32_t type; // see #defines
	uint32_t flags;
	uint64_t offset;
	void* vaddr;
	void* paddr;
	uint64_t filesz;
	uint64_t memsz;
	uint64_t align; //0,1, or power of 2
};

//section header types
#define SHT_NULL          0x0
#define SHT_PROGBITS      0x1
#define SHT_SYMTAB        0x2
#define SHT_STRTAB        0x3
#define SHT_RELA          0x4
#define SHT_HASH          0x5
#define SHT_DYNAMIC       0x6
#define SHT_NOTE          0x7
#define SHT_NOBITS        0x8
#define SHT_REL           0x9
#define SHT_SHLIB         0xA
#define SHT_DYNSYM        0xB
#define SHT_INIT_ARRAY    0xE
#define SHT_FINI_ARRAY    0xF
#define SHT_PREINIT_ARRAY 0x10
#define SHT_GROUP         0x11
#define SHT_SYMTAB_SHNDX  0x12
#define SHT_SYMTAB_NUM    0x13

//section header flags
#define SHF_WRITE      (1<<0)
#define SHF_ALLOC      (1<<1)
#define SHF_EXECINSTR  (1<<2)
//don't ask me why 1<<3(0x8) is skipped
#define SHF_MERGE      (1<<4)
#define SHF_STRINGS    (1<<5)
#define STF_INFO_LINK  (1<<6)
#define SHF_LINK_ORDER (1<<7)
#define SHF_OS_NONCONF (1<<8)
#define SHF_GROUP      (1<<9)
#define SHF_TLS        (1<<10)
#define SHF_MASKOS     0x0ff00000 //os specific, ignore
#define SHF_MASKPROC   0xf0000000 // processor specific

#define SHN_LORESERVE 0xff00
#define SHN_HIRESERVE 0xffff

struct elf_section_header_64 {
	uint32_t name; //offset into section name section
	uint32_t type; //see the #defines above
	uint64_t flags;
	uint64_t addr;
	uint64_t offset;
	uint64_t size;
	uint32_t link;
	uint32_t info;
	uint64_t addralign;
	uint64_t entsize; //0 for non-fixed size entries
} __attribute__((packed));

struct elf_symtab_entry {
	uint32_t name;
	uint8_t info;
	uint8_t other;
	uint16_t shidx;
	uint64_t value;
	uint64_t size;
} __attribute__((packed));


#define ELF_METADATA_HDR_CHANGED (1)
#define ELF_METADATA_PROG_CHANGED (1<<1)
#define ELF_METADATA_SEC_CHANGED (1<<2)

struct elf_file_metadata {
	void* base_ptr;
	size_t total_sz;
	struct elf_header *hdr;
	struct elf_prog_header_64 *proghdrs;
	size_t numproghdrs;
	struct elf_section_header_64 *sechdrs;
	size_t numsechdrs;

	struct analyze_section_metadata_s *metas;
	char changed;
};


struct elf32_rel {
	uint32_t offset;
	uint32_t info;
}__attribute__((packed));
struct elf32_rela {
	uint32_t offset;
	uint32_t info;
	int32_t  addend;
}__attribute__((packed));

struct elf64_rel {
	uint64_t offset;
	uint64_t info;
}__attribute__((packed));
struct elf64_rela {
	uint64_t offset;
	uint64_t info;
	int64_t  addend;
}__attribute__((packed));

void run_tests( void );

int parse_elf( uint8_t *in_data, size_t in_len, struct elf_file_metadata *out_parsed );

int get_num(const struct elf_file_metadata *elf, struct elf_section_header_64 * in_sec);

int get_section_name( const struct elf_file_metadata *elf, size_t secnum, char **out_name );
int get_section( const struct elf_file_metadata *elf, size_t secnum, struct elf_section_header_64 **out_sec );

int rejigger_elf( struct elf_file_metadata *in_elf, struct elf_file_metadata *out_elf );
int write_elf( struct elf_file_metadata *elf, char* fn );
int free_elf( struct elf_file_metadata *elf );



#endif //PARSER_H
