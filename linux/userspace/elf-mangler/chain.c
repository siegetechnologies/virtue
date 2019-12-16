#include <stdio.h>
#include <stdarg.h>

int printf_chainable( const char* format, ... ) {
	va_list args;
	va_start( args, format );
	vprintf( format, args );
	return 0;
}
