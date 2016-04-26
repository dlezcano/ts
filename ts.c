#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/time.h>

#include "trace.h"
#include "energy.h"
#include "options.h"
#include "plugin.h"
#include "results.h"
#include "script.h"
#include "topology.h"

static int compare(struct ts_options *tso)
{
	struct ts_results *tsr1;
	struct ts_results *tsr2;

	tsr1 = results_load(tso->file1);
	if (!tsr1) {
		CRITICAL("Failed to load '%s' for comparison\n", tso->file1);
		return 1;
	}

	tsr2 = results_load(tso->file2);
	if (!tsr2) {
		CRITICAL("Failed to load '%s' for comparison\n", tso->file2);
		return 1;
	}

	if (results_compare(tsr1, tsr2)) {
		ERROR("Failed to compare results\n");
		return 1;
	}

	results_free(tsr1);
	results_free(tsr2);

	return 0;
}

static int publish(struct ts_options *tso)
{
	struct ts_results *tsr;

	tsr = results_load(tso->file);
	if (!tsr) {
		CRITICAL("Failed to load file '%s'\n", tso->file);
		return 1;
	}

	if (results_publish(tsr)) {
		CRITICAL("Failed to publish results\n");
		return 1;
	}

	return 0;
}

int run(struct ts_options *tso)
{
	struct ts_results *tsr;
	struct topology *topology;
	struct energy *energy;
	int ret;

	tsr = results_alloc();
	if (!tsr)
		FATAL("Failed to allocated results array\na");

	topology = topology_init();
	if (!topology)
		FATAL("Failed to initialize topology\n");

	energy = energy_init(topology);
	if (!energy)
		WARNING("Failed to initialize energy\n");

	ret = scripts_run(tso, tsr, energy);
	if (ret)
		FATAL("Failed to run scripts\n");

	ret = plugins_run(tso, tsr, energy);
	if (ret)
		FATAL("Failed to run plugins\n");

	if (tso->save && results_save(tso->file, tsr))
		ERROR("Failed to save results\n");

	if (results_publish(tsr))
		ERROR("Failed to publish results\n");

	results_free(tsr);
	energy_fini(energy);
	topology_fini(topology);

	return ret ? 1 : 0;
}

int main(int argc, char *argv[])
{
	struct ts_options tso;

	if (ts_getoptions(argc, argv, &tso))
		FATAL("Failed to parse options\n");

	if (trace_set_level(tso.loglevel))
		ERROR("Failed to set log level\n");

	if (tso.compare)
		return compare(&tso);

	if (tso.publish)
		return publish(&tso);

	return run(&tso);
}
