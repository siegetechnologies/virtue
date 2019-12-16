#ifndef NEW_SYSCALLS_H
#define NEW_SYSCALLS_H

struct api_response;

void add_trusted_daemon( char* dname, struct api_response* out );
void cleanup_new_syscalls( void );

asmlinkage long new_sys_delete_module( const char __user *name_user,
                                       unsigned int flags);

asmlinkage long new_sys_execve( const char __user *filename,
                                const char __user *const __user *argv,
                                const char __user *const __user *envp);
asmlinkage long new_sys_execveat( int fd, const char __user *filename,
                                const char __user *const __user *argv,
                                const char __user *const __user *envp,
				int flags);
asmlinkage long new_sys_open( const char __user *filename,
                              int flags,
                              umode_t mode );
asmlinkage long new_sys_openat( int dirfd, const char __user *filename,
                              int flags,
                              umode_t mode );
asmlinkage long new_sys_creat( const char __user *filename,
                              int flags);


int new_syscall_init(void);
#endif
