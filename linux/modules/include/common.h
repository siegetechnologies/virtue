//Anything common to every part of maven
//pure helper functions, central config parameters
// as well as common data types
// since some helper functions don't get used in all files in which they're included
// they're marked as possibly unused

#ifndef COMMON_H
#define COMMON_H

#include <linux/uuid.h>

struct api_response {
	int code;
	char* msg;
	int should_free_msg;
};

struct transducer_descr {
	char* name;
	uuid_t uuid;
	int (*get_enabled)(void); // 0 is disabled
	int (*set_enabled)( int );
	// should return 1 for now enabled, 0 for now disabled, -1 for error
};

//determines if 2 strings are equal
static __attribute__((unused)) int streq( const char* c1, const char* c2 ) {
	while( *c1 && *c2 ) {
		if( *(c1++) != *(c2++) ) {
			return 0;
		}
	}
	return *c1 == *c2;
}

// splits a string based on the delim, by replacing the delim with a '\0'
// returns a pointer to everything after the delim, or NULL, if the
// delim is not present in the original string
static __attribute__((unused)) char* split_on( char* str, char delim) {
	char* ptr = str;
	while( 1 ) {
		if( !(*ptr) ) {
			return NULL;
		}
		if( *ptr == delim ) {
			*ptr = '\0';
			return ptr+1;
		}
		ptr++;
	}
}

#endif //COMMON_H
