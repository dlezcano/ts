#ifndef __TRACE_H
#define __TRACE_H

#include <stdlib.h>

typedef enum { TRACE = 0, DEBUG, NOTICE, WARNING, ERROR, CRITICAL, FATAL } trace_level_t;

extern void trace(trace_level_t lvl, char *fmt, ...);
extern trace_level_t trace_char2level(const char *clvl);
extern int trace_set_level(trace_level_t lvl);
extern void trace_set_prefix(const char *prefix);
extern void trace_raw(trace_level_t lvl, char *fmt, ...);

#define FATAL(...)    do { trace(FATAL, __VA_ARGS__); exit(1); } while (0)
#define CRITICAL(...) trace(CRITICAL, __VA_ARGS__)
#define ERROR(...)    trace(ERROR, __VA_ARGS__)
#define WARNING(...)  trace(WARNING, __VA_ARGS__)
#define NOTICE(...)   trace(NOTICE, __VA_ARGS__)
#define DEBUG(...)    trace(DEBUG, __VA_ARGS__)
#define TRACE(...)    trace(TRACE, __VA_ARGS__)


#endif
