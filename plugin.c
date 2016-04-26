#define _GNU_SOURCE 
#include <dirent.h>
#include <dlfcn.h>
#include <unistd.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>

#include "trace.h"
#include "options.h"
#include "results.h"
#include "energy.h"
#include "topology.h"

static void  (*plugin_init)(struct ts_options *);
static void *(*plugin_prerun)(void);
static int   (*plugin_run)(void *);
static int   (*plugin_postrun)(void *);

static int _plugin_run(struct ts_options *tso, const char *path,
		       unsigned long *duration, struct energy *energy)
{
	void *handle, *data;
	struct timeval begin, end;
	struct energy *nrj;
	int ret = -1;

	nrj = energy_clone(energy);

	handle = dlopen(path, RTLD_LAZY);
	if (!handle) {
		ERROR("Failed to dlopen '%s': %s\n", path, dlerror());
		return -1;
	}

	plugin_init = dlsym(handle, "plugin_init");
	if (plugin_init)
		plugin_init(tso);
	else ERROR("No init function defined for plugin\n");

	plugin_prerun = dlsym(handle, "plugin_prerun");
	if (plugin_prerun)
		data = plugin_prerun();
	else DEBUG("No prerun function defined for plugin '%s'\n", path);

	plugin_run = dlsym(handle, "plugin_run");
	if (!plugin_run) {
		ERROR("plugin has no 'run' function\n");
		goto out;
	}

	trace_raw(NOTICE, "NOTICE: Running '%s'... ", path);
	gettimeofday(&begin, NULL);

	if (energy_read(nrj))
		ERROR("Failed to read sensor energie\n");

	ret = plugin_run(data);
	
	if (energy_read(energy))
		ERROR("Failed to read sensor energie\n");

	gettimeofday(&end, NULL);

	trace_raw(NOTICE, "%s\n", ret ? "Fail" : "Ok");

	energy_delta(nrj, energy, energy);

	plugin_postrun = dlsym(handle, "plugin_postrun");
	if (plugin_postrun)
		plugin_postrun(data);
	else DEBUG("No postrun function defined for plugin '%s'\n", path);
	
	*duration = (end.tv_sec - begin.tv_sec) * 1000000;
	*duration += (end.tv_usec - begin.tv_usec);
out:
	dlclose(handle);
	return ret;
}

#define avg(a, b, i) ((a) + (((b) - (a)) / (i)))

int plugin_is_excluded(char **exclude_list, const char *name)
{
	if (!exclude_list)
		return 0;

	while (*exclude_list) {
		if (!strcmp(*exclude_list, name))
			return 1;

		exclude_list++;
	}

	return 0;
}

char **plugins_exclude_list_init(const char *dir)
{
	char **exclude_list = NULL;
	char *path;
	char buffer[MAXPATHLEN];
	int nrexclude = 0;
	FILE *f;

	if (asprintf(&path, "%s/exclude_list", dir) < 0)
		FATAL("Failed to allocate exclude_list path\n");

	f = fopen(path,"r");
	if (!f)
		goto out;

	while (fscanf(f, "%s", buffer) != EOF) {

		/* Add NULL pointer to the end of the array => + 2 */
		exclude_list = realloc(exclude_list, nrexclude + 2);
		if (!exclude_list)
			FATAL("Failed to allocate exclude list\n");

		exclude_list[nrexclude] = strdup(buffer);
		if (!exclude_list[nrexclude])
			FATAL("Failed to dup exclude string\n");
		exclude_list[nrexclude + 1] = NULL;
		nrexclude++;
	}

	fclose(f);
out:
	free(path);
	return exclude_list;
}

void plugins_exclude_list_fini(char **exclude_list)
{
	char **p = exclude_list;

	while (p && *p) {
		free(*p);
		p++;
	}

	free(exclude_list);
}

int plugins_run(struct ts_options *tso, struct ts_results *tsr, struct energy *energy)
{
	DIR *dir;
	struct dirent dirent, *direntp;
	unsigned long duration;
	double avg_duration = 0;
	double avg_energy = 0;
	regex_t regex;
	char *path;
	char **exclude_list;

	if (regcomp(&regex, "^.*[.]so$", 0)) {
		ERROR("Failed to compile regular expression\n");
		return -1;
	}

	dir = opendir(tso->pluginspath);
	if (!dir) {
		ERROR("Failed to open plugin '%s'\n", tso->pluginspath);
		return -1;
	}

	exclude_list = plugins_exclude_list_init(tso->pluginspath);
	if (!exclude_list)
		DEBUG("No exclude list suitable\n");

	while (!readdir_r(dir, &dirent, &direntp)) {

		int i, ret;

		if (!direntp)
			break;
		
		if (regexec(&regex, direntp->d_name, 0, NULL, 0))
			continue;

		TRACE("Found '%s'\n", direntp->d_name);

		if (plugin_is_excluded(exclude_list, direntp->d_name)) {
			DEBUG("'%s' is excluded\n", direntp->d_name);
			continue;
		}

		if (asprintf(&path, "%s/%s", tso->pluginspath, direntp->d_name) < 0) {
			ERROR("Failed to allocate path for plugin\n");
			return -1;
		}

		avg_duration = 0;
		avg_energy   = 0;

		for (i = 0; i < tso->iterations; i++) {
			ret = _plugin_run(tso, path, &duration, energy);
			if (ret) {
				WARNING("'%s' failed \n", path);
				break;
			}

			avg_duration = avg(avg_duration, duration, i + 1);
			avg_energy = avg(avg_energy, energy_cost(energy), i + 1);
		}

		if (!ret && results_update(tsr, path, avg_duration, avg_energy)) {
			ERROR("Failed to update results for '%s'",
			      direntp->d_name);
			return -1;
		}

		free(path);
	}

	plugins_exclude_list_fini(exclude_list);
	
	closedir(dir);

	return 0;
}
