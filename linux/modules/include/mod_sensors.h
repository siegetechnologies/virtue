#ifndef MOD_SENSORS_H
#define MOD_SENSORS_H

#include "common.h"

#define DECLARE_TRANSDUCER(basename)\
	extern int set_transducer_##basename##_enabled( int on );\
	extern int get_transducer_##basename##_enabled( void ); \
	struct transducer_descr transducer_##basename = {\
		.name=#basename,\
		.get_enabled=get_transducer_##basename##_enabled,\
		.set_enabled=set_transducer_##basename##_enabled,\
	};

DECLARE_TRANSDUCER( cpustats );
DECLARE_TRANSDUCER( memstats );
DECLARE_TRANSDUCER( procstats );
DECLARE_TRANSDUCER( scthash );
DECLARE_TRANSDUCER( idthash );

#endif //MOD_SENSORS_H
