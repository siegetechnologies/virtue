#ifndef MOD_INTEGRITY_H
#define MOD_INTEGRITY_H

#define F_INDIRECT 0x1

extern int maven_check_sig( char* name );
extern int maven_check_system( void );
extern int maven_store_sig( char* name, unsigned long addr, size_t mem_len, uint32_t flags );

#endif //MOD_INTEGRITY_H
