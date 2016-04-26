/*
 * Template file as an example to write a plugin for the test suite
 *
 * Rule1: do not use printf but the traces API
 *
 * Rule2: The plugin_prerun, plugin_run and plugin_postrun functions
 *        must be implemented with these names and function signatures
 *
 * Rule3: Put in the plug_prerun function all initialization
 *
 * Rule4: Put in the run function all the test run, no initialization,
 * no cleanups, only the testing, because the measurements will be
 * done for this function
 *
 * Rule5: Put in the postrun function all cleanups
 *
 * Rule6: Use a private structure to pass data from pre->run->post
 * no global static variables1;
 *
 * Rule7: plugin_name and plugin_desc must be declared and initialized
 *
 */
#include <unistd.h>
#include "common.h"

const char *plugin_name = "sleep1";
const char *plugin_desc = "Sleep for a duration of 1 second";

void *plugin_prerun(void)
{
	return NULL;
}

int plugin_run(void *arg)
{
	sleep(1);
	return 0;
}

void plugin_postrun(void *arg)
{
}

