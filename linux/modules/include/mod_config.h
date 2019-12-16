#ifndef CONFIG_HEADER
#define CONFIG_HEADER

typedef uint64_t id_t;

typedef struct config_s {
	char * virtueid;
	uint64_t id;	//idlist style
	//todo conf goes here
	struct netruler_s * netrules;
	uint64_t syscallflags;
} config_t;


extern config_t config;

typedef void (*cleanupfunc_t)(config_t *c);

//returns 0 on success
int config_addCleanup(cleanupfunc_t cleanf);
int config_removeCleanup(cleanupfunc_t cleanf);

//returns a config_t for a given id
//if that config does not exist, returns NULL
//if the config does exist, you msut call config_put after you are done using it
//config_t * config_grabById(id_t id, unsigned long  *myflags);
//simialr to above, but will return a config_t for a given name
//(you should probably cache the id from the returned config to make subsequent lookups fast)
//config_t * config_grabByName(char * virtueid, unsigned long *myflags);

//call this when you are done with your config_t *
//void config_put(unsigned long myflags);

// ease of use (basically calls grabByName and then put automatically)
//id_t config_nameToId(char * virtueid);

//initializes a config
//returns the ID of the new config (so you dont have to get it with the virtueid)
//id_t config_addRINT(unsigned char * virtueid);

//removes a config from the list
//returns 0 on success
//int config_remove(id_t id);



//shortens the list to be nice
//dont need to call this terribly often, but it may occasionally free some memory
//however, it may make subsequent calls to add take slightly longer, if they have to go grab more memory
//returns the size pruned off
//size_t config_prune(void);

struct partial_msg * msg_init_config_wrapper(const char * tag, uint64_t severity, size_t kv_pairs);


#endif
