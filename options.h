#ifndef __TS_OPTIONS_H
#define __TS_OPTIONS_H

#include <stdbool.h>

struct ts_options {
	int loglevel;
	int iterations;
	bool compare;
	bool save;
	bool publish;
	const char *file;
	const char *file1;
	const char *file2;
	const char *pluginspath;
	const char *scriptspath;
};

extern int ts_getoptions(int argc, char *argv[], struct ts_options *options);

#endif
