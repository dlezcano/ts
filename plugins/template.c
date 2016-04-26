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

#include "common.h"

const char *plugin_name = "Template";
const char *plugin_desc = "Template plugin for example";

static struct private_data {
	int a_value;
} pdata = { 1234 };

void *plugin_prerun(void *arg)
{
	/* arg is a ts_options structure coming for the ts */
	return &pdata;
}

int plugin_run(void *arg)
{
	struct private_data *pdata = arg;
	DEBUG("Pluging running with value %d\n", pdata->a_value);
	return 0;
}

void plugin_postrun(void *arg)
{
	struct private_data *pdata = arg;
	DEBUG("Pluging finishing with value %d\n", pdata->a_value);
}

