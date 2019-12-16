#ifndef TRANSDUCER_LIST_H
#define TRANSDUCER_LIST_H

#include "../../include/common.h"
#include "../../include/mod_sensors.h"
#include "../../include/mod_net.h"
#include "../../include/mod_kl.h"


struct transducer_descr* transducers[] = {
	&transducer_cpustats,
	&transducer_memstats,
	&transducer_idthash,
	&transducer_scthash,
	&transducer_procstats,
	&netfilter_transducer,
	&transducer_kl,
	NULL,
};

#endif //TRANSDUCER_LIST_H
