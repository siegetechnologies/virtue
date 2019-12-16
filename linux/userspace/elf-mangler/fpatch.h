#ifndef FPATCHEADER
#define FPATCHEADER


typedef struct changeins_s{
	size_t insind;
	size_t loc;			//cache it, calculated at the beginning of claculate_patch
	size_t bytes_orig;		//just copied from ins->bytesofs
	size_t bytes_current;
	ssize_t towhere_orig;		//calculated from the ins data
	ssize_t towhere_new;		//might not need

	size_t len_orig;
	size_t len_new;

	ssize_t bsize;			//how much to bump by
	int opcodeindx;			//index into the list. -1 is error/notfound
	//todo add a callback?
} changeins_t;

#define MOD_DONTFREE	1
//#define MOD_REPLACE	2
//#define MOD_AFTER	4 //todo

typedef struct mod_s {
	size_t insind;	//insert right before this instruction (like a cruftable)
	size_t loc;	//cache it
	size_t size;
	char flags;	//one is dontfree
	size_t replacesize;
	void * data;
	//todo a callback instead!
	//callback would also need one that will figure out the size?
} mod_t;

typedef struct fixup_tracker_s {
	size_t loc;
	ssize_t size;
} fixup_tracker_t;

typedef struct patch_s {
	//sorted (when finalized)
	mod_t *modtable;
	size_t nummods;
	size_t sizemods;
	changeins_t * chtable;	//sorted
	size_t numch;
	size_t addsz;

	fixup_tracker_t * fixups;	//sorted
	size_t numfixups;

} patch_t;

int calculate_patch(analyze_section_metadata_t *meta, patch_t *p);
int apply_patch(analyze_section_metadata_t *meta);

int clean_mod(mod_t *m);
int clean_patch(patch_t *p);

int patch_addmod(patch_t *p, mod_t m);
int patch_addpatch(patch_t *orig, patch_t *n);

size_t patch_genfixups(patch_t *p);

int mod_compareofs(const void *a, const void *b);
int changeins_compareofs(const void *a, const void *b);



//used by fpatch internally as well as cruftable
size_t jumphack_get_ofs(struct elf_file_metadata *elf, size_t secnum, size_t loc);


size_t apply_and_fixup_patches(struct elf_file_metadata *elf, analyze_section_metadata_t *metas);



int bumpchangeins(analyze_section_metadata_t *meta, patch_t *p, size_t op, ssize_t bumpsize);

void print_patch(patch_t *p);

#endif
