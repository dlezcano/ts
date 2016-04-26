#ifndef __TS_SCRIPT_H
#define __TS_SCRIPT_H

struct ts_options;
struct ts_results;
struct energy;

extern int scripts_run(struct ts_options *,
		       struct ts_results *, struct energy *);

#endif
