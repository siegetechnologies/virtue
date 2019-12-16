#ifndef ANALHEADER
#define ANALHEADER

typedef struct shim_s {
	size_t refnumber;
	char hasadd;
	char hassub;
	uint8_t opbytes;
	size_t *inslocs;
	size_t numinslocs;
	char is_supershim;
	size_t start;		//earliest sub rsp instruction
} shim_t;


typedef struct reloc_s {
	size_t where;		//currently we only care about where a reloc happens. Might be useful to get more infor.
} reloc_t;
typedef struct symbol_s {
	size_t where;		//currently only used for printing purposes
	size_t size;
	uint8_t type;
} symbol_t;

typedef struct badsection_s {
	size_t start;		//offset
	size_t end;
	size_t causeop;	//instruction that causes badop
	size_t causeoploc;	//offset of the instruction... probably unused
	size_t space;		//how much space i have left to insert cruft (super-cruftables)
} badsection_t;

typedef struct instr_s {
	size_t loc;
	uint8_t len;
	//todo turn many of these into bitfields
	char cantcruft;	//able to put cruft right before instruction (not after)
	char isbadcause;//does this cause a badsection
	size_t causebad;//the badsection that it causes
	size_t resize;	//im being resized
	uint8_t bytesofs;	//how many bytes in my data (1 is usually short jump)
	char hasreloc;	//does a relocation point anywhere inside of this intruction
	char hassymbol;	//does a symbol point anywhere inside of this intruction
	char userip;	//does this instruction use rip?
	char usersp;	//does this instruction use rsp?
	char memread;	//does this instruction read from memory?
	char memwrite;	//does it write?
	int id;	//same as aop.id;
	int32_t type;	//same as aop.id;

	char * esil;
	uint64_t regs;
	char * mnemonic;
} instr_t;



//todo move these?
typedef struct shuffleinstr_s {
	size_t instrind;
	//sorted backwards
	size_t *deps;
	size_t numdeps;
	size_t stackorder;	//useful
}shuffleinstr_t;

typedef struct entry_s {
	size_t instrind;

	//sorted
	shuffleinstr_t *shins;
	size_t numshins;


	size_t *shuffy;

	uint64_t regs;		//so i dont have to reget them every time
	size_t leavelist;	//start of LL to my entries, index -1 (so 0 is invalid)
} entry_t;

typedef struct leave_s {
	size_t entryind; //pretty much which function I correspond to
	size_t instrind;

	//sorted backwards
	shuffleinstr_t *shins;
	size_t numshins;

	size_t *shuffy;

	uint64_t regs;		//so i dont have to reget them every time
	size_t nextleave;	// linked list of leaves per entry, 0 is invalid, index-1
} leave_t;



typedef struct analyze_section_metadata_s {
	size_t section;		//index
	struct elf_file_metadata * elf;
	void * data;		//overrides getting it from the section
	size_t datasize;
	char * section_name;	//may be unused
	//unsorted
	badsection_t *bs;
	size_t numbs;
	//sorted
	instr_t *ins;
	size_t numins;
	//sorted
//	struct cruft_s *cr;
//	size_t numcr;
	//unsorted
	reloc_t * rl;
	size_t numrl;
	//unsorted
	symbol_t *sym;
	size_t numsym;
	//sorted
	entry_t *entries;
	size_t numentries;
	//sorted
	leave_t *leaves;
	size_t numleaves;
	shim_t *shims;

	char issuper;
	char isanalyzed;

	struct patch_s * pendingpatch;
} analyze_section_metadata_t;



int print_code(analyze_section_metadata_t * meta);
int clean_metadata(analyze_section_metadata_t * meta);
int clean_metadata_notreally(analyze_section_metadata_t * meta);
analyze_section_metadata_t analyze_meta(size_t section, struct elf_file_metadata *elf);
int analyze(analyze_section_metadata_t *meta);
uint64_t ins_getregs(instr_t *ins);


char * forcegetmnemonic(analyze_section_metadata_t *meta, instr_t *ins);

//RAnal *anal;
//RAsm * rasm;	//get it with extern
void analyze_init(void);
void analyze_shutdown(void);


void printregs(uint64_t regs);
#endif
