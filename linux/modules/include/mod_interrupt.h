#ifndef MOD_INTERRUPT_H
#define MOD_INTERRUPT_H

#include <asm/ptrace.h>

// Public api
typedef void (*int_handler_fn)(struct pt_regs* regs, unsigned long code );
extern int hook_interrupt( size_t inum, char* existing_ksym, int_handler_fn hook );
extern void unhook_interrupt( size_t inum );
extern void unhook_all_interrupts( void );

extern int_handler_fn get_ref_fn( size_t inum );

#endif //MOD_INTERRUPT_H
