#ifndef CRUFTABLEHEADER
#define CRUFTABLEHEADER

typedef struct cruft_s {
	size_t location;
	size_t ins;
	size_t amount;
} cruft_t;



size_t write_cruftable_wrapper(void * out, size_t len);
size_t generate_cruftables(analyze_section_metadata_t *meta);
size_t generate_cruftables_patch(analyze_section_metadata_t *meta);
size_t get_total_size(analyze_section_metadata_t *meta);


size_t modify_super_cruftables(analyze_section_metadata_t *meta);
size_t copy_with_cruftables(analyze_section_metadata_t * meta, void * output);

size_t fixup_rel(struct elf_section_header_64 * section, analyze_section_metadata_t *meta);
size_t fixup_rela(struct elf_section_header_64 * section, analyze_section_metadata_t *meta);

size_t fixup_rela_addend(struct elf_section_header_64 * section, struct elf_file_metadata * elf, analyze_section_metadata_t *metas);

size_t fixup_symtab(struct elf_section_header_64 * section, struct elf_file_metadata * elf, analyze_section_metadata_t *metas);

size_t generate_cruftables_wrapper(analyze_section_metadata_t *m);

#endif
