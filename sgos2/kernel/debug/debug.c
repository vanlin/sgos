// Ported from SGOS1

#include <sgos.h>
#include <ctype.h>
#include <debug.h>
#include <string.h>
#include <stdarg.h>
#include <arch.h>
#include <terminal.h>

static int putstr(const char* str);

static char printbuf[1024];

static int (*printer)(const char*);

void kprintf(const char *fmt, ...)
{
	va_list args;
	int i;
	va_start(args, fmt);
	i=vsprintf( printbuf, fmt, args );
	printer( printbuf );
	va_end(args);
}

void die(const char *s )
{
	KERROR( s );
}

void debug_init()
{
	printer = putstr;
	clrscr();
	printer("System Debugger( 2009-08-04 ) for SGOS2.\n");
}

int debug_print( char *buf )
{
	return printer(buf);
}

static int putstr( const char* buf )
{
	int i=0;
	while(buf[i])
		putchar(buf[i++]);
	return i;
}
