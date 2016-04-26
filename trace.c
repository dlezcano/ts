#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "trace.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

static trace_level_t trace_level = TRACE;
static const char *trace_prefix;
static const char *level2char[] = {
	"TRACE",
	"DEBUG",
	"NOTICE",
	"WARNING",
	"ERROR",
	"CRITICAL",
	"FATAL",
};

trace_level_t trace_char2level(const char *clvl)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(level2char); i++)
		if (!strcmp(level2char[i], clvl))
			return i;

	return -1;
}

void trace(trace_level_t lvl, char *fmt, ...)
{
	va_list args;
	
	if (lvl < trace_level)
		return;

	printf("%s", level2char[lvl]);

	if (trace_prefix)
		printf("(%s): ", trace_prefix);
	else
		printf(": ");

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

int trace_set_level(trace_level_t lvl)
{
	if (lvl < TRACE && lvl > FATAL)
		return -1;

	trace_level = lvl;
	return 0;
}

void trace_set_prefix(const char *prefix)
{
	if (prefix)
		trace_prefix = prefix;
}

void trace_raw(trace_level_t lvl, char *fmt, ...)
{
	va_list args;
	
	if (lvl < trace_level)
		return;

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);

	fflush(stdout);
}
