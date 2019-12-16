#ifndef FRAMEFIXHEADER
#define FRAMEFIXHEADER

	ssize_t count_pushbeforesave(analyze_section_metadata_t * meta, size_t start, size_t end);

	int fixup_frame(patch_t *p, analyze_section_metadata_t * meta, size_t start, size_t end, size_t bump);
	int fixup_frame_before(patch_t *p, analyze_section_metadata_t * meta, size_t start, size_t shimloc, size_t bump);
	int fixup_stackreorder(patch_t *p, analyze_section_metadata_t * meta, size_t start, size_t end, size_t bump);


#endif
