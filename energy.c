#define _GNU_SOURCE 
#include <dirent.h>
#include <dlfcn.h>
#include <unistd.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "energy.h"
#include "trace.h"
#include "topology.h"

#define PLUGINS_SENSOR "./sensors"

static int    (*sensor_init)(struct energy *);
static void   (*sensor_fini)(struct energy *);
static int    (*sensor_read)(struct energy *);
static int    (*sensor_probe)(void);

void *energy_sensor_probe(const char *path, struct topology *topology)
{
	void *handle;

	handle = dlopen(path, RTLD_LAZY);
	if (!handle) {
		ERROR("Failed to dlopen '%s': %s\n", path, dlerror());
		return NULL;
	}

	sensor_probe = dlsym(handle, "sensor_probe");
	if (!sensor_probe) {
		ERROR("No probe function defined for sensor\n");
		goto out_close;
	}

	if (sensor_probe())
		goto out_close;

	NOTICE("'%s' probed successfully\n", path);

	return handle;
out_close:
	dlclose(handle);
	return NULL;
}

static int energy_sensor_init(void *handle, struct energy *energy)
{
	sensor_init = dlsym(handle, "sensor_init");
	if (!sensor_init)
		FATAL("No sensor init function\n");

	return sensor_init(energy);
}

static void energy_sensor_fini(void *handle, struct energy *energy)
{
	sensor_fini = dlsym(handle, "sensor_fini");
	if (!sensor_fini)
		FATAL("No sensor fini function\n");

	sensor_fini(energy);
}

static int energy_sensor_read(void *handle, struct energy *energy)
{
	sensor_read = dlsym(handle, "sensor_read");
	if (!sensor_read)
		FATAL("No sensor read function\n");

	return sensor_read(energy);
}

int energy_read(struct energy *energy)
{
	return energy_sensor_read(energy->handle, energy);
}

void energy_delta(struct energy *before, struct energy *after, struct energy *result)
{
	int i;

	for (i = 0; i  < after->topology->nrpackages; i++) {
		result->pkg[i].pkg = after->pkg[i].pkg - before->pkg[i].pkg;
		result->pkg[i].core = after->pkg[i].core - before->pkg[i].core;
		result->pkg[i].noncore = after->pkg[i].noncore - before->pkg[i].noncore;
		TRACE("energy package[%d]: pkg=%lf, core=%lf, noncore=%lf uJ\n", i,
		      result->pkg[i].pkg, result->pkg[i].core, result->pkg[i].noncore);
	}

	result->sys.dram = after->sys.dram - before->sys.dram;
	result->sys.gpu = after->sys.gpu - before->sys.gpu;

	TRACE("energy sys: gpu=%lf, dram=%lf uJ\n", result->sys.gpu, result->sys.dram);
}

struct energy *energy_alloc(struct topology *topology)
{
	struct energy *energy;

	energy = calloc(sizeof(*energy), 1);
	if (!energy)
		FATAL("Failed to allocate energy structure\n");

	energy->pkg = calloc(sizeof(*energy->pkg), topology->nrpackages);
	if (!energy->pkg)
		FATAL("Failed to allocate energy pkg's structure\n");

	energy->topology = topology;

	return energy;
}

double energy_cost(struct energy *energy)
{
	double cost = 0;
	int i;

	for (i = 0; i < energy->topology->nrpackages; i++)
		cost += energy->pkg[i].pkg;

	cost += energy->sys.gpu;
	cost += energy->sys.dram;

	return cost;
}

struct energy *energy_clone(struct energy *energy)
{
	struct energy *nrj;

	nrj = energy_alloc(energy->topology);
	nrj->data = energy->data;
	nrj->flags = energy->flags;
	nrj->handle = energy->handle;

	return nrj;
}

void energy_free(struct energy *energy)
{
	free(energy->pkg);
	free(energy);
}

struct energy *energy_init(struct topology *topology)
{
	DIR *dir;
	struct dirent dirent, *direntp;
	struct energy *energy;
	regex_t regex;
	char *path;

	energy = energy_alloc(topology);

	if (regcomp(&regex, "^.*[.]so$", 0))
		FATAL("Failed to compile regular expression\n");

	dir = opendir(PLUGINS_SENSOR);
	if (!dir)
		FATAL("Failed to open plugin '%s'\n", PLUGINS_SENSOR);

	while (!readdir_r(dir, &dirent, &direntp)) {

		void *handle;

		if (!direntp)
			break;
		
		if (regexec(&regex, direntp->d_name, 0, NULL, 0))
			continue;

		TRACE("Found '%s'\n", direntp->d_name);

		if (asprintf(&path, "%s/%s", PLUGINS_SENSOR, direntp->d_name) < 0) {
			CRITICAL("Failed to allocate path for plugin\n");
			continue;
		}

		handle = energy_sensor_probe(path, topology);

		free(path);

		if (!handle)
			continue;

		energy->handle = handle;

		if (energy_sensor_init(handle, energy))
			FATAL("Sensor initialization failed\n");

		break;
	}
	
	closedir(dir);

	return energy;
}

void energy_fini(struct energy *energy)
{
	energy_sensor_fini(energy->handle, energy);
	energy_free(energy);
}
