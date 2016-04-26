#include <getopt.h>
#include <string.h>

#include "trace.h"
#include "options.h"

static struct option long_options[] = {
	{ "loglevel",   0, 0, 'l' },
	{ "publish",    0, 0, 'b' },
	{ "compare",    0, 0, 'c' },
	{ "save",       0, 0, 's' },
	{ "plugins",    0, 0, 'p' },
	{ "scripts",    0, 0, 'r' },
	{ "iterations", 0, 0, 'i' },
	{ "file",       0, 0, 'f' },
	{ "file1",      0, 0, 'x' },
	{ "file2",      0, 0, 'y' },
        { 0, 0, 0, 0 },
};

int ts_getoptions(int argc, char *argv[], struct ts_options *tso)
{
	int c;

	memset(tso, 0, sizeof(*tso));
	tso->loglevel = NOTICE;
	tso->pluginspath = "./plugins";
	tso->scriptspath = "./scripts";
	tso->iterations = 1;

	while (1) {
		int optindex = 0;

		c = getopt_long(argc, argv, "bcsr:f:p:l:i:v",
				long_options, &optindex);
		if (c == -1)
			break;

		switch (c) {
		case 'l':
			tso->loglevel = trace_char2level(optarg);
			break;
		case 'p':
			tso->pluginspath = optarg;
			break;
		case 'r':
			tso->scriptspath = optarg;
			break;
		case 'b':
			tso->publish = true;
			break;
		case 'c':
			tso->compare = true;
			break;
		case 's':
			tso->save = true;
			break;
		case 'f':
			tso->file = optarg;
			break;
		case 'i':
			tso->iterations = atoi(optarg);
			break;
		default:
			return -1;
		}
	}

	if (tso->iterations < 1)
		FATAL("'iterations' option must be greater than zero\n");

	if (tso->compare && tso->save)
		FATAL("'compare' and 'save' options are mutually exclusive\n");

	if (tso->save && !tso->file)
		FATAL("'save' option set but no specified file to save in\n");

	if (tso->publish && !tso->file)
		FATAL("'publish' option set but no specified file to show\n");

	if (tso->compare && !tso->file)
		FATAL("'compare' option set but no specified files to compare to\n");

	if (tso->compare) {
		char *token;

		token = strchr(tso->file, ',');
		if (!token)
			FATAL("Failed to parse file name\n");

		*token = '\0';
		tso->file1 = tso->file;
		tso->file2 = token + 1;
	}

	return 0;
}
