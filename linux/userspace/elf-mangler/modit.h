#ifndef MODITHEADER
#define MODITHEADER


int add_symbol(analyze_section_metadata_t *symeta, size_t secnum, size_t ofs, size_t size, char * name);

int add_rela(analyze_section_metadata_t * relameta, size_t symbol, size_t ofs, ssize_t addend);
int add_rel(analyze_section_metadata_t * relameta, size_t symbol, size_t ofs);


size_t add_str(analyze_section_metadata_t *meta, char * name);

size_t add_section(struct elf_file_metadata *elf, struct elf_section_header_64 sec, char * name);

size_t add_rela_section(struct elf_file_metadata * elf, size_t link, size_t info, char * name);




#endif
