#ifndef __TS_PLUGIN_H
#define __TS_PLUGIN_H

struct ts_options;
struct ts_results;
struct energy;

extern int plugins_run(struct ts_options *,
		       struct ts_results *, struct energy *);

#endif
