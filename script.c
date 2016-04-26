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
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "trace.h"
#include "options.h"
#include "results.h"
#include "energy.h"
#include "topology.h"

static int script_exec(const char *script, const char *parameter)
{
	const char *const argv[] = { script, parameter, NULL };
	pid_t pid;
	int status, cout, cerr;

	pid = fork();
	if (pid < 0)
		FATAL("Failed to fork script process\n");

	if (!pid) {
/*		cout = open(script, O_CREAT, 0600);
		if (cout < 0) {
			ERROR("Failed to open '%s': %m", );
		}

                cerr = open(script, O_CREAT, 0600);
                if (cout < 0) {
                        ERROR("Failed to open ");
                }
*/
		execv(script, argv);
		ERROR("Failed to execute script: %m\n");
		exit(1);
	}

	if (waitpid(pid, &status, 0) < 0) {
		ERROR("Failed to wait pid '%d': %m\n", pid);
		return -1;
	}

	if (WIFEXITED(status) && !WEXITSTATUS(status))
		return 0;

	ERROR("The script did not returned without error\n");
	return -1;
}

static int script_run(struct ts_options *tso, const char *path,
		      unsigned long *duration, struct energy *energy)
{
	struct timeval begin, end;
	struct energy *nrj;
	int ret = -1;

	nrj = energy_clone(energy);

	if (script_exec(path, "prerun")) {
		ERROR("Failed to run '%s prerun\n");
		return -1;
	}

	trace_raw(NOTICE, "NOTICE: Running '%s'... ", path);

	gettimeofday(&begin, NULL);

	if (energy_read(nrj))
		ERROR("Failed to read sensor energie\n");

	ret = script_exec(path, "run");
	if (ret) {
		ERROR("Failed to run '%s run\n");
		return -1;
	}

	if (energy_read(energy))
		ERROR("Failed to read sensor energie\n");

	gettimeofday(&end, NULL);

	trace_raw(NOTICE, "%s\n", ret ? "Fail" : "Ok");

	energy_delta(nrj, energy, energy);

	if (script_exec(path, "postrun")) {
		ERROR("Failed to run '%s postrun\n");
		return -1;
	}
	
	*duration = (end.tv_sec - begin.tv_sec) * 1000000;
	*duration += (end.tv_usec - begin.tv_usec);

	return 0;
}

#define avg(a, b, i) ((a) + (((b) - (a)) / (i)))

int script_is_excluded(char **exclude_list, const char *name)
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

char **script_exclude_list_init(const char *dir)
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

void script_exclude_list_fini(char **exclude_list)
{
	char **p = exclude_list;

	while (p && *p) {
		free(*p);
		p++;
	}

	free(exclude_list);
}

int scripts_run(struct ts_options *tso, struct ts_results *tsr, struct energy *energy)
{
	DIR *dir;
	struct dirent dirent, *direntp;
	unsigned long duration;
	double avg_duration = 0;
	double avg_energy = 0;
	regex_t regex;
	char *path;
	char **exclude_list;

	if (regcomp(&regex, "^.*[.]sh$", 0)) {
		ERROR("Failed to compile regular expression\n");
		return -1;
	}

	dir = opendir(tso->scriptspath);
	if (!dir) {
		WARNING("Failed to open script dir '%s'\n", tso->scriptspath);
		return 0;
	}

	exclude_list = script_exclude_list_init(tso->scriptspath);
	if (!exclude_list)
		DEBUG("No exclude list suitable\n");

	while (!readdir_r(dir, &dirent, &direntp)) {

		int i, ret;

		if (!direntp)
			break;
		
		if (regexec(&regex, direntp->d_name, 0, NULL, 0))
			continue;

		TRACE("Found '%s'\n", direntp->d_name);

		if (script_is_excluded(exclude_list, direntp->d_name)) {
			DEBUG("'%s' is excluded\n", direntp->d_name);
			continue;
		}

		if (asprintf(&path, "%s/%s", tso->scriptspath, direntp->d_name) < 0) {
			ERROR("Failed to allocate path for scripts\n");
			return -1;
		}

		avg_duration = 0;
		avg_energy   = 0;

		for (i = 0; i < tso->iterations; i++) {
			ret = script_run(tso, path, &duration, energy);
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

	script_exclude_list_fini(exclude_list);
	
	closedir(dir);

	return 0;
}
