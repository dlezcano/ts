#include "../trace.h"
#include "../options.h"

extern const char *plugin_name;
extern const char *plugin_desc;

void plugin_init(struct ts_options *tso)
{
	trace_set_prefix(plugin_name);
	trace_set_level(tso->loglevel);
	TRACE("Plugin name: %s\n", plugin_name);
	TRACE("Plugin description: %s\n", plugin_desc);
	TRACE("Plugin initialized\n");
}

