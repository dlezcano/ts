#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "trace.h"
#include "topology.h"

static int cpu_sysfs_read_id(int cpu, const char *file)
{
	FILE *f;
	int value = -1;
	char *path;

	if (asprintf(&path, "/sys/devices/system/cpu/cpu%d/topology/%s", cpu, file) < 0) {
		ERROR("Failed to allocate cpu%d's pat\n", cpu);
		return -1;
	}

	f = fopen(path, "r");
	if (!f) {
		ERROR("Failed to open '%s'\n", path);
		goto out;
	}

	if (fscanf(f, "%d", &value) < 1) {
		ERROR("Failed to read '%s' value\n", path);
		goto out_close;
	}
out_close:
	fclose(f);
out:
	free(path);

	return value;
}

static int cpu_get_core_id(int cpu)
{
	return cpu_sysfs_read_id(cpu, "core_id");
}

static int cpu_get_package_id(int cpu)
{
	return cpu_sysfs_read_id(cpu, "physical_package_id");
}

static int ht_build(struct package *package, struct core *core)
{
	int cpu, nrcpus;
	int nrthreads = 0;
	struct thread *thread = NULL;

	nrcpus = sysconf(_SC_NPROCESSORS_CONF);
	if (nrcpus < 0)
		return -1;

	for (cpu = 0; cpu < nrcpus; cpu++) {

		int package_id, core_id;

		package_id = cpu_get_package_id(cpu);
		if (package_id < 0)
			return -1;

		if (package->package_id != package_id)
			continue;

		core_id = cpu_get_core_id(cpu);
		if (core_id < 0)
			return -1;

		if (core->core_id != core_id)
			continue;

		thread = realloc(thread, sizeof(*thread) * (nrthreads + 1));
		if (!thread)
			FATAL("Failed to allocate memory for ht\n");

		thread[nrthreads].os_id = cpu;
		thread[nrthreads].thread_id = nrthreads;
		nrthreads++;
	}

	core->thread = thread;
	core->nrthreads = nrthreads;

	return 0;
}

static int core_build(struct package *package)
{
	int cpu, nrcpus;
	int nrcores = 0;
	cpu_set_t cpuset;
	struct core *core = NULL;

	CPU_ZERO(&cpuset);

	nrcpus = sysconf(_SC_NPROCESSORS_CONF);
	if (nrcpus < 0)
		return -1;

	for (cpu = 0; cpu < nrcpus; cpu++) {

		int package_id, core_id;

		package_id = cpu_get_package_id(cpu);
		if (package_id < 0)
			return -1;

		if (package->package_id != package_id)
			continue;

		core_id = cpu_get_core_id(cpu);
		if (core_id < 0)
			return -1;

		if (CPU_ISSET(core_id, &cpuset))
			continue;

		CPU_SET(core_id, &cpuset);

		core = realloc(core, sizeof(*core) * (nrcores + 1));
		if (!core)
			FATAL("Failed to allocate memory for core\n");
		core[nrcores].core_id = core_id;
		core[nrcores].os_id = cpu;

		if (ht_build(package, &core[nrcores])) {
			ERROR("Failed to build HT topology\n");
			return -1;
		}


		nrcores++;
	}

	package->core = core;
	package->nrcores = nrcores;

	return 0;
}

static int package_build(struct topology *topology)
{
	int cpu, nrcpus;
	int nrpackages = 0;
	cpu_set_t cpuset;
	struct package *package = NULL;
	
	CPU_ZERO(&cpuset);

	nrcpus = sysconf(_SC_NPROCESSORS_CONF);
	if (nrcpus < 0)
		return -1;

	for (cpu = 0; cpu < nrcpus; cpu++) {

		int package_id;

		package_id = cpu_get_package_id(cpu);
		if (package_id < 0)
			return -1;

		if (CPU_ISSET(package_id, &cpuset))
			continue;

		CPU_SET(package_id, &cpuset);

		package = realloc(package, sizeof(*package) * (nrpackages + 1));
		if (!package)
			FATAL("Failed to allocate memory for package\n");
		package[nrpackages].package_id = package_id;

		if (core_build(&package[nrpackages])) {
			ERROR("Failed to build core topology\n");
			return -1;
		}

		nrpackages++;
	}

	topology->package = package;
	topology->nrpackages = nrpackages;

	return 0;
}

void topology_show(struct topology *topology)
{
	int i, j, k;

	if (!topology)
		return;

	for (i = 0; i < topology->nrpackages; i++) {
		struct package *package = topology->package;
		DEBUG("package %d\n", package[i].package_id);
		
		for (j = 0; j < package[i].nrcores; j++) {
			struct core *core = package[i].core;

			if (core[j].nrthreads)
				DEBUG("  core %d\n",
				      core[j].core_id);
			else
				DEBUG("  core %d (cpu %d)\n",
				      core[j].core_id, core[j].os_id);

			for (k = 0; k < core[j].nrthreads; k++) {
				struct thread *thread = core[j].thread;
				DEBUG("    thread %d (cpu %d)\n",
				      thread[k].thread_id, thread[k].os_id);
			}
		}
	}
}

struct topology *topology_init(void)
{
	struct topology *topology;
	
	topology = calloc(sizeof(*topology), 1);
	if (!topology)
		return NULL;

	if (package_build(topology))
		goto out_free;

	topology_show(topology);
out:
	return topology;
out_free:
	free(topology);
	topology = NULL;
	goto out;
}

void topology_fini(struct topology *topology)
{
	int i, j;

	if (!topology)
		return;

	for (i = 0; i < topology->nrpackages; i++) {
		for (j = 0; j < topology->package[i].nrcores; j++)
			free(topology->package[i].core[j].thread);
		free(topology->package[i].core);
	}
	free(topology->package);
}
