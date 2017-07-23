/*
 * Output debugging information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "pv.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>


#ifdef ENABLE_DEBUGGING
/*
 * Output debugging information to the file given in the DEBUG environment
 * variable, if it is defined.
 */
void debugging_output(const char *function, const char *file, int line,
		      const char *format, ...)
{
	static int tried_open = 0;
	static FILE *debugfptr = NULL;
	char *debugfile;
	va_list ap;
	time_t t;
	struct tm *tm;
	char tbuf[128];

	if (0 == tried_open) {
		debugfile = getenv("DEBUG");
		if (NULL != debugfile)
			debugfptr = fopen(debugfile, "a");
		tried_open = 1;
	}

	if (NULL == debugfptr)
		return;

	time(&t);
	tm = localtime(&t);
	tbuf[0] = 0;
	strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);

	fprintf(debugfptr, "[%s] (%d) %s (%s:%d): ", tbuf, getpid(),
		function, file, line);

	va_start(ap, format);
	vfprintf(debugfptr, format, ap);
	va_end(ap);

	fprintf(debugfptr, "\n");
	fflush(debugfptr);
}

#else				/* ! ENABLE_DEBUGGING */

/*
 * Stub debugging output function.
 */
void debugging_output(const char *function, const char *file, int line,
		      const char *format, ...)
{
}

#endif				/* ENABLE_DEBUGGING */

/* EOF */
