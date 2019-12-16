struct api_response;
extern int hook_syscall( size_t syscall_num, void* new_call );
extern int unhook_syscall( size_t syscall_num );
extern int unhook_all( void );
extern int hook_all( void );
extern void add_trusted_daemon( char* dname, struct api_response* out );
