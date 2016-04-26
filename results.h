#ifndef __TS_RESULTS_H
#define __TS_RESULTS_H

struct ts_results;

extern struct ts_results *results_alloc(void);
extern struct ts_results *results_load(const char *path);
extern void results_free(struct ts_results *tsr);

extern int results_update(struct ts_results *tsr, const char *path,
			  double duration, double energy);

extern int results_compare(struct ts_results *tsr1, struct ts_results *tsr2);

extern int results_publish(struct ts_results *tsr);

extern struct ts_results *results_load(const char *path);

extern int results_save(const char *path, struct ts_results *tsr);

#endif
