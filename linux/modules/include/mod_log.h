

#define TYPE_INT 0
#define TYPE_STRING 1
#define TYPE_BYTES 2
#define TYPE_UUID 3


#define SEVERITY_DEBUG 0
#define SEVERITY_INFO 1
#define SEVERITY_WARNING 2
#define SEVERITY_ERROR 3
#define SEVERITY_CRITICAL 4

struct partial_msg;
struct uuid_t;

extern uint64_t maven_global_risk;
//extern uuid_t maven_global_uuid;
extern char * maven_global_virtueid;

extern struct partial_msg* msg_init( const char* tag, uint64_t severity, size_t kv_pairs );
extern int msg_additem( struct partial_msg* msg, const char* key, const void* value, int type );
extern int msg_finalize( struct partial_msg* msg );
extern int maven_panic( char* info, const char* file, int line);

extern void msg_moduleload(const char * name);
extern void msg_moduleunload(const char * name);

/* example usage:

struct partial_msg* msg = msg_init( "not a real tag", SEVERITY_INFO, 2 );
int sev = 1;
err = msg_additem( msg, "severity", &sev, TYPE_INT ); // note we pass a pointer, even for ints
// error checking omited
err = msg_additem( msg, "message", "testing", TYPE_STRING );
// error checking omited
msg_finalize( msg );

*/
/*
This generates a log message like:
"severity:i:1;message:s:testing;"
*/


//extern struct partial_msg* msg_init_registry_wrapper( const char* tag, uint64_t severity, size_t kv_pairs );
