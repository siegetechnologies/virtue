#ifndef STACKSHUFFLE_HEADER
#define STACKSHUFFLE_HEADER

/*
typedef struct shuffleinstr_s {
	size_t instrind;
	//sorted backwards (last instruction comes first)
	size_t *deps;
	size_t numdeps;
	size_t stackorder;
}shuffleinstr_t;
*/


size_t findentries(analyze_section_metadata_t *meta);
int leaves_find_entry(analyze_section_metadata_t *meta);
int clean_entry(entry_t *l);
int clean_leave(leave_t *l);

int findentryshuffleranges(analyze_section_metadata_t *meta);
int findleaveshuffleranges(analyze_section_metadata_t *meta);
int dotrunc(analyze_section_metadata_t *meta);
int calcdeps(analyze_section_metadata_t *meta);

int calcshuffle(analyze_section_metadata_t *meta);
int applyshuffle(analyze_section_metadata_t *meta);

int calcentireshuffle(analyze_section_metadata_t *meta);

#endif
