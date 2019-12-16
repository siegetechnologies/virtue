
#ifndef MAVEN_FILTER_H

#define MAVEN_FILTER_H

#include "..\..\include\maven.h"
/* defines */


#ifdef __cplusplus
extern "C" {
#endif

/* Typedefs */

	typedef WCHAR MAVEN_EXTENSION[MAVEN_MAX_EXT_LENGTH];

	typedef struct _MAVEN_PROCESS_PERMISSIONS {
		PUNICODE_STRING  pProcessImageName;	/* Image name including path */
		DWORD32	  count;					/* number of times this process requested service */
		DWORD32	  fail;						/* number of times this process request failed */
		MAVEN_EXTENSION extensions[MAVEN_MAX_EXT_ENTRIES]; /* list of file types that this app may access */
	} MAVEN_PROCESS_PERMISSIONS;

	typedef struct _MAVEN_CONFIG_RECORD {
		PUNICODE_STRING  pProcessImageName;
	} MAVEN_CONFIG_RECORD;

	typedef struct _MAVEN_STATE_CHANGE {
		OPERATING_MODE mode;
	} MAVEN_STATE_CHANGE;

#ifdef __cplusplus
}
#endif

#endif /* MAVEN_FILTER_H */
